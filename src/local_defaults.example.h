// Copy to src/local_defaults.h (ignored by git) to build firmware that joins your Wi-Fi straight
// away. It is used only until Wi-Fi is saved in the panel. Without it, a new board opens the
// "Whisli-Bean-XXXX" access point.
#pragma once
#define DEFAULT_WIFI_SSID "my-network"
#define DEFAULT_WIFI_PASSWORD "my-password"
