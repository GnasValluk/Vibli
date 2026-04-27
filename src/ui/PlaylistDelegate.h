#pragma once

#include "../core/ThumbnailCache.h"
#include "IconFont.h"
#include <QStyledItemDelegate>

/**
 * @brief PlaylistDelegate – renders each item in the playlist.
 *
 * Retrieves thumbnails directly from ThumbnailCache (no new QIcon created),
 * draws 2 lines of text (title + sub-info), and an icon placeholder for local
 * files.
 *
 * Holds no QPixmap — zero memory overhead beyond ThumbnailCache.
 */
class PlaylistDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  static constexpr int ITEM_HEIGHT = 64;
  static constexpr int THUMB_W = 56;
  static constexpr int THUMB_H = 56;
  static constexpr int PADDING = 8;

  explicit PlaylistDelegate(ThumbnailCache *thumbCache,
                            QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;

private:
  void drawThumbnail(QPainter *p, const QRect &rect, const QString &videoId,
                     bool isYouTube, bool isVideo) const;

  ThumbnailCache *m_thumbCache;

  // Static icons — created once, reused forever (not recreated on each paint)
  mutable QIcon m_audioIcon;
  mutable QIcon m_videoIcon;
  mutable QIcon m_ytPlaceholder;
  mutable bool m_iconsInited = false;

  void ensureIcons() const;
};
