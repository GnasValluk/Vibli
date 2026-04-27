#include "DownloadManagerDialog.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
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
  m_state = JobState::Downloading;

  // Giai đoạn hậu kỳ (convert, embed...) — progress bar indeterminate
  if (percent < 0) {
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
  m_progressBar->setObjectName("progressFailed");
  m_progressBar->style()->unpolish(m_progressBar);
  m_progressBar->style()->polish(m_progressBar);

  const QFontMetrics fm(m_fileLabel->font());
  const int maxW = m_fileLabel->width() > 0 ? m_fileLabel->width() : 300;
  m_fileLabel->setText(fm.elidedText("✕  " + message, Qt::ElideRight, maxW));
  m_fileLabel->setObjectName("jobFileError");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);

  m_metaLabel->clear();
  m_actionBtn->setEnabled(false);
  m_actionBtn->setText("Error");
  m_actionBtn->setObjectName("doneBtn");
  m_actionBtn->style()->unpolish(m_actionBtn);
  m_actionBtn->style()->polish(m_actionBtn);
}

void DownloadJobWidget::setQueued() {
  m_state = JobState::Queued;
  m_fileLabel->setText("Waiting in queue...");
  m_metaLabel->clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// DownloadManagerDialog
// ─────────────────────────────────────────────────────────────────────────────

DownloadManagerDialog::DownloadManagerDialog(YtDlpService *service,
                                             QWidget *parent)
    : QDialog(parent), m_service(service) {
  setWindowTitle("Download Manager");
  setMinimumWidth(460);
  setMinimumHeight(120);
  setMaximumHeight(600);
  resize(540, 120); // start small, auto-expand as jobs are added
  setWindowFlags(windowFlags() | Qt::Window);

  // ── Header bar ────────────────────────────────────────────────────────
  auto *headerWidget = new QWidget(this);
  headerWidget->setObjectName("headerBar");
  headerWidget->setFixedHeight(40);

  m_headerLabel = new QLabel("No downloads yet", headerWidget);
  m_headerLabel->setObjectName("headerLabel");

  auto *headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(14, 0, 14, 0);
  headerLayout->addWidget(m_headerLabel);
  headerLayout->addStretch();

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
  scrollArea->setFrameShape(QFrame::NoFrame);

  // ── Main layout ───────────────────────────────────────────────────────
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  mainLayout->addWidget(headerWidget);
  mainLayout->addWidget(separator);
  mainLayout->addWidget(scrollArea, 1);

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
    QFrame#separator {
        color: #2a2a2a;
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
}

void DownloadManagerDialog::addJob(const DownloadJob &job,
                                   const QString &displayLabel) {
  m_emptyLabel->hide();
  ++m_totalJobs;
  ++m_activeJobs;

  auto *w = new DownloadJobWidget(job.jobId, displayLabel, job.format,
                                  m_listContainer);
  connect(w, &DownloadJobWidget::cancelRequested, this,
          &DownloadManagerDialog::onCancelRequested);

  m_listLayout->addWidget(w);
  m_widgets.insert(job.jobId, w);

  updateHeader();

  // Auto-adjust height based on job count (max 600px)
  adjustSize();

  m_service->downloadMedia(job);
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

void DownloadManagerDialog::updateHeader() {
  if (m_totalJobs == 0) {
    m_headerLabel->setText("No downloads yet");
    return;
  }
  QStringList parts;
  if (m_activeJobs > 0)
    parts << QString("%1 downloading").arg(m_activeJobs);
  if (m_finishedJobs > 0)
    parts << QString("%1 completed").arg(m_finishedJobs);
  const int other = m_totalJobs - m_activeJobs - m_finishedJobs;
  if (other > 0)
    parts << QString("%1 other").arg(other);
  m_headerLabel->setText(parts.join("  •  "));
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
  // percent == -1 → currently in post-processing phase (convert/embed)
  const QStringList parts = payload.split('\x1F');
  const QString fileName = parts.value(0);
  const QString speed = parts.value(1);
  const QString eta = parts.value(2);
  const QString phaseText = parts.value(3);
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
  w->setFinished(outputDir);
}

void DownloadManagerDialog::onDownloadError(const QString &jobId,
                                            const QString &errorMessage) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;
  if (w->state() == JobState::Downloading || w->state() == JobState::Queued) {
    --m_activeJobs;
    updateHeader();
  }
  w->setFailed(errorMessage);
}

void DownloadManagerDialog::onDownloadCancelled(const QString &jobId) {
  auto *w = m_widgets.value(jobId, nullptr);
  if (!w)
    return;
  if (w->state() == JobState::Downloading || w->state() == JobState::Queued) {
    --m_activeJobs;
    updateHeader();
  }
  w->setCancelled();
}

void DownloadManagerDialog::onCancelRequested(const QString &jobId) {
  m_service->cancelDownload(jobId);
}
