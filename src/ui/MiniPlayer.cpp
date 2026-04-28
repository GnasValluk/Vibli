#include "MiniPlayer.h"
#include "../core/ThumbnailCache.h"
#include "IconFont.h"

#include <QPainter>
#include <QPainterPath>

static constexpr int THUMB_SIZE = 70;

MiniPlayer::MiniPlayer(AudioPlayer *player, PlaylistManager *playlist,
                       ThumbnailCache *thumbCache, QWidget *parent)
    : OverlayWidget(parent), m_player(player), m_playlist(playlist),
      m_thumbCache(thumbCache) {
  setupUi();
  applyStyle();
  setFixedSize(360, 90);
  setAnchorPosition(OverlayWidget::AnchorPosition::BottomRight, {20, 60});

  // ── AudioPlayer → UI ──────────────────────────────────────────────────
  connect(m_player, &AudioPlayer::playbackStarted, this,
          [this] { updatePlaybackState(true); });
  connect(m_player, &AudioPlayer::playbackPaused, this,
          [this] { updatePlaybackState(false); });
  connect(m_player, &AudioPlayer::playbackStopped, this,
          [this] { updatePlaybackState(false); });
  connect(m_player, &AudioPlayer::positionChanged, this,
          &MiniPlayer::updatePosition);
  connect(m_player, &AudioPlayer::durationChanged, this,
          &MiniPlayer::updateDuration);
  connect(m_player, &AudioPlayer::videoAvailableChanged, this,
          &MiniPlayer::onVideoAvailableChanged);

  // Real metadata from file
  connect(m_player, &AudioPlayer::metadataChanged, this,
          [this](const QString &title, const QString &artist, const QString &) {
            if (!title.isEmpty())
              m_titleLabel->setText(title);
            if (!artist.isEmpty())
              m_artistLabel->setText(artist);
          });

  // ── PlaylistManager → UI ──────────────────────────────────────────────
  connect(m_playlist, &PlaylistManager::currentTrackChanged, this,
          [this](int, const Track &track) { updateTrackInfo(track); });
}

// ── Setup UI
// ──────────────────────────────────────────────────────────────────

void MiniPlayer::setupUi() {
  // ── Thumbnail stack ───────────────────────────────────────────────────
  m_thumbStack = new QStackedWidget(this);
  m_thumbStack->setFixedSize(THUMB_SIZE, THUMB_SIZE);

  // Page 0: audio/YouTube thumbnail — true rounded corners
  m_audioArt = new RoundedImageWidget(10, m_thumbStack);
  m_audioArt->setFixedSize(THUMB_SIZE, THUMB_SIZE);
  m_audioArt->setPlaceholderText("♪");
  m_audioArt->setObjectName("audioArt");

  // Page 1: video thumbnail – center crop, rounded corners, no stretching
  m_videoThumb = new VideoThumbnail(m_thumbStack);
  m_videoThumb->setFixedSize(THUMB_SIZE, THUMB_SIZE);
  m_videoThumb->setObjectName("videoThumb");

  m_thumbStack->addWidget(m_audioArt);   // index 0
  m_thumbStack->addWidget(m_videoThumb); // index 1
  m_thumbStack->setCurrentIndex(0);

  // Attach video output to thumbnail
  m_videoThumb->setMediaPlayer(m_player->mediaPlayer());

  // ── Track info ────────────────────────────────────────────────────────
  m_titleLabel = new QLabel("No track", this);
  m_artistLabel = new QLabel("Unknown artist", this);
  m_timeLabel = new QLabel("0:00 / 0:00", this);
  m_titleLabel->setObjectName("titleLabel");
  m_artistLabel->setObjectName("artistLabel");
  m_timeLabel->setObjectName("timeLabel");
  m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  // ── Seek slider ───────────────────────────────────────────────────────
  m_seekSlider = new BufferedSeekSlider(Qt::Horizontal, this);
  m_seekSlider->setRange(0, 1000);
  m_seekSlider->setObjectName("seekSlider");

  // ── Control buttons ───────────────────────────────────────────────────
  m_prevBtn = new QPushButton(this);
  m_playPauseBtn = new QPushButton(this);
  m_nextBtn = new QPushButton(this);
  m_muteBtn = new QPushButton(this);
  m_shuffleBtn = new QPushButton(this);
  m_repeatBtn = new QPushButton(this);

  m_prevBtn->setObjectName("controlBtn");
  m_playPauseBtn->setObjectName("playPauseBtn");
  m_nextBtn->setObjectName("controlBtn");
  m_muteBtn->setObjectName("controlBtn");
  m_shuffleBtn->setObjectName("controlBtn");
  m_repeatBtn->setObjectName("controlBtn");

  m_prevBtn->setFixedSize(28, 28);
  m_playPauseBtn->setFixedSize(36, 36);
  m_nextBtn->setFixedSize(28, 28);
  m_muteBtn->setFixedSize(24, 24);
  m_shuffleBtn->setFixedSize(24, 24);
  m_repeatBtn->setFixedSize(24, 24);

  m_prevBtn->setIcon(IconFont::iconDual(IconFont::SKIP_PREVIOUS, 16));
  m_prevBtn->setIconSize({16, 16});
  m_playPauseBtn->setIcon(IconFont::iconDual(
      IconFont::PLAY_ARROW, 20, QColor(255, 255, 255), QColor(255, 255, 255)));
  m_playPauseBtn->setIconSize({20, 20});
  m_nextBtn->setIcon(IconFont::iconDual(IconFont::SKIP_NEXT, 16));
  m_nextBtn->setIconSize({16, 16});
  m_muteBtn->setIcon(IconFont::iconDual(IconFont::VOLUME_UP, 14));
  m_muteBtn->setIconSize({14, 14});
  m_shuffleBtn->setIcon(
      IconFont::icon(IconFont::SHUFFLE, 14, QColor(100, 100, 100)));
  m_shuffleBtn->setIconSize({14, 14});
  m_shuffleBtn->setToolTip("Shuffle");
  m_repeatBtn->setIcon(
      IconFont::icon(IconFont::REPEAT, 14, QColor(100, 100, 100)));
  m_repeatBtn->setIconSize({14, 14});
  m_repeatBtn->setToolTip("Repeat: Off");

  // ── Volume slider — shorter ───────────────────────────────────────────
  m_volumeSlider = new QSlider(Qt::Horizontal, this);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(80);
  m_volumeSlider->setFixedWidth(44); // reduced from 60 → 44
  m_volumeSlider->setObjectName("volumeSlider");

  // ── Layouts ───────────────────────────────────────────────────────────
  auto *infoLayout = new QVBoxLayout;
  infoLayout->setSpacing(2);
  infoLayout->setContentsMargins(0, 0, 0, 0);
  infoLayout->addWidget(m_titleLabel);
  infoLayout->addWidget(m_artistLabel);
  infoLayout->addWidget(m_seekSlider);

  auto *controlLayout = new QHBoxLayout;
  controlLayout->setSpacing(3);
  controlLayout->setContentsMargins(0, 0, 0, 0);
  controlLayout->addWidget(m_prevBtn);
  controlLayout->addWidget(m_playPauseBtn);
  controlLayout->addWidget(m_nextBtn);
  controlLayout->addSpacing(4);
  controlLayout->addWidget(m_muteBtn);
  controlLayout->addWidget(m_volumeSlider);
  controlLayout->addSpacing(4);
  controlLayout->addWidget(m_shuffleBtn);
  controlLayout->addWidget(m_repeatBtn);
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
  connect(m_playPauseBtn, &QPushButton::clicked, this,
          &MiniPlayer::onPlayPauseClicked);
  connect(m_prevBtn, &QPushButton::clicked, this,
          &MiniPlayer::onPreviousClicked);
  connect(m_nextBtn, &QPushButton::clicked, this, &MiniPlayer::onNextClicked);
  connect(m_seekSlider, &QSlider::sliderMoved, this,
          &MiniPlayer::onSeekSliderMoved);
  connect(m_volumeSlider, &QSlider::valueChanged, this,
          &MiniPlayer::onVolumeSliderMoved);
  connect(m_seekSlider, &QSlider::sliderPressed, this,
          [this] { m_seeking = true; });
  connect(m_seekSlider, &QSlider::sliderReleased, this,
          [this] { m_seeking = false; });
  connect(m_shuffleBtn, &QPushButton::clicked, this,
          &MiniPlayer::onShuffleClicked);
  connect(m_repeatBtn, &QPushButton::clicked, this,
          &MiniPlayer::onRepeatClicked);
}

void MiniPlayer::applyStyle() {
  setStyleSheet(R"(
        QStackedWidget {
            border-radius: 10px;
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

// ── Paint
// ─────────────────────────────────────────────────────────────────────

void MiniPlayer::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Semi-transparent background for the whole widget
  QPainterPath bg;
  bg.addRoundedRect(rect(), 14, 14);
  painter.fillPath(bg, QColor(18, 18, 18, 220));

  // Border around thumbnail (drawn on top of video/audio art)
  const QRect thumbRect = m_thumbStack->geometry();
  QPainterPath thumbBorder;
  thumbBorder.addRoundedRect(thumbRect.adjusted(-1, -1, 1, 1), 11, 11);
  painter.strokePath(thumbBorder, QPen(QColor(255, 255, 255, 35), 1.5));
}

// ── Slots
// ─────────────────────────────────────────────────────────────────────

void MiniPlayer::onVideoAvailableChanged(bool available) {
  // Switch thumbnail: video or audio art
  m_thumbStack->setCurrentIndex(available ? 1 : 0);
}

void MiniPlayer::updateTrackInfo(const Track &track) {
  m_currentVideoId = track.isYouTube ? track.videoId : track.url.toLocalFile();
  m_titleLabel->setText(track.title.isEmpty() ? "Unknown title" : track.title);

  if (track.isYouTube) {
    m_artistLabel->setText(track.uploader.isEmpty() ? "YouTube"
                                                    : track.uploader);
    if (m_thumbCache && m_thumbCache->contains(track.videoId)) {
      m_audioArt->setPixmap(m_thumbCache->get(track.videoId));
      m_thumbStack->setCurrentIndex(0);
    } else {
      m_audioArt->clearPixmap();
      m_audioArt->setPlaceholderText("▶");
    }
  } else {
    m_artistLabel->setText(track.artist.isEmpty() ? "Unknown artist"
                                                  : track.artist);
    const QString localKey = track.url.toLocalFile();
    if (m_thumbCache && m_thumbCache->contains(localKey)) {
      m_audioArt->setPixmap(m_thumbCache->get(localKey));
    } else {
      m_audioArt->clearPixmap();
      m_audioArt->setPlaceholderText(track.isVideo ? "🎬" : "♪");
    }
  }
}

void MiniPlayer::onThumbnailReady(const QString &videoId) {
  // Only update if this is the currently displayed track
  if (videoId != m_currentVideoId || !m_thumbCache)
    return;
  const QPixmap px = m_thumbCache->get(videoId);
  if (!px.isNull()) {
    m_audioArt->setPixmap(px);
    m_thumbStack->setCurrentIndex(0);
  }
}

void MiniPlayer::updatePlaybackState(bool playing) {
  m_playPauseBtn->setIcon(
      IconFont::iconDual(playing ? IconFont::PAUSE : IconFont::PLAY_ARROW, 20,
                         QColor(255, 255, 255), QColor(255, 255, 255)));
}

void MiniPlayer::updatePosition(qint64 posMs) {
  if (!m_seeking && m_player->duration() > 0) {
    m_seekSlider->setValue(
        static_cast<int>(posMs * 1000 / m_player->duration()));
  }
  m_timeLabel->setText(formatTime(posMs) + " / " +
                       formatTime(m_player->duration()));
}

void MiniPlayer::updateDuration(qint64 /*durMs*/) {}

void MiniPlayer::onPlayPauseClicked() {
  m_player->isPlaying() ? m_player->pause() : m_player->play();
}

void MiniPlayer::onNextClicked() {
  m_playlist->next();
  m_player->play(m_playlist->currentTrack().url);
}

void MiniPlayer::onPreviousClicked() {
  m_playlist->previous();
  m_player->play(m_playlist->currentTrack().url);
}

void MiniPlayer::onSeekSliderMoved(int value) {
  if (m_player->duration() > 0)
    m_player->seek(static_cast<qint64>(value) * m_player->duration() / 1000);
}

void MiniPlayer::onVolumeSliderMoved(int value) {
  m_player->setVolume(value / 100.0f);
}

void MiniPlayer::onShuffleClicked() {
  const bool newShuffle = !m_playlist->isShuffle();
  m_playlist->setShuffle(newShuffle);

  // Bright icon when on, dim when off
  const QColor c = newShuffle ? QColor(29, 185, 84) : QColor(100, 100, 100);
  m_shuffleBtn->setIcon(IconFont::icon(IconFont::SHUFFLE, 14, c));
  m_shuffleBtn->setToolTip(newShuffle ? "Shuffle: On" : "Shuffle: Off");
}

void MiniPlayer::onRepeatClicked() {
  // Cycle: None → All → One → None
  using RM = PlaylistManager::RepeatMode;
  const RM current = m_playlist->repeatMode();
  RM next;
  switch (current) {
  case RM::None:
    next = RM::All;
    break;
  case RM::All:
    next = RM::One;
    break;
  default:
    next = RM::None;
    break;
  }
  m_playlist->setRepeatMode(next);

  // Update icon and tooltip
  switch (next) {
  case RM::All:
    m_repeatBtn->setIcon(
        IconFont::icon(IconFont::REPEAT, 14, QColor(29, 185, 84)));
    m_repeatBtn->setToolTip("Repeat: All");
    break;
  case RM::One:
    m_repeatBtn->setIcon(
        IconFont::icon(IconFont::REPEAT_ONE, 14, QColor(29, 185, 84)));
    m_repeatBtn->setToolTip("Repeat: One");
    break;
  default:
    m_repeatBtn->setIcon(
        IconFont::icon(IconFont::REPEAT, 14, QColor(100, 100, 100)));
    m_repeatBtn->setToolTip("Repeat: Off");
    break;
  }
}

QString MiniPlayer::formatTime(qint64 ms) const {
  qint64 s = ms / 1000;
  return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

void MiniPlayer::updateBufferProgress(float progress) {
  if (m_seekSlider) {
    m_seekSlider->setBufferProgress(progress);
  }
}
