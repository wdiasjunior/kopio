#include "TrashOps.h"

#include <QDebug>
#include <QFile>

namespace TrashOps {

bool dryRun()
{
    static const bool dry = qEnvironmentVariableIntValue("KOPIO_DRY_RUN") != 0;
    return dry;
}

Result trash(const QStringList &paths)
{
    Result r;
    for (const QString &p : paths) {
        if (dryRun()) {
            qInfo() << "[dry-run] would trash" << p;
            r.ok++;
            continue;
        }
        QFile f(p);
        if (f.moveToTrash())
            r.ok++;
        else
            r.failed.append(p);
    }
    return r;
}

} // namespace TrashOps
