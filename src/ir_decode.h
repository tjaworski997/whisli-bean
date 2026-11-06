// Turns the timings of one IR frame into a stable button code. Free of Arduino, so it can be
// tested on a computer: see test/decode_test.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum class Protocol : uint8_t { unknown, nec, necRepeat, rc5, rc6, raw };

struct Decoded {
  Protocol protocol = Protocol::unknown;
  uint32_t raw = 0;
  uint16_t address = 0;
  uint8_t command = 0;
  bool trusted = false;  // carries its own checksum, so one frame is enough to learn it
  char code[16] = "";    // empty when the frame identifies no button
};

// d holds the times between edges in microseconds, firstLevel the pin level after the first edge.
Decoded decodeFrame(const uint32_t *d, size_t n, int firstLevel);
const char *protocolName(Protocol protocol);
