#include "VideoThumbnail.h"

#include <QGraphicsScene>
#include <QResizeEvent>
#include <QPainter>
#include <QPainterPath>

VideoThumbnail::VideoThumbnail(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_videoItem(new QGraphicsVideoItem)
{
    setScene(m_scene);
    m_scene->addItem(m_videoItem);

    // Tắt scrollbar, background, border mặc định của QGraphicsView
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet("background: transparent; border: none;");

    // Render chất lượng cao
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);

    // Clip nội dung theo viewport (crop phần thừa)
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // Khi video thay đổi kích thước native → recalculate scale
    connect(m_videoItem, &QGraphicsVideoItem::nativeSizeChanged,
            this, &VideoThumbnail::updateVideoGeometry);
}

void VideoThumbnail::setMediaPlayer(QMediaPlayer *player)
{
    player->setVideoOutput(m_videoItem);
}

void VideoThumbnail::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateVideoGeometry();
}

void VideoThumbnail::updateVideoGeometry()
{
    const QSizeF native = m_videoItem->nativeSize();
    const QSize  view   = viewport()->size();

    if (native.isEmpty() || view.isEmpty()) return;

    // Scale để cover: chọn scale lớn hơn giữa width và height
    const double scaleW = view.width()  / native.width();
    const double scaleH = view.height() / native.height();
    const double scale  = qMax(scaleW, scaleH);   // cover, không letterbox

    const double scaledW = native.width()  * scale;
    const double scaledH = native.height() * scale;

    // Đặt kích thước video item
    m_videoItem->setSize(QSizeF(scaledW, scaledH));

    // Căn giữa (crop đều 2 bên)
    m_videoItem->setPos(
        (view.width()  - scaledW) / 2.0,
        (view.height() - scaledH) / 2.0
    );

    // Scene rect = viewport để không có scroll
    setSceneRect(0, 0, view.width(), view.height());
}

void VideoThumbnail::paintEvent(QPaintEvent *event)
{
    // Clip theo rounded rect trước khi vẽ video
    QPainter clipPainter(viewport());
    clipPainter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(viewport()->rect(), 10, 10);

    // Áp clip region
    QRegion region(clip.toFillPolygon().toPolygon());
    viewport()->setMask(region);

    QGraphicsView::paintEvent(event);
}
