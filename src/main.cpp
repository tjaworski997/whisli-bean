// Whisli Bean - turns any IR remote into a smart home controller.
// Panel at http://<ip>/ (or http://whisli-<name>.local/), JSON API under /api/v1.

#include <Arduino.h>
#include <time.h>

#include "actions.h"
#include "auth.h"
#include "buttons.h"
#include "device.h"
#include "eventlog.h"
#include "generated/version.h"
#include "hooks.h"
#include "ir.h"
#include "led.h"
#include "lock.h"
#include "mqtt.h"
#include "net.h"
#include "servers.h"
#include "state.h"
#include "storage.h"
#include "web.h"

void setup() {
  led::begin();
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // without a USB host reading, writes would block loop()
  Serial.printf("\nWhisli Bean %s\n", FIRMWARE_VERSION);
  eventlog::info("system", "boot", (String(FIRMWARE_VERSION) + " · " + state::resetReason()).c_str());

  device::begin();
  auth::begin();
  storage::begin();
  buttons::begin();
  hooks::begin();
  servers::begin();
  net::begin();
  configTime(0, 0, "pool.ntp.org", "time.google.com");  // UTC: for session expiry and log times
  mqtt::begin();
  web::begin();
  actions::begin();
  ir::begin();
}

void loop() {
  const uint32_t startedUs = micros();
  ir::loop();
  net::loop();
  mqtt::loop();
  web::loop();
  auth::loop();
  bool problem;
  {
    StateLock lock;
    state::recordLoop(micros() - startedUs);
    problem = state::hasProblem();
  }
  led::show(auth::resetting(), !net::connected(), problem);
  delay(1);  // let lower priority tasks (idle, watchdog) run
}
