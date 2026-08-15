#pragma once

#include <QMainWindow>

class QStackedWidget;
class GreetingPage;
class ScanPage;
class ResultsPage;
class ScanController;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void startScan(const QString &dir);

private:

    QStackedWidget *m_stack = nullptr;
    GreetingPage *m_greeting = nullptr;
    ScanPage *m_scan = nullptr;
    ResultsPage *m_results = nullptr;
    ScanController *m_controller = nullptr;
};
