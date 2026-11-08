#pragma once

#include <QMainWindow>

class QStackedWidget;
class GreetingPage;
class ScanPage;
class ResultsPage;
class QAction;
class ScanController;
struct KopPreselect;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void startScan(const QString &dir);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildSettingsMenu();
    KopPreselect preselect() const;

    QAction *m_preAllFlagged = nullptr;
    QAction *m_preDupes = nullptr;
    QAction *m_preJunkFiles = nullptr;
    QAction *m_preJunkPages = nullptr;
    QStackedWidget *m_stack = nullptr;
    GreetingPage *m_greeting = nullptr;
    ScanPage *m_scan = nullptr;
    ResultsPage *m_results = nullptr;
    ScanController *m_controller = nullptr;
};
