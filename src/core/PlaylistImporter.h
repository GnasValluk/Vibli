#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>


#include "PlaylistManager.h"
#include "YtDlpService.h"


/**
 * @brief PlaylistImporter – điều phối luồng import YouTube playlist.
 *
 * Validate URL → gọi YtDlpService → thêm vào PlaylistManager → tải thumbnail.
 */
class PlaylistImporter : public QObject {
  Q_OBJECT

public:
  explicit PlaylistImporter(YtDlpService *ytDlp, PlaylistManager *playlist,
                            QNetworkAccessManager *nam,
                            QObject *parent = nullptr);
  ~PlaylistImporter() override = default;

  // Validate và bắt đầu import
  void importPlaylist(const QString &url);

  // Kiểm tra URL hợp lệ (static, dùng được từ UI)
  static bool isValidPlaylistUrl(const QString &url);
  static QString validationErrorMessage(const QString &url);

signals:
  void importStarted();
  void importFinished(int trackCount);
  void importFailed(const QString &reason);
  void thumbnailLoaded(const QString &videoId, const QPixmap &pixmap);

private slots:
  void onMetadataReady(const QList<Track> &tracks);
  void onYtDlpError(const QString &error);

private:
  void downloadThumbnails(const QList<Track> &tracks);

  YtDlpService *m_ytDlp;
  PlaylistManager *m_playlist;
  QNetworkAccessManager *m_nam;
};
