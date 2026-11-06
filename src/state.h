#pragma once
// The device's live state as JSON. All functions expect StateLock held.
#include <ArduinoJson.h>

namespace state {

// Everything the panel shows live. Sent over ws://<ip>/api/v1/ws every second and at once when a
// frame arrives or learning moves on; also GET /api/v1/state.
void full(JsonObject out);
bool hasProblem();

// Why the chip last started: "powerOn", "software", "panic", "watchdog", "brownout", "usb", "other"
const char *resetReason();
// loop() reports how long each pass took; diagnostics shows the average and the longest
void recordLoop(uint32_t micros);
// Chip, memory, flash and loop timing for System → Diagnostics
void system(JsonObject out);

}  // namespace state
