#pragma once

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QUrl>

/**
 * @brief Track – thông tin một track (audio hoặc video) trong playlist.
 */
struct Track {
  QUrl url;
  QString title;
  QString artist;
  QString album;
  QString genre;
  qint64 durationMs = 0;
  bool isVideo = false; // true nếu là file video

  // ── Trường mới cho YouTube ────────────────────────────────────────────
  bool isYouTube = false; // true nếu nguồn gốc từ YouTube
  QString videoId;        // YouTube video ID (e.g. "dQw4w9WgXcQ")
  QString thumbnailUrl;   // URL thumbnail từ yt-dlp
  QPixmap thumbnail;      // ảnh đã tải (in-memory, không serialize)
  QString uploader;       // Tên kênh / tác giả
  QString description;    // Mô tả video (200 ký tự đầu)
  qint64 viewCount = 0;   // Lượt xem
  qint64 likeCount = 0;   // Lượt thích
  QString uploadDate;     // Ngày đăng (YYYYMMDD)
  QString webpageUrl;     // URL trang video đầy đủ
};

/**
 * @brief PlaylistManager – quản lý danh sách phát.
 *
 * Hỗ trợ:
 *  - Thêm / xóa track
 *  - Chuyển bài (next / previous)
 *  - Chế độ shuffle và repeat
 */
class PlaylistManager : public QObject {
  Q_OBJECT

public:
  enum class RepeatMode { None, One, All };
  Q_ENUM(RepeatMode)

  explicit PlaylistManager(QObject *parent = nullptr);
  ~PlaylistManager() override = default;

  // ── Playlist ──────────────────────────────────────────────────────────
  void addTrack(const Track &track);
  void addTracks(const QList<Track> &tracks);
  void removeTrack(int index);
  void clear();
  int count() const;
  Track trackAt(int index) const;
  QList<Track> tracks() const;
  void updateTrackThumbnail(const QString &videoId, const QPixmap &pixmap);

  // ── Navigation ────────────────────────────────────────────────────────
  int currentIndex() const;
  Track currentTrack() const;
  bool hasNext() const;
  bool hasPrevious() const;

  // ── Modes ─────────────────────────────────────────────────────────────
  bool isShuffle() const;
  RepeatMode repeatMode() const;

public slots:
  void next();
  void previous();
  void jumpTo(int index);
  void setShuffle(bool enabled);
  void setRepeatMode(RepeatMode mode);

signals:
  void currentTrackChanged(int index, const Track &track);
  void playlistChanged();

private:
  QList<Track> m_tracks;
  QList<int> m_shuffleOrder;
  int m_currentIndex = -1;
  bool m_shuffle = false;
  RepeatMode m_repeatMode = RepeatMode::None;

  void rebuildShuffleOrder();
  int nextIndex() const;
  int previousIndex() const;
};
