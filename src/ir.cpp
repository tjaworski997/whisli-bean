#include "ir.h"

#include <Arduino.h>

#include "actions.h"
#include "board.h"
#include "buttons.h"
#include "eventlog.h"
#include "ir_decode.h"
#include "led.h"
#include "lock.h"
#include "soc/gpio_reg.h"

namespace ir {
namespace {

constexpr size_t MAX_EDGES = 300;
// Below the gap between frames of a held button (about 20 ms on Sony and JVC, 6 ms at the very
// least), above the longest space inside a frame (about 9 ms), or repeats would merge into one.
constexpr uint32_t FRAME_GAP_US = 10000;
constexpr size_t STORED_FRAMES = 20;
constexpr size_t STORED_DURATIONS = 150;
constexpr size_t LEARN_SEEN = 8;
constexpr size_t CODE_SIZE = sizeof(Decoded::code);

// Written by the interrupt
volatile uint32_t edgeCount = 0;
volatile uint32_t stamps[MAX_EDGES];
volatile size_t stampCount = 0;
volatile int firstLevel = 0;

void IRAM_ATTR onEdge() {
  edgeCount++;
  if (stampCount == 0) firstLevel = (REG_READ(GPIO_IN_REG) >> pins::IR_RECEIVER) & 1;
  if (stampCount < MAX_EDGES) stamps[stampCount++] = micros();
}

struct Frame : Decoded {
  uint32_t id = 0, atMs = 0;
  uint16_t edges = 0, durationCount = 0;
  uint16_t durations[STORED_DURATIONS];
  uint8_t level = 0;
};

enum class LearnState : uint8_t { idle, waiting, learned, timedOut };

struct Learn {
  LearnState state = LearnState::idle;
  uint32_t startedMs = 0, distinct = 0, unreadable = 0;
  char code[CODE_SIZE] = "";
  char seen[LEARN_SEEN][CODE_SIZE] = {};
};

struct Press {
  uint32_t count = 0, atMs = 0;
  char code[CODE_SIZE] = "";
  const char *gesture = "";
};

// Shared (StateLock)
Frame stored[STORED_FRAMES];
uint32_t lastFrameId = 0, framesWithCode = 0, changes = 0;
Learn learn;
Press press;
GestureTracker tracker;

// Stray pulses last 200-330 us and reach 12 edges or more, so edge count alone is not proof: a remote's
// frame is long and opens with a leader or bit of at least about 800 us.
bool looksLikeRemote(const Frame &frame) {
  return frame.protocol != Protocol::unknown || (frame.edges >= 20 && frame.durationCount && frame.durations[0] >= 800);
}

void expireLearn() {
  if (learn.state != LearnState::waiting || millis() - learn.startedMs < LEARN_TIMEOUT_MS) return;
  learn.state = LearnState::timedOut;
  changes++;
}

// A code without a checksum could be a garbled frame, so it counts once it shows up a second time.
// Any earlier code may match: some remotes alternate two different frames for one press.
void learnFrom(const Frame &frame) {
  expireLearn();
  if (learn.state != LearnState::waiting) return;
  if (!frame.code[0]) {
    if (frame.protocol == Protocol::unknown && looksLikeRemote(frame)) learn.unreadable++;
    return;
  }
  bool confirmed = frame.trusted;
  for (const auto &seen : learn.seen)
    if (!strcmp(seen, frame.code)) confirmed = true;
  if (confirmed) {
    strlcpy(learn.code, frame.code, sizeof(learn.code));
    learn.state = LearnState::learned;
    eventlog::info("ir", "learned", frame.code);
    return;
  }
  strlcpy(learn.seen[learn.distinct % LEARN_SEEN], frame.code, CODE_SIZE);
  learn.distinct++;
}

void remember(const Frame &frame) {
  if (!frame.code[0]) return;
  const bool lastKnown = press.count && buttons::nameOf(press.code);
  if (lastKnown && !buttons::nameOf(frame.code) && frame.atMs - press.atMs < GestureTracker::SECOND_FRAME_MS) return;
  strlcpy(press.code, frame.code, sizeof(press.code));
  press.atMs = frame.atMs;
  press.count++;
}

void trackGesture(const Frame &frame) {
  if (learn.state != LearnState::idle) return;  // pressing a button to learn it must not run anything
  if (frame.code[0]) tracker.frame(frame.code, buttons::nameOf(frame.code), buttons::usesGestures(frame.code), buttons::longRepeats(frame.code), frame.atMs, fire);
  else if (frame.protocol == Protocol::necRepeat) tracker.repeat(frame.atMs, fire);
}

void collectFrame() {
  static uint32_t times[MAX_EDGES];
  noInterrupts();
  const size_t edges = stampCount;
  const int level = firstLevel;
  const bool ended = edges > 0 && micros() - stamps[edges - 1] > FRAME_GAP_US;
  if (ended) {
    for (size_t i = 0; i < edges; i++) times[i] = stamps[i];
    stampCount = 0;
  }
  interrupts();
  if (!ended) return;

  led::frameReceived();
  const size_t durationCount = edges - 1;
  for (size_t i = 0; i < durationCount; i++) times[i] = times[i + 1] - times[i];

  static Frame frame;
  frame = Frame();
  static_cast<Decoded &>(frame) = decodeFrame(times, durationCount, level);
  frame.atMs = millis();
  frame.level = level;
  frame.edges = edges;
  frame.durationCount = min(durationCount, STORED_DURATIONS);
  for (size_t i = 0; i < frame.durationCount; i++) frame.durations[i] = min<uint32_t>(times[i], UINT16_MAX);

  StateLock lock;
  frame.id = ++lastFrameId;
  stored[frame.id % STORED_FRAMES] = frame;
  if (frame.code[0]) framesWithCode++;
  learnFrom(frame);
  remember(frame);
  trackGesture(frame);
  changes++;
}

const char *learnStateName(LearnState state) {
  switch (state) {
    case LearnState::waiting: return "waiting";
    case LearnState::learned: return "learned";
    case LearnState::timedOut: return "timedOut";
    default: return "idle";
  }
}

}  // namespace

void begin() {
  pinMode(pins::IR_RECEIVER, INPUT_PULLUP);
  attachInterrupt(pins::IR_RECEIVER, onEdge, CHANGE);
}

void loop() {
  collectFrame();
  StateLock lock;
  expireLearn();
  tracker.tick(millis(), fire);
}

void fire(const char *code, Gesture gesture, bool repeat) {
  press.gesture = gestureName(gesture);
  changes++;
  actions::trigger(code, gesture, repeat);
}

uint32_t revision() { return changes; }

void learnStatus(JsonObject out) {
  expireLearn();
  out["state"] = learnStateName(learn.state);
  out["distinct"] = learn.distinct;  // codes seen so far that have not repeated
  out["unreadable"] = learn.unreadable;
  if (learn.state == LearnState::waiting) out["remainingMs"] = LEARN_TIMEOUT_MS - (millis() - learn.startedMs);
  if (learn.state == LearnState::learned) {
    out["code"] = learn.code;
    if (const char *name = buttons::nameOf(learn.code)) out["name"] = name;
  }
}

void status(JsonObject out) {
  out["pin"] = pins::IR_RECEIVER;
  out["level"] = digitalRead(pins::IR_RECEIVER);
  out["lastFrameId"] = lastFrameId;
  if (press.count) {
    JsonObject last = out["press"].to<JsonObject>();
    last["count"] = press.count;
    last["code"] = press.code;
    last["atMs"] = press.atMs;
    last["gesture"] = press.gesture;
    if (const char *name = buttons::nameOf(press.code)) last["name"] = name;
  } else {
    out["press"] = nullptr;
  }
  learnStatus(out["learn"].to<JsonObject>());
}

void frames(JsonObject out, uint32_t since) {
  out["uptimeMs"] = millis();
  out["lastId"] = lastFrameId;
  JsonArray list = out["frames"].to<JsonArray>();
  const uint32_t oldest = lastFrameId > STORED_FRAMES ? lastFrameId - STORED_FRAMES + 1 : 1;
  // since > lastFrameId means the device restarted, so the caller gets everything again.
  const uint32_t from = since > lastFrameId ? oldest : max(since + 1, oldest);
  for (uint32_t id = from; id <= lastFrameId; id++) {
    const Frame &frame = stored[id % STORED_FRAMES];
    JsonObject item = list.add<JsonObject>();
    item["id"] = frame.id;
    item["atMs"] = frame.atMs;
    item["firstLevel"] = frame.level;
    item["edges"] = frame.edges;
    item["protocol"] = protocolName(frame.protocol);
    item["remote"] = looksLikeRemote(frame);
    if (frame.code[0]) {
      item["code"] = frame.code;
      if (const char *name = buttons::nameOf(frame.code)) item["name"] = name;
    }
    JsonArray durations = item["durationsUs"].to<JsonArray>();
    for (size_t i = 0; i < frame.durationCount; i++) durations.add(frame.durations[i]);
  }
}

void diagnostics(JsonObject out) {
  out["pin"] = pins::IR_RECEIVER;
  out["level"] = digitalRead(pins::IR_RECEIVER);
  out["edges"] = edgeCount;
  out["frames"] = lastFrameId;
  out["framesWithCode"] = framesWithCode;
}

void startLearning() {
  learn = Learn();
  learn.state = LearnState::waiting;
  learn.startedMs = millis();
  changes++;
}

void cancelLearning() {
  learn = Learn();
  changes++;
}

void learningSaved(const String &code) {
  if (learn.state != LearnState::learned || code != learn.code) return;
  learn = Learn();
  changes++;
}

}  // namespace ir
