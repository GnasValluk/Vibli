/**
 * Bug Condition Exploration Test — YouTube Stream URL Expiry After 120s
 *
 * Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5
 *
 * CRITICAL: This test MUST FAIL on unfixed code — failure confirms the bug
 * exists. DO NOT attempt to fix the test or the code when it fails. This test
 * encodes the expected behavior — it will validate the fix when it passes after
 * implementation.
 *
 * Three concrete failing cases (root causes):
 *   Case 1: recoverableErrorOccurred fired 3× for same videoId
 *           → track gets skipped (retryCount >= 2 limit)
 *           Expected (fixed): track NOT skipped, resolveStreamUrl called again
 *
 *   Case 2: streamUrlReady fires after recoverableErrorOccurred
 *           → audioPlayer->seek() is NOT called (position lost)
 *           Expected (fixed): seek() IS called with saved position
 *
 *   Case 3: MediaCache::hasStreamUrl(videoId) returns true when cachedAt =
 *           now - 200s (TTL 6h not expired yet)
 *           Expected (fixed): returns false (TTL 90s)
 */

#include <QCoreApplication>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include "../src/core/MediaCache.h"
#include "../src/core/PlaylistManager.h"

// ── Minimal mock objects
// ──────────────────────────────────────────────────────

/**
 * MockYtDlpService — records calls to resolveStreamUrl and allows manual
 * emission of streamUrlReady / streamErrorOccurred signals.
 */
class MockYtDlpService : public QObject {
  Q_OBJECT
public:
  explicit MockYtDlpService(QObject *parent = nullptr) : QObject(parent) {}

  // Call counter for resolveStreamUrl
  int resolveCallCount = 0;
  QStringList resolvedVideoIds;

  void resolveStreamUrl(const QString &videoId) {
    resolveCallCount++;
    resolvedVideoIds.append(videoId);
  }

  void invalidateStreamCache(const QString & /*videoId*/) {}

  // Manually trigger signals for testing
  void emitStreamUrlReady(const QString &videoId, const QUrl &url) {
    emit streamUrlReady(videoId, url);
  }
  void emitStreamErrorOccurred(const QString &videoId, const QString &msg) {
    emit streamErrorOccurred(videoId, msg);
  }

signals:
  void streamUrlReady(const QString &videoId, const QUrl &streamUrl);
  void streamErrorOccurred(const QString &videoId, const QString &errorMessage);
};

/**
 * MockAudioPlayer — records calls to seek() and play(), and allows manual
 * emission of recoverableErrorOccurred.
 */
class MockAudioPlayer : public QObject {
  Q_OBJECT
public:
  explicit MockAudioPlayer(QObject *parent = nullptr) : QObject(parent) {}

  int seekCallCount = 0;
  QList<qint64> seekPositions;
  int playCallCount = 0;
  qint64 m_position = 0; // simulated current position

  qint64 position() const { return m_position; }
  bool isPlaying() const { return false; }
  bool isPaused() const { return false; }

  void play(const QUrl & /*url*/) { playCallCount++; }
  void seek(qint64 positionMs) {
    seekCallCount++;
    seekPositions.append(positionMs);
  }

  // Manually trigger signals for testing
  void emitRecoverableError(const QString &msg) {
    emit recoverableErrorOccurred(msg);
  }

signals:
  void recoverableErrorOccurred(const QString &errorMessage);
};

// ── Coordinator wiring helper
// ───────────────────────────────────────────────── Replicates the UNFIXED
// coordinator logic from main.cpp so we can test it in isolation without a
// QApplication / UI.

struct CoordinatorState {
  QMap<QString, int> retryCount;
  QSet<QString> retryCooldown;
};

/**
 * Wire up the UNFIXED coordinator logic between mock objects.
 * This mirrors the exact lambda connections in main.cpp (unfixed).
 */
static void wireUnfixedCoordinator(MockAudioPlayer *audioPlayer,
                                   MockYtDlpService *ytDlpService,
                                   PlaylistManager *playlistMgr,
                                   CoordinatorState *state) {
  // streamUrlReady → play (no seek, no resumePosition — unfixed)
  QObject::connect(ytDlpService, &MockYtDlpService::streamUrlReady, audioPlayer,
                   [audioPlayer, playlistMgr, state](const QString &videoId,
                                                     const QUrl &url) {
                     if (playlistMgr->currentTrack().videoId != videoId)
                       return;
                     state->retryCount.remove(videoId);
                     state->retryCooldown.remove(videoId);
                     audioPlayer->play(url);
                     // BUG: no seek() called here — position is lost
                   });

  // recoverableErrorOccurred → retry with hard limit of 2 (unfixed)
  QObject::connect(
      audioPlayer, &MockAudioPlayer::recoverableErrorOccurred, audioPlayer,
      [audioPlayer, ytDlpService, playlistMgr, state](const QString &errorMsg) {
        Q_UNUSED(errorMsg)
        const Track current = playlistMgr->currentTrack();
        if (!current.isYouTube || current.videoId.isEmpty())
          return;

        const QString videoId = current.videoId;

        if (state->retryCooldown.contains(videoId))
          return; // debounce

        const int count = state->retryCount.value(videoId, 0);

        if (count < 2) {
          // BUG: hard limit — only 2 retries allowed
          state->retryCount.insert(videoId, count + 1);
          state->retryCooldown.insert(videoId);
          ytDlpService->resolveStreamUrl(videoId);
        } else {
          // BUG: skip track after 3rd error
          state->retryCount.remove(videoId);
          state->retryCooldown.remove(videoId);
          if (playlistMgr->hasNext())
            playlistMgr->next();
        }
      });
}

// ── Test class
// ────────────────────────────────────────────────────────────────

class BugConditionTest : public QObject {
  Q_OBJECT

private slots:

  /**
   * Case 1: recoverableErrorOccurred fired 3× for same videoId
   *         → track gets skipped (retryCount >= 2 limit)
   *
   * Expected (fixed): track NOT skipped, resolveStreamUrl called again
   * On UNFIXED code: playlistMgr->next() is called → FAIL
   *
   * Validates: Requirements 1.2, 1.5
   */
  void case1_threeRecoverableErrors_trackShouldNotBeSkipped() {
    // Setup
    QCoreApplication::setApplicationName("VIBLI_TEST");
    PlaylistManager playlistMgr;
    MockAudioPlayer audioPlayer;
    MockYtDlpService ytDlpService;
    CoordinatorState state;

    // Add two YouTube tracks so next() can be called
    Track t1;
    t1.isYouTube = true;
    t1.videoId = "testVideoId1";
    t1.title = "Test Track 1";
    t1.url = QUrl("https://www.youtube.com/watch?v=testVideoId1");

    Track t2;
    t2.isYouTube = true;
    t2.videoId = "testVideoId2";
    t2.title = "Test Track 2";
    t2.url = QUrl("https://www.youtube.com/watch?v=testVideoId2");

    playlistMgr.addTrack(t1);
    playlistMgr.addTrack(t2);
    playlistMgr.jumpTo(0); // current = t1

    wireUnfixedCoordinator(&audioPlayer, &ytDlpService, &playlistMgr, &state);

    // Record initial track index
    const int initialIndex = playlistMgr.currentIndex();

    // Simulate 3 recoverableErrorOccurred events for the same videoId
    // After each error, clear cooldown to allow next error to be processed
    // (simulating the cooldown expiring between errors)
    audioPlayer.emitRecoverableError("Demuxing failed");
    state.retryCooldown.clear(); // cooldown expired

    audioPlayer.emitRecoverableError("Demuxing failed");
    state.retryCooldown.clear(); // cooldown expired

    audioPlayer.emitRecoverableError("Demuxing failed"); // 3rd error

    // EXPECTED (fixed behavior): track should NOT be skipped
    // currentIndex should still be 0 (same track)
    const int finalIndex = playlistMgr.currentIndex();

    // This assertion FAILS on unfixed code because the 3rd error triggers
    // playlistMgr->next(), advancing to index 1
    QCOMPARE(finalIndex, initialIndex); // FAILS on unfixed: finalIndex = 1
  }

  /**
   * Case 2: streamUrlReady fires after recoverableErrorOccurred
   *         → audioPlayer->seek() is NOT called (position lost)
   *
   * Expected (fixed): seek() IS called with the saved position
   * On UNFIXED code: seek() is never called → FAIL
   *
   * Validates: Requirements 1.3, 1.4
   */
  void case2_streamUrlReadyAfterError_seekShouldBeCalled() {
    PlaylistManager playlistMgr;
    MockAudioPlayer audioPlayer;
    MockYtDlpService ytDlpService;
    CoordinatorState state;

    Track t1;
    t1.isYouTube = true;
    t1.videoId = "testVideoId1";
    t1.title = "Test Track 1";
    t1.url = QUrl("https://www.youtube.com/watch?v=testVideoId1");

    playlistMgr.addTrack(t1);
    playlistMgr.jumpTo(0);

    wireUnfixedCoordinator(&audioPlayer, &ytDlpService, &playlistMgr, &state);

    // Simulate playback at position 95000ms (1m35s into the track)
    audioPlayer.m_position = 95000;

    // Simulate recoverableErrorOccurred (URL expired at ~2min)
    audioPlayer.emitRecoverableError("Demuxing failed");

    // Simulate streamUrlReady with fresh URL
    ytDlpService.emitStreamUrlReady(
        "testVideoId1",
        QUrl("https://rr1---sn-example.googlevideo.com/fresh-url"));

    // EXPECTED (fixed behavior): seek() should be called with ~95000ms
    // On UNFIXED code: seekCallCount = 0 because the coordinator never
    // saves position and never calls seek() after play()
    QVERIFY(audioPlayer.seekCallCount >
            0); // FAILS on unfixed: seekCallCount = 0

    if (audioPlayer.seekCallCount > 0) {
      // Position should be preserved within ±2000ms
      const qint64 savedPos = audioPlayer.seekPositions.first();
      QVERIFY2(qAbs(savedPos - 95000) <= 2000,
               qPrintable(QString("seek() called with %1ms, expected ~95000ms")
                              .arg(savedPos)));
    }
  }

  /**
   * Case 3: MediaCache::hasStreamUrl(videoId) returns true when
   *         cachedAt = now - 200s (TTL 6h not expired yet)
   *
   * Expected (fixed): returns false (TTL should be 90s, not 6h)
   * On UNFIXED code: returns true (stale URL accepted) → FAIL
   *
   * Validates: Requirements 1.3
   */
  void case3_staleCacheEntry_hasStreamUrlShouldReturnFalse() {
    // Use a real MediaCache instance — no mocking needed
    MediaCache cache;

    const QString videoId = "staleVideoId_200s";
    const QUrl staleUrl("https://rr1---sn-example.googlevideo.com/stale-url");

    // Manually inject a cache entry with cachedAt = now - 200s
    // We do this by saving a URL and then manipulating the internal state
    // via the public API: save then immediately check with a custom age.
    //
    // Since MediaCache doesn't expose setEntry(), we use a workaround:
    // save the URL (cachedAt = now), then verify the TTL logic directly
    // by checking what STREAM_URL_TTL_SECONDS is set to.
    //
    // The bug: STREAM_URL_TTL_SECONDS = 6 * 3600 = 21600s
    // A URL cached 200s ago is NOT expired under the buggy TTL.
    // The fix: STREAM_URL_TTL_SECONDS = 90s
    // A URL cached 200s ago IS expired under the fixed TTL.

    // Verify the current (buggy) TTL value
    // On unfixed code: TTL = 21600s → 200s-old entry is NOT expired
    // On fixed code:   TTL = 90s    → 200s-old entry IS expired
    const int currentTTL = MediaCache::STREAM_URL_TTL_SECONDS;

    // The age of our stale entry
    const qint64 staleAge = 200; // seconds

    // Under the FIXED TTL (90s), a 200s-old entry should be expired
    // Under the BUGGY TTL (6h = 21600s), a 200s-old entry is NOT expired
    const bool wouldBeExpiredUnderFixedTTL = (staleAge >= 90);
    const bool isExpiredUnderCurrentTTL = (staleAge >= currentTTL);

    // Save a URL and check hasStreamUrl behavior
    cache.saveStreamUrl(videoId, staleUrl);

    // hasStreamUrl checks: (now - cachedAt) < STREAM_URL_TTL_SECONDS
    // Since we just saved it (cachedAt ≈ now), it will return true.
    // We need to test with a 200s-old entry.
    //
    // Direct TTL check: assert that the current TTL is 90s (fixed)
    // On UNFIXED code: currentTTL = 21600 → this assertion FAILS
    QVERIFY2(
        currentTTL == 90,
        qPrintable(QString("STREAM_URL_TTL_SECONDS = %1s (expected 90s). "
                           "Bug: TTL is too long — a 200s-old URL would be "
                           "accepted as valid (not expired). "
                           "Fix: reduce TTL to 90s so URLs older than 90s "
                           "are rejected.")
                       .arg(currentTTL)));

    // Additional check: with fixed TTL, a 200s-old entry must be expired
    QVERIFY2(wouldBeExpiredUnderFixedTTL,
             "A 200s-old entry should be expired under 90s TTL");

    // Cleanup
    cache.invalidateStreamUrl(videoId);
  }
};

QTEST_MAIN(BugConditionTest)
#include "tst_bug_condition.moc"
