#include "PlaylistDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

PlaylistDelegate::PlaylistDelegate(ThumbnailCache *thumbCache, QObject *parent)
    : QStyledItemDelegate(parent), m_thumbCache(thumbCache) {}

void PlaylistDelegate::ensureIcons() const {
  if (m_iconsInited)
    return;
  m_audioIcon = IconFont::icon(IconFont::MUSIC_NOTE, 20, QColor(120, 120, 120));
  m_videoIcon = IconFont::icon(IconFont::MOVIE, 20, QColor(120, 120, 120));
  m_ytPlaceholder =
      IconFont::icon(IconFont::PLAY_ARROW, 20, QColor(255, 80, 80));
  m_iconsInited = true;
}

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem & /*option*/,
                                 const QModelIndex & /*index*/) const {
  return QSize(0, ITEM_HEIGHT);
}

void PlaylistDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);

  const QRect rect = option.rect;
  const bool selected = option.state & QStyle::State_Selected;
  const bool hovered = (option.state & QStyle::State_MouseOver) && !selected;

  // ── Background ────────────────────────────────────────────────────────
  if (selected) {
    painter->fillRect(rect, QColor(29, 185, 84)); // #1db954
  } else if (hovered) {
    painter->fillRect(rect, QColor(42, 42, 42));
  }
  // Separator line
  painter->setPen(QPen(QColor(34, 34, 34), 1));
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());

  // ── Thumbnail area ────────────────────────────────────────────────────
  const QString videoId = index.data(Qt::UserRole).toString();
  const bool isYouTube = index.data(Qt::UserRole + 1).toBool();
  const bool isVideo = index.data(Qt::UserRole + 2).toBool();

  const int thumbX = rect.left() + PADDING;
  const int thumbY = rect.top() + (rect.height() - THUMB_H) / 2;
  const QRect thumbRect(thumbX, thumbY, THUMB_W, THUMB_H);

  drawThumbnail(painter, thumbRect, videoId, isYouTube, isVideo);

  // ── Text area ─────────────────────────────────────────────────────────
  const int textX = thumbX + THUMB_W + PADDING;
  const int textW = rect.right() - textX - PADDING;
  const QRect textRect(textX, rect.top(), textW, rect.height());

  const QString displayText = index.data(Qt::DisplayRole).toString();
  const QStringList lines = displayText.split('\n');
  const QString title = lines.value(0);
  const QString sub = lines.value(1);

  const QColor titleColor = selected ? Qt::white : QColor(224, 224, 224);
  const QColor subColor =
      selected ? QColor(200, 255, 200) : QColor(120, 120, 120);

  // Title — bold, 13px
  QFont titleFont = painter->font();
  titleFont.setPixelSize(13);
  titleFont.setBold(false);
  painter->setFont(titleFont);
  painter->setPen(titleColor);

  if (sub.isEmpty()) {
    // Chỉ 1 dòng — căn giữa dọc
    painter->drawText(
        textRect, Qt::AlignVCenter | Qt::AlignLeft,
        painter->fontMetrics().elidedText(title, Qt::ElideRight, textW));
  } else {
    // 2 dòng
    const int lineH = rect.height() / 2;
    const QRect titleRect(textX, rect.top() + 4, textW, lineH);
    const QRect subRect(textX, rect.top() + lineH - 2, textW, lineH);

    painter->drawText(
        titleRect, Qt::AlignBottom | Qt::AlignLeft,
        painter->fontMetrics().elidedText(title, Qt::ElideRight, textW));

    QFont subFont = titleFont;
    subFont.setPixelSize(11);
    subFont.setBold(false);
    painter->setFont(subFont);
    painter->setPen(subColor);
    painter->drawText(
        subRect, Qt::AlignTop | Qt::AlignLeft,
        painter->fontMetrics().elidedText(sub, Qt::ElideRight, textW));
  }

  painter->restore();
}

void PlaylistDelegate::drawThumbnail(QPainter *p, const QRect &rect,
                                     const QString &videoId, bool isYouTube,
                                     bool isVideo) const {
  ensureIcons();

  // Bo góc cho thumbnail
  QPainterPath clip;
  clip.addRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
  p->save();
  p->setClipPath(clip);

  if (m_thumbCache && m_thumbCache->contains(videoId)) {
    // Lấy thẳng từ ThumbnailCache — không tạo QIcon mới
    const QPixmap &px = m_thumbCache->get(videoId);
    if (!px.isNull()) {
      // Scale cover + crop center
      const QPixmap scaled =
          px.scaled(rect.size(), Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation);
      const int ox = (scaled.width() - rect.width()) / 2;
      const int oy = (scaled.height() - rect.height()) / 2;
      p->drawPixmap(rect, scaled, QRect(ox, oy, rect.width(), rect.height()));
      p->restore();
      // Border
      p->setPen(QPen(QColor(255, 255, 255, 25), 1));
      p->drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
      return;
    }
  }

  // Placeholder background
  p->fillPath(clip, QColor(32, 32, 32));
  p->restore();

  // Icon placeholder ở giữa
  const QIcon &icon =
      isYouTube ? m_ytPlaceholder : (isVideo ? m_videoIcon : m_audioIcon);
  const int iconSize = 24;
  const QRect iconRect(rect.left() + (rect.width() - iconSize) / 2,
                       rect.top() + (rect.height() - iconSize) / 2, iconSize,
                       iconSize);
  icon.paint(p, iconRect);

  // Border
  p->setPen(QPen(QColor(255, 255, 255, 20), 1));
  p->drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
}
