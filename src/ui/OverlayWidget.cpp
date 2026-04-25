#include "OverlayWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>

OverlayWidget::OverlayWidget(QWidget *parent)
    : QWidget(parent)
{
    applyWindowFlags();
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowOpacity(0.0);
}

void OverlayWidget::applyWindowFlags()
{
    setWindowFlags(
        Qt::FramelessWindowHint       |  // Không có khung cửa sổ
        Qt::WindowStaysOnTopHint      |  // Luôn nằm trên cùng
        Qt::Tool                      |  // Không hiện trên taskbar
        Qt::NoDropShadowWindowHint       // Không có bóng đổ hệ thống
    );
}

// ── Show / Hide ───────────────────────────────────────────────────────────────

void OverlayWidget::showOverlay()
{
    snapToAnchor();
    show();

    auto *anim = new QPropertyAnimation(this, "opacity", this);
    anim->setDuration(250);
    anim->setStartValue(windowOpacity());
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void OverlayWidget::hideOverlay()
{
    auto *anim = new QPropertyAnimation(this, "opacity", this);
    anim->setDuration(200);
    anim->setStartValue(windowOpacity());
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void OverlayWidget::toggleOverlay()
{
    isVisible() ? hideOverlay() : showOverlay();
}

void OverlayWidget::setAnchorPosition(AnchorPosition anchor, const QPoint &offset)
{
    m_anchor       = anchor;
    m_anchorOffset = offset;
    if (isVisible()) snapToAnchor();
}

// ── Snap to screen corner ─────────────────────────────────────────────────────

void OverlayWidget::snapToAnchor()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect geo = screen->availableGeometry();
    QPoint pos;

    switch (m_anchor) {
    case AnchorPosition::TopLeft:
        pos = geo.topLeft() + m_anchorOffset;
        break;
    case AnchorPosition::TopRight:
        pos = geo.topRight() - QPoint(width(), 0)
              + QPoint(-m_anchorOffset.x(), m_anchorOffset.y());
        break;
    case AnchorPosition::BottomLeft:
        pos = geo.bottomLeft() - QPoint(0, height())
              + QPoint(m_anchorOffset.x(), -m_anchorOffset.y());
        break;
    case AnchorPosition::BottomRight:
        pos = geo.bottomRight() - QPoint(width(), height())
              + QPoint(-m_anchorOffset.x(), -m_anchorOffset.y());
        break;
    case AnchorPosition::Center:
        pos = geo.center() - QPoint(width() / 2, height() / 2);
        break;
    }

    move(pos);
}

// ── Drag to move ──────────────────────────────────────────────────────────────

void OverlayWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging   = true;
        m_dragOffset = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void OverlayWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging)
        move(event->globalPosition().toPoint() - m_dragOffset);
    QWidget::mouseMoveEvent(event);
}

void OverlayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    // Nền trong suốt – các widget con tự vẽ nội dung của mình.
    // Nếu muốn vẽ nền mờ cho toàn overlay, bỏ comment đoạn dưới:
    /*
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    painter.fillPath(path, QColor(20, 20, 20, 180));
    */
}

void OverlayWidget::enterEvent(QEnterEvent *event)
{
    // Tăng opacity khi hover
    auto *anim = new QPropertyAnimation(this, "opacity", this);
    anim->setDuration(150);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    QWidget::enterEvent(event);
}

void OverlayWidget::leaveEvent(QEvent *event)
{
    // Giảm opacity khi rời chuột
    auto *anim = new QPropertyAnimation(this, "opacity", this);
    anim->setDuration(300);
    anim->setEndValue(0.85);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    QWidget::leaveEvent(event);
}
