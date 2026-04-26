#pragma once

#include "../core/PlaylistManager.h"
#include "../core/ThumbnailCache.h"
#include <QAbstractListModel>

/**
 * @brief PlaylistModel – model cho QListView hiển thị playlist.
 *
 * Thay thế QListWidget để tận dụng virtual scrolling:
 * chỉ render ~10 item đang visible, không giữ QIcon cho 80 item trong RAM.
 *
 * Custom roles:
 *  - Qt::DisplayRole    → QString hiển thị (title + sub-line)
 *  - Qt::UserRole       → QString videoId (để delegate lấy thumbnail)
 *  - Qt::UserRole + 1   → bool isYouTube
 *  - Qt::UserRole + 2   → bool isVideo (local)
 *  - Qt::ToolTipRole    → tooltip HTML
 */
class PlaylistModel : public QAbstractListModel {
  Q_OBJECT

public:
  explicit PlaylistModel(PlaylistManager *playlist, ThumbnailCache *thumbCache,
                         QObject *parent = nullptr);

  // QAbstractListModel interface
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  /** Gọi khi thumbnail của videoId vừa sẵn sàng — invalidate row đó. */
  void onThumbnailReady(const QString &videoId);

private slots:
  void onPlaylistChanged();

private:
  QString formatDuration(qint64 ms) const;
  QString buildYouTubeTooltip(const Track &t) const;

  PlaylistManager *m_playlist;
  ThumbnailCache *m_thumbCache;
};
