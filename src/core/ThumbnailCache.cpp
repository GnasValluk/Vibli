#include "ThumbnailCache.h"

ThumbnailCache::ThumbnailCache(int maxSize, QObject *parent)
    : QObject(parent), m_maxSize(qMax(1, maxSize)) {}

QPixmap ThumbnailCache::get(const QString &videoId) const {
  if (!m_map.contains(videoId))
    return {};

  // Cập nhật LRU: đưa lên đầu
  m_lruOrder.removeOne(videoId);
  m_lruOrder.prepend(videoId);

  return m_map.value(videoId);
}

void ThumbnailCache::put(const QString &videoId, const QPixmap &pixmap) {
  if (videoId.isEmpty() || pixmap.isNull())
    return;

  if (m_map.contains(videoId)) {
    // Cập nhật ảnh + đưa lên đầu LRU
    m_map.insert(videoId, pixmap);
    m_lruOrder.removeOne(videoId);
    m_lruOrder.prepend(videoId);
    return;
  }

  // Evict nếu đầy
  while (m_map.size() >= m_maxSize)
    evictLru();

  m_map.insert(videoId, pixmap);
  m_lruOrder.prepend(videoId);
}

void ThumbnailCache::remove(const QString &videoId) {
  m_map.remove(videoId);
  m_lruOrder.removeOne(videoId);
}

void ThumbnailCache::clear() {
  m_map.clear();
  m_lruOrder.clear();
}

bool ThumbnailCache::contains(const QString &videoId) const {
  return m_map.contains(videoId);
}

void ThumbnailCache::evictLru() {
  if (m_lruOrder.isEmpty())
    return;
  const QString lruId = m_lruOrder.takeLast();
  m_map.remove(lruId);
}
