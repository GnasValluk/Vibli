#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QString>

/**
 * @brief ThumbnailCache – LRU in-memory cache for YouTube thumbnails.
 *
 * Limits the number of images kept in RAM (default 30).
 * When the limit is exceeded, the least recently used (LRU) image is evicted.
 * Images on disk remain in MediaCache — they just need to be reloaded when
 * needed.
 *
 * Goal: an 82-track playlist keeps at most 30 QPixmaps in RAM (~45MB),
 * instead of 82 QPixmaps (~123MB) as before.
 */
class ThumbnailCache : public QObject {
  Q_OBJECT

public:
  static constexpr int DEFAULT_MAX_SIZE = 30;

  explicit ThumbnailCache(int maxSize = DEFAULT_MAX_SIZE,
                          QObject *parent = nullptr);
  ~ThumbnailCache() override = default;

  /** Gets a thumbnail. Returns null pixmap if not in cache. */
  QPixmap get(const QString &videoId) const;

  /** Stores a thumbnail. Evicts LRU entry if over the limit. */
  void put(const QString &videoId, const QPixmap &pixmap);

  /** Removes a single entry. */
  void remove(const QString &videoId);

  /** Clears the entire cache. */
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
