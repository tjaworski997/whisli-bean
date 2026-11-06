#include "buttons.h"

#include <vector>

#include "storage.h"

namespace buttons {
namespace {

constexpr const char *FILE_PATH = "/buttons.json";
constexpr int FORMAT_VERSION = 3;  // 1 had only code and name; lists of actions in 2 now count as none

struct Button {
  String code, name, note;
  bool gestures = false;
  String actions = "{}";
  bool repeat = false;  // read from actions, because frames ask for it many times a second
};

std::vector<Button> all;

Button *find(const char *code) {
  for (Button &button : all)
    if (button.code == code) return &button;
  return nullptr;
}

bool repeatsIn(const String &actions) {
  JsonDocument doc;
  return !deserializeJson(doc, actions) && !(doc["long"]["once"] | true);
}

void add(JsonArrayConst items) {
  for (JsonObjectConst item : items)
    if (item["code"].is<const char *>() && item["name"].is<const char *>() && all.size() < MAX_BUTTONS) {
      Button button;
      button.code = item["code"].as<String>();
      button.name = item["name"].as<String>();
      button.note = item["note"] | "";
      button.gestures = item["gestures"] | false;
      if (item["actions"].is<JsonObjectConst>()) {
        button.actions = "";
        serializeJson(item["actions"], button.actions);
      }
      button.repeat = repeatsIn(button.actions);
      all.push_back(button);
    }
}

bool write() {
  JsonDocument doc;
  doc["version"] = FORMAT_VERSION;
  list(doc["buttons"].to<JsonArray>());
  return storage::writeJson(FILE_PATH, doc);
}

}  // namespace

void begin() {
  JsonDocument doc;
  if (storage::readJson(FILE_PATH, doc)) add(doc["buttons"].as<JsonArrayConst>());
}

const char *nameOf(const char *code) {
  const Button *button = find(code);
  return button ? button->name.c_str() : nullptr;
}

bool usesGestures(const char *code) {
  const Button *button = find(code);
  return button && button->gestures;
}

bool longRepeats(const char *code) {
  const Button *button = find(code);
  return button && button->repeat;
}

String actionsOf(const char *code) {
  const Button *button = find(code);
  return button ? button->actions : String("{}");
}

Save save(const String &code, const String &name, const Changes &changes) {
  Button *button = find(code.c_str());
  if (!button && all.size() >= MAX_BUTTONS) return Save::full;
  const String &newActions = changes.actions ? *changes.actions : (button ? button->actions : String());
  const String &newNote = changes.note ? *changes.note : (button ? button->note : String());
  size_t total = newActions.length() + newNote.length();
  for (const Button &other : all)
    if (&other != button) total += other.actions.length() + other.note.length();
  if (newActions.length() > MAX_ACTIONS_BYTES || total > MAX_TOTAL_BYTES) return Save::full;
  if (!button) {
    all.emplace_back();
    button = &all.back();
    button->code = code;
  }
  button->name = name;
  if (changes.note) button->note = *changes.note;
  if (changes.gestures >= 0) button->gestures = changes.gestures;
  if (changes.actions) {
    button->actions = *changes.actions;
    button->repeat = repeatsIn(button->actions);
  }
  return write() ? Save::ok : Save::failed;
}

bool at(size_t index, String &code, String &name, bool &gestures) {
  if (index >= all.size()) return false;
  code = all[index].code;
  name = all[index].name;
  gestures = all[index].gestures;
  return true;
}

bool remove(const String &code) {
  Button *button = find(code.c_str());
  if (!button) return false;
  all.erase(all.begin() + (button - all.data()));
  return write();
}

void list(JsonArray out) {
  for (const Button &button : all) {
    JsonObject item = out.add<JsonObject>();
    item["code"] = button.code;
    item["name"] = button.name;
    if (button.note.length()) item["note"] = button.note;
    item["gestures"] = button.gestures;
    item["actions"] = serialized(button.actions);
  }
}

}  // namespace buttons
