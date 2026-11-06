#include "web.h"

#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <WiFi.h>

#include <functional>

#include "actions.h"
#include "auth.h"
#include "buttons.h"
#include "device.h"
#include "eventlog.h"
#include "generated/ui.h"
#include "generated/version.h"
#include "hooks.h"
#include "ir.h"
#include "ir_decode.h"
#include "led.h"
#include "lock.h"
#include "mqtt.h"
#include "net.h"
#include "servers.h"
#include "state.h"

namespace web {
namespace {

using Request = AsyncWebServerRequest *;

constexpr uint32_t STATE_EVERY_MS = 1000;

AsyncWebServer server(80);
AsyncWebSocket panelSocket("/api/v1/ws");
// Set in the web server task, handled in loop()
volatile bool panelJoined = false;
volatile uint32_t restartAt = 0;

// --- Replies ---

void reply(Request r, JsonDocument &doc, int code = 200) {
  String body;
  serializeJson(doc, body);
  r->send(code, "application/json", body);
}

void ok(Request r) {
  JsonDocument doc;
  doc["ok"] = true;
  reply(r, doc);
}

void fail(Request r, int code, const char *error) {
  JsonDocument doc;
  doc["error"] = error;  // the panel translates it
  reply(r, doc, code);
}

enum class Access { open, signedIn };

// Handlers run under StateLock, so they read and change module state directly.
void on(const char *path, WebRequestMethodComposite method, Access access, std::function<void(Request)> handler) {
  server.on(AsyncURIMatcher::exact(path), method, [access, handler](Request r) {
    if (access == Access::signedIn && !auth::allowed(r)) return fail(r, 401, "unauthorized");
    StateLock lock;
    handler(r);
  });
}

// --- Form fields ---

String text(Request r, const char *name) {
  String value = r->hasArg(name) ? r->arg(name) : String();
  value.trim();
  return value;
}

bool flag(Request r, const char *name, bool fallback) {
  if (!r->hasArg(name)) return fallback;
  return r->arg(name) == "1" || r->arg(name) == "true";
}

// Optional whole number: absent leaves `out` as is; false when present but not a number in range.
bool number(Request r, const char *name, long min, long max, int32_t &out) {
  if (!r->hasArg(name)) return true;
  const String &value = r->arg(name);
  char *end;
  const long n = strtol(value.c_str(), &end, 10);
  if (value.isEmpty() || *end || n < min || n > max) return false;
  out = n;
  return true;
}

// The id of a webhook or server; 0, which is never used, when absent or wrong.
uint8_t idArg(Request r) {
  int32_t id = 0;
  return number(r, "id", 1, 255, id) ? id : 0;
}

// Codes end up in MQTT topics, so they may not hold "/", "#", "+" or anything else with a meaning there.
bool validCode(const String &code) {
  if (code.isEmpty() || code.length() >= sizeof(Decoded::code)) return false;
  for (const char c : code)
    if (!isalnum((uint8_t)c) && c != '-') return false;
  return true;
}

// --- Routes ---

void authRoutes() {
  on("/api/v1/auth", HTTP_GET, Access::open, [](Request r) {
    JsonDocument doc;
    doc["signedIn"] = auth::allowed(r);
    doc["defaultPassword"] = auth::usingDefault();
    // Shown on the login screen, so you know which device you are signing in to
    doc["name"] = device::name();
    doc["hostname"] = device::hostname();
    doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    reply(r, doc);
  });
  on("/api/v1/auth/login", HTTP_POST, Access::open, [](Request r) {
    String token;
    const bool remember = flag(r, "remember", false);
    switch (auth::login(text(r, "password"), remember, token)) {
      case auth::Login::wrongPassword:
        eventlog::warning("system", "loginFailed", r->client()->remoteIP().toString().c_str());
        return fail(r, 403, "wrongPassword");
      case auth::Login::ok: break;
    }
    AsyncWebServerResponse *res = r->beginResponse(200, "application/json", "{\"ok\":true}");
    res->addHeader("Set-Cookie", String(auth::COOKIE) + "=" + token + "; Path=/; HttpOnly; SameSite=Strict" +
                                     (remember ? "; Max-Age=" + String(auth::REMEMBER_DAYS * 86400UL) : ""));
    r->send(res);
  });
  on("/api/v1/auth/logout", HTTP_POST, Access::open, [](Request r) {
    auth::logout(r);
    AsyncWebServerResponse *res = r->beginResponse(200, "application/json", "{\"ok\":true}");
    res->addHeader("Set-Cookie", String(auth::COOKIE) + "=; Path=/; Max-Age=0");
    r->send(res);
  });
  on("/api/v1/auth/password", HTTP_POST, Access::signedIn, [](Request r) {
    const String next = text(r, "password");
    if (next.length() < 8 || next.length() > 64) return fail(r, 400, "passwordLength");
    if (!auth::changePassword(text(r, "current"), next)) return fail(r, 403, "wrongPassword");
    eventlog::info("system", "passwordChanged");
    ok(r);
  });
}

void irRoutes() {
  // Only frames newer than ?since=<id>, so polling stays small.
  on("/api/v1/ir", HTTP_GET, Access::signedIn, [](Request r) {
    int32_t since = 0;
    if (!number(r, "since", 0, INT32_MAX, since)) return fail(r, 400, "invalid");
    JsonDocument doc;
    ir::frames(doc.to<JsonObject>(), since);
    reply(r, doc);
  });
  on("/api/v1/ir/learn", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    ir::learnStatus(doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/ir/learn", HTTP_POST, Access::signedIn, [](Request r) {
    ir::startLearning();
    JsonDocument doc;
    ir::learnStatus(doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/ir/learn/cancel", HTTP_POST, Access::signedIn, [](Request r) {
    ir::cancelLearning();
    ok(r);
  });

  on("/api/v1/buttons", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    buttons::list(doc["buttons"].to<JsonArray>());
    actions::runs(doc["runs"].to<JsonArray>());
    reply(r, doc);
  });
  // Saves a new button or updates a known one. Fields: code, name; optional, kept when absent:
  // note, gestures (1/0), actions (JSON, see actions.h).
  on("/api/v1/buttons", HTTP_POST, Access::signedIn, [](Request r) {
    const String code = text(r, "code"), name = text(r, "name");
    if (!validCode(code)) return fail(r, 400, "invalidCode");
    if (name.isEmpty() || name.length() > buttons::MAX_NAME_BYTES) return fail(r, 400, "invalidName");
    buttons::Changes changes;
    String note;
    if (r->hasArg("note")) {
      note = text(r, "note");
      if (note.length() > buttons::MAX_NOTE_BYTES) return fail(r, 400, "tooLong");
      changes.note = &note;
    }
    if (r->hasArg("gestures")) changes.gestures = flag(r, "gestures", false);
    String actionsJson;
    if (r->hasArg("actions")) {
      if (!actions::validate(r->arg("actions"), actionsJson)) return fail(r, 400, "invalidAction");
      changes.actions = &actionsJson;
    }
    switch (buttons::save(code, name, changes)) {
      case buttons::Save::full: return fail(r, 409, "full");
      case buttons::Save::failed: return fail(r, 500, "saveFailed");
      case buttons::Save::ok: break;
    }
    if (changes.actions) actions::forgetRun(code);
    ir::learningSaved(code);
    mqtt::buttonChanged(code);
    eventlog::info("ir", "buttonSaved", name.c_str());
    JsonDocument doc;
    buttons::list(doc["buttons"].to<JsonArray>());
    reply(r, doc);
  });
  // Runs a saved button's gesture as if the remote had sent it. Fields: code, gesture.
  on("/api/v1/buttons/test", HTTP_POST, Access::signedIn, [](Request r) {
    const String code = text(r, "code");
    Gesture gesture;
    if (!buttons::nameOf(code.c_str())) return fail(r, 404, "notFound");
    if (!gestureFromName(text(r, "gesture").c_str(), gesture)) return fail(r, 400, "invalid");
    ir::fire(code.c_str(), gesture);
    ok(r);
  });
  on("/api/v1/buttons/delete", HTTP_POST, Access::signedIn, [](Request r) {
    const String code = text(r, "code");
    if (!buttons::remove(code)) return fail(r, 404, "notFound");
    actions::forgetRun(code);
    mqtt::buttonRemoved(code);
    eventlog::info("ir", "buttonDeleted", code.c_str());
    JsonDocument doc;
    buttons::list(doc["buttons"].to<JsonArray>());
    reply(r, doc);
  });
}

void hookRoutes() {
  on("/api/v1/hooks", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    hooks::list(doc["hooks"].to<JsonArray>());
    hooks::limits(doc["limits"].to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/hooks", HTTP_POST, Access::signedIn, [](Request r) {
    const uint8_t id = hooks::create(text(r, "name"));
    if (!id) return fail(r, 409, "full");
    JsonDocument doc;
    doc["id"] = id;
    reply(r, doc);
  });
  on("/api/v1/hooks/detail", HTTP_GET, Access::signedIn, [](Request r) {
    hooks::Hook hook;
    if (!hooks::load(idArg(r), hook)) return fail(r, 404, "notFound");
    JsonDocument doc;
    hooks::toJson(hook, doc.to<JsonObject>());
    reply(r, doc);
  });
  // Fields: id, name, method (GET, POST, PUT, DELETE), url, headers ("Name: value" lines), body
  // (none, json, form, text), bodyTemplate, insecure (1/0).
  on("/api/v1/hooks/update", HTTP_POST, Access::signedIn, [](Request r) {
    hooks::Hook hook;
    hook.id = idArg(r);
    hook.name = text(r, "name");
    hook.method = text(r, "method");
    hook.url = text(r, "url");
    hook.headers = r->arg("headers");
    hook.body = text(r, "body");
    hook.bodyTemplate = r->arg("bodyTemplate");
    hook.insecure = flag(r, "insecure", false);
    if (!hooks::exists(hook.id)) return fail(r, 404, "notFound");
    if (const char *error = hooks::validate(hook)) return fail(r, 400, error);
    if (!hooks::update(hook)) return fail(r, 500, "saveFailed");
    ok(r);
  });
  on("/api/v1/hooks/remove", HTTP_POST, Access::signedIn, [](Request r) {
    if (!hooks::remove(idArg(r))) return fail(r, 404, "notFound");
    ok(r);
  });
  // Sends in the background; GET tells how it went.
  on("/api/v1/hooks/test", HTTP_POST, Access::signedIn, [](Request r) {
    const uint8_t id = idArg(r);
    if (!hooks::exists(id)) return fail(r, 404, "notFound");
    if (!actions::testHook(id)) return fail(r, 409, "busy");
    ok(r);
  });
  on("/api/v1/hooks/test", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    actions::hookTestStatus(doc.to<JsonObject>());
    reply(r, doc);
  });
}

void serverRoutes() {
  on("/api/v1/servers", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    servers::list(doc["servers"].to<JsonArray>());
    doc["limit"] = servers::MAX_SERVERS;
    reply(r, doc);
  });
  on("/api/v1/servers", HTTP_POST, Access::signedIn, [](Request r) {
    const uint8_t id = servers::create(text(r, "name"));
    if (!id) return fail(r, 409, "full");
    JsonDocument doc;
    doc["id"] = id;
    reply(r, doc);
  });
  // Fields: id, name, mac (AA:BB:CC:DD:EE:FF), broadcast (empty: this network).
  on("/api/v1/servers/update", HTTP_POST, Access::signedIn, [](Request r) {
    servers::Server server;
    server.id = idArg(r);
    server.name = text(r, "name");
    server.mac = text(r, "mac");
    server.broadcast = text(r, "broadcast");
    if (!servers::exists(server.id)) return fail(r, 404, "notFound");
    if (const char *error = servers::validate(server)) return fail(r, 400, error);
    if (!servers::update(server)) return fail(r, 500, "saveFailed");
    ok(r);
  });
  on("/api/v1/servers/remove", HTTP_POST, Access::signedIn, [](Request r) {
    if (!servers::remove(idArg(r))) return fail(r, 404, "notFound");
    ok(r);
  });
  // One UDP packet with no answer to wait for, so it is fine under the lock.
  on("/api/v1/servers/wake", HTTP_POST, Access::signedIn, [](Request r) {
    servers::Server server;
    if (!servers::load(idArg(r), server)) return fail(r, 404, "notFound");
    String detail;
    const bool sent = servers::wake(server, detail);
    JsonDocument doc;
    doc["sent"] = sent;
    doc["detail"] = detail;
    reply(r, doc);
  });
}

void networkRoutes() {
  on("/api/v1/wifi", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    net::status(doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/wifi", HTTP_POST, Access::signedIn, [](Request r) {
    const String ssid = text(r, "ssid"), password = r->hasArg("password") ? r->arg("password") : String();
    if (ssid.isEmpty() || ssid.length() > 32) return fail(r, 400, "invalidSsid");
    if (password.length() && (password.length() < 8 || password.length() > 63)) return fail(r, 400, "wifiPasswordLength");
    net::connect(ssid, password);
    ok(r);
  });
  on("/api/v1/wifi/forget", HTTP_POST, Access::signedIn, [](Request r) {
    net::forget();
    ok(r);
  });
  on("/api/v1/wifi/scan", HTTP_POST, Access::signedIn, [](Request r) {
    net::startScan();
    ok(r);
  });
  on("/api/v1/wifi/scan", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    net::scanResults(doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/wifi/ap", HTTP_POST, Access::signedIn, [](Request r) {
    const String password = r->hasArg("password") ? r->arg("password") : String();
    if (password.length() < 8 || password.length() > 63) return fail(r, 400, "wifiPasswordLength");
    net::setApPassword(password);
    ok(r);
  });

  // The password never leaves the device; GET only says whether one is set.
  on("/api/v1/mqtt", HTTP_GET, Access::signedIn, [](Request r) {
    const mqtt::Settings s = mqtt::settings();
    JsonDocument doc;
    doc["enabled"] = s.enabled;
    doc["host"] = s.host;
    doc["port"] = s.port;
    doc["user"] = s.user;
    doc["hasPassword"] = !s.password.isEmpty();
    doc["baseTopic"] = s.baseTopic;
    doc["defaultBaseTopic"] = mqtt::defaultBaseTopic();
    doc["discovery"] = s.discovery;
    doc["discoveryPrefix"] = s.discoveryPrefix;
    doc["state"] = mqtt::state();
    doc["error"] = mqtt::error();
    reply(r, doc);
  });
  // Fields, all optional: enabled, host, port, user, password, clearPassword, baseTopic, discovery,
  // discoveryPrefix
  on("/api/v1/mqtt", HTTP_POST, Access::signedIn, [](Request r) {
    mqtt::Settings s = mqtt::settings();
    int32_t port = s.port;
    s.enabled = flag(r, "enabled", s.enabled);
    if (r->hasArg("host")) s.host = text(r, "host");
    if (r->hasArg("user")) s.user = text(r, "user");
    if (r->hasArg("password")) s.password = r->arg("password");
    if (flag(r, "clearPassword", false)) s.password = "";
    if (r->hasArg("baseTopic")) s.baseTopic = text(r, "baseTopic");
    s.discovery = flag(r, "discovery", s.discovery);
    if (r->hasArg("discoveryPrefix")) s.discoveryPrefix = text(r, "discoveryPrefix");
    if (!number(r, "port", 1, 65535, port)) return fail(r, 400, "invalidPort");
    s.port = port;
    if (s.enabled && s.host.isEmpty()) return fail(r, 400, "invalidHost");
    if (s.host.length() > 64 || s.user.length() > 64 || s.password.length() > 64) return fail(r, 400, "invalid");
    if ((s.baseTopic.length() && !mqtt::validTopic(s.baseTopic)) || !mqtt::validTopic(s.discoveryPrefix))
      return fail(r, 400, "invalidTopic");
    mqtt::save(s);
    eventlog::info("mqtt", "mqttSaved", s.enabled ? s.host.c_str() : "");
    ok(r);
  });
}

void systemRoutes() {
  on("/api/v1/state", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    state::full(doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/device", HTTP_POST, Access::signedIn, [](Request r) {
    if (!device::setName(text(r, "name"))) return fail(r, 400, "invalidName");
    eventlog::info("system", "renamed", device::name().c_str());
    JsonDocument doc;
    doc["hostname"] = device::hostname();
    reply(r, doc);
  });
  // The LED. Fields, all optional: mode (off, status, received, sent), brightness (1-100 %), alerts (1/0).
  on("/api/v1/led", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    led::toJson(led::settings(), doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/led", HTTP_POST, Access::signedIn, [](Request r) {
    led::Settings s = led::settings();
    int32_t brightness = s.brightness;
    const String mode = text(r, "mode");
    if ((mode.length() && !led::modeFromName(mode.c_str(), s)) || !number(r, "brightness", 1, 100, brightness)) return fail(r, 400, "invalid");
    s.brightness = brightness;
    s.alerts = flag(r, "alerts", s.alerts);
    led::save(s);
    JsonDocument doc;
    led::toJson(s, doc.to<JsonObject>());
    reply(r, doc);
  });
  on("/api/v1/system", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    state::system(doc.to<JsonObject>());
    doc["id"] = device::id();
    doc["mac"] = WiFi.macAddress();
    reply(r, doc);
  });
  on("/api/v1/system/restart", HTTP_POST, Access::signedIn, [](Request r) {
    eventlog::info("system", "restart");
    restartAt = millis() + 500;  // after the reply went out
    ok(r);
  });
  on("/api/v1/diagnostics", HTTP_GET, Access::signedIn, [](Request r) {
    JsonDocument doc;
    state::system(doc["system"].to<JsonObject>());
    ir::diagnostics(doc["ir"].to<JsonObject>());
    net::diagnostics(doc["network"].to<JsonObject>());
    mqtt::diagnostics(doc["mqtt"].to<JsonObject>());
    reply(r, doc);
  });
  // Event log, newest last. Optional: after / before (entry id), limit (1-120, default 60),
  // level (info, warning, error), category (system, network, ir, mqtt).
  on("/api/v1/logs", HTTP_GET, Access::signedIn, [](Request r) {
    int32_t after = 0, before = 0, limit = 60;
    if (!number(r, "after", 0, INT32_MAX, after) || !number(r, "before", 0, INT32_MAX, before) || !number(r, "limit", 1, 120, limit))
      return fail(r, 400, "invalid");
    JsonDocument doc;
    eventlog::toJson(doc.to<JsonObject>(), after, before, limit, text(r, "level").c_str(), text(r, "category").c_str());
    reply(r, doc);
  });

  // New firmware as the raw request body (Content-Type: application/octet-stream), e.g.
  // curl -u whisli:<password> -H "Content-Type: application/octet-stream" --data-binary @firmware.bin
  //   http://<ip>/api/v1/firmware
  // The body arrives in chunks in the web server task; flash writes happen there, outside StateLock.
  server.on(
      AsyncURIMatcher::exact("/api/v1/firmware"), HTTP_POST,
      [](Request r) {
        if (!auth::allowed(r)) return fail(r, 401, "unauthorized");
        if (Update.hasError() || !Update.isFinished()) {
          JsonDocument doc;
          doc["error"] = "firmwareFailed";
          doc["detail"] = Update.errorString();
          return reply(r, doc, 500);
        }
        restartAt = millis() + 1000;
        ok(r);
      },
      nullptr,
      [](Request r, uint8_t *data, size_t length, size_t index, size_t total) {
        if (index == 0) {
          if (!auth::allowed(r)) return;
          Serial.printf("Firmware update: %u B\n", total);
          eventlog::info("system", "firmwareStart", (String(total / 1024) + " KB").c_str());
          if (!Update.begin(total)) eventlog::error("system", "firmwareFailed", Update.errorString());
        }
        if (!Update.isRunning()) return;
        if (Update.write(data, length) != length) Update.printError(Serial);
        if (index + length == total) {
          if (Update.end(true)) eventlog::info("system", "firmwareOk");
          else eventlog::error("system", "firmwareFailed", Update.errorString());
        }
      });
}

void broadcastState() {
  String json;
  {
    StateLock lock;
    JsonDocument doc;
    state::full(doc.to<JsonObject>());
    serializeJson(doc, json);
  }
  panelSocket.textAll(json);  // outside StateLock: the socket has its own lock
}

}  // namespace

void begin() {
  server.on(AsyncURIMatcher::exact("/"), HTTP_GET, [](Request r) {
    AsyncWebServerResponse *res = r->beginResponse(200, "text/html", UI_GZ, sizeof(UI_GZ));
    res->addHeader("Content-Encoding", "gzip");
    res->addHeader("Cache-Control", "no-cache");
    r->send(res);
  });

  // Socket event handlers must not take StateLock (the socket's lock may be held); they set flags.
  panelSocket.handleHandshake([](Request r) { return auth::allowed(r); });
  panelSocket.onEvent([](AsyncWebSocket *, AsyncWebSocketClient *, AwsEventType type, void *, uint8_t *, size_t) {
    if (type == WS_EVT_CONNECT) panelJoined = true;
  });
  server.addHandler(&panelSocket);

  authRoutes();
  irRoutes();
  hookRoutes();
  serverRoutes();
  networkRoutes();
  systemRoutes();

  server.onNotFound([](Request r) {
    if (r->url().startsWith("/api/")) return fail(r, 404, "notFound");
    r->redirect("/");  // also brings phones on the setup access point to the panel
  });
  server.begin();
}

void loop() {
  static uint32_t stateAt = 0, cleanupAt = 0, sentRevision = 0;
  const uint32_t now = millis();

  uint32_t revision;
  {
    StateLock lock;
    revision = ir::revision() + actions::revision();
  }
  if (panelSocket.count() && (revision != sentRevision || panelJoined || now - stateAt >= STATE_EVERY_MS)) {
    panelJoined = false;
    sentRevision = revision;
    stateAt = now;
    broadcastState();
  }
  if (now - cleanupAt >= 1000) {
    cleanupAt = now;
    panelSocket.cleanupClients();
  }
  if (restartAt && (int32_t)(now - restartAt) >= 0) ESP.restart();
}

}  // namespace web
