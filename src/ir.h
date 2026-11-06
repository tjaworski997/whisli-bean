#pragma once
// IR reception: captures edges in an interrupt, cuts them into frames, decodes each one
// (ir_decode.h), remembers the latest frames and the last pressed button, turns frames into gestures
// (gesture.h) and runs learning. Gestures start the button's actions.
#include <ArduinoJson.h>

#include "gesture.h"

namespace ir {

constexpr uint32_t LEARN_TIMEOUT_MS = 30000;

void begin();
void loop();  // loop(), without StateLock

// With StateLock held:
uint32_t revision();  // changes with every frame and learning step, so the panel updates at once
void status(JsonObject out);  // level, frame counts, last press, learning
void frames(JsonObject out, uint32_t since);  // stored frames newer than `since`, with timings
void diagnostics(JsonObject out);
void startLearning();
void cancelLearning();
void learnStatus(JsonObject out);
void learningSaved(const String &code);  // the learned code got its name, so learning is finished
void fire(const char *code, Gesture gesture, bool repeat = false);  // what a gesture does; also the panel's test button

}  // namespace ir
