<div align="center">

<img src="resources/imgs/logo_256.png" width="72" alt="VIBLI" />

# VIBLI v1.0.1

**Release Date:** April 27, 2026

</div>

---

## 📦 Downloads

| Package | File | Description |
|---|---|---|
| 🗜️ **Portable** | `VIBLI_1.0.1_portable.zip` | Unzip and run — no installation needed |
| 🔧 **Installer** | `VIBLI_Setup_1.0.1.exe` | Standard Windows installer with Start Menu shortcut |

> **Recommended:** Use the Installer for a cleaner experience.  
> Use Portable if you want to run from a USB drive or keep everything in one folder.

---

## 🖥️ System Requirements

| | |
|---|---|
| **OS** | Windows 10 / 11 (64-bit) |
| **Runtime** | Bundled — no extra installs required |
| **Bundled tools** | yt-dlp `2026.02.04` · ffmpeg `8.0.1` (essentials build) |
| **Optional** | [LAV Filters](https://github.com/Nevcairiel/LAVFilters/releases) or [K-Lite Codec Pack](https://codecguide.com/download_kl.htm) for MKV / FLAC / OPUS playback |

---

## 🚀 Quick Start

### Portable
1. Download `VIBLI_1.0.1_portable.zip`
2. Extract to any folder
3. Run `VIBLI.exe`
4. Look for the **VIBLI icon in the system tray** — the app runs in the background

### Installer
1. Download `VIBLI_Setup_1.0.1.exe`
2. Run the installer and follow the steps
3. VIBLI launches automatically after install
4. Look for the **VIBLI icon in the system tray**

---

## ✨ What's in this release

### New — Download Manager
- **Download MP3 / MP4** directly from any YouTube playlist or video URL
- Live progress bar with download speed and ETA
- Automatic phase indicators: *Downloading → Converting → Embedding thumbnail → Writing metadata*
- Embedded thumbnail and full metadata (title, artist, date) in every downloaded file
- Cancel any download at any time — no false error messages

### New — Import Dialog
- Two new buttons alongside **Import Playlist**: **⬇ Download MP3** and **⬇ Download MP4**
- All three actions share the same URL input — no extra steps

### Improved — Log Viewer
- **Clear** now permanently deletes the log file on disk — reopening the viewer shows a clean slate
- Log icon in the toolbar resets after clearing

### Improved — Playlist
- **Clear Playlist** now saves immediately — playlist stays empty after restarting the app
- Stream URL cache is also cleared when the playlist is cleared

### Improved — Download status UI
- Progress bar switches to **indeterminate animation** during post-processing (ffmpeg convert / embed)
- Status text shows the current phase in plain language
- Dialog auto-sizes to fit the number of active jobs — no wasted empty space
- Job cards show format badge (MP3 / MP4), file name, speed, and ETA

---

## 🐛 Known Issues

- On some systems, MKV / FLAC / OPUS files may not play without [LAV Filters](https://github.com/Nevcairiel/LAVFilters/releases)
- YouTube stream URLs expire after ~6 hours; VIBLI will automatically re-resolve on next play

---

## 📋 Installation Notes

### Portable
- All data (playlist, cache, logs) is stored in `%APPDATA%\VIBLI\`
- To fully uninstall: delete the `VIBLI` folder and `%APPDATA%\VIBLI\`

### Installer
- Installs to `%LOCALAPPDATA%\Programs\VIBLI\` by default
- Adds a Start Menu shortcut
- Uninstall via **Settings → Apps** or the uninstaller in the install folder

---

<div align="center">
  <sub>Built with Qt 6.11.0 · yt-dlp 2026.02.04 · ffmpeg 8.0.1 · MinGW 13</sub>
</div>
