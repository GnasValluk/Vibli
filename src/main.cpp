#include <QApplication>
#include <QNetworkAccessManager>
#include <QPixmapCache>
#include <QSet>
#include <QStyleFactory>
#include <QTimer>

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
  // Bật High-DPI scaling
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);
  app.setApplicationName("VIBLI");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("VIBLI");

  // Đảm bảo Qt resource system đã init
  Q_INIT_RESOURCE(resources);

  // App icon — dùng nhiều kích thước để Windows chọn đúng
  QIcon appIcon;
  appIcon.addFile(":/imgs/logo_32.png", QSize(32, 32));
  appIcon.addFile(":/imgs/logo_256.png", QSize(256, 256));
  appIcon.addFile(":/imgs/logo.png", QSize(1024, 1024));
  app.setWindowIcon(appIcon);

  // Load icon font
  IconFont::init();

  // Giới hạn Qt internal pixmap cache — mặc định 10MB, giảm xuống 2MB
  // vì app tự quản lý thumbnail qua ThumbnailCache LRU
  QPixmapCache::setCacheLimit(2048);

  VLOG_INFO("App", "VIBLI khởi động – version 1.0.0");

  // Không thoát khi đóng cửa sổ cuối – ứng dụng chạy nền qua tray
  app.setQuitOnLastWindowClosed(false);

  // Kiểm tra hỗ trợ system tray
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    qCritical("System tray is not available on this system.");
    return 1;
  }

  // ── Khởi tạo các thành phần core ─────────────────────────────────────
  auto *audioPlayer = new AudioPlayer(&app);
  auto *playlistMgr = new PlaylistManager(&app);
  auto *ytDlpService = new YtDlpService(&app);
  auto *nam = new QNetworkAccessManager(&app);
  auto *mediaCache = new MediaCache(&app);
  auto *thumbCache = new ThumbnailCache(30, &app); // LRU: tối đa 30 ảnh ~45MB
  auto *importer = new PlaylistImporter(ytDlpService, playlistMgr, nam,
                                        mediaCache, thumbCache, &app);
  auto *persistence = new PlaylistPersistence(&app);

  // Gắn MediaCache vào YtDlpService để dùng persistent stream URL cache
  ytDlpService->setMediaCache(mediaCache);

  // ── Khởi tạo UI ──────────────────────────────────────────────────────
  auto *miniPlayer = new MiniPlayer(audioPlayer, playlistMgr, thumbCache);
  auto *mainWindow = new MainWindow(audioPlayer, playlistMgr, ytDlpService,
                                    importer, thumbCache);
  auto *trayManager = new TrayManager(&app);

  // ── Coordinator: phát YouTube track ──────────────────────────────────
  // Retry state: { videoId → số lần đã retry }
  using RetryMap = QMap<QString, int>;
  auto *retryCount = new RetryMap();

  // Debounce guard: ignore error events trong vòng 2s sau khi đã bắt đầu retry
  // Dùng QSet để track videoId đang trong cooldown
  auto *retryCooldown = new QSet<QString>();
  auto *cooldownTimer = new QTimer(&app);
  cooldownTimer->setSingleShot(false);
  cooldownTimer->setInterval(2000); // 2s cooldown per retry

  // Khi currentTrackChanged: resolve stream URL + pre-fetch track tiếp theo
  QObject::connect(playlistMgr, &PlaylistManager::currentTrackChanged, &app,
                   [audioPlayer, ytDlpService, playlistMgr, retryCount,
                    retryCooldown](int index, const Track &track) {
                     // Reset retry state khi chuyển track
                     retryCount->clear();
                     retryCooldown->clear();

                     if (track.isYouTube) {
                       ytDlpService->resolveStreamUrl(track.videoId);

                       // Pre-fetch track tiếp theo ở background
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

  // Khi streamUrlReady → phát nếu đúng track đang chờ
  QObject::connect(ytDlpService, &YtDlpService::streamUrlReady, &app,
                   [audioPlayer, playlistMgr, retryCount,
                    retryCooldown](const QString &videoId, const QUrl &url) {
                     if (playlistMgr->currentTrack().videoId != videoId)
                       return; // prefetch cho track khác, bỏ qua

                     retryCount->remove(videoId);
                     retryCooldown->remove(
                         videoId); // hết cooldown khi có URL mới
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

        // Debounce: nếu đang trong cooldown → bỏ qua error event thừa
        if (retryCooldown->contains(videoId)) {
          VLOG_DEBUG("Coordinator",
                     "Debounce error [" + videoId + "] (trong cooldown)");
          return;
        }

        const int count = retryCount->value(videoId, 0);
        VLOG_WARN("Coordinator", QString("Recoverable error [%1] retry=%2: %3")
                                     .arg(videoId)
                                     .arg(count)
                                     .arg(errorMsg));

        if (count < 2) {
          retryCount->insert(videoId, count + 1);
          retryCooldown->insert(videoId); // bắt đầu cooldown

          // Dừng player trước khi retry để tránh emit error tiếp
          // (không dùng stop() vì sẽ trigger playbackStopped)
          ytDlpService->invalidateStreamCache(videoId);
          // Delay nhỏ để QMediaPlayer flush error queue trước khi resolve
          QTimer::singleShot(300, ytDlpService, [ytDlpService, videoId]() {
            ytDlpService->resolveStreamUrl(videoId);
          });
        } else {
          retryCount->remove(videoId);
          retryCooldown->remove(videoId);
          VLOG_WARN("Coordinator", "Hết retry, skip track: " + videoId);
          if (playlistMgr->hasNext())
            playlistMgr->next();
        }
      });

  // ── Stream resolve error → retry hoặc skip
  QObject::connect(
      ytDlpService, &YtDlpService::streamErrorOccurred, &app,
      [playlistMgr, ytDlpService, retryCount](const QString &videoId,
                                              const QString &msg) {
        VLOG_WARN("Coordinator", "Stream lỗi [" + videoId + "]: " + msg);
        const int count = retryCount->value(videoId, 0);
        if (count < 1) {
          retryCount->insert(videoId, count + 1);
          VLOG_INFO("Coordinator", "Auto-retry lần 1 cho: " + videoId);
          ytDlpService->invalidateStreamCache(videoId);
          ytDlpService->resolveStreamUrl(videoId);
        } else {
          retryCount->remove(videoId);
          VLOG_WARN("Coordinator", "Bỏ qua track sau retry: " + videoId);
          if (playlistMgr->currentTrack().videoId == videoId &&
              playlistMgr->hasNext()) {
            playlistMgr->next();
          }
        }
      });

  // ── Thumbnail → MainWindow / MiniPlayer ──────────────────────────────
  QObject::connect(importer, &PlaylistImporter::thumbnailReady, mainWindow,
                   &MainWindow::onThumbnailReady);
  QObject::connect(importer, &PlaylistImporter::thumbnailReady, miniPlayer,
                   &MiniPlayer::onThumbnailReady);

  // ── Kết nối Tray → App ────────────────────────────────────────────────
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

  // ── Kết nối Player → Tray ─────────────────────────────────────────────
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

  // ── Tự động phát bài tiếp theo khi hết bài ───────────────────────────
  QObject::connect(audioPlayer, &AudioPlayer::mediaStatusChanged, &app,
                   [playlistMgr](QMediaPlayer::MediaStatus status) {
                     if (status == QMediaPlayer::EndOfMedia &&
                         playlistMgr->hasNext())
                       playlistMgr->next();
                   });

  // ── Persistence ───────────────────────────────────────────────────────
  QObject::connect(&app, &QApplication::aboutToQuit, persistence,
                   [persistence, playlistMgr]() {
                     persistence->save(playlistMgr->tracks());
                   });

  const QList<Track> savedTracks = persistence->load();
  if (!savedTracks.isEmpty()) {
    playlistMgr->addTracks(savedTracks);
    importer->restoreCachedThumbnails(savedTracks);
  }

  // ── Khởi động ─────────────────────────────────────────────────────────
  trayManager->show();
  miniPlayer->showOverlay();
  mainWindow->show();

  return app.exec();
}
