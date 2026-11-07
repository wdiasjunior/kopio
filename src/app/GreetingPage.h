// First screen: pick a library directory and confirm the scan.
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class GreetingPage : public QWidget
{
    Q_OBJECT
public:
    explicit GreetingPage(QWidget *parent = nullptr);

    void reset();

signals:
    void scanRequested(const QString &dir);

private:
    void chooseDirectory();

    QString m_dir;
    // Last folder picked in this process. In memory only (each launch starts at
    // home again) and deliberately kept across reset(), so "New scan" reopens
    // the picker next to the previous folder.
    QString m_lastDir;
    QLabel *m_pathLabel = nullptr;
    QPushButton *m_startButton = nullptr;
};
