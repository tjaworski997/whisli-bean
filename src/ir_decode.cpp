#include "ir_decode.h"

#include <stdio.h>

#include <algorithm>

namespace {

bool isNear(uint32_t value, uint32_t target) {
  const uint32_t tolerance = target / 4;
  return value + tolerance >= target && value <= target + tolerance;
}

// The receiver stretches or shrinks pulses with signal strength: 560 us marks arrive anywhere from
// about 220 to 720 us, and a short 560 us space up to about 1100 us. A weak signal also breaks up the
// long leader, and adds dips of a few dozen us. The checksum protects the result, which allows wide
// limits: narrow ones lost readable frames and gave one button different codes depending on aim.
constexpr uint32_t NEC_MIN_MARK_US = 120, NEC_MAX_MARK_US = 1000;
constexpr uint32_t NEC_MIN_SPACE_US = 250, NEC_SPACE_SPLIT_US = 1300, NEC_MAX_SPACE_US = 2500;
constexpr uint32_t NEC_GLITCH_US = 120;
constexpr size_t NEC_MAX_LEADER_PIECES = 8;
constexpr size_t NEC_BIT_DURATIONS = 64;  // 32 bits, each a mark and a space

// Very short dips are noise, not signal: each is joined with the times around it.
size_t removeGlitches(const uint32_t *d, size_t n, uint32_t *out, size_t capacity) {
  size_t length = 0;
  for (size_t i = 0; i < n && length < capacity; i++) {
    if (d[i] < NEC_GLITCH_US && length > 0 && i + 1 < n) {
      out[length - 1] += d[i] + d[i + 1];
      i++;
    } else {
      out[length++] = d[i];
    }
  }
  return length;
}

// Where the bits start: after a 4.5 ms space, with everything before it adding up to the 9 ms
// mark, which a weak signal can break into pieces. 0 when this is not an NEC frame.
size_t findNecBits(const uint32_t *d, size_t n) {
  uint32_t leader = 0;
  for (size_t i = 0; i < n && i < NEC_MAX_LEADER_PIECES; i++) {
    if (i > 0 && i % 2 == 1 && isNear(d[i], 4500) && isNear(leader, 9000) && n - (i + 1) >= NEC_BIT_DURATIONS) return i + 1;
    leader += d[i];
  }
  return 0;
}

bool decodeNec(const uint32_t *raw, size_t rawLength, Decoded &out) {
  static uint32_t d[320];
  const size_t n = removeGlitches(raw, rawLength, d, sizeof(d) / sizeof(d[0]));
  if (n >= 3 && n < 10 && isNear(d[0], 9000) && isNear(d[1], 2250)) {
    out.protocol = Protocol::necRepeat;  // a held button, no code of its own
    return true;
  }
  const size_t start = findNecBits(d, n);
  if (!start) return false;
  uint32_t value = 0;
  for (int bit = 0; bit < 32; bit++) {
    const uint32_t mark = d[start + bit * 2], space = d[start + bit * 2 + 1];
    if (mark < NEC_MIN_MARK_US || mark > NEC_MAX_MARK_US || space < NEC_MIN_SPACE_US || space > NEC_MAX_SPACE_US) return false;
    if (space > NEC_SPACE_SPLIT_US) value |= 1UL << bit;
  }
  const uint8_t command = value >> 16;
  // Without the inverted command this may be another NEC-like protocol: the fingerprint handles it.
  if ((uint8_t)~command != (uint8_t)(value >> 24)) return false;
  // Extended NEC uses all 16 address bits, so keep them or two remotes could share a code.
  const uint8_t low = value, high = value >> 8;
  out.protocol = Protocol::nec;
  out.raw = value;
  out.address = (uint8_t)~low == high ? low : (uint16_t)value;
  out.command = command;
  out.trusted = true;
  snprintf(out.code, sizeof(out.code), "NEC-%04X-%02X", (unsigned)out.address, (unsigned)command);
  return true;
}

// Manchester protocols: splits edge times into half bits holding the signal level.
bool expandHalves(const uint32_t *d, size_t n, uint32_t unit, uint32_t maxUnits, uint8_t *halves, size_t capacity, size_t &length) {
  uint8_t level = 1;
  for (size_t i = 0; i < n; i++) {
    const uint32_t units = (d[i] + unit / 2) / unit;
    if (units < 1 || units > maxUnits) return false;
    const uint32_t expected = units * unit;
    const uint32_t off = d[i] > expected ? d[i] - expected : expected - d[i];
    if (off > unit * 35 / 100) return false;
    for (uint32_t u = 0; u < units; u++) {
      if (length >= capacity) return false;
      halves[length++] = level;
    }
    level = !level;
  }
  return true;
}

bool decodeRc5(const uint32_t *d, size_t n, Decoded &out) {
  uint8_t halves[32];
  size_t length = 0;
  halves[length++] = 0;  // the start bit opens with a half bit of silence, which has no edge
  if (!expandHalves(d, n, 889, 2, halves, sizeof(halves) - 1, length)) return false;
  if (length % 2) halves[length++] = 0;  // a final silent half bit has no closing edge either
  if (length != 28) return false;
  uint16_t bits = 0;
  for (size_t i = 0; i < 14; i++) {
    if (halves[i * 2] == halves[i * 2 + 1]) return false;
    bits = bits << 1 | halves[i * 2 + 1];
  }
  if (!(bits >> 13)) return false;
  // Bit 11 toggles on every press, so it stays out of the code. Bit 12 inverted is command bit 6.
  out.protocol = Protocol::rc5;
  out.address = bits >> 6 & 0x1F;
  out.command = (bits & 0x3F) | (bits >> 12 & 1 ? 0 : 0x40);
  snprintf(out.code, sizeof(out.code), "RC5-%02X-%02X", (unsigned)out.address, (unsigned)out.command);
  return true;
}

bool decodeRc6(const uint32_t *d, size_t n, Decoded &out) {
  if (n < 20 || !isNear(d[0], 2666) || !isNear(d[1], 889)) return false;
  uint8_t halves[96];
  size_t length = 0;
  if (!expandHalves(d + 2, n - 2, 444, 6, halves, sizeof(halves) - 1, length)) return false;
  if (length % 2) halves[length++] = 0;
  size_t i = 0;
  if (length < 12 || halves[0] != 1 || halves[1] != 0) return false;
  i = 2;
  uint8_t mode = 0;
  for (int bit = 0; bit < 3; bit++, i += 2) {
    if (halves[i] == halves[i + 1]) return false;
    mode = mode << 1 | halves[i];
  }
  // The toggle bit is twice as long and changes on every press, so it stays out of the code.
  if (halves[i] != halves[i + 1] || halves[i + 2] != halves[i + 3] || halves[i] == halves[i + 2]) return false;
  i += 4;
  const size_t dataBits = (length - i) / 2;
  if (dataBits != 16 && dataBits != 20 && dataBits != 24 && dataBits != 32) return false;
  uint32_t data = 0;
  for (; i < length; i += 2) {
    if (halves[i] == halves[i + 1]) return false;
    data = data << 1 | halves[i];
  }
  // Long RC6 variants, such as Windows Media Center remotes, keep a toggle in bit 15.
  if (dataBits == 32) data &= ~(1UL << 15);
  out.protocol = Protocol::rc6;
  out.raw = data;
  out.address = data >> 8;
  out.command = data;
  snprintf(out.code, sizeof(out.code), "RC6-%X-%0*lX", (unsigned)mode, (int)(dataBits / 4), (unsigned long)data);
  return true;
}

// --- Fingerprint for every other remote ---

// The shortest real frame without its own decoder is 12-bit Sony, 25 times. Stray pulses here reached
// 16 edges and were taken for a button below this.
constexpr size_t MIN_FINGERPRINT_DURATIONS = 24;
constexpr uint32_t MIN_PULSE_US = 150;
constexpr size_t MAX_CLASSES = 4;
constexpr size_t MAX_VALUES = 150;

struct Classes {
  uint32_t upper[MAX_CLASSES];
  size_t count = 0;

  uint8_t rank(uint32_t value) const {
    uint8_t r = 0;
    while (r + 1 < count && value > upper[r]) r++;
    return r;
  }
};

// Groups times into classes such as "short" and "long". Real remotes give a few tight groups,
// stray pulses do not, so failing here is what rejects noise.
bool findClasses(uint32_t *values, size_t count, Classes &classes) {
  std::sort(values, values + count);
  uint32_t start = values[0];
  for (size_t i = 1; i <= count; i++) {
    if (i < count && values[i] * 10 <= values[i - 1] * 13) continue;
    if (values[i - 1] * 10 > start * 16 || classes.count >= MAX_CLASSES) return false;
    classes.upper[classes.count++] = values[i - 1];
    if (i < count) start = values[i];
  }
  return true;
}

// A leader is a first mark clearly longer than all others. It carries no data, and its space can
// sit close to the long data space, so grouping them together would split unpredictably.
bool hasLeader(const uint32_t *d, size_t n) {
  uint32_t longest = 0;
  for (size_t i = 2; i < n; i += 2) longest = std::max(longest, d[i]);
  return d[0] * 10 >= longest * 15;
}

// Ranks within this frame instead of absolute times: receiver skew and a remote's clock drift
// shift all times together, so the same button keeps the same sequence of ranks.
bool fingerprint(const uint32_t *d, size_t n, Decoded &out) {
  if (n < MIN_FINGERPRINT_DURATIONS) return false;
  const bool leader = hasLeader(d, n);
  const size_t begin = leader ? 2 : 0;
  static uint32_t values[2][MAX_VALUES];
  size_t counts[2] = {0, 0};
  for (size_t i = 0; i < n; i++) {
    if (d[i] < MIN_PULSE_US) return false;
    if (i >= begin && counts[i % 2] < MAX_VALUES) values[i % 2][counts[i % 2]++] = d[i];
  }
  Classes classes[2];
  if (!findClasses(values[0], counts[0], classes[0]) || !findClasses(values[1], counts[1], classes[1])) return false;
  if (classes[0].count + classes[1].count + (leader ? 1 : 0) < 3) return false;  // an even beat carries no data
  uint32_t hash = 2166136261UL;  // FNV-1a
  hash = (hash ^ (leader ? 1 : 0)) * 16777619UL;
  for (size_t i = begin; i < n; i++) {
    hash ^= classes[i % 2].rank(d[i]);
    hash *= 16777619UL;
  }
  out.protocol = Protocol::raw;
  out.raw = hash;
  snprintf(out.code, sizeof(out.code), "IR-%08lX", (unsigned long)hash);
  return true;
}

}  // namespace

Decoded decodeFrame(const uint32_t *d, size_t n, int firstLevel) {
  Decoded out;
  if (firstLevel != 0 || n == 0) return out;  // receivers idle high, so a real frame starts low
  if (decodeNec(d, n, out) || decodeRc5(d, n, out) || decodeRc6(d, n, out)) return out;
  fingerprint(d, n, out);
  return out;
}

const char *protocolName(Protocol protocol) {
  switch (protocol) {
    case Protocol::nec: return "nec";
    case Protocol::necRepeat: return "nec-repeat";
    case Protocol::rc5: return "rc5";
    case Protocol::rc6: return "rc6";
    case Protocol::raw: return "raw";
    default: return "unknown";
  }
}
