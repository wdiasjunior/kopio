// Enlarged preview of a single item, opened with Ctrl+click on the grid.
#pragma once

#include <QDialog>
#include <QPixmap>

class QLabel;
class QScrollArea;

class PreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreviewDialog(QWidget *parent = nullptr);

    // sourceRow refers to the results model, not the proxy
    void showEntry(int sourceRow, const QString &path, const QString &caption,
                   const QString &reasons, bool isImage);
    int sourceRow() const { return m_sourceRow; }

signals:
    void deleteRequested(int sourceRow);
    void reviewRequested(int sourceRow);
    void allowRequested(int sourceRow);

protected:
    bool eventFilter(QObject *watched, QEvent *ev) override;

private:
    void updateScaled();

    int m_sourceRow = -1;
    QPixmap m_full;
    QLabel *m_image = nullptr;
    QLabel *m_caption = nullptr;
    QScrollArea *m_scroll = nullptr;
};
