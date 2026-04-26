#pragma once

#include "../core/ThumbnailCache.h"
#include "IconFont.h"
#include <QStyledItemDelegate>

/**
 * @brief PlaylistDelegate – vẽ từng item trong playlist.
 *
 * Lấy thumbnail trực tiếp từ ThumbnailCache (không tạo QIcon mới),
 * vẽ 2 dòng text (title + sub-info), và icon placeholder cho local files.
 *
 * Không giữ bất kỳ QPixmap nào — zero memory overhead ngoài ThumbnailCache.
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

  // Icon tĩnh — tạo 1 lần, dùng lại mãi (không tạo mới mỗi lần paint)
  mutable QIcon m_audioIcon;
  mutable QIcon m_videoIcon;
  mutable QIcon m_ytPlaceholder;
  mutable bool m_iconsInited = false;

  void ensureIcons() const;
};
