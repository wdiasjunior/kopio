#include "ResultsPage.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "PreviewDialog.h"
#include "ResultsDelegate.h"
#include "TrashOps.h"

using namespace kop_pipeline;

namespace {

const char *bucketLabel(KopBucket b)
{
    switch (b) {
    case KopBucket::AllFlagged: return QT_TRANSLATE_NOOP("ResultsPage", "All flagged");
    case KopBucket::JunkFiles: return QT_TRANSLATE_NOOP("ResultsPage", "Junk files");
    case KopBucket::ExactDupes: return QT_TRANSLATE_NOOP("ResultsPage", "Exact dupes");
    case KopBucket::JunkPages: return QT_TRANSLATE_NOOP("ResultsPage", "Junk pages");
    case KopBucket::Review: return QT_TRANSLATE_NOOP("ResultsPage", "Needs review");
    case KopBucket::Allowed: return QT_TRANSLATE_NOOP("ResultsPage", "Allowed");
    case KopBucket::Clean: return QT_TRANSLATE_NOOP("ResultsPage", "Clean");
    default: return "";
    }
}

// QListView::updateGeometries() resets the vertical single-step to the grid
// cell height (~250px) on every relayout, which makes wheel scrolling jump
// almost a full screen per notch. Re-apply a small pixel step after it runs.
class GridView : public QListView
{
public:
    using QListView::QListView;

protected:
    void updateGeometries() override
    {
        QListView::updateGeometries();
        verticalScrollBar()->setSingleStep(40);
    }
};

} // namespace

ResultsPage::ResultsPage(QWidget *parent)
    : QWidget(parent)
{
    m_model = new ResultsModel(this);
    m_proxy = new KopSortFilterProxy(this);
    m_proxy->setSourceModel(m_model);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    // ---- chip bar + sort selector ----
    auto *chipRow = new QHBoxLayout;
    auto *chipGroup = new QButtonGroup(this);
    chipGroup->setExclusive(true);
    for (int i = 0; i < static_cast<int>(KopBucket::Count); i++) {
        auto *chip = new QToolButton(this);
        chip->setCheckable(true);
        chip->setAutoRaise(false);
        chip->setText(tr(bucketLabel(static_cast<KopBucket>(i))));
        chipGroup->addButton(chip, i);
        chipRow->addWidget(chip);
        m_chips.append(chip);
    }
    m_chips[0]->setChecked(true);
    connect(chipGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_proxy->setBucket(static_cast<KopBucket>(id));
        updateStatusLine();
    });
    chipRow->addStretch();

    chipRow->addWidget(new QLabel(tr("Sort by:"), this));
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("File contents"), int(KopSortFilterProxy::SortMode::Content));
    m_sortCombo->addItem(tr("Name"), int(KopSortFilterProxy::SortMode::Name));
    m_sortCombo->addItem(tr("File size"), int(KopSortFilterProxy::SortMode::Size));
    connect(m_sortCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_proxy->setSortMode(static_cast<KopSortFilterProxy::SortMode>(
            m_sortCombo->currentData().toInt()));
    });
    chipRow->addWidget(m_sortCombo);
    layout->addLayout(chipRow);

    // ---- grid ----
    m_view = new GridView(this);
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setUniformItemSizes(true);
    m_view->setGridSize(QSize(ResultsDelegate::CellW + 8, ResultsDelegate::CellH + 8));
    m_view->setSpacing(4);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setMovement(QListView::Static);
    // scroll by pixels (~120px per wheel notch with the 3-line default)
    // instead of jumping a whole 250px row per notch; see GridView above
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setModel(m_proxy);
    m_view->setItemDelegate(new ResultsDelegate(this));
    connect(m_view, &QListView::clicked, this, &ResultsPage::onGridClicked);
    layout->addWidget(m_view, 1);

    // ---- status line + action bar ----
    auto *actionRow = new QHBoxLayout;
    m_statusLine = new QLabel(this);
    actionRow->addWidget(m_statusLine, 1);

    auto *selectAll = new QPushButton(tr("Select all"), this);
    connect(selectAll, &QPushButton::clicked, this, [this] { selectAllVisible(true); });
    actionRow->addWidget(selectAll);

    auto *deselectAll = new QPushButton(tr("Deselect all"), this);
    connect(deselectAll, &QPushButton::clicked, this, [this] { selectAllVisible(false); });
    actionRow->addWidget(deselectAll);

    auto *allow = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")),
                                  tr("Allow"), this);
    allow->setToolTip(tr("Keep the checked files and stop flagging them"));
    connect(allow, &QPushButton::clicked, this,
            [this] { applyStatusToChecked(KOP_ST_ALLOWED); });
    actionRow->addWidget(allow);

    auto *review = new QPushButton(QIcon::fromTheme(QStringLiteral("view-task")),
                                   tr("To review"), this);
    review->setToolTip(tr("Move the checked files to the review list"));
    connect(review, &QPushButton::clicked, this,
            [this] { applyStatusToChecked(KOP_ST_REVIEW); });
    actionRow->addWidget(review);

    auto *del = new QPushButton(QIcon::fromTheme(QStringLiteral("user-trash")),
                                tr("Move to Trash…"), this);
    connect(del, &QPushButton::clicked, this, &ResultsPage::deleteChecked);
    actionRow->addWidget(del);

    auto *rescan = new QPushButton(QIcon::fromTheme(QStringLiteral("go-home")),
                                   tr("New scan"), this);
    connect(rescan, &QPushButton::clicked, this, &ResultsPage::newScanRequested);
    actionRow->addWidget(rescan);

    layout->addLayout(actionRow);

    m_preview = new PreviewDialog(this);
    connect(m_preview, &PreviewDialog::deleteRequested, this,
            [this](int row) { deleteRows({ row }); });
    connect(m_preview, &PreviewDialog::reviewRequested, this,
            [this](int row) { m_model->setStatus(row, KOP_ST_REVIEW); updateChips(); });
    connect(m_preview, &PreviewDialog::allowRequested, this,
            [this](int row) { m_model->setStatus(row, KOP_ST_ALLOWED); updateChips(); });

    connect(m_model, &ResultsModel::countsChanged, this, [this] {
        updateChips();
        updateStatusLine();
    });
}

void ResultsPage::setEntries(const QVector<KopEntry> &entries)
{
    m_model->setEntries(entries);
    m_proxy->setSortMode(KopSortFilterProxy::SortMode::Content);
    updateChips();
    updateStatusLine();

    // test hook: KOPIO_PREVIEW=1 opens the first visible item's preview
    if (qEnvironmentVariableIntValue("KOPIO_PREVIEW") && m_proxy->rowCount() > 0) {
        const QModelIndex src = m_proxy->mapToSource(m_proxy->index(0, 0));
        m_preview->showEntry(src.row(),
                             src.data(ResultsModel::PathRole).toString(),
                             src.data(ResultsModel::ChapterLabelRole).toString(),
                             src.data(ResultsModel::ReasonsTextRole).toString(),
                             src.data(ResultsModel::IsImageRole).toBool());
    }
}

void ResultsPage::updateChips()
{
    for (int i = 0; i < m_chips.size(); i++) {
        const auto b = static_cast<KopBucket>(i);
        m_chips[i]->setText(QStringLiteral("%1 (%2)")
                                .arg(tr(bucketLabel(b)))
                                .arg(m_model->countInBucket(b)));
    }
}

void ResultsPage::updateStatusLine()
{
    const double mb = double(m_model->checkedBytes()) / (1024.0 * 1024.0);
    m_statusLine->setText(tr("%1 shown · %2 checked · %3 MB to free")
                              .arg(m_proxy->rowCount())
                              .arg(m_model->checkedCount())
                              .arg(mb, 0, 'f', 1));
}

void ResultsPage::selectAllVisible(bool checked)
{
    for (int i = 0; i < m_proxy->rowCount(); i++) {
        const QModelIndex src = m_proxy->mapToSource(m_proxy->index(i, 0));
        m_model->setChecked(src.row(), checked);
    }
}

QVector<int> ResultsPage::checkedVisibleSourceRows() const
{
    QVector<int> rows;
    for (int i = 0; i < m_proxy->rowCount(); i++) {
        const QModelIndex src = m_proxy->mapToSource(m_proxy->index(i, 0));
        if (m_model->isChecked(src.row()))
            rows.append(src.row());
    }
    return rows;
}

void ResultsPage::applyStatusToChecked(KopUserStatus st)
{
    const QVector<int> rows = checkedVisibleSourceRows();
    if (rows.isEmpty())
        return;
    for (int row : rows)
        m_model->setStatus(row, st);
    if (st == KOP_ST_REVIEW) {
        // tell the user where the reviewed files live
        QStringList places;
        for (int row : rows) {
            const QString label =
                m_model->index(row).data(ResultsModel::ChapterLabelRole).toString();
            if (!places.contains(label))
                places.append(label);
        }
        QMessageBox::information(
            this, tr("Added to review list"),
            tr("%n file(s) moved to the review list, from:\n%1", nullptr,
               int(rows.size()))
                .arg(places.join(QStringLiteral("\n"))));
    }
}

void ResultsPage::deleteChecked()
{
    deleteRows(checkedVisibleSourceRows());
}

void ResultsPage::deleteRows(const QVector<int> &sourceRows)
{
    if (sourceRows.isEmpty())
        return;
    qint64 bytes = 0;
    for (int row : sourceRows)
        bytes += m_model->index(row).data(ResultsModel::SizeRole).toLongLong();
    const double mb = double(bytes) / (1024.0 * 1024.0);

    QString title = TrashOps::dryRun() ? tr("Move to trash? (DRY RUN)")
                                       : tr("Move to trash?");
    const auto answer = QMessageBox::question(
        this, title,
        tr("Move %n file(s) (%1 MB) to the trash?", nullptr, int(sourceRows.size()))
            .arg(mb, 0, 'f', 1),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    QStringList paths;
    for (int row : sourceRows)
        paths.append(m_model->pathAt(row));
    const TrashOps::Result result = TrashOps::trash(paths);

    // drop the rows whose files were actually trashed (dry run keeps them)
    if (!TrashOps::dryRun()) {
        QVector<int> trashedRows;
        for (qsizetype i = 0; i < paths.size(); i++)
            if (!result.failed.contains(paths[i]))
                trashedRows.append(sourceRows[i]);
        m_model->removeRowsByIndex(trashedRows);
    }

    if (!result.failed.isEmpty()) {
        QMessageBox::warning(
            this, tr("Some files could not be trashed"),
            tr("%n file(s) could not be moved to the trash:\n%1", nullptr,
               int(result.failed.size()))
                .arg(result.failed.mid(0, 15).join(QStringLiteral("\n"))));
    } else if (TrashOps::dryRun()) {
        QMessageBox::information(this, tr("Dry run"),
                                 tr("KOPIO_DRY_RUN is set — nothing was trashed."));
    }
    updateChips();
    updateStatusLine();
}

void ResultsPage::onGridClicked(const QModelIndex &proxyIndex)
{
    const QModelIndex src = m_proxy->mapToSource(proxyIndex);
    if (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) {
        m_preview->showEntry(src.row(),
                             src.data(ResultsModel::PathRole).toString(),
                             src.data(ResultsModel::ChapterLabelRole).toString(),
                             src.data(ResultsModel::ReasonsTextRole).toString(),
                             src.data(ResultsModel::IsImageRole).toBool());
        return;
    }
    m_model->setChecked(src.row(), !m_model->isChecked(src.row()));
}
