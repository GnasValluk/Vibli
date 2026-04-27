#include <QApplication>
#include <QNetworkAccessManager>
#include <QPixmapCache>
#include <QSet>
#include <QStyleFactory>
#include <QTimer>

#include "version.h"

#include "core/AudioPlayer.h"
#include "core/LogService.h"
#include "core/MediaCache.h"
#include "core/PlaylistImporter.h"
#include "core/PlaylistManager.h"
#include "core/PlaylistPersistence.h"
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

  // ── Initialize UI ────────────────────────────────────────────────────
  auto *miniPlayer = new MiniPlayer(audioPlayer, playlistMgr, thumbCache);
  auto *mainWindow = new MainWindow(audioPlayer, playlistMgr, ytDlpService,
                                    importer, thumbCache);
  auto *trayManager = new TrayManager(&app);

  // ── Coordinator: play YouTube tracks ─────────────────────────────────
  // Retry state: { videoId → retry count }
  using RetryMap = QMap<QString, int>;
  auto *retryCount = new RetryMap();

  // Debounce guard: ignore error events within 2s after a retry has started
  // Use QSet to track videoIds currently in cooldown
  auto *retryCooldown = new QSet<QString>();
  auto *cooldownTimer = new QTimer(&app);
  cooldownTimer->setSingleShot(false);
  cooldownTimer->setInterval(2000); // 2s cooldown per retry

  // On currentTrackChanged: resolve stream URL + pre-fetch next track
  QObject::connect(playlistMgr, &PlaylistManager::currentTrackChanged, &app,
                   [audioPlayer, ytDlpService, playlistMgr, retryCount,
                    retryCooldown](int index, const Track &track) {
                     // Reset retry state on track change
                     retryCount->clear();
                     retryCooldown->clear();

                     if (track.isYouTube) {
                       ytDlpService->resolveStreamUrl(track.videoId);

                       // Pre-fetch next track in background
                       const int nextIdx = index + 1;
                       if (nextIdx < playlistMgr->count()) {
                         const Track next = playlistMgr->trackAt(nextIdx);
                         if (next.isYouTube && !next.videoId.isEmpty())
                           ytDlpService->prefetchStreamUrl(next.videoId);
                       }
                     } else if (track.url.isValid()) {
                       audioPlayer->play(track.url);
                     }
                   });

  // On streamUrlReady → play if this is the track we're waiting for
  QObject::connect(ytDlpService, &YtDlpService::streamUrlReady, &app,
                   [audioPlayer, playlistMgr, retryCount,
                    retryCooldown](const QString &videoId, const QUrl &url) {
                     if (playlistMgr->currentTrack().videoId != videoId)
                       return; // prefetch for a different track, ignore

                     retryCount->remove(videoId);
                     retryCooldown->remove(
                         videoId); // cooldown ends when new URL arrives
                     audioPlayer->play(url);
                   });

  // ── Recoverable error (Demuxing failed, 403, network) → invalidate + retry
  QObject::connect(
      audioPlayer, &AudioPlayer::recoverableErrorOccurred, &app,
      [playlistMgr, ytDlpService, retryCount,
       retryCooldown](const QString &errorMsg) {
        const Track current = playlistMgr->currentTrack();
        if (!current.isYouTube || current.videoId.isEmpty())
          return;

        const QString videoId = current.videoId;

        // Debounce: if already in cooldown → ignore redundant error events
        if (retryCooldown->contains(videoId)) {
          VLOG_DEBUG("Coordinator",
                     "Debounce error [" + videoId + "] (in cooldown)");
          return;
        }

        const int count = retryCount->value(videoId, 0);
        VLOG_WARN("Coordinator", QString("Recoverable error [%1] retry=%2: %3")
                                     .arg(videoId)
                                     .arg(count)
                                     .arg(errorMsg));

        if (count < 2) {
          retryCount->insert(videoId, count + 1);
          retryCooldown->insert(videoId); // start cooldown

          // Invalidate cache before retry to avoid replaying the bad URL
          // (don't call stop() as it would trigger playbackStopped)
          ytDlpService->invalidateStreamCache(videoId);
          // Small delay to let QMediaPlayer flush its error queue before
          // resolving
          QTimer::singleShot(300, ytDlpService, [ytDlpService, videoId]() {
            ytDlpService->resolveStreamUrl(videoId);
          });
        } else {
          retryCount->remove(videoId);
          retryCooldown->remove(videoId);
          VLOG_WARN("Coordinator",
                    "Retries exhausted, skipping track: " + videoId);
          if (playlistMgr->hasNext())
            playlistMgr->next();
        }
      });

  // ── Stream resolve error → retry or skip
  QObject::connect(
      ytDlpService, &YtDlpService::streamErrorOccurred, &app,
      [playlistMgr, ytDlpService, retryCount](const QString &videoId,
                                              const QString &msg) {
        VLOG_WARN("Coordinator", "Stream error [" + videoId + "]: " + msg);
        const int count = retryCount->value(videoId, 0);
        if (count < 1) {
          retryCount->insert(videoId, count + 1);
          VLOG_INFO("Coordinator", "Auto-retry attempt 1 for: " + videoId);
          ytDlpService->invalidateStreamCache(videoId);
          ytDlpService->resolveStreamUrl(videoId);
        } else {
          retryCount->remove(videoId);
          VLOG_WARN("Coordinator", "Skipping track after retry: " + videoId);
          if (playlistMgr->currentTrack().videoId == videoId &&
              playlistMgr->hasNext()) {
            playlistMgr->next();
          }
        }
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
