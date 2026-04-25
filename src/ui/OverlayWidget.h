#pragma once

#include <QWidget>

/**
 * @brief OverlayWidget – cửa sổ frameless, luôn nằm trên cùng (always-on-top),
 *        trong suốt nền, dùng làm lớp phủ đè lên màn hình.
 *
 * Tính năng:
 *  - Không có title bar, không có border
 *  - Click-through vùng trong suốt (chỉ nhận sự kiện ở vùng có nội dung)
 *  - Có thể kéo để di chuyển
 *  - Hỗ trợ animation hiện/ẩn
 */
class OverlayWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ windowOpacity WRITE setWindowOpacity)

public:
    explicit OverlayWidget(QWidget *parent = nullptr);
    ~OverlayWidget() override = default;

    void showOverlay();
    void hideOverlay();
    void toggleOverlay();

    // Vị trí neo (góc màn hình)
    enum class AnchorPosition { TopLeft, TopRight, BottomLeft, BottomRight, Center };
    void setAnchorPosition(AnchorPosition anchor, const QPoint &offset = {});

protected:
    void mousePressEvent(QMouseEvent *event)   override;
    void mouseMoveEvent(QMouseEvent *event)    override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event)        override;
    void enterEvent(QEnterEvent *event)        override;
    void leaveEvent(QEvent *event)             override;

private:
    void applyWindowFlags();
    void snapToAnchor();

    bool   m_dragging     = false;
    QPoint m_dragOffset;
    AnchorPosition m_anchor = AnchorPosition::BottomRight;
    QPoint m_anchorOffset   = {20, 20};
};
