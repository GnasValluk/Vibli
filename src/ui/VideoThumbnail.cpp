#include "VideoThumbnail.h"

#include <QGraphicsScene>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

VideoThumbnail::VideoThumbnail(QWidget *parent)
    : QGraphicsView(parent), m_scene(new QGraphicsScene(this)),
      m_videoItem(new QGraphicsVideoItem) {
  setScene(m_scene);
  m_scene->addItem(m_videoItem);

  // Disable scrollbars, background, and default QGraphicsView border
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFrameShape(QFrame::NoFrame);
  setStyleSheet("background: transparent; border: none;");

  // High quality rendering
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform);

  // Clip content to viewport (crop excess)
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  // Recalculate scale when native video size changes
  connect(m_videoItem, &QGraphicsVideoItem::nativeSizeChanged, this,
          &VideoThumbnail::updateVideoGeometry);
}

void VideoThumbnail::setMediaPlayer(QMediaPlayer *player) {
  player->setVideoOutput(m_videoItem);
}

void VideoThumbnail::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  updateVideoGeometry();
}

void VideoThumbnail::updateVideoGeometry() {
  const QSizeF native = m_videoItem->nativeSize();
  const QSize view = viewport()->size();

  if (native.isEmpty() || view.isEmpty())
    return;

  // Scale to cover: choose the larger of width and height scale factors
  const double scaleW = view.width() / native.width();
  const double scaleH = view.height() / native.height();
  const double scale = qMax(scaleW, scaleH); // cover, no letterbox

  const double scaledW = native.width() * scale;
  const double scaledH = native.height() * scale;

  // Set video item size
  m_videoItem->setSize(QSizeF(scaledW, scaledH));

  // Center (crop evenly on both sides)
  m_videoItem->setPos((view.width() - scaledW) / 2.0,
                      (view.height() - scaledH) / 2.0);

  // Scene rect = viewport to prevent scrolling
  setSceneRect(0, 0, view.width(), view.height());
}

void VideoThumbnail::paintEvent(QPaintEvent *event) {
  // Clip to rounded rect before drawing video
  QPainter clipPainter(viewport());
  clipPainter.setRenderHint(QPainter::Antialiasing);
  QPainterPath clip;
  clip.addRoundedRect(viewport()->rect(), 10, 10);

  // Apply clip region
  QRegion region(clip.toFillPolygon().toPolygon());
  viewport()->setMask(region);

  QGraphicsView::paintEvent(event);
}
