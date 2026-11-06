#include "mqtt.h"

#include <Preferences.h>
#include <WiFi.h>
#include <mqtt_client.h>

#include <vector>

#include "buttons.h"
#include "device.h"
#include "eventlog.h"
#include "lock.h"

namespace mqtt {
namespace {

constexpr uint32_t HEARTBEAT_MS = 60000;     // full state (with RSSI) even without changes
constexpr const char *NVS = "bean-mqtt";

struct Outgoing {
  String topic, payload;
  bool retain;
};

constexpr size_t MAX_OUTGOING = 24;  // one gesture makes 3 messages
constexpr uint32_t TRIGGER_EVERY_MS = 100;  // each button is 3 retained messages: not all at once
constexpr const char *TRIGGER_TYPES[][2] = {{"short", "button_short_press"}, {"long", "button_long_press"}};

// Shared (StateLock)
Settings saved;
bool restartPending = false;
std::vector<Outgoing> outgoing;
std::vector<String> removedButtons, changedButtons;

// loop() only. The client keeps pointers to these strings for its whole life.
Settings current;
esp_mqtt_client_handle_t client = nullptr;
String base, nodeId, willTopic, haStatusTopic;
uint32_t announcedName = 0;

// Written by the esp-mqtt task
volatile bool connected = false;
volatile bool resync = false;  // just connected or HA restarted: announce everything again
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
const char *currentState = "disabled";
const char *errorCode = "";

uint32_t published = 0, connects = 0;  // for diagnostics

void setStatus(const char *state, const char *error = "") {
  portENTER_CRITICAL(&statusMux);
  currentState = state;
  errorCode = error;
  portEXIT_CRITICAL(&statusMux);
}

String baseOf(const Settings &s) { return s.baseTopic.length() ? s.baseTopic : defaultBaseTopic(); }

String quote(const String &text) {
  String out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  return out + '"';
}

const char *errorOf(const esp_mqtt_error_codes_t *e) {
  if (e && e->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
    const bool credentials = e->connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME ||
                             e->connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED;
    return credentials ? "credentials" : "refused";
  }
  if (e && e->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) return "unreachable";
  return "failed";
}

void onEvent(void *, esp_event_base_t, int32_t id, void *data) {
  auto event = static_cast<esp_mqtt_event_handle_t>(data);
  switch (id) {
    case MQTT_EVENT_CONNECTED:
      connects++;
      eventlog::info("mqtt", "mqttConnected");
      connected = true;
      resync = true;
      setStatus("connected");
      esp_mqtt_client_subscribe(event->client, haStatusTopic.c_str(), 0);
      break;
    case MQTT_EVENT_DISCONNECTED:
      if (connected) eventlog::warning("mqtt", "mqttLost");
      connected = false;
      if (strcmp(currentState, "error") != 0) setStatus("connecting");
      break;
    case MQTT_EVENT_ERROR:
      if (strcmp(currentState, "error") != 0) eventlog::error("mqtt", "mqttError", errorOf(event->error_handle));  // once, not every retry
      setStatus("error", errorOf(event->error_handle));
      break;
    case MQTT_EVENT_DATA:
      // HA restarted and forgot non-retained states
      if (event->data_len == 6 && !strncmp(event->data, "online", 6)) resync = true;
      break;
    default:
      break;
  }
}

void enqueue(const String &topic, const String &payload, bool retain) {
  esp_mqtt_client_enqueue(client, topic.c_str(), payload.c_str(), payload.length(), 1, retain, true);
  published++;
}

struct Entity {
  const char *component, *key, *name, *deviceClass, *unit;
  bool diagnostic;
};
// unique_id = <nodeId>_<key>; never change the keys or HA loses the entities.
// Names are English in every panel language (HA users translate or rename them there).
const Entity ENTITIES[] = {
    {"sensor", "rssi", "Wi-Fi signal", "signal_strength", "dBm", true},
};

void publishDiscovery(const String &name) {
  const String device = "{\"identifiers\":[\"" + nodeId + "\"],\"name\":" + quote(name) +
                        ",\"manufacturer\":\"Whisli\",\"model\":\"Whisli Bean\",\"configuration_url\":\"http://" +
                        WiFi.localIP().toString() + "/\"}";
  for (const Entity &e : ENTITIES) {
    const String topic = current.discoveryPrefix + "/" + e.component + "/" + nodeId + "/" + e.key + "/config";
    if (!current.discovery) {  // an empty retained message removes the entity from HA
      enqueue(topic, "", true);
      continue;
    }
    String json = "{\"name\":" + quote(e.name) + ",\"unique_id\":\"" + nodeId + "_" + e.key +
                  "\",\"state_topic\":\"" + base + "/state\",\"availability_topic\":\"" + base +
                  "/status\",\"device_class\":\"" + e.deviceClass + "\",\"value_template\":\"{{ value_json." +
                  e.key + " }}\"";
    if (e.unit) json += ",\"unit_of_measurement\":" + quote(e.unit) + ",\"state_class\":\"measurement\"";
    if (e.diagnostic) json += ",\"entity_category\":\"diagnostic\"";
    json += ",\"device\":" + device + "}";
    enqueue(topic, json, true);
  }
  const String eventTopic = current.discoveryPrefix + "/event/" + nodeId + "/button/config";
  enqueue(eventTopic, !current.discovery ? "" :
          "{\"name\":\"Button\",\"unique_id\":\"" + nodeId + "_button\",\"state_topic\":\"" + base +
          "/event\",\"availability_topic\":\"" + base + "/status\",\"device_class\":\"button\","
          "\"event_types\":[\"short\",\"long\"],\"device\":" + device + "}", true);
}

void publishState() {
  JsonDocument doc;
  doc["rssi"] = WiFi.RSSI();
  String json;
  serializeJson(doc, json);
  enqueue(base + "/state", json, true);
}

String triggerTopic(const String &code, const char *gesture) {
  return current.discoveryPrefix + "/device_automation/" + nodeId + "/" + code + "_" + gesture + "/config";
}

// A device trigger lets HA's automation editor offer "ok, short press" for this device.
// An empty retained message removes it: a trigger exists only for a switched-on gesture with MQTT
// ticked, and long only for buttons using gestures.
void publishTriggers(const String &code, const String &name, bool gestures, const String &actions) {
  JsonDocument doc;
  deserializeJson(doc, actions);
  for (const auto &entry : TRIGGER_TYPES) {
    const bool wanted = current.discovery && (gestures || !strcmp(entry[0], "short")) && (doc[entry[0]]["enabled"] | true) && (doc[entry[0]]["mqtt"] | false);
    String json;
    if (wanted)
      json = "{\"automation_type\":\"trigger\",\"type\":\"" + String(entry[1]) + "\",\"subtype\":" + quote(name) +
             ",\"topic\":\"" + base + "/button/" + code + "\",\"payload\":\"" + entry[0] +
             "\",\"device\":{\"identifiers\":[\"" + nodeId + "\"]}}";
    enqueue(triggerTopic(code, entry[0]), json, true);
  }
}

// Shared (StateLock). False without a connection: a late event would surprise.
bool send(const String &topic, const String &payload, bool retain) {
  if (!connected || outgoing.size() >= MAX_OUTGOING) return false;
  outgoing.push_back({topic, payload, retain});
  return true;
}

// Stops the client. forget: first clear our retained messages (other broker/topic, or disabled).
void stop(bool forget) {
  if (!client) return;
  if (forget && connected) {
    esp_mqtt_client_publish(client, (base + "/status").c_str(), "", 0, 1, true);
    esp_mqtt_client_publish(client, (base + "/state").c_str(), "", 0, 1, true);
  }
  esp_mqtt_client_stop(client);
  esp_mqtt_client_destroy(client);
  client = nullptr;
  connected = false;
}

void start() {
  base = baseOf(current);
  nodeId = "whisli_bean_" + device::id();
  willTopic = base + "/status";
  haStatusTopic = current.discoveryPrefix + "/status";
  esp_mqtt_client_config_t config = {};
  config.broker.address.hostname = current.host.c_str();
  config.broker.address.port = current.port;
  config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  config.credentials.client_id = nodeId.c_str();
  if (current.user.length()) config.credentials.username = current.user.c_str();
  if (current.password.length()) config.credentials.authentication.password = current.password.c_str();
  config.session.last_will.topic = willTopic.c_str();
  config.session.last_will.msg = "offline";
  config.session.last_will.retain = true;
  config.session.last_will.qos = 1;
  config.network.timeout_ms = 5000;
  config.network.reconnect_timeout_ms = 10000;
  config.task.priority = 3;
  config.task.stack_size = 4096;
  config.buffer.size = 1024;
  client = esp_mqtt_client_init(&config);
  if (!client) return setStatus("error", "failed");
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onEvent, nullptr);
  esp_mqtt_client_start(client);
  setStatus("connecting");
}

Settings load(const char *ns) {
  Settings s;
  Preferences prefs;
  prefs.begin(ns, true);
  s.enabled = prefs.getBool("enabled", s.enabled);
  s.host = prefs.getString("host", s.host);
  s.port = prefs.getUShort("port", s.port);
  s.user = prefs.getString("user", s.user);
  s.password = prefs.getString("pass", s.password);
  s.baseTopic = prefs.getString("topic", s.baseTopic);
  s.discovery = prefs.getBool("disc", s.discovery);
  s.discoveryPrefix = prefs.getString("discPrefix", s.discoveryPrefix);
  prefs.end();
  return s;
}

}  // namespace

String defaultBaseTopic() { return "whisli-bean/" + device::id(); }

bool validTopic(const String &t) {
  return t.length() && t.length() <= 64 && !t.startsWith("/") && !t.endsWith("/") && t.indexOf("//") < 0 &&
         t.indexOf('#') < 0 && t.indexOf('+') < 0;
}

void begin() {
  saved = current = load(NVS);
}

Settings settings() { return saved; }

void save(const Settings &s) {
  Preferences prefs;
  prefs.begin(NVS, false);
  prefs.putBool("enabled", s.enabled);
  prefs.putString("host", s.host);
  prefs.putUShort("port", s.port);
  prefs.putString("user", s.user);
  prefs.putString("pass", s.password);
  prefs.putString("topic", s.baseTopic);
  prefs.putBool("disc", s.discovery);
  prefs.putString("discPrefix", s.discoveryPrefix);
  prefs.end();
  saved = s;
  restartPending = true;
}

void loop() {
  String deviceName;
  uint32_t nameVersion;
  bool restart;
  Settings next;
  {
    StateLock lock;
    deviceName = device::name();
    nameVersion = device::nameVersion();
    restart = restartPending;
    restartPending = false;
    next = saved;
  }

  if (restart) {
    const bool moved = next.host != current.host || next.port != current.port || baseOf(next) != base;
    stop(moved || !next.enabled);
    current = next;
    if (!current.enabled || current.host.isEmpty()) setStatus("disabled");
  }
  if (!client && current.enabled && current.host.length() && WiFi.isConnected()) start();
  if (!client || !connected) return;

  static uint32_t heartbeatAt = 0, triggerAt = 0;
  constexpr size_t NO_PASS = SIZE_MAX;
  static size_t triggerIndex = NO_PASS;
  const uint32_t t = millis();
  std::vector<Outgoing> toSend;
  std::vector<String> toRemove;
  String code, name, actions;
  bool gestures = false, hasTrigger = false, hasChanged = false;
  {
    StateLock lock;
    toSend.swap(outgoing);
    toRemove.swap(removedButtons);
    if (t - triggerAt >= TRIGGER_EVERY_MS) {
      if (!changedButtons.empty()) {
        code = changedButtons.front();
        changedButtons.erase(changedButtons.begin());
        const char *known = buttons::nameOf(code.c_str());
        hasChanged = known;
        if (known) name = known;
        gestures = buttons::usesGestures(code.c_str());
      } else if (triggerIndex != NO_PASS) {
        hasTrigger = buttons::at(triggerIndex, code, name, gestures);
        // A pass over all buttons ends here; a button saved later announces itself as changed.
        if (!hasTrigger) triggerIndex = NO_PASS;
      }
      if (hasTrigger || hasChanged) actions = buttons::actionsOf(code.c_str());
    }
  }
  for (const Outgoing &message : toSend) enqueue(message.topic, message.payload, message.retain);
  for (const String &removed : toRemove)
    for (const auto &entry : TRIGGER_TYPES) enqueue(triggerTopic(removed, entry[0]), "", true);
  if (hasTrigger || hasChanged) {
    publishTriggers(code, name, gestures, actions);
    if (hasTrigger) triggerIndex++;
    triggerAt = t;
  }

  if (resync || nameVersion != announcedName) {
    resync = false;
    announcedName = nameVersion;
    enqueue(base + "/status", "online", true);
    publishDiscovery(deviceName);
    triggerIndex = 0;
  } else if (t - heartbeatAt < HEARTBEAT_MS) {
    return;
  }
  publishState();
  heartbeatAt = t;
}

bool buttonEvent(const char *code, const char *name, const char *gesture) {
  const String topic = baseOf(saved), codeText = code, gestureText = gesture;
  const bool sent = send(topic + "/button", "{\"code\":\"" + codeText + "\",\"name\":" + quote(name) + ",\"gesture\":\"" + gestureText + "\"}", false);
  send(topic + "/button/" + codeText, gestureText, false);
  send(topic + "/event", "{\"event_type\":\"" + gestureText + "\",\"button\":" + quote(name) + ",\"code\":\"" + codeText + "\"}", false);
  return sent;
}

void buttonChanged(const String &code) {
  if (connected && changedButtons.size() < buttons::MAX_BUTTONS) changedButtons.push_back(code);
}

void buttonRemoved(const String &code) {
  if (removedButtons.size() < buttons::MAX_BUTTONS) removedButtons.push_back(code);
}

void diagnostics(JsonObject out) {
  out["state"] = state();
  out["error"] = error();
  out["host"] = saved.host;
  out["port"] = saved.port;
  out["base"] = baseOf(saved);
  out["published"] = published;
  out["connects"] = connects;
}

const char *state() {
  portENTER_CRITICAL(&statusMux);
  const char *s = currentState;
  portEXIT_CRITICAL(&statusMux);
  return s;
}

String error() {
  portENTER_CRITICAL(&statusMux);
  const char *e = errorCode;
  portEXIT_CRITICAL(&statusMux);
  return e;
}

}  // namespace mqtt
