#include "LoadingOverlay.h"

#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>


static constexpr int SHIMMER_INTERVAL = 16;   // ~60fps
static constexpr qreal SHIMMER_SPEED = 0.012; // tốc độ di chuyển mỗi frame

LoadingOverlay::LoadingOverlay(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAutoFillBackground(false);

  // Chỉ chiếm phần trên cùng
  setFixedHeight(BAR_H + 20); // bar + label

  // Label trạng thái nhỏ bên dưới bar
  m_label = new QLabel(this);
  m_label->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 11px; "
                         "background: transparent;");
  m_label->setAlignment(Qt::AlignCenter);
  m_label->setGeometry(0, BAR_H + 4, width(), 14);

  // Shimmer timer
  m_shimmerTimer = new QTimer(this);
  m_shimmerTimer->setInterval(SHIMMER_INTERVAL);
  connect(m_shimmerTimer, &QTimer::timeout, this, [this]() {
    m_shimmerPos += SHIMMER_SPEED;
    if (m_shimmerPos > 1.3)
      m_shimmerPos = -0.3;
    update();
  });

  // Fade animation
  m_fadeAnim = new QPropertyAnimation(this, "opacity", this);
  m_fadeAnim->setDuration(300);
  connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
    if (!m_running)
      hide();
  });

  hide();
}

void LoadingOverlay::start(const QString &message) {
  m_running = true;
  m_loaded = 0;
  m_total = -1;
  m_shimmerPos = -0.3;
  m_opacity = 1.0;

  if (parentWidget())
    setGeometry(0, 0, parentWidget()->width(), BAR_H + 20);

  m_label->setText(message);
  m_label->setGeometry(0, BAR_H + 4, width(), 14);

  show();
  raise();
  m_shimmerTimer->start();
}

void LoadingOverlay::stop() {
  m_running = false;
  m_shimmerTimer->stop();

  // Fade out
  m_fadeAnim->setStartValue(1.0);
  m_fadeAnim->setEndValue(0.0);
  m_fadeAnim->start();
}

void LoadingOverlay::setProgress(int loaded, int total) {
  m_loaded = loaded;
  m_total = total;
  update();
}

void LoadingOverlay::setMessage(const QString &msg) { m_label->setText(msg); }

void LoadingOverlay::resizeEvent(QResizeEvent *) {
  if (parentWidget())
    setFixedWidth(parentWidget()->width());
  m_label->setGeometry(0, BAR_H + 4, width(), 14);
}

void LoadingOverlay::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setOpacity(m_opacity);

  const QRect barRect(0, 0, width(), BAR_H);

  // ── Track (nền bar) ───────────────────────────────────────────────────
  p.fillRect(barRect, QColor(255, 255, 255, 20));

  if (m_total > 0) {
    // ── Determinate: fill theo % ──────────────────────────────────────
    const int fillW = static_cast<int>(width() * m_loaded / double(m_total));
    QLinearGradient grad(0, 0, fillW, 0);
    grad.setColorAt(0.0, QColor(29, 185, 84)); // #1db954
    grad.setColorAt(1.0, QColor(30, 215, 96)); // #1ed760
    p.fillRect(QRect(0, 0, fillW, BAR_H), grad);

    // Shimmer trên phần đã fill
    if (fillW > 0) {
      const int sx = static_cast<int>((m_shimmerPos - 0.15) * fillW);
      const int sw = static_cast<int>(0.3 * fillW);
      QLinearGradient shimmer(sx, 0, sx + sw, 0);
      shimmer.setColorAt(0.0, Qt::transparent);
      shimmer.setColorAt(0.5, QColor(255, 255, 255, 60));
      shimmer.setColorAt(1.0, Qt::transparent);
      p.fillRect(QRect(sx, 0, sw, BAR_H), shimmer);
    }
  } else {
    // ── Indeterminate: shimmer chạy qua lại ──────────────────────────
    const int sw = width() / 3;
    const int sx = static_cast<int>((m_shimmerPos - 0.15) * width());

    // Phần xanh chạy
    QLinearGradient grad(sx, 0, sx + sw, 0);
    grad.setColorAt(0.0, Qt::transparent);
    grad.setColorAt(0.3, QColor(29, 185, 84, 200));
    grad.setColorAt(0.5, QColor(30, 215, 96, 255));
    grad.setColorAt(0.7, QColor(29, 185, 84, 200));
    grad.setColorAt(1.0, Qt::transparent);
    p.fillRect(barRect, grad);
  }
}
