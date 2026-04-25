#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include "PlaylistManager.h"
#include "YtDlpService.h"

/**
 * @brief PlaylistImporter – điều phối luồng import YouTube playlist.
 *
 * Streaming mode: mỗi track được thêm vào playlist ngay khi parse xong,
 * thumbnail tải async song song.
 */
class PlaylistImporter : public QObject {
  Q_OBJECT

public:
  explicit PlaylistImporter(YtDlpService *ytDlp, PlaylistManager *playlist,
                            QNetworkAccessManager *nam,
                            QObject *parent = nullptr);
  ~PlaylistImporter() override = default;

  void importPlaylist(const QString &url);

  static bool isValidPlaylistUrl(const QString &url);
  static QString validationErrorMessage(const QString &url);

signals:
  void importStarted();
  void importFinished(int trackCount);
  void importFailed(const QString &reason);
  void thumbnailLoaded(const QString &videoId, const QPixmap &pixmap);
  // Tiến trình streaming: bao nhiêu track đã load
  void trackImported(int loaded);

private slots:
  void onTrackFetched(const Track &track);
  void onMetadataReady(const QList<Track> &tracks);
  void onYtDlpError(const QString &error);

private:
  void downloadThumbnail(const QString &videoId, const QString &url);

  YtDlpService *m_ytDlp;
  PlaylistManager *m_playlist;
  QNetworkAccessManager *m_nam;
  int m_importedCount = 0;
};
