// Triage screen: filter chips, sortable thumbnail grid, and the action bar.
#pragma once

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

signals:
    void newScanRequested();

private:
    void updateChips();
    void updateStatusLine();
    void selectAllVisible(bool checked);
    void applyStatusToChecked(KopUserStatus st);
    void deleteChecked();
    void deleteRows(const QVector<int> &sourceRows);
    void onGridClicked(const QModelIndex &proxyIndex);
    QVector<int> checkedVisibleSourceRows() const;

    ResultsModel *m_model = nullptr;
    KopSortFilterProxy *m_proxy = nullptr;
    QListView *m_view = nullptr;
    PreviewDialog *m_preview = nullptr;
    QLabel *m_statusLine = nullptr;
    QComboBox *m_sortCombo = nullptr;
    QVector<QToolButton *> m_chips; // indexed by KopBucket
};
