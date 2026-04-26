# VIBLI

Trình phát nhạc/video chạy nền, có mini player overlay luôn nằm trên cùng màn hình. Hỗ trợ local files và YouTube playlist.

---

## Tính năng

- Phát audio/video local: `mp3 flac wav ogg aac m4a opus wma aiff ape` / `mp4 mkv avi mov webm wmv`
- Import YouTube playlist qua yt-dlp
- Mini Player overlay — kéo di chuyển, luôn trên cùng
- System Tray — điều khiển nhanh không cần mở cửa sổ
- Shuffle, Repeat (None / One / All)
- Tự động lưu và khôi phục playlist khi khởi động lại

---

## Yêu cầu

### Qt 6

Tải tại https://www.qt.io/download-qt-installer, cài các component:

| Component | |
|---|---|
| Qt 6.x → MinGW 64-bit | ✅ |
| Qt 6.x → Qt Multimedia + MultimediaWidgets | ✅ |
| Developer Tools → MinGW 13.x 64-bit | ✅ |
| Developer Tools → CMake + Ninja | ✅ |

> Dự án dùng Qt **6.11.0** cài tại `D:\data\qt`.  
> Nếu cài ở đường dẫn khác, sửa `CMAKE_PREFIX_PATH` trong `CMakePresets.json`.

### Codec (Windows)

Để phát đầy đủ định dạng (mkv, flac, opus...), cài một trong hai:
- **LAV Filters** (nhẹ): https://github.com/Nevcairiel/LAVFilters/releases
- **K-Lite Codec Pack**: https://codecguide.com/download_kl.htm

---

## Build Debug

```powershell
# Thêm tools vào PATH
$env:PATH = "D:\data\qt\Tools\mingw1310_64\bin;D:\data\qt\Tools\Ninja;D:\data\qt\Tools\CMake_64\bin;$env:PATH"

# Configure + Build
cmake --preset default
cmake --build build --parallel
```

Chạy:
```powershell
$env:PATH = "D:\data\qt\6.11.0\mingw_64\bin;D:\data\qt\Tools\mingw1310_64\bin;$env:PATH"
.\build\VIBLI.exe
```

---

## Build Release

```powershell
powershell -ExecutionPolicy Bypass -File tools\Release.ps1
```

Output:
- `deploy\VIBLI.exe` — portable, zip và gửi được ngay
- `dist\VIBLI_Setup_x.y.z.exe` — installer (cần [Inno Setup 6](https://jrsoftware.org/isdl.php))

Tùy chọn:
```powershell
-SkipBuild      # Chỉ repackage, không build lại
-SkipInstaller  # Không tạo installer
-Version "2.0.0"  # Override version
```

### Đổi version

Chỉ sửa **1 chỗ** trong `CMakeLists.txt`:
```cmake
project(VIBLI VERSION 1.2.0 LANGUAGES CXX)
```
CMake tự cập nhật version trong code, installer, và tên file output.

---

## Cấu trúc dự án

```
src/
├── main.cpp
├── core/
│   ├── AudioPlayer        # Engine phát media (Qt Multimedia)
│   ├── PlaylistManager    # Danh sách phát, shuffle/repeat
│   ├── YtDlpService       # Giao tiếp yt-dlp, resolve stream URL
│   ├── PlaylistImporter   # Import YouTube playlist
│   ├── MediaCache         # Cache thumbnail (disk) + stream URL (TTL 6h)
│   ├── ThumbnailCache     # LRU in-memory cache (tối đa 30 ảnh)
│   ├── PlaylistPersistence  # Lưu/khôi phục playlist (JSON)
│   └── LogService         # Logging có dedup
├── ui/
│   ├── MiniPlayer         # Overlay player
│   ├── MainWindow         # Cửa sổ playlist
│   ├── PlaylistModel      # QAbstractListModel cho playlist
│   ├── PlaylistDelegate   # Custom item renderer
│   └── ...
└── tray/
    └── TrayManager        # System tray icon & menu
```

---

## Lưu ý

- App **không hiện cửa sổ khi khởi động** — chạy nền qua system tray
- Click tray icon → toggle MiniPlayer
- Double-click tray icon → mở cửa sổ playlist
- Playlist được tự động lưu khi thoát và khôi phục khi mở lại
