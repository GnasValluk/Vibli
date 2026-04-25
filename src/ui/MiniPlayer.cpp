#include "MiniPlayer.h"
#include "IconFont.h"

#include <QPainter>
#include <QPainterPath>

static constexpr int THUMB_SIZE = 70;

MiniPlayer::MiniPlayer(AudioPlayer *player,
                       PlaylistManager *playlist,
                       QWidget *parent)
    : OverlayWidget(parent)
    , m_player(player)
    , m_playlist(playlist)
{
    setupUi();
    applyStyle();
    setFixedSize(360, 90);
    setAnchorPosition(OverlayWidget::AnchorPosition::BottomRight, {20, 60});

    // ── AudioPlayer → UI ──────────────────────────────────────────────────
    connect(m_player, &AudioPlayer::playbackStarted,
            this, [this]{ updatePlaybackState(true); });
    connect(m_player, &AudioPlayer::playbackPaused,
            this, [this]{ updatePlaybackState(false); });
    connect(m_player, &AudioPlayer::playbackStopped,
            this, [this]{ updatePlaybackState(false); });
    connect(m_player, &AudioPlayer::positionChanged,
            this, &MiniPlayer::updatePosition);
    connect(m_player, &AudioPlayer::durationChanged,
            this, &MiniPlayer::updateDuration);
    connect(m_player, &AudioPlayer::videoAvailableChanged,
            this, &MiniPlayer::onVideoAvailableChanged);

    // Metadata thật từ file
    connect(m_player, &AudioPlayer::metadataChanged,
            this, [this](const QString &title, const QString &artist, const QString &) {
                if (!title.isEmpty())  m_titleLabel->setText(title);
                if (!artist.isEmpty()) m_artistLabel->setText(artist);
            });

    // ── PlaylistManager → UI ──────────────────────────────────────────────
    connect(m_playlist, &PlaylistManager::currentTrackChanged,
            this, [this](int, const Track &track){ updateTrackInfo(track); });
}

// ── Setup UI ──────────────────────────────────────────────────────────────────

void MiniPlayer::setupUi()
{
    // ── Thumbnail stack ───────────────────────────────────────────────────
    m_thumbStack = new QStackedWidget(this);
    m_thumbStack->setFixedSize(THUMB_SIZE, THUMB_SIZE);

    // Trang 0: audio art (label tối)
    m_audioArt = new QLabel(m_thumbStack);
    m_audioArt->setFixedSize(THUMB_SIZE, THUMB_SIZE);
    m_audioArt->setAlignment(Qt::AlignCenter);
    m_audioArt->setText("♪");
    m_audioArt->setObjectName("audioArt");

    // Trang 1: video thumbnail – crop center, bo góc, không bóp méo
    m_videoThumb = new VideoThumbnail(m_thumbStack);
    m_videoThumb->setFixedSize(THUMB_SIZE, THUMB_SIZE);
    m_videoThumb->setObjectName("videoThumb");

    m_thumbStack->addWidget(m_audioArt);    // index 0
    m_thumbStack->addWidget(m_videoThumb);  // index 1
    m_thumbStack->setCurrentIndex(0);

    // Gắn video output vào thumbnail
    m_videoThumb->setMediaPlayer(m_player->mediaPlayer());

    // ── Track info ────────────────────────────────────────────────────────
    m_titleLabel  = new QLabel("No track", this);
    m_artistLabel = new QLabel("Unknown artist", this);
    m_timeLabel   = new QLabel("0:00 / 0:00", this);
    m_titleLabel->setObjectName("titleLabel");
    m_artistLabel->setObjectName("artistLabel");
    m_timeLabel->setObjectName("timeLabel");
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // ── Seek slider ───────────────────────────────────────────────────────
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setObjectName("seekSlider");

    // ── Control buttons ───────────────────────────────────────────────────
    m_prevBtn      = new QPushButton(this);
    m_playPauseBtn = new QPushButton(this);
    m_nextBtn      = new QPushButton(this);
    m_muteBtn      = new QPushButton(this);
    m_prevBtn->setObjectName("controlBtn");
    m_playPauseBtn->setObjectName("playPauseBtn");
    m_nextBtn->setObjectName("controlBtn");
    m_muteBtn->setObjectName("controlBtn");
    m_prevBtn->setFixedSize(28, 28);
    m_playPauseBtn->setFixedSize(36, 36);
    m_nextBtn->setFixedSize(28, 28);
    m_muteBtn->setFixedSize(28, 28);

    m_prevBtn->setIcon(IconFont::iconDual(IconFont::SKIP_PREVIOUS, 16));
    m_prevBtn->setIconSize({16, 16});
    m_playPauseBtn->setIcon(IconFont::iconDual(IconFont::PLAY_ARROW, 20,
                                               QColor(255,255,255), QColor(255,255,255)));
    m_playPauseBtn->setIconSize({20, 20});
    m_nextBtn->setIcon(IconFont::iconDual(IconFont::SKIP_NEXT, 16));
    m_nextBtn->setIconSize({16, 16});
    m_muteBtn->setIcon(IconFont::iconDual(IconFont::VOLUME_UP, 16));
    m_muteBtn->setIconSize({16, 16});

    // ── Volume slider ─────────────────────────────────────────────────────
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(60);
    m_volumeSlider->setObjectName("volumeSlider");

    // ── Layouts ───────────────────────────────────────────────────────────
    auto *infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(2);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(m_artistLabel);
    infoLayout->addWidget(m_seekSlider);

    auto *controlLayout = new QHBoxLayout;
    controlLayout->setSpacing(4);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(m_prevBtn);
    controlLayout->addWidget(m_playPauseBtn);
    controlLayout->addWidget(m_nextBtn);
    controlLayout->addSpacing(6);
    controlLayout->addWidget(m_muteBtn);
    controlLayout->addWidget(m_volumeSlider);
    controlLayout->addStretch();
    controlLayout->addWidget(m_timeLabel);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(4);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addLayout(infoLayout);
    rightLayout->addLayout(controlLayout);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    mainLayout->addWidget(m_thumbStack);
    mainLayout->addLayout(rightLayout, 1);

    // ── Signals ───────────────────────────────────────────────────────────
    connect(m_playPauseBtn, &QPushButton::clicked, this, &MiniPlayer::onPlayPauseClicked);
    connect(m_prevBtn,      &QPushButton::clicked, this, &MiniPlayer::onPreviousClicked);
    connect(m_nextBtn,      &QPushButton::clicked, this, &MiniPlayer::onNextClicked);
    connect(m_seekSlider,   &QSlider::sliderMoved,  this, &MiniPlayer::onSeekSliderMoved);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MiniPlayer::onVolumeSliderMoved);
    connect(m_seekSlider,   &QSlider::sliderPressed,  this, [this]{ m_seeking = true; });
    connect(m_seekSlider,   &QSlider::sliderReleased, this, [this]{ m_seeking = false; });
}

void MiniPlayer::applyStyle()
{
    setStyleSheet(R"(
        QLabel#audioArt {
            background: #2a2a2a;
            border-radius: 10px;
            color: #555555;
            font-size: 24px;
        }
        QStackedWidget {
            border-radius: 10px;
            border: 1.5px solid rgba(255,255,255,0.12);
            background: #000000;
        }
        QLabel#titleLabel {
            color: #ffffff;
            font-size: 13px;
            font-weight: bold;
        }
        QLabel#artistLabel {
            color: #aaaaaa;
            font-size: 11px;
        }
        QLabel#timeLabel {
            color: #888888;
            font-size: 10px;
        }
        QPushButton#controlBtn {
            background: transparent;
            color: #cccccc;
            border: none;
            font-size: 14px;
            border-radius: 14px;
        }
        QPushButton#controlBtn:hover {
            background: rgba(255,255,255,0.1);
            color: #ffffff;
        }
        QPushButton#playPauseBtn {
            background: #1db954;
            color: #ffffff;
            border: none;
            font-size: 16px;
            border-radius: 18px;
        }
        QPushButton#playPauseBtn:hover {
            background: #1ed760;
        }
        QSlider#seekSlider::groove:horizontal {
            height: 3px;
            background: #444444;
            border-radius: 2px;
        }
        QSlider#seekSlider::sub-page:horizontal {
            background: #1db954;
            border-radius: 2px;
        }
        QSlider#seekSlider::handle:horizontal {
            width: 10px; height: 10px;
            margin: -4px 0;
            background: #ffffff;
            border-radius: 5px;
        }
        QSlider#volumeSlider::groove:horizontal {
            height: 3px;
            background: #444444;
            border-radius: 2px;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background: #888888;
            border-radius: 2px;
        }
        QSlider#volumeSlider::handle:horizontal {
            width: 8px; height: 8px;
            margin: -3px 0;
            background: #cccccc;
            border-radius: 4px;
        }
    )");
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void MiniPlayer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Nền mờ toàn widget
    QPainterPath bg;
    bg.addRoundedRect(rect(), 14, 14);
    painter.fillPath(bg, QColor(18, 18, 18, 220));

    // Border cho thumbnail (vẽ đè lên trên video/audio art)
    const QRect thumbRect = m_thumbStack->geometry();
    QPainterPath thumbBorder;
    thumbBorder.addRoundedRect(thumbRect.adjusted(-1, -1, 1, 1), 11, 11);
    painter.strokePath(thumbBorder,
        QPen(QColor(255, 255, 255, 35), 1.5));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MiniPlayer::onVideoAvailableChanged(bool available)
{
    // Chuyển thumbnail: video hoặc audio art
    m_thumbStack->setCurrentIndex(available ? 1 : 0);
}

void MiniPlayer::updateTrackInfo(const Track &track)
{
    m_titleLabel->setText(track.title.isEmpty() ? "Unknown title" : track.title);
    m_artistLabel->setText(track.artist.isEmpty() ? "Unknown artist" : track.artist);
}

void MiniPlayer::updatePlaybackState(bool playing)
{
    m_playPauseBtn->setIcon(
        IconFont::iconDual(playing ? IconFont::PAUSE : IconFont::PLAY_ARROW,
                           20, QColor(255,255,255), QColor(255,255,255)));
}

void MiniPlayer::updatePosition(qint64 posMs)
{
    if (!m_seeking && m_player->duration() > 0) {
        m_seekSlider->setValue(
            static_cast<int>(posMs * 1000 / m_player->duration()));
    }
    m_timeLabel->setText(
        formatTime(posMs) + " / " + formatTime(m_player->duration()));
}

void MiniPlayer::updateDuration(qint64 /*durMs*/) {}

void MiniPlayer::onPlayPauseClicked()
{
    m_player->isPlaying() ? m_player->pause() : m_player->play();
}

void MiniPlayer::onNextClicked()
{
    m_playlist->next();
    m_player->play(m_playlist->currentTrack().url);
}

void MiniPlayer::onPreviousClicked()
{
    m_playlist->previous();
    m_player->play(m_playlist->currentTrack().url);
}

void MiniPlayer::onSeekSliderMoved(int value)
{
    if (m_player->duration() > 0)
        m_player->seek(static_cast<qint64>(value) * m_player->duration() / 1000);
}

void MiniPlayer::onVolumeSliderMoved(int value)
{
    m_player->setVolume(value / 100.0f);
}

QString MiniPlayer::formatTime(qint64 ms) const
{
    qint64 s = ms / 1000;
    return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}
