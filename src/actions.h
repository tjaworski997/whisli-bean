#pragma once
// What a button does, separately for each gesture. Stored as JSON (buttons.cpp):
//   {"short":{"enabled":true,"mqtt":true,"webhook":3,"wol":1},"long":{"enabled":false,"mqtt":true}}
//   enabled  a switched-off gesture keeps its settings but does nothing (on when absent)
//   once     long only: act once, or (false) repeat while the button is down. Waking a server never
//            repeats, and a repeat is skipped while an earlier webhook is still being sent.
//   mqtt     publish the button's name and the gesture (mqtt.h)
//   webhook  id of a webhook defined on its own (hooks.h)
//   wol      id of a server to wake, defined on its own (servers.h)
// They do not depend on each other. MQTT goes out at once; the rest runs in one task, so slow
// networks never hold up IR reception.
#include <ArduinoJson.h>

#include "gesture.h"

namespace actions {

void begin();
// Any task, never blocks, with StateLock held. Dropped when too busy.
void trigger(const char *code, Gesture gesture, bool repeat = false);

// With StateLock held:
bool validate(const String &json, String &normalized);  // false when anything in it is wrong
void status(JsonObject out);  // the run in progress or last finished
void runs(JsonArray out);     // the last run of every button, each action with its own status
void forgetRun(const String &code);  // its actions changed, so the old statuses mean nothing
uint32_t revision();          // changes with every status, so the panel updates at once
bool testHook(uint8_t id);    // sends a webhook from the actions task; false when one is already being tested
void hookTestStatus(JsonObject out);  // how the last test went

}  // namespace actions
