#include "MainWindow.h"
#include "IconFont.h"
#include "PlaylistImportDialog.h"

#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QVBoxLayout>


// ── Định dạng hỗ trợ ─────────────────────────────────────────────────────────
static const QSet<QString> AUDIO_EXTENSIONS = {
    "mp3",  "flac", "wav", "ogg", "aac", "m4a", "opus", "wma",
    "aiff", "aif",  "ape", "wv",  "dsf", "dff", "mka"};
static const QSet<QString> VIDEO_EXTENSIONS = {
    "mp4", "mkv",  "avi", "mov", "webm", "wmv", "flv",
    "ts",  "m2ts", "m4v", "3gp", "ogv",  "vob"};

MainWindow::MainWindow(AudioPlayer *player, PlaylistManager *playlist,
                       YtDlpService *ytDlpService, PlaylistImporter *importer,
                       QWidget *parent)
    : QMainWindow(parent), m_player(player), m_playlist(playlist),
      m_ytDlpService(ytDlpService), m_importer(importer) {
  setWindowTitle("VIBLI – Playlist");
  setMinimumSize(520, 420);
  setupUi();
  applyStyle();

  connect(m_playlist, &PlaylistManager::playlistChanged, this,
          &MainWindow::onPlaylistChanged);
  connect(m_playlist, &PlaylistManager::currentTrackChanged, this,
          &MainWindow::onCurrentTrackChanged);
  connect(m_player, &AudioPlayer::metadataChanged, this,
          &MainWindow::onMetadataChanged);

  // ── YouTube import connections ─────────────────────────────────────────
  connect(m_importer, &PlaylistImporter::importStarted, this,
          [this]() { m_statusLabel->setText("Đang tải playlist..."); });
  connect(m_importer, &PlaylistImporter::importFinished, this,
          [this](int count) {
            m_statusLabel->setText(
                QString("Đã thêm %1 track từ YouTube.").arg(count));
            onPlaylistChanged();
          });
  connect(m_importer, &PlaylistImporter::importFailed, this,
          [this](const QString &reason) {
            m_statusLabel->setText("Lỗi import.");
            QMessageBox::critical(this, "Lỗi Import YouTube", reason);
          });
  connect(m_importer, &PlaylistImporter::thumbnailLoaded, this,
          &MainWindow::onThumbnailLoaded);

  // ── AudioPlayer error → retry for YouTube tracks ───────────────────────
  connect(m_player, &AudioPlayer::errorOccurred, this,
          [this](const QString &errorMsg) {
            const Track current = m_playlist->currentTrack();
            if (!current.isYouTube)
              return;

            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Lỗi phát YouTube");
            msgBox.setText("Không thể phát track YouTube: " + current.title);
            msgBox.setInformativeText(errorMsg);
            QPushButton *retryBtn =
                msgBox.addButton("Thử lại", QMessageBox::AcceptRole);
            msgBox.addButton("Bỏ qua", QMessageBox::RejectRole);
            msgBox.exec();

            if (msgBox.clickedButton() == retryBtn) {
              onRetryYouTubeTrack();
            }
          });
}

void MainWindow::closeEvent(QCloseEvent *event) {
  hide();
  event->ignore();
}

// ── Setup UI
// ──────────────────────────────────────────────────────────────────

void MainWindow::setupUi() {
  auto *central = new QWidget(this);
  setCentralWidget(central);

  // Now playing
  m_nowPlayingLabel = new QLabel("♪  No track playing", central);
  m_nowPlayingLabel->setObjectName("nowPlayingLabel");
  m_nowPlayingLabel->setAlignment(Qt::AlignCenter);

  // Playlist
  m_playlistView = new QListWidget(central);
  m_playlistView->setObjectName("playlistView");
  m_playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_playlistView->setIconSize(QSize(48, 48));

  // Buttons
  m_addFolderBtn = new QPushButton("  Add Folder", central);
  m_removeBtn = new QPushButton("  Remove", central);
  m_clearBtn = new QPushButton("  Clear", central);
  m_importYtBtn = new QPushButton("  Import YouTube", central);
  m_addFolderBtn->setObjectName("actionBtn");
  m_removeBtn->setObjectName("actionBtn");
  m_clearBtn->setObjectName("actionBtn");
  m_importYtBtn->setObjectName("actionBtn");

  const QColor btnColor(180, 180, 180);
  m_addFolderBtn->setIcon(IconFont::icon(IconFont::FOLDER_OPEN, 16, btnColor));
  m_addFolderBtn->setIconSize({16, 16});
  m_removeBtn->setIcon(IconFont::icon(IconFont::DELETE_ICON, 16, btnColor));
  m_removeBtn->setIconSize({16, 16});
  m_clearBtn->setIcon(IconFont::icon(IconFont::CLEAR_ALL, 16, btnColor));
  m_clearBtn->setIconSize({16, 16});
  // YouTube button – use a simple text icon or material symbol if available
  m_importYtBtn->setIcon(
      IconFont::icon(IconFont::PLAY_ARROW, 16, QColor(255, 0, 0)));
  m_importYtBtn->setIconSize({16, 16});

  m_statusLabel = new QLabel("No tracks loaded", central);
  m_statusLabel->setObjectName("statusLabel");

  auto *btnLayout = new QHBoxLayout;
  btnLayout->setSpacing(6);
  btnLayout->addWidget(m_addFolderBtn);
  btnLayout->addWidget(m_removeBtn);
  btnLayout->addWidget(m_clearBtn);
  btnLayout->addWidget(m_importYtBtn);
  btnLayout->addStretch();

  auto *mainLayout = new QVBoxLayout(central);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(8);
  mainLayout->addWidget(m_nowPlayingLabel);
  mainLayout->addWidget(m_playlistView, 1);
  mainLayout->addLayout(btnLayout);
  mainLayout->addWidget(m_statusLabel);

  connect(m_addFolderBtn, &QPushButton::clicked, this,
          &MainWindow::onAddFolder);
  connect(m_removeBtn, &QPushButton::clicked, this,
          &MainWindow::onRemoveSelected);
  connect(m_clearBtn, &QPushButton::clicked, this,
          &MainWindow::onClearPlaylist);
  connect(m_importYtBtn, &QPushButton::clicked, this,
          &MainWindow::onImportYouTube);
  connect(m_playlistView, &QListWidget::itemDoubleClicked, this,
          &MainWindow::onPlaylistItemDoubleClicked);
}

void MainWindow::applyStyle() {
  setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #121212;
            color: #e0e0e0;
        }
        QLabel#nowPlayingLabel {
            color: #1db954;
            font-size: 13px;
            font-weight: bold;
            padding: 6px;
            background: #1a1a1a;
            border-radius: 6px;
        }
        QListWidget#playlistView {
            background: #1e1e1e;
            border: 1px solid #2a2a2a;
            border-radius: 6px;
            font-size: 13px;
            color: #e0e0e0;
            outline: none;
        }
        QListWidget#playlistView::item {
            padding: 5px 10px;
            border-bottom: 1px solid #222222;
            min-height: 52px;
        }
        QListWidget#playlistView::item:selected {
            background: #1db954;
            color: #ffffff;
        }
        QListWidget#playlistView::item:hover {
            background: #2a2a2a;
        }
        QPushButton#actionBtn {
            background: #2a2a2a;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            border-radius: 6px;
            padding: 6px 16px;
            font-size: 12px;
        }
        QPushButton#actionBtn:hover {
            background: #333333;
            border-color: #1db954;
            color: #ffffff;
        }
        QLabel#statusLabel {
            color: #555555;
            font-size: 11px;
            padding: 2px;
        }
    )");
}

// ── Helpers
// ───────────────────────────────────────────────────────────────────

QString MainWindow::formatDuration(qint64 ms) const {
  if (ms <= 0)
    return "0:00";
  const qint64 s = ms / 1000;
  return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

// ── Scan folder đệ quy
// ────────────────────────────────────────────────────────

void MainWindow::scanFolder(const QString &folderPath, QList<Track> &tracks) {
  QDirIterator it(folderPath, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    const QString path = it.next();
    const QString ext = QFileInfo(path).suffix().toLower();

    if (!AUDIO_EXTENSIONS.contains(ext) && !VIDEO_EXTENSIONS.contains(ext))
      continue;

    Track t;
    t.url = QUrl::fromLocalFile(path);
    t.title = QFileInfo(path).completeBaseName();
    t.isVideo = VIDEO_EXTENSIONS.contains(ext);
    tracks.append(t);
  }

  // Sắp xếp theo tên file
  std::sort(tracks.begin(), tracks.end(), [](const Track &a, const Track &b) {
    return a.title.toLower() < b.title.toLower();
  });
}

// ── Slots
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onAddFolder() {
  const QString folder = QFileDialog::getExistingDirectory(
      this, "Select Media Folder", QDir::homePath(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (folder.isEmpty())
    return;

  QList<Track> tracks;
  scanFolder(folder, tracks);

  if (tracks.isEmpty()) {
    m_statusLabel->setText("No supported media files found in folder.");
    return;
  }

  m_playlist->addTracks(tracks);
}

void MainWindow::onRemoveSelected() {
  QList<int> rows;
  for (auto *item : m_playlistView->selectedItems())
    rows.append(m_playlistView->row(item));
  std::sort(rows.rbegin(), rows.rend());
  for (int row : rows)
    m_playlist->removeTrack(row);
}

void MainWindow::onClearPlaylist() {
  m_playlist->clear();
  m_player->stop();
}

void MainWindow::onImportYouTube() {
  PlaylistImportDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString url = dlg.playlistUrl();
  if (!url.isEmpty()) {
    m_importer->importPlaylist(url);
  }
}

void MainWindow::onPlaylistItemDoubleClicked(QListWidgetItem *item) {
  int row = m_playlistView->row(item);
  m_playlist->jumpTo(row);
  const Track &t = m_playlist->currentTrack();
  if (!t.isYouTube) {
    m_player->play(t.url);
  }
  // YouTube tracks: coordinator in main.cpp handles resolveStreamUrl
}

void MainWindow::onPlaylistChanged() {
  m_playlistView->clear();
  const QIcon audioIcon =
      IconFont::icon(IconFont::MUSIC_NOTE, 14, QColor(150, 150, 150));
  const QIcon videoIcon =
      IconFont::icon(IconFont::MOVIE, 14, QColor(150, 150, 150));
  const QIcon ytIcon =
      IconFont::icon(IconFont::PLAY_ARROW, 14, QColor(255, 80, 80));

  const QList<Track> &tracks = m_playlist->tracks();
  for (int i = 0; i < tracks.size(); ++i) {
    const Track &t = tracks.at(i);

    QString display;
    QIcon icon;

    if (t.isYouTube) {
      icon = ytIcon;
      const QString dur = formatDuration(t.durationMs);
      display = "[YT] " + t.title + "  (" + dur + ")";
    } else {
      icon = t.isVideo ? videoIcon : audioIcon;
      display = t.artist.isEmpty() ? t.title : t.artist + " – " + t.title;
    }

    auto *item = new QListWidgetItem(icon, display);
    item->setData(Qt::UserRole,
                  t.videoId); // store videoId for thumbnail lookup

    // Show thumbnail if available
    if (t.isYouTube && !t.thumbnail.isNull()) {
      item->setIcon(QIcon(t.thumbnail.scaled(48, 48, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation)));
    }

    m_playlistView->addItem(item);
  }

  int count = m_playlist->count();
  m_statusLabel->setText(
      count == 0 ? "No tracks loaded"
                 : QString("%1 item%2").arg(count).arg(count > 1 ? "s" : ""));
}

void MainWindow::onCurrentTrackChanged(int index, const Track &track) {
  m_playlistView->setCurrentRow(index);

  QString display;
  if (track.isYouTube) {
    display = "▶  [YT] " + track.title;
  } else {
    display = track.artist.isEmpty() ? track.title
                                     : track.artist + " – " + track.title;
    display = (track.isVideo ? "🎬  " : "♪  ") + display;
  }
  m_nowPlayingLabel->setText(display);
}

void MainWindow::onMetadataChanged(const QString &title, const QString &artist,
                                   const QString &album) {
  int idx = m_playlist->currentIndex();
  if (idx < 0)
    return;

  Track t = m_playlist->trackAt(idx);
  if (t.isYouTube)
    return; // Don't override YouTube track metadata

  if (!title.isEmpty())
    t.title = title;
  if (!artist.isEmpty())
    t.artist = artist;
  if (!album.isEmpty())
    t.album = album;

  QString display = t.artist.isEmpty() ? t.title : t.artist + " – " + t.title;
  m_nowPlayingLabel->setText((t.isVideo ? "🎬  " : "♪  ") + display);

  if (auto *item = m_playlistView->item(idx)) {
    QString icon = t.isVideo ? "🎬" : "♪";
    item->setText(icon + "  " + display);
  }
}

void MainWindow::onThumbnailLoaded(const QString &videoId,
                                   const QPixmap &pixmap) {
  // Find the list item with matching videoId and update its icon
  for (int i = 0; i < m_playlistView->count(); ++i) {
    QListWidgetItem *item = m_playlistView->item(i);
    if (item && item->data(Qt::UserRole).toString() == videoId) {
      item->setIcon(QIcon(pixmap.scaled(48, 48, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation)));
      break;
    }
  }
}

void MainWindow::onRetryYouTubeTrack() {
  const Track current = m_playlist->currentTrack();
  if (!current.isYouTube || current.videoId.isEmpty())
    return;

  m_ytDlpService->invalidateStreamCache(current.videoId);
  m_ytDlpService->resolveStreamUrl(current.videoId);
}
