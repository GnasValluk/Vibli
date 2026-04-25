#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <optional>


#include "PlaylistManager.h"

/**
 * @brief PlaylistPersistence – lưu và khôi phục playlist vào file JSON cục bộ.
 *
 * Schema: { "version": 1, "tracks": [...] }
 * - Local track: { "type": "local", "url": "...", "title": "...", ... }
 * - YouTube track: { "type": "youtube", "videoId": "...", "title": "...", ... }
 *
 * Không serialize: url (StreamUrl) và thumbnail (QPixmap) của YouTubeTrack.
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
