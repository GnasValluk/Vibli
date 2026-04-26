#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QString>

/**
 * @brief ThumbnailCache – LRU in-memory cache cho thumbnail YouTube.
 *
 * Giới hạn số lượng ảnh giữ trong RAM (mặc định 30).
 * Khi vượt giới hạn, ảnh ít dùng nhất (LRU) bị evict.
 * Ảnh trên disk vẫn còn trong MediaCache — chỉ cần load lại khi cần.
 *
 * Mục tiêu: 82 bài playlist chỉ giữ tối đa 30 QPixmap trong RAM (~45MB),
 * thay vì 82 QPixmap (~123MB) như trước.
 */
class ThumbnailCache : public QObject {
  Q_OBJECT

public:
  static constexpr int DEFAULT_MAX_SIZE = 30;

  explicit ThumbnailCache(int maxSize = DEFAULT_MAX_SIZE,
                          QObject *parent = nullptr);
  ~ThumbnailCache() override = default;

  /** Lấy thumbnail. Trả về null pixmap nếu không có trong cache. */
  QPixmap get(const QString &videoId) const;

  /** Lưu thumbnail vào cache. Evict LRU nếu vượt giới hạn. */
  void put(const QString &videoId, const QPixmap &pixmap);

  /** Xóa một entry. */
  void remove(const QString &videoId);

  /** Xóa toàn bộ cache. */
  void clear();

  bool contains(const QString &videoId) const;
  int size() const { return m_map.size(); }

private:
  void evictLru();

  int m_maxSize;
  // LRU order: front = most recently used, back = least recently used
  mutable QList<QString> m_lruOrder;
  QHash<QString, QPixmap> m_map;
};
