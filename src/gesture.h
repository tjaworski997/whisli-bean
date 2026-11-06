#pragma once
// Turns the frames of a remote into gestures. Free of Arduino, so it can be tested on a computer:
// see test/gesture_test.cpp.
//
// A remote repeats its frame while a button is down, and many send a few frames even for a tap, so
// a press ends only after a pause. Buttons work in one of two ways:
//   without gestures: "short" at the first frame, with no delay, once per press
//   with gestures:    "short" on release, or "long" after LONG_MS: once, or repeated every REPEAT_MS
#include <stdint.h>
#include <string.h>

enum class Gesture : uint8_t { shortPress, longPress };

inline const char *gestureName(Gesture gesture) {
  return gesture == Gesture::shortPress ? "short" : "long";
}

inline bool gestureFromName(const char *name, Gesture &out) {
  for (Gesture g : {Gesture::shortPress, Gesture::longPress}) {
    if (!strcmp(name, gestureName(g))) {
      out = g;
      return true;
    }
  }
  return false;
}

class GestureTracker {
 public:
  using Emit = void (*)(const char *code, Gesture gesture, bool repeat);

  static constexpr uint32_t RELEASE_MS = 250;  // above the longest gap of a tap, about 165 ms
  static constexpr uint32_t LONG_MS = 800;     // a tap lasts up to about 350 ms on remotes sending 3 frames
  static constexpr uint32_t REPEAT_MS = 300;
  // Once a long press has fired, a few lost frames must not count as a release and fire it again.
  static constexpr uint32_t LONG_RELEASE_MS = 800;
  // Some remotes send a second, different frame per press: it must not end the known button.
  static constexpr uint32_t SECOND_FRAME_MS = 400;

  // A frame with a code. known: the code is saved; gestures: it tells long presses apart;
  // repeat: its long press repeats while the button is down, instead of acting once.
  void frame(const char *code, bool known, bool gestures, bool repeat, uint32_t now, Emit emit) {
    if (active && strcmp(code, activeCode)) {
      if (activeKnown && !known && now - lastAt < SECOND_FRAME_MS) return;
      finish(emit);
    }
    if (active) return advance(now, emit);
    active = true;
    strncpy(activeCode, code, sizeof(activeCode) - 1);
    activeCode[sizeof(activeCode) - 1] = '\0';
    activeKnown = known;
    activeGestures = gestures;
    activeRepeat = repeat;
    longDone = false;
    startedAt = lastAt = now;
    if (!gestures) emit(activeCode, Gesture::shortPress, false);
  }

  // A repeat frame without a code of its own: it can only continue the current press.
  void repeat(uint32_t now, Emit emit) {
    if (active) advance(now, emit);
  }

  void tick(uint32_t now, Emit emit) {
    if (active && now - lastAt >= (longDone ? LONG_RELEASE_MS : RELEASE_MS)) finish(emit);
  }

 private:
  void advance(uint32_t now, Emit emit) {
    lastAt = now;
    if (!activeGestures) return;
    // Decided on arriving frames, not on the clock: the button is known to be down only then.
    if (!longDone) {
      if (now - startedAt < LONG_MS) return;
      longDone = true;
      nextRepeatAt = now + REPEAT_MS;
      emit(activeCode, Gesture::longPress, false);
    } else if (activeRepeat && (int32_t)(now - nextRepeatAt) >= 0) {
      nextRepeatAt = now + REPEAT_MS;
      emit(activeCode, Gesture::longPress, true);
    }
  }

  void finish(Emit emit) {
    active = false;
    if (activeGestures && !longDone) emit(activeCode, Gesture::shortPress, false);
  }

  bool active = false, activeKnown = false, activeGestures = false, activeRepeat = false, longDone = false;
  uint32_t startedAt = 0, lastAt = 0, nextRepeatAt = 0;
  char activeCode[16] = "";
};
