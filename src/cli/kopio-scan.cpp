// kopio-scan: headless analysis harness. Runs the exact same core pipeline as
// the GUI and prints one TSV row per file, for tuning thresholds on a corpus.
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTextStream>

#include "Pipeline.h"

extern "C" {
#include "kop_classify.h"
}

using namespace kop_pipeline;

int main(int argc, char *argv[])
{
    // Allow running without a display server (e.g. over ssh on the archive box).
    if (qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kopio-scan"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Scan a manga library tree and report junk-page analysis as TSV."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("dir"), QStringLiteral("Directory to scan."));
    QCommandLineOption classifyOpt(QStringLiteral("classify"),
                                   QStringLiteral("Run the classifier and print verdicts."));
    parser.addOption(classifyOpt);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1)
        parser.showHelp(2);

    QTextStream out(stdout), err(stderr);

    const QStringList files = collectFiles(args.first());
    QVector<KopEntry> entries;
    entries.reserve(files.size());
    for (const QString &p : files) {
        KopEntry e;
        e.path = p;
        entries.append(e);
    }
    assignIds(args.first(), entries);

    for (qsizetype i = 0; i < entries.size(); i++) {
        KopEntry &e = entries[i];
        e.rec.id = static_cast<int32_t>(i);
        stage1(e);
        const KopFileKind k = e.rec.kind;
        if (k != KOP_FMT_XML && k != KOP_FMT_NOMEDIA) {
            if (!analyzeImage(e, false) && e.rec.file_size > 0)
                err << "warn: could not decode " << e.path << "\n";
        }
    }

    const bool classify = parser.isSet(classifyOpt);
    if (classify) {
        QVector<KopRecord> recs(entries.size());
        for (qsizetype i = 0; i < entries.size(); i++)
            recs[i] = entries[i].rec;
        KopClassifyParams params;
        kop_classify_defaults(&params);
        if (kop_classify(recs.data(), static_cast<int>(recs.size()), &params) != 0) {
            err << "error: classification failed\n";
            return 1;
        }
        for (qsizetype i = 0; i < entries.size(); i++)
            entries[i].rec = recs[i];
    }

    out << "path\tkind\twidth\theight\taspect\tbytes\tbpp\tcolor\twhite\tink\tedge\tuniq\tborder\tdhash";
    if (classify)
        out << "\tcategory\tscore\tdupe_group\tsim_cluster\treasons";
    out << "\n";

    for (const KopEntry &e : entries) {
        const KopRecord &r = e.rec;
        const double px = double(r.hdr.width) * double(r.hdr.height);
        out << e.path << '\t' << kindName(r.kind) << '\t'
            << r.hdr.width << '\t' << r.hdr.height << '\t'
            << QString::number(r.hdr.height ? double(r.hdr.width) / r.hdr.height : 0.0, 'f', 3) << '\t'
            << r.file_size << '\t'
            << QString::number(px > 0 ? double(r.file_size) / px : 0.0, 'f', 4) << '\t'
            << (r.m.is_color ? "color" : "gray") << '\t'
            << QString::number(r.m.white_ratio, 'f', 3) << '\t'
            << QString::number(r.m.ink_ratio, 'f', 3) << '\t'
            << QString::number(r.m.edge_density, 'f', 4) << '\t'
            << r.m.unique_colors << '\t'
            << QString::number(r.m.border_white, 'f', 3) << '\t'
            << QString::number(r.dhash, 16);
        if (classify) {
            out << '\t' << categoryName(r.category) << '\t'
                << QString::number(r.score, 'f', 2) << '\t'
                << r.dupe_group << '\t' << r.sim_cluster << '\t'
                << reasonsText(r.reasons);
        }
        out << "\n";
    }
    return 0;
}
