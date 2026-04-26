#include "PlaylistImporter.h"

#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>

PlaylistImporter::PlaylistImporter(YtDlpService *ytDlp,
                                   PlaylistManager *playlist,
                                   QNetworkAccessManager *nam,
                                   MediaCache *cache, QObject *parent)
    : QObject(parent), m_ytDlp(ytDlp), m_playlist(playlist), m_nam(nam),
      m_cache(cache) {
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

  // Kiểm tra disk cache trước khi download
  if (!track.thumbnailUrl.isEmpty()) {
    if (m_cache && m_cache->hasThumbnail(track.videoId)) {
      // Cache hit: load từ disk, không cần network
      const QPixmap cached = m_cache->loadThumbnail(track.videoId);
      if (!cached.isNull()) {
        emit thumbnailLoaded(track.videoId, cached);
        return;
      }
    }
    downloadThumbnail(track.videoId, track.thumbnailUrl);
  }
}

void PlaylistImporter::onMetadataReady(const QList<Track> &tracks) {
  emit importFinished(tracks.size());
}

void PlaylistImporter::onYtDlpError(const QString &error) {
  // Nếu đã có một số track rồi thì không fail hoàn toàn
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
    if (px.loadFromData(reply->readAll())) {
      // Lưu xuống disk cache
      if (m_cache)
        m_cache->saveThumbnail(videoId, px);
      emit thumbnailLoaded(videoId, px);
    }
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
      if (!px.isNull())
        emit thumbnailLoaded(track.videoId, px);
    } else if (!track.thumbnailUrl.isEmpty()) {
      // Không có trong cache → download và lưu
      downloadThumbnail(track.videoId, track.thumbnailUrl);
    }
  }
}
