#pragma once
// The flash file system, for data that outgrows NVS: buttons, webhooks and servers.
#include <ArduinoJson.h>

namespace storage {

void begin();  // mounts it; formats an empty partition on the first start
bool ok();     // false when it could not be mounted: reading finds nothing, writing fails
bool readJson(const char *path, JsonDocument &doc);  // false when missing or damaged
// Goes to a temporary file first, so power loss during a save never damages the old file.
bool writeJson(const char *path, const JsonDocument &doc);

}  // namespace storage
