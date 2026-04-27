#pragma once

#include <QGraphicsVideoItem>
#include <QGraphicsView>
#include <QMediaPlayer>


/**
 * @brief VideoThumbnail – displays video as a square thumbnail.
 *
 * - Preserves aspect ratio (no stretching)
 * - Scales to cover the entire square (crops excess from center)
 * - Rounded corners + clean border
 */
class VideoThumbnail : public QGraphicsView {
  Q_OBJECT

public:
  explicit VideoThumbnail(QWidget *parent = nullptr);

  // Attach to QMediaPlayer to receive video frames
  void setMediaPlayer(QMediaPlayer *player);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void updateVideoGeometry();

  QGraphicsScene *m_scene;
  QGraphicsVideoItem *m_videoItem;
};
