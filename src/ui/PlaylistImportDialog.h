#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

/**
 * @brief Chế độ người dùng chọn khi đóng dialog.
 */
enum class ImportAction {
  ImportPlaylist, ///< Thêm playlist vào thư viện (OK)
  DownloadMp3,    ///< Tải xuống dạng MP3
  DownloadMp4,    ///< Tải xuống dạng MP4
};

/**
 * @brief PlaylistImportDialog – hộp thoại nhập URL YouTube playlist.
 *
 * Layout:
 *  - QLineEdit nhập URL
 *  - QLabel hiển thị lỗi inline (ẩn khi hợp lệ)
 *  - Nút OK (Import), Download MP3, Download MP4, và Cancel
 */
class PlaylistImportDialog : public QDialog {
  Q_OBJECT

public:
  explicit PlaylistImportDialog(QWidget *parent = nullptr);
  ~PlaylistImportDialog() override = default;

  QString playlistUrl() const;

  /** Trả về hành động người dùng đã chọn (chỉ hợp lệ khi exec() == Accepted).
   */
  ImportAction selectedAction() const { return m_selectedAction; }

private slots:
  void onUrlChanged(const QString &text);

private:
  void acceptWith(ImportAction action);

  QLineEdit *m_urlEdit;
  QLabel *m_errorLabel;
  QPushButton *m_okBtn;
  QPushButton *m_mp3Btn;
  QPushButton *m_mp4Btn;

  ImportAction m_selectedAction = ImportAction::ImportPlaylist;
};
