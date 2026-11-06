#include "led.h"

#include <Arduino.h>
#include <Preferences.h>

#include "board.h"

namespace led {
namespace {

const char *const MODE_NAMES[] = {"off", "status", "received", "sent"};
constexpr const char *NVS = "bean-led";
constexpr uint32_t FRAME_FLASH_MS = 60, SENT_FLASH_MS = 120, ALERT_ON_MS = 100;
constexpr uint32_t PWM_HZ = 5000;
constexpr uint8_t PWM_BITS = 8;

Settings current;  // shared (StateLock); show() copies these few bytes without it
volatile uint32_t flashUntil = 0;
int lastDuty = -1;

void write(uint8_t percent) {
  // The LED lights when the pin is low, so full duty is dark.
  const int duty = 255 - percent * 255 / 100;
  if (duty == lastDuty) return;  // write the pin only when it changes
  lastDuty = duty;
  ledcWrite(pins::LED, duty);
}

}  // namespace

void begin() {
  ledcAttach(pins::LED, PWM_HZ, PWM_BITS);
  write(0);
  Preferences prefs;
  prefs.begin(NVS, true);
  current.mode = (Mode)min<uint8_t>(prefs.getUChar("mode", (uint8_t)current.mode), (uint8_t)Mode::sent);
  current.brightness = constrain(prefs.getUChar("bright", current.brightness), 1, 100);
  current.alerts = prefs.getBool("alerts", current.alerts);
  prefs.end();
}

void frameReceived() {
  if (current.mode == Mode::received) flashUntil = millis() + FRAME_FLASH_MS;
}

void actionSent() {
  if (current.mode == Mode::sent) flashUntil = millis() + SENT_FLASH_MS;
}

void show(bool resetting, bool offline, bool problem) {
  const Settings s = current;  // a torn read would only last one pass
  const uint32_t now = millis();
  if (resetting) return write(now / 100 % 2 ? 100 : 0);
  if (s.mode == Mode::off) return write(0);
  const bool showProblems = s.mode == Mode::status || s.alerts;
  if (showProblems && offline) return write(now % 2000 < ALERT_ON_MS ? s.brightness : 0);
  if (showProblems && problem) return write(now % 3000 < ALERT_ON_MS ? s.brightness : 0);
  write((int32_t)(flashUntil - now) > 0 ? s.brightness : 0);
}

Settings settings() { return current; }

void save(const Settings &s) {
  Preferences prefs;
  prefs.begin(NVS, false);
  prefs.putUChar("mode", (uint8_t)s.mode);
  prefs.putUChar("bright", s.brightness);
  prefs.putBool("alerts", s.alerts);
  prefs.end();
  current = s;
}

void toJson(const Settings &s, JsonObject out) {
  out["mode"] = MODE_NAMES[(int)s.mode];
  out["brightness"] = s.brightness;
  out["alerts"] = s.alerts;
}

bool modeFromName(const char *name, Settings &out) {
  for (int i = 0; i < 4; i++) {
    if (strcmp(name, MODE_NAMES[i])) continue;
    out.mode = (Mode)i;
    return true;
  }
  return false;
}

}  // namespace led
