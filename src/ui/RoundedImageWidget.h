#pragma once
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

/**
 * @brief RoundedImageWidget – displays a QPixmap with true rounded corners.
 *
 * Caches the scaled pixmap — only rescales when size changes,
 * avoiding scaled() calls on every paintEvent.
 */
class RoundedImageWidget : public QWidget {
  Q_OBJECT
public:
  explicit RoundedImageWidget(int radius = 10, QWidget *parent = nullptr)
      : QWidget(parent), m_radius(radius) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
  }

  void setPixmap(const QPixmap &px) {
    m_pixmap = px;
    m_scaledCache = QPixmap(); // invalidate cache
    update();
  }
  void setPlaceholderText(const QString &text) {
    m_placeholder = text;
    update();
  }

  void clearPixmap() {
    m_pixmap = QPixmap();
    m_scaledCache = QPixmap();
    update();
  }

protected:
  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    m_scaledCache = QPixmap(); // invalidate on resize
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath clip;
    clip.addRoundedRect(r, m_radius, m_radius);
    p.setClipPath(clip);

    if (!m_pixmap.isNull()) {
      // Rebuild scaled cache only when needed
      if (m_scaledCache.isNull() || m_scaledCache.size() != size()) {
        m_scaledCache = m_pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
      }
      const int x = (m_scaledCache.width() - width()) / 2;
      const int y = (m_scaledCache.height() - height()) / 2;
      p.drawPixmap(0, 0, m_scaledCache, x, y, width(), height());
    } else {
      p.fillPath(clip, QColor(42, 42, 42));
      if (!m_placeholder.isEmpty()) {
        p.setPen(QColor(100, 100, 100));
        p.setFont(QFont(font().family(), 20));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
      }
    }

    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.drawRoundedRect(r, m_radius, m_radius);
  }

private:
  QPixmap m_pixmap;
  mutable QPixmap m_scaledCache; // cached scaled version
  QString m_placeholder = "♪";
  int m_radius;
};
