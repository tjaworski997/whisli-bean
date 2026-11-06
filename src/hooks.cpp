#include "hooks.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <time.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "board.h"
#include "device.h"
#include "lock.h"
#include "storage.h"

// The Mozilla CA list built into the framework
extern const uint8_t ca_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t ca_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace hooks {
namespace {

constexpr const char *DIRECTORY = "/hooks";
constexpr uint32_t CONNECT_TIMEOUT_MS = 3000, REPLY_TIMEOUT_MS = 5000, TLS_TIMEOUT_S = 8;
constexpr size_t RESPONSE_BYTES = 400;

struct Entry {
  uint8_t id;
  String name;
};

std::vector<Entry> entries;  // Shared (StateLock)

String pathOf(uint8_t id) { return String(DIRECTORY) + "/h" + id + ".json"; }

bool read(uint8_t id, Hook &out) {
  JsonDocument doc;
  if (!storage::readJson(pathOf(id).c_str(), doc)) return false;
  out = Hook();
  out.id = id;
  out.name = doc["name"] | "";
  out.method = doc["method"] | "POST";
  out.url = doc["url"] | "";
  out.headers = doc["headers"] | "";
  out.body = doc["body"] | "json";
  out.bodyTemplate = doc["bodyTemplate"] | "";
  out.insecure = doc["insecure"] | false;
  return true;
}

bool write(const Hook &hook) {
  JsonDocument doc;
  doc["name"] = hook.name;
  doc["method"] = hook.method;
  doc["url"] = hook.url;
  doc["headers"] = hook.headers;
  doc["body"] = hook.body;
  doc["bodyTemplate"] = hook.bodyTemplate;
  doc["insecure"] = hook.insecure;
  return storage::writeJson(pathOf(hook.id).c_str(), doc);
}

Entry *find(uint8_t id) {
  for (Entry &entry : entries)
    if (entry.id == id) return &entry;
  return nullptr;
}

enum class Encoding : uint8_t { plain, url, json };

String encodeValue(const String &value, Encoding encoding) {
  if (encoding == Encoding::plain) return value;
  String out;
  if (encoding == Encoding::json) {
    JsonDocument doc;
    doc.set(value);
    serializeJson(doc, out);
    return out.substring(1, out.length() - 1);  // without the quotes: the template has its own
  }
  char hex[4];
  for (const char c : value) {
    if (isalnum((uint8_t)c) || strchr("-_.~", c)) {
      out += c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", (uint8_t)c);
      out += hex;
    }
  }
  return out;
}

struct Values {
  String button, code, gesture, controller, ip, time, uptime;
};

String expand(const String &text, const Values &v, Encoding encoding) {
  String out = text;
  const std::pair<const char *, const String *> names[] = {{"{button}", &v.button}, {"{code}", &v.code}, {"{gesture}", &v.gesture},
                                                           {"{controller}", &v.controller}, {"{ip}", &v.ip}, {"{time}", &v.time}, {"{uptime}", &v.uptime}};
  for (const auto &name : names)
    if (out.indexOf(name.first) >= 0) out.replace(name.first, encodeValue(*name.second, encoding));
  return out;
}

Values valuesFrom(const Variables &variables) {
  Values v;
  v.button = variables.button;
  v.code = variables.code;
  v.gesture = variables.gesture;
  {
    StateLock lock;
    v.controller = device::name();
  }
  v.ip = WiFi.localIP().toString();
  v.uptime = String(millis() / 1000);
  const time_t now = time(nullptr);
  if (now > CLOCK_SET_AFTER) {
    char text[24];
    strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    v.time = text;
  }
  return v;
}

void addHeaders(HTTPClient &http, const String &headers, const Values &values, bool &hasContentType) {
  int start = 0;
  while (start < (int)headers.length()) {
    int end = headers.indexOf('\n', start);
    if (end < 0) end = headers.length();
    const String line = headers.substring(start, end);
    start = end + 1;
    const int colon = line.indexOf(':');
    if (colon <= 0) continue;
    String name = line.substring(0, colon), value = line.substring(colon + 1);
    name.trim();
    value.trim();
    if (name.equalsIgnoreCase("Content-Type")) hasContentType = true;
    // A button's name may hold anything, and a line break there would start a header of its own.
    String expanded = expand(value, values, Encoding::plain);
    expanded.replace("\r", " ");
    expanded.replace("\n", " ");
    http.addHeader(name, expanded);
  }
}

const char *contentType(const String &body) {
  if (body == "json") return "application/json";
  if (body == "form") return "application/x-www-form-urlencoded";
  return "text/plain; charset=utf-8";
}

}  // namespace

void begin() {
  if (!storage::ok()) return;
  if (!LittleFS.exists(DIRECTORY)) LittleFS.mkdir(DIRECTORY);
  File directory = LittleFS.open(DIRECTORY);
  if (!directory) return;
  for (File file = directory.openNextFile(); file; file = directory.openNextFile()) {
    const String name = file.name();
    file.close();
    if (!name.startsWith("h") || !name.endsWith(".json")) continue;
    const long id = name.substring(1).toInt();
    Hook hook;
    if (id >= 1 && id <= 255 && read(id, hook)) entries.push_back({(uint8_t)id, hook.name});
  }
  std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.id < b.id; });
}

bool exists(uint8_t id) { return find(id); }

void list(JsonArray out) {
  for (const Entry &entry : entries) {
    Hook hook;
    if (!read(entry.id, hook)) continue;
    JsonObject item = out.add<JsonObject>();
    item["id"] = hook.id;
    item["name"] = hook.name;
    item["method"] = hook.method;
    item["url"] = hook.url;
  }
}

void limits(JsonObject out) {
  out["count"] = MAX_HOOKS;
  out["name"] = MAX_NAME;
  out["url"] = MAX_URL;
  out["headers"] = MAX_HEADERS;
  out["body"] = MAX_BODY;
}

bool load(uint8_t id, Hook &out) { return find(id) && read(id, out); }

uint8_t create(const String &name) {
  if (entries.size() >= MAX_HOOKS || name.isEmpty() || name.length() > MAX_NAME) return 0;
  uint8_t id = 1;
  while (find(id)) id++;
  Hook hook;
  hook.id = id;
  hook.name = name;
  if (!write(hook)) return 0;
  entries.push_back({id, name});
  return id;
}

const char *validate(const Hook &hook) {
  if (hook.name.isEmpty() || hook.name.length() > MAX_NAME) return "invalidName";
  // Empty is allowed: a new webhook is saved field by field. It just cannot be sent yet.
  if (hook.url.length() && !hook.url.startsWith("http://") && !hook.url.startsWith("https://")) return "invalidUrl";
  if (hook.url.length() > MAX_URL || hook.headers.length() > MAX_HEADERS || hook.bodyTemplate.length() > MAX_BODY) return "tooLong";
  if (hook.method != "GET" && hook.method != "POST" && hook.method != "PUT" && hook.method != "DELETE") return "invalid";
  if (hook.body != "none" && hook.body != "json" && hook.body != "form" && hook.body != "text") return "invalid";
  return nullptr;
}

bool update(const Hook &hook) {
  Entry *entry = find(hook.id);
  if (!entry || !write(hook)) return false;
  entry->name = hook.name;
  return true;
}

bool remove(uint8_t id) {
  Entry *entry = find(id);
  if (!entry || !LittleFS.remove(pathOf(id))) return false;
  entries.erase(entries.begin() + (entry - entries.data()));
  return true;
}

void toJson(const Hook &hook, JsonObject out) {
  out["id"] = hook.id;
  out["name"] = hook.name;
  out["method"] = hook.method;
  out["url"] = hook.url;
  out["headers"] = hook.headers;
  out["body"] = hook.body;
  out["bodyTemplate"] = hook.bodyTemplate;
  out["insecure"] = hook.insecure;
}

Result send(uint8_t id, const Variables &variables, bool keepResponse) {
  Result result;
  Hook hook;
  {
    StateLock lock;
    if (!load(id, hook)) {
      result.error = "webhook not found";
      return result;
    }
  }
  if (hook.url.isEmpty()) {
    result.error = "no URL";
    return result;
  }
  if (!WiFi.isConnected()) {
    result.error = "no Wi-Fi";
    return result;
  }
  const Values values = valuesFrom(variables);
  const String url = expand(hook.url, values, Encoding::url);
  const bool secure = url.startsWith("https://");
  NetworkClient plain;
  NetworkClientSecure tls;
  if (secure) {
    tls.setHandshakeTimeout(TLS_TIMEOUT_S);
    if (hook.insecure) tls.setInsecure();
    else tls.setCACertBundle(ca_bundle_start, ca_bundle_end - ca_bundle_start);
  }
  HTTPClient http;
  http.setConnectTimeout(CONNECT_TIMEOUT_MS);
  http.setTimeout(REPLY_TIMEOUT_MS);
  if (!http.begin(secure ? static_cast<NetworkClient &>(tls) : plain, url)) {
    result.error = "bad URL";
    return result;
  }
  bool hasContentType = false;
  addHeaders(http, hook.headers, values, hasContentType);
  const bool hasBody = hook.method != "GET" && hook.body != "none";
  String body;
  if (hasBody) {
    body = expand(hook.bodyTemplate, values, hook.body == "json" ? Encoding::json : hook.body == "form" ? Encoding::url : Encoding::plain);
    if (!hasContentType) http.addHeader("Content-Type", contentType(hook.body));
  }
  const int status = hook.method == "GET" ? http.GET() : http.sendRequest(hook.method.c_str(), body);
  if (status > 0) {
    result.sent = true;
    result.status = status;
    if (keepResponse) {
      // Only the start: an answer of any size must not run the device out of memory.
      NetworkClient *stream = http.getStreamPtr();
      const uint32_t startedAt = millis();
      while (stream && result.response.length() < RESPONSE_BYTES && (stream->connected() || stream->available()) && millis() - startedAt < 1000) {
        if (stream->available()) result.response += (char)stream->read();
        else delay(5);
      }
    }
  } else {
    result.error = HTTPClient::errorToString(status);
    if (secure) {  // HTTPClient calls any TLS failure "connection refused"
      char reason[80] = "";
      if (tls.lastError(reason, sizeof(reason)) && reason[0]) result.error = reason;
    }
  }
  http.end();
  return result;
}

}  // namespace hooks
