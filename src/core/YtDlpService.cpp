#include "YtDlpService.h"
#include "LogService.h"
#include "MediaCache.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

static constexpr int STREAM_TIMEOUT_MS = 30000;

YtDlpService::YtDlpService(QObject *parent) : QObject(parent) {
  // ── Metadata process ──────────────────────────────────────────────────
  m_metadataProcess = new QProcess(this);
  connect(m_metadataProcess, &QProcess::readyReadStandardOutput, this,
          &YtDlpService::onMetadataReadyRead);
  connect(m_metadataProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &YtDlpService::onMetadataProcessFinished);

  // ── Stream process ────────────────────────────────────────────────────
  m_streamProcess = new QProcess(this);
  connect(m_streamProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &YtDlpService::onStreamProcessFinished);

  // ── Stream timeout timer ──────────────────────────────────────────────
  m_streamTimer = new QTimer(this);
  m_streamTimer->setSingleShot(true);
  m_streamTimer->setInterval(STREAM_TIMEOUT_MS);
  connect(m_streamTimer, &QTimer::timeout, this,
          &YtDlpService::onStreamProcessTimeout);
}

// ── Static helpers
// ────────────────────────────────────────────────────────────

QString YtDlpService::ytDlpPath() {
  return QCoreApplication::applicationDirPath() + "/yt_dlp/yt-dlp.exe";
}

bool YtDlpService::isAvailable() { return QFileInfo::exists(ytDlpPath()); }

// ── MediaCache integration
// ────────────────────────────────────────────────

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
  m_fetchedTracks.clear();
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
  // Layer 1: in-memory cache
  if (m_streamCache.contains(videoId)) {
    VLOG_DEBUG("YtDlpService", "Memory cache hit cho videoId: " + videoId);
    emit streamUrlReady(videoId, m_streamCache.value(videoId));
    return;
  }

  // Layer 2: persistent disk cache (TTL 6h)
  if (m_mediaCache && m_mediaCache->hasStreamUrl(videoId)) {
    const QUrl cachedUrl = m_mediaCache->loadStreamUrl(videoId);
    if (cachedUrl.isValid()) {
      VLOG_DEBUG("YtDlpService", "Disk cache hit cho videoId: " + videoId);
      // Nạp vào memory cache để lần sau nhanh hơn
      m_streamCache.insert(videoId, cachedUrl);
      emit streamUrlReady(videoId, cachedUrl);
      return;
    }
  }

  if (!isAvailable()) {
    VLOG_ERROR("YtDlpService", "Không tìm thấy yt-dlp.exe");
    emit streamErrorOccurred(videoId, "Không tìm thấy yt-dlp.exe");
    return;
  }
  if (m_streamProcess->state() != QProcess::NotRunning) {
    m_streamProcess->kill();
    m_streamProcess->waitForFinished(1000);
  }
  m_pendingVideoId = videoId;
  VLOG_INFO("YtDlpService", "Đang lấy stream URL cho: " + videoId);

  const QString videoUrl = "https://www.youtube.com/watch?v=" + videoId;
  QStringList args;
  args << "-f" << "bestaudio" << "-g" << videoUrl;

  m_streamTimer->start();
  m_streamProcess->start(ytDlpPath(), args);
}

void YtDlpService::invalidateStreamCache(const QString &videoId) {
  m_streamCache.remove(videoId);
  if (m_mediaCache)
    m_mediaCache->invalidateStreamUrl(videoId);
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

  // ── Uploader / channel ────────────────────────────────────────────────
  t.uploader = obj.value("uploader").toString();
  if (t.uploader.isEmpty())
    t.uploader = obj.value("channel").toString();
  t.artist = t.uploader; // dùng chung field artist

  // ── Duration ──────────────────────────────────────────────────────────
  const QJsonValue durVal = obj.value("duration");
  t.durationMs =
      durVal.isDouble() ? static_cast<qint64>(durVal.toDouble() * 1000.0) : 0;

  // ── Stats ─────────────────────────────────────────────────────────────
  t.viewCount = static_cast<qint64>(obj.value("view_count").toDouble(0));
  t.likeCount = static_cast<qint64>(obj.value("like_count").toDouble(0));
  t.uploadDate = obj.value("upload_date").toString();

  // ── Description (200 ký tự đầu) ──────────────────────────────────────
  t.description = obj.value("description").toString().left(200);

  // ── Thumbnail – chọn ảnh rộng nhất từ mảng thumbnails[] ──────────────
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

// ── Private slots
// ─────────────────────────────────────────────────────────────

void YtDlpService::onMetadataReadyRead() {
  // Đọc output mới, ghép vào buffer
  m_metadataBuffer += m_metadataProcess->readAllStandardOutput();

  // Tách từng dòng hoàn chỉnh (kết thúc bằng \n)
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
    m_fetchedTracks.append(track.value());

    // Emit ngay để UI hiển thị dần
    emit trackFetched(track.value());
    emit fetchProgress(m_fetchedCount, -1); // total chưa biết
  }
}

void YtDlpService::onMetadataProcessFinished(int exitCode,
                                             QProcess::ExitStatus /*status*/) {
  // Xử lý phần còn lại trong buffer (dòng cuối không có \n)
  if (!m_metadataBuffer.trimmed().isEmpty()) {
    QJsonParseError err;
    const QJsonDocument doc =
        QJsonDocument::fromJson(m_metadataBuffer.trimmed(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      auto track = parseVideoJson(doc.object());
      if (track.has_value()) {
        m_fetchedCount++;
        m_fetchedTracks.append(track.value());
        emit trackFetched(track.value());
      }
    }
    m_metadataBuffer.clear();
  }

  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_metadataProcess->readAllStandardError());
    if (m_fetchedCount > 0) {
      // Có một số track rồi, vẫn emit kết quả
      emit fetchProgress(m_fetchedCount, m_fetchedCount);
      emit playlistMetadataReady(m_fetchedTracks);
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
  emit playlistMetadataReady(m_fetchedTracks);
}

void YtDlpService::onStreamProcessFinished(int exitCode,
                                           QProcess::ExitStatus /*status*/) {
  m_streamTimer->stop();

  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_streamProcess->readAllStandardError());
    VLOG_ERROR("YtDlpService",
               "Stream lỗi [" + m_pendingVideoId + "]: " + errOutput.trimmed());
    emit streamErrorOccurred(m_pendingVideoId,
                             "Không lấy được stream URL: " + errOutput);
    return;
  }

  const QString url =
      QString::fromUtf8(m_streamProcess->readAllStandardOutput()).trimmed();

  if (url.isEmpty()) {
    VLOG_ERROR("YtDlpService", "Stream URL rỗng cho: " + m_pendingVideoId);
    emit streamErrorOccurred(m_pendingVideoId, "yt-dlp trả về stream URL rỗng");
    return;
  }

  const QUrl streamUrl(url);
  m_streamCache.insert(m_pendingVideoId, streamUrl);
  // Lưu vào persistent cache để dùng lại sau khi restart
  if (m_mediaCache)
    m_mediaCache->saveStreamUrl(m_pendingVideoId, streamUrl);
  VLOG_INFO("YtDlpService", "Stream URL OK cho: " + m_pendingVideoId);
  emit streamUrlReady(m_pendingVideoId, streamUrl);
}

void YtDlpService::onStreamProcessTimeout() {
  if (m_streamProcess->state() != QProcess::NotRunning)
    m_streamProcess->kill();
  VLOG_WARN("YtDlpService",
            "Timeout 30s khi lấy stream URL cho: " + m_pendingVideoId);
  emit streamErrorOccurred(m_pendingVideoId, "Timeout 30s khi lấy stream URL");
}
