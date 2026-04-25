#include <QApplication>
#include <QStyleFactory>

#include "core/AudioPlayer.h"
#include "core/PlaylistManager.h"
#include "ui/MiniPlayer.h"
#include "ui/MainWindow.h"
#include "ui/IconFont.h"
#include "tray/TrayManager.h"

int main(int argc, char *argv[])
{
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
    auto *audioPlayer    = new AudioPlayer(&app);
    auto *playlistMgr    = new PlaylistManager(&app);

    // ── Khởi tạo UI ──────────────────────────────────────────────────────
    auto *miniPlayer  = new MiniPlayer(audioPlayer, playlistMgr);
    auto *mainWindow  = new MainWindow(audioPlayer, playlistMgr);
    auto *trayManager = new TrayManager(&app);

    // ── Kết nối Tray → App ────────────────────────────────────────────────
    QObject::connect(trayManager, &TrayManager::toggleOverlayRequested,
                     miniPlayer,  &MiniPlayer::toggleOverlay);

    QObject::connect(trayManager, &TrayManager::showMainWindowRequested,
                     mainWindow,  &MainWindow::show);

    QObject::connect(trayManager, &TrayManager::playPauseRequested,
                     audioPlayer, [audioPlayer]{
                         audioPlayer->isPlaying()
                             ? audioPlayer->pause()
                             : audioPlayer->play();
                     });

    QObject::connect(trayManager, &TrayManager::nextTrackRequested,
                     playlistMgr, &PlaylistManager::next);

    QObject::connect(trayManager, &TrayManager::previousTrackRequested,
                     playlistMgr, &PlaylistManager::previous);

    QObject::connect(trayManager, &TrayManager::quitRequested,
                     &app,        &QApplication::quit);

    // ── Kết nối Player → Tray ─────────────────────────────────────────────
    QObject::connect(audioPlayer, &AudioPlayer::playbackStarted,
                     trayManager, [trayManager]{ trayManager->updatePlaybackState(true); });
    QObject::connect(audioPlayer, &AudioPlayer::playbackPaused,
                     trayManager, [trayManager]{ trayManager->updatePlaybackState(false); });
    QObject::connect(audioPlayer, &AudioPlayer::playbackStopped,
                     trayManager, [trayManager]{ trayManager->updatePlaybackState(false); });

    QObject::connect(playlistMgr, &PlaylistManager::currentTrackChanged,
                     trayManager, [trayManager](int, const Track &t){
                         trayManager->updateTrackTitle(t.title);
                     });

    // ── Tự động phát bài tiếp theo khi hết bài ───────────────────────────
    QObject::connect(audioPlayer, &AudioPlayer::mediaStatusChanged,
                     audioPlayer, [audioPlayer, playlistMgr](QMediaPlayer::MediaStatus status){
                         if (status == QMediaPlayer::EndOfMedia && playlistMgr->hasNext()) {
                             playlistMgr->next();
                             audioPlayer->play(playlistMgr->currentTrack().url);
                         }
                     });

    // ── Khởi động ─────────────────────────────────────────────────────────
    trayManager->show();
    miniPlayer->showOverlay();
    mainWindow->show();   // Mở MainWindow ngay khi khởi động

    return app.exec();
}
