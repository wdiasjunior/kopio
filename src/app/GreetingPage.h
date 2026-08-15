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
    QLabel *m_pathLabel = nullptr;
    QPushButton *m_startButton = nullptr;
};
