#include "PlaylistModel.h"

PlaylistModel::PlaylistModel(PlaylistManager *playlist,
                             ThumbnailCache *thumbCache, QObject *parent)
    : QAbstractListModel(parent), m_playlist(playlist),
      m_thumbCache(thumbCache) {
  connect(m_playlist, &PlaylistManager::playlistChanged, this,
          &PlaylistModel::onPlaylistChanged);
}

int PlaylistModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_playlist->count();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_playlist->count())
    return {};

  const Track &t = m_playlist->trackAt(index.row());

  switch (role) {
  case Qt::DisplayRole: {
    if (t.isYouTube) {
      const QString dur = formatDuration(t.durationMs);
      QString sub;
      if (!t.uploader.isEmpty())
        sub += t.uploader;
      if (!dur.isEmpty() && dur != "0:00") {
        if (!sub.isEmpty())
          sub += "  ·  ";
        sub += dur;
      }
      if (t.viewCount > 0) {
        if (!sub.isEmpty())
          sub += "  ·  ";
        if (t.viewCount >= 1000000)
          sub += QString::number(t.viewCount / 1000000) + "M views";
        else if (t.viewCount >= 1000)
          sub += QString::number(t.viewCount / 1000) + "K views";
        else
          sub += QString::number(t.viewCount) + " views";
      }
      // Dùng \n để delegate tách 2 dòng
      return t.title + (sub.isEmpty() ? "" : "\n" + sub);
    } else {
      return t.artist.isEmpty() ? t.title : t.artist + " – " + t.title;
    }
  }

  case Qt::DecorationRole:
    // Không trả QIcon ở đây — delegate tự lấy từ ThumbnailCache
    // để tránh duplicate pixmap
    return {};

  case Qt::ToolTipRole:
    return t.isYouTube ? buildYouTubeTooltip(t) : QVariant{};

  case Qt::UserRole:
    // YouTube → videoId; local → đường dẫn file (làm key ThumbnailCache)
    return t.isYouTube ? t.videoId : t.url.toLocalFile();

  case Qt::UserRole + 1:
    return t.isYouTube;

  case Qt::UserRole + 2:
    return t.isVideo;

  default:
    return {};
  }
}

void PlaylistModel::onPlaylistChanged() {
  beginResetModel();
  endResetModel();
}

void PlaylistModel::onThumbnailReady(const QString &key) {
  const int count = m_playlist->count();
  for (int i = 0; i < count; ++i) {
    const Track &t = m_playlist->trackAt(i);
    const QString trackKey = t.isYouTube ? t.videoId : t.url.toLocalFile();
    if (trackKey == key) {
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx, {Qt::DecorationRole});
      break;
    }
  }
}

QString PlaylistModel::formatDuration(qint64 ms) const {
  if (ms <= 0)
    return "0:00";
  const qint64 s = ms / 1000;
  return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

QString PlaylistModel::buildYouTubeTooltip(const Track &t) const {
  QStringList lines;
  lines << "<b>" + t.title.toHtmlEscaped() + "</b>";
  if (!t.uploader.isEmpty())
    lines << "📺 " + t.uploader.toHtmlEscaped();
  if (t.durationMs > 0)
    lines << "⏱ " + formatDuration(t.durationMs);
  if (t.viewCount > 0)
    lines << "👁 " + QString::number(t.viewCount) + " views";
  if (t.likeCount > 0)
    lines << "👍 " + QString::number(t.likeCount) + " likes";
  if (!t.uploadDate.isEmpty() && t.uploadDate.length() == 8)
    lines << "📅 " + t.uploadDate.mid(0, 4) + "-" + t.uploadDate.mid(4, 2) +
                 "-" + t.uploadDate.mid(6, 2);
  if (!t.description.isEmpty())
    lines << "<hr>" + t.description.toHtmlEscaped().replace("\n", "<br>");
  return lines.join("<br>");
}
