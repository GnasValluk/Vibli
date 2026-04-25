#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QUrl>

/**
 * @brief AudioPlayer – wrapper quanh QMediaPlayer của Qt6.
 *
 * Hỗ trợ:
 *  - Audio: mp3, flac, wav, ogg, aac, m4a, opus, ape, wv, dsf, aiff, wma…
 *  - Video: mp4, mkv, avi, mov, webm, wmv, flv, ts, m2ts…
 *  - Đọc metadata (title, artist, album, duration)
 *  - Video output qua QVideoSink
 */
class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override = default;

    // ── Trạng thái ────────────────────────────────────────────────────────
    bool        isPlaying()      const;
    bool        isPaused()       const;
    qint64      position()       const;   // ms
    qint64      duration()       const;   // ms
    float       volume()         const;   // 0.0 – 1.0
    QString     currentSource()  const;
    bool        hasVideo()       const;

    // ── Video output ──────────────────────────────────────────────────────
    QMediaPlayer *mediaPlayer()  const { return m_player; }

public slots:
    void play(const QUrl &url);
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);
    void setVolume(float volume);   // 0.0 – 1.0
    void setMuted(bool muted);

signals:
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void volumeChanged(float volume);
    void errorOccurred(const QString &errorMessage);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void metadataChanged(const QString &title, const QString &artist,
                         const QString &album);
    void videoAvailableChanged(bool available);

private slots:
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onMetaDataChanged();

private:
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
};
