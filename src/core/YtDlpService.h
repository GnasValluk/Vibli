#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QUrl>
#include <optional>

#include "MediaCache.h"
#include "PlaylistManager.h"

/** Download format for media files. */
enum class DownloadFormat { Mp3, Mp4 };

/**
 * @brief Describes a download job.
 *
 * Pass to YtDlpService::downloadMedia() to start a download.
 */
struct DownloadJob {
  QString url; ///< YouTube playlist or video URL
  DownloadFormat format = DownloadFormat::Mp3;
  QString outputDir; ///< Output directory (must exist)
  QString jobId;     ///< UUID created by caller to track progress
};

/**
 * @brief YtDlpService – communicates with yt-dlp.exe via QProcess.
 *
 * Stream URL resolution:
 *  - Format priority: m4a → webm/opus → best  (avoids demux failures on
 * Windows)
 *  - Queue: multiple videoIds can be enqueued and processed sequentially
 *  - Pre-resolve: call prefetchStreamUrl() to resolve the next track in advance
 *  - Retry with format fallback when primary format fails
 *
 * Two-layer cache:
 *  - In-memory (QMap): fastest, lost on app exit
 *  - Persistent (MediaCache): saved to disk, TTL 6h, survives restarts
 *
 * Download:
 *  - downloadMedia() runs yt-dlp with --newline to parse per-line progress
 *  - Each job has a unique jobId; multiple jobs can be queued
 *  - cancelDownload() cancels the running job or removes it from the queue
 */
class YtDlpService : public QObject {
  Q_OBJECT

public:
  explicit YtDlpService(QObject *parent = nullptr);
  ~YtDlpService() override = default;

  void setMediaCache(MediaCache *cache);
  static bool isAvailable();

  void fetchPlaylistMetadata(const QString &playlistUrl);

  /**
   * Resolves the stream URL for a videoId.
   * If busy, enqueues and processes later.
   * Result is returned via streamUrlReady / streamErrorOccurred signals.
   */
  void resolveStreamUrl(const QString &videoId);

  /**
   * Pre-fetches the stream URL for the next track in the background.
   * Does not emit a signal if already cached.
   * Lower priority than resolveStreamUrl.
   */
  void prefetchStreamUrl(const QString &videoId);

  void invalidateStreamCache(const QString &videoId);

  /**
   * Starts a download job (async).
   * Progress is reported via downloadProgress / downloadFinished /
   * downloadError. If busy, the job is queued and processed sequentially.
   */
  void downloadMedia(const DownloadJob &job);

  /**
   * Cancels the running job or removes it from the queue.
   * If the job is running, the process is killed and downloadError is emitted.
   */
  void cancelDownload(const QString &jobId);

  // Parser (public for independent testing)
  std::optional<Track> parseVideoJson(const QJsonObject &obj) const;

signals:
  void trackFetched(const Track &track);
  void playlistMetadataReady(const QList<Track> &tracks);
  void fetchProgress(int loaded, int total);

  void streamUrlReady(const QString &videoId, const QUrl &streamUrl);
  void streamErrorOccurred(const QString &videoId, const QString &errorMessage);
  void errorOccurred(const QString &errorMessage);
  void progressUpdated(const QString &statusMessage);

  // ── Download signals ───────────────────────────────────────────────────
  /** Download progress: 0-100, currentFile is the name of the file being
   * downloaded. */
  void downloadProgress(const QString &jobId, int percent,
                        const QString &currentFile);
  /** Download complete; outputDir is the folder containing the downloaded file.
   */
  void downloadFinished(const QString &jobId, const QString &outputDir);
  /** Download failed (real error, not user-cancelled). */
  void downloadError(const QString &jobId, const QString &errorMessage);
  /** Download cancelled by user. */
  void downloadCancelled(const QString &jobId);

private slots:
  void onMetadataReadyRead();
  void onMetadataProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessTimeout();
  void onDownloadReadyRead();
  void onDownloadProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
  // Format selector in priority order
  // Index 0 = primary, 1 = fallback 1, 2 = fallback 2
  static const QStringList FORMAT_PRIORITY;

  static QString ytDlpPath();
  static QString ffmpegDir();
  void processNextInQueue();
  void startStreamProcess(const QString &videoId, const QString &format);
  void processNextDownload();
  void startDownloadProcess(const DownloadJob &job);

  QProcess *m_metadataProcess = nullptr;
  QProcess *m_streamProcess = nullptr;
  QTimer *m_streamTimer = nullptr;

  // In-memory stream cache
  QMap<QString, QUrl> m_streamCache;
  MediaCache *m_mediaCache = nullptr;

  // Queue: { videoId, isPrefetch }
  struct StreamRequest {
    QString videoId;
    bool isPrefetch = false;
  };
  QQueue<StreamRequest> m_streamQueue;
  bool m_streamBusy = false;

  // Retry state for the current request
  QString m_pendingVideoId;
  int m_pendingFormatIdx = 0; // index into FORMAT_PRIORITY
  bool m_pendingIsPrefetch = false;

  QByteArray m_metadataBuffer;
  int m_fetchedCount = 0;

  // ── Download state ─────────────────────────────────────────────────────
  QProcess *m_downloadProcess = nullptr;
  QQueue<DownloadJob> m_downloadQueue;
  bool m_downloadBusy = false;
  bool m_downloadCancelled = false; ///< true when job was cancelled by user
  DownloadJob m_activeDownload;     ///< Currently running job
  QString m_currentDownloadFile;    ///< Name of the file being downloaded (from
                                    ///< stdout)
};
