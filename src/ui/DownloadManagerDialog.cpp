#include "DownloadManagerDialog.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>

// ─────────────────────────────────────────────────────────────────────────────
// DownloadJobWidget
// ─────────────────────────────────────────────────────────────────────────────

DownloadJobWidget::DownloadJobWidget(const QString &jobId, const QString &label,
                                     QWidget *parent)
    : QWidget(parent), m_jobId(jobId) {
  setObjectName("jobWidget");

  m_titleLabel = new QLabel(label, this);
  m_titleLabel->setObjectName("jobTitle");

  m_fileLabel = new QLabel("Đang chuẩn bị...", this);
  m_fileLabel->setObjectName("jobFile");
  m_fileLabel->setWordWrap(false);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  m_progressBar->setTextVisible(true);
  m_progressBar->setFormat("%p%");

  m_statusLabel = new QLabel(this);
  m_statusLabel->setObjectName("jobStatus");
  m_statusLabel->hide();

  m_actionBtn = new QPushButton("Hủy", this);
  m_actionBtn->setObjectName("cancelBtn");
  m_actionBtn->setFixedWidth(100);

  auto *topRow = new QHBoxLayout;
  topRow->addWidget(m_titleLabel, 1);
  topRow->addWidget(m_actionBtn);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 10, 12, 10);
  layout->setSpacing(4);
  layout->addLayout(topRow);
  layout->addWidget(m_fileLabel);
  layout->addWidget(m_progressBar);
  layout->addWidget(m_statusLabel);

  connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
    if (m_outputDir.isEmpty()) {
      emit cancelRequested(m_jobId);
    } else {
      QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir));
    }
  });
}

void DownloadJobWidget::setProgress(int percent, const QString &currentFile) {
  m_progressBar->setValue(percent);
  if (!currentFile.isEmpty()) {
    // Truncate tên file dài
    const QString display =
        currentFile.length() > 60 ? "..." + currentFile.right(57) : currentFile;
    m_fileLabel->setText(display);
  }
}

void DownloadJobWidget::setFinished(const QString &outputDir) {
  m_outputDir = outputDir;
  m_progressBar->setValue(100);
  m_fileLabel->setText("Hoàn tất ✓");
  m_fileLabel->setObjectName("jobFileDone");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);

  m_actionBtn->setText("Mở thư mục");
  m_actionBtn->setObjectName("openBtn");
  m_actionBtn->style()->unpolish(m_actionBtn);
  m_actionBtn->style()->polish(m_actionBtn);
}

void DownloadJobWidget::setError(const QString &message) {
  m_progressBar->setValue(0);
  m_fileLabel->setText("Lỗi: " + message.left(80));
  m_fileLabel->setObjectName("jobFileError");
  m_fileLabel->style()->unpolish(m_fileLabel);
  m_fileLabel->style()->polish(m_fileLabel);

  m_actionBtn->setEnabled(false);
  m_actionBtn->setText("Thất bại");
}

// ─────────────────────────────────────────────────────────────────────────────
// DownloadManagerDialog
// ─────────────────────────────────────────────────────────────────────────────

DownloadManagerDialog::DownloadManagerDialog(YtDlpService *service,
                                             QWidget *parent)
    : QDialog(parent), m_service(service) {
  setWindowTitle("Download Manager");
  setMinimumSize(520, 300);
  resize(560, 400);
  // Non-modal: user vẫn dùng được app trong khi download
  setWindowFlags(windowFlags() | Qt::Window);

  // ── Inner list container ───────────────────────────────────────────────
  m_listContainer = new QWidget;
  m_listLayout = new QVBoxLayout(m_listContainer);
  m_listLayout->setContentsMargins(0, 0, 0, 0);
  m_listLayout->setSpacing(1);
  m_listLayout->addStretch();

  m_emptyLabel = new QLabel("Chưa có download nào.", m_listContainer);
  m_emptyLabel->setObjectName("emptyLabel");
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_listLayout->insertWidget(0, m_emptyLabel);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidget(m_listContainer);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->addWidget(scrollArea);

  // ── Style ──────────────────────────────────────────────────────────────
  setStyleSheet(R"(
    QDialog, QWidget {
        background-color: #1a1a1a;
        color: #e0e0e0;
    }
    QWidget#jobWidget {
        background: #242424;
        border-bottom: 1px solid #2e2e2e;
    }
    QLabel#jobTitle {
        font-size: 13px;
        font-weight: bold;
        color: #e0e0e0;
    }
    QLabel#jobFile {
        font-size: 11px;
        color: #888888;
    }
    QLabel#jobFileDone {
        font-size: 11px;
        color: #1db954;
    }
    QLabel#jobFileError {
        font-size: 11px;
        color: #ff6b6b;
    }
    QLabel#emptyLabel {
        color: #555555;
        font-size: 13px;
        padding: 40px;
    }
    QProgressBar {
        background: #2e2e2e;
        border: none;
        border-radius: 3px;
        height: 6px;
        text-align: center;
        font-size: 10px;
        color: #aaaaaa;
    }
    QProgressBar::chunk {
        background: #1db954;
        border-radius: 3px;
    }
    QPushButton#cancelBtn {
        background: #2a2a2a;
        color: #ff6b6b;
        border: 1px solid #3a3a3a;
        border-radius: 4px;
        padding: 4px 10px;
        font-size: 11px;
    }
    QPushButton#cancelBtn:hover {
        background: #3a1a1a;
        border-color: #ff6b6b;
    }
    QPushButton#openBtn {
        background: #2a2a2a;
        color: #1db954;
        border: 1px solid #1db954;
        border-radius: 4px;
        padding: 4px 10px;
        font-size: 11px;
    }
    QPushButton#openBtn:hover {
        background: #0d2a1a;
    }
    QScrollBar:vertical {
        background: #1a1a1a;
        width: 6px;
        border-radius: 3px;
    }
    QScrollBar::handle:vertical {
        background: #3a3a3a;
        border-radius: 3px;
        min-height: 20px;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0;
    }
  )");

  // ── Connect service signals ────────────────────────────────────────────
  connect(m_service, &YtDlpService::downloadProgress, this,
          &DownloadManagerDialog::onDownloadProgress);
  connect(m_service, &YtDlpService::downloadFinished, this,
          &DownloadManagerDialog::onDownloadFinished);
  connect(m_service, &YtDlpService::downloadError, this,
          &DownloadManagerDialog::onDownloadError);
}

void DownloadManagerDialog::addJob(const DownloadJob &job,
                                   const QString &displayLabel) {
  m_emptyLabel->hide();

  auto *w = new DownloadJobWidget(job.jobId, displayLabel, m_listContainer);
  connect(w, &DownloadJobWidget::cancelRequested, this,
          &DownloadManagerDialog::onCancelRequested);

  // Chèn trước stretch ở cuối
  m_listLayout->insertWidget(m_listLayout->count() - 1, w);
  m_widgets.insert(job.jobId, w);

  // Bắt đầu download
  m_service->downloadMedia(job);
}

// ── Slots
// ─────────────────────────────────────────────────────────────────────

void DownloadManagerDialog::onDownloadProgress(const QString &jobId,
                                               int percent,
                                               const QString &currentFile) {
  if (auto *w = m_widgets.value(jobId, nullptr))
    w->setProgress(percent, currentFile);
}

void DownloadManagerDialog::onDownloadFinished(const QString &jobId,
                                               const QString &outputDir) {
  if (auto *w = m_widgets.value(jobId, nullptr))
    w->setFinished(outputDir);
}

void DownloadManagerDialog::onDownloadError(const QString &jobId,
                                            const QString &errorMessage) {
  if (auto *w = m_widgets.value(jobId, nullptr))
    w->setError(errorMessage);
}

void DownloadManagerDialog::onCancelRequested(const QString &jobId) {
  m_service->cancelDownload(jobId);
}
