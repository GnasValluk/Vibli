#pragma once
#include <QCoreApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <qdebug.h>

/**
 * @brief IconFont – Material Symbols Rounded dùng codepoint trực tiếp.
 *
 * Codepoint lấy từ cmap của font file (Private Use Area).
 * Render thành QIcon qua QPainter để dùng cho QPushButton, QListWidget.
 */
namespace IconFont {

// ── Codepoints (đọc từ font cmap) ────────────────────────────────────────────
constexpr char32_t PLAY_ARROW = 0x0E037;
constexpr char32_t PAUSE = 0x0E034;
constexpr char32_t SKIP_NEXT = 0x0E044;
constexpr char32_t SKIP_PREVIOUS = 0x0E045;
constexpr char32_t VOLUME_UP = 0x0E050;
constexpr char32_t VOLUME_OFF = 0x0E04F;
constexpr char32_t FOLDER_OPEN = 0x0E2C8;
constexpr char32_t DELETE_ICON = 0x0E872;
constexpr char32_t CLEAR_ALL = 0x0E0B8;
constexpr char32_t MUSIC_NOTE = 0x0E405;
constexpr char32_t MOVIE = 0x0E02C;
constexpr char32_t SHUFFLE = 0x0E043;
constexpr char32_t REPEAT = 0x0E040;
constexpr char32_t REPEAT_ONE = 0x0E041;
constexpr char32_t SETTINGS = 0x0E8B8;
constexpr char32_t CLOSE = 0x0E5CD;
constexpr char32_t QUEUE_MUSIC = 0x0E03D;
constexpr char32_t DOWNLOAD = 0x0E2C4;
constexpr char32_t DESCRIPTION = 0x0E873;

inline void init() {
  // Ưu tiên load từ Qt resource (embedded) — đảm bảo hoạt động cả debug lẫn
  // release
  int id =
      QFontDatabase::addApplicationFont(":/fonts/MaterialSymbolsRounded.ttf");
  if (id >= 0) {
    return; // loaded from resource
  }
  // Fallback: load từ file cạnh exe (legacy / dev build)
  const QString fontPath =
      QCoreApplication::applicationDirPath() + "/MaterialSymbolsRounded.ttf";
  id = QFontDatabase::addApplicationFont(fontPath);
  if (id < 0) {
    qWarning("IconFont: failed to load font from resource AND from %s",
             qPrintable(fontPath));
  }
}

inline QFont rawFont(int pixelSize = 20) {
  QFont f("Material Symbols Rounded");
  f.setPixelSize(pixelSize);
  return f;
}

// Chuyển codepoint → QString
inline QString cp(char32_t codepoint) {
  return QString::fromUcs4(&codepoint, 1);
}

// Render codepoint thành QIcon với nền trong suốt
inline QIcon icon(char32_t codepoint, int size = 20,
                  QColor color = QColor(204, 204, 204)) {
  const int dpr = 2;
  QPixmap pm(size * dpr, size * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.setPen(color);
  p.setFont(rawFont(size));
  p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, cp(codepoint));
  p.end();

  return QIcon(pm);
}

// QIcon với 2 trạng thái normal / active (hover)
inline QIcon iconDual(char32_t codepoint, int size = 20,
                      QColor normal = QColor(204, 204, 204),
                      QColor active = QColor(255, 255, 255)) {
  QIcon ic;
  ic.addPixmap(icon(codepoint, size, normal).pixmap(size, size), QIcon::Normal);
  ic.addPixmap(icon(codepoint, size, active).pixmap(size, size), QIcon::Active);
  return ic;
}

} // namespace IconFont
