#include "servers.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include <vector>

#include "storage.h"

namespace servers {
namespace {

constexpr const char *FILE_PATH = "/servers.json";
constexpr uint16_t WOL_PORT = 9;

std::vector<Server> all;

Server *find(uint8_t id) {
  for (Server &server : all)
    if (server.id == id) return &server;
  return nullptr;
}

bool parseMac(const char *text, uint8_t *mac) {
  unsigned v[6];
  if (strlen(text) != 17 || sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) mac[i] = v[i];
  return true;
}

bool write() {
  JsonDocument doc;
  list(doc["servers"].to<JsonArray>());
  return storage::writeJson(FILE_PATH, doc);
}

}  // namespace

void begin() {
  JsonDocument doc;
  if (!storage::readJson(FILE_PATH, doc)) return;
  for (JsonObjectConst item : doc["servers"].as<JsonArrayConst>()) {
    Server server;
    server.id = item["id"] | 0;
    server.name = item["name"] | "";
    server.mac = item["mac"] | "";
    server.broadcast = item["broadcast"] | "";
    if (server.id && all.size() < MAX_SERVERS) all.push_back(server);
  }
}

bool exists(uint8_t id) { return find(id); }

bool load(uint8_t id, Server &out) {
  const Server *server = find(id);
  if (!server) return false;
  out = *server;
  return true;
}

void list(JsonArray out) {
  for (const Server &server : all) {
    JsonObject item = out.add<JsonObject>();
    item["id"] = server.id;
    item["name"] = server.name;
    item["mac"] = server.mac;
    item["broadcast"] = server.broadcast;
  }
}

uint8_t create(const String &name) {
  if (all.size() >= MAX_SERVERS || name.isEmpty() || name.length() > MAX_NAME) return 0;
  uint8_t id = 1;
  while (find(id)) id++;
  Server server;
  server.id = id;
  server.name = name;
  all.push_back(server);
  if (write()) return id;
  all.pop_back();
  return 0;
}

const char *validate(const Server &server) {
  if (server.name.isEmpty() || server.name.length() > MAX_NAME) return "invalidName";
  uint8_t mac[6];
  // Empty is allowed: a new server is saved field by field. It just cannot be woken yet.
  if (server.mac.length() && !parseMac(server.mac.c_str(), mac)) return "invalidMac";
  IPAddress address;
  if (server.broadcast.length() && !address.fromString(server.broadcast)) return "invalidAddress";
  return nullptr;
}

bool update(const Server &server) {
  Server *current = find(server.id);
  if (!current) return false;
  *current = server;
  return write();
}

bool remove(uint8_t id) {
  Server *server = find(id);
  if (!server) return false;
  all.erase(all.begin() + (server - all.data()));
  return write();
}

bool wake(const Server &server, String &detail) {
  if (!WiFi.isConnected()) return detail = "no Wi-Fi", false;
  uint8_t mac[6];
  if (!parseMac(server.mac.c_str(), mac)) return detail = "no MAC address", false;
  IPAddress target = WiFi.broadcastIP();
  if (server.broadcast.length() && !target.fromString(server.broadcast)) return detail = "bad address", false;
  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; i++) memcpy(packet + 6 + i * 6, mac, 6);
  WiFiUDP udp;
  const bool sent = udp.beginPacket(target, WOL_PORT) && udp.write(packet, sizeof(packet)) == sizeof(packet) && udp.endPacket();
  detail = target.toString();
  return sent;
}

}  // namespace servers
