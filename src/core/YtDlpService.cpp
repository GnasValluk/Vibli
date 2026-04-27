#include "YtDlpService.h"
#include "LogService.h"
#include "MediaCache.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>

static constexpr int STREAM_TIMEOUT_MS =
    35000; // 35s — enough for slow networks

// Format priority: m4a is best on Windows/FFmpeg, webm as fallback, best as
// last resort Uses yt-dlp selector:
// https://github.com/yt-dlp/yt-dlp#format-selection
const QStringList YtDlpService::FORMAT_PRIORITY = {
    "bestaudio[ext=m4a]/bestaudio[acodec=mp4a]/bestaudio[acodec=aac]",
    "bestaudio[ext=webm]/bestaudio[acodec=opus]", "bestaudio/best"};

YtDlpService::YtDlpService(QObject *parent) : QObject(parent) {
  m_metadataProcess = new QProcess(this);
  connect(m_metadataProcess, &QProcess::readyReadStandardOutput, this,
          &YtDlpService::onMetadataReadyRead);
  connect(m_metadataProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &YtDlpService::onMetadataProcessFinished);

  m_streamProcess = new QProcess(this);
  connect(m_streamProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &YtDlpService::onStreamProcessFinished);

  m_streamTimer = new QTimer(this);
  m_streamTimer->setSingleShot(true);
  m_streamTimer->setInterval(STREAM_TIMEOUT_MS);
  connect(m_streamTimer, &QTimer::timeout, this,
          &YtDlpService::onStreamProcessTimeout);

  m_downloadProcess = new QProcess(this);
  connect(m_downloadProcess, &QProcess::readyReadStandardOutput, this,
          &YtDlpService::onDownloadReadyRead);
  connect(m_downloadProcess, &QProcess::readyReadStandardError, this,
          &YtDlpService::onDownloadReadyRead);
  connect(m_downloadProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &YtDlpService::onDownloadProcessFinished);
}

QString YtDlpService::ytDlpPath() {
  return QCoreApplication::applicationDirPath() + "/yt_dlp/yt-dlp.exe";
}

QString YtDlpService::ffmpegDir() {
  return QCoreApplication::applicationDirPath() + "/yt_dlp";
}

bool YtDlpService::isAvailable() { return QFileInfo::exists(ytDlpPath()); }

void YtDlpService::setMediaCache(MediaCache *cache) { m_mediaCache = cache; }

// ── Public API
// ────────────────────────────────────────────────────────────────

void YtDlpService::fetchPlaylistMetadata(const QString &playlistUrl) {
  if (!isAvailable()) {
    VLOG_ERROR("YtDlpService", "yt-dlp.exe not found at: " + ytDlpPath());
    emit errorOccurred(
        "yt-dlp.exe not found. Please check the yt_dlp/ directory.");
    return;
  }
  if (m_metadataProcess->state() != QProcess::NotRunning) {
    m_metadataProcess->kill();
    m_metadataProcess->waitForFinished(1000);
  }
  m_metadataBuffer.clear();
  m_fetchedCount = 0;
  VLOG_INFO("YtDlpService", "Starting playlist fetch: " + playlistUrl);
  emit progressUpdated("Loading playlist...");
  emit fetchProgress(0, -1);

  QStringList args;
  args << "--dump-json"
       << "--no-warnings"
       << "--ffmpeg-location"
       << (QCoreApplication::applicationDirPath() + "/yt_dlp") << playlistUrl;
  m_metadataProcess->start(ytDlpPath(), args);
}

void YtDlpService::resolveStreamUrl(const QString &videoId) {
  if (videoId.isEmpty())
    return;

  // Cache hit: return immediately
  if (m_streamCache.contains(videoId)) {
    VLOG_DEBUG("YtDlpService", "Memory cache hit: " + videoId);
    emit streamUrlReady(videoId, m_streamCache.value(videoId));
    return;
  }
  if (m_mediaCache && m_mediaCache->hasStreamUrl(videoId)) {
    const QUrl url = m_mediaCache->loadStreamUrl(videoId);
    if (url.isValid()) {
      VLOG_DEBUG("YtDlpService", "Disk cache hit: " + videoId);
      m_streamCache.insert(videoId, url);
      emit streamUrlReady(videoId, url);
      return;
    }
  }

  // Already processing this videoId → skip
  if (m_streamBusy && m_pendingVideoId == videoId && !m_pendingIsPrefetch)
    return;

  // Push to front of queue (high priority — user is waiting)
  // Remove existing entry if already queued
  for (int i = 0; i < m_streamQueue.size(); ++i) {
    if (m_streamQueue[i].videoId == videoId) {
      m_streamQueue.removeAt(i);
      break;
    }
  }
  m_streamQueue.prepend({videoId, false});
  processNextInQueue();
}

void YtDlpService::prefetchStreamUrl(const QString &videoId) {
  if (videoId.isEmpty())
    return;

  // Already cached → no need to prefetch
  if (m_streamCache.contains(videoId))
    return;
  if (m_mediaCache && m_mediaCache->hasStreamUrl(videoId))
    return;

  // Already in queue → don't add again
  for (const auto &req : m_streamQueue) {
    if (req.videoId == videoId)
      return;
  }
  if (m_streamBusy && m_pendingVideoId == videoId)
    return;

  // Thêm vào cuối queue (ưu tiên thấp)
  m_streamQueue.enqueue({videoId, true});
  processNextInQueue();
}

void YtDlpService::invalidateStreamCache(const QString &videoId) {
  m_streamCache.remove(videoId);
  if (m_mediaCache)
    m_mediaCache->invalidateStreamUrl(videoId);
}

// ── Download API
// ──────────────────────────────────────────────────────────────

void YtDlpService::downloadMedia(const DownloadJob &job) {
  if (job.url.isEmpty() || job.jobId.isEmpty() || job.outputDir.isEmpty()) {
    VLOG_ERROR("YtDlpService", "downloadMedia: job is missing required fields");
    emit downloadError(job.jobId, "Invalid job (url/jobId/outputDir is empty)");
    return;
  }
  if (!isAvailable()) {
    emit downloadError(job.jobId, "yt-dlp.exe not found");
    return;
  }
  m_downloadQueue.enqueue(job);
  processNextDownload();
}

void YtDlpService::cancelDownload(const QString &jobId) {
  // Remove from queue if not yet running
  for (int i = 0; i < m_downloadQueue.size(); ++i) {
    if (m_downloadQueue[i].jobId == jobId) {
      m_downloadQueue.removeAt(i);
      emit downloadCancelled(jobId);
      return;
    }
  }
  // Kill if currently running — set flag before killing
  if (m_downloadBusy && m_activeDownload.jobId == jobId) {
    m_downloadCancelled = true;
    if (m_downloadProcess->state() != QProcess::NotRunning)
      m_downloadProcess->kill();
    // onDownloadProcessFinished will emit downloadCancelled
  }
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

void YtDlpService::processNextInQueue() {
  if (m_streamBusy || m_streamQueue.isEmpty())
    return;
  if (!isAvailable()) {
    // Drain queue with error
    while (!m_streamQueue.isEmpty()) {
      const auto req = m_streamQueue.dequeue();
      if (!req.isPrefetch)
        emit streamErrorOccurred(req.videoId, "yt-dlp.exe not found");
    }
    return;
  }

  const StreamRequest req = m_streamQueue.dequeue();
  m_pendingVideoId = req.videoId;
  m_pendingFormatIdx = 0; // start with highest priority format
  m_pendingIsPrefetch = req.isPrefetch;
  m_streamBusy = true;

  startStreamProcess(m_pendingVideoId, FORMAT_PRIORITY[m_pendingFormatIdx]);
}

void YtDlpService::startStreamProcess(const QString &videoId,
                                      const QString &format) {
  if (m_streamProcess->state() != QProcess::NotRunning) {
    m_streamProcess->kill();
    m_streamProcess->waitForFinished(1000);
  }

  VLOG_INFO("YtDlpService",
            QString("Resolving stream [%1] format=%2").arg(videoId, format));

  const QString videoUrl = "https://www.youtube.com/watch?v=" + videoId;
  QStringList args;
  args << "-f" << format << "--no-warnings"
       << "-g" << videoUrl;

  m_streamTimer->start();
  m_streamProcess->start(ytDlpPath(), args);
}

void YtDlpService::processNextDownload() {
  if (m_downloadBusy || m_downloadQueue.isEmpty())
    return;
  m_activeDownload = m_downloadQueue.dequeue();
  m_downloadBusy = true;
  m_currentDownloadFile.clear();
  startDownloadProcess(m_activeDownload);
}

void YtDlpService::startDownloadProcess(const DownloadJob &job) {
  if (m_downloadProcess->state() != QProcess::NotRunning) {
    m_downloadProcess->kill();
    m_downloadProcess->waitForFinished(1000);
  }

  VLOG_INFO("YtDlpService",
            QString("Download [%1] format=%2 → %3")
                .arg(job.jobId,
                     job.format == DownloadFormat::Mp3 ? "mp3" : "mp4",
                     job.outputDir));

  QStringList args;
  args << "--ffmpeg-location" << ffmpegDir() << "--no-warnings"
       << "--newline"         // per-line progress
       << "--no-part"         // no .part files
       << "--embed-thumbnail" // embed thumbnail into file
       << "--embed-metadata"  // embed title, artist, date...
       << "--add-metadata"    // add extra metadata
       << "-o" << (job.outputDir + "/%(title)s.%(ext)s");

  if (job.format == DownloadFormat::Mp3) {
    args << "-x"
         << "--audio-format" << "mp3"
         << "--audio-quality" << "0"         // VBR best quality
         << "--convert-thumbnails" << "jpg"; // thumbnail must be jpg for ID3
  } else {
    // MP4: video + audio, merge into mp4
    args << "-f"
         << "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best"
         << "--merge-output-format" << "mp4";
  }

  args << job.url;

  m_downloadProcess->setWorkingDirectory(job.outputDir);
  m_downloadProcess->start(ytDlpPath(), args);
}

// ── Private slots
// ─────────────────────────────────────────────────────────────

void YtDlpService::onMetadataReadyRead() {
  m_metadataBuffer += m_metadataProcess->readAllStandardOutput();

  int newlinePos;
  while ((newlinePos = m_metadataBuffer.indexOf('\n')) != -1) {
    const QByteArray line = m_metadataBuffer.left(newlinePos).trimmed();
    m_metadataBuffer.remove(0, newlinePos + 1);
    if (line.isEmpty())
      continue;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
      continue;

    auto track = parseVideoJson(doc.object());
    if (!track.has_value())
      continue;

    m_fetchedCount++;
    emit trackFetched(track.value());
    emit fetchProgress(m_fetchedCount, -1);
  }
}

void YtDlpService::onMetadataProcessFinished(int exitCode,
                                             QProcess::ExitStatus /*status*/) {
  if (!m_metadataBuffer.trimmed().isEmpty()) {
    QJsonParseError err;
    const QJsonDocument doc =
        QJsonDocument::fromJson(m_metadataBuffer.trimmed(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      auto track = parseVideoJson(doc.object());
      if (track.has_value()) {
        m_fetchedCount++;
        emit trackFetched(track.value());
      }
    }
    m_metadataBuffer.clear();
  }

  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_metadataProcess->readAllStandardError());
    if (m_fetchedCount > 0) {
      emit fetchProgress(m_fetchedCount, m_fetchedCount);
      emit playlistMetadataReady({});
    } else {
      emit errorOccurred("yt-dlp error (exit " + QString::number(exitCode) +
                         "): " + errOutput);
    }
    return;
  }

  if (m_fetchedCount == 0) {
    emit errorOccurred("Playlist has no videos or is not accessible.");
    return;
  }

  emit fetchProgress(m_fetchedCount, m_fetchedCount);
  VLOG_INFO("YtDlpService",
            QString("Playlist fetch complete: %1 tracks").arg(m_fetchedCount));
  emit playlistMetadataReady({});
}

void YtDlpService::onStreamProcessFinished(int exitCode,
                                           QProcess::ExitStatus /*status*/) {
  m_streamTimer->stop();

  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_streamProcess->readAllStandardError()).trimmed();
    VLOG_WARN("YtDlpService", QString("Stream error [%1] format_idx=%2: %3")
                                  .arg(m_pendingVideoId)
                                  .arg(m_pendingFormatIdx)
                                  .arg(errOutput));

    // Try next format if available
    const int nextIdx = m_pendingFormatIdx + 1;
    if (nextIdx < FORMAT_PRIORITY.size()) {
      m_pendingFormatIdx = nextIdx;
      VLOG_INFO("YtDlpService",
                QString("Trying format fallback [%1]: %2")
                    .arg(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]));
      startStreamProcess(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]);
      return; // don't release m_streamBusy — still processing
    }

    // All formats exhausted → report error
    VLOG_ERROR("YtDlpService",
               "All format fallbacks exhausted for: " + m_pendingVideoId);
    m_streamBusy = false;
    if (!m_pendingIsPrefetch)
      emit streamErrorOccurred(m_pendingVideoId,
                               "Failed to get stream URL after " +
                                   QString::number(FORMAT_PRIORITY.size()) +
                                   " formats: " + errOutput);
    processNextInQueue();
    return;
  }

  // Read all URLs from output (yt-dlp may return multiple lines for DASH)
  const QString rawOutput =
      QString::fromUtf8(m_streamProcess->readAllStandardOutput()).trimmed();

  // Take the first line as the audio URL (when video+audio, line 2 is video)
  const QString urlStr = rawOutput.split('\n').first().trimmed();

  if (urlStr.isEmpty()) {
    VLOG_ERROR("YtDlpService", "Empty stream URL for: " + m_pendingVideoId);
    m_streamBusy = false;
    if (!m_pendingIsPrefetch)
      emit streamErrorOccurred(m_pendingVideoId, "yt-dlp returned empty URL");
    processNextInQueue();
    return;
  }

  const QUrl streamUrl(urlStr);
  m_streamCache.insert(m_pendingVideoId, streamUrl);
  if (m_mediaCache)
    m_mediaCache->saveStreamUrl(m_pendingVideoId, streamUrl);

  VLOG_INFO("YtDlpService", QString("Stream URL OK [%1] format_idx=%2")
                                .arg(m_pendingVideoId)
                                .arg(m_pendingFormatIdx));

  const QString resolvedId = m_pendingVideoId;
  m_streamBusy = false;

  // Always emit so the coordinator knows the URL is ready
  emit streamUrlReady(resolvedId, streamUrl);

  processNextInQueue();
}

void YtDlpService::onStreamProcessTimeout() {
  if (m_streamProcess->state() != QProcess::NotRunning)
    m_streamProcess->kill();

  VLOG_WARN("YtDlpService", QString("Timeout [%1] format_idx=%2")
                                .arg(m_pendingVideoId)
                                .arg(m_pendingFormatIdx));

  // Timeout also tries the next format
  const int nextIdx = m_pendingFormatIdx + 1;
  if (nextIdx < FORMAT_PRIORITY.size()) {
    m_pendingFormatIdx = nextIdx;
    VLOG_INFO("YtDlpService",
              QString("Timeout → trying format fallback [%1]: %2")
                  .arg(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]));
    startStreamProcess(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]);
    return;
  }

  m_streamBusy = false;
  if (!m_pendingIsPrefetch)
    emit streamErrorOccurred(m_pendingVideoId,
                             "Timeout while getting stream URL (all formats)");
  processNextInQueue();
}

// ── Download slots
// ────────────────────────────────────────────────────────────

void YtDlpService::onDownloadReadyRead() {
  // Read both stdout and stderr
  const QString out =
      QString::fromUtf8(m_downloadProcess->readAllStandardOutput());
  const QString err =
      QString::fromUtf8(m_downloadProcess->readAllStandardError());
  const QString combined = out + err;

  // [download]  45.3% of   3.45MiB at    1.23MiB/s ETA 00:02
  static const QRegularExpression reProgress(
      R"(\[download\]\s+([\d.]+)%\s+of\s+[\d.]+\S+\s+at\s+([\d.]+\S+)\s+ETA\s+(\S+))",
      QRegularExpression::CaseInsensitiveOption);
  // Fallback: chỉ có percent
  static const QRegularExpression rePercent(
      R"(\[download\]\s+([\d.]+)%)", QRegularExpression::CaseInsensitiveOption);
  // Tên file destination
  static const QRegularExpression reFile(
      R"(\[(?:download|ExtractAudio|ffmpeg|EmbedThumbnail|Metadata)\] Destination:\s*(.+))",
      QRegularExpression::CaseInsensitiveOption);
  // Giai đoạn xử lý hậu kỳ
  static const QRegularExpression rePhase(
      R"(\[(ExtractAudio|ffmpeg|EmbedThumbnail|Metadata|MoveFiles)\])",
      QRegularExpression::CaseInsensitiveOption);

  for (const QString &line : combined.split('\n')) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
      continue;

    // Cập nhật tên file
    QRegularExpressionMatch mFile = reFile.match(trimmed);
    if (mFile.hasMatch()) {
      m_currentDownloadFile = QFileInfo(mFile.captured(1).trimmed()).fileName();
      VLOG_DEBUG("YtDlpService", "Download file: " + m_currentDownloadFile);
    }

    // Detect giai đoạn hậu kỳ → emit status text riêng
    QRegularExpressionMatch mPhase = rePhase.match(trimmed);
    if (mPhase.hasMatch()) {
      const QString phase = mPhase.captured(1).toLower();
      QString statusText;
      if (phase == "extractaudio")
        statusText = "Converting to MP3...";
      else if (phase == "ffmpeg")
        statusText = "Processing with ffmpeg...";
      else if (phase == "embedthumbnail")
        statusText = "Embedding thumbnail...";
      else if (phase == "metadata")
        statusText = "Writing metadata...";
      else if (phase == "movefiles")
        statusText = "Moving files...";

      if (!statusText.isEmpty()) {
        // Use percent = -1 as signal for "post-processing, no percentage"
        const QString payload =
            m_currentDownloadFile + "\x1F\x1F\x1F" + statusText;
        emit downloadProgress(m_activeDownload.jobId, -1, payload);
        continue;
      }
    }

    // Parse progress đầy đủ (percent + speed + ETA)
    QRegularExpressionMatch mFull = reProgress.match(trimmed);
    if (mFull.hasMatch()) {
      const int percent =
          qBound(0, static_cast<int>(mFull.captured(1).toDouble()), 100);
      const QString speed = mFull.captured(2);
      const QString eta = mFull.captured(3);
      const QString payload =
          m_currentDownloadFile + "\x1F" + speed + "\x1F" + eta + "\x1F";
      emit downloadProgress(m_activeDownload.jobId, percent, payload);
      continue;
    }

    // Fallback: chỉ có percent
    QRegularExpressionMatch mPct = rePercent.match(trimmed);
    if (mPct.hasMatch()) {
      const int percent =
          qBound(0, static_cast<int>(mPct.captured(1).toDouble()), 100);
      const QString payload = m_currentDownloadFile + "\x1F\x1F\x1F";
      emit downloadProgress(m_activeDownload.jobId, percent, payload);
    }
  }
}

void YtDlpService::onDownloadProcessFinished(int exitCode,
                                             QProcess::ExitStatus /*status*/) {
  const QString jobId = m_activeDownload.jobId;
  const QString outDir = m_activeDownload.outputDir;
  const bool wasCancelled = m_downloadCancelled;
  m_downloadBusy = false;
  m_downloadCancelled = false;

  if (wasCancelled) {
    VLOG_INFO("YtDlpService", "Download cancelled by user [" + jobId + "]");
    emit downloadCancelled(jobId);
    processNextDownload();
    return;
  }

  if (exitCode != 0) {
    const QString errOut =
        QString::fromUtf8(m_downloadProcess->readAllStandardError()).trimmed();
    VLOG_ERROR("YtDlpService", QString("Download error [%1] exit=%2: %3")
                                   .arg(jobId)
                                   .arg(exitCode)
                                   .arg(errOut));
    emit downloadError(jobId, "yt-dlp error (exit " +
                                  QString::number(exitCode) + "): " + errOut);
  } else {
    VLOG_INFO("YtDlpService", "Download complete [" + jobId + "]");
    emit downloadProgress(jobId, 100, m_currentDownloadFile);
    emit downloadFinished(jobId, outDir);
  }

  processNextDownload();
}

// ── Parser
// ────────────────────────────────────────────────────────────────────

std::optional<Track>
YtDlpService::parseVideoJson(const QJsonObject &obj) const {
  const QString id = obj.value("id").toString();
  const QString title = obj.value("title").toString();
  if (id.isEmpty() || title.isEmpty()) {
    qWarning("YtDlpService: skipping video with missing id or title");
    return std::nullopt;
  }

  Track t;
  t.isYouTube = true;
  t.videoId = id;
  t.title = title;
  t.webpageUrl = obj.value("webpage_url").toString();
  if (t.webpageUrl.isEmpty())
    t.webpageUrl = "https://www.youtube.com/watch?v=" + id;

  t.uploader = obj.value("uploader").toString();
  if (t.uploader.isEmpty())
    t.uploader = obj.value("channel").toString();
  t.artist = t.uploader;

  const QJsonValue durVal = obj.value("duration");
  t.durationMs =
      durVal.isDouble() ? static_cast<qint64>(durVal.toDouble() * 1000.0) : 0;

  t.viewCount = static_cast<qint64>(obj.value("view_count").toDouble(0));
  t.likeCount = static_cast<qint64>(obj.value("like_count").toDouble(0));
  t.uploadDate = obj.value("upload_date").toString();
  t.description = obj.value("description").toString().left(200);

  const QJsonArray thumbs = obj.value("thumbnails").toArray();
  if (!thumbs.isEmpty()) {
    QString bestUrl;
    int bestWidth = 0;
    for (const QJsonValue &tv : thumbs) {
      const QJsonObject to = tv.toObject();
      const QString tUrl = to.value("url").toString();
      if (tUrl.isEmpty())
        continue;
      const int w = to.value("width").toInt(0);
      if (w > bestWidth) {
        bestWidth = w;
        bestUrl = tUrl;
      }
    }
    t.thumbnailUrl = bestUrl.isEmpty()
                         ? thumbs.last().toObject().value("url").toString()
                         : bestUrl;
  } else {
    t.thumbnailUrl = obj.value("thumbnail").toString();
  }

  return t;
}
