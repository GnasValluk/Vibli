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
 * @brief State of a download job.
 */
enum class JobState { Queued, Downloading, Done, Cancelled, Failed };

/**
 * @brief Widget displaying a single download job.
 *
 * Layout:
 *  ┌─────────────────────────────────────────────────────┐
 *  │ [badge] Short display name                [Cancel / Open]  │
 *  │ ▸ File name being downloaded (elided)               │
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
  QLabel *m_titleLabel; ///< Short name, elided
  QLabel *m_fileLabel;  ///< Name of file being downloaded
  QLabel *m_metaLabel;  ///< Speed + ETA
  QProgressBar *m_progressBar;
  QPushButton *m_actionBtn;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief DownloadManagerDialog – non-modal dialog for managing download jobs.
 *
 * Header bar shows total jobs / running count.
 * Each job has its own widget with progress bar, speed, ETA, Cancel / Open
 * folder button.
 */
class DownloadManagerDialog : public QDialog {
  Q_OBJECT

public:
  explicit DownloadManagerDialog(YtDlpService *service,
                                 QWidget *parent = nullptr);
  ~DownloadManagerDialog() override = default;

  /**
   * Adds a new job and starts the download.
   * @param job           Full job info (including jobId).
   * @param displayLabel  Short display name (e.g. "Lofi Playlist").
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

  QLabel *m_headerLabel; ///< "X downloading  •  Y completed"
  QWidget *m_listContainer;
  QVBoxLayout *m_listLayout;
  QLabel *m_emptyLabel;

  QMap<QString, DownloadJobWidget *> m_widgets; ///< jobId → widget
  int m_totalJobs = 0;
  int m_activeJobs = 0;
  int m_finishedJobs = 0;
};
