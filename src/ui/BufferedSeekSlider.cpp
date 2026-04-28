#include "BufferedSeekSlider.h"
#include <QHelpEvent>
#include <QStyleOptionSlider>
#include <QToolTip>


BufferedSeekSlider::BufferedSeekSlider(Qt::Orientation orientation,
                                       QWidget *parent)
    : QSlider(orientation, parent), m_bufferProgress(0.0f) {}

void BufferedSeekSlider::setBufferProgress(float progress) {
  m_bufferProgress = qBound(0.0f, progress, 1.0f);
  update(); // Trigger repaint
}

void BufferedSeekSlider::paintEvent(QPaintEvent *event) {
  // Draw base slider first
  QSlider::paintEvent(event);

  // Only draw buffer overlay if there's progress
  if (m_bufferProgress <= 0.0f) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Get groove rectangle
  QStyleOptionSlider opt;
  initStyleOption(&opt);
  QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt,
                                             QStyle::SC_SliderGroove, this);

  // Calculate buffer overlay width
  int bufferWidth = 0;
  if (orientation() == Qt::Horizontal) {
    bufferWidth = static_cast<int>(grooveRect.width() * m_bufferProgress);

    // Draw semi-transparent gray rectangle for buffered region
    QRect bufferRect = grooveRect;
    bufferRect.setWidth(bufferWidth);
    painter.fillRect(bufferRect, QColor(80, 80, 80, 120));
  } else {
    // Vertical orientation
    int bufferHeight = static_cast<int>(grooveRect.height() * m_bufferProgress);
    QRect bufferRect = grooveRect;
    bufferRect.setTop(grooveRect.bottom() - bufferHeight);
    painter.fillRect(bufferRect, QColor(80, 80, 80, 120));
  }
}

bool BufferedSeekSlider::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip && m_bufferProgress > 0.0f) {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

    // Calculate buffered time
    qint64 totalDurationMs = maximum();
    qint64 bufferedMs = static_cast<qint64>(totalDurationMs * m_bufferProgress);

    // Format time as MM:SS
    int bufferedSec = bufferedMs / 1000;
    int bufferedMin = bufferedSec / 60;
    bufferedSec = bufferedSec % 60;

    QString tooltip = QString("Buffered: 0:00 - %1:%2")
                          .arg(bufferedMin)
                          .arg(bufferedSec, 2, 10, QChar('0'));

    QToolTip::showText(helpEvent->globalPos(), tooltip);
    return true;
  }

  return QSlider::event(event);
}
