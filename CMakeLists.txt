cmake_minimum_required(VERSION 3.20)

project(VIBLI VERSION 1.0.1 LANGUAGES CXX)

# ── Version header (single source of truth) ───────────────────────────────────
# Sửa version CHỈ ở dòng project() bên trên — tất cả chỗ khác tự cập nhật
configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_BINARY_DIR}/version.h"
    @ONLY
)
# Inject version vào Inno Setup script
configure_file(
    "${CMAKE_SOURCE_DIR}/installer/vibli_setup.iss.in"
    "${CMAKE_SOURCE_DIR}/installer/vibli_setup.iss"
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Tìm Qt6 ──────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
    Network
)

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/core/AudioPlayer.cpp
    src/core/MediaCache.cpp
    src/core/PlaylistManager.cpp
    src/core/ThumbnailCache.cpp
    src/core/YtDlpService.cpp
    src/core/PlaylistImporter.cpp
    src/core/PlaylistPersistence.cpp
    src/ui/OverlayWidget.cpp
    src/ui/MiniPlayer.cpp
    src/ui/MainWindow.cpp
    src/ui/PlaylistModel.cpp
    src/ui/PlaylistDelegate.cpp
    src/ui/VideoThumbnail.cpp
    src/ui/PlaylistImportDialog.cpp
    src/ui/LoadingOverlay.cpp
    src/ui/LogViewerDialog.cpp
    src/tray/TrayManager.cpp
)

set(HEADERS
    src/core/AudioPlayer.h
    src/core/MediaCache.h
    src/core/PlaylistManager.h
    src/core/ThumbnailCache.h
    src/core/YtDlpService.h
    src/core/PlaylistImporter.h
    src/core/PlaylistPersistence.h
    src/core/LogService.h
    src/ui/OverlayWidget.h
    src/ui/MiniPlayer.h
    src/ui/MainWindow.h
    src/ui/PlaylistModel.h
    src/ui/PlaylistDelegate.h
    src/ui/VideoThumbnail.h
    src/ui/IconFont.h
    src/ui/PlaylistImportDialog.h
    src/ui/LoadingOverlay.h
    src/ui/RoundedImageWidget.h
    src/ui/LogViewerDialog.h
    src/tray/TrayManager.h
)

set(RESOURCES
    resources/resources.qrc
)

# Windows resource file – embed .ico vào exe (title bar, taskbar, Explorer)
if(WIN32)
    list(APPEND RESOURCES resources/app.rc)
endif()

# ── Executable ────────────────────────────────────────────────────────────────
if(WIN32)
    # Chạy nền, không hiện console window trên Windows
    add_executable(VIBLI WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
else()
    add_executable(VIBLI ${SOURCES} ${HEADERS} ${RESOURCES})
endif()

target_include_directories(VIBLI PRIVATE src ${CMAKE_BINARY_DIR})

# ── Copy font file cạnh exe ───────────────────────────────────────────────────
add_custom_command(TARGET VIBLI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/MaterialSymbolsRounded.ttf"
        "$<TARGET_FILE_DIR:VIBLI>/MaterialSymbolsRounded.ttf"
    COMMENT "Copying font file..."
)

# ── Copy yt_dlp folder cạnh exe ───────────────────────────────────────────────
add_custom_command(TARGET VIBLI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/yt_dlp"
        "$<TARGET_FILE_DIR:VIBLI>/yt_dlp"
    COMMENT "Copying yt_dlp folder..."
)

target_link_libraries(VIBLI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)

# ── Release optimizations ─────────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Link-time optimization — giảm binary size ~20-30%
    target_compile_options(VIBLI PRIVATE -flto -ffunction-sections -fdata-sections)
    target_link_options(VIBLI PRIVATE -flto -Wl,--gc-sections -s)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS VIBLI
    RUNTIME DESTINATION bin
)
