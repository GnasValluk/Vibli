#include <QApplication>
#include <QNetworkAccessManager>
#include <QStyleFactory>

#include "core/AudioPlayer.h"
#include "core/PlaylistImporter.h"
#include "core/PlaylistManager.h"
#include "core/PlaylistPersistence.h"
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

  // Load icon font
  IconFont::init();

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
  auto *importer = new PlaylistImporter(ytDlpService, playlistMgr, nam, &app);
  auto *persistence = new PlaylistPersistence(&app);

  // ── Khởi tạo UI ──────────────────────────────────────────────────────
  auto *miniPlayer = new MiniPlayer(audioPlayer, playlistMgr);
  auto *mainWindow =
      new MainWindow(audioPlayer, playlistMgr, ytDlpService, importer);
  auto *trayManager = new TrayManager(&app);

  // ── Coordinator: phát YouTube track ──────────────────────────────────
  // Khi currentTrackChanged: nếu isYouTube → resolveStreamUrl; nếu local → play
  // trực tiếp
  QObject::connect(playlistMgr, &PlaylistManager::currentTrackChanged, &app,
                   [audioPlayer, ytDlpService](int, const Track &track) {
                     if (track.isYouTube) {
                       ytDlpService->resolveStreamUrl(track.videoId);
                     } else if (track.url.isValid()) {
                       audioPlayer->play(track.url);
                     }
                   });

  // Khi streamUrlReady → phát
  QObject::connect(ytDlpService, &YtDlpService::streamUrlReady, audioPlayer,
                   [audioPlayer](const QString &, const QUrl &url) {
                     audioPlayer->play(url);
                   });

  // Thumbnail loaded → cập nhật vào PlaylistManager để MiniPlayer nhận được
  QObject::connect(
      importer, &PlaylistImporter::thumbnailLoaded, playlistMgr,
      [playlistMgr](const QString &videoId, const QPixmap &pixmap) {
        playlistMgr->updateTrackThumbnail(videoId, pixmap);
      });

  // Khi YtDlpService lỗi stream → bỏ qua track, chuyển next (có guard)
  QObject::connect(ytDlpService, &YtDlpService::streamErrorOccurred, &app,
                   [playlistMgr](const QString &videoId, const QString &msg) {
                     qWarning("Stream error [%s]: %s",
                              videoId.toUtf8().constData(),
                              msg.toUtf8().constData());
                     // Chỉ skip nếu track lỗi đúng là track đang phát
                     if (playlistMgr->currentTrack().videoId == videoId &&
                         playlistMgr->hasNext()) {
                       playlistMgr->next();
                     }
                   });

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
  QObject::connect(
      audioPlayer, &AudioPlayer::mediaStatusChanged, &app,
      [audioPlayer, playlistMgr](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia && playlistMgr->hasNext()) {
          playlistMgr->next();
          // currentTrackChanged coordinator sẽ xử lý phát
        }
      });

  // ── Persistence ───────────────────────────────────────────────────────
  // Lưu playlist khi thoát
  QObject::connect(&app, &QApplication::aboutToQuit, persistence,
                   [persistence, playlistMgr]() {
                     persistence->save(playlistMgr->tracks());
                   });

  // Khôi phục playlist khi khởi động
  const QList<Track> savedTracks = persistence->load();
  if (!savedTracks.isEmpty()) {
    playlistMgr->addTracks(savedTracks);
  }

  // ── Khởi động ─────────────────────────────────────────────────────────
  trayManager->show();
  miniPlayer->showOverlay();
  mainWindow->show(); // Mở MainWindow ngay khi khởi động

  return app.exec();
}
