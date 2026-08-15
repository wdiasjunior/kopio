#include "ResultsModel.h"

#include <QFileInfo>

#include <algorithm>
#include <climits>
#include <functional>

using namespace kop_pipeline;

static KopBucket bucketFor(KopCategory cat, KopUserStatus st)
{
    if (st == KOP_ST_ALLOWED)
        return KopBucket::Allowed;
    if (st == KOP_ST_REVIEW)
        return KopBucket::Review;
    switch (cat) {
    case KOP_CAT_JUNK_FILE: return KopBucket::JunkFiles;
    case KOP_CAT_EXACT_DUPE: return KopBucket::ExactDupes;
    case KOP_CAT_JUNK_PAGE: return KopBucket::JunkPages;
    case KOP_CAT_REVIEW: return KopBucket::Review;
    default: return KopBucket::Clean;
    }
}

ResultsModel::ResultsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void ResultsModel::setEntries(const QVector<KopEntry> &entries)
{
    beginResetModel();
    m_items.clear();
    m_items.reserve(entries.size());
    for (const KopEntry &e : entries) {
        Item it;
        it.e = e;
        const KopCategory c = e.rec.category;
        // deletion candidates arrive pre-checked; review/clean unchecked
        it.check = (c == KOP_CAT_JUNK_FILE || c == KOP_CAT_EXACT_DUPE ||
                    c == KOP_CAT_JUNK_PAGE)
                       ? Qt::Checked : Qt::Unchecked;
        m_items.append(it);
    }
    m_thumbCache.clear();
    endResetModel();
    emit countsChanged();
}

int ResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &it = m_items[index.row()];
    const KopEntry &e = it.e;

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return QFileInfo(e.path).fileName();
    case Qt::DecorationRole: {
        if (e.thumbJpeg.isEmpty())
            return {};
        if (QPixmap *cached = m_thumbCache.object(e.rec.id))
            return *cached;
        auto *pm = new QPixmap;
        pm->loadFromData(e.thumbJpeg, "JPEG");
        m_thumbCache.insert(e.rec.id, pm);
        return *pm;
    }
    case Qt::CheckStateRole:
        return it.check;
    case Qt::ToolTipRole: {
        QString tip = e.path;
        const QString why = reasonsText(e.rec.reasons);
        if (!why.isEmpty())
            tip += QStringLiteral("\n") + why;
        return tip;
    }
    case PathRole:
        return e.path;
    case ChapterLabelRole:
        return e.seriesName.isEmpty()
                   ? e.chapterName
                   : e.seriesName + QStringLiteral(" · ") + e.chapterName;
    case BucketRole:
        return static_cast<int>(bucketFor(e.rec.category, it.status));
    case CategoryRole:
        return static_cast<int>(e.rec.category);
    case ScoreRole:
        return e.rec.score;
    case SizeRole:
        return static_cast<qlonglong>(e.rec.file_size);
    case DupeGroupRole:
        return e.rec.dupe_group;
    case ClusterRole:
        return e.rec.sim_cluster;
    case DhashRole:
        return static_cast<qulonglong>(e.rec.dhash);
    case ReasonsTextRole:
        return reasonsText(e.rec.reasons);
    case IsImageRole:
        return e.rec.kind != KOP_FMT_XML && e.rec.kind != KOP_FMT_NOMEDIA;
    case KindRole:
        return static_cast<int>(e.rec.kind);
    default:
        return {};
    }
}

bool ResultsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole)
        return false;
    m_items[index.row()].check = static_cast<Qt::CheckState>(value.toInt());
    emit dataChanged(index, index, { Qt::CheckStateRole });
    emit countsChanged();
    return true;
}

Qt::ItemFlags ResultsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
}

KopBucket ResultsModel::bucketOf(int row) const
{
    const Item &it = m_items[row];
    return bucketFor(it.e.rec.category, it.status);
}

int ResultsModel::countInBucket(KopBucket b) const
{
    int n = 0;
    for (int i = 0; i < m_items.size(); i++) {
        const KopBucket ib = bucketOf(i);
        if (b == KopBucket::AllFlagged ? ib != KopBucket::Clean : ib == b)
            n++;
    }
    return n;
}

qint64 ResultsModel::checkedBytes() const
{
    qint64 sum = 0;
    for (const Item &it : m_items)
        if (it.check == Qt::Checked)
            sum += static_cast<qint64>(it.e.rec.file_size);
    return sum;
}

int ResultsModel::checkedCount() const
{
    int n = 0;
    for (const Item &it : m_items)
        if (it.check == Qt::Checked)
            n++;
    return n;
}

void ResultsModel::setChecked(int row, bool on)
{
    if (row < 0 || row >= m_items.size())
        return;
    setData(index(row), on ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
}

bool ResultsModel::isChecked(int row) const
{
    return row >= 0 && row < m_items.size() && m_items[row].check == Qt::Checked;
}

void ResultsModel::setStatus(int row, KopUserStatus st)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_items[row].status = st;
    if (st == KOP_ST_ALLOWED || st == KOP_ST_REVIEW)
        m_items[row].check = Qt::Unchecked;
    const QModelIndex ix = index(row);
    emit dataChanged(ix, ix);
    emit countsChanged();
}

QString ResultsModel::pathAt(int row) const
{
    return m_items[row].e.path;
}

const KopEntry &ResultsModel::entryAt(int row) const
{
    return m_items[row].e;
}

void ResultsModel::removeRowsByIndex(QVector<int> rows)
{
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }
    emit countsChanged();
}

/* ---- proxy ------------------------------------------------------------ */

KopSortFilterProxy::KopSortFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void KopSortFilterProxy::setBucket(KopBucket b)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_bucket = b;
    endFilterChange();
#else
    m_bucket = b;
    invalidateFilter();
#endif
}

void KopSortFilterProxy::setSortMode(SortMode m)
{
    m_mode = m;
    invalidate();
    sort(0);
}

bool KopSortFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &parent) const
{
    const QModelIndex ix = sourceModel()->index(sourceRow, 0, parent);
    const auto b = static_cast<KopBucket>(ix.data(ResultsModel::BucketRole).toInt());
    if (m_bucket == KopBucket::AllFlagged)
        return b != KopBucket::Clean;
    return b == m_bucket;
}

bool KopSortFilterProxy::lessThan(const QModelIndex &a, const QModelIndex &b) const
{
    switch (m_mode) {
    case SortMode::Size: {
        const qlonglong sa = a.data(ResultsModel::SizeRole).toLongLong();
        const qlonglong sb = b.data(ResultsModel::SizeRole).toLongLong();
        if (sa != sb)
            return sa > sb; // biggest first: that's what you want to delete
        break;
    }
    case SortMode::Content: {
        // clusters end up adjacent: exact groups, then similarity, then hash
        const int ga = a.data(ResultsModel::DupeGroupRole).toInt();
        const int gb = b.data(ResultsModel::DupeGroupRole).toInt();
        if (ga != gb)
            return (ga < 0 ? INT_MAX : ga) < (gb < 0 ? INT_MAX : gb);
        const int ca = a.data(ResultsModel::ClusterRole).toInt();
        const int cb = b.data(ResultsModel::ClusterRole).toInt();
        if (ca != cb)
            return (ca < 0 ? INT_MAX : ca) < (cb < 0 ? INT_MAX : cb);
        const qulonglong da = a.data(ResultsModel::DhashRole).toULongLong();
        const qulonglong db = b.data(ResultsModel::DhashRole).toULongLong();
        if (da != db)
            return da < db;
        break;
    }
    case SortMode::Name:
        break;
    }
    return QString::localeAwareCompare(a.data(ResultsModel::PathRole).toString(),
                                       b.data(ResultsModel::PathRole).toString()) < 0;
}
