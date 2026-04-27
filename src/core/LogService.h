#pragma once
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QStandardPaths>
#include <QTextStream>

/**
 * @brief LogService – structured logger with automatic deduplication.
 *
 * Format:
 *   [2026-04-26 14:32:01.123] ✅ INFO    | AudioPlayer | Started playing:
 * song.mp3 [2026-04-26 14:32:05.456] ⚠️  WARN   | YtDlpService | Stream URL
 * expired [2026-04-26 14:32:06.789] ❌ ERROR   | AudioPlayer | Demuxing failed
 * (×47 times in 2s)
 *
 * Dedup: same component + message within DEDUP_WINDOW_MS → merged into one
 * line.
 */
class LogService : public QObject {
  Q_OBJECT

public:
  enum class Level { Debug, Info, Warn, Error };

  static LogService &instance() {
    static LogService s;
    return s;
  }

  static QString logFilePath() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/vibli.log";
  }

  void log(Level level, const QString &component, const QString &message) {
    QMutexLocker lock(&m_mutex);

    const QString key = component + "|" + message;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // ── Dedup: same key within DEDUP_WINDOW_MS → count, don't write yet ──
    if (key == m_lastKey && (now - m_lastTime) < DEDUP_WINDOW_MS) {
      m_dedupCount++;
      return;
    }

    // Flush dedup if any
    if (m_dedupCount > 1) {
      const QString summary = QString("  └─ (repeated ×%1 times in ~%2s)")
                                  .arg(m_dedupCount)
                                  .arg((now - m_lastTime) / 1000.0, 0, 'f', 1);
      if (m_file.isOpen()) {
        m_stream << summary << "\n";
        m_stream.flush();
      }
    }

    // Ghi dòng mới
    const QString ts =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString line = QString("[%1] %2 | %3 | %4\n")
                             .arg(ts, levelStr(level), component, message);
    if (m_file.isOpen()) {
      m_stream << line;
      m_stream.flush();
    }

    // Cập nhật dedup state
    m_lastKey = key;
    m_lastTime = now;
    m_dedupCount = 1;

    // Emit cho UI
    emit logged(level, component, message, ts);

    // Emit hasError if error level
    if (level == Level::Error || level == Level::Warn)
      emit errorLogged();
  }

  void debug(const QString &c, const QString &m) { log(Level::Debug, c, m); }
  void info(const QString &c, const QString &m) { log(Level::Info, c, m); }
  void warn(const QString &c, const QString &m) { log(Level::Warn, c, m); }
  void error(const QString &c, const QString &m) { log(Level::Error, c, m); }

  QString readAll() {
    QFile f(logFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
      return {};
    return QString::fromUtf8(f.readAll());
  }

  /** Clear all log file content on disk and reset dedup state. */
  void clearLog() {
    QMutexLocker lock(&m_mutex);

    // Close file, truncate via resize(0), reopen
    m_file.close();
    m_file.resize(0); // truncate without open — avoids nodiscard warning

    // Reopen in Append mode to continue writing
    if (m_file.open(QIODevice::Append | QIODevice::Text)) {
      m_stream.setDevice(&m_file);
      m_stream.setEncoding(QStringConverter::Utf8);
      const QString sep = QString("=").repeated(70);
      m_stream << "\n"
               << sep << "\n"
               << QString("  Log cleared at %1\n")
                      .arg(QDateTime::currentDateTime().toString(
                          "yyyy-MM-dd HH:mm:ss"))
               << sep << "\n";
      m_stream.flush();
    }

    // Reset dedup state
    m_lastKey.clear();
    m_lastTime = 0;
    m_dedupCount = 0;

    emit logCleared();
  }

signals:
  void logged(LogService::Level level, const QString &component,
              const QString &message, const QString &timestamp);
  void errorLogged(); // emitted on warn/error → UI shows red dot
  void logCleared();  // emitted when log file is cleared

private:
  static constexpr qint64 DEDUP_WINDOW_MS = 5000; // 5 seconds

  LogService() {
    m_file.setFileName(logFilePath());
    if (m_file.open(QIODevice::Append | QIODevice::Text)) {
      m_stream.setDevice(&m_file);
      m_stream.setEncoding(QStringConverter::Utf8);
      const QString sep = QString("=").repeated(70);
      m_stream << "\n"
               << sep << "\n"
               << QString("  VIBLI started at %1\n")
                      .arg(QDateTime::currentDateTime().toString(
                          "yyyy-MM-dd HH:mm:ss"))
               << sep << "\n";
      m_stream.flush();
    }
  }

  static QString levelStr(Level l) {
    switch (l) {
    case Level::Debug:
      return "🔍 DEBUG  ";
    case Level::Info:
      return "✅ INFO   ";
    case Level::Warn:
      return "⚠️  WARN   ";
    case Level::Error:
      return "❌ ERROR  ";
    }
    return "   INFO   ";
  }

  QFile m_file;
  QTextStream m_stream;
  QMutex m_mutex;

  // Dedup state
  QString m_lastKey;
  qint64 m_lastTime = 0;
  int m_dedupCount = 0;
};

#define VLOG_DEBUG(comp, msg) LogService::instance().debug(comp, msg)
#define VLOG_INFO(comp, msg) LogService::instance().info(comp, msg)
#define VLOG_WARN(comp, msg) LogService::instance().warn(comp, msg)
#define VLOG_ERROR(comp, msg) LogService::instance().error(comp, msg)
