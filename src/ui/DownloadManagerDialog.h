#pragma once

#include <QDateTime>
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
enum class JobState { Queued, Downloading, Retrying, Done, Cancelled, Failed };

/**
 * @brief Widget displaying a single download job.
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
  void setFinishedWithStats(const QString &outputDir, int downloaded,
                            int skipped, int total);
  void setCancelled();
  void setFailed(const QString &message);
  void setRetrying(int attempt);
  void setQueued();

  JobState state() const { return m_state; }

signals:
  void cancelRequested(const QString &jobId);

private:
  QString m_jobId;
  QString m_outputDir;
  JobState m_state = JobState::Queued;

  QLabel *m_badgeLabel;
  QLabel *m_titleLabel;
  QLabel *m_fileLabel;
  QLabel *m_metaLabel;
  QProgressBar *m_progressBar;
  QPushButton *m_actionBtn;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief DownloadManagerDialog – non-modal dialog for managing download jobs.
 */
class DownloadManagerDialog : public QDialog {
  Q_OBJECT

public:
  explicit DownloadManagerDialog(YtDlpService *service,
                                 QWidget *parent = nullptr);
  ~DownloadManagerDialog() override = default;

  void addJob(const DownloadJob &job, const QString &displayLabel);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onDownloadProgress(const QString &jobId, int percent,
                          const QString &payload);
  void onDownloadFinished(const QString &jobId, const QString &outputDir);
  void onDownloadError(const QString &jobId, const QString &errorMessage);
  void onDownloadCancelled(const QString &jobId);
  void onDownloadRetrying(const QString &jobId, int attempt);
  void onDownloadStatsUpdated(const QString &jobId, int downloaded, int skipped,
                              int total, const QStringList &skippedNames);
  void onCancelRequested(const QString &jobId);

private:
  void updateHeader();
  void checkAndShowSummary();
  void clearAll();

  YtDlpService *m_service;

  QWidget *m_listContainer;
  QVBoxLayout *m_listLayout;
  QLabel *m_emptyLabel;

  QWidget *m_summaryWidget = nullptr;
  QLabel *m_summaryStatsLabel = nullptr;
  QLabel *m_summarySkippedLabel = nullptr;

  QMap<QString, DownloadJobWidget *> m_widgets;
  QMap<QString, QString> m_jobLabels;

  // Per-job playlist stats: jobId → {downloaded, skipped, total, skippedNames}
  struct JobStats {
    int downloaded = 0;
    int skipped = 0;
    int total = 0;
    QStringList skippedNames;
  };
  QMap<QString, JobStats> m_jobStats;

  int m_totalJobs = 0;
  int m_activeJobs = 0;
  int m_finishedJobs = 0;
  int m_failedJobs = 0;
  int m_cancelledJobs = 0;
  QStringList m_failedJobNames;

  // Speed tracking for average calculation
  QList<double> m_speedSamples; ///< speed samples in bytes/s
  QDateTime m_sessionStart;
};
