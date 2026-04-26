#include "PlaylistManager.h"
#include <QRandomGenerator>
#include <algorithm>

PlaylistManager::PlaylistManager(QObject *parent) : QObject(parent) {}

// ── Playlist
// ──────────────────────────────────────────────────────────────────

void PlaylistManager::addTrack(const Track &track) {
  m_tracks.append(track);
  if (m_shuffle)
    rebuildShuffleOrder();
  emit playlistChanged();

  // Tự động chọn track đầu tiên
  if (m_currentIndex < 0)
    jumpTo(0);
}

void PlaylistManager::addTracks(const QList<Track> &tracks) {
  m_tracks.append(tracks);
  if (m_shuffle)
    rebuildShuffleOrder();
  emit playlistChanged();

  if (m_currentIndex < 0 && !m_tracks.isEmpty())
    jumpTo(0);
}

void PlaylistManager::removeTrack(int index) {
  if (index < 0 || index >= m_tracks.size())
    return;
  m_tracks.removeAt(index);
  if (m_shuffle)
    rebuildShuffleOrder();
  if (m_currentIndex >= m_tracks.size())
    m_currentIndex = m_tracks.size() - 1;
  emit playlistChanged();
}

void PlaylistManager::clear() {
  m_tracks.clear();
  m_shuffleOrder.clear();
  m_currentIndex = -1;
  emit playlistChanged();
}

int PlaylistManager::count() const { return m_tracks.size(); }

Track PlaylistManager::trackAt(int index) const {
  if (index < 0 || index >= m_tracks.size())
    return {};
  return m_tracks.at(index);
}

QList<Track> PlaylistManager::tracks() const { return m_tracks; }

// ── Navigation
// ────────────────────────────────────────────────────────────────

int PlaylistManager::currentIndex() const { return m_currentIndex; }

Track PlaylistManager::currentTrack() const { return trackAt(m_currentIndex); }

bool PlaylistManager::hasNext() const { return nextIndex() >= 0; }

bool PlaylistManager::hasPrevious() const { return previousIndex() >= 0; }

void PlaylistManager::next() {
  int idx = nextIndex();
  if (idx >= 0)
    jumpTo(idx);
}

void PlaylistManager::previous() {
  int idx = previousIndex();
  if (idx >= 0)
    jumpTo(idx);
}

void PlaylistManager::jumpTo(int index) {
  if (index < 0 || index >= m_tracks.size())
    return;
  m_currentIndex = index;
  emit currentTrackChanged(m_currentIndex, m_tracks.at(m_currentIndex));
}

// ── Modes
// ─────────────────────────────────────────────────────────────────────

bool PlaylistManager::isShuffle() const { return m_shuffle; }

PlaylistManager::RepeatMode PlaylistManager::repeatMode() const {
  return m_repeatMode;
}

void PlaylistManager::setShuffle(bool enabled) {
  m_shuffle = enabled;
  if (enabled)
    rebuildShuffleOrder();
}

void PlaylistManager::setRepeatMode(RepeatMode mode) { m_repeatMode = mode; }

// ── Private
// ───────────────────────────────────────────────────────────────────

void PlaylistManager::rebuildShuffleOrder() {
  m_shuffleOrder.clear();
  for (int i = 0; i < m_tracks.size(); ++i)
    m_shuffleOrder.append(i);

  // Fisher-Yates shuffle
  for (int i = m_shuffleOrder.size() - 1; i > 0; --i) {
    int j = static_cast<int>(QRandomGenerator::global()->bounded(i + 1));
    std::swap(m_shuffleOrder[i], m_shuffleOrder[j]);
  }
}

int PlaylistManager::nextIndex() const {
  if (m_tracks.isEmpty())
    return -1;

  if (m_repeatMode == RepeatMode::One)
    return m_currentIndex;

  if (m_shuffle) {
    int pos = m_shuffleOrder.indexOf(m_currentIndex);
    if (pos < 0)
      return -1;
    int nextPos = pos + 1;
    if (nextPos >= m_shuffleOrder.size()) {
      return (m_repeatMode == RepeatMode::All) ? m_shuffleOrder.first() : -1;
    }
    return m_shuffleOrder.at(nextPos);
  }

  int next = m_currentIndex + 1;
  if (next >= m_tracks.size()) {
    return (m_repeatMode == RepeatMode::All) ? 0 : -1;
  }
  return next;
}

int PlaylistManager::previousIndex() const {
  if (m_tracks.isEmpty())
    return -1;

  if (m_repeatMode == RepeatMode::One)
    return m_currentIndex;

  if (m_shuffle) {
    int pos = m_shuffleOrder.indexOf(m_currentIndex);
    if (pos <= 0)
      return -1;
    return m_shuffleOrder.at(pos - 1);
  }

  int prev = m_currentIndex - 1;
  return (prev < 0) ? -1 : prev;
}
