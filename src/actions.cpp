#include "actions.h"

#include <vector>

#include "buttons.h"
#include "eventlog.h"
#include "hooks.h"
#include "led.h"
#include "lock.h"
#include "mqtt.h"
#include "servers.h"

namespace actions {
namespace {

constexpr uint32_t STACK_BYTES = 12288;  // TLS, for webhooks, needs a lot
constexpr uint8_t QUEUE_LENGTH = 8;

enum class MqttResult : uint8_t { unused, sent, offline };

// A press of a button, or a test of a webhook from the panel (testHook is its id).
struct Event {
  char code[16];
  Gesture gesture;
  MqttResult mqtt;
  bool repeat;
  uint8_t testHook;
};

QueueHandle_t queue = nullptr;

// Status of every action of a button's last run, for the panel. Only buttons that have run take memory.
enum class StepStatus : uint8_t { pending, running, ok, failed };
const char *const STEP_NAMES[] = {"pending", "running", "ok", "failed"};

struct Step {
  const char *type = "";
  StepStatus status = StepStatus::pending;
  char detail[32] = "";
};

struct Run {
  String code;
  const char *gesture = "";
  uint32_t count = 0, atMs = 0;
  std::vector<Step> steps;
};

// Shared (StateLock)
std::vector<Run> lastRuns;
uint32_t runCount = 0, changes = 0;
String latestRunCode;
struct {
  uint8_t id = 0;
  bool running = false;
  hooks::Result result;
} hookTest;

Run *findRun(const String &code) {
  for (Run &run : lastRuns)
    if (run.code == code) return &run;
  return nullptr;
}

void setStep(const String &code, size_t index, StepStatus status, const String &detail = "") {
  StateLock lock;
  Run *run = findRun(code);
  if (!run || index >= run->steps.size()) return;
  run->steps[index].status = status;
  strlcpy(run->steps[index].detail, detail.c_str(), sizeof(run->steps[index].detail));
  changes++;
}

void runToJson(const Run &run, JsonObject out) {
  out["code"] = run.code;
  out["gesture"] = run.gesture;
  out["count"] = run.count;
  out["atMs"] = run.atMs;
  JsonArray steps = out["steps"].to<JsonArray>();
  for (const Step &step : run.steps) {
    JsonObject item = steps.add<JsonObject>();
    item["type"] = step.type;
    item["status"] = STEP_NAMES[(int)step.status];
    if (step.detail[0]) item["detail"] = step.detail;
  }
}

void handle(const Event &event) {
  String name, stored;
  {
    StateLock lock;
    const char *known = buttons::nameOf(event.code);
    if (!known) return;
    name = known;
    stored = buttons::actionsOf(event.code);
  }
  const char *gesture = gestureName(event.gesture);
  JsonDocument doc;
  if (deserializeJson(doc, stored)) return;
  JsonObjectConst chosen = doc[gesture].as<JsonObjectConst>();
  if (!(chosen["enabled"] | true)) return;
  const uint8_t webhook = chosen["webhook"] | 0, wol = event.repeat ? 0 : (chosen["wol"] | 0);
  if (event.mqtt == MqttResult::unused && !webhook && !wol) return;

  led::actionSent();
  const String code = event.code;
  {
    StateLock lock;
    Run *run = findRun(code);
    if (!run) {
      if (lastRuns.size() >= buttons::MAX_BUTTONS) lastRuns.erase(lastRuns.begin());
      lastRuns.emplace_back();
      run = &lastRuns.back();
      run->code = code;
    }
    run->gesture = gesture;
    run->count = ++runCount;
    run->atMs = millis();
    run->steps.clear();
    for (const char *type : {"mqtt", "webhook", "wol"}) {
      const bool used = !strcmp(type, "mqtt") ? event.mqtt != MqttResult::unused : !strcmp(type, "webhook") ? webhook : wol;
      if (!used) continue;
      Step step;
      step.type = type;
      run->steps.push_back(step);
    }
    latestRunCode = code;
    changes++;
  }

  bool ok = true;
  String failure;
  size_t index = 0;
  if (event.mqtt != MqttResult::unused) {
    const bool sent = event.mqtt == MqttResult::sent;
    setStep(code, index++, sent ? StepStatus::ok : StepStatus::failed, sent ? "" : "MQTT offline");
    if (!sent) ok = false, failure = "MQTT offline";
  }
  if (webhook) {
    const size_t step = index++;
    setStep(code, step, StepStatus::running);
    const hooks::Result result = hooks::send(webhook, {name, code, gesture}, false);
    const bool good = result.sent && result.status < 400;
    const String detail = result.sent ? "HTTP " + String(result.status) : result.error;
    setStep(code, step, good ? StepStatus::ok : StepStatus::failed, detail);
    if (!good) ok = false, failure = "webhook: " + detail;
  }
  if (wol) {
    const size_t step = index++;
    setStep(code, step, StepStatus::running);
    String detail = "server not found";
    servers::Server server;
    bool found;
    {
      StateLock lock;
      found = servers::load(wol, server);
    }
    const bool good = found && servers::wake(server, detail);
    setStep(code, step, good ? StepStatus::ok : StepStatus::failed, detail);
    if (!good) ok = false, failure = "WoL: " + detail;
  }

  const String text = name + " · " + gesture + (failure.length() ? " · " + failure : "");
  if (!ok) eventlog::warning("action", "actionFailed", text.c_str());
  else if (!event.repeat) eventlog::info("action", "actionOk", text.c_str());
}

void runHookTest(uint8_t id) {
  const hooks::Result result = hooks::send(id, {"Test", "TEST", "short"}, true);
  StateLock lock;
  hookTest.result = result;
  hookTest.running = false;
}

void task(void *) {
  Event event;
  for (;;) {
    if (xQueueReceive(queue, &event, portMAX_DELAY) != pdTRUE) continue;
    if (event.testHook) runHookTest(event.testHook);
    else handle(event);
  }
}

}  // namespace

void begin() {
  queue = xQueueCreate(QUEUE_LENGTH, sizeof(Event));
  xTaskCreate(task, "actions", STACK_BYTES, nullptr, 1, nullptr);
}

void trigger(const char *code, Gesture gesture, bool repeat) {
  if (!queue) return;
  const char *name = buttons::nameOf(code);
  if (!name) return;
  Event event = {};
  strlcpy(event.code, code, sizeof(event.code));
  event.gesture = gesture;
  event.repeat = repeat;
  // Sent here, ahead of the queue: a slow webhook of an earlier press must not delay MQTT.
  JsonDocument doc;
  if (!deserializeJson(doc, buttons::actionsOf(code)) && (doc[gestureName(gesture)]["enabled"] | true) && (doc[gestureName(gesture)]["mqtt"] | false))
    event.mqtt = mqtt::buttonEvent(code, name, gestureName(gesture)) ? MqttResult::sent : MqttResult::offline;
  // When the network is slow, a repeat is skipped instead of queueing up and arriving after the release.
  if (repeat && uxQueueMessagesWaiting(queue) > 0) return;
  if (xQueueSend(queue, &event, 0) != pdTRUE && !repeat) eventlog::warning("action", "actionDropped", code);
}

bool validate(const String &json, String &normalized) {
  JsonDocument doc;
  if (deserializeJson(doc, json) || !doc.is<JsonObject>()) return false;
  JsonDocument clean;
  clean.to<JsonObject>();
  for (JsonPair group : doc.as<JsonObject>()) {
    Gesture gesture;
    if (!gestureFromName(group.key().c_str(), gesture) || !group.value().is<JsonObject>()) return false;
    JsonObject chosen = group.value().as<JsonObject>();
    JsonObject out = clean[group.key()].to<JsonObject>();
    const bool enabled = chosen["enabled"] | true;
    out["enabled"] = enabled;
    if (chosen["mqtt"] | false) out["mqtt"] = true;
    if (!chosen["once"].isNull()) out["once"] = chosen["once"].as<bool>();
    // A switched-off gesture keeps whatever it had, even a webhook deleted since or not chosen yet.
    if (!chosen["webhook"].isNull()) {
      if (!chosen["webhook"].is<uint8_t>() || (enabled && !hooks::exists(chosen["webhook"].as<uint8_t>()))) return false;
      out["webhook"] = chosen["webhook"];
    }
    if (!chosen["wol"].isNull()) {
      if (!chosen["wol"].is<uint8_t>() || (enabled && !servers::exists(chosen["wol"].as<uint8_t>()))) return false;
      out["wol"] = chosen["wol"];
    }
  }
  serializeJson(clean, normalized);
  return true;
}

void status(JsonObject out) {
  if (const Run *run = findRun(latestRunCode)) runToJson(*run, out["run"].to<JsonObject>());
  else out["run"] = nullptr;
}

void runs(JsonArray out) {
  for (const Run &run : lastRuns) runToJson(run, out.add<JsonObject>());
}

void forgetRun(const String &code) {
  for (size_t i = 0; i < lastRuns.size(); i++) {
    if (lastRuns[i].code != code) continue;
    lastRuns.erase(lastRuns.begin() + i);
    changes++;
    return;
  }
}

uint32_t revision() { return changes; }

bool testHook(uint8_t id) {
  if (!queue || hookTest.running) return false;
  Event event = {};
  event.testHook = id;
  if (xQueueSend(queue, &event, 0) != pdTRUE) return false;
  hookTest.id = id;
  hookTest.running = true;
  hookTest.result = hooks::Result();
  return true;
}

void hookTestStatus(JsonObject out) {
  out["id"] = hookTest.id;
  out["running"] = hookTest.running;
  out["sent"] = hookTest.result.sent;
  out["status"] = hookTest.result.status;
  out["error"] = hookTest.result.error;
  out["response"] = hookTest.result.response;
}

}  // namespace actions
