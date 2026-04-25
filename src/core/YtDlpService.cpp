#include "YtDlpService.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>


static constexpr int STREAM_TIMEOUT_MS = 30000;

YtDlpService::YtDlpService(QObject *parent) : QObject(parent) {
  // ── Metadata process ──────────────────────────────────────────────────
  m_metadataProcess = new QProcess(this);
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

// ── Public API
// ────────────────────────────────────────────────────────────────

void YtDlpService::fetchPlaylistMetadata(const QString &playlistUrl) {
  if (!isAvailable()) {
    emit errorOccurred(QString(
        "Khong tim thay yt-dlp.exe. Vui long kiem tra thu muc yt_dlp/"));
    return;
  }

  if (m_metadataProcess->state() != QProcess::NotRunning) {
    m_metadataProcess->kill();
    m_metadataProcess->waitForFinished(1000);
  }

  emit progressUpdated(QString("Dang tai metadata playlist..."));

  QStringList args;
  args << "--flat-playlist" << "--dump-json" << playlistUrl;
  m_metadataProcess->start(ytDlpPath(), args);
}

void YtDlpService::resolveStreamUrl(const QString &videoId) {
  // Check cache first
  if (m_streamCache.contains(videoId)) {
    emit streamUrlReady(videoId, m_streamCache.value(videoId));
    return;
  }

  if (!isAvailable()) {
    emit errorOccurred(QString(
        "Khong tim thay yt-dlp.exe. Vui long kiem tra thu muc yt_dlp/"));
    return;
  }

  if (m_streamProcess->state() != QProcess::NotRunning) {
    m_streamProcess->kill();
    m_streamProcess->waitForFinished(1000);
  }

  m_pendingVideoId = videoId;

  const QString videoUrl = "https://www.youtube.com/watch?v=" + videoId;
  QStringList args;
  args << "-f" << "bestaudio" << "-g" << videoUrl;

  m_streamTimer->start();
  m_streamProcess->start(ytDlpPath(), args);
}

void YtDlpService::invalidateStreamCache(const QString &videoId) {
  m_streamCache.remove(videoId);
}

// ── Parser
// ────────────────────────────────────────────────────────────────────

QList<Track>
YtDlpService::parsePlaylistOutput(const QByteArray &jsonLines) const {
  QList<Track> tracks;
  const QList<QByteArray> lines = jsonLines.split('\n');
  for (const QByteArray &line : lines) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
      continue;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
      qWarning("YtDlpService: failed to parse JSON line: %s",
               err.errorString().toUtf8().constData());
      continue;
    }

    auto track = parseVideoJson(doc.object());
    if (track.has_value()) {
      tracks.append(track.value());
    }
  }
  return tracks;
}

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
  t.thumbnailUrl = obj.value("thumbnail").toString();

  // duration may be null or absent -> durationMs = 0
  const QJsonValue durVal = obj.value("duration");
  if (durVal.isDouble()) {
    t.durationMs = static_cast<qint64>(durVal.toDouble() * 1000.0);
  } else {
    t.durationMs = 0;
  }

  return t;
}

// ── Private slots
// ─────────────────────────────────────────────────────────────

void YtDlpService::onMetadataProcessFinished(int exitCode,
                                             QProcess::ExitStatus /*status*/) {
  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_metadataProcess->readAllStandardError());
    if (errOutput.contains("private") || errOutput.contains("unavailable")) {
      emit errorOccurred(
          QString("Playlist khong kha dung hoac o che do rieng tu: ") +
          errOutput);
    } else {
      emit errorOccurred(QString("yt-dlp tra ve loi (exit ") +
                         QString::number(exitCode) + QString("): ") +
                         errOutput);
    }
    return;
  }

  const QByteArray output = m_metadataProcess->readAllStandardOutput();
  if (output.trimmed().isEmpty()) {
    emit errorOccurred(
        QString("Playlist khong co video nao hoac khong the truy cap."));
    return;
  }

  const QList<Track> tracks = parsePlaylistOutput(output);
  if (tracks.isEmpty()) {
    emit errorOccurred(QString("Khong parse duoc video nao tu playlist."));
    return;
  }

  emit playlistMetadataReady(tracks);
}

void YtDlpService::onStreamProcessFinished(int exitCode,
                                           QProcess::ExitStatus /*status*/) {
  m_streamTimer->stop();

  if (exitCode != 0) {
    const QString errOutput =
        QString::fromUtf8(m_streamProcess->readAllStandardError());
    emit errorOccurred(QString("Khong lay duoc stream URL: ") + errOutput);
    return;
  }

  const QString url =
      QString::fromUtf8(m_streamProcess->readAllStandardOutput()).trimmed();

  if (url.isEmpty()) {
    emit errorOccurred(QString("yt-dlp tra ve stream URL rong cho video: ") +
                       m_pendingVideoId);
    return;
  }

  const QUrl streamUrl(url);
  m_streamCache.insert(m_pendingVideoId, streamUrl);
  emit streamUrlReady(m_pendingVideoId, streamUrl);
}

void YtDlpService::onStreamProcessTimeout() {
  if (m_streamProcess->state() != QProcess::NotRunning) {
    m_streamProcess->kill();
  }
  emit errorOccurred(QString("Timeout 30s khi lay stream URL cho video: ") +
                     m_pendingVideoId);
}
