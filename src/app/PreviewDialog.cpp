#include "PreviewDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

PreviewDialog::PreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preview"));
    // modal so grid rows can't shift under the stored source row
    setWindowModality(Qt::ApplicationModal);
    resize(900, 1000);

    auto *layout = new QVBoxLayout(this);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_image = new QLabel(this);
    m_image->setAlignment(Qt::AlignCenter);
    m_scroll->setWidget(m_image);
    // rescale whenever the viewport gets its real size (first layout, resizes)
    m_scroll->viewport()->installEventFilter(this);
    layout->addWidget(m_scroll, 1);

    m_caption = new QLabel(this);
    m_caption->setWordWrap(true);
    m_caption->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_caption);

    auto *buttons = new QDialogButtonBox(this);
    auto *del = buttons->addButton(tr("Move to Trash"), QDialogButtonBox::DestructiveRole);
    del->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
    auto *review = buttons->addButton(tr("To Review"), QDialogButtonBox::ActionRole);
    review->setIcon(QIcon::fromTheme(QStringLiteral("view-task")));
    auto *allow = buttons->addButton(tr("Allow"), QDialogButtonBox::ActionRole);
    allow->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));
    auto *close = buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(del, &QPushButton::clicked, this, [this] {
        emit deleteRequested(m_sourceRow);
        accept();
    });
    connect(review, &QPushButton::clicked, this, [this] {
        emit reviewRequested(m_sourceRow);
        accept();
    });
    connect(allow, &QPushButton::clicked, this, [this] {
        emit allowRequested(m_sourceRow);
        accept();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void PreviewDialog::showEntry(int sourceRow, const QString &path, const QString &caption,
                              const QString &reasons, bool isImage)
{
    m_sourceRow = sourceRow;
    m_full = QPixmap();
    if (isImage) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        m_full = QPixmap::fromImage(reader.read());
    }
    QString text = QStringLiteral("<b>%1</b><br>%2").arg(caption.toHtmlEscaped(),
                                                         path.toHtmlEscaped());
    if (!reasons.isEmpty())
        text += QStringLiteral("<br><i>%1</i>").arg(reasons.toHtmlEscaped());
    m_caption->setText(text);
    if (m_full.isNull())
        m_image->setText(tr("(no preview)"));
    updateScaled();
    show();
    raise();
    activateWindow();
}

bool PreviewDialog::eventFilter(QObject *watched, QEvent *ev)
{
    if (watched == m_scroll->viewport() && ev->type() == QEvent::Resize)
        updateScaled();
    return QDialog::eventFilter(watched, ev);
}

void PreviewDialog::updateScaled()
{
    if (m_full.isNull())
        return;
    const QSize avail = m_scroll->viewport()->size() - QSize(4, 4);
    if (avail.width() < 32 || avail.height() < 32)
        return; // not laid out yet; the viewport resize will re-trigger us
    // fit to window in both directions, like Gwenview's fit view
    m_image->setPixmap(
        m_full.scaled(avail, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
