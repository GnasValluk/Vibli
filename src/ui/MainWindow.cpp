#include "MainWindow.h"
#include "../core/LogService.h"
#include "IconFont.h"
#include "LogViewerDialog.h"
#include "PlaylistImportDialog.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTimer>
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
                       ThumbnailCache *thumbCache, QWidget *parent)
    : QMainWindow(parent), m_player(player), m_playlist(playlist),
      m_ytDlpService(ytDlpService), m_importer(importer),
      m_thumbCache(thumbCache) {
  setWindowTitle("VIBLI – Playlist");
  setMinimumSize(520, 420);
  setupUi();
  applyStyle();

  m_loadingOverlay = new LoadingOverlay(centralWidget());

  // ── Playlist signals ───────────────────────────────────────────────────
  // playlistChanged được xử lý bởi PlaylistModel trực tiếp
  connect(m_playlist, &PlaylistManager::currentTrackChanged, this,
          &MainWindow::onCurrentTrackChanged);
  connect(m_player, &AudioPlayer::metadataChanged, this,
          &MainWindow::onMetadataChanged);

  // ── YouTube import ─────────────────────────────────────────────────────
  connect(m_importer, &PlaylistImporter::importStarted, this, [this]() {
    m_loadingOverlay->start("Đang tải playlist...");
    m_statusLabel->setText("Đang tải playlist...");
    m_importYtBtn->setEnabled(false);
  });
  connect(
      m_importer, &PlaylistImporter::trackImported, this, [this](int loaded) {
        m_loadingOverlay->setProgress(loaded, -1);
        m_loadingOverlay->setMessage(QString("Đã tải %1 track...").arg(loaded));
        m_statusLabel->setText(QString("Đang tải... (%1 track)").arg(loaded));
      });
  connect(m_importer, &PlaylistImporter::importFinished, this,
          [this](int count) {
            m_loadingOverlay->setProgress(count, count);
            QTimer::singleShot(600, this, [this, count]() {
              m_loadingOverlay->stop();
              m_importYtBtn->setEnabled(true);
              const int n = m_playlist->count();
              m_statusLabel->setText(
                  QString("%1 item%2").arg(n).arg(n > 1 ? "s" : ""));
            });
          });
  connect(m_importer, &PlaylistImporter::importFailed, this,
          [this](const QString &reason) {
            m_loadingOverlay->stop();
            m_importYtBtn->setEnabled(true);
            m_statusLabel->setText("Lỗi import.");
            QMessageBox::critical(this, "Lỗi Import YouTube", reason);
          });
  connect(m_ytDlpService, &YtDlpService::progressUpdated, this,
          [this](const QString &msg) {
            if (m_loadingOverlay->isVisible())
              m_loadingOverlay->setMessage(msg);
          });

  // ── Log error dot ──────────────────────────────────────────────────────
  connect(&LogService::instance(), &LogService::errorLogged, this, [this]() {
    m_downloadLogBtn->setIcon(
        IconFont::icon(IconFont::DESCRIPTION, 14, QColor(255, 80, 80)));
    m_downloadLogBtn->setToolTip("⚠ Có lỗi – Click để xem log");
  });
  connect(&LogService::instance(), &LogService::logCleared, this, [this]() {
    m_downloadLogBtn->setIcon(
        IconFont::icon(IconFont::DESCRIPTION, 14, QColor(100, 100, 100)));
    m_downloadLogBtn->setToolTip("Xem log ứng dụng");
  });
  connect(m_player, &AudioPlayer::errorOccurred, this,
          [this](const QString &errorMsg) {
            if (m_playlist->currentTrack().isYouTube)
              m_statusLabel->setText("⚠ Lỗi phát: " + errorMsg.left(80));
          });
  connect(m_player, &AudioPlayer::recoverableErrorOccurred, this,
          [this](const QString &errorMsg) {
            // Chỉ log nhẹ ở status bar, không làm phiền user
            // Coordinator trong main.cpp sẽ tự retry
            if (m_playlist->currentTrack().isYouTube)
              m_statusLabel->setText("↻ Đang thử lại...");
            Q_UNUSED(errorMsg)
          });

  // ── Status label update khi playlist thay đổi ─────────────────────────
  connect(m_playlist, &PlaylistManager::playlistChanged, this, [this]() {
    const int n = m_playlist->count();
    m_statusLabel->setText(
        n == 0 ? "No tracks loaded"
               : QString("%1 item%2").arg(n).arg(n > 1 ? "s" : ""));
  });
}

void MainWindow::closeEvent(QCloseEvent *event) {
  hide();
  event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (m_loadingOverlay)
    m_loadingOverlay->resize(centralWidget()->size());
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

  // ── Model / View / Delegate ───────────────────────────────────────────
  m_playlistModel = new PlaylistModel(m_playlist, m_thumbCache, this);
  m_playlistDelegate = new PlaylistDelegate(m_thumbCache, this);

  m_playlistView = new QListView(central);
  m_playlistView->setObjectName("playlistView");
  m_playlistView->setModel(m_playlistModel);
  m_playlistView->setItemDelegate(m_playlistDelegate);
  m_playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_playlistView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_playlistView->setUniformItemSizes(
      true);                              // tất cả item cùng height → nhanh hơn
  m_playlistView->setMouseTracking(true); // để hover state hoạt động

  // ── Buttons ───────────────────────────────────────────────────────────
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
  m_importYtBtn->setIcon(
      IconFont::icon(IconFont::PLAY_ARROW, 16, QColor(255, 0, 0)));
  m_importYtBtn->setIconSize({16, 16});

  m_statusLabel = new QLabel("No tracks loaded", central);
  m_statusLabel->setObjectName("statusLabel");

  m_downloadLogBtn = new QPushButton(central);
  m_downloadLogBtn->setObjectName("logBtn");
  m_downloadLogBtn->setToolTip("Xem log ứng dụng");
  m_downloadLogBtn->setFixedSize(28, 28);
  m_downloadLogBtn->setIcon(
      IconFont::icon(IconFont::DESCRIPTION, 14, QColor(100, 100, 100)));
  m_downloadLogBtn->setIconSize({14, 14});

  auto *btnLayout = new QHBoxLayout;
  btnLayout->setSpacing(6);
  btnLayout->addWidget(m_addFolderBtn);
  btnLayout->addWidget(m_removeBtn);
  btnLayout->addWidget(m_clearBtn);
  btnLayout->addWidget(m_importYtBtn);
  btnLayout->addStretch();
  btnLayout->addWidget(m_downloadLogBtn);

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
  connect(m_downloadLogBtn, &QPushButton::clicked, this,
          &MainWindow::onDownloadLog);
  connect(m_playlistView, &QListView::doubleClicked, this,
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
    QListView#playlistView {
        background: #1e1e1e;
        border: 1px solid #2a2a2a;
        border-radius: 6px;
        font-size: 13px;
        color: #e0e0e0;
        outline: none;
    }
    QListView#playlistView::item {
        border-bottom: 1px solid #222222;
    }
    QListView#playlistView::item:selected {
        background: #1db954;
        color: #ffffff;
    }
    QListView#playlistView::item:hover {
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
    QPushButton#logBtn {
        background: transparent;
        border: 1px solid #2a2a2a;
        border-radius: 6px;
    }
    QPushButton#logBtn:hover {
        background: #2a2a2a;
        border-color: #555555;
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
    QScrollBar::handle:vertical:hover {
        background: #555555;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0;
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
  const QModelIndexList selected =
      m_playlistView->selectionModel()->selectedIndexes();
  // Xóa từ index lớn nhất xuống để không bị lệch index
  QList<int> rows;
  for (const QModelIndex &idx : selected)
    rows.append(idx.row());
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
  if (url.isEmpty())
    return;

  switch (dlg.selectedAction()) {
  case ImportAction::ImportPlaylist:
    m_importer->importPlaylist(url);
    break;
  case ImportAction::DownloadMp3:
    startDownload(url, DownloadFormat::Mp3);
    break;
  case ImportAction::DownloadMp4:
    startDownload(url, DownloadFormat::Mp4);
    break;
  }
}

void MainWindow::startDownload(const QString &url, DownloadFormat format) {
  // Chọn thư mục lưu
  const QString outputDir = QFileDialog::getExistingDirectory(
      this, "Chọn thư mục lưu file", QDir::homePath(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (outputDir.isEmpty())
    return;

  // Tạo job
  DownloadJob job;
  job.url = url;
  job.format = format;
  job.outputDir = outputDir;
  job.jobId = QString::number(QDateTime::currentMSecsSinceEpoch());

  // Label ngắn gọn: chỉ lấy phần cuối URL hoặc "YouTube Playlist"
  QString shortUrl = url;
  // Cắt bỏ scheme và www
  shortUrl.remove(QRegularExpression(
      R"(^https?://(www\.)?)", QRegularExpression::CaseInsensitiveOption));
  // Giới hạn 50 ký tự
  if (shortUrl.length() > 50)
    shortUrl = shortUrl.left(47) + "...";
  const QString label = shortUrl;

  // Lazy-create dialog (non-modal, tồn tại suốt vòng đời MainWindow)
  if (!m_downloadManager)
    m_downloadManager = new DownloadManagerDialog(m_ytDlpService, this);

  m_downloadManager->addJob(job, label);
  m_downloadManager->show();
  m_downloadManager->raise();
  m_downloadManager->activateWindow();
}

void MainWindow::onPlaylistItemDoubleClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;
  m_playlist->jumpTo(index.row());
  const Track &t = m_playlist->currentTrack();
  if (!t.isYouTube)
    m_player->play(t.url);
  // YouTube: coordinator trong main.cpp xử lý resolveStreamUrl
}

void MainWindow::onCurrentTrackChanged(int index, const Track &track) {
  // Highlight row đang phát
  const QModelIndex modelIdx = m_playlistModel->index(index);
  m_playlistView->setCurrentIndex(modelIdx);
  m_playlistView->scrollTo(modelIdx, QAbstractItemView::EnsureVisible);

  QString display;
  if (track.isYouTube) {
    display = "▶  " + track.title;
    if (!track.uploader.isEmpty())
      display += "  —  " + track.uploader;
  } else {
    display = track.artist.isEmpty() ? track.title
                                     : track.artist + " – " + track.title;
    display = (track.isVideo ? "🎬  " : "♪  ") + display;
  }
  m_nowPlayingLabel->setText(display);
}

void MainWindow::onMetadataChanged(const QString &title, const QString &artist,
                                   const QString &album) {
  const int idx = m_playlist->currentIndex();
  if (idx < 0)
    return;
  const Track t = m_playlist->trackAt(idx);
  if (t.isYouTube)
    return;

  // Cập nhật nowPlaying label
  const QString displayTitle = title.isEmpty() ? t.title : title;
  const QString displayArtist = artist.isEmpty() ? t.artist : artist;
  const QString display = displayArtist.isEmpty()
                              ? displayTitle
                              : displayArtist + " – " + displayTitle;
  m_nowPlayingLabel->setText((t.isVideo ? "🎬  " : "♪  ") + display);

  // Invalidate model row để delegate vẽ lại text
  const QModelIndex modelIdx = m_playlistModel->index(idx);
  emit m_playlistModel->dataChanged(modelIdx, modelIdx, {Qt::DisplayRole});
}

void MainWindow::onThumbnailReady(const QString &videoId) {
  // Delegate lấy từ ThumbnailCache trực tiếp — chỉ cần invalidate model row
  m_playlistModel->onThumbnailReady(videoId);
}

void MainWindow::onDownloadLog() {
  m_downloadLogBtn->setIcon(
      IconFont::icon(IconFont::DESCRIPTION, 14, QColor(100, 100, 100)));
  m_downloadLogBtn->setToolTip("Xem log ứng dụng");
  LogViewerDialog dlg(this);
  dlg.exec();
}

void MainWindow::onRetryYouTubeTrack() {
  const Track current = m_playlist->currentTrack();
  if (!current.isYouTube || current.videoId.isEmpty())
    return;
  m_ytDlpService->invalidateStreamCache(current.videoId);
  m_ytDlpService->resolveStreamUrl(current.videoId);
}
