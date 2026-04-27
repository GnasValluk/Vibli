#include "PlaylistImportDialog.h"
#include "../core/PlaylistImporter.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

PlaylistImportDialog::PlaylistImportDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Import YouTube Playlist");
  setMinimumWidth(520);

  // ── Widgets ───────────────────────────────────────────────────────────
  auto *promptLabel = new QLabel("Nhập URL YouTube Playlist / Video:", this);

  m_urlEdit = new QLineEdit(this);
  m_urlEdit->setPlaceholderText("https://www.youtube.com/playlist?list=...");

  m_errorLabel = new QLabel(this);
  m_errorLabel->setObjectName("errorLabel");
  m_errorLabel->setWordWrap(true);
  m_errorLabel->hide();

  // Nút Import (OK)
  m_okBtn = new QPushButton("Import Playlist", this);
  m_okBtn->setEnabled(false);
  m_okBtn->setDefault(true);
  m_okBtn->setToolTip("Thêm playlist vào thư viện");

  // Nút Download MP3
  m_mp3Btn = new QPushButton("Download MP3", this);
  m_mp3Btn->setEnabled(false);
  m_mp3Btn->setObjectName("mp3Btn");
  m_mp3Btn->setToolTip("Tải xuống toàn bộ playlist dưới dạng MP3");

  // Nút Download MP4
  m_mp4Btn = new QPushButton("Download MP4", this);
  m_mp4Btn->setEnabled(false);
  m_mp4Btn->setObjectName("mp4Btn");
  m_mp4Btn->setToolTip("Tải xuống toàn bộ playlist dưới dạng MP4");

  auto *cancelBtn = new QPushButton("Hủy", this);

  // ── Layout ────────────────────────────────────────────────────────────
  auto *btnLayout = new QHBoxLayout;
  btnLayout->setSpacing(8);
  btnLayout->addStretch();
  btnLayout->addWidget(m_mp3Btn);
  btnLayout->addWidget(m_mp4Btn);
  btnLayout->addWidget(m_okBtn);
  btnLayout->addWidget(cancelBtn);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(8);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  mainLayout->addWidget(promptLabel);
  mainLayout->addWidget(m_urlEdit);
  mainLayout->addWidget(m_errorLabel);
  mainLayout->addLayout(btnLayout);

  // ── Style ─────────────────────────────────────────────────────────────
  setStyleSheet(R"(
        QDialog {
            background-color: #1e1e1e;
            color: #e0e0e0;
        }
        QLabel {
            color: #e0e0e0;
            font-size: 13px;
        }
        QLabel#errorLabel {
            color: #ff6b6b;
            font-size: 12px;
        }
        QLineEdit {
            background: #2a2a2a;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 6px 8px;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: #1db954;
        }
        QPushButton {
            background: #2a2a2a;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: #333333;
            border-color: #1db954;
        }
        QPushButton:disabled {
            color: #555555;
            border-color: #2a2a2a;
        }
        QPushButton#mp3Btn:enabled {
            border-color: #e67e22;
            color: #e67e22;
        }
        QPushButton#mp3Btn:enabled:hover {
            background: #2d2010;
            border-color: #f39c12;
            color: #f39c12;
        }
        QPushButton#mp4Btn:enabled {
            border-color: #3498db;
            color: #3498db;
        }
        QPushButton#mp4Btn:enabled:hover {
            background: #0d1e2d;
            border-color: #5dade2;
            color: #5dade2;
        }
    )");

  // ── Connections ───────────────────────────────────────────────────────
  connect(m_urlEdit, &QLineEdit::textChanged, this,
          &PlaylistImportDialog::onUrlChanged);
  connect(m_okBtn, &QPushButton::clicked, this,
          [this] { acceptWith(ImportAction::ImportPlaylist); });
  connect(m_mp3Btn, &QPushButton::clicked, this,
          [this] { acceptWith(ImportAction::DownloadMp3); });
  connect(m_mp4Btn, &QPushButton::clicked, this,
          [this] { acceptWith(ImportAction::DownloadMp4); });
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString PlaylistImportDialog::playlistUrl() const {
  return m_urlEdit->text().trimmed();
}

void PlaylistImportDialog::acceptWith(ImportAction action) {
  m_selectedAction = action;
  accept();
}

void PlaylistImportDialog::onUrlChanged(const QString &text) {
  const bool valid = PlaylistImporter::isValidPlaylistUrl(text.trimmed());
  m_okBtn->setEnabled(valid);
  m_mp3Btn->setEnabled(valid);
  m_mp4Btn->setEnabled(valid);

  if (text.trimmed().isEmpty()) {
    m_errorLabel->hide();
  } else if (!valid) {
    m_errorLabel->setText(
        PlaylistImporter::validationErrorMessage(text.trimmed()));
    m_errorLabel->show();
  } else {
    m_errorLabel->hide();
  }
}
