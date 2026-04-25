#pragma once

#include <QObject>
#include <QList>
#include <QUrl>
#include <QString>

/**
 * @brief Track – thông tin một track (audio hoặc video) trong playlist.
 */
struct Track {
    QUrl    url;
    QString title;
    QString artist;
    QString album;
    QString genre;
    qint64  durationMs = 0;
    bool    isVideo    = false;   // true nếu là file video
};

/**
 * @brief PlaylistManager – quản lý danh sách phát.
 *
 * Hỗ trợ:
 *  - Thêm / xóa track
 *  - Chuyển bài (next / previous)
 *  - Chế độ shuffle và repeat
 */
class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    enum class RepeatMode { None, One, All };
    Q_ENUM(RepeatMode)

    explicit PlaylistManager(QObject *parent = nullptr);
    ~PlaylistManager() override = default;

    // ── Playlist ──────────────────────────────────────────────────────────
    void            addTrack(const Track &track);
    void            addTracks(const QList<Track> &tracks);
    void            removeTrack(int index);
    void            clear();
    int             count()        const;
    Track           trackAt(int index) const;
    QList<Track>    tracks()       const;

    // ── Navigation ────────────────────────────────────────────────────────
    int             currentIndex() const;
    Track           currentTrack() const;
    bool            hasNext()      const;
    bool            hasPrevious()  const;

    // ── Modes ─────────────────────────────────────────────────────────────
    bool            isShuffle()    const;
    RepeatMode      repeatMode()   const;

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
    QList<int>   m_shuffleOrder;
    int          m_currentIndex = -1;
    bool         m_shuffle      = false;
    RepeatMode   m_repeatMode   = RepeatMode::None;

    void rebuildShuffleOrder();
    int  nextIndex()     const;
    int  previousIndex() const;
};
