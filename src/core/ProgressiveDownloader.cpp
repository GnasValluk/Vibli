#include "ProgressiveDownloader.h"
#include "LogService.h"
#include "YtDlpService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

// ── DownloadState implementation ─────────────────────────────────────

QJsonObject DownloadState::toJson() const {
  QJsonObject obj;
  obj["videoId"] = videoId;
  obj["streamUrl"] = streamUrl;
  obj["bytesDownloaded"] = static_cast<double>(bytesDownloaded);
  obj["totalBytes"] = static_cast<double>(totalBytes);
  obj["durationMs"] = static_cast<double>(durationMs);
  obj["timestamp"] = static_cast<double>(timestamp);
  return obj;
}

std::optional<DownloadState> DownloadState::fromJson(const QJsonObject &obj) {
  if (!obj.contains("videoId") || !obj.contains("bytesDownloaded"))
    return std::nullopt;

  DownloadState state;
  state.videoId = obj["videoId"].toString();
  state.streamUrl = obj["streamUrl"].toString();
  state.bytesDownloaded =
      static_cast<qint64>(obj["bytesDownloaded"].toDouble());
  state.totalBytes = static_cast<qint64>(obj["totalBytes"].toDouble());
  state.durationMs = static_cast<qint64>(obj["durationMs"].toDouble());
  state.timestamp = static_cast<qint64>(obj["timestamp"].toDouble());
  return state;
}

bool DownloadState::isValid() const {
  // State is invalid if older than 2 hours
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  if (now - timestamp > 7200)
    return false;

  // State is invalid if temp file doesn't exist
  const QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  const QString tempFilePath = tempDir + "/VIBLI/streaming/" + videoId + ".m4a";
  if (!QFileInfo::exists(tempFilePath))
    return false;

  // State is invalid if file size doesn't match bytesDownloaded
  const qint64 fileSize = QFileInfo(tempFilePath).size();
  if (fileSize != bytesDownloaded)
    return false;

  return true;
}

// ── ProgressiveDownloader implementation ─────────────────────────────

ProgressiveDownloader::ProgressiveDownloader(YtDlpService *ytDlpService,
                                             QObject *parent)
    : QObject(parent), m_ytDlpService(ytDlpService),
      m_networkManager(new QNetworkAccessManager(this)) {
  ensureStreamingDirectoryExists();

  // Connect to YtDlpService signals
  connect(m_ytDlpService, &YtDlpService::streamUrlReady, this,
          &ProgressiveDownloader::onStreamUrlReady);
  connect(m_ytDlpService, &YtDlpService::streamErrorOccurred, this,
          &ProgressiveDownloader::onStreamError);
}

ProgressiveDownloader::~ProgressiveDownloader() { cleanup(); }

void ProgressiveDownloader::startDownload(const QString &videoId,
                                          qint64 durationMs) {
  // Cancel any existing download
  cancel();

  m_videoId = videoId;
  m_durationMs = durationMs;
  m_bytesDownloaded = 0;
  m_totalBytes = 0;
  m_retryCount = 0;
  m_initialBufferEmitted = false;
  m_downloadActive = true;

  VLOG_INFO("ProgressiveDownloader", "Starting download for: " + videoId);

  // Try to load existing state for resume
  loadState();

  // Request stream URL from YtDlpService
  m_ytDlpService->resolveStreamUrl(videoId);
}

void ProgressiveDownloader::cancel() {
  m_downloadActive = false;

  // Abort current network request
  if (m_currentReply) {
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }

  // Close and delete temp file
  if (m_tempFile) {
    m_tempFile->close();
    m_tempFile->deleteLater();
    m_tempFile = nullptr;
  }

  // Delete temp file and state file
  if (!m_videoId.isEmpty()) {
    QFile::remove(tempFilePath(m_videoId));
    QFile::remove(stateFilePath());
  }

  // Reset state
  m_videoId.clear();
  m_streamUrl.clear();
  m_durationMs = 0;
  m_totalBytes = 0;
  m_bytesDownloaded = 0;
  m_retryCount = 0;
  m_initialBufferEmitted = false;
}

QString ProgressiveDownloader::tempFilePath(const QString &videoId) const {
  const QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  return tempDir + "/VIBLI/streaming/" + videoId + ".m4a";
}

float ProgressiveDownloader::bufferProgress() const {
  if (m_totalBytes == 0)
    return 0.0f;
  return static_cast<float>(m_bytesDownloaded) / m_totalBytes;
}

qint64 ProgressiveDownloader::bufferedDurationMs() const {
  if (m_totalBytes == 0)
    return 0;
  return (m_bytesDownloaded * m_durationMs) / m_totalBytes;
}

bool ProgressiveDownloader::isPositionBuffered(qint64 positionMs) const {
  return positionMs <= bufferedDurationMs();
}

void ProgressiveDownloader::onStreamUrlReady(const QString &videoId,
                                             const QUrl &streamUrl) {
  if (videoId != m_videoId || !m_downloadActive)
    return; // Not for current track

  m_streamUrl = streamUrl;
  m_retryCount = 0; // Reset retry counter with fresh URL

  VLOG_INFO("ProgressiveDownloader", "Stream URL ready for: " + videoId);

  // Start downloading chunks
  downloadNextChunk();
}

void ProgressiveDownloader::onStreamError(const QString &videoId,
                                          const QString &errorMessage) {
  if (videoId != m_videoId)
    return;

  VLOG_ERROR("ProgressiveDownloader",
             "Stream error for " + videoId + ": " + errorMessage);
  emit downloadFailed("Failed to resolve stream URL: " + errorMessage);
  cleanup();
}

void ProgressiveDownloader::onChunkReadyRead() {
  if (!m_currentReply || !m_tempFile)
    return;

  // Read available data from network reply
  const QByteArray data = m_currentReply->readAll();
  if (data.isEmpty())
    return;

  // Write to temp file
  const qint64 bytesWritten = m_tempFile->write(data);
  if (bytesWritten < 0) {
    VLOG_ERROR("ProgressiveDownloader",
               "Failed to write to temp file: " + m_tempFile->errorString());
    emit downloadFailed("Disk write error");
    cleanup();
    return;
  }

  m_tempFile->flush();

  // Update progress
  m_bytesDownloaded += bytesWritten;

  // Emit progress signal
  if (m_totalBytes > 0) {
    emit bufferProgressChanged(m_bytesDownloaded, m_totalBytes);
  }
}

void ProgressiveDownloader::onChunkFinished() {
  if (!m_currentReply)
    return;

  // Get total file size from Content-Range header (first chunk only)
  if (m_totalBytes == 0) {
    const QByteArray contentRange = m_currentReply->rawHeader("Content-Range");
    if (!contentRange.isEmpty()) {
      // Format: "bytes 0-262143/3145728"
      const QList<QByteArray> parts = contentRange.split('/');
      if (parts.size() == 2) {
        m_totalBytes = parts[1].toLongLong();
        VLOG_INFO("ProgressiveDownloader",
                  QString("Total file size: %1 bytes").arg(m_totalBytes));
      }
    }

    // Fallback: estimate from Content-Length if Content-Range not available
    if (m_totalBytes == 0) {
      const QByteArray contentLength =
          m_currentReply->rawHeader("Content-Length");
      if (!contentLength.isEmpty()) {
        m_totalBytes = contentLength.toLongLong();
        VLOG_INFO("ProgressiveDownloader",
                  QString("Total file size (from Content-Length): %1 bytes")
                      .arg(m_totalBytes));
      }
    }

    // Last resort: estimate based on typical bitrate (128kbps for audio)
    if (m_totalBytes == 0 && m_durationMs > 0) {
      // Assume 128kbps = 16KB/s
      m_totalBytes = (m_durationMs / 1000) * 16 * 1024;
      VLOG_WARN("ProgressiveDownloader",
                QString("Estimated file size: %1 bytes (based on duration)")
                    .arg(m_totalBytes));
    }
  }

  // Check for initial buffer ready
  if (!m_initialBufferEmitted && m_totalBytes > 0) {
    // CRITICAL: QMediaPlayer cannot play files that are still being written!
    // We MUST wait for 100% download completion before starting playback.
    // Otherwise QMediaPlayer will hit EOF and trigger EndOfMedia prematurely.
    if (m_bytesDownloaded >= m_totalBytes) {
      m_initialBufferEmitted = true;
      VLOG_INFO("ProgressiveDownloader",
                QString("Download complete, ready to play: %1").arg(m_videoId));
      emit initialBufferReady(tempFilePath(m_videoId));
    }
  }

  // Cleanup current reply
  m_currentReply->deleteLater();
  m_currentReply = nullptr;

  // Continue downloading if not complete
  if (m_bytesDownloaded < m_totalBytes) {
    saveState(); // Save progress
    downloadNextChunk();
  } else {
    // Download complete
    VLOG_INFO("ProgressiveDownloader", "Download complete for: " + m_videoId);
    emit downloadComplete();
    if (m_tempFile) {
      m_tempFile->close();
    }
  }
}

void ProgressiveDownloader::onChunkError(QNetworkReply::NetworkError error) {
  if (!m_currentReply)
    return;

  const QString errorString = m_currentReply->errorString();
  const int httpStatus =
      m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
          .toInt();

  VLOG_WARN("ProgressiveDownloader",
            QString("Chunk download error: %1 (HTTP %2)")
                .arg(errorString)
                .arg(httpStatus));

  // Check for URL expiration (403, 404, 410)
  if (httpStatus == 403 || httpStatus == 404 || httpStatus == 410) {
    handleUrlExpiration();
    return;
  }

  // Retry with exponential backoff
  if (m_retryCount < MAX_RETRIES) {
    m_retryCount++;
    const int delayMs = qPow(2, m_retryCount - 1) * 1000; // 1s, 2s, 4s
    VLOG_WARN("ProgressiveDownloader", QString("Retry %1/%2 in %3ms")
                                           .arg(m_retryCount)
                                           .arg(MAX_RETRIES)
                                           .arg(delayMs));

    // Cleanup current reply
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    // Schedule retry
    QTimer::singleShot(delayMs, this,
                       &ProgressiveDownloader::retryCurrentChunk);
  } else {
    // Max retries exceeded
    VLOG_ERROR("ProgressiveDownloader", "Max retries exceeded, giving up");
    emit downloadFailed("Network error after 3 retries");
    cleanup();
  }
}

void ProgressiveDownloader::downloadNextChunk() {
  if (!m_downloadActive || m_streamUrl.isEmpty())
    return;

  // Check if download is complete
  if (m_totalBytes > 0 && m_bytesDownloaded >= m_totalBytes) {
    VLOG_INFO("ProgressiveDownloader", "Download complete for: " + m_videoId);
    emit downloadComplete();
    cleanup();
    return;
  }

  // Open temp file for writing (append mode)
  if (!m_tempFile) {
    m_tempFile = new QFile(tempFilePath(m_videoId), this);
    if (!m_tempFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
      VLOG_ERROR("ProgressiveDownloader",
                 "Failed to open temp file: " + m_tempFile->errorString());
      emit downloadFailed("Disk write error");
      cleanup();
      return;
    }
  }

  // Calculate chunk range
  const qint64 chunkEnd = m_bytesDownloaded + CHUNK_SIZE - 1;
  const QString rangeHeader =
      QString("bytes=%1-%2").arg(m_bytesDownloaded).arg(chunkEnd);

  // Create network request with Range header
  QNetworkRequest request(m_streamUrl);
  request.setRawHeader("Range", rangeHeader.toUtf8());

  // Send request
  m_currentReply = m_networkManager->get(request);
  connect(m_currentReply, &QNetworkReply::readyRead, this,
          &ProgressiveDownloader::onChunkReadyRead);
  connect(m_currentReply, &QNetworkReply::finished, this,
          &ProgressiveDownloader::onChunkFinished);
  connect(m_currentReply, &QNetworkReply::errorOccurred, this,
          &ProgressiveDownloader::onChunkError);

  VLOG_DEBUG(
      "ProgressiveDownloader",
      QString("Downloading chunk: %1-%2").arg(m_bytesDownloaded).arg(chunkEnd));
}

void ProgressiveDownloader::retryCurrentChunk() {
  VLOG_INFO("ProgressiveDownloader", "Retrying chunk download");
  downloadNextChunk();
}

void ProgressiveDownloader::handleUrlExpiration() {
  VLOG_WARN("ProgressiveDownloader",
            "Stream URL expired, requesting fresh URL");

  // Save current progress
  saveState();

  // Cleanup current reply
  if (m_currentReply) {
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }

  // Invalidate cache and request fresh URL
  m_ytDlpService->invalidateStreamCache(m_videoId);
  m_ytDlpService->resolveStreamUrl(m_videoId);

  // onStreamUrlReady will be called with fresh URL and resume download
}

void ProgressiveDownloader::saveState() {
  if (m_videoId.isEmpty())
    return;

  DownloadState state;
  state.videoId = m_videoId;
  state.streamUrl = m_streamUrl.toString();
  state.bytesDownloaded = m_bytesDownloaded;
  state.totalBytes = m_totalBytes;
  state.durationMs = m_durationMs;
  state.timestamp = QDateTime::currentSecsSinceEpoch();

  QFile file(stateFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    VLOG_WARN("ProgressiveDownloader",
              "Failed to save state file: " + file.errorString());
    return;
  }

  file.write(QJsonDocument(state.toJson()).toJson(QJsonDocument::Compact));
  file.close();
}

void ProgressiveDownloader::loadState() {
  const QString stateFile = stateFilePath();
  if (!QFileInfo::exists(stateFile))
    return;

  QFile file(stateFile);
  if (!file.open(QIODevice::ReadOnly)) {
    VLOG_WARN("ProgressiveDownloader",
              "Failed to load state file: " + file.errorString());
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isObject())
    return;

  const auto state = DownloadState::fromJson(doc.object());
  if (!state.has_value() || !state->isValid()) {
    VLOG_WARN("ProgressiveDownloader", "Invalid state file, starting fresh");
    QFile::remove(stateFile);
    return;
  }

  // Resume from saved state
  m_streamUrl = QUrl(state->streamUrl);
  m_bytesDownloaded = state->bytesDownloaded;
  m_totalBytes = state->totalBytes;
  m_durationMs = state->durationMs;

  VLOG_INFO("ProgressiveDownloader",
            QString("Resuming download from %1 bytes").arg(m_bytesDownloaded));
}

void ProgressiveDownloader::cleanup() { cancel(); }

qint64 ProgressiveDownloader::calculateInitialBufferBytes() const {
  if (m_durationMs == 0 || m_totalBytes == 0)
    return 0;

  // Calculate bytes needed for INITIAL_BUFFER_SECONDS
  // Use double to avoid integer division precision loss
  const double bytesPerSecond =
      static_cast<double>(m_totalBytes) / (m_durationMs / 1000.0);
  const qint64 initialBytes =
      static_cast<qint64>(bytesPerSecond * INITIAL_BUFFER_SECONDS);

  VLOG_DEBUG("ProgressiveDownloader",
             QString("Initial buffer: %1 bytes (%2s of %3s total)")
                 .arg(initialBytes)
                 .arg(INITIAL_BUFFER_SECONDS)
                 .arg(m_durationMs / 1000));

  return initialBytes;
}

QString ProgressiveDownloader::stateFilePath() const {
  const QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  return tempDir + "/VIBLI/streaming/" + m_videoId + ".state.json";
}

void ProgressiveDownloader::ensureStreamingDirectoryExists() {
  const QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  const QString streamingDir = tempDir + "/VIBLI/streaming/";
  QDir().mkpath(streamingDir);
}
