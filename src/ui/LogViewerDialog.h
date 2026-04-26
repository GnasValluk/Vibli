#pragma once

#include "../core/LogService.h"
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

/**
 * @brief LogViewerDialog – xem log trực tiếp trong app.
 *
 * - Hiển thị log có màu theo level (xanh/vàng/đỏ)
 * - Tự cuộn xuống dòng mới nhất
 * - Nút Copy (copy toàn bộ) và Export (lưu file)
 * - Realtime: nhận log mới qua signal LogService::logged
 */
class LogViewerDialog : public QDialog {
  Q_OBJECT
public:
  explicit LogViewerDialog(QWidget *parent = nullptr);

private slots:
  void onNewLog(LogService::Level level, const QString &component,
                const QString &message, const QString &timestamp);
  void onCopy();
  void onExport();

private:
  void loadExistingLog();
  void appendLine(LogService::Level level, const QString &component,
                  const QString &message, const QString &timestamp);
  void updateCountLabel();

  QTextEdit *m_logView;
  QPushButton *m_copyBtn;
  QPushButton *m_exportBtn;
  QPushButton *m_clearBtn;
  QLabel *m_countLabel;
  int m_errorCount = 0;
};
