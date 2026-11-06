// Computer test of gesture detection, with no hardware. Run with scripts/test.sh.
// Timing follows the real remote: one frame every 108 ms, at least 3 frames per tap.
#include <stdio.h>

#include <string>
#include <vector>

#include "gesture.h"

static std::vector<std::string> events;
static int failures = 0;

static void emit(const char *code, Gesture gesture, bool repeat) { events.push_back(std::string(code) + ":" + gestureName(gesture) + (repeat ? "+" : "")); }

static void check(const char *what, const std::vector<std::string> &expected) {
  const bool ok = events == expected;
  std::string got;
  for (const std::string &e : events) got += e + " ";
  printf("%s %s -> %s\n", ok ? "ok  " : "FAIL", what, got.c_str());
  if (!ok) failures++;
  events.clear();
}

// Presses a button: a frame every 108 ms for `frames` frames. The clock runs through tick() too.
static uint32_t press(GestureTracker &t, const char *code, int frames, bool gestures, uint32_t now, bool known = true, bool repeat = false) {
  for (int i = 0; i < frames; i++) {
    t.frame(code, known, gestures, repeat, now, emit);
    for (int ms = 0; ms < 108; ms += 10) t.tick(now + ms, emit);
    now += 108;
  }
  return now;
}

static uint32_t wait(GestureTracker &t, uint32_t now, uint32_t ms) {
  for (uint32_t m = 0; m < ms; m += 10) t.tick(now + m, emit);
  return now + ms;
}

int main() {
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 3, false, 1000);
    check("no gestures: short at the first frame", {"A:short"});
    wait(t, now, 400);
    check("no gestures: nothing on release", {});
    now = press(t, "A", 30, false, now + 400);
    wait(t, now, 400);
    check("no gestures: holding fires once", {"A:short"});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 3, true, 1000);
    check("gestures: nothing while the tap lasts", {});
    now = wait(t, now, 400);
    check("gestures: tap gives short on release", {"A:short"});
    now = press(t, "A", 3, true, now);
    now = wait(t, now, 400);
    now = press(t, "A", 3, true, now);
    wait(t, now, 400);
    check("gestures: two separate taps give two shorts", {"A:short", "A:short"});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 7, true, 1000);  // up to 648 ms, just under the limit
    wait(t, now, 400);
    check("gestures: released before the limit is still short", {"A:short"});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 10, true, 1000);  // about 1 s
    wait(t, now, 400);
    check("gestures: long, and no short after it", {"A:long"});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 20, true, 1000);  // about 2 s
    wait(t, now, 400);
    check("gestures: holding on gives long only once", {"A:long"});
  }
  {
    GestureTracker t;
    t.frame("A", true, true, false, 1000, emit);
    uint32_t now = 1000;
    for (int i = 0; i < 9; i++) {
      now += 108;
      t.repeat(now, emit);
    }
    check("NEC repeat frames continue the press", {"A:long"});
    GestureTracker idle;
    idle.repeat(5000, emit);
    idle.tick(6000, emit);
    check("a repeat frame alone does nothing", {});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 2, true, 1000);
    now = press(t, "B", 2, true, now);
    wait(t, now, 400);
    check("another button ends the press at once", {"A:short", "B:short"});
  }
  {
    GestureTracker t;
    uint32_t now = 1000;
    for (int i = 0; i < 4; i++) {
      t.frame("A", true, true, false, now, emit);
      t.frame("X", false, false, false, now + 40, emit);
      now += 108;
    }
    wait(t, now, 400);
    check("an unknown second frame does not break a known press", {"A:short"});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 1, false, 1000);
    now = wait(t, now, 400);
    press(t, "X", 1, false, now, false);
    check("an unknown button after a pause still counts", {"A:short", "X:short"});
  }
  {
    GestureTracker t;
    uint32_t now = press(t, "A", 3, true, 0xFFFFFF00u);  // millis() about to wrap around
    wait(t, now, 400);
    check("survives the clock wrapping around", {"A:short"});
  }
  // The real remote while held: 3 frames 108 ms apart, then a gap of 163 ms.
  const auto hold = [](GestureTracker &t, int bursts, bool repeat, int lostFrom, int lostFor, uint32_t now) {
    int index = 0;
    for (int b = 0; b < bursts; b++) {
      for (int f = 0; f < 3; f++, index++) {
        if (index < lostFrom || index >= lostFrom + lostFor) t.frame("A", true, true, repeat, now, emit);
        const uint32_t gap = f == 2 ? 163 : 108;
        for (uint32_t ms = 0; ms < gap; ms += 10) t.tick(now + ms, emit);
        now += gap;
      }
    }
    return now;
  };
  {
    GestureTracker t;
    uint32_t now = hold(t, 12, false, 999, 0, 1000);
    wait(t, now, 900);
    check("real hold pattern, once: one long", {"A:long"});
    now = hold(t, 12, false, 12, 5, now + 900);
    wait(t, now, 900);
    check("once: five frames lost in the middle do not fire it again", {"A:long"});
    now = press(t, "A", 3, true, now + 900);
    wait(t, now, 400);
    check("once: the next tap after a pause still works", {"A:short"});
  }
  {
    GestureTracker t;
    // Frames at 866, then the first at least 300 ms after each: 1245, 1624 and 2003 ms; the last is at 2111 ms.
    uint32_t now = hold(t, 6, true, 999, 0, 1000);
    wait(t, now, 900);
    check("real hold pattern, repeating", {"A:long", "A:long+", "A:long+", "A:long+"});
  }
  {
    GestureTracker t;
    uint32_t now = hold(t, 1, false, 999, 0, 1000);
    wait(t, now, 400);
    check("a tap of one burst is short, whatever the mode", {"A:short"});
  }
  puts(failures ? "\nFAILED" : "\nall passed");
  return failures ? 1 : 0;
}
