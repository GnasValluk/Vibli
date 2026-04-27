#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

/**
 * @brief Track – information about a single track (audio or video) in the
 * playlist.
 *
 * QPixmap is NOT stored in Track to avoid high RAM usage with large playlists.
 * Thumbnails are managed separately by ThumbnailCache (in-memory LRU) and
 * MediaCache (disk). Access via ThumbnailCache::get(videoId).
 */
struct Track {
  QUrl url;
  QString title;
  QString artist;
  QString album;
  QString genre;
  qint64 durationMs = 0;
  bool isVideo = false; // true if this is a video file

  // ── YouTube fields ────────────────────────────────────────────────────
  bool isYouTube = false; // true if sourced from YouTube
  QString videoId;        // YouTube video ID (e.g. "dQw4w9WgXcQ")
  QString thumbnailUrl;   // thumbnail URL from yt-dlp
  // QPixmap thumbnail removed — use ThumbnailCache::get(videoId)
  QString uploader;     // channel / author name
  QString description;  // video description (first 200 chars)
  qint64 viewCount = 0; // view count
  qint64 likeCount = 0; // like count
  QString uploadDate;   // upload date (YYYYMMDD)
  QString webpageUrl;   // full video page URL
};

/**
 * @brief PlaylistManager – manages the playback queue.
 *
 * Supports:
 *  - Adding / removing tracks
 *  - Track navigation (next / previous)
 *  - Shuffle and repeat modes
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
  void playlistCleared(); ///< Emitted immediately on clear — so persistence
                          ///< saves right away

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
