#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

#include "../core/AudioPlayer.h"
#include "../core/PlaylistManager.h"

/**
 * @brief MainWindow – cửa sổ quản lý playlist.
 *
 * - Không phát video ở đây (video phát trong MiniPlayer thumbnail)
 * - Import cả thư mục thay vì từng file
 * - Ẩn khi đóng, chạy nền qua tray
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AudioPlayer *player,
                        PlaylistManager *playlist,
                        QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAddFolder();
    void onRemoveSelected();
    void onClearPlaylist();
    void onPlaylistItemDoubleClicked(QListWidgetItem *item);
    void onPlaylistChanged();
    void onCurrentTrackChanged(int index, const Track &track);
    void onMetadataChanged(const QString &title, const QString &artist,
                           const QString &album);

private:
    void setupUi();
    void applyStyle();
    void scanFolder(const QString &folderPath, QList<Track> &tracks);

    AudioPlayer     *m_player;
    PlaylistManager *m_playlist;

    QListWidget *m_playlistView;
    QPushButton *m_addFolderBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QLabel      *m_statusLabel;
    QLabel      *m_nowPlayingLabel;
};
