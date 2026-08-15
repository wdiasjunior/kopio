// Paints one grid cell: thumbnail, checkbox, category badge, two-line caption.
#pragma once

#include <QStyledItemDelegate>

class ResultsDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    static constexpr int CellW = 180;
    static constexpr int CellH = 244;
    static constexpr int ThumbH = 190;

    explicit ResultsDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};
