#pragma once

#include <QDialog>
#include <QLabel>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "../core/YtDlpService.h"

/**
 * @brief Widget hiển thị một download job trong danh sách.
 *
 * Gồm: tên job, tên file đang tải, progress bar, nút Cancel / Mở thư mục.
 */
class DownloadJobWidget : public QWidget {
  Q_OBJECT
public:
  explicit DownloadJobWidget(const QString &jobId, const QString &label,
                             QWidget *parent = nullptr);

  void setProgress(int percent, const QString &currentFile);
  void setFinished(const QString &outputDir);
  void setError(const QString &message);

signals:
  void cancelRequested(const QString &jobId);

private:
  QString m_jobId;
  QString m_outputDir;

  QLabel *m_titleLabel;
  QLabel *m_fileLabel;
  QProgressBar *m_progressBar;
  QPushButton *m_actionBtn; ///< "Hủy" khi đang chạy, "Mở thư mục" khi xong
  QLabel *m_statusLabel;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief DownloadManagerDialog – dialog non-modal quản lý các download job.
 *
 * Kết nối với YtDlpService để nhận tiến trình và hiển thị cho user.
 * Có thể mở nhiều job cùng lúc (xếp hàng trong YtDlpService).
 */
class DownloadManagerDialog : public QDialog {
  Q_OBJECT

public:
  explicit DownloadManagerDialog(YtDlpService *service,
                                 QWidget *parent = nullptr);
  ~DownloadManagerDialog() override = default;

  /**
   * Thêm một job mới vào dialog và bắt đầu download.
   * @param job  Job đã được điền đầy đủ thông tin (kể cả jobId).
   * @param displayLabel  Tên hiển thị cho user (ví dụ: "Playlist – MP3").
   */
  void addJob(const DownloadJob &job, const QString &displayLabel);

private slots:
  void onDownloadProgress(const QString &jobId, int percent,
                          const QString &currentFile);
  void onDownloadFinished(const QString &jobId, const QString &outputDir);
  void onDownloadError(const QString &jobId, const QString &errorMessage);
  void onCancelRequested(const QString &jobId);

private:
  YtDlpService *m_service;

  QWidget *m_listContainer;
  QVBoxLayout *m_listLayout;
  QLabel *m_emptyLabel;

  QMap<QString, DownloadJobWidget *> m_widgets; ///< jobId → widget
};
