#include "TrayManager.h"
#include "../ui/IconFont.h"
#include <QApplication>
#include <QIcon>
#include <QStyle>

TrayManager::TrayManager(QObject *parent)
    : QObject(parent), m_trayIcon(new QSystemTrayIcon(this)),
      m_menu(new QMenu) {
  QIcon icon(":/imgs/logo_32.png");
  if (icon.isNull()) {
    icon = QIcon::fromTheme("audio-x-generic");
    if (icon.isNull())
      icon = QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
  }
  m_trayIcon->setIcon(icon);
  m_trayIcon->setToolTip("VIBLI");

  buildMenu();

  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &TrayManager::onTrayActivated);
}

void TrayManager::show() { m_trayIcon->show(); }

void TrayManager::buildMenu() {
  const QColor ic(220, 220, 220);  // màu icon mặc định
  const QColor icRed(255, 80, 80); // màu quit

  // Track title (disabled)
  m_trackTitleAction = m_menu->addAction("No track playing");
  m_trackTitleAction->setEnabled(false);
  m_menu->addSeparator();

  // Play / Pause
  m_playPauseAction = m_menu->addAction(
      IconFont::icon(IconFont::PLAY_ARROW, 16, ic), "Play / Pause");
  connect(m_playPauseAction, &QAction::triggered, this,
          &TrayManager::playPauseRequested);

  // Previous
  auto *prevAction = m_menu->addAction(
      IconFont::icon(IconFont::SKIP_PREVIOUS, 16, ic), "Previous");
  connect(prevAction, &QAction::triggered, this,
          &TrayManager::previousTrackRequested);

  // Next
  auto *nextAction =
      m_menu->addAction(IconFont::icon(IconFont::SKIP_NEXT, 16, ic), "Next");
  connect(nextAction, &QAction::triggered, this,
          &TrayManager::nextTrackRequested);

  m_menu->addSeparator();

  // Toggle Overlay
  auto *overlayAction = m_menu->addAction(
      IconFont::icon(IconFont::QUEUE_MUSIC, 16, ic), "Toggle Overlay");
  connect(overlayAction, &QAction::triggered, this,
          &TrayManager::toggleOverlayRequested);

  // Settings / Playlist
  auto *settingsAction =
      m_menu->addAction(IconFont::icon(IconFont::SETTINGS, 16, ic), "Settings");
  connect(settingsAction, &QAction::triggered, this,
          &TrayManager::showMainWindowRequested);

  m_menu->addSeparator();

  // Quit
  auto *quitAction = m_menu->addAction(
      IconFont::icon(IconFont::CLOSE, 16, icRed), "Quit VIBLI");
  connect(quitAction, &QAction::triggered, this, &TrayManager::quitRequested);

  m_trayIcon->setContextMenu(m_menu);
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
  case QSystemTrayIcon::Trigger:
    emit toggleOverlayRequested();
    break;
  case QSystemTrayIcon::DoubleClick:
    emit showMainWindowRequested();
    break;
  default:
    break;
  }
}

void TrayManager::updatePlaybackState(bool playing) {
  const QColor ic(220, 220, 220);
  m_playPauseAction->setIcon(
      IconFont::icon(playing ? IconFont::PAUSE : IconFont::PLAY_ARROW, 16, ic));
  m_playPauseAction->setText(playing ? "Pause" : "Play");
}

void TrayManager::updateTrackTitle(const QString &title) {
  m_trackTitleAction->setText(title.isEmpty() ? "No track playing" : title);
  m_trayIcon->setToolTip("VIBLI – " + title);
}
