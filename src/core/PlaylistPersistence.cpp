#include "PlaylistPersistence.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>

PlaylistPersistence::PlaylistPersistence(QObject *parent) : QObject(parent) {}

// ── Static helpers
// ────────────────────────────────────────────────────────────

QString PlaylistPersistence::persistencePath() {
  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  // AppDataLocation already includes the app name on Windows (%APPDATA%/VIBLI)
  // but we add /VIBLI explicitly to be safe on all platforms
  return appData + "/playlist.json";
}

// ── Public API
// ────────────────────────────────────────────────────────────────

void PlaylistPersistence::save(const QList<Track> &tracks) {
  const QString path = persistencePath();
  QDir().mkpath(QFileInfo(path).absolutePath());

  QJsonArray arr;
  for (const Track &t : tracks) {
    arr.append(trackToJson(t));
  }

  QJsonObject root;
  root["version"] = 1;
  root["tracks"] = arr;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning("PlaylistPersistence: cannot open file for writing: %s",
             path.toUtf8().constData());
    return;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QList<Track> PlaylistPersistence::load() {
  const QString path = persistencePath();
  QFile file(path);
  if (!file.exists())
    return {};

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("PlaylistPersistence: cannot open file for reading: %s",
             path.toUtf8().constData());
    return {};
  }

  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    qWarning("PlaylistPersistence: corrupt JSON file (%s), starting with empty "
             "playlist.",
             err.errorString().toUtf8().constData());
    return {};
  }

  const QJsonObject root = doc.object();
  if (!root.contains("tracks") || !root["tracks"].isArray()) {
    qWarning("PlaylistPersistence: missing 'tracks' array, starting with empty "
             "playlist.");
    return {};
  }

  QList<Track> tracks;
  for (const QJsonValue &val : root["tracks"].toArray()) {
    if (!val.isObject())
      continue;
    auto t = trackFromJson(val.toObject());
    if (t.has_value()) {
      tracks.append(t.value());
    }
  }
  return tracks;
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

QJsonObject PlaylistPersistence::trackToJson(const Track &track) const {
  QJsonObject obj;
  if (track.isYouTube) {
    obj["type"] = "youtube";
    obj["videoId"] = track.videoId;
    obj["title"] = track.title;
    obj["durationMs"] = track.durationMs;
    obj["thumbnailUrl"] = track.thumbnailUrl;
    obj["uploader"] = track.uploader;
    obj["description"] = track.description;
    obj["viewCount"] = static_cast<double>(track.viewCount);
    obj["likeCount"] = static_cast<double>(track.likeCount);
    obj["uploadDate"] = track.uploadDate;
    obj["webpageUrl"] = track.webpageUrl;
    // Do NOT save url (StreamUrl) or thumbnail (QPixmap) — handled by
    // MediaCache
  } else {
    obj["type"] = "local";
    obj["url"] = track.url.toString();
    obj["title"] = track.title;
    obj["artist"] = track.artist;
    obj["album"] = track.album;
    obj["genre"] = track.genre;
    obj["durationMs"] = track.durationMs;
    obj["isVideo"] = track.isVideo;
  }
  return obj;
}

std::optional<Track>
PlaylistPersistence::trackFromJson(const QJsonObject &obj) const {
  const QString type = obj.value("type").toString();

  if (type == "youtube") {
    const QString videoId = obj.value("videoId").toString();
    const QString title = obj.value("title").toString();
    if (videoId.isEmpty() || title.isEmpty()) {
      qWarning("PlaylistPersistence: skipping youtube track with missing "
               "videoId or title");
      return std::nullopt;
    }
    Track t;
    t.isYouTube = true;
    t.videoId = videoId;
    t.title = title;
    t.durationMs = static_cast<qint64>(obj.value("durationMs").toDouble(0));
    t.thumbnailUrl = obj.value("thumbnailUrl").toString();
    t.uploader = obj.value("uploader").toString();
    t.artist = t.uploader;
    t.description = obj.value("description").toString();
    t.viewCount = static_cast<qint64>(obj.value("viewCount").toDouble(0));
    t.likeCount = static_cast<qint64>(obj.value("likeCount").toDouble(0));
    t.uploadDate = obj.value("uploadDate").toString();
    t.webpageUrl = obj.value("webpageUrl").toString();
    if (t.webpageUrl.isEmpty() && !videoId.isEmpty())
      t.webpageUrl = "https://www.youtube.com/watch?v=" + videoId;
    return t;

  } else if (type == "local") {
    const QString urlStr = obj.value("url").toString();
    if (urlStr.isEmpty()) {
      qWarning("PlaylistPersistence: skipping local track with missing url");
      return std::nullopt;
    }
    Track t;
    t.url = QUrl(urlStr);
    t.title = obj.value("title").toString();
    t.artist = obj.value("artist").toString();
    t.album = obj.value("album").toString();
    t.genre = obj.value("genre").toString();
    t.durationMs = static_cast<qint64>(obj.value("durationMs").toDouble(0));
    t.isVideo = obj.value("isVideo").toBool(false);
    return t;

  } else {
    qWarning("PlaylistPersistence: unknown track type '%s', skipping",
             type.toUtf8().constData());
    return std::nullopt;
  }
}
