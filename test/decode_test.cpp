// Computer test of the decoder, with no hardware. Run with scripts/test.sh.
// Frames are synthetic with receiver-like skew and jitter, except those marked as captured.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <utility>
#include <string>
#include <vector>

#include "ir_decode.h"

using Times = std::vector<uint32_t>;

static int failures = 0;

static void check(bool ok, const std::string &what) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok) failures++;
}

// Receivers stretch marks and shrink spaces, plus random jitter on every edge.
static Times distort(const Times &ideal, int jitterPercent = 8, int skew = 60) {
  Times out;
  for (size_t i = 0; i < ideal.size(); i++) {
    const int jitter = (int)ideal[i] * (rand() % (2 * jitterPercent + 1) - jitterPercent) / 100;
    out.push_back(ideal[i] + jitter + (i % 2 ? -skew : skew));
  }
  return out;
}

static Decoded decode(const Times &t, int level = 0) { return decodeFrame(t.data(), t.size(), level); }

static Times pulseDistance(uint32_t leadMark, uint32_t leadSpace, uint32_t mark, uint32_t zero, uint32_t one, int bits, uint64_t value) {
  Times t;
  if (leadMark) t.insert(t.end(), {leadMark, leadSpace});
  for (int b = 0; b < bits; b++) t.insert(t.end(), {mark, value >> b & 1 ? one : zero});
  t.push_back(mark);
  return t;
}

static Times sony(int bits, uint32_t value) {
  Times t = {2400, 600};
  for (int b = 0; b < bits; b++) {
    t.push_back(value >> b & 1 ? 1200 : 600);
    if (b + 1 < bits) t.push_back(600);
  }
  return t;
}

// Times from half-bit levels, without the silence before the first and after the last edge.
static Times fromHalves(const std::vector<int> &halves, uint32_t unit, Times lead = {}) {
  Times t = lead;
  size_t i = 0;
  while (i < halves.size() && !halves[i]) i++;
  size_t last = halves.size();
  while (last > 0 && !halves[last - 1]) last--;
  while (i < last) {
    size_t run = i;
    while (run < last && halves[run] == halves[i]) run++;
    t.push_back((run - i) * unit);
    i = run;
  }
  return t;
}

static Times rc5(int toggle, int address, int command) {
  const int s2 = command & 0x40 ? 0 : 1;
  const int bits = 1 << 13 | s2 << 12 | toggle << 11 | address << 6 | (command & 0x3F);
  std::vector<int> halves;
  for (int b = 13; b >= 0; b--) {
    const int bit = bits >> b & 1;
    halves.push_back(!bit);
    halves.push_back(bit);
  }
  return fromHalves(halves, 889);
}

static Times rc6(int mode, int toggle, uint64_t data, int dataBits) {
  std::vector<int> halves = {1, 0};
  for (int b = 2; b >= 0; b--) {
    const int bit = mode >> b & 1;
    halves.push_back(bit);
    halves.push_back(!bit);
  }
  halves.insert(halves.end(), {toggle, toggle, !toggle, !toggle});
  for (int b = dataBits - 1; b >= 0; b--) {
    const int bit = data >> b & 1;
    halves.push_back(bit);
    halves.push_back(!bit);
  }
  return fromHalves(halves, 444, {2666, 889});
}

// The same button must always give one code, and that code must not be empty.
static std::string stable(const char *name, Times (*make)(uint64_t), uint64_t button, Protocol expected) {
  std::set<std::string> codes;
  int empty = 0;
  for (int i = 0; i < 500; i++) {
    const Decoded d = decode(distort(make(button)));
    if (!d.code[0] || d.protocol != expected) empty++;
    else codes.insert(d.code);
  }
  std::string first = codes.empty() ? "" : *codes.begin();
  check(empty == 0 && codes.size() == 1, std::string(name) + " stable over 500 noisy frames: " + first + " (codes " + std::to_string(codes.size()) + ", misses " + std::to_string(empty) + ")");
  return first;
}

static Times samsung(uint64_t v) { return pulseDistance(4500, 4500, 560, 560, 1690, 32, v); }
static Times kaseikyo(uint64_t v) { return pulseDistance(3456, 1728, 432, 432, 1296, 48, v); }
static Times jvc(uint64_t v) { return pulseDistance(8400, 4200, 526, 526, 1578, 16, v); }
static Times lg(uint64_t v) { return pulseDistance(8500, 4250, 560, 560, 1600, 28, v); }
static Times sharp(uint64_t v) { return pulseDistance(0, 0, 320, 680, 1680, 15, v); }
static Times denon(uint64_t v) { return pulseDistance(0, 0, 260, 775, 1800, 15, v); }
static Times sony12(uint64_t v) { return sony(12, v); }
static Times sony20(uint64_t v) { return sony(20, v); }
static Times necNoChecksum(uint64_t v) { return pulseDistance(9000, 4500, 560, 560, 1690, 32, v); }
static Times nec(uint64_t v) { return pulseDistance(9000, 4500, 560, 560, 1690, 32, v); }

static void pair(const char *name, Times (*make)(uint64_t), uint64_t a, uint64_t b, Protocol p) {
  const std::string first = stable(name, make, a, p), second = stable(name, make, b, p);
  check(first != second, std::string(name) + " buttons differ: " + first + " vs " + second);
}

int main() {
  srand(1);

  puts("--- captured from the real remote");
  const Times captured = {9105, 4429, 625, 522, 622, 1650, 627, 491, 653, 1619, 627, 1644, 623, 1653, 624, 494, 650, 1622, 624, 1648, 629, 1596, 682, 483, 650, 1626, 620, 525, 630, 487, 656, 1590, 657, 513, 620, 497, 647, 496, 648, 501, 622, 521, 623, 494, 650, 1596, 650, 1627, 650, 1644, 623, 1601, 676, 1623, 623, 1623, 654, 1618, 650, 1623, 654, 494, 629, 515, 630, 488, 655};
  check(!strcmp(decode(captured).code, "NEC-4BBA-E0"), std::string("real NEC frame: ") + decode(captured).code);
  check(decode(captured).trusted, "NEC is trusted");

  // These three arrived with single marks of 700-711 us and once got separate fingerprints.
  const std::vector<std::pair<const char *, Times>> stretched = {
      {"NEC-4BBA-02", {9100, 4408, 677, 492, 620, 1652, 626, 493, 651, 1624, 622, 1650, 627, 1645, 622, 496, 648, 1624, 622, 1654, 623, 1624, 653, 490, 654, 1618, 629, 515, 629, 493, 652, 1594, 682, 488, 624, 519, 625, 1599, 710, 461, 651, 492, 621, 496, 648, 496, 648, 495, 628, 517, 627, 1622, 657, 486, 625, 1647, 630, 1647, 620, 1600, 677, 1595, 652, 1620, 657, 1615, 652}},
      {"NEC-4BBA-03", {9105, 4403, 703, 465, 648, 1625, 652, 491, 622, 1625, 683, 1567, 648, 1645, 622, 501, 684, 1588, 648, 1602, 654, 1619, 648, 491, 653, 1597, 711, 460, 653, 490, 622, 1624, 654, 490, 622, 1650, 627, 1623, 655, 490, 653, 490, 623, 520, 624, 493, 651, 493, 630, 513, 620, 523, 621, 502, 652, 1594, 652, 1645, 622, 1625, 653, 1597, 680, 1614, 622, 1655, 622}},
      {"NEC-4BBA-03", {9100, 4463, 653, 464, 649, 1623, 654, 490, 622, 1649, 650, 1575, 702, 1596, 650, 493, 630, 1643, 624, 1600, 677, 1617, 629, 520, 624, 1649, 628, 488, 656, 489, 624, 1648, 629, 488, 656, 1617, 650, 1626, 620, 523, 621, 498, 656, 488, 626, 516, 626, 491, 653, 491, 653, 501, 622, 516, 628, 1619, 648, 1624, 622, 1628, 649, 1646, 621, 1625, 652, 1624, 622}},
  };
  for (const auto &[expected, t] : stretched)
    check(!strcmp(decode(t).code, expected), std::string("real frame with a stretched mark: ") + decode(t).code + ", expected " + expected);

  // Captured with a weak signal: short marks, long spaces, a broken leader, a dip of 65 us.
  const std::vector<std::pair<const char *, Times>> weak = {
      {"short marks", {9000, 4637, 413, 867, 277, 1863, 413, 626, 517, 1782, 224, 1994, 521, 1755, 521, 676, 228, 1991, 494, 1805, 471, 1728, 225, 1071, 384, 1735, 542, 629, 441, 807, 388, 1727, 383, 760, 519, 677, 435, 656, 518, 577, 514, 708, 467, 599, 544, 1719, 526, 1786, 490, 1726, 592, 1657, 391, 1908, 524, 1723, 543, 1913, 332, 1730, 567, 634, 488, 576, 567, 631, 523}},
      {"leader in 3 pieces", {6686, 454, 1780, 4877, 309, 728, 416, 1779, 486, 866, 257, 1811, 434, 1759, 227, 1992, 574, 621, 522, 1725, 521, 1754, 542, 1652, 593, 734, 388, 1858, 439, 599, 514, 630, 513, 1816, 470, 620, 513, 578, 524, 830, 334, 598, 546, 598, 544, 652, 471, 1728, 485, 1812, 517, 1703, 230, 2121, 467, 1707, 569, 1701, 575, 1908, 358, 1677, 568, 629, 514, 655, 488, 576, 546}},
      {"leader in 3 pieces, again", {7370, 450, 1119, 4597, 547, 598, 359, 1942, 490, 628, 515, 1837, 439, 1730, 328, 1995, 201, 943, 468, 2071, 226, 1912, 312, 1832, 257, 860, 470, 1724, 542, 602, 520, 807, 337, 1752, 524, 650, 462, 630, 513, 788, 231, 755, 523, 648, 413, 679, 516, 1702, 595, 1677, 568, 1761, 494, 1779, 487, 1733, 543, 1731, 547, 1646, 598, 1750, 515, 713, 441, 598, 493, 626, 551}},
      {"broken leader and a 65 us dip", {4823, 374, 1703, 424, 1571, 4796, 339, 648, 568, 1730, 255, 913, 490, 1814, 233, 1960, 441, 1848, 210, 968, 466, 1703, 574, 1701, 575, 1701, 544, 648, 391, 1855, 515, 576, 567, 760, 362, 1733, 461, 653, 573, 544, 516, 707, 488, 655, 489, 602, 520, 649, 494, 1678, 547, 1778, 519, 1701, 596, 1729, 225, 65, 226, 1681, 595, 1730, 546, 1701, 627, 1699, 494, 701, 443, 570, 542, 600, 543}},
  };
  for (const auto &[what, t] : weak) {
    const Decoded d = decode(t);
    check(d.protocol == Protocol::nec && !strncmp(d.code, "NEC-4BBA-", 9), std::string("weak signal, ") + what + ": " + d.code);
  }
  const Times missingBits = {9023, 4615, 436, 923, 231, 1751, 389, 729, 228, 4446, 386, 1759, 227, 892, 490, 1807, 490, 1914, 206, 2018, 414, 883, 229, 1811, 226, 1076, 307, 625, 258, 1963, 573, 615, 497, 654, 489, 576, 225, 950, 515, 680, 463, 655, 489, 1810, 466, 1679, 566, 1833, 308, 1816, 595, 1751, 494, 1717, 320, 1987, 518, 1754, 522, 653, 469, 596, 548, 596, 550};
  check(decode(missingBits).protocol != Protocol::nec, "frame with lost bits is not guessed");

  const std::vector<Times> stray = {
      {231}, {328, 6796, 202}, {233, 370, 223, 2007, 207, 345, 227, 3162, 207, 1471, 203},
      {203, 167, 415, 65535, 203, 65535, 256, 20244, 232, 15255, 1010, 7015, 1023, 7006, 991, 10989, 1022, 7570, 281, 65535, 540, 3201, 782, 65535, 464, 7534, 224, 203, 536, 7515, 357},
      {250, 95, 227, 288, 232, 65535, 199, 231, 236, 12466, 939, 341, 595, 6992, 1077, 6964, 232, 7860, 626, 7547, 367, 7362, 1009, 7790, 228},
      {427, 169, 309, 92, 387, 274, 277, 90, 232, 6884, 1113, 14934, 1039, 7319, 688, 7396, 455, 7173, 876, 7092, 988, 18992, 984},
      {689, 8685, 227, 5645, 417, 4829, 204, 889, 1077, 11138, 862, 6941, 1067, 10932, 673, 2335, 1034, 3925, 1078, 7373, 644},
  };
  for (const Times &t : stray) {
    const Decoded d = decode(t);
    check(!d.code[0], "captured stray pulses, " + std::to_string(t.size() + 1) + " edges: no code" + (d.code[0] ? std::string(" but got ") + d.code : ""));
  }

  puts("--- NEC family");
  pair("NEC", nec, 0x1FE04BBA, 0x9A654BBA, Protocol::nec);
  stable("NEC without inverted command (Apple-like)", necNoChecksum, 0x12345678, Protocol::raw);
  for (int skew : {-120, -60, 60, 120, 180}) {
    std::set<std::string> codes;
    for (int i = 0; i < 500; i++) codes.insert(decode(distort(nec(0x1FE04BBA), 8, skew)).code);
    check(codes.size() == 1 && *codes.begin() == "NEC-4BBA-E0", "NEC keeps one code at receiver skew " + std::to_string(skew) + " us");
  }
  const Times repeat = {9000, 2250, 560};
  check(decode(distort(repeat)).protocol == Protocol::necRepeat && !decode(repeat).code[0], "NEC repeat has no code");

  puts("--- fingerprint");
  pair("Samsung", samsung, 0xE0E040BF, 0xE0E0D02F, Protocol::raw);
  pair("Panasonic/Kaseikyo 48 bit", kaseikyo, 0x0100BCBD2002ULL, 0x0100BCBD2003ULL, Protocol::raw);
  pair("JVC", jvc, 0xC5E8, 0xC5E9, Protocol::raw);
  pair("LG", lg, 0x20DF10E, 0x20DF40B, Protocol::raw);
  pair("Sony 12 bit", sony12, 0xA90, 0xA91, Protocol::raw);
  pair("Sony 20 bit", sony20, 0xB4B8F, 0xB4B8E, Protocol::raw);
  pair("Sharp (no leader), first bit differs", sharp, 0x41A2, 0x41A3, Protocol::raw);
  check(strcmp(decode(sony12(0)).code, "") != 0, "Sony with all data bits zero still gets a code");
  pair("Denon (no leader)", denon, 0x2A5C, 0x2A5D, Protocol::raw);

  puts("--- toggle protocols: one code per button whatever the toggle");
  for (int command : {0x00, 0x01, 0x0C, 0x20, 0x3F, 0x40, 0x7F}) {
    std::set<std::string> codes;
    int bad = 0;
    for (int i = 0; i < 400; i++) {
      const Decoded d = decode(distort(rc5(i % 2, 5, command), 8, 50));
      if (d.protocol != Protocol::rc5) bad++;
      else codes.insert(d.code);
    }
    char expected[16];
    snprintf(expected, sizeof(expected), "RC5-05-%02X", command);
    check(!bad && codes.size() == 1 && *codes.begin() == expected, std::string("RC5 ") + expected + " (misses " + std::to_string(bad) + ")");
  }
  for (uint32_t data : {0x0000u, 0x0001u, 0x800Cu, 0x04FFu, 0xFFFFu}) {
    std::set<std::string> codes;
    int bad = 0;
    for (int i = 0; i < 400; i++) {
      const Decoded d = decode(distort(rc6(0, i % 2, data, 16), 6, 40));
      if (d.protocol != Protocol::rc6) bad++;
      else codes.insert(d.code);
    }
    check(!bad && codes.size() == 1, "RC6 mode 0, data " + std::to_string(data) + ": " + (codes.empty() ? "" : *codes.begin()) + " (misses " + std::to_string(bad) + ")");
  }
  {
    std::set<std::string> codes;
    int bad = 0;
    for (int i = 0; i < 400; i++) {
      const uint64_t data = 0x800F040CULL | (i % 2 ? 1ULL << 15 : 0);
      const Decoded d = decode(distort(rc6(6, 0, data, 32), 6, 40));
      if (d.protocol != Protocol::rc6) bad++;
      else codes.insert(d.code);
    }
    check(!bad && codes.size() == 1, "Media Center RC6, toggle in bit 15: " + (codes.empty() ? "" : *codes.begin()) + " (misses " + std::to_string(bad) + ")");
  }

  puts("--- rejected");
  Times shortBurst;
  for (int i = 0; i < 8; i++) shortBurst.insert(shortBurst.end(), {600, i % 2 ? 1200u : 600u});
  shortBurst.push_back(600);
  check(!decode(shortBurst).code[0], "tidy burst of 18 edges, too short for any remote: no code");
  Times even;
  for (int i = 0; i < 20; i++) even.insert(even.end(), {1000, 7000});
  check(!decode(even).code[0], "even beat without data: no code");
  check(!decode(samsung(0xE0E040BF), 1).code[0], "frame starting high: no code");

  puts(failures ? "\nFAILED" : "\nall passed");
  return failures ? 1 : 0;
}
