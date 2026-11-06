#pragma once
// Wiring and fixed settings. Board: ESP32-C3 Super Mini.
// Avoid GPIO 2, 8, 9 for new wiring (strapping pins) and 18/19 (USB).

namespace pins {
constexpr int IR_RECEIVER = 4;  // output of a 38 kHz receiver such as VS1838, idles high
constexpr int LED = 8;          // on-board LED, on when low
constexpr int BOOT = 9;         // BOOT button; holding it 10 s resets the panel password
}  // namespace pins

// Until SNTP answers, time() counts from 1970: anything below this means the clock is not set.
constexpr long CLOCK_SET_AFTER = 1700000000;

// Panel password and access point password until the user changes them.
constexpr const char *DEFAULT_PASSWORD = "whisli_gogo";
