# VIBLI 🎵

Ứng dụng phát media chạy nền với overlay phủ đè lên màn hình, xây dựng bằng **C++17 + Qt 6**.

Hỗ trợ audio và video, đọc metadata tự động, system tray, mini player overlay luôn nằm trên cùng.

---

## Tính năng

- 🎵 Phát audio: `mp3 flac wav ogg aac m4a opus wma aiff ape wv dsf mka`
- 🎬 Phát video: `mp4 mkv avi mov webm wmv flv ts m2ts 3gp ogv vob`
- 🏷️ Đọc metadata tự động (title, artist, album) từ file
- 🖥️ Mini Player overlay – luôn nằm trên cùng, kéo di chuyển được
- 🔔 System Tray icon với context menu điều khiển nhanh
- 📋 Quản lý playlist (thêm, xóa, chuyển bài, double-click để phát)
- 🔀 Shuffle & Repeat (None / One / All)
- 🌙 Chạy nền – không hiện trên taskbar

---

## Yêu cầu cài đặt

### 1. Qt 6 (bắt buộc)

Tải về tại **https://www.qt.io/download-qt-installer**

Trong Qt Installer, chọn phiên bản Qt và tick các component sau:

| Component | Bắt buộc |
|---|---|
| Qt 6.x.x → MinGW 64-bit | ✅ |
| Qt 6.x.x → Qt Multimedia | ✅ |
| Qt 6.x.x → Qt Multimedia Widgets | ✅ |
| Developer and Designer Tools → MinGW 13.x 64-bit | ✅ |
| Developer and Designer Tools → CMake | ✅ |
| Developer and Designer Tools → Ninja | ✅ |

> Dự án này đang dùng Qt **6.11.0** cài tại `D:\data\qt`.
> Nếu bạn cài ở đường dẫn khác, cập nhật `CMAKE_PREFIX_PATH` trong `CMakePresets.json`.

### 2. Codec (khuyến nghị cho Windows)

Qt trên Windows dùng **Windows Media Foundation** làm backend. Để phát đầy đủ các định dạng như `.mkv`, `.flac`, `.opus`, `.ape`, cài thêm một trong hai:

- **LAV Filters** (nhẹ, khuyến nghị): https://github.com/Nevcairiel/LAVFilters/releases
- **K-Lite Codec Pack** (đầy đủ hơn): https://codecguide.com/download_kl.htm

### 3. VS Code Extensions

| Extension | ID |
|---|---|
| clangd | `llvm-vs-code-extensions.vscode-clangd` |
| CMake Tools | `ms-vscode.cmake-tools` |
| C/C++ Debug (GDB) | tùy IDE |

---

## Build & Chạy

### Cấu hình một lần (tạo build files)

```powershell
# Thêm MinGW và CMake vào PATH tạm
$env:PATH = "D:\data\qt\Tools\mingw1310_64\bin;D:\data\qt\Tools\Ninja;D:\data\qt\Tools\CMake_64\bin;$env:PATH"

# Configure (dùng preset mặc định – Debug)
cmake --preset default
```

### Build Debug

```powershell
$env:PATH = "D:\data\qt\Tools\mingw1310_64\bin;D:\data\qt\Tools\Ninja;D:\data\qt\Tools\CMake_64\bin;$env:PATH"

cmake --build build --parallel
```

Executable output: `build\VIBLI.exe`

### Chạy Debug

```powershell
$env:PATH = "D:\data\qt\6.11.0\mingw_64\bin;D:\data\qt\Tools\mingw1310_64\bin;$env:PATH"

.\build\VIBLI.exe
```

> Qt DLL phải có trong PATH khi chạy. Dòng trên thêm `mingw_64\bin` chứa toàn bộ Qt DLL.

### Build Release

```powershell
$env:PATH = "D:\data\qt\Tools\mingw1310_64\bin;D:\data\qt\Tools\Ninja;D:\data\qt\Tools\CMake_64\bin;$env:PATH"

# Configure release
cmake --preset release

# Build
cmake --build build --parallel
```

### Đóng gói Release (windeployqt)

Sau khi build release, chạy `windeployqt` để copy đủ Qt DLL vào thư mục:

```powershell
$env:PATH = "D:\data\qt\6.11.0\mingw_64\bin;D:\data\qt\Tools\mingw1310_64\bin;$env:PATH"

# Tạo thư mục deploy
mkdir deploy

# Copy exe
copy build\VIBLI.exe deploy\

# Deploy Qt dependencies
windeployqt --release --no-translations deploy\VIBLI.exe
```

Thư mục `deploy\` sau đó có thể zip và phân phối.

---

## Cấu trúc dự án

```
VIBLI/
├── CMakeLists.txt              # Build config
├── CMakePresets.json           # Preset debug/release với đường dẫn Qt
├── .clangd                     # Config cho clangd IntelliSense
├── .vscode/
│   ├── settings.json           # VS Code / Kiro settings
│   ├── tasks.json              # Build tasks
│   └── launch.json             # Debug launch config
├── src/
│   ├── main.cpp                # Entry point
│   ├── core/
│   │   ├── AudioPlayer.h/.cpp  # Engine phát media (Qt Multimedia)
│   │   └── PlaylistManager.h/.cpp  # Playlist + shuffle/repeat
│   ├── ui/
│   │   ├── OverlayWidget.h/.cpp    # Base overlay (frameless, always-on-top)
│   │   ├── MiniPlayer.h/.cpp       # Mini player overlay
│   │   └── MainWindow.h/.cpp       # Cửa sổ playlist + video output
│   └── tray/
│       └── TrayManager.h/.cpp      # System tray icon & menu
└── resources/
    └── resources.qrc           # Icons, assets
```

---

## Debug trong VS Code / Kiro

1. Mở Command Palette (`Ctrl+Shift+P`) → **CMake: Select Configure Preset** → chọn `default`
2. `Ctrl+Shift+P` → **CMake: Build**
3. Nhấn `F5` để chạy debug (dùng GDB qua extension C/C++ Debug)

Hoặc dùng task có sẵn: `Ctrl+Shift+B` → **CMake Build**

---

## Lưu ý

- App **không hiện cửa sổ khi khởi động** — chạy nền qua system tray. Nhìn vào góc phải taskbar để thấy icon.
- Click tray icon → toggle MiniPlayer overlay
- Double-click tray icon → mở cửa sổ quản lý playlist
- Khi phát file video, cửa sổ playlist tự mở và hiển thị video

---

## Roadmap

- [ ] Equalizer
- [ ] Stream URL / radio online
- [ ] Global hotkey
- [ ] Lưu/tải playlist M3U
- [ ] Lyrics trên overlay
- [ ] Album art từ metadata
- [ ] Tray icon thay đổi theo trạng thái phát
