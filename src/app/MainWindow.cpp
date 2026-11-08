#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>

#include "GreetingPage.h"
#include "ResultsPage.h"
#include "ScanController.h"
#include "ScanPage.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("kopio"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("kopio"),
                                   QIcon::fromTheme(QStringLiteral("edit-find"))));
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
                m_results->setEntries(entries, preselect());
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
    // close() rather than qApp->quit() so Ctrl+Q also gets the closeEvent warning
    connect(quit, &QAction::triggered, this, &QWidget::close);

    buildSettingsMenu();

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

// Settings → which flagged files arrive checked after a scan. Choices are
// saved with QSettings and take effect on the next scan.
void MainWindow::buildSettingsMenu()
{
    QMenu *menu = menuBar()->addMenu(tr("&Settings"));
    menu->addSection(tr("Pre-select after a scan"));
    const KopPreselect defaults;
    auto addOption = [this, menu](const QString &text, const QString &key, bool def) {
        QAction *a = menu->addAction(text);
        a->setCheckable(true);
        a->setChecked(QSettings().value(key, def).toBool());
        connect(a, &QAction::toggled, this,
                [key](bool on) { QSettings().setValue(key, on); });
        return a;
    };
    m_preAllFlagged = addOption(tr("Everything in \"All flagged\""),
                                QStringLiteral("preselect/allFlagged"), defaults.allFlagged);
    menu->addSeparator();
    m_preDupes = addOption(tr("Duplicates"),
                           QStringLiteral("preselect/dupes"), defaults.dupes);
    m_preJunkFiles = addOption(tr("Junk files"),
                               QStringLiteral("preselect/junkFiles"), defaults.junkFiles);
    m_preJunkPages = addOption(tr("Junk images"),
                               QStringLiteral("preselect/junkPages"), defaults.junkPages);

    // "All flagged" already includes the other three; grey them out meanwhile
    auto sync = [this](bool all) {
        for (QAction *a : { m_preDupes, m_preJunkFiles, m_preJunkPages })
            a->setEnabled(!all);
    };
    connect(m_preAllFlagged, &QAction::toggled, this, sync);
    sync(m_preAllFlagged->isChecked());
}

KopPreselect MainWindow::preselect() const
{
    KopPreselect p;
    p.allFlagged = m_preAllFlagged->isChecked();
    p.dupes = m_preDupes->isChecked();
    p.junkFiles = m_preJunkFiles->isChecked();
    p.junkPages = m_preJunkPages->isChecked();
    return p;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // checked files on the results screen haven't been trashed yet
    const int checked = m_results->checkedCount();
    if (m_stack->currentWidget() == m_results && checked > 0) {
        const auto answer = QMessageBox::warning(
            this, tr("Quit kopio?"),
            tr("%n checked file(s) haven't been moved to the trash yet.\n"
               "Quit anyway?", nullptr, checked),
            QMessageBox::Close | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Close) {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::startScan(const QString &dir)
{
    m_scan->begin(dir);
    m_stack->setCurrentWidget(m_scan);
    m_controller->start(dir);
}
