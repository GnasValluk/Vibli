#pragma once

#include <QPainter>
#include <QSlider>
#include <QStyleOptionSlider>


/**
 * @brief Custom QSlider with buffer progress overlay
 *
 * Displays a semi-transparent gray overlay showing how much of the audio
 * has been buffered/downloaded. Used for YouTube progressive streaming.
 */
class BufferedSeekSlider : public QSlider {
  Q_OBJECT

public:
  explicit BufferedSeekSlider(Qt::Orientation orientation,
                              QWidget *parent = nullptr);

  /**
   * Set buffer progress (0.0 - 1.0)
   * @param progress Percentage of file buffered (0.0 = 0%, 1.0 = 100%)
   */
  void setBufferProgress(float progress);

  /**
   * Get current buffer progress
   */
  float bufferProgress() const { return m_bufferProgress; }

protected:
  void paintEvent(QPaintEvent *event) override;
  bool event(QEvent *event) override;

private:
  float m_bufferProgress = 0.0f;
};
