#pragma once

#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPushButton>

#include "../core/AudioPlayer.h"
#include "../core/PlaylistImporter.h"
#include "../core/PlaylistManager.h"
#include "../core/YtDlpService.h"
#include "LoadingOverlay.h"

/**
 * @brief MainWindow – cửa sổ quản lý playlist.
 *
 * - Không phát video ở đây (video phát trong MiniPlayer thumbnail)
 * - Import cả thư mục thay vì từng file
 * - Import YouTube playlist qua YtDlpService
 * - Ẩn khi đóng, chạy nền qua tray
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(AudioPlayer *player, PlaylistManager *playlist,
                      YtDlpService *ytDlpService, PlaylistImporter *importer,
                      QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onAddFolder();
  void onRemoveSelected();
  void onClearPlaylist();
  void onImportYouTube();
  void onPlaylistItemDoubleClicked(QListWidgetItem *item);
  void onPlaylistChanged();
  void onCurrentTrackChanged(int index, const Track &track);
  void onMetadataChanged(const QString &title, const QString &artist,
                         const QString &album);
  void onThumbnailLoaded(const QString &videoId, const QPixmap &pixmap);
  void onRetryYouTubeTrack();

private:
  void setupUi();
  void applyStyle();
  void scanFolder(const QString &folderPath, QList<Track> &tracks);
  QString formatDuration(qint64 ms) const;
  QString buildYouTubeTooltip(const Track &t) const;

  AudioPlayer *m_player;
  PlaylistManager *m_playlist;
  YtDlpService *m_ytDlpService;
  PlaylistImporter *m_importer;

  QListWidget *m_playlistView;
  QPushButton *m_addFolderBtn;
  QPushButton *m_removeBtn;
  QPushButton *m_clearBtn;
  QPushButton *m_importYtBtn;
  QLabel *m_statusLabel;
  QLabel *m_nowPlayingLabel;
  LoadingOverlay *m_loadingOverlay = nullptr;
};
