#include "TrayManager.h"
#include <QIcon>
#include <QApplication>
#include <QStyle>

TrayManager::TrayManager(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu)
{
    // Icon – dùng icon mặc định nếu chưa có resource
    QIcon icon = QIcon::fromTheme("audio-x-generic");
    if (icon.isNull())
        icon = QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip("VIBLI");

    buildMenu();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &TrayManager::onTrayActivated);
}

void TrayManager::show()
{
    m_trayIcon->show();
}

void TrayManager::buildMenu()
{
    // Track title (disabled, chỉ để hiển thị)
    m_trackTitleAction = m_menu->addAction("No track playing");
    m_trackTitleAction->setEnabled(false);
    m_menu->addSeparator();

    // Controls
    m_playPauseAction = m_menu->addAction("▶  Play / Pause");
    connect(m_playPauseAction, &QAction::triggered,
            this, &TrayManager::playPauseRequested);

    auto *prevAction = m_menu->addAction("⏮  Previous");
    connect(prevAction, &QAction::triggered,
            this, &TrayManager::previousTrackRequested);

    auto *nextAction = m_menu->addAction("⏭  Next");
    connect(nextAction, &QAction::triggered,
            this, &TrayManager::nextTrackRequested);

    m_menu->addSeparator();

    auto *overlayAction = m_menu->addAction("🎵  Toggle Overlay");
    connect(overlayAction, &QAction::triggered,
            this, &TrayManager::toggleOverlayRequested);

    auto *settingsAction = m_menu->addAction("⚙  Settings");
    connect(settingsAction, &QAction::triggered,
            this, &TrayManager::showMainWindowRequested);

    m_menu->addSeparator();

    auto *quitAction = m_menu->addAction("✕  Quit VIBLI");
    connect(quitAction, &QAction::triggered,
            this, &TrayManager::quitRequested);

    m_trayIcon->setContextMenu(m_menu);
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:       // Single click
        emit toggleOverlayRequested();
        break;
    case QSystemTrayIcon::DoubleClick:   // Double click
        emit showMainWindowRequested();
        break;
    default:
        break;
    }
}

void TrayManager::updatePlaybackState(bool playing)
{
    m_playPauseAction->setText(playing ? "⏸  Pause" : "▶  Play");
}

void TrayManager::updateTrackTitle(const QString &title)
{
    m_trackTitleAction->setText(title.isEmpty() ? "No track playing" : title);
    m_trayIcon->setToolTip("VIBLI – " + title);
}
