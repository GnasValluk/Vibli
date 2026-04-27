#pragma once

#include <QDialog>
#include <QLabel>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "../core/YtDlpService.h"

/**
 * @brief Trạng thái của một download job.
 */
enum class JobState { Queued, Downloading, Done, Cancelled, Failed };

/**
 * @brief Widget hiển thị một download job.
 *
 * Layout:
 *  ┌─────────────────────────────────────────────────────┐
 *  │ [badge] Tên hiển thị ngắn gọn          [Hủy / Mở]  │
 *  │ ▸ Tên file đang tải (elided)                        │
 *  │ ████████████░░░░░░░░  45%   1.2 MB/s  ETA 0:32     │
 *  └─────────────────────────────────────────────────────┘
 */
class DownloadJobWidget : public QWidget {
  Q_OBJECT
public:
  explicit DownloadJobWidget(const QString &jobId, const QString &displayLabel,
                             DownloadFormat format, QWidget *parent = nullptr);

  void setProgress(int percent, const QString &currentFile,
                   const QString &speed = {}, const QString &eta = {},
                   const QString &phaseText = {});
  void setFinished(const QString &outputDir);
  void setCancelled();
  void setFailed(const QString &message);
  void setQueued();

  JobState state() const { return m_state; }

signals:
  void cancelRequested(const QString &jobId);

private:
  void refreshStyle();

  QString m_jobId;
  QString m_outputDir;
  JobState m_state = JobState::Queued;

  QLabel *m_badgeLabel; ///< "MP3" / "MP4"
  QLabel *m_titleLabel; ///< Tên ngắn, elided
  QLabel *m_fileLabel;  ///< Tên file đang tải
  QLabel *m_metaLabel;  ///< Speed + ETA
  QProgressBar *m_progressBar;
  QPushButton *m_actionBtn;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief DownloadManagerDialog – dialog non-modal quản lý download jobs.
 *
 * Header bar hiển thị tổng số job / đang chạy.
 * Mỗi job có widget riêng với progress bar, speed, ETA, nút Hủy / Mở thư mục.
 */
class DownloadManagerDialog : public QDialog {
  Q_OBJECT

public:
  explicit DownloadManagerDialog(YtDlpService *service,
                                 QWidget *parent = nullptr);
  ~DownloadManagerDialog() override = default;

  /**
   * Thêm job mới và bắt đầu download.
   * @param job           Job đầy đủ thông tin (kể cả jobId).
   * @param displayLabel  Tên ngắn hiển thị (ví dụ: "Lofi Playlist").
   */
  void addJob(const DownloadJob &job, const QString &displayLabel);

private slots:
  void onDownloadProgress(const QString &jobId, int percent,
                          const QString &currentFile);
  void onDownloadFinished(const QString &jobId, const QString &outputDir);
  void onDownloadError(const QString &jobId, const QString &errorMessage);
  void onDownloadCancelled(const QString &jobId);
  void onCancelRequested(const QString &jobId);

private:
  void updateHeader();

  YtDlpService *m_service;

  QLabel *m_headerLabel; ///< "X đang tải  •  Y hoàn tất"
  QWidget *m_listContainer;
  QVBoxLayout *m_listLayout;
  QLabel *m_emptyLabel;

  QMap<QString, DownloadJobWidget *> m_widgets; ///< jobId → widget
  int m_totalJobs = 0;
  int m_activeJobs = 0;
  int m_finishedJobs = 0;
};
