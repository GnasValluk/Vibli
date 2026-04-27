#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QUrl>
#include <optional>

#include "MediaCache.h"
#include "PlaylistManager.h"

/** Định dạng file khi download. */
enum class DownloadFormat { Mp3, Mp4 };

/**
 * @brief Mô tả một download job.
 *
 * Truyền vào YtDlpService::downloadMedia() để bắt đầu tải.
 */
struct DownloadJob {
  QString url; ///< URL playlist hoặc video YouTube
  DownloadFormat format = DownloadFormat::Mp3;
  QString outputDir; ///< Thư mục lưu file (phải tồn tại)
  QString jobId;     ///< UUID do caller tạo để track tiến trình
};

/**
 * @brief YtDlpService – giao tiếp với yt-dlp.exe qua QProcess.
 *
 * Stream URL resolution:
 *  - Format priority: m4a → webm/opus → best  (tránh demux fail trên Windows)
 *  - Queue: nhiều videoId có thể được enqueue, xử lý tuần tự
 *  - Pre-resolve: gọi prefetchStreamUrl() để resolve sẵn track tiếp theo
 *  - Retry với format fallback khi primary format fail
 *
 * Cache 2 lớp:
 *  - In-memory (QMap): nhanh nhất, mất khi thoát app
 *  - Persistent (MediaCache): lưu disk, TTL 6h, sống qua restart
 *
 * Download:
 *  - downloadMedia() chạy yt-dlp với --newline để parse progress từng dòng
 *  - Mỗi job có jobId riêng; nhiều job có thể xếp hàng
 *  - cancelDownload() hủy job đang chạy hoặc xóa khỏi queue
 */
class YtDlpService : public QObject {
  Q_OBJECT

public:
  explicit YtDlpService(QObject *parent = nullptr);
  ~YtDlpService() override = default;

  void setMediaCache(MediaCache *cache);
  static bool isAvailable();

  void fetchPlaylistMetadata(const QString &playlistUrl);

  /**
   * Resolve stream URL cho videoId.
   * Nếu đang bận, enqueue và xử lý sau.
   * Kết quả trả về qua signal streamUrlReady / streamErrorOccurred.
   */
  void resolveStreamUrl(const QString &videoId);

  /**
   * Pre-fetch stream URL cho track tiếp theo ở background.
   * Không emit signal nếu đã có trong cache.
   * Ưu tiên thấp hơn resolveStreamUrl.
   */
  void prefetchStreamUrl(const QString &videoId);

  void invalidateStreamCache(const QString &videoId);

  /**
   * Bắt đầu download job (async).
   * Tiến trình trả về qua downloadProgress / downloadFinished / downloadError.
   * Nếu đang bận, job được xếp hàng và xử lý tuần tự.
   */
  void downloadMedia(const DownloadJob &job);

  /**
   * Hủy job đang chạy hoặc xóa khỏi queue.
   * Nếu job đang chạy, process bị kill và downloadError được emit.
   */
  void cancelDownload(const QString &jobId);

  // Parser (public để test độc lập)
  std::optional<Track> parseVideoJson(const QJsonObject &obj) const;

signals:
  void trackFetched(const Track &track);
  void playlistMetadataReady(const QList<Track> &tracks);
  void fetchProgress(int loaded, int total);

  void streamUrlReady(const QString &videoId, const QUrl &streamUrl);
  void streamErrorOccurred(const QString &videoId, const QString &errorMessage);
  void errorOccurred(const QString &errorMessage);
  void progressUpdated(const QString &statusMessage);

  // ── Download signals ───────────────────────────────────────────────────
  /** Tiến trình download: 0-100, currentFile là tên file đang tải. */
  void downloadProgress(const QString &jobId, int percent,
                        const QString &currentFile);
  /** Download hoàn tất; outputDir là thư mục chứa file đã tải. */
  void downloadFinished(const QString &jobId, const QString &outputDir);
  /** Download thất bại hoặc bị hủy. */
  void downloadError(const QString &jobId, const QString &errorMessage);

private slots:
  void onMetadataReadyRead();
  void onMetadataProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onStreamProcessTimeout();
  void onDownloadReadyRead();
  void onDownloadProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
  // Format selector theo thứ tự ưu tiên
  // Index 0 = primary, 1 = fallback 1, 2 = fallback 2
  static const QStringList FORMAT_PRIORITY;

  static QString ytDlpPath();
  static QString ffmpegDir();
  void processNextInQueue();
  void startStreamProcess(const QString &videoId, const QString &format);
  void processNextDownload();
  void startDownloadProcess(const DownloadJob &job);

  QProcess *m_metadataProcess = nullptr;
  QProcess *m_streamProcess = nullptr;
  QTimer *m_streamTimer = nullptr;

  // In-memory stream cache
  QMap<QString, QUrl> m_streamCache;
  MediaCache *m_mediaCache = nullptr;

  // Queue: { videoId, isPrefetch }
  struct StreamRequest {
    QString videoId;
    bool isPrefetch = false;
  };
  QQueue<StreamRequest> m_streamQueue;
  bool m_streamBusy = false;

  // Retry state cho request đang xử lý
  QString m_pendingVideoId;
  int m_pendingFormatIdx = 0; // index vào FORMAT_PRIORITY
  bool m_pendingIsPrefetch = false;

  QByteArray m_metadataBuffer;
  int m_fetchedCount = 0;

  // ── Download state ─────────────────────────────────────────────────────
  QProcess *m_downloadProcess = nullptr;
  QQueue<DownloadJob> m_downloadQueue;
  bool m_downloadBusy = false;
  DownloadJob m_activeDownload;  ///< Job đang chạy
  QString m_currentDownloadFile; ///< Tên file đang tải (từ stdout)
};
