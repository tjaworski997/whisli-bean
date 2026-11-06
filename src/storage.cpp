#include "storage.h"

#include <LittleFS.h>

#include "eventlog.h"

namespace storage {
namespace {

bool mounted = false;

}  // namespace

void begin() {
  mounted = LittleFS.begin(true);
  if (!mounted) eventlog::error("system", "storageFailed", "mount");
}

bool ok() { return mounted; }

bool readJson(const char *path, JsonDocument &doc) {
  if (!mounted) return false;
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  const DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (!error) return true;
  eventlog::error("system", "storageFailed", path);  // a damaged file must not stop the device
  return false;
}

bool writeJson(const char *path, const JsonDocument &doc) {
  if (!mounted) return false;
  const String temp = String(path) + ".tmp";
  File file = LittleFS.open(temp, "w");
  bool written = false;
  if (file) {
    written = serializeJson(doc, file) == measureJson(doc);
    file.close();
  }
  if (written && LittleFS.rename(temp, path)) return true;
  LittleFS.remove(temp);
  eventlog::error("system", "storageFailed", path);
  return false;
}

}  // namespace storage
