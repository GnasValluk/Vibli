#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <optional>


#include "PlaylistManager.h"

/**
 * @brief YtDlpService – giao tiếp với yt-dlp.exe qua QProcess.
 *
 * Chịu trách nhiệm duy nhất: gọi yt-dlp.exe và parse kết quả.
 * Dùng hai QProcess riêng biệt (metadata và stream) để tránh xung đột.
 */
class YtDlpService : public QObject {
  Q_OBJECT

public:
  explicit YtDlpService(QObject *parent = nullptr);
  ~YtDlpService() override = default;

  // Kiểm tra yt-dlp.exe có tồn tại không
  static bool isAvailable();

  // Lấy metadata toàn bộ playlist (async)
  void fetchPlaylistMetadata(const QString &playlistUrl);

  // Lấy stream URL cho một video (async, lazy, có cache)
  void resolveStreamUrl(const QString &videoId);

  // Xóa cache stream URL của một video (dùng khi retry)
  void invalidateStreamCache(const QString &videoId);

  // Parser (public để có thể test độc lập)
  QList<Track> parsePlaylistOutput(const QByteArray &jsonLines) const;
  std::optional<Track> parseVideoJson(const QJsonObject &obj) const;

signals:
  void playlistMetadataReady(const QList<Track> &tracks);
  void streamUrlReady(const QString &videoId, const QUrl &streamUrl);
  void errorOccurred(const QString &errorMessage);
  void progressUpdated(const QString &statusMessage);

private slots:
  void onMetadataProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessTimeout();

private:
  static QString ytDlpPath();

  QProcess *m_metadataProcess = nullptr;
  QProcess *m_streamProcess = nullptr;
  QTimer *m_streamTimer = nullptr;
  QMap<QString, QUrl> m_streamCache; // videoId → streamUrl
  QString m_pendingVideoId;
};
