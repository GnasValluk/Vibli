#pragma once

#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QMediaPlayer>

/**
 * @brief VideoThumbnail – hiển thị video dạng thumbnail vuông.
 *
 * - Giữ nguyên tỉ lệ khung hình (không bóp méo)
 * - Scale để cover toàn bộ ô vuông (crop phần thừa ở giữa)
 * - Bo góc + border đẹp
 */
class VideoThumbnail : public QGraphicsView
{
    Q_OBJECT

public:
    explicit VideoThumbnail(QWidget *parent = nullptr);

    // Gắn vào QMediaPlayer để nhận video frames
    void setMediaPlayer(QMediaPlayer *player);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateVideoGeometry();

    QGraphicsScene    *m_scene;
    QGraphicsVideoItem *m_videoItem;
};
