#include "YtDlpService.h"
#include "LogService.h"
#include "MediaCache.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

static constexpr int STREAM_TIMEOUT_MS = 35000; // 35s — đủ cho mạng chậm

// Format priority: m4a tốt nhất trên Windows/FFmpeg, webm fallback, best cuối
// Dùng selector yt-dlp: https://github.com/yt-dlp/yt-dlp#format-selection
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
}

QString YtDlpService::ytDlpPath() {
  return QCoreApplication::applicationDirPath() + "/yt_dlp/yt-dlp.exe";
}

bool YtDlpService::isAvailable() { return QFileInfo::exists(ytDlpPath()); }

void YtDlpService::setMediaCache(MediaCache *cache) { m_mediaCache = cache; }

// ── Public API
// ────────────────────────────────────────────────────────────────

void YtDlpService::fetchPlaylistMetadata(const QString &playlistUrl) {
  if (!isAvailable()) {
    VLOG_ERROR("YtDlpService", "Không tìm thấy yt-dlp.exe tại: " + ytDlpPath());
    emit errorOccurred(
        "Khong tim thay yt-dlp.exe. Vui long kiem tra thu muc yt_dlp/");
    return;
  }
  if (m_metadataProcess->state() != QProcess::NotRunning) {
    m_metadataProcess->kill();
    m_metadataProcess->waitForFinished(1000);
  }
  m_metadataBuffer.clear();
  m_fetchedCount = 0;
  VLOG_INFO("YtDlpService", "Bắt đầu fetch playlist: " + playlistUrl);
  emit progressUpdated("Đang tải playlist...");
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

  // Cache hit: trả về ngay
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

  // Nếu đang xử lý đúng videoId này rồi → bỏ qua
  if (m_streamBusy && m_pendingVideoId == videoId && !m_pendingIsPrefetch)
    return;

  // Đưa lên đầu queue (ưu tiên cao — user đang chờ)
  // Xóa entry cũ nếu đã có trong queue
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

  // Đã có cache → không cần prefetch
  if (m_streamCache.contains(videoId))
    return;
  if (m_mediaCache && m_mediaCache->hasStreamUrl(videoId))
    return;

  // Đã có trong queue → không thêm nữa
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

// ── Private helpers
// ───────────────────────────────────────────────────────────

void YtDlpService::processNextInQueue() {
  if (m_streamBusy || m_streamQueue.isEmpty())
    return;
  if (!isAvailable()) {
    // Drain queue với lỗi
    while (!m_streamQueue.isEmpty()) {
      const auto req = m_streamQueue.dequeue();
      if (!req.isPrefetch)
        emit streamErrorOccurred(req.videoId, "Không tìm thấy yt-dlp.exe");
    }
    return;
  }

  const StreamRequest req = m_streamQueue.dequeue();
  m_pendingVideoId = req.videoId;
  m_pendingFormatIdx = 0; // bắt đầu từ format ưu tiên nhất
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
            QString("Resolve stream [%1] format=%2").arg(videoId, format));

  const QString videoUrl = "https://www.youtube.com/watch?v=" + videoId;
  QStringList args;
  args << "-f" << format << "--no-warnings"
       << "-g" << videoUrl;

  m_streamTimer->start();
  m_streamProcess->start(ytDlpPath(), args);
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
      emit errorOccurred("yt-dlp lỗi (exit " + QString::number(exitCode) +
                         "): " + errOutput);
    }
    return;
  }

  if (m_fetchedCount == 0) {
    emit errorOccurred("Playlist không có video nào hoặc không thể truy cập.");
    return;
  }

  emit fetchProgress(m_fetchedCount, m_fetchedCount);
  VLOG_INFO("YtDlpService",
            QString("Fetch xong playlist: %1 track").arg(m_fetchedCount));
  emit playlistMetadataReady({});
}

void YtDlpService::onStreamProcessFinished(int exitCode,
                                           QProcess::ExitStatus /*status*/) {
  m_streamTimer->stop();

  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_streamProcess->readAllStandardError()).trimmed();
    VLOG_WARN("YtDlpService", QString("Stream lỗi [%1] format_idx=%2: %3")
                                  .arg(m_pendingVideoId)
                                  .arg(m_pendingFormatIdx)
                                  .arg(errOutput));

    // Thử format tiếp theo nếu còn
    const int nextIdx = m_pendingFormatIdx + 1;
    if (nextIdx < FORMAT_PRIORITY.size()) {
      m_pendingFormatIdx = nextIdx;
      VLOG_INFO("YtDlpService",
                QString("Thử format fallback [%1]: %2")
                    .arg(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]));
      startStreamProcess(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]);
      return; // không giải phóng m_streamBusy — vẫn đang xử lý
    }

    // Hết format fallback → báo lỗi
    VLOG_ERROR("YtDlpService", "Hết format fallback cho: " + m_pendingVideoId);
    m_streamBusy = false;
    if (!m_pendingIsPrefetch)
      emit streamErrorOccurred(m_pendingVideoId,
                               "Không lấy được stream URL sau " +
                                   QString::number(FORMAT_PRIORITY.size()) +
                                   " format: " + errOutput);
    processNextInQueue();
    return;
  }

  // Đọc tất cả URL từ output (yt-dlp có thể trả về nhiều dòng cho DASH)
  const QString rawOutput =
      QString::fromUtf8(m_streamProcess->readAllStandardOutput()).trimmed();

  // Lấy dòng đầu tiên là audio URL (khi có video+audio, dòng 2 là video)
  const QString urlStr = rawOutput.split('\n').first().trimmed();

  if (urlStr.isEmpty()) {
    VLOG_ERROR("YtDlpService", "Stream URL rỗng cho: " + m_pendingVideoId);
    m_streamBusy = false;
    if (!m_pendingIsPrefetch)
      emit streamErrorOccurred(m_pendingVideoId, "yt-dlp trả về URL rỗng");
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

  // Emit chỉ khi không phải prefetch silent, hoặc luôn emit để coordinator biết
  emit streamUrlReady(resolvedId, streamUrl);

  processNextInQueue();
}

void YtDlpService::onStreamProcessTimeout() {
  if (m_streamProcess->state() != QProcess::NotRunning)
    m_streamProcess->kill();

  VLOG_WARN("YtDlpService", QString("Timeout [%1] format_idx=%2")
                                .arg(m_pendingVideoId)
                                .arg(m_pendingFormatIdx));

  // Timeout cũng thử format tiếp theo
  const int nextIdx = m_pendingFormatIdx + 1;
  if (nextIdx < FORMAT_PRIORITY.size()) {
    m_pendingFormatIdx = nextIdx;
    VLOG_INFO("YtDlpService",
              QString("Timeout → thử format fallback [%1]: %2")
                  .arg(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]));
    startStreamProcess(m_pendingVideoId, FORMAT_PRIORITY[nextIdx]);
    return;
  }

  m_streamBusy = false;
  if (!m_pendingIsPrefetch)
    emit streamErrorOccurred(m_pendingVideoId,
                             "Timeout khi lấy stream URL (tất cả format)");
  processNextInQueue();
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
