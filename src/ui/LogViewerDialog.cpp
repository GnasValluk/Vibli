#include "LogViewerDialog.h"
#include "IconFont.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

LogViewerDialog::LogViewerDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("VIBLI – Log");
  setMinimumSize(700, 480);
  resize(800, 520);

  // ── Log view ──────────────────────────────────────────────────────────
  m_logView = new QTextEdit(this);
  m_logView->setReadOnly(true);
  m_logView->setObjectName("logView");
  m_logView->document()->setMaximumBlockCount(2000); // limit to 2000 lines

  // ── Toolbar ───────────────────────────────────────────────────────────
  m_countLabel = new QLabel("0 errors", this);
  m_countLabel->setObjectName("countLabel");

  m_copyBtn = new QPushButton("  Copy", this);
  m_copyBtn->setObjectName("toolBtn");
  m_copyBtn->setIcon(
      IconFont::icon(IconFont::DESCRIPTION, 14, QColor(180, 180, 180)));
  m_copyBtn->setIconSize({14, 14});
  m_copyBtn->setToolTip("Copy all log to clipboard");

  m_exportBtn = new QPushButton("  Save", this);
  m_exportBtn->setObjectName("toolBtn");
  m_exportBtn->setIcon(
      IconFont::icon(IconFont::DOWNLOAD, 14, QColor(180, 180, 180)));
  m_exportBtn->setIconSize({14, 14});
  m_exportBtn->setToolTip("Save log to file for bug reports");

  m_clearBtn = new QPushButton("  Clear", this);
  m_clearBtn->setObjectName("toolBtn");
  m_clearBtn->setIcon(
      IconFont::icon(IconFont::CLEAR_ALL, 14, QColor(180, 180, 180)));
  m_clearBtn->setIconSize({14, 14});
  m_clearBtn->setToolTip("Clear displayed log (does not delete file)");

  auto *toolbar = new QHBoxLayout;
  toolbar->setSpacing(6);
  toolbar->addWidget(m_countLabel);
  toolbar->addStretch();
  toolbar->addWidget(m_copyBtn);
  toolbar->addWidget(m_exportBtn);
  toolbar->addWidget(m_clearBtn);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(8);
  layout->addLayout(toolbar);
  layout->addWidget(m_logView, 1);

  // ── Style ─────────────────────────────────────────────────────────────
  setStyleSheet(R"(
        QDialog {
            background: #0d0d0d;
            color: #e0e0e0;
        }
        QTextEdit#logView {
            background: #0a0a0a;
            color: #cccccc;
            border: 1px solid #222222;
            border-radius: 6px;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
            selection-background-color: #1db954;
        }
        QPushButton#toolBtn {
            background: #1e1e1e;
            color: #cccccc;
            border: 1px solid #333333;
            border-radius: 5px;
            padding: 4px 12px;
            font-size: 12px;
        }
        QPushButton#toolBtn:hover {
            background: #2a2a2a;
            border-color: #1db954;
        }
        QLabel#countLabel {
            color: #666666;
            font-size: 11px;
        }
    )");

  // ── Connections ───────────────────────────────────────────────────────
  // Lazy: only receive realtime log when dialog is open
  connect(&LogService::instance(), &LogService::logged, this,
          &LogViewerDialog::onNewLog);
  connect(m_copyBtn, &QPushButton::clicked, this, &LogViewerDialog::onCopy);
  connect(m_exportBtn, &QPushButton::clicked, this, &LogViewerDialog::onExport);
  connect(m_clearBtn, &QPushButton::clicked, this, &LogViewerDialog::onClear);
  // Load log hiện có
  loadExistingLog();
}

LogViewerDialog::~LogViewerDialog() {
  // Disconnect on close to avoid keeping QTextEdit DOM in memory
  disconnect(&LogService::instance(), &LogService::logged, this,
             &LogViewerDialog::onNewLog);
}

// ── Load log file hiện có
// ─────────────────────────────────────────────────────

void LogViewerDialog::loadExistingLog() {
  QFile f(LogService::logFilePath());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&f);
  in.setEncoding(QStringConverter::Utf8);

  while (!in.atEnd()) {
    const QString line = in.readLine();
    if (line.isEmpty())
      continue;

    // Parse level from emoji in the line
    LogService::Level level = LogService::Level::Info;
    if (line.contains("❌ ERROR")) {
      level = LogService::Level::Error;
      m_errorCount++;
    } else if (line.contains("⚠️")) {
      level = LogService::Level::Warn;
    } else if (line.contains("🔍")) {
      level = LogService::Level::Debug;
    }

    // Display with corresponding color
    QString color;
    switch (level) {
    case LogService::Level::Error:
      color = "#ff6b6b";
      break;
    case LogService::Level::Warn:
      color = "#ffd93d";
      break;
    case LogService::Level::Debug:
      color = "#666666";
      break;
    default:
      color = "#aaaaaa";
      break;
    }

    m_logView->append(
        QString("<span style='color:%1;white-space:pre;'>%2</span>")
            .arg(color, line.toHtmlEscaped()));
  }

  updateCountLabel();

  // Scroll to bottom
  m_logView->verticalScrollBar()->setValue(
      m_logView->verticalScrollBar()->maximum());
}

// ── Nhận log mới realtime
// ─────────────────────────────────────────────────────

void LogViewerDialog::onNewLog(LogService::Level level,
                               const QString &component, const QString &message,
                               const QString &timestamp) {
  appendLine(level, component, message, timestamp);
}

void LogViewerDialog::appendLine(LogService::Level level,
                                 const QString &component,
                                 const QString &message,
                                 const QString &timestamp) {
  QString color, levelStr;
  switch (level) {
  case LogService::Level::Error:
    color = "#ff6b6b";
    levelStr = "❌ ERROR  ";
    m_errorCount++;
    updateCountLabel();
    break;
  case LogService::Level::Warn:
    color = "#ffd93d";
    levelStr = "⚠️  WARN   ";
    m_errorCount++;
    updateCountLabel();
    break;
  case LogService::Level::Debug:
    color = "#555555";
    levelStr = "🔍 DEBUG  ";
    break;
  default:
    color = "#88cc88";
    levelStr = "✅ INFO   ";
    break;
  }

  const QString line =
      QString("[%1] %2 | %3 | %4").arg(timestamp, levelStr, component, message);

  m_logView->append(QString("<span style='color:%1;white-space:pre;'>%2</span>")
                        .arg(color, line.toHtmlEscaped()));

  // Auto-scroll if at the bottom
  QScrollBar *sb = m_logView->verticalScrollBar();
  if (sb->value() >= sb->maximum() - 20)
    sb->setValue(sb->maximum());
}

void LogViewerDialog::updateCountLabel() {
  if (m_errorCount == 0)
    m_countLabel->setText("No errors");
  else
    m_countLabel->setText(
        QString("<span style='color:#ff6b6b;'>%1 errors/warnings</span>")
            .arg(m_errorCount));
  m_countLabel->setTextFormat(Qt::RichText);
}

// ── Actions
// ───────────────────────────────────────────────────────────────────

void LogViewerDialog::onCopy() {
  QApplication::clipboard()->setText(m_logView->toPlainText());
  m_copyBtn->setText("  Copied!");
  QTimer::singleShot(1500, this, [this]() { m_copyBtn->setText("  Copy"); });
}

void LogViewerDialog::onClear() {
  // Clear log file on disk + reset UI
  LogService::instance().clearLog();
  m_logView->clear();
  m_errorCount = 0;
  updateCountLabel();
}
void LogViewerDialog::onExport() {
  const QString dst = QFileDialog::getSaveFileName(
      this, "Save VIBLI Log", QDir::homePath() + "/vibli_log.txt",
      "Text Files (*.txt);;All Files (*)");
  if (dst.isEmpty())
    return;

  QFile::remove(dst);
  if (QFile::copy(LogService::logFilePath(), dst))
    m_exportBtn->setText("  Saved!");
  else
    m_exportBtn->setText("  Error!");
  QTimer::singleShot(2000, this, [this]() { m_exportBtn->setText("  Save"); });
}
