#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>


/**
 * @brief PlaylistImportDialog – hộp thoại nhập URL YouTube playlist.
 *
 * Layout:
 *  - QLineEdit nhập URL
 *  - QLabel hiển thị lỗi inline (ẩn khi hợp lệ)
 *  - Nút OK (disabled cho đến khi URL hợp lệ) và Cancel
 */
class PlaylistImportDialog : public QDialog {
  Q_OBJECT

public:
  explicit PlaylistImportDialog(QWidget *parent = nullptr);
  ~PlaylistImportDialog() override = default;

  QString playlistUrl() const;

private slots:
  void onUrlChanged(const QString &text);

private:
  QLineEdit *m_urlEdit;
  QLabel *m_errorLabel;
  QPushButton *m_okBtn;
};
