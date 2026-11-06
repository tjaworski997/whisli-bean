#pragma once
// The on-board LED. What it shows is set in the panel (System → Settings) and saved in NVS:
//   received  - a short flash for every frame from a remote (default)
//   sent      - a short flash when a button's actions really run
//   status    - dark while all is well, and shows problems
//   off       - never lit
// Problems: a blink every 2 s = no Wi-Fi, every 3 s = another one. In received and sent they take
// priority when "alerts" is on. BOOT held (password reset) always blinks fast, at full brightness.
#include <ArduinoJson.h>

namespace led {

enum class Mode : uint8_t { off, status, received, sent };

struct Settings {
  Mode mode = Mode::received;
  uint8_t brightness = 100;  // %, 1-100
  bool alerts = true;
};

void begin();  // loads the settings
void frameReceived();  // safe from any task
void actionSent();     // safe from any task
void show(bool resetting, bool offline, bool problem);  // loop()

// With StateLock held:
Settings settings();
void save(const Settings &settings);
void toJson(const Settings &settings, JsonObject out);
bool modeFromName(const char *name, Settings &out);  // false if the name is unknown

}  // namespace led
