#pragma once

#include <QDateTime>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QUrl>

/**
 * @brief MediaCache – thumbnail (disk) and stream URL (disk + TTL) cache.
 *
 * Thumbnail cache:
 *  - Stores images as PNG files at %APPDATA%/VIBLI/thumbnails/<videoId>.png
 *  - No TTL (YouTube thumbnail URLs are stable)
 *
 * Stream URL cache (persistent):
 *  - Stored at %APPDATA%/VIBLI/stream_cache.json
 *  - Each entry: { "url": "...", "cachedAt": <unix timestamp> }
 *  - Default TTL: 6 hours (YouTube stream URLs expire after ~6h)
 */
class MediaCache : public QObject {
  Q_OBJECT

public:
  static constexpr int STREAM_URL_TTL_SECONDS = 6 * 3600; // 6 hours

  explicit MediaCache(QObject *parent = nullptr);
  ~MediaCache() override = default;

  // ── Thumbnail ─────────────────────────────────────────────────────────
  /** Returns true if the thumbnail is cached on disk. */
  bool hasThumbnail(const QString &videoId) const;

  /** Loads thumbnail from disk. Returns null pixmap if not found. */
  QPixmap loadThumbnail(const QString &videoId) const;

  /** Saves thumbnail to disk. */
  void saveThumbnail(const QString &videoId, const QPixmap &pixmap);

  /** Returns the thumbnail file path. */
  QString thumbnailPath(const QString &videoId) const;

  // ── Stream URL ────────────────────────────────────────────────────────
  /** Returns true if a valid stream URL exists (not expired). */
  bool hasStreamUrl(const QString &videoId) const;

  /** Gets stream URL from persistent cache. Returns QUrl() if missing or
   * expired. */
  QUrl loadStreamUrl(const QString &videoId) const;

  /** Saves stream URL to persistent cache. */
  void saveStreamUrl(const QString &videoId, const QUrl &url);

  /** Removes stream URL from persistent cache. */
  void invalidateStreamUrl(const QString &videoId);

  /** Clears all stream URL cache (in-memory + disk). */
  void clearAll();

  // ── Paths ─────────────────────────────────────────────────────────────
  static QString cacheDir();        // %APPDATA%/VIBLI/
  static QString thumbnailDir();    // %APPDATA%/VIBLI/thumbnails/
  static QString streamCachePath(); // %APPDATA%/VIBLI/stream_cache.json

  // ── Maintenance ───────────────────────────────────────────────────────
  /** Removes all expired stream URLs from the cache file. */
  void pruneExpiredStreamUrls();

private:
  void ensureDirs() const;
  void loadStreamCacheFile();
  void saveStreamCacheFile() const;

  // videoId → { url, cachedAt }
  struct StreamEntry {
    QUrl url;
    qint64 cachedAt = 0; // Unix timestamp (seconds)
  };
  QMap<QString, StreamEntry> m_streamEntries;
};
