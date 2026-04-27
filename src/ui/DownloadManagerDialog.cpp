#include "DownloadManagerDialog.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>

// ─────────────────────────────────────────────────────────────────────────────
// DownloadJobWidget
// ─────────────────────────────────────────────────────────────────────────────

DownloadJobWidget::DownloadJobWidget(const QString &jobId,
                                     const QString &displayLabel,
                                     DownloadFormat format, QWidget *parent)
    : QWidget(parent), m_jobId(jobId) {
  setObjectName("jobWidget");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // ── Badge MP3 / MP4 ───────────────────────────────────────────────────
  m_badgeLabel =
      new QLabel(format == DownloadFormat::Mp3 ? "MP3" : "MP4", this);
  m_badgeLabel->setObjectName(format == DownloadFormat::Mp3 ? "badgeMp3"
                                                            : "badgeMp4");
  m_badgeLabel->setFixedSize(36, 20);
  m_badgeLabel->setAlignment(Qt::AlignCenter);

  // ── Title (elided) ────────────────────────────────────────────────────
  m_titleLabel = new QLabel(displayLabel, this);
  m_titleLabel->setObjectName("jobTitle");
  m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  // Elide long text → prevents layout stretching
  m_titleLabel->setMinimumWidth(0);
  m_titleLabel->setMaximumWidth(9999);

  // ── Action button (fixed width, won't be pushed out of layout) ───────
  m_actionBtn = new QPushButton("Cancel", this);
  m_actionBtn->setObjectName("cancelBtn");
  m_actionBtn->setFixedSize(80, 28);
  m_actionBtn->setCursor(Qt::PointingHandCursor);

  // ── File label ────────────────────────────────────────────────────────
  m_fileLabel = new QLabel("Waiting in queue...", this);
  m_fileLabel->setObjectName("jobFile");
  m_fileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_fileLabel->setMinimumWidth(0);

  // ── Meta: speed + ETA ─────────────────────────────────────────────────
  m_metaLabel = new QLabel(this);
  m_metaLabel->setObjectName("jobMeta");
  m_metaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_metaLabel->setFixedWidth(160);

  // ── Progress bar ──────────────────────────────────────────────────────
  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  m_progressBar->setTextVisible(false);
  m_progressBar->setFixedHeight(6);
  m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // ── Layouts ───────────────────────────────────────────────────────────
  // Row 1: badge + title + button
  auto *row1 = new QHBoxLayout;
  row1->setSpacing(8);
  row1->setContentsMargins(0, 0, 0, 0);
  row1->addWidget(m_badgeLabel);
  row1->addWidget(m_titleLabel, 1); // stretch=1 → takes all remaining space
  row1->addWidget(m_actionBtn);

  // Row 2: file label + meta
  auto *row2 = new QHBoxLayout;
  row2->setSpacing(8);
  row2->setContentsMargins(0, 0, 0, 0);
  row2->addWidget(m_fileLabel, 1);
  row2->addWidget(m_metaLabel);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 10, 14, 12);
  layout->setSpacing(6);
  layout->addLayout(row1);
  layout->addLayout(row2);
  layout->addWidget(m_progressBar);

  // ── Connections ───────────────────────────────────────────────────────
  connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
    if (m_state == JobState::Done) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir));
    } else if (m_state == JobState::Downloading ||
               m_state == JobState::Queued) {
      emit cancelRequested(m_jobId);
    }
  });
}

void DownloadJobWidget::setProgress(int percent, const QString &currentFile,
                                    const QString &speed, const QString &eta,
                                    const QString &phaseText) {
  // Reset from Failed/Retrying state if we receive new progress
  // (means yt-dlp moved to next file in playlist)
  if (m_state == JobState::Failed || m_state == JobState::Retrying) {
    m_state = JobState::Downloading;
    m_progressBar->setObjectName("");
    m_progressBar->style()->unpolish(m_progressBar);
    m_progressBar->style()->polish(m_progressBar);
    m_fileLabel->setObjectName("jobFile");
    m_fileLabel->style()->unpolish(m_fileLabel);
    m_fileLabel->style()->polish(m_fileLabel);
    m_actionBtn->setEnabled(true);
    m_actionBtn->setText("Cancel");
    m_actionBtn->setObjectName("cancelBtn");
    m_actionBtn->style()->unpolish(m_actionBtn);
    m_actionBtn->style()->polish(m_actionBtn);
  }

  m_state = JobState::Downloading;

  // Giai đoạn hậu kỳ (convert, embed...) — progress bar indeterminate
  if (percent < 0) {
    if (percent == -2) {
      // Live playlist counter — chỉ update meta label, không đổi state
      m_metaLabel->setText(phaseText);
      return;
    }
    m_progressBar->setRange(0, 0); // indeterminate animation
    m_fileLabel->setObjectName("jobFilePhase");
    m_fileLabel->style()->unpolish(m_fileLabel);
    m_fileLabel->style()->polish(m_fileLabel);
    m_fileLabel->setText("⚙  " + phaseText);
    m_metaLabel->clear();
    return;
  }

  // Ensure progress bar is back to normal if it was previously indeterminate
  if (m_progressBar->maximum() == 0) {
    m_progressBar->setRange(0, 100);
    m_fileLabel->setObjectName("jobFile");
    m_fileLabel->style()->unpolish(m_fileLabel);
    m_fileLabel->style()->polish(m_fileLabel);
  }

  m_progressBar->setValue(percent);

  if (!currentFile.isEmpty()) {
    const QFontMetrics fm(m_fileLabel->font());
    const int maxW = m_fileLabel->width() > 0 ? m_fileLabel->width() : 300;
    m_fileLabel->setText(
        fm.elidedText("▸ " + currentFile, Qt::ElideMiddle, maxW));
  }

  // Speed + ETA
  QString meta;
  if (!speed.isEmpty())
    meta += speed;
  if (!eta.isEmpty())
    meta += (meta.isEmpty() ? QString{} : QStringLiteral("  ")) + "ETA " + eta;
  m_metaLabel->setText(meta);
}

void DownloadJobWidget::setFinished(const QString &outputDir) {
  m_state = JobState::Done;
  m_outputDir = outputDir;
  m_progressBar->setValue(100);
  if (m_progressBar->maximum() == 0)
    m_progressBar->setRange(0, 100);
  m_progressBar->setObjectName("progressDone");
  m_progressBar->style()->unpolish(m_progressBar);
  m_progressBar->style()->polish(m_progressBar);

  m_fileLabel->setText("✓  Completed");
  m_fileLabel->setObjectName("jobFileDone");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);

  m_metaLabel->clear();

  m_actionBtn->setText("Open Folder");
  m_actionBtn->setObjectName("openBtn");
  m_actionBtn->style()->unpolish(m_actionBtn);
  m_actionBtn->style()->polish(m_actionBtn);
}

void DownloadJobWidget::setFinishedWithStats(const QString &outputDir,
                                             int downloaded, int skipped,
                                             int total) {
  setFinished(outputDir);
  if (total > 0) {
    QString statusText;
    if (skipped == 0) {
      statusText = QString("✓  Downloaded %1/%1").arg(total);
    } else {
      statusText = QString("✓  Downloaded %1/%2  —  %3 skipped")
                       .arg(downloaded)
                       .arg(total)
                       .arg(skipped);
    }
    m_fileLabel->setText(statusText);
  }
}

void DownloadJobWidget::setCancelled() {
  m_state = JobState::Cancelled;
  m_progressBar->setObjectName("progressCancelled");
  m_progressBar->style()->unpolish(m_progressBar);
  m_progressBar->style()->polish(m_progressBar);

  m_fileLabel->setText("⊘  Cancelled");
  m_fileLabel->setObjectName("jobFileCancelled");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);

  m_metaLabel->clear();
  m_actionBtn->setEnabled(false);
  m_actionBtn->setText("Cancelled");
  m_actionBtn->setObjectName("doneBtn");
  m_actionBtn->style()->unpolish(m_actionBtn);
  m_actionBtn->style()->polish(m_actionBtn);
}

void DownloadJobWidget::setFailed(const QString &message) {
  m_state = JobState::Failed;

  // Reset progress bar from indeterminate if needed
  if (m_progressBar->maximum() == 0)
    m_progressBar->setRange(0, 100);

  m_progressBar->setObjectName("progressFailed");
  m_progressBar->style()->unpolish(m_progressBar);
  m_progressBar->style()->polish(m_progressBar);

  m_fileLabel->setText("✕  Error — max retries exceeded, skipped");
  m_fileLabel->setObjectName("jobFileError");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);

  m_metaLabel->clear();
  m_actionBtn->setEnabled(false);
  m_actionBtn->setText("Skipped");
  m_actionBtn->setObjectName("doneBtn");
  m_actionBtn->style()->unpolish(m_actionBtn);
  m_actionBtn->style()->polish(m_actionBtn);
}

void DownloadJobWidget::setQueued() {
  m_state = JobState::Queued;
  m_fileLabel->setText("Waiting in queue...");
  m_metaLabel->clear();
}

void DownloadJobWidget::setRetrying(int attempt) {
  m_state = JobState::Retrying;

  // Reset progress bar to indeterminate
  m_progressBar->setRange(0, 0);
  m_fileLabel->setObjectName("jobFilePhase");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);
  m_fileLabel->setText(
      QString("⚠  Error encountered, retrying... (%1/1)").arg(attempt));
  m_metaLabel->clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// DownloadManagerDialog
// ─────────────────────────────────────────────────────────────────────────────

DownloadManagerDialog::DownloadManagerDialog(YtDlpService *service,
                                             QWidget *parent)
    : QDialog(parent), m_service(service) {
  setWindowTitle("Download Manager");
  setMinimumWidth(520);
  setMinimumHeight(80);
  // No maximum height — controlled by scroll area
  resize(600,
         80 * 3 + 2); // start at 3-job height, wide enough to be comfortable
  setWindowFlags(windowFlags() | Qt::Window);

  // ── Separator ─────────────────────────────────────────────────────────
  auto *separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setObjectName("separator");

  // ── List container ────────────────────────────────────────────────────
  m_listContainer = new QWidget;
  m_listContainer->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);

  m_listLayout = new QVBoxLayout(m_listContainer);
  m_listLayout->setContentsMargins(0, 0, 0, 0);
  m_listLayout->setSpacing(0);
  m_listLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

  m_emptyLabel =
      new QLabel("No downloads yet.\nClick Download MP3 / MP4 to get started.",
                 m_listContainer);
  m_emptyLabel->setObjectName("emptyLabel");
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_emptyLabel->setWordWrap(true);
  m_listLayout->addWidget(m_emptyLabel);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidget(m_listContainer);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea->setFrameShape(QFrame::NoFrame);
  // Show up to 3 jobs (~80px each), scroll if more
  scrollArea->setMinimumHeight(80);
  scrollArea->setMaximumHeight(80 * 3);

  // ── Main layout ───────────────────────────────────────────────────────
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  mainLayout->addWidget(separator);
  mainLayout->addWidget(scrollArea, 1);

  // ── Summary widget (shown when all jobs complete) ─────────────────────
  auto *summaryFrame = new QFrame(this);
  summaryFrame->setObjectName("summaryFrame");
  summaryFrame->setFrameShape(QFrame::NoFrame);

  // Row 1: stats (time + speed)
  m_summaryStatsLabel = new QLabel(summaryFrame);
  m_summaryStatsLabel->setObjectName("summaryStats");

  // Row 2: skipped names
  m_summarySkippedLabel = new QLabel(summaryFrame);
  m_summarySkippedLabel->setObjectName("summarySkipped");
  m_summarySkippedLabel->setWordWrap(true);
  m_summarySkippedLabel->hide();

  auto *summaryInner = new QVBoxLayout(summaryFrame);
  summaryInner->setContentsMargins(16, 12, 16, 12);
  summaryInner->setSpacing(4);
  summaryInner->addWidget(m_summaryStatsLabel);
  summaryInner->addWidget(m_summarySkippedLabel);

  m_summaryWidget = summaryFrame;
  m_summaryWidget->hide();
  mainLayout->addWidget(m_summaryWidget);

  // ── Stylesheet ────────────────────────────────────────────────────────
  setStyleSheet(R"(
    QDialog {
        background-color: #161616;
        color: #e0e0e0;
    }
    QWidget {
        background-color: #161616;
        color: #e0e0e0;
    }
    QWidget#headerBar {
        background: #1e1e1e;
    }
    QLabel#headerLabel {
        font-size: 12px;
        font-weight: bold;
        color: #888888;
        letter-spacing: 0.5px;
    }
    QFrame#separator {        color: #2a2a2a;
        background: #2a2a2a;
        max-height: 1px;
    }

    /* ── Job widget ── */
    QWidget#jobWidget {
        background: #1e1e1e;
        border-bottom: 1px solid #252525;
    }
    QWidget#jobWidget:hover {
        background: #222222;
    }

    /* Badge */
    QLabel#badgeMp3 {
        background: #7c3a00;
        color: #f0a060;
        font-size: 10px;
        font-weight: bold;
        border-radius: 3px;
    }
    QLabel#badgeMp4 {
        background: #003a6e;
        color: #60a8f0;
        font-size: 10px;
        font-weight: bold;
        border-radius: 3px;
    }

    /* Title */
    QLabel#jobTitle {
        font-size: 13px;
        font-weight: bold;
        color: #d8d8d8;
    }

    /* File / status labels */
    QLabel#jobFile {
        font-size: 11px;
        color: #666666;
    }
    QLabel#jobFileDone {
        font-size: 11px;
        color: #1db954;
    }
    QLabel#jobFileCancelled {
        font-size: 11px;
        color: #888888;
    }
    QLabel#jobFileError {
        font-size: 11px;
        color: #e05555;
    }
    QLabel#jobFilePhase {
        font-size: 11px;
        color: #d4a017;
    }

    /* Meta (speed/ETA) */
    QLabel#jobMeta {
        font-size: 10px;
        color: #555555;
    }

    /* Progress bars */
    QProgressBar {
        background: #2a2a2a;
        border: none;
        border-radius: 3px;
    }
    QProgressBar::chunk {
        background: #1db954;
        border-radius: 3px;
    }
    QProgressBar#progressDone::chunk {
        background: #1db954;
    }
    QProgressBar#progressCancelled::chunk {
        background: #555555;
    }
    QProgressBar#progressFailed::chunk {
        background: #e05555;
    }

    /* Buttons */
    QPushButton#cancelBtn {
        background: transparent;
        color: #888888;
        border: 1px solid #333333;
        border-radius: 4px;
        font-size: 11px;
    }
    QPushButton#cancelBtn:hover {
        color: #ff6b6b;
        border-color: #ff6b6b;
        background: #2a1515;
    }
    QPushButton#openBtn {
        background: transparent;
        color: #1db954;
        border: 1px solid #1db954;
        border-radius: 4px;
        font-size: 11px;
    }
    QPushButton#openBtn:hover {
        background: #0d2a1a;
    }
    QPushButton#doneBtn {
        background: transparent;
        color: #444444;
        border: 1px solid #2a2a2a;
        border-radius: 4px;
        font-size: 11px;
    }

    /* Empty state */
    QLabel#emptyLabel {
        color: #444444;
        font-size: 13px;
        padding: 48px 24px;
        line-height: 1.6;
    }

    /* Scrollbar */
    QScrollBar:vertical {
        background: #161616;
        width: 5px;
        border-radius: 2px;
    }
    QScrollBar::handle:vertical {
        background: #333333;
        border-radius: 2px;
        min-height: 24px;
    }
    QScrollBar::handle:vertical:hover {
        background: #555555;
    }
    QScrollBar::add-line:vertical,
    QScrollBar::sub-line:vertical { height: 0; }

    QFrame#summaryFrame {
        background: #1c1c1c;
        border-top: 1px solid #2a2a2a;
    }
    QLabel#summaryResult {
        font-size: 13px;
        font-weight: bold;
        color: #1db954;
    }
    QLabel#summaryResultPartial {
        font-size: 13px;
        font-weight: bold;
        color: #d4a017;
    }
    QLabel#summaryStats {
        font-size: 11px;
        color: #555555;
    }
    QLabel#summarySkipped {
        font-size: 11px;
        color: #c0704a;
    }
  )");

  // ── Connect service signals ────────────────────────────────────────────
  connect(m_service, &YtDlpService::downloadProgress, this,
          &DownloadManagerDialog::onDownloadProgress);
  connect(m_service, &YtDlpService::downloadFinished, this,
          &DownloadManagerDialog::onDownloadFinished);
  connect(m_service, &YtDlpService::downloadError, this,
          &DownloadManagerDialog::onDownloadError);
  connect(m_service, &YtDlpService::downloadCancelled, this,
          &DownloadManagerDialog::onDownloadCancelled);
  connect(m_service, &YtDlpService::downloadRetrying, this,
          &DownloadManagerDialog::onDownloadRetrying);
  connect(m_service, &YtDlpService::downloadStatsUpdated, this,
          &DownloadManagerDialog::onDownloadStatsUpdated);
}

void DownloadManagerDialog::addJob(const DownloadJob &job,
                                   const QString &displayLabel) {
  m_emptyLabel->hide();
  ++m_totalJobs;
  ++m_activeJobs;

  if (m_sessionStart.isNull())
    m_sessionStart = QDateTime::currentDateTime();

  auto *w = new DownloadJobWidget(job.jobId, displayLabel, job.format,
                                  m_listContainer);
  connect(w, &DownloadJobWidget::cancelRequested, this,
          &DownloadManagerDialog::onCancelRequested);

  m_listLayout->addWidget(w);
  m_widgets.insert(job.jobId, w);
  m_jobLabels.insert(job.jobId, displayLabel);
  m_summaryWidget->hide();

  updateHeader();

  // Always keep 3-job height — scroll handles overflow
  const int summaryH =
      m_summaryWidget->isVisible() ? m_summaryWidget->sizeHint().height() : 0;
  resize(width(), 80 * 3 + summaryH + 2);

  m_service->downloadMedia(job);
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

void DownloadManagerDialog::updateHeader() {
  // Header removed — no-op
}

void DownloadManagerDialog::closeEvent(QCloseEvent *event) {
  clearAll();
  event->accept();
}

void DownloadManagerDialog::clearAll() {
  // Cancel any active/queued downloads
  for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
    const JobState s = it.value()->state();
    if (s == JobState::Downloading || s == JobState::Queued ||
        s == JobState::Retrying) {
      m_service->cancelDownload(it.key());
    }
  }

  // Remove all job widgets
  QLayoutItem *item;
  while ((item = m_listLayout->takeAt(0)) != nullptr) {
    if (item->widget() && item->widget() != m_emptyLabel)
      item->widget()->deleteLater();
    delete item;
  }
  m_listLayout->addWidget(m_emptyLabel);
  m_emptyLabel->show();

  // Reset all state
  m_widgets.clear();
  m_jobLabels.clear();
  m_jobStats.clear();
  m_totalJobs = 0;
  m_activeJobs = 0;
  m_finishedJobs = 0;
  m_failedJobs = 0;
  m_cancelledJobs = 0;
  m_failedJobNames.clear();
  m_speedSamples.clear();
  m_sessionStart = QDateTime();

  m_summaryWidget->hide();
  m_summaryStatsLabel->clear();
  m_summarySkippedLabel->clear();
  m_summarySkippedLabel->hide();

  resize(600, 80 * 3 + 2);
}

// ── Slots
// ─────────────────────────────────────────────────────────────────────

void DownloadManagerDialog::onDownloadProgress(const QString &jobId,
                                               int percent,
                                               const QString &payload) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;

  // Payload format: "filename\x1Fspeed\x1Feta\x1FphaseText"
  const QStringList parts = payload.split('\x1F');
  const QString fileName = parts.value(0);
  const QString speed = parts.value(1);
  const QString eta = parts.value(2);
  const QString phaseText = parts.value(3);

  // Collect speed samples for average calculation (e.g. "1.23MiB/s")
  if (!speed.isEmpty()) {
    static const QRegularExpression reSpeed(
        R"(([\d.]+)\s*(KiB|MiB|GiB|KB|MB|GB)/s)",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = reSpeed.match(speed);
    if (m.hasMatch()) {
      double val = m.captured(1).toDouble();
      const QString unit = m.captured(2).toLower();
      if (unit == "kib" || unit == "kb")
        val *= 1024.0;
      else if (unit == "mib" || unit == "mb")
        val *= 1024.0 * 1024.0;
      else if (unit == "gib" || unit == "gb")
        val *= 1024.0 * 1024.0 * 1024.0;
      m_speedSamples.append(val);
      // Keep last 200 samples to avoid unbounded growth
      if (m_speedSamples.size() > 200)
        m_speedSamples.removeFirst();
    }
  }

  w->setProgress(percent, fileName, speed, eta, phaseText);
}

void DownloadManagerDialog::onDownloadFinished(const QString &jobId,
                                               const QString &outputDir) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;
  if (w->state() != JobState::Done) {
    --m_activeJobs;
    ++m_finishedJobs;
    updateHeader();
  }
  const JobStats &s = m_jobStats.value(jobId);
  w->setFinishedWithStats(outputDir, s.downloaded, s.skipped, s.total);
  checkAndShowSummary();
}
void DownloadManagerDialog::onDownloadError(const QString &jobId,
                                            const QString &errorMessage) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;
  if (w->state() == JobState::Downloading || w->state() == JobState::Queued ||
      w->state() == JobState::Retrying) {
    --m_activeJobs;
    updateHeader();
  }
  w->setFailed(errorMessage);
  ++m_failedJobs;
  m_failedJobNames << m_jobLabels.value(jobId, jobId);
  checkAndShowSummary();
}

void DownloadManagerDialog::onDownloadCancelled(const QString &jobId) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;
  if (w->state() == JobState::Downloading || w->state() == JobState::Queued ||
      w->state() == JobState::Retrying) {
    --m_activeJobs;
    updateHeader();
  }
  ++m_cancelledJobs;
  w->setCancelled();
  checkAndShowSummary();
}

void DownloadManagerDialog::onDownloadRetrying(const QString &jobId,
                                               int attempt) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;
  w->setRetrying(attempt);
}

void DownloadManagerDialog::onDownloadStatsUpdated(
    const QString &jobId, int downloaded, int skipped, int total,
    const QStringList &skippedNames) {
  m_jobStats[jobId] = {downloaded, skipped, total, skippedNames};

  // Live update the file label while downloading
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w || w->state() == JobState::Done || w->state() == JobState::Failed)
    return;
  if (total > 0) {
    const QString statsText =
        QString("▸ %1/%2 files").arg(downloaded + skipped).arg(total);
    // Only update meta label to avoid overriding current file name
    // We'll use metaLabel for this live counter
    // Actually emit a special progress to update the display
    const QString payload = QString("\x1F\x1F\x1F") + statsText;
    // Use setProgress with special phaseText to show counter
    w->setProgress(-2, {}, {}, {}, statsText);
  }
}

void DownloadManagerDialog::onCancelRequested(const QString &jobId) {
  m_service->cancelDownload(jobId);
}

void DownloadManagerDialog::checkAndShowSummary() {
  if (m_activeJobs > 0 || m_totalJobs == 0)
    return;

  // Aggregate stats across all jobs
  int totalDownloaded = 0, totalSkipped = 0, totalFiles = 0;
  QStringList allSkippedNames;
  for (auto it = m_jobStats.begin(); it != m_jobStats.end(); ++it) {
    totalDownloaded += it->downloaded;
    totalSkipped += it->skipped;
    totalFiles += it->total;
    allSkippedNames << it->skippedNames;
  }
  // Also count jobs that had no per-file stats (single video jobs)
  for (auto *w : m_widgets) {
    const QString jobId = m_widgets.key(w);
    if (!m_jobStats.contains(jobId)) {
      if (w->state() == JobState::Done)
        totalDownloaded++;
      else if (w->state() == JobState::Failed)
        totalSkipped++;
      if (w->state() != JobState::Cancelled)
        totalFiles++;
    }
  }

  // Elapsed time
  QString elapsedStr;
  if (!m_sessionStart.isNull()) {
    const int secs =
        static_cast<int>(m_sessionStart.secsTo(QDateTime::currentDateTime()));
    if (secs < 60)
      elapsedStr = QString("%1s").arg(secs);
    else if (secs < 3600)
      elapsedStr = QString("%1m %2s").arg(secs / 60).arg(secs % 60);
    else
      elapsedStr = QString("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60);
  }

  // Average speed
  QString avgSpeedStr;
  if (!m_speedSamples.isEmpty()) {
    double sum = 0;
    for (double s : m_speedSamples)
      sum += s;
    double avg = sum / m_speedSamples.size();
    if (avg >= 1024.0 * 1024.0)
      avgSpeedStr = QString("%1 MiB/s").arg(avg / (1024.0 * 1024.0), 0, 'f', 1);
    else if (avg >= 1024.0)
      avgSpeedStr = QString("%1 KiB/s").arg(avg / 1024.0, 0, 'f', 0);
    else
      avgSpeedStr = QString("%1 B/s").arg(avg, 0, 'f', 0);
  }

  // ── Stats label ───────────────────────────────────────────────────────
  QStringList statsItems;
  if (!elapsedStr.isEmpty())
    statsItems << QString("⏱  %1").arg(elapsedStr);
  if (!avgSpeedStr.isEmpty())
    statsItems << QString("⚡  avg %1").arg(avgSpeedStr);
  m_summaryStatsLabel->setText(statsItems.join("     "));
  m_summaryStatsLabel->setVisible(!statsItems.isEmpty());

  // ── Skipped names ─────────────────────────────────────────────────────
  if (!allSkippedNames.isEmpty()) {
    m_summarySkippedLabel->setText("Skipped: " + allSkippedNames.join(", "));
    m_summarySkippedLabel->show();
  } else {
    m_summarySkippedLabel->hide();
  }

  m_summaryWidget->show();
  adjustSize();
}
