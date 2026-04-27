#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <optional>

#include "PlaylistManager.h"

/**
 * @brief PlaylistPersistence – saves and restores the playlist to/from a local
 * JSON file.
 *
 * Schema: { "version": 2, "tracks": [...] }
 * - Local track: { "type": "local", "url": "...", "title": "...", ... }
 * - YouTube track: { "type": "youtube", "videoId": "...", "title": "...",
 *                    "uploader": "...", "description": "...",
 *                    "viewCount": 0, "likeCount": 0, "uploadDate": "...",
 *                    "webpageUrl": "...", "thumbnailUrl": "..." }
 *
 * Not serialized: url (StreamUrl) and thumbnail (QPixmap) of YouTubeTrack.
 * Thumbnails are cached separately by MediaCache.
 */
class PlaylistPersistence : public QObject {
  Q_OBJECT

public:
  explicit PlaylistPersistence(QObject *parent = nullptr);
  ~PlaylistPersistence() override = default;

  void save(const QList<Track> &tracks);
  QList<Track> load();

  static QString persistencePath(); // %APPDATA%/VIBLI/playlist.json

private:
  QJsonObject trackToJson(const Track &track) const;
  std::optional<Track> trackFromJson(const QJsonObject &obj) const;
};
