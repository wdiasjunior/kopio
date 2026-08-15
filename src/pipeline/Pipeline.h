// Shared scan/analysis pipeline used by both the CLI and the GUI.
// Owns all path handling; the C core never sees a file path.
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

extern "C" {
#include "kop_types.h"
}

struct KopEntry {
    QString path;
    QString seriesName;
    QString chapterName;
    KopRecord rec {};
    QByteArray thumbJpeg; // 256px JPEG preview, empty unless requested
};

namespace kop_pipeline {

// Recursively collect every regular file under root, sorted by path.
QStringList collectFiles(const QString &root);

// Stage 1: stat + name rules (.nomedia, *.xml) + content sniff of the head.
// Fills rec.kind, rec.file_size, rec.hdr. Cheap; no full decode.
void stage1(KopEntry &e);

// Stage 2 (image kinds only): full read, XXH3-128, decode, metrics, dHash,
// optional thumbnail. Sets rec.decoded. Returns false when decoding failed.
bool analyzeImage(KopEntry &e, bool makeThumb);

// Derive series/chapter names and assign stable series_id/chapter_id values.
// Chapter = directory containing the file; series = its parent (clamped to root).
void assignIds(const QString &root, QVector<KopEntry> &entries);

QString kindName(KopFileKind k);
QString categoryName(KopCategory c);
QString reasonsText(uint32_t reasons);

} // namespace kop_pipeline
