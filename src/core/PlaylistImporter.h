#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include "MediaCache.h"
#include "PlaylistManager.h"
#include "ThumbnailCache.h"
#include "YtDlpService.h"

/**
 * @brief PlaylistImporter – orchestrates the YouTube playlist import flow.
 *
 * Streaming mode: each track is added to the playlist as soon as it is parsed,
 * thumbnails are downloaded asynchronously in parallel. Thumbnails are cached
 * on disk (MediaCache) and in-memory LRU (ThumbnailCache).
 */
class PlaylistImporter : public QObject {
  Q_OBJECT

public:
  explicit PlaylistImporter(YtDlpService *ytDlp, PlaylistManager *playlist,
                            QNetworkAccessManager *nam, MediaCache *cache,
                            ThumbnailCache *thumbCache,
                            QObject *parent = nullptr);
  ~PlaylistImporter() override = default;

  void importPlaylist(const QString &url);

  /**
   * @brief Restores thumbnails from disk cache for already-loaded tracks.
   *
   * Called after playlist is restored from PlaylistPersistence to
   * repopulate ThumbnailCache without re-downloading.
   */
  void restoreCachedThumbnails(const QList<Track> &tracks);

  static bool isValidPlaylistUrl(const QString &url);
  static QString validationErrorMessage(const QString &url);

signals:
  void importStarted();
  void importFinished(int trackCount);
  void importFailed(const QString &reason);
  /** Thumbnail is ready in ThumbnailCache — UI uses videoId to retrieve it. */
  void thumbnailReady(const QString &videoId);
  // Streaming progress: how many tracks have been loaded
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
  MediaCache *m_cache;
  ThumbnailCache *m_thumbCache;
  int m_importedCount = 0;
};
