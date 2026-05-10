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
  // Accept both playlist URLs and single video URLs
  static const QRegularExpression playlistRe(
      R"(^https://(www\.)?youtube\.com/playlist\?.*list=.+)");
  static const QRegularExpression videoRe(
      R"(^https://(www\.)?youtube\.com/watch\?.*v=[A-Za-z0-9_-]{11})");
  static const QRegularExpression shortRe(
      R"(^https://youtu\.be/[A-Za-z0-9_-]{11})");

  return playlistRe.match(url).hasMatch() || videoRe.match(url).hasMatch() ||
         shortRe.match(url).hasMatch();
}

QString PlaylistImporter::validationErrorMessage(const QString &url) {
  if (url.isEmpty())
    return "URL must not be empty.";
  if (!url.startsWith("https://"))
    return "URL must start with https://.";
  if (!url.contains("youtube.com") && !url.contains("youtu.be"))
    return "URL must be a YouTube address.";

  // Check if it's a valid playlist or video URL
  if (url.contains("/playlist")) {
    if (!url.contains("list="))
      return "Playlist URL must contain the list= parameter.";
    const int idx = url.indexOf("list=");
    if (url.mid(idx + 5).split('&').first().isEmpty())
      return "Playlist ID (list=) must not be empty.";
  } else if (url.contains("/watch")) {
    if (!url.contains("v="))
      return "Video URL must contain the v= parameter.";
    const int idx = url.indexOf("v=");
    const QString videoId = url.mid(idx + 2).split('&').first();
    if (videoId.length() != 11)
      return "Video ID must be 11 characters long.";
  } else if (url.contains("youtu.be/")) {
    const int idx = url.indexOf("youtu.be/");
    const QString videoId = url.mid(idx + 9).split('?').first();
    if (videoId.length() != 11)
      return "Video ID must be 11 characters long.";
  } else {
    return "URL must be a YouTube playlist or video link.";
  }

  return "Invalid URL. Expected format:\n"
         "Playlist: https://www.youtube.com/playlist?list=<ID>\n"
         "Video: https://www.youtube.com/watch?v=<ID>\n"
         "Short: https://youtu.be/<ID>";
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
  // Add immediately to playlist — UI will see tracks appear progressively
  m_playlist->addTrack(track);
  m_importedCount++;
  emit trackImported(m_importedCount);

  if (track.thumbnailUrl.isEmpty())
    return;

  // Cache hit: load from disk into ThumbnailCache, no network needed
  if (m_cache && m_cache->hasThumbnail(track.videoId)) {
    const QPixmap cached = m_cache->loadThumbnail(track.videoId);
    if (!cached.isNull()) {
      // Scale once when inserting into cache — 64×64 is enough for list (56px)
      // and MiniPlayer (70px)
      const QPixmap scaled = cached.scaled(
          64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
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

    // Save original image to disk (for later restore)
    if (m_cache)
      m_cache->saveThumbnail(videoId, px);

    // Scale once before inserting into LRU cache — 64×64
    const QPixmap scaled = px.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
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
        const QPixmap scaled = px.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
        if (m_thumbCache)
          m_thumbCache->put(track.videoId, scaled);
        emit thumbnailReady(track.videoId);
      }
    } else if (!track.thumbnailUrl.isEmpty()) {
      downloadThumbnail(track.videoId, track.thumbnailUrl);
    }
  }
}
