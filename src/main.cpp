#include <QApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QPixmapCache>
#include <QSet>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>

#include "version.h"

#include "core/AudioPlayer.h"
#include "core/LogService.h"
#include "core/MediaCache.h"
#include "core/PlaylistImporter.h"
#include "core/PlaylistManager.h"
#include "core/PlaylistPersistence.h"
#include "core/ProgressiveDownloader.h"
#include "core/ThumbnailCache.h"
#include "core/YtDlpService.h"
#include "tray/TrayManager.h"
#include "ui/IconFont.h"
#include "ui/MainWindow.h"
#include "ui/MiniPlayer.h"

int main(int argc, char *argv[]) {
  // Enable High-DPI scaling
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);
  app.setApplicationName("VIBLI");
  app.setApplicationVersion(VIBLI_VERSION);
  app.setOrganizationName("VIBLI");

  // Ensure Qt resource system is initialized
  Q_INIT_RESOURCE(resources);

  // App icon — use multiple sizes for Windows to select the right one
  QIcon appIcon;
  appIcon.addFile(":/imgs/logo_32.png", QSize(32, 32));
  appIcon.addFile(":/imgs/logo_256.png", QSize(256, 256));
  appIcon.addFile(":/imgs/logo.png", QSize(1024, 1024));
  app.setWindowIcon(appIcon);

  // Load icon font
  IconFont::init();

  // Limit Qt internal pixmap cache — default 10MB, reduce to 2MB
  // because app manages thumbnails via ThumbnailCache LRU
  QPixmapCache::setCacheLimit(2048);

  VLOG_INFO("App", QString("VIBLI started – version %1").arg(VIBLI_VERSION));

  // Don't quit when last window closes – app runs in background via tray
  app.setQuitOnLastWindowClosed(false);

  // Check system tray support
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    qCritical("System tray is not available on this system.");
    return 1;
  }

  // ── Initialize core components ───────────────────────────────────────
  auto *audioPlayer = new AudioPlayer(&app);
  auto *playlistMgr = new PlaylistManager(&app);
  auto *ytDlpService = new YtDlpService(&app);
  auto *nam = new QNetworkAccessManager(&app);
  auto *mediaCache = new MediaCache(&app);
  auto *thumbCache = new ThumbnailCache(30, &app); // LRU: max 30 images ~45MB
  auto *importer = new PlaylistImporter(ytDlpService, playlistMgr, nam,
                                        mediaCache, thumbCache, &app);
  auto *persistence = new PlaylistPersistence(&app);

  // Attach MediaCache to YtDlpService for persistent stream URL cache
  ytDlpService->setMediaCache(mediaCache);

  // ── Cleanup leftover streaming files from previous session ──────────
  const QString streamDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      "/VIBLI/streaming";
  if (QDir(streamDir).exists()) {
    QDir(streamDir).removeRecursively();
    VLOG_INFO("App",
              "Cleaned up leftover streaming files from previous session");
  }

  // ── Progressive Download for YouTube streaming ──────────────────────
  auto *progressiveDownloader = new ProgressiveDownloader(ytDlpService, &app);

  // ── Initialize UI ────────────────────────────────────────────────────
  auto *miniPlayer = new MiniPlayer(audioPlayer, playlistMgr, thumbCache);
  auto *mainWindow = new MainWindow(audioPlayer, playlistMgr, ytDlpService,
                                    importer, thumbCache);
  auto *trayManager = new TrayManager(&app);

  // ── Coordinator: play YouTube tracks ─────────────────────────────────
  // On currentTrackChanged: resolve stream URL + pre-fetch next track
  QObject::connect(
      playlistMgr, &PlaylistManager::currentTrackChanged, &app,
      [audioPlayer, ytDlpService, playlistMgr, progressiveDownloader,
       miniPlayer](int index, const Track &track) {
        // Cancel previous download
        progressiveDownloader->cancel();

        if (track.isYouTube) {
          // Use progressive download for YouTube tracks
          progressiveDownloader->startDownload(track.videoId, track.durationMs);

          // Pre-fetch next track in background (keep existing
          // logic)
          const int nextIdx = index + 1;
          if (nextIdx < playlistMgr->count()) {
            const Track next = playlistMgr->trackAt(nextIdx);
            if (next.isYouTube && !next.videoId.isEmpty())
              ytDlpService->prefetchStreamUrl(next.videoId);
          }
        } else if (track.url.isValid()) {
          // Play local files directly
          audioPlayer->play(track.url);
          // Hide buffer overlay for local tracks
          miniPlayer->updateBufferProgress(0.0f);
        }
      });

  // Connect initialBufferReady signal to start playback
  QObject::connect(
      progressiveDownloader, &ProgressiveDownloader::initialBufferReady, &app,
      [audioPlayer](const QString &tempFilePath) {
        VLOG_INFO("Coordinator", "Initial buffer ready, starting playback");
        audioPlayer->play(QUrl::fromLocalFile(tempFilePath));
      });

  // Connect bufferProgressChanged signal to update UI
  QObject::connect(progressiveDownloader,
                   &ProgressiveDownloader::bufferProgressChanged, &app,
                   [miniPlayer](qint64 bytesDownloaded, qint64 totalBytes) {
                     float progress = bytesDownloaded / (float)totalBytes;
                     miniPlayer->updateBufferProgress(progress);
                   });

  // Connect downloadComplete signal
  QObject::connect(
      progressiveDownloader, &ProgressiveDownloader::downloadComplete, &app,
      []() { VLOG_INFO("Coordinator", "Progressive download complete"); });

  // Connect downloadFailed signal to skip to next track
  QObject::connect(
      progressiveDownloader, &ProgressiveDownloader::downloadFailed, &app,
      [playlistMgr](const QString &errorMessage) {
        VLOG_ERROR("Coordinator", "Download failed: " + errorMessage);
        if (playlistMgr->hasNext()) {
          playlistMgr->next();
        }
      });

  // Cleanup on app exit
  QObject::connect(&app, &QApplication::aboutToQuit, progressiveDownloader,
                   [progressiveDownloader]() {
                     progressiveDownloader->cancel();
                     // Delete all files in streaming directory
                     const QString streamDir =
                         QStandardPaths::writableLocation(
                             QStandardPaths::TempLocation) +
                         "/VIBLI/streaming";
                     QDir(streamDir).removeRecursively();
                     VLOG_INFO("Coordinator", "Cleaned up streaming directory");
                   });

  // ── Thumbnail → MainWindow / MiniPlayer ─────────────────────────────
  QObject::connect(importer, &PlaylistImporter::thumbnailReady, mainWindow,
                   &MainWindow::onThumbnailReady);
  QObject::connect(importer, &PlaylistImporter::thumbnailReady, miniPlayer,
                   &MiniPlayer::onThumbnailReady);

  // Cover art from local file (embedded metadata)
  QObject::connect(audioPlayer, &AudioPlayer::coverArtReady, &app,
                   [thumbCache, mainWindow,
                    miniPlayer](const QString &localPath, const QImage &image) {
                     if (!thumbCache->contains(localPath)) {
                       const QPixmap pixmap = QPixmap::fromImage(image);
                       thumbCache->put(localPath, pixmap);
                       mainWindow->onThumbnailReady(localPath);
                       miniPlayer->onThumbnailReady(localPath);
                     }
                   });

  // ── Connect Tray → App ───────────────────────────────────────────────
  QObject::connect(trayManager, &TrayManager::toggleOverlayRequested,
                   miniPlayer, &MiniPlayer::toggleOverlay);
  QObject::connect(trayManager, &TrayManager::showMainWindowRequested,
                   mainWindow, &MainWindow::show);
  QObject::connect(trayManager, &TrayManager::playPauseRequested, audioPlayer,
                   [audioPlayer] {
                     audioPlayer->isPlaying() ? audioPlayer->pause()
                                              : audioPlayer->play();
                   });
  QObject::connect(trayManager, &TrayManager::nextTrackRequested, playlistMgr,
                   &PlaylistManager::next);
  QObject::connect(trayManager, &TrayManager::previousTrackRequested,
                   playlistMgr, &PlaylistManager::previous);
  QObject::connect(trayManager, &TrayManager::quitRequested, &app,
                   &QApplication::quit);

  // ── Connect Player → Tray ────────────────────────────────────────────
  QObject::connect(audioPlayer, &AudioPlayer::playbackStarted, trayManager,
                   [trayManager] { trayManager->updatePlaybackState(true); });
  QObject::connect(audioPlayer, &AudioPlayer::playbackPaused, trayManager,
                   [trayManager] { trayManager->updatePlaybackState(false); });
  QObject::connect(audioPlayer, &AudioPlayer::playbackStopped, trayManager,
                   [trayManager] { trayManager->updatePlaybackState(false); });
  QObject::connect(playlistMgr, &PlaylistManager::currentTrackChanged,
                   trayManager, [trayManager](int, const Track &t) {
                     trayManager->updateTrackTitle(t.title);
                   });

  // ── Auto-advance to next track when current ends ─────────────────────
  QObject::connect(audioPlayer, &AudioPlayer::mediaStatusChanged, &app,
                   [playlistMgr](QMediaPlayer::MediaStatus status) {
                     if (status == QMediaPlayer::EndOfMedia &&
                         playlistMgr->hasNext())
                       playlistMgr->next();
                   });

  // ── Persistence ──────────────────────────────────────────────────────
  QObject::connect(&app, &QApplication::aboutToQuit, persistence,
                   [persistence, playlistMgr]() {
                     persistence->save(playlistMgr->tracks());
                   });

  // Save immediately when user clears playlist (don't wait for app exit)
  QObject::connect(playlistMgr, &PlaylistManager::playlistCleared, &app,
                   [persistence, mediaCache]() {
                     persistence->save({});  // write empty file immediately
                     mediaCache->clearAll(); // clear stream URL cache
                     VLOG_INFO("App",
                               "Playlist cleared – persistence & cache reset");
                   });

  const QList<Track> savedTracks = persistence->load();
  if (!savedTracks.isEmpty()) {
    playlistMgr->addTracks(savedTracks);
    importer->restoreCachedThumbnails(savedTracks);
  }

  // ── Start ────────────────────────────────────────────────────────────
  trayManager->show();
  miniPlayer->showOverlay();
  mainWindow->show();

  return app.exec();
}
