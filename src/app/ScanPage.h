// Progress screen while the scan pipeline runs.
#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;

class ScanPage : public QWidget
{
    Q_OBJECT
public:
    explicit ScanPage(QWidget *parent = nullptr);

    void begin(const QString &dir);
    void setFound(int count);
    void setProgress(int analyzed, int total);

signals:
    void cancelRequested();

private:
    QLabel *m_dirLabel = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_bar = nullptr;
};
