#pragma once

#include "../core/PlaylistManager.h"
#include "../core/ThumbnailCache.h"
#include <QAbstractListModel>

/**
 * @brief PlaylistModel – model for QListView to display the playlist.
 *
 * Replaces QListWidget to leverage virtual scrolling:
 * only ~10 visible items are rendered, no QIcon held for 80 items in RAM.
 *
 * Custom roles:
 *  - Qt::DisplayRole    → QString display text (title + sub-line)
 *  - Qt::UserRole       → QString videoId (for delegate to fetch thumbnail)
 *  - Qt::UserRole + 1   → bool isYouTube
 *  - Qt::UserRole + 2   → bool isVideo (local)
 *  - Qt::ToolTipRole    → HTML tooltip
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

  /** Called when the thumbnail for videoId is ready — invalidates that row. */
  void onThumbnailReady(const QString &videoId);

private slots:
  void onPlaylistChanged();

private:
  QString formatDuration(qint64 ms) const;
  QString buildYouTubeTooltip(const Track &t) const;

  PlaylistManager *m_playlist;
  ThumbnailCache *m_thumbCache;
};
