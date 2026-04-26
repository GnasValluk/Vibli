#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <optional>

#include "MediaCache.h"
#include "PlaylistManager.h"

/**
 * @brief YtDlpService – giao tiếp với yt-dlp.exe qua QProcess.
 *
 * Streaming mode: emit trackFetched() cho từng video ngay khi parse xong,
 * không đợi hết playlist. Cho phép UI hiển thị dần dần.
 *
 * Stream URL được cache 2 lớp:
 *  - In-memory (QMap): nhanh nhất, mất khi thoát app
 *  - Persistent (MediaCache): lưu disk, TTL 6h, sống qua restart
 */
class YtDlpService : public QObject {
  Q_OBJECT

public:
  explicit YtDlpService(QObject *parent = nullptr);
  ~YtDlpService() override = default;

  /** Gắn MediaCache để dùng persistent stream URL cache. */
  void setMediaCache(MediaCache *cache);

  static bool isAvailable();

  void fetchPlaylistMetadata(const QString &playlistUrl);
  void resolveStreamUrl(const QString &videoId);
  void invalidateStreamCache(const QString &videoId);

  // Parser (public để test độc lập)
  std::optional<Track> parseVideoJson(const QJsonObject &obj) const;

signals:
  // Streaming: emit từng track ngay khi parse xong
  void trackFetched(const Track &track);
  // Khi toàn bộ playlist xong
  void playlistMetadataReady(const QList<Track> &tracks);
  // Tiến trình: (loaded, total) — total = -1 nếu chưa biết
  void fetchProgress(int loaded, int total);

  void streamUrlReady(const QString &videoId, const QUrl &streamUrl);
  // Lỗi khi fetch metadata playlist
  void metadataErrorOccurred(const QString &errorMessage);
  // Lỗi khi resolve stream URL (dùng riêng để tránh vòng lặp)
  void streamErrorOccurred(const QString &videoId, const QString &errorMessage);
  // Lỗi chung (backward compat)
  void errorOccurred(const QString &errorMessage);
  void progressUpdated(const QString &statusMessage);

private slots:
  void onMetadataReadyRead();
  void onMetadataProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessTimeout();

private:
  static QString ytDlpPath();

  QProcess *m_metadataProcess = nullptr;
  QProcess *m_streamProcess = nullptr;
  QTimer *m_streamTimer = nullptr;

  // In-memory stream cache (layer 1 – fastest)
  QMap<QString, QUrl> m_streamCache;
  // Persistent stream cache (layer 2 – survives restart)
  MediaCache *m_mediaCache = nullptr;

  QString m_pendingVideoId;

  QByteArray m_metadataBuffer; // buffer dòng chưa hoàn chỉnh
  int m_fetchedCount = 0;
};
