#pragma once
// Webhooks: named HTTP requests that button actions call, defined once and kept apart from buttons,
// so one request can serve many buttons and a token changes in one place. Each is a file on the
// flash file system; only ids and names stay in RAM. Sending happens in the actions task.
//
// URL, headers and body may use {button}, {code}, {gesture}, {controller}, {ip}, {time} and {uptime}.
// Values are encoded for where they land (URL, JSON, form or plain text); other braces,
// such as those of JSON, stay as they are.
#include <ArduinoJson.h>

namespace hooks {

constexpr size_t MAX_HOOKS = 20;  // up to about 3.6 KB each, on a 128 KB partition shared with the buttons
constexpr size_t MAX_NAME = 64, MAX_URL = 512, MAX_HEADERS = 1024, MAX_BODY = 2048;

struct Hook {
  uint8_t id = 0;
  String name, method = "POST", url, headers, body = "json", bodyTemplate;
  bool insecure = false;  // https:// without checking the certificate (self-signed, home servers)
};

// What fills the variables.
struct Variables {
  String button, code, gesture;
};

struct Result {
  bool sent = false;  // a response arrived, with any status
  int status = 0;
  String error;       // why nothing arrived
  String response;    // the start of the response, when asked for
};

void begin();

// With StateLock held:
bool exists(uint8_t id);
void list(JsonArray out);  // id, name, method and url of each
void limits(JsonObject out);
bool load(uint8_t id, Hook &out);
uint8_t create(const String &name);  // the new id, 0 when it cannot be added
bool update(const Hook &hook);       // false when not saved; validate() first
bool remove(uint8_t id);
const char *validate(const Hook &hook);  // nullptr when fine, otherwise an error code for the panel
void toJson(const Hook &hook, JsonObject out);

// Without StateLock, from the actions task only: blocks up to the timeouts.
Result send(uint8_t id, const Variables &variables, bool keepResponse);

}  // namespace hooks
