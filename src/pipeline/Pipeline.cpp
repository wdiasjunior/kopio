#include "Pipeline.h"

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QObject>

#include <cstdlib>
#include <cstring>

#define XXH_INLINE_ALL
#include "xxhash.h"

extern "C" {
#include "kop_hash.h"
#include "kop_meta.h"
#include "kop_metrics.h"
}

namespace kop_pipeline {

QStringList collectFiles(const QString &root)
{
    QStringList files;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
        files.append(it.next());
    files.sort();
    return files;
}

void stage1(KopEntry &e)
{
    QFileInfo fi(e.path);
    e.rec.file_size = static_cast<uint64_t>(fi.size());
    e.rec.kind = KOP_FMT_UNKNOWN;
    memset(&e.rec.hdr, 0, sizeof(e.rec.hdr));

    const QString name = fi.fileName();
    if (name.compare(QStringLiteral(".nomedia"), Qt::CaseInsensitive) == 0) {
        e.rec.kind = KOP_FMT_NOMEDIA;
        return;
    }
    if (name.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive)) {
        e.rec.kind = KOP_FMT_XML;
        return;
    }

    QFile f(e.path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    // 64 KB covers JPEGs with bulky EXIF/ICC blocks before the SOF marker.
    const QByteArray head = f.read(64 * 1024);
    kop_sniff(reinterpret_cast<const uint8_t *>(head.constData()),
              static_cast<size_t>(head.size()), &e.rec.kind, &e.rec.hdr);
}

bool analyzeImage(KopEntry &e, bool makeThumb)
{
    e.rec.decoded = 0;

    QFile f(e.path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    e.rec.file_size = static_cast<uint64_t>(data.size());

    const XXH128_hash_t h = XXH3_128bits(data.constData(), static_cast<size_t>(data.size()));
    e.rec.chash_lo = h.low64;
    e.rec.chash_hi = h.high64;

    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    reader.setAutoTransform(true);
    QImage img = reader.read(); // first frame for animated formats
    if (img.isNull())
        return false;

    img = img.convertToFormat(QImage::Format_RGBA8888);
    if (img.isNull())
        return false;

    const int w = img.width(), hgt = img.height();
    e.rec.hdr.width = static_cast<uint32_t>(w);
    e.rec.hdr.height = static_cast<uint32_t>(hgt);
    e.rec.hdr.has_alpha = img.hasAlphaChannel();

    const uint8_t *rgba = img.constBits(); // RGBA8888: stride == width*4

    if (kop_compute_metrics(rgba, w, hgt, &e.rec.m) != 0)
        return false;
    e.rec.hdr.is_grayscale = !e.rec.m.is_color;

    QVector<uint8_t> gray(static_cast<qsizetype>(w) * hgt);
    kop_rgba_to_gray(rgba, w, hgt, gray.data());
    uint8_t small[9 * 8];
    kop_gray_downscale(gray.constData(), w, hgt, small, 9, 8);
    e.rec.dhash = kop_dhash64(small);
    e.rec.decoded = 1;

    if (makeThumb) {
        QImage thumb = img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QBuffer out(&e.thumbJpeg);
        out.open(QIODevice::WriteOnly);
        thumb.convertToFormat(QImage::Format_RGB32).save(&out, "JPEG", 85);
    }
    return true;
}

void assignIds(const QString &root, QVector<KopEntry> &entries)
{
    const QString rootPath = QDir(root).absolutePath();
    QHash<QString, int32_t> chapterIds, seriesIds;

    for (KopEntry &e : entries) {
        QDir chapterDir = QFileInfo(e.path).dir();
        QString chapterPath = chapterDir.absolutePath();
        QString seriesPath;
        if (chapterPath == rootPath) {
            seriesPath = rootPath;
        } else {
            QDir seriesDir = chapterDir;
            seriesDir.cdUp();
            seriesPath = seriesDir.absolutePath();
        }

        e.chapterName = QDir(chapterPath).dirName();
        e.seriesName = seriesPath == rootPath && chapterPath == rootPath
                           ? QString()
                           : QDir(seriesPath).dirName();

        auto cit = chapterIds.constFind(chapterPath);
        if (cit == chapterIds.constEnd())
            cit = chapterIds.insert(chapterPath, static_cast<int32_t>(chapterIds.size()));
        e.rec.chapter_id = cit.value();

        auto sit = seriesIds.constFind(seriesPath);
        if (sit == seriesIds.constEnd())
            sit = seriesIds.insert(seriesPath, static_cast<int32_t>(seriesIds.size()));
        e.rec.series_id = sit.value();
    }
}

QString kindName(KopFileKind k)
{
    switch (k) {
    case KOP_FMT_JPEG: return QStringLiteral("jpeg");
    case KOP_FMT_PNG: return QStringLiteral("png");
    case KOP_FMT_GIF: return QStringLiteral("gif");
    case KOP_FMT_WEBP: return QStringLiteral("webp");
    case KOP_FMT_XML: return QStringLiteral("xml");
    case KOP_FMT_NOMEDIA: return QStringLiteral("nomedia");
    default: return QStringLiteral("unknown");
    }
}

QString categoryName(KopCategory c)
{
    switch (c) {
    case KOP_CAT_JUNK_FILE: return QStringLiteral("junk-file");
    case KOP_CAT_EXACT_DUPE: return QStringLiteral("exact-dupe");
    case KOP_CAT_JUNK_PAGE: return QStringLiteral("junk-page");
    case KOP_CAT_REVIEW: return QStringLiteral("review");
    case KOP_CAT_CLEAN: return QStringLiteral("clean");
    default: return QStringLiteral("unscanned");
    }
}

QString reasonsText(uint32_t reasons)
{
    QStringList parts;
    if (reasons & KOP_R_NOMEDIA) parts << QObject::tr(".nomedia marker");
    if (reasons & KOP_R_XML) parts << QObject::tr("metadata XML");
    if (reasons & KOP_R_EXACT_DUPE) parts << QObject::tr("exact duplicate");
    if (reasons & KOP_R_FREQ_CLUSTER) parts << QObject::tr("repeats across chapters");
    if (reasons & KOP_R_WIDE_BANNER) parts << QObject::tr("wide banner shape");
    if (reasons & KOP_R_SQUARE) parts << QObject::tr("square logo shape");
    if (reasons & KOP_R_BLANK) parts << QObject::tr("blank page");
    if (reasons & KOP_R_TITLE_ONLY) parts << QObject::tr("title-only page");
    if (reasons & KOP_R_TEXT_HEAVY) parts << QObject::tr("text-heavy (notes?)");
    if (reasons & KOP_R_COLOR_GUARD) parts << QObject::tr("looks like cover/spread");
    if (reasons & KOP_R_FLAT_COLOR) parts << QObject::tr("flat color art");
    return parts.join(QStringLiteral(", "));
}

} // namespace kop_pipeline
