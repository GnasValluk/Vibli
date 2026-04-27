#pragma once

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>


/**
 * @brief TrayManager – manages the system tray icon and context menu.
 *
 * Click on tray icon → toggle MiniPlayer overlay.
 * Double-click → open MainWindow.
 */
class TrayManager : public QObject {
  Q_OBJECT

public:
  explicit TrayManager(QObject *parent = nullptr);
  ~TrayManager() override = default;

  void show();

signals:
  void toggleOverlayRequested();
  void showMainWindowRequested();
  void playPauseRequested();
  void nextTrackRequested();
  void previousTrackRequested();
  void quitRequested();

public slots:
  void updatePlaybackState(bool playing);
  void updateTrackTitle(const QString &title);

private slots:
  void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
  void buildMenu();

  QSystemTrayIcon *m_trayIcon;
  QMenu *m_menu;

  QAction *m_playPauseAction;
  QAction *m_trackTitleAction;
};
