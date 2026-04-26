#pragma once

#include <QDateTime>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QUrl>

/**
 * @brief MediaCache – cache thumbnail (disk) và stream URL (disk + TTL).
 *
 * Thumbnail cache:
 *  - Lưu ảnh dưới dạng file PNG tại %APPDATA%/VIBLI/thumbnails/<videoId>.png
 *  - Không có TTL (ảnh thumbnail YouTube ổn định)
 *
 * Stream URL cache (persistent):
 *  - Lưu tại %APPDATA%/VIBLI/stream_cache.json
 *  - Mỗi entry: { "url": "...", "cachedAt": <unix timestamp> }
 *  - TTL mặc định: 6 giờ (stream URL YouTube expire sau ~6h)
 */
class MediaCache : public QObject {
  Q_OBJECT

public:
  static constexpr int STREAM_URL_TTL_SECONDS = 6 * 3600; // 6 giờ

  explicit MediaCache(QObject *parent = nullptr);
  ~MediaCache() override = default;

  // ── Thumbnail ─────────────────────────────────────────────────────────
  /** Trả về true nếu thumbnail đã được cache trên disk. */
  bool hasThumbnail(const QString &videoId) const;

  /** Load thumbnail từ disk. Trả về null pixmap nếu không có. */
  QPixmap loadThumbnail(const QString &videoId) const;

  /** Lưu thumbnail xuống disk. */
  void saveThumbnail(const QString &videoId, const QPixmap &pixmap);

  /** Đường dẫn file thumbnail. */
  QString thumbnailPath(const QString &videoId) const;

  // ── Stream URL ────────────────────────────────────────────────────────
  /** Trả về true nếu có stream URL hợp lệ (chưa hết TTL). */
  bool hasStreamUrl(const QString &videoId) const;

  /** Lấy stream URL từ persistent cache. Trả về QUrl() nếu không có / hết hạn.
   */
  QUrl loadStreamUrl(const QString &videoId) const;

  /** Lưu stream URL vào persistent cache. */
  void saveStreamUrl(const QString &videoId, const QUrl &url);

  /** Xóa stream URL khỏi persistent cache. */
  void invalidateStreamUrl(const QString &videoId);

  // ── Paths ─────────────────────────────────────────────────────────────
  static QString cacheDir();        // %APPDATA%/VIBLI/
  static QString thumbnailDir();    // %APPDATA%/VIBLI/thumbnails/
  static QString streamCachePath(); // %APPDATA%/VIBLI/stream_cache.json

  // ── Maintenance ───────────────────────────────────────────────────────
  /** Xóa tất cả stream URL đã hết hạn khỏi file cache. */
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
