#include "AudioPlayer.h"
#include <QDebug>
#include <QMediaMetaData>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.8f);

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this,     &AudioPlayer::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this,     &AudioPlayer::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this,     &AudioPlayer::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this,     &AudioPlayer::durationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this,     &AudioPlayer::onErrorOccurred);
    connect(m_player, &QMediaPlayer::metaDataChanged,
            this,     &AudioPlayer::onMetaDataChanged);
    connect(m_player, &QMediaPlayer::hasVideoChanged,
            this,     &AudioPlayer::videoAvailableChanged);
    connect(m_audioOutput, &QAudioOutput::volumeChanged,
            this,          &AudioPlayer::volumeChanged);
}

// ── Getters ───────────────────────────────────────────────────────────────────

bool AudioPlayer::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

bool AudioPlayer::isPaused() const
{
    return m_player->playbackState() == QMediaPlayer::PausedState;
}

qint64 AudioPlayer::position() const  { return m_player->position(); }
qint64 AudioPlayer::duration() const  { return m_player->duration(); }
float  AudioPlayer::volume()   const  { return m_audioOutput->volume(); }
bool   AudioPlayer::hasVideo() const  { return m_player->hasVideo(); }

QString AudioPlayer::currentSource() const
{
    return m_player->source().toString();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AudioPlayer::play(const QUrl &url)
{
    m_player->setSource(url);
    m_player->play();
}

void AudioPlayer::play()   { m_player->play(); }
void AudioPlayer::pause()  { m_player->pause(); }
void AudioPlayer::stop()   { m_player->stop(); }

void AudioPlayer::seek(qint64 positionMs)
{
    m_player->setPosition(positionMs);
}

void AudioPlayer::setVolume(float volume)
{
    m_audioOutput->setVolume(qBound(0.0f, volume, 1.0f));
}

void AudioPlayer::setMuted(bool muted)
{
    m_audioOutput->setMuted(muted);
}

// ── Private slots ─────────────────────────────────────────────────────────────

void AudioPlayer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
    case QMediaPlayer::PlayingState: emit playbackStarted(); break;
    case QMediaPlayer::PausedState:  emit playbackPaused();  break;
    case QMediaPlayer::StoppedState: emit playbackStopped(); break;
    }
}

void AudioPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    emit mediaStatusChanged(status);
}

void AudioPlayer::onErrorOccurred(QMediaPlayer::Error /*error*/, const QString &errorString)
{
    qWarning() << "[AudioPlayer] Error:" << errorString;
    emit errorOccurred(errorString);
}

void AudioPlayer::onMetaDataChanged()
{
    const QMediaMetaData meta = m_player->metaData();

    QString title  = meta.value(QMediaMetaData::Title).toString();
    QString artist = meta.value(QMediaMetaData::ContributingArtist).toString();
    if (artist.isEmpty())
        artist = meta.value(QMediaMetaData::AlbumArtist).toString();
    QString album  = meta.value(QMediaMetaData::AlbumTitle).toString();

    if (!title.isEmpty() || !artist.isEmpty())
        emit metadataChanged(title, artist, album);
}
