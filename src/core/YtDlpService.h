#pragma once

#include <QByteArray>
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
 * Streaming mode: emit trackFetched() cho từng video ngay khi parse xong,
 * không đợi hết playlist. Cho phép UI hiển thị dần dần.
 */
class YtDlpService : public QObject {
  Q_OBJECT

public:
  explicit YtDlpService(QObject *parent = nullptr);
  ~YtDlpService() override = default;

  static bool isAvailable();

  void fetchPlaylistMetadata(const QString &playlistUrl);
  void resolveStreamUrl(const QString &videoId);
  void invalidateStreamCache(const QString &videoId);

  // Parser (public để test độc lập)
  std::optional<Track> parseVideoJson(const QJsonObject &obj) const;

signals:
  // Streaming: emit từng track ngay khi parse xong
  void trackFetched(const Track &track);
  // Khi toàn bộ playlist xong
  void playlistMetadataReady(const QList<Track> &tracks);
  // Tiến trình: (loaded, total) — total = -1 nếu chưa biết
  void fetchProgress(int loaded, int total);

  void streamUrlReady(const QString &videoId, const QUrl &streamUrl);
  // Lỗi khi fetch metadata playlist
  void metadataErrorOccurred(const QString &errorMessage);
  // Lỗi khi resolve stream URL (dùng riêng để tránh vòng lặp)
  void streamErrorOccurred(const QString &videoId, const QString &errorMessage);
  // Lỗi chung (backward compat)
  void errorOccurred(const QString &errorMessage);
  void progressUpdated(const QString &statusMessage);

private slots:
  void onMetadataReadyRead();
  void onMetadataProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessTimeout();

private:
  static QString ytDlpPath();

  QProcess *m_metadataProcess = nullptr;
  QProcess *m_streamProcess = nullptr;
  QTimer *m_streamTimer = nullptr;
  QMap<QString, QUrl> m_streamCache;
  QString m_pendingVideoId;

  QByteArray m_metadataBuffer;  // buffer dòng chưa hoàn chỉnh
  QList<Track> m_fetchedTracks; // tích lũy để emit playlistMetadataReady
  int m_fetchedCount = 0;
};
