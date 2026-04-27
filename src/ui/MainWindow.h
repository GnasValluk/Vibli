#pragma once

#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QPushButton>

#include "../core/AudioPlayer.h"
#include "../core/LogService.h"
#include "../core/PlaylistImporter.h"
#include "../core/PlaylistManager.h"
#include "../core/ThumbnailCache.h"
#include "../core/YtDlpService.h"
#include "DownloadManagerDialog.h"
#include "LoadingOverlay.h"
#include "PlaylistDelegate.h"
#include "PlaylistModel.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(AudioPlayer *player, PlaylistManager *playlist,
                      YtDlpService *ytDlpService, PlaylistImporter *importer,
                      ThumbnailCache *thumbCache, QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

public slots:
  void onThumbnailReady(const QString &videoId);

private slots:
  void onAddFolder();
  void onRemoveSelected();
  void onClearPlaylist();
  void onImportYouTube();
  void onDownloadLog();
  void onPlaylistItemDoubleClicked(const QModelIndex &index);
  void onCurrentTrackChanged(int index, const Track &track);
  void onMetadataChanged(const QString &title, const QString &artist,
                         const QString &album);
  void onRetryYouTubeTrack();

private:
  void setupUi();
  void applyStyle();
  void scanFolder(const QString &folderPath, QList<Track> &tracks);
  QString formatDuration(qint64 ms) const;
  void startDownload(const QString &url, DownloadFormat format);

  AudioPlayer *m_player;
  PlaylistManager *m_playlist;
  YtDlpService *m_ytDlpService;
  PlaylistImporter *m_importer;
  ThumbnailCache *m_thumbCache;

  // Model/View replace QListWidget
  PlaylistModel *m_playlistModel = nullptr;
  PlaylistDelegate *m_playlistDelegate = nullptr;
  QListView *m_playlistView = nullptr;

  QPushButton *m_addFolderBtn;
  QPushButton *m_removeBtn;
  QPushButton *m_clearBtn;
  QPushButton *m_importYtBtn;
  QPushButton *m_downloadLogBtn;
  QLabel *m_statusLabel;
  QLabel *m_nowPlayingLabel;
  LoadingOverlay *m_loadingOverlay = nullptr;

  // Download manager (lazy-created, non-modal)
  DownloadManagerDialog *m_downloadManager = nullptr;
};
