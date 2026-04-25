cmake_minimum_required(VERSION 3.20)

project(VIBLI VERSION 1.0.0 LANGUAGES CXX)

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
    src/core/PlaylistManager.cpp
    src/core/YtDlpService.cpp
    src/core/PlaylistImporter.cpp
    src/core/PlaylistPersistence.cpp
    src/ui/OverlayWidget.cpp
    src/ui/MiniPlayer.cpp
    src/ui/MainWindow.cpp
    src/ui/VideoThumbnail.cpp
    src/ui/PlaylistImportDialog.cpp
    src/tray/TrayManager.cpp
)

set(HEADERS
    src/core/AudioPlayer.h
    src/core/PlaylistManager.h
    src/core/YtDlpService.h
    src/core/PlaylistImporter.h
    src/core/PlaylistPersistence.h
    src/ui/OverlayWidget.h
    src/ui/MiniPlayer.h
    src/ui/MainWindow.h
    src/ui/VideoThumbnail.h
    src/ui/IconFont.h
    src/ui/PlaylistImportDialog.h
    src/tray/TrayManager.h
)

set(RESOURCES
    resources/resources.qrc
)

# ── Executable ────────────────────────────────────────────────────────────────
if(WIN32)
    # Chạy nền, không hiện console window trên Windows
    add_executable(VIBLI WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
else()
    add_executable(VIBLI ${SOURCES} ${HEADERS} ${RESOURCES})
endif()

target_include_directories(VIBLI PRIVATE src)

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

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS VIBLI
    RUNTIME DESTINATION bin
)
