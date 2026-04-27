#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

/**
 * @brief LoadingOverlay – thin iOS-style progress bar at the top of the parent
 * widget.
 *
 * - When total = -1: indeterminate (shimmer runs back and forth)
 * - When total > 0:  determinate (fills by percentage)
 * - Auto-hides with fade-out when stop() is called
 */
class LoadingOverlay : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal shimmerPos READ shimmerPos WRITE setShimmerPos)
  Q_PROPERTY(qreal opacity READ barOpacity WRITE setBarOpacity)

public:
  explicit LoadingOverlay(QWidget *parent = nullptr);

  void start(const QString &message = "");
  void stop();
  void setProgress(int loaded, int total); // total=-1 → indeterminate
  void setMessage(const QString &msg);

  qreal shimmerPos() const { return m_shimmerPos; }
  void setShimmerPos(qreal v) {
    m_shimmerPos = v;
    update();
  }

  qreal barOpacity() const { return m_opacity; }
  void setBarOpacity(qreal v) {
    m_opacity = v;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

private:
  static constexpr int BAR_H = 3; // bar height (px)

  QTimer *m_shimmerTimer;
  QPropertyAnimation *m_fadeAnim;
  QLabel *m_label;

  qreal m_shimmerPos = 0.0; // 0.0 → 1.0, shimmer position
  qreal m_opacity = 1.0;
  int m_loaded = 0;
  int m_total = -1; // -1 = indeterminate
  bool m_running = false;
};
