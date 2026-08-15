#include <QApplication>
#include <QDir>
#include <QTimer>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kopio"));
    QCoreApplication::setOrganizationName(QStringLiteral("kopio"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    MainWindow win;
    win.show();

    // optional: pass a library directory to scan it right away
    const QStringList args = app.arguments();
    if (args.size() > 1 && QDir(args.at(1)).exists())
        win.startScan(args.at(1));

    // test hook: KOPIO_SCREENSHOT=/path/out.png grabs the window and exits
    const QString shot = qEnvironmentVariable("KOPIO_SCREENSHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(6000, &win, [&win, &app, shot] {
            QWidget *target = QApplication::activeModalWidget();
            if (!target)
                target = &win;
            target->grab().save(shot);
            app.quit();
        });
    }

    return app.exec();
}
