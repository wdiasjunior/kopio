#include "ScanPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ScanPage::ScanPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->addStretch(2);

    m_dirLabel = new QLabel(this);
    m_dirLabel->setAlignment(Qt::AlignCenter);
    m_dirLabel->setWordWrap(true);
    layout->addWidget(m_dirLabel);

    m_bar = new QProgressBar(this);
    m_bar->setMaximumWidth(480);
    auto *barRow = new QHBoxLayout;
    barRow->addStretch();
    barRow->addWidget(m_bar);
    barRow->addStretch();
    layout->addLayout(barRow);

    m_status = new QLabel(this);
    m_status->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_status);

    auto *cancel = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")),
                                   tr("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &ScanPage::cancelRequested);
    auto *cancelRow = new QHBoxLayout;
    cancelRow->addStretch();
    cancelRow->addWidget(cancel);
    cancelRow->addStretch();
    layout->addLayout(cancelRow);

    layout->addStretch(3);
}

void ScanPage::begin(const QString &dir)
{
    m_dirLabel->setText(tr("Scanning <b>%1</b>…").arg(dir.toHtmlEscaped()));
    m_status->setText(tr("Looking for files…"));
    m_bar->setRange(0, 0); // indeterminate while walking
}

void ScanPage::setFound(int count)
{
    m_status->setText(tr("%1 files found, analyzing…").arg(count));
}

void ScanPage::setProgress(int analyzed, int total)
{
    m_bar->setRange(0, total);
    m_bar->setValue(analyzed);
    m_status->setText(tr("Analyzed %1 of %2 files").arg(analyzed).arg(total));
}
