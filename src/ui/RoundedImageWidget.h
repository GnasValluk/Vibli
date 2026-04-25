#pragma once
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>


/**
 * @brief RoundedImageWidget – hiển thị QPixmap với bo góc thật sự.
 *
 * QLabel + border-radius không clip được ảnh.
 * Widget này vẽ trực tiếp bằng QPainter với QPainterPath clip.
 *
 * Khi không có ảnh: hiện placeholder tối với icon ký tự ở giữa.
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
    update();
  }

  void setPlaceholderText(const QString &text) {
    m_placeholder = text;
    update();
  }

  void clearPixmap() {
    m_pixmap = QPixmap();
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // Clip path bo góc
    QPainterPath clip;
    clip.addRoundedRect(r, m_radius, m_radius);
    p.setClipPath(clip);

    if (!m_pixmap.isNull()) {
      // Scale cover + crop center
      const QSize s = size();
      QPixmap scaled = m_pixmap.scaled(s, Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
      const int x = (scaled.width() - s.width()) / 2;
      const int y = (scaled.height() - s.height()) / 2;
      p.drawPixmap(0, 0, scaled, x, y, s.width(), s.height());
    } else {
      // Placeholder tối
      p.fillPath(clip, QColor(42, 42, 42));
      if (!m_placeholder.isEmpty()) {
        p.setPen(QColor(100, 100, 100));
        p.setFont(QFont(font().family(), 20));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
      }
    }

    // Border mỏng
    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.drawRoundedRect(r, m_radius, m_radius);
  }

private:
  QPixmap m_pixmap;
  QString m_placeholder = "♪";
  int m_radius;
};
