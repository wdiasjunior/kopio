#include "GreetingPage.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

GreetingPage::GreetingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->addStretch(2);

    auto *icon = new QLabel(this);
    const QIcon appIcon = QIcon::fromTheme(QStringLiteral("edit-find"));
    icon->setPixmap(appIcon.pixmap(96, 96));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto *title = new QLabel(QStringLiteral("<h1>kopio</h1>"), this);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *blurb = new QLabel(
        tr("Find scanlation credit pages, blank padding, leftover .nomedia and\n"
           "ComicInfo.xml files in your manga library, and move them to the trash."),
        this);
    blurb->setAlignment(Qt::AlignCenter);
    layout->addWidget(blurb);

    layout->addSpacing(24);

    auto *choose = new QPushButton(QIcon::fromTheme(QStringLiteral("folder-open")),
                                   tr("Choose library folder…"), this);
    choose->setMinimumHeight(40);
    connect(choose, &QPushButton::clicked, this, &GreetingPage::chooseDirectory);
    auto *chooseRow = new QHBoxLayout;
    chooseRow->addStretch();
    chooseRow->addWidget(choose);
    chooseRow->addStretch();
    layout->addLayout(chooseRow);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setWordWrap(true);
    layout->addWidget(m_pathLabel);

    m_startButton = new QPushButton(QIcon::fromTheme(QStringLiteral("system-search")),
                                    tr("Scan this folder recursively"), this);
    m_startButton->setMinimumHeight(40);
    m_startButton->setVisible(false);
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (!m_dir.isEmpty())
            emit scanRequested(m_dir);
    });
    auto *startRow = new QHBoxLayout;
    startRow->addStretch();
    startRow->addWidget(m_startButton);
    startRow->addStretch();
    layout->addLayout(startRow);

    layout->addStretch(3);
}

void GreetingPage::reset()
{
    m_dir.clear();
    m_pathLabel->clear();
    m_startButton->setVisible(false);
}

void GreetingPage::chooseDirectory()
{
    const QString start = m_dir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        : m_dir;
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose the manga library folder"), start);
    if (dir.isEmpty())
        return;
    m_dir = dir;
    m_pathLabel->setText(tr("Selected: <b>%1</b>").arg(dir.toHtmlEscaped()));
    m_startButton->setVisible(true);
}
