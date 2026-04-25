#pragma once

#include "../core/AudioPlayer.h"
#include "../core/PlaylistManager.h"
#include "OverlayWidget.h"

#include "RoundedImageWidget.h"
#include "VideoThumbnail.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVideoWidget>

/**
 * @brief MiniPlayer – overlay player.
 *
 * Layout:
 *  ┌──────────────────────────────────────────┐
 *  │ [Thumbnail/Video 70x70]  Title / Artist  │
 *  │                          ─────────────── │
 *  │                          |◄  ▶  ►|  🔊─ │
 *  └──────────────────────────────────────────┘
 *
 * - Audio: thumbnail là ô vuông tối với icon nhạc
 * - Video: thumbnail phát video trực tiếp, fill vuông (crop center)
 */
class MiniPlayer : public OverlayWidget {
  Q_OBJECT

public:
  explicit MiniPlayer(AudioPlayer *player, PlaylistManager *playlist,
                      QWidget *parent = nullptr);
  ~MiniPlayer() override = default;

public slots:
  void updateTrackInfo(const Track &track);
  void updatePlaybackState(bool playing);
  void updatePosition(qint64 posMs);
  void updateDuration(qint64 durMs);

private slots:
  void onPlayPauseClicked();
  void onNextClicked();
  void onPreviousClicked();
  void onSeekSliderMoved(int value);
  void onVolumeSliderMoved(int value);
  void onVideoAvailableChanged(bool available);

private:
  void setupUi();
  void applyStyle();
  void paintEvent(QPaintEvent *event) override;
  QString formatTime(qint64 ms) const;

  AudioPlayer *m_player;
  PlaylistManager *m_playlist;

  // ── Thumbnail area (stack: video hoặc audio art) ──────────────────────
  QStackedWidget *m_thumbStack;
  RoundedImageWidget *m_audioArt; // audio/YouTube thumbnail — bo góc thật
  VideoThumbnail *m_videoThumb;   // video playback — crop center, bo góc

  // ── Info & controls ───────────────────────────────────────────────────
  QLabel *m_titleLabel;
  QLabel *m_artistLabel;
  QLabel *m_timeLabel;
  QSlider *m_seekSlider;
  QPushButton *m_prevBtn;
  QPushButton *m_playPauseBtn;
  QPushButton *m_nextBtn;
  QPushButton *m_muteBtn;
  QSlider *m_volumeSlider;

  bool m_seeking = false;
};
