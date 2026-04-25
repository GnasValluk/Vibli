#include "MainWindow.h"
#include "IconFont.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

// ── Định dạng hỗ trợ ─────────────────────────────────────────────────────────
static const QSet<QString> AUDIO_EXTENSIONS = {
    "mp3","flac","wav","ogg","aac","m4a","opus","wma",
    "aiff","aif","ape","wv","dsf","dff","mka"
};
static const QSet<QString> VIDEO_EXTENSIONS = {
    "mp4","mkv","avi","mov","webm","wmv","flv",
    "ts","m2ts","m4v","3gp","ogv","vob"
};

MainWindow::MainWindow(AudioPlayer *player,
                       PlaylistManager *playlist,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_player(player)
    , m_playlist(playlist)
{
    setWindowTitle("VIBLI – Playlist");
    setMinimumSize(520, 420);
    setupUi();
    applyStyle();

    connect(m_playlist, &PlaylistManager::playlistChanged,
            this, &MainWindow::onPlaylistChanged);
    connect(m_playlist, &PlaylistManager::currentTrackChanged,
            this, &MainWindow::onCurrentTrackChanged);
    connect(m_player, &AudioPlayer::metadataChanged,
            this, &MainWindow::onMetadataChanged);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

// ── Setup UI ──────────────────────────────────────────────────────────────────

void MainWindow::setupUi()
{
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

    // Buttons
    m_addFolderBtn = new QPushButton("  Add Folder", central);
    m_removeBtn    = new QPushButton("  Remove",     central);
    m_clearBtn     = new QPushButton("  Clear",      central);
    m_addFolderBtn->setObjectName("actionBtn");
    m_removeBtn->setObjectName("actionBtn");
    m_clearBtn->setObjectName("actionBtn");

    const QColor btnColor(180, 180, 180);
    m_addFolderBtn->setIcon(IconFont::icon(IconFont::FOLDER_OPEN, 16, btnColor));
    m_addFolderBtn->setIconSize({16, 16});
    m_removeBtn->setIcon(IconFont::icon(IconFont::DELETE_ICON, 16, btnColor));
    m_removeBtn->setIconSize({16, 16});
    m_clearBtn->setIcon(IconFont::icon(IconFont::CLEAR_ALL, 16, btnColor));
    m_clearBtn->setIconSize({16, 16});

    m_statusLabel = new QLabel("No tracks loaded", central);
    m_statusLabel->setObjectName("statusLabel");

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(6);
    btnLayout->addWidget(m_addFolderBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addStretch();

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(m_nowPlayingLabel);
    mainLayout->addWidget(m_playlistView, 1);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(m_statusLabel);

    connect(m_addFolderBtn, &QPushButton::clicked, this, &MainWindow::onAddFolder);
    connect(m_removeBtn,    &QPushButton::clicked, this, &MainWindow::onRemoveSelected);
    connect(m_clearBtn,     &QPushButton::clicked, this, &MainWindow::onClearPlaylist);
    connect(m_playlistView, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onPlaylistItemDoubleClicked);
}

void MainWindow::applyStyle()
{
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

// ── Scan folder đệ quy ────────────────────────────────────────────────────────

void MainWindow::scanFolder(const QString &folderPath, QList<Track> &tracks)
{
    QDirIterator it(folderPath,
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString path = it.next();
        const QString ext  = QFileInfo(path).suffix().toLower();

        if (!AUDIO_EXTENSIONS.contains(ext) && !VIDEO_EXTENSIONS.contains(ext))
            continue;

        Track t;
        t.url     = QUrl::fromLocalFile(path);
        t.title   = QFileInfo(path).completeBaseName();
        t.isVideo = VIDEO_EXTENSIONS.contains(ext);
        tracks.append(t);
    }

    // Sắp xếp theo tên file
    std::sort(tracks.begin(), tracks.end(), [](const Track &a, const Track &b){
        return a.title.toLower() < b.title.toLower();
    });
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onAddFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this,
        "Select Media Folder",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (folder.isEmpty()) return;

    QList<Track> tracks;
    scanFolder(folder, tracks);

    if (tracks.isEmpty()) {
        m_statusLabel->setText("No supported media files found in folder.");
        return;
    }

    m_playlist->addTracks(tracks);
}

void MainWindow::onRemoveSelected()
{
    QList<int> rows;
    for (auto *item : m_playlistView->selectedItems())
        rows.append(m_playlistView->row(item));
    std::sort(rows.rbegin(), rows.rend());
    for (int row : rows)
        m_playlist->removeTrack(row);
}

void MainWindow::onClearPlaylist()
{
    m_playlist->clear();
    m_player->stop();
}

void MainWindow::onPlaylistItemDoubleClicked(QListWidgetItem *item)
{
    int row = m_playlistView->row(item);
    m_playlist->jumpTo(row);
    m_player->play(m_playlist->currentTrack().url);
}

void MainWindow::onPlaylistChanged()
{
    m_playlistView->clear();
    const QIcon audioIcon = IconFont::icon(IconFont::MUSIC_NOTE, 14, QColor(150,150,150));
    const QIcon videoIcon = IconFont::icon(IconFont::MOVIE,      14, QColor(150,150,150));
    for (const Track &t : m_playlist->tracks()) {
        QString display = t.artist.isEmpty()
                          ? t.title
                          : t.artist + " – " + t.title;
        auto *item = new QListWidgetItem(t.isVideo ? videoIcon : audioIcon, display);
        m_playlistView->addItem(item);
    }

    int count = m_playlist->count();
    m_statusLabel->setText(
        count == 0 ? "No tracks loaded"
                   : QString("%1 item%2").arg(count).arg(count > 1 ? "s" : ""));
}

void MainWindow::onCurrentTrackChanged(int index, const Track &track)
{
    m_playlistView->setCurrentRow(index);
    QString display = track.artist.isEmpty()
                      ? track.title
                      : track.artist + " – " + track.title;
    m_nowPlayingLabel->setText((track.isVideo ? "🎬  " : "♪  ") + display);
}

void MainWindow::onMetadataChanged(const QString &title,
                                   const QString &artist,
                                   const QString &album)
{
    int idx = m_playlist->currentIndex();
    if (idx < 0) return;

    Track t = m_playlist->trackAt(idx);
    if (!title.isEmpty())  t.title  = title;
    if (!artist.isEmpty()) t.artist = artist;
    if (!album.isEmpty())  t.album  = album;

    QString display = t.artist.isEmpty() ? t.title : t.artist + " – " + t.title;
    m_nowPlayingLabel->setText((t.isVideo ? "🎬  " : "♪  ") + display);

    if (auto *item = m_playlistView->item(idx)) {
        QString icon = t.isVideo ? "🎬" : "♪";
        item->setText(icon + "  " + display);
    }
}
