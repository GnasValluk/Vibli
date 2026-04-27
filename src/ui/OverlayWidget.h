#pragma once

#include <QWidget>

/**
 * @brief OverlayWidget – frameless, always-on-top window with transparent
 *        background, used as a screen overlay.
 *
 * Features:
 *  - No title bar, no border
 *  - Click-through on transparent areas (only receives events on content areas)
 *  - Draggable to reposition
 *  - Show/hide animation support
 */
class OverlayWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal opacity READ windowOpacity WRITE setWindowOpacity)

public:
  explicit OverlayWidget(QWidget *parent = nullptr);
  ~OverlayWidget() override = default;

  void showOverlay();
  void hideOverlay();
  void toggleOverlay();

  // Anchor position (screen corner)
  enum class AnchorPosition {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
  };
  void setAnchorPosition(AnchorPosition anchor, const QPoint &offset = {});

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  void applyWindowFlags();
  void snapToAnchor();

  bool m_dragging = false;
  QPoint m_dragOffset;
  AnchorPosition m_anchor = AnchorPosition::BottomRight;
  QPoint m_anchorOffset = {20, 20};
};
