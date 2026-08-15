#include "ScanController.h"

#include <QtConcurrent>

extern "C" {
#include "kop_classify.h"
}

using namespace kop_pipeline;

ScanController::ScanController(QObject *parent)
    : QObject(parent)
{
}

ScanController::~ScanController()
{
    cancel();
    m_future.waitForFinished();
}

bool ScanController::isRunning() const
{
    return m_future.isRunning();
}

void ScanController::cancel()
{
    m_cancel.store(true);
}

void ScanController::start(const QString &root)
{
    if (isRunning())
        return;
    m_cancel.store(false);
    m_analyzed.store(0);

    m_future = QtConcurrent::run([this, root]() {
        QStringList files = collectFiles(root);
        emit filesFound(static_cast<int>(files.size()));

        QVector<KopEntry> entries;
        entries.reserve(files.size());
        for (const QString &p : files) {
            KopEntry e;
            e.path = p;
            entries.append(e);
        }
        assignIds(root, entries);

        for (qsizetype i = 0; i < entries.size(); i++) {
            entries[i].rec.id = static_cast<int32_t>(i);
            stage1(entries[i]);
        }

        const int total = static_cast<int>(entries.size());
        // parallel per-image analysis; metadata files need no decode
        QtConcurrent::blockingMap(entries, [this, total](KopEntry &e) {
            if (!m_cancel.load()) {
                if (e.rec.kind != KOP_FMT_XML && e.rec.kind != KOP_FMT_NOMEDIA)
                    analyzeImage(e, true);
            }
            const int done = m_analyzed.fetch_add(1) + 1;
            if ((done & 0x0F) == 0 || done == total)
                emit progress(done, total);
        });

        const bool cancelled = m_cancel.load();
        if (!cancelled && !entries.isEmpty()) {
            QVector<KopRecord> recs(entries.size());
            for (qsizetype i = 0; i < entries.size(); i++)
                recs[i] = entries[i].rec;
            KopClassifyParams params;
            kop_classify_defaults(&params);
            kop_classify(recs.data(), static_cast<int>(recs.size()), &params);
            for (qsizetype i = 0; i < entries.size(); i++)
                entries[i].rec = recs[i];
        }
        emit finished(entries, cancelled);
    });
}
