#pragma once
// Home Assistant over MQTT (optional - the device works the same without a broker).
// MQTT Discovery: one HA device with a Wi-Fi signal entity, a button event entity and a device
// trigger for every saved button. Topics, <base> = "whisli-bean/<id>" unless set:
//   <base>/status  online / offline (retained, last will)
//   <base>/state   {"rssi":-60}
//   <base>/button  a gesture of a button with MQTT ticked: {"code":"NEC-4BBA-E0","name":"ok","gesture":"short"}
//   <base>/button/<code>  short / long, for device triggers
//   <base>/event   the same for the event entity: {"event_type":"short","button":"ok","code":"…"}
// esp-mqtt connects and reconnects in its own task, so a bad broker is only an error in the panel.
#include <ArduinoJson.h>

namespace mqtt {

struct Settings {
  bool enabled = false;
  String host;
  uint16_t port = 1883;
  String user, password;
  String baseTopic;  // empty = default
  bool discovery = true;
  String discoveryPrefix = "homeassistant";
};

void begin();
void loop();  // loop(), without StateLock

// With StateLock held:
Settings settings();
void save(const Settings &settings);  // NVS now, the client restarts in loop()
String defaultBaseTopic();
bool validTopic(const String &topic);
const char *state();  // "disabled", "connecting", "connected", "error"
String error();       // message from the broker connection, if any
void diagnostics(JsonObject out);  // state, broker, messages sent, connections since start
// Queued and sent from loop(). False when there is no connection.
bool buttonEvent(const char *code, const char *name, const char *gesture);
void buttonChanged(const String &code);  // announce its triggers again
void buttonRemoved(const String &code);  // remove its triggers from HA

}  // namespace mqtt
