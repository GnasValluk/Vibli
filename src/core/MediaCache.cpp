#include "MediaCache.h"
#include "LogService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

// ── Constructor
// ───────────────────────────────────────────────────────────────

MediaCache::MediaCache(QObject *parent) : QObject(parent) {
  ensureDirs();
  loadStreamCacheFile();
  pruneExpiredStreamUrls();
}

// ── Static paths
// ──────────────────────────────────────────────────────────────

QString MediaCache::cacheDir() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         "/";
}

QString MediaCache::thumbnailDir() { return cacheDir() + "thumbnails/"; }

QString MediaCache::streamCachePath() {
  return cacheDir() + "stream_cache.json";
}

// ── Thumbnail
// ─────────────────────────────────────────────────────────────────

QString MediaCache::thumbnailPath(const QString &videoId) const {
  return thumbnailDir() + videoId + ".png";
}

bool MediaCache::hasThumbnail(const QString &videoId) const {
  return QFileInfo::exists(thumbnailPath(videoId));
}

QPixmap MediaCache::loadThumbnail(const QString &videoId) const {
  const QString path = thumbnailPath(videoId);
  if (!QFileInfo::exists(path))
    return {};
  QPixmap px;
  if (!px.load(path)) {
    VLOG_WARN("MediaCache", "Không load được thumbnail: " + path);
    return {};
  }
  return px;
}

void MediaCache::saveThumbnail(const QString &videoId, const QPixmap &pixmap) {
  if (pixmap.isNull() || videoId.isEmpty())
    return;
  ensureDirs();
  const QString path = thumbnailPath(videoId);
  if (!pixmap.save(path, "PNG")) {
    VLOG_WARN("MediaCache", "Không lưu được thumbnail: " + path);
  } else {
    VLOG_DEBUG("MediaCache", "Đã lưu thumbnail: " + videoId);
  }
}

// ── Stream URL
// ────────────────────────────────────────────────────────────────

bool MediaCache::hasStreamUrl(const QString &videoId) const {
  if (!m_streamEntries.contains(videoId))
    return false;
  const StreamEntry &entry = m_streamEntries.value(videoId);
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  return (now - entry.cachedAt) < STREAM_URL_TTL_SECONDS;
}

QUrl MediaCache::loadStreamUrl(const QString &videoId) const {
  if (!hasStreamUrl(videoId))
    return {};
  return m_streamEntries.value(videoId).url;
}

void MediaCache::saveStreamUrl(const QString &videoId, const QUrl &url) {
  if (videoId.isEmpty() || !url.isValid())
    return;
  StreamEntry entry;
  entry.url = url;
  entry.cachedAt = QDateTime::currentSecsSinceEpoch();
  m_streamEntries.insert(videoId, entry);
  saveStreamCacheFile();
  VLOG_DEBUG("MediaCache", "Đã lưu stream URL: " + videoId);
}

void MediaCache::invalidateStreamUrl(const QString &videoId) {
  if (m_streamEntries.remove(videoId) > 0) {
    saveStreamCacheFile();
    VLOG_DEBUG("MediaCache", "Đã xóa stream URL cache: " + videoId);
  }
}

void MediaCache::clearAll() {
  m_streamEntries.clear();
  saveStreamCacheFile();
  VLOG_INFO("MediaCache", "Đã xóa toàn bộ stream URL cache");
}

void MediaCache::pruneExpiredStreamUrls() {
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  int removed = 0;
  for (auto it = m_streamEntries.begin(); it != m_streamEntries.end();) {
    if ((now - it.value().cachedAt) >= STREAM_URL_TTL_SECONDS) {
      it = m_streamEntries.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  if (removed > 0) {
    saveStreamCacheFile();
    VLOG_INFO("MediaCache",
              QString("Đã xóa %1 stream URL hết hạn").arg(removed));
  }
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

void MediaCache::ensureDirs() const { QDir().mkpath(thumbnailDir()); }

void MediaCache::loadStreamCacheFile() {
  const QString path = streamCachePath();
  QFile file(path);
  if (!file.exists())
    return;
  if (!file.open(QIODevice::ReadOnly)) {
    VLOG_WARN("MediaCache", "Không mở được stream cache: " + path);
    return;
  }

  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    VLOG_WARN("MediaCache", "Stream cache JSON lỗi: " + err.errorString());
    return;
  }

  const QJsonObject root = doc.object();
  for (auto it = root.begin(); it != root.end(); ++it) {
    if (!it.value().isObject())
      continue;
    const QJsonObject obj = it.value().toObject();
    const QString urlStr = obj.value("url").toString();
    const qint64 cachedAt =
        static_cast<qint64>(obj.value("cachedAt").toDouble(0));
    if (urlStr.isEmpty() || cachedAt == 0)
      continue;
    StreamEntry entry;
    entry.url = QUrl(urlStr);
    entry.cachedAt = cachedAt;
    m_streamEntries.insert(it.key(), entry);
  }
  VLOG_INFO(
      "MediaCache",
      QString("Đã load %1 stream URL từ cache").arg(m_streamEntries.size()));
}

void MediaCache::saveStreamCacheFile() const {
  const QString path = streamCachePath();
  ensureDirs();

  QJsonObject root;
  for (auto it = m_streamEntries.begin(); it != m_streamEntries.end(); ++it) {
    QJsonObject obj;
    obj["url"] = it.value().url.toString();
    obj["cachedAt"] = static_cast<double>(it.value().cachedAt);
    root[it.key()] = obj;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    VLOG_WARN("MediaCache", "Không ghi được stream cache: " + path);
    return;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
