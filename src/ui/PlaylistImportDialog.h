#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

/**
 * @brief Action selected by the user when closing the dialog.
 */
enum class ImportAction {
  ImportPlaylist, ///< Add playlist to library (OK)
  DownloadMp3,    ///< Download as MP3
  DownloadMp4,    ///< Download as MP4
};

/**
 * @brief PlaylistImportDialog – dialog for entering a YouTube playlist URL.
 *
 * Layout:
 *  - QLineEdit for URL input
 *  - QLabel for inline error display (hidden when valid)
 *  - Buttons: OK (Import), Download MP3, Download MP4, and Cancel
 */
class PlaylistImportDialog : public QDialog {
  Q_OBJECT

public:
  explicit PlaylistImportDialog(QWidget *parent = nullptr);
  ~PlaylistImportDialog() override = default;

  QString playlistUrl() const;

  /** Returns the action selected by the user (only valid when exec() ==
   * Accepted). */
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
