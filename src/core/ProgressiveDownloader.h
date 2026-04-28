#pragma once

#include <QFile>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <optional>

class YtDlpService;

/**
 * @brief Download state for resume after app restart
 */
struct DownloadState {
  QString videoId;
  QString streamUrl;
  qint64 bytesDownloaded = 0;
  qint64 totalBytes = 0;
  qint64 durationMs = 0;
  qint64 timestamp = 0; // Unix timestamp

  QJsonObject toJson() const;
  static std::optional<DownloadState> fromJson(const QJsonObject &obj);
  bool isValid() const;
};

/**
 * @brief ProgressiveDownloader – downloads YouTube audio chunks to temp file
 *
 * Workflow:
 * 1. Resolve stream URL via YtDlpService
 * 2. Download audio in 256KB chunks using HTTP range requests
 * 3. Write chunks to temp file while playback continues
 * 4. Handle URL expiration by requesting fresh URL and resuming
 * 5. Persist download state for resume after app restart
 */
class ProgressiveDownloader : public QObject {
  Q_OBJECT

public:
  explicit ProgressiveDownloader(YtDlpService *ytDlpService,
                                 QObject *parent = nullptr);
  ~ProgressiveDownloader() override;

  /**
   * Start downloading audio for videoId.
   * @param videoId YouTube video ID
   * @param durationMs Track duration in milliseconds (for progress
   * calculation)
   */
  void startDownload(const QString &videoId, qint64 durationMs);

  /**
   * Cancel current download and cleanup temp file.
   */
  void cancel();

  /**
   * Get temp file path for videoId.
   */
  QString tempFilePath(const QString &videoId) const;

  /**
   * Get buffer progress: bytes downloaded / total bytes (0.0 - 1.0)
   */
  float bufferProgress() const;

  /**
   * Get buffered duration in milliseconds.
   */
  qint64 bufferedDurationMs() const;

  /**
   * Check if position is within buffered range.
   */
  bool isPositionBuffered(qint64 positionMs) const;

signals:
  /**
   * Emitted when initial buffer (30s) is ready for playback.
   * @param tempFilePath Local file path to play
   */
  void initialBufferReady(const QString &tempFilePath);

  /**
   * Emitted periodically as download progresses.
   * @param bytesDownloaded Bytes downloaded so far
   * @param totalBytes Total file size in bytes
   */
  void bufferProgressChanged(qint64 bytesDownloaded, qint64 totalBytes);

  /**
   * Emitted when entire file is downloaded.
   */
  void downloadComplete();

  /**
   * Emitted when download fails after all retries.
   * @param errorMessage Human-readable error description
   */
  void downloadFailed(const QString &errorMessage);

private slots:
  void onStreamUrlReady(const QString &videoId, const QUrl &streamUrl);
  void onStreamError(const QString &videoId, const QString &errorMessage);
  void onChunkReadyRead();
  void onChunkFinished();
  void onChunkError(QNetworkReply::NetworkError error);

private:
  static constexpr qint64 CHUNK_SIZE = 256 * 1024; // 256KB
  static constexpr qint64 INITIAL_BUFFER_SECONDS =
      45; // Increased from 30 to 45
  static constexpr int MAX_RETRIES = 3;

  void downloadNextChunk();
  void retryCurrentChunk();
  void handleUrlExpiration();
  void saveState();
  void loadState();
  void cleanup();
  qint64 calculateInitialBufferBytes() const;
  QString stateFilePath() const;
  void ensureStreamingDirectoryExists();

  YtDlpService *m_ytDlpService;
  QNetworkAccessManager *m_networkManager;
  QNetworkReply *m_currentReply = nullptr;
  QFile *m_tempFile = nullptr;

  QString m_videoId;
  QUrl m_streamUrl;
  qint64 m_durationMs = 0;
  qint64 m_totalBytes = 0;
  qint64 m_bytesDownloaded = 0;
  int m_retryCount = 0;
  bool m_initialBufferEmitted = false;
  bool m_downloadActive = false;
};
