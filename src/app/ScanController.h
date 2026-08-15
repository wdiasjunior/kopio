// Runs the scan pipeline off the GUI thread:
// walk + sniff, then parallel per-image analysis, then one classify pass.
#pragma once

#include <QFuture>
#include <QObject>
#include <QVector>

#include <atomic>

#include "Pipeline.h"

class ScanController : public QObject
{
    Q_OBJECT
public:
    explicit ScanController(QObject *parent = nullptr);
    ~ScanController() override;

    void start(const QString &root);
    void cancel();
    bool isRunning() const;

signals:
    void filesFound(int count);
    void progress(int analyzed, int total);
    void finished(const QVector<KopEntry> &entries, bool cancelled);

private:
    std::atomic<bool> m_cancel { false };
    std::atomic<int> m_analyzed { 0 };
    QFuture<void> m_future;
};
