#pragma once

#include "../core/LogService.h"
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

class LogViewerDialog : public QDialog {
  Q_OBJECT
public:
  explicit LogViewerDialog(QWidget *parent = nullptr);
  ~LogViewerDialog() override;

private slots:
  void onNewLog(LogService::Level level, const QString &component,
                const QString &message, const QString &timestamp);
  void onCopy();
  void onExport();
  void onClear();

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
