#pragma once

#include "../core/AudioPlayer.h"
#include "../core/PlaylistManager.h"
#include "../core/ThumbnailCache.h"
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

class PlaylistImporter;

/**
 * @brief MiniPlayer – overlay player.
 */
class MiniPlayer : public OverlayWidget {
  Q_OBJECT

public:
  explicit MiniPlayer(AudioPlayer *player, PlaylistManager *playlist,
                      ThumbnailCache *thumbCache, QWidget *parent = nullptr);
  ~MiniPlayer() override = default;

public slots:
  void updateTrackInfo(const Track &track);
  void updatePlaybackState(bool playing);
  void updatePosition(qint64 posMs);
  void updateDuration(qint64 durMs);
  /** Called when the thumbnail for videoId is ready in ThumbnailCache. */
  void onThumbnailReady(const QString &videoId);

private slots:
  void onPlayPauseClicked();
  void onNextClicked();
  void onPreviousClicked();
  void onSeekSliderMoved(int value);
  void onVolumeSliderMoved(int value);
  void onVideoAvailableChanged(bool available);
  void onShuffleClicked();
  void onRepeatClicked();

private:
  void setupUi();
  void applyStyle();
  void paintEvent(QPaintEvent *event) override;
  QString formatTime(qint64 ms) const;

  AudioPlayer *m_player;
  PlaylistManager *m_playlist;
  ThumbnailCache *m_thumbCache;

  // ── Thumbnail area ────────────────────────────────────────────────────
  QStackedWidget *m_thumbStack;
  RoundedImageWidget *m_audioArt;
  VideoThumbnail *m_videoThumb;
  // ── Info & controls ───────────────────────────────────────────────────
  QLabel *m_titleLabel;
  QLabel *m_artistLabel;
  QLabel *m_timeLabel;
  QSlider *m_seekSlider;
  QPushButton *m_prevBtn;
  QPushButton *m_playPauseBtn;
  QPushButton *m_nextBtn;
  QPushButton *m_muteBtn;
  QPushButton *m_shuffleBtn;
  QPushButton *m_repeatBtn;
  QSlider *m_volumeSlider;

  bool m_seeking = false;
  QString m_currentVideoId; // videoId of the currently displayed track
};
