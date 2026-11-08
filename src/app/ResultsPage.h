// Triage screen: filter chips, sortable thumbnail grid, and the action bar.
#pragma once

#include <array>

#include <QPersistentModelIndex>
#include <QWidget>

#include "Pipeline.h"
#include "ResultsModel.h"

class QComboBox;
class QLabel;
class QListView;
class QToolButton;
class PreviewDialog;

class ResultsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsPage(QWidget *parent = nullptr);

    void setEntries(const QVector<KopEntry> &entries);
    int checkedCount() const { return m_model->checkedCount(); }

signals:
    void newScanRequested();

private:
    void resetScrollPositions();
    void updateChips();
    void updateStatusLine();
    void selectAllVisible(bool checked);
    void applyStatusToChecked(KopUserStatus st);
    void deleteChecked();
    void deleteRows(const QVector<int> &sourceRows);
    void onGridClicked(const QModelIndex &proxyIndex);
    void openPreview(const QModelIndex &sourceIndex);
    QVector<int> checkedVisibleSourceRows() const;

    ResultsModel *m_model = nullptr;
    KopSortFilterProxy *m_proxy = nullptr;
    QListView *m_view = nullptr;
    PreviewDialog *m_preview = nullptr;
    QLabel *m_statusLine = nullptr;
    QComboBox *m_sortCombo = nullptr;
    QVector<QToolButton *> m_chips; // indexed by KopBucket
    KopBucket m_bucket = KopBucket::AllFlagged; // chip currently shown
    // each chip's last vertical scroll offset, indexed by KopBucket
    std::array<int, size_t(KopBucket::Count)> m_scrollPos {};
    // last plain-clicked card (source model) for Shift+click range select
    QPersistentModelIndex m_anchor;
};
