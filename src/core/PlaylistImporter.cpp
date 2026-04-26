#include "PlaylistImporter.h"

#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>

PlaylistImporter::PlaylistImporter(YtDlpService *ytDlp,
                                   PlaylistManager *playlist,
                                   QNetworkAccessManager *nam,
                                   MediaCache *cache,
                                   ThumbnailCache *thumbCache, QObject *parent)
    : QObject(parent), m_ytDlp(ytDlp), m_playlist(playlist), m_nam(nam),
      m_cache(cache), m_thumbCache(thumbCache) {
  connect(m_ytDlp, &YtDlpService::trackFetched, this,
          &PlaylistImporter::onTrackFetched);
  connect(m_ytDlp, &YtDlpService::playlistMetadataReady, this,
          &PlaylistImporter::onMetadataReady);
  connect(m_ytDlp, &YtDlpService::errorOccurred, this,
          &PlaylistImporter::onYtDlpError);
}

// ── Validation
// ────────────────────────────────────────────────────────────────

bool PlaylistImporter::isValidPlaylistUrl(const QString &url) {
  static const QRegularExpression re(
      R"(^https://(www\.)?youtube\.com/playlist\?.*list=.+)");
  return re.match(url).hasMatch();
}

QString PlaylistImporter::validationErrorMessage(const QString &url) {
  if (url.isEmpty())
    return "URL không được để trống.";
  if (!url.startsWith("https://"))
    return "URL phải bắt đầu bằng https://.";
  if (!url.contains("youtube.com"))
    return "URL phải là địa chỉ YouTube.";
  if (!url.contains("/playlist"))
    return "URL phải trỏ đến một playlist YouTube.";
  if (!url.contains("list="))
    return "URL phải chứa tham số list=.";
  const int idx = url.indexOf("list=");
  if (url.mid(idx + 5).split('&').first().isEmpty())
    return "ID playlist (list=) không được để trống.";
  return "URL không hợp lệ. Định dạng: "
         "https://www.youtube.com/playlist?list=<ID>";
}

// ── Public API
// ────────────────────────────────────────────────────────────────

void PlaylistImporter::importPlaylist(const QString &url) {
  if (!isValidPlaylistUrl(url)) {
    emit importFailed(validationErrorMessage(url));
    return;
  }
  m_importedCount = 0;
  emit importStarted();
  m_ytDlp->fetchPlaylistMetadata(url);
}

// ── Private slots
// ─────────────────────────────────────────────────────────────

void PlaylistImporter::onTrackFetched(const Track &track) {
  // Thêm ngay vào playlist — UI sẽ thấy track xuất hiện dần
  m_playlist->addTrack(track);
  m_importedCount++;
  emit trackImported(m_importedCount);

  if (track.thumbnailUrl.isEmpty())
    return;

  // Cache hit: load từ disk vào ThumbnailCache, không cần network
  if (m_cache && m_cache->hasThumbnail(track.videoId)) {
    const QPixmap cached = m_cache->loadThumbnail(track.videoId);
    if (!cached.isNull()) {
      // Scale xuống 1 lần duy nhất khi đưa vào cache
      const QPixmap scaled = cached.scaled(
          120, 68, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      if (m_thumbCache)
        m_thumbCache->put(track.videoId, scaled);
      emit thumbnailReady(track.videoId);
      return;
    }
  }
  downloadThumbnail(track.videoId, track.thumbnailUrl);
}

void PlaylistImporter::onMetadataReady(const QList<Track> &tracks) {
  emit importFinished(tracks.size());
}

void PlaylistImporter::onYtDlpError(const QString &error) {
  if (m_importedCount > 0)
    emit importFinished(m_importedCount);
  else
    emit importFailed(error);
}

void PlaylistImporter::downloadThumbnail(const QString &videoId,
                                         const QString &url) {
  QNetworkRequest req{QUrl(url)};
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = m_nam->get(req);

  connect(reply, &QNetworkReply::finished, this, [this, reply, videoId]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
      return;
    QPixmap px;
    if (!px.loadFromData(reply->readAll()))
      return;

    // Lưu ảnh gốc xuống disk (để restore sau)
    if (m_cache)
      m_cache->saveThumbnail(videoId, px);

    // Scale 1 lần duy nhất trước khi đưa vào LRU cache
    const QPixmap scaled = px.scaled(120, 68, Qt::KeepAspectRatioByExpanding,
                                     Qt::SmoothTransformation);
    if (m_thumbCache)
      m_thumbCache->put(videoId, scaled);

    emit thumbnailReady(videoId);
  });
}

void PlaylistImporter::restoreCachedThumbnails(const QList<Track> &tracks) {
  if (!m_cache)
    return;
  for (const Track &track : tracks) {
    if (!track.isYouTube || track.videoId.isEmpty())
      continue;
    if (m_cache->hasThumbnail(track.videoId)) {
      const QPixmap px = m_cache->loadThumbnail(track.videoId);
      if (!px.isNull()) {
        const QPixmap scaled = px.scaled(
            120, 68, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (m_thumbCache)
          m_thumbCache->put(track.videoId, scaled);
        emit thumbnailReady(track.videoId);
      }
    } else if (!track.thumbnailUrl.isEmpty()) {
      downloadThumbnail(track.videoId, track.thumbnailUrl);
    }
  }
}
