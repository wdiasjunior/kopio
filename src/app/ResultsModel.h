// List model over scan results, plus the filter/sort proxy the grid uses.
#pragma once

#include <QAbstractListModel>
#include <QCache>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <QVector>

#include "Pipeline.h"

// Buckets shown as filter chips. Allowed/Review are user statuses that
// override the classifier category for display purposes.
enum class KopBucket {
    AllFlagged, // everything except clean
    JunkFiles,
    ExactDupes,
    JunkPages,
    Review,
    Allowed,
    Clean,
    Count
};

class ResultsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        ChapterLabelRole, // "Series · Chapter"
        BucketRole,
        CategoryRole,
        ScoreRole,
        SizeRole,
        DupeGroupRole,
        ClusterRole,
        DhashRole,
        ReasonsTextRole,
        IsImageRole,
        KindRole, // KopFileKind
    };

    explicit ResultsModel(QObject *parent = nullptr);

    void setEntries(const QVector<KopEntry> &entries);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    KopBucket bucketOf(int row) const;
    int countInBucket(KopBucket b) const;
    qint64 checkedBytes() const;
    int checkedCount() const;

    void setChecked(int row, bool on);
    bool isChecked(int row) const;
    void setStatus(int row, KopUserStatus st);
    QString pathAt(int row) const;
    const KopEntry &entryAt(int row) const;
    void removeRowsByIndex(QVector<int> rows); // after successful trash

signals:
    void countsChanged();

private:
    struct Item {
        KopEntry e;
        Qt::CheckState check = Qt::Unchecked;
        KopUserStatus status = KOP_ST_PENDING;
    };
    QVector<Item> m_items;
    mutable QCache<int, QPixmap> m_thumbCache { 512 };
};

class KopSortFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum class SortMode { Name, Content, Size };

    explicit KopSortFilterProxy(QObject *parent = nullptr);

    void setBucket(KopBucket b);
    void setSortMode(SortMode m);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &a, const QModelIndex &b) const override;

private:
    KopBucket m_bucket = KopBucket::AllFlagged;
    SortMode m_mode = SortMode::Content;
};
