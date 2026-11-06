#pragma once
// The database of learned buttons with their actions: a JSON file on the flash file system, written
// atomically. Settings stay in NVS, which only has about 20 KB.
// All functions except begin() expect StateLock held.
#include <ArduinoJson.h>

namespace buttons {

constexpr size_t MAX_BUTTONS = 64;
constexpr size_t MAX_NAME_BYTES = 64;  // 32 characters even if all are Polish letters
constexpr size_t MAX_NOTE_BYTES = 400;      // 200 characters even if all are Polish letters
constexpr size_t MAX_ACTIONS_BYTES = 2048;  // per button
constexpr size_t MAX_TOTAL_BYTES = 16384;   // notes and actions of all buttons stay in RAM, which TLS also needs

enum class Save : uint8_t { ok, full, failed };

struct Changes {
  int gestures = -1;                 // 0, 1; -1 keeps the current value
  const String *actions = nullptr;  // validated JSON; nullptr keeps the current ones
  const String *note = nullptr;     // nullptr keeps the current one
};

void begin();
const char *nameOf(const char *code);  // nullptr when the code is not saved
bool usesGestures(const char *code);
bool longRepeats(const char *code);  // holding repeats the long press; once is the default
String actionsOf(const char *code);  // JSON, "{}" when none
Save save(const String &code, const String &name, const Changes &changes = {});  // adds, or updates a known code
bool at(size_t index, String &code, String &name, bool &gestures);
bool remove(const String &code);  // false when not saved or not written
void list(JsonArray out);

}  // namespace buttons
