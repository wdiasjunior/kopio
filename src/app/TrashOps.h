// The one platform seam for deletion: everything goes through moveToTrash.
// Set KOPIO_DRY_RUN=1 to log instead of trashing.
#pragma once

#include <QString>
#include <QStringList>

namespace TrashOps {

struct Result {
    int ok = 0;
    QStringList failed;
};

Result trash(const QStringList &paths);
bool dryRun();

} // namespace TrashOps
