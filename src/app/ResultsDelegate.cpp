#include "ResultsDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "ResultsModel.h"

extern "C" {
#include "kop_types.h"
}

namespace {

struct BadgeStyle {
    QColor color;
    const char *label;
};

BadgeStyle badgeFor(KopBucket b)
{
    switch (b) {
    case KopBucket::JunkFiles: return { QColor(0xda, 0x44, 0x53), "meta" };
    case KopBucket::ExactDupes: return { QColor(0xf6, 0x74, 0x00), "dupe" };
    case KopBucket::JunkPages: return { QColor(0xe9, 0x3a, 0x9a), "junk" };
    case KopBucket::Review: return { QColor(0x24, 0x6f, 0xcd), "review" };
    case KopBucket::Allowed: return { QColor(0x27, 0xae, 0x60), "ok" };
    default: return { QColor(0x7f, 0x8c, 0x8d), "" };
    }
}

} // namespace

ResultsDelegate::ResultsDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize ResultsDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return { CellW, CellH };
}

void ResultsDelegate::paint(QPainter *p, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const QRect cell = option.rect.adjusted(4, 4, -4, -4);
    const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    const auto bucket = static_cast<KopBucket>(index.data(ResultsModel::BucketRole).toInt());

    // card background; checked items get a tinted frame
    QColor bg = option.palette.base().color();
    QColor frame = option.palette.mid().color();
    if (checked) {
        frame = option.palette.highlight().color();
        bg = option.palette.highlight().color();
        bg.setAlpha(24);
    }
    QPainterPath card;
    card.addRoundedRect(cell, 6, 6);
    p->fillPath(card, bg);
    p->setPen(QPen(frame, checked ? 2 : 1));
    p->drawPath(card);

    // thumbnail (or a placeholder for metadata files)
    const QRect thumbArea(cell.x() + 4, cell.y() + 4, cell.width() - 8, ThumbH - 8);
    const QPixmap pm = qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));
    if (!pm.isNull()) {
        QPixmap scaled = pm.scaled(thumbArea.size(), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        const QPoint at(thumbArea.x() + (thumbArea.width() - scaled.width()) / 2,
                        thumbArea.y() + (thumbArea.height() - scaled.height()) / 2);
        p->drawPixmap(at, scaled);
    } else {
        const auto kind =
            static_cast<KopFileKind>(index.data(ResultsModel::KindRole).toInt());
        QString iconName;
        switch (kind) {
        case KOP_FMT_NOMEDIA: iconName = QStringLiteral("text-plain"); break;
        case KOP_FMT_XML: iconName = QStringLiteral("application-xml"); break;
        default: iconName = QStringLiteral("image-missing"); break;
        }
        QIcon icon = QIcon::fromTheme(iconName);
        if (icon.isNull()) // no icon theme available (e.g. inside an AppImage)
            icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
        // keep theme icons at a crisp native size instead of stretching
        const QRect iconRect(thumbArea.center().x() - 32,
                             thumbArea.center().y() - 32, 64, 64);
        icon.paint(p, iconRect, Qt::AlignCenter);
    }

    // checkbox top-left
    QStyleOptionButton cb;
    cb.state = QStyle::State_Enabled | (checked ? QStyle::State_On : QStyle::State_Off);
    cb.rect = QRect(cell.x() + 8, cell.y() + 8, 20, 20);
    QApplication::style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cb, p);

    // category badge top-right
    const BadgeStyle badge = badgeFor(bucket);
    if (badge.label[0]) {
        QFont f = option.font;
        f.setPointSizeF(f.pointSizeF() * 0.75);
        f.setBold(true);
        p->setFont(f);
        const QString text = QLatin1String(badge.label);
        const QRect br = p->fontMetrics().boundingRect(text).adjusted(-6, -2, 6, 2);
        QRect badgeRect(cell.right() - br.width() - 6, cell.y() + 8, br.width(), br.height());
        QPainterPath bp;
        bp.addRoundedRect(badgeRect, badgeRect.height() / 2.0, badgeRect.height() / 2.0);
        p->fillPath(bp, badge.color);
        p->setPen(Qt::white);
        p->drawText(badgeRect, Qt::AlignCenter, text);
    }

    // two-line caption
    p->setFont(option.font);
    const QRect line1(cell.x() + 6, cell.y() + ThumbH, cell.width() - 12,
                      p->fontMetrics().height());
    const QRect line2(cell.x() + 6, line1.bottom() + 2, cell.width() - 12,
                      p->fontMetrics().height());
    p->setPen(option.palette.text().color());
    p->drawText(line1, Qt::AlignHCenter,
                p->fontMetrics().elidedText(index.data(ResultsModel::NameRole).toString(),
                                            Qt::ElideMiddle, line1.width()));
    QColor sub = option.palette.text().color();
    sub.setAlpha(160);
    p->setPen(sub);
    p->drawText(line2, Qt::AlignHCenter,
                p->fontMetrics().elidedText(
                    index.data(ResultsModel::ChapterLabelRole).toString(),
                    Qt::ElideRight, line2.width()));

    p->restore();
}
