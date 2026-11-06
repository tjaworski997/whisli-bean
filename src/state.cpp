#include "state.h"

#include <WiFi.h>
#include <time.h>

#include "actions.h"
#include "board.h"
#include "device.h"
#include "generated/version.h"
#include "ir.h"
#include "mqtt.h"
#include "storage.h"

namespace state {
namespace {

// Loop timing over the last 10 s window and the one before it (so the numbers never read empty)
struct LoopWindow {
  uint32_t count = 0, totalUs = 0, maxUs = 0;
};
LoopWindow loopNow, loopLast;
uint32_t loopWindowAt = 0;

bool mqttFailed() { return !strcmp(mqtt::state(), "error"); }

// Problem codes, translated by the panel.
void problems(JsonArray out) {
  if (!storage::ok()) out.add("storage");
  if (mqttFailed()) out.add("mqtt");
}

}  // namespace

const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "powerOn";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_USB: return "usb";
    default: return "other";
  }
}

void recordLoop(uint32_t micros) {
  loopNow.count++;
  loopNow.totalUs += micros;
  loopNow.maxUs = max(loopNow.maxUs, micros);
  if (millis() - loopWindowAt >= 10000) {
    loopWindowAt = millis();
    loopLast = loopNow;
    loopNow = LoopWindow();
  }
}

void system(JsonObject out) {
  out["version"] = FIRMWARE_VERSION;
  out["uptime"] = millis() / 1000;
  out["resetReason"] = resetReason();
  out["chip"] = ESP.getChipModel();
  out["chipRevision"] = ESP.getChipRevision();
  out["cpuMhz"] = ESP.getCpuFreqMHz();
  out["sdk"] = ESP.getSdkVersion();
  out["heap"] = ESP.getFreeHeap();
  out["heapTotal"] = ESP.getHeapSize();
  out["minHeap"] = ESP.getMinFreeHeap();
  out["maxBlock"] = ESP.getMaxAllocHeap();
  out["flash"] = ESP.getFlashChipSize();
  out["firmwareSize"] = ESP.getSketchSize();
  out["firmwareSpace"] = ESP.getFreeSketchSpace();
  const LoopWindow &w = loopLast.count ? loopLast : loopNow;
  out["loopPerSecond"] = w.count / 10;
  out["loopAverageUs"] = w.count ? w.totalUs / w.count : 0;
  out["loopMaxUs"] = w.maxUs;
}

void full(JsonObject out) {
  out["name"] = device::name();
  out["hostname"] = device::hostname();
  out["version"] = FIRMWARE_VERSION;
  out["uptime"] = millis() / 1000;
  out["uptimeMs"] = millis();
  problems(out["problems"].to<JsonArray>());
  ir::status(out["ir"].to<JsonObject>());
  actions::status(out["actions"].to<JsonObject>());
  out["mqtt"] = mqtt::state();
  out["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  out["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : "";
  out["heap"] = ESP.getFreeHeap();
  const time_t now = time(nullptr);
  if (now > CLOCK_SET_AFTER) out["clock"] = now;  // unix time from SNTP
  else out["clock"] = nullptr;
}

bool hasProblem() { return !storage::ok() || mqttFailed(); }

}  // namespace state
