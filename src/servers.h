#pragma once
// Servers to wake over the network (Wake-on-LAN), defined once and chosen on buttons, like webhooks.
// The list expects StateLock held, except begin() and wake().
#include <ArduinoJson.h>

namespace servers {

constexpr size_t MAX_SERVERS = 16, MAX_NAME = 64;

struct Server {
  uint8_t id = 0;
  String name, mac;
  String broadcast;  // where the packet goes; empty: this network's broadcast. Needed for another VLAN.
};

void begin();
bool exists(uint8_t id);
bool load(uint8_t id, Server &out);
void list(JsonArray out);
uint8_t create(const String &name);  // the new id, 0 when it cannot be added
const char *validate(const Server &server);  // nullptr when fine, otherwise an error code for the panel
bool update(const Server &server);
bool remove(uint8_t id);
bool wake(const Server &server, String &detail);  // sends the packet; there is no answer to wait for

}  // namespace servers
