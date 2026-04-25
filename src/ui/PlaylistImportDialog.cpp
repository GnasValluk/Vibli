#include "PlaylistImportDialog.h"
#include "../core/PlaylistImporter.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>


PlaylistImportDialog::PlaylistImportDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Import YouTube Playlist");
  setMinimumWidth(480);

  // ── Widgets ───────────────────────────────────────────────────────────
  auto *promptLabel = new QLabel("Nhập URL YouTube Playlist:", this);

  m_urlEdit = new QLineEdit(this);
  m_urlEdit->setPlaceholderText("https://www.youtube.com/playlist?list=...");

  m_errorLabel = new QLabel(this);
  m_errorLabel->setObjectName("errorLabel");
  m_errorLabel->setWordWrap(true);
  m_errorLabel->hide();

  m_okBtn = new QPushButton("OK", this);
  m_okBtn->setEnabled(false);
  m_okBtn->setDefault(true);

  auto *cancelBtn = new QPushButton("Hủy", this);

  // ── Layout ────────────────────────────────────────────────────────────
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
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
    )");

  // ── Connections ───────────────────────────────────────────────────────
  connect(m_urlEdit, &QLineEdit::textChanged, this,
          &PlaylistImportDialog::onUrlChanged);
  connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString PlaylistImportDialog::playlistUrl() const {
  return m_urlEdit->text().trimmed();
}

void PlaylistImportDialog::onUrlChanged(const QString &text) {
  const bool valid = PlaylistImporter::isValidPlaylistUrl(text.trimmed());
  m_okBtn->setEnabled(valid);

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
