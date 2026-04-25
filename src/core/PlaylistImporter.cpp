#include "PlaylistImporter.h"

#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>

PlaylistImporter::PlaylistImporter(YtDlpService *ytDlp,
                                   PlaylistManager *playlist,
                                   QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_ytDlp(ytDlp), m_playlist(playlist), m_nam(nam) {
  connect(m_ytDlp, &YtDlpService::playlistMetadataReady, this,
          &PlaylistImporter::onMetadataReady);
  connect(m_ytDlp, &YtDlpService::errorOccurred, this,
          &PlaylistImporter::onYtDlpError);
}

// ── Static helpers
// ────────────────────────────────────────────────────────────

bool PlaylistImporter::isValidPlaylistUrl(const QString &url) {
  // Chấp nhận cả youtube.com và www.youtube.com, có thể có &si=... ở cuối
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
    return "URL phải là địa chỉ YouTube (youtube.com).";

  if (!url.contains("/playlist"))
    return "URL phải trỏ đến một playlist YouTube (/playlist).";

  if (!url.contains("list="))
    return "URL phải chứa tham số list= (ID playlist).";

  // list= present but value might be empty
  const int listIdx = url.indexOf("list=");
  const QString listVal = url.mid(listIdx + 5).split('&').first();
  if (listVal.isEmpty())
    return "ID playlist (list=) không được để trống.";

  return "URL playlist YouTube không hợp lệ. Định dạng đúng: "
         "https://www.youtube.com/playlist?list=<ID>";
}

// ── Public API
// ────────────────────────────────────────────────────────────────

void PlaylistImporter::importPlaylist(const QString &url) {
  if (!isValidPlaylistUrl(url)) {
    emit importFailed(validationErrorMessage(url));
    return;
  }

  emit importStarted();
  m_ytDlp->fetchPlaylistMetadata(url);
}

// ── Private slots
// ─────────────────────────────────────────────────────────────

void PlaylistImporter::onMetadataReady(const QList<Track> &tracks) {
  m_playlist->addTracks(tracks);
  emit importFinished(tracks.size());
  downloadThumbnails(tracks);
}

void PlaylistImporter::onYtDlpError(const QString &error) {
  emit importFailed(error);
}

void PlaylistImporter::downloadThumbnails(const QList<Track> &tracks) {
  for (const Track &track : tracks) {
    if (track.thumbnailUrl.isEmpty())
      continue;

    const QString videoId = track.videoId;
    QNetworkRequest request(QUrl(track.thumbnailUrl));
    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, videoId]() {
      reply->deleteLater();

      if (reply->error() != QNetworkReply::NoError) {
        // Silent on error – keep placeholder
        return;
      }

      const QByteArray data = reply->readAll();
      QPixmap pixmap;
      if (!pixmap.loadFromData(data)) {
        // Silent on decode error
        return;
      }

      emit thumbnailLoaded(videoId, pixmap);
    });
  }
}
