#include "MainWindow.h"

#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>

#include "GreetingPage.h"
#include "ResultsPage.h"
#include "ScanController.h"
#include "ScanPage.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("kopio"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    resize(1100, 800);

    m_stack = new QStackedWidget(this);
    m_greeting = new GreetingPage(this);
    m_scan = new ScanPage(this);
    m_results = new ResultsPage(this);
    m_stack->addWidget(m_greeting);
    m_stack->addWidget(m_scan);
    m_stack->addWidget(m_results);
    setCentralWidget(m_stack);

    m_controller = new ScanController(this);

    connect(m_greeting, &GreetingPage::scanRequested, this, &MainWindow::startScan);
    connect(m_scan, &ScanPage::cancelRequested, this, [this] {
        m_controller->cancel();
    });
    connect(m_results, &ResultsPage::newScanRequested, this, [this] {
        m_greeting->reset();
        m_stack->setCurrentWidget(m_greeting);
    });

    connect(m_controller, &ScanController::filesFound, m_scan, &ScanPage::setFound);
    connect(m_controller, &ScanController::progress, m_scan, &ScanPage::setProgress);
    connect(m_controller, &ScanController::finished, this,
            [this](const QVector<KopEntry> &entries, bool cancelled) {
                if (cancelled) {
                    m_greeting->reset();
                    m_stack->setCurrentWidget(m_greeting);
                    return;
                }
                m_results->setEntries(entries);
                m_stack->setCurrentWidget(m_results);
            });

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *open = fileMenu->addAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                                        tr("&New Scan…"));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        if (m_controller->isRunning())
            return;
        m_greeting->reset();
        m_stack->setCurrentWidget(m_greeting);
    });
    QAction *quit = fileMenu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")),
                                        tr("&Quit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *about = helpMenu->addAction(tr("&About kopio"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this, tr("About kopio"),
            tr("<b>kopio</b> — finds scanlation credit pages, blank padding and "
               "Mihon leftovers in a manga library, and moves them to the trash.<br><br>"
               "Click a thumbnail to check it, Ctrl+click to preview it."));
    });
}

void MainWindow::startScan(const QString &dir)
{
    m_scan->begin(dir);
    m_stack->setCurrentWidget(m_scan);
    m_controller->start(dir);
}
