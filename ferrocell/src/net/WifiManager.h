// =============================================================================
//  WifiManager — the ONLY class that talks to WiFi (M4, issue #5).
//
//  Wraps the vendored `tabahi/ESP-Wifi-Config` (v2.2.6, lib/ESP-Wifi-Config)
//  so the rest of the app never calls `WiFi.*` directly. It gives the device
//  two ways to get online:
//
//    * AP mode  — no saved WiFi (or the saved one is out of range) → an
//                 access point `Ferrocell_<9-digit-mac>` at 192.168.1.1 with
//                 a web config page on port 8080:  http://192.168.1.1:8080/
//                 (login admin / pass_ESP). Saving credentials there persists
//                 them and reboots the device into STA mode.
//    * STA mode — saved WiFi in range → connect + auto-reconnect; the device
//                 gets an IP and the (future, M5) web UI is reachable at
//                 http://<ip>/ on port 80.
//
//  Design notes:
//    * Port 8080 for the config page (not 80) keeps the root path free for
//      the M5 `UiServer` web UI at http://<ip>/ and http://192.168.1.1/.
//    * The config page stays up PERPETUALLY (Start_HTTP_Server(0)) so the
//      device is re-configurable at any time — even in STA mode.
//    * `scan()` is intentionally NOT exposed: the library's scan results are
//      only used internally and are not stored in a readable form. The web
//      config page (`/readSSID`) is the supported way to pick a network.
//
//  Depends on the Arduino core (WiFi/WebServer), so it is compiled into the
//  firmware only — NOT part of the host test harness. It is a thin wrapper
//  (little pure logic), verified by the `firmware-build` CI job, consistent
//  with the other hw-layer classes (LedController, ConfigStore).
// =============================================================================
#ifndef FERROCELL_NET_WIFI_MANAGER_H
#define FERROCELL_NET_WIFI_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ESPWifiConfig.h"

class WifiManager {
 public:
  // Default-constructs the underlying ESPWifiConfig (no fallback WiFi, no
  // reset button, debug on). Declared here so the header's implicit copy
  // path is well-formed (ESPWifiConfig has no default constructor).
  WifiManager();

  // AP / config-page constants (single source of truth).
  static constexpr char kSysName[] = "Ferrocell";  // AP SSID prefix (lib appends "_<mac>")
  static constexpr int  kConfigPort = 8080;        // config page port (root 80 stays free for M5)
  static constexpr char kApIp[] = "192.168.1.1";   // library default AP IP (documented)
  static constexpr int  kReconnectDelayMs = 10000; // STA auto-reconnect interval

  // Boot the WiFi layer: load saved credentials, enter STA (if a saved
  // network is in range) or AP mode, and start the (perpetual) config-page
  // HTTP server. Call once in setup(). Logs the resulting mode + endpoint.
  void begin();

  // Non-blocking pump: STA auto-reconnect + serve the config page. Call
  // every loop() iteration. Cheap when nothing is pending.
  void handle();

  // --- state (for the UI / logging) ----------------------------------------
  // Current mode: "ap" (access point) or "sta" (station).
  std::string mode() const;
  // True when connected to the saved network (STA mode only).
  bool connected() const;
  // The device IP as a dotted string ("192.168.x.y" in STA, "192.168.1.1"
  // in AP). Empty until the mode is established.
  std::string ip() const;
  // The SSID we are connected to (STA) — empty in AP mode / when offline.
  std::string ssid() const;
  // Signal strength of the current connection in dBm (STA) — 0 in AP mode.
  int rssi() const;
  // The AP SSID ("Ferrocell_<9-digit-mac>") — what to join in AP mode.
  std::string apName();  // non-const: the library's get_AP_name() is non-const
  // The config-page URL ("http://192.168.1.1:8080").
  std::string configUrl() const;

  // --- actions --------------------------------------------------------------
  // Wipe the saved WiFi credentials (and page login) from flash. The device
  // reboots and comes back in AP mode, ready to be configured again.
  // (The library performs the reboot; this call does not return.)
  void resetConfig();

  // --- scan / save (M5, issue #6) ------------------------------------------
  // A network found by scan().
  struct Network {
    std::string ssid;
    int rssi = 0;          // dBm
    bool locked = false;   // true if it requires a password
  };

  // Run a live scan and return the found networks (capped at kMaxScanResults,
  // strongest first). Blocking (~2 s) — only called from the /api/wifi scan
  // action. Returns an empty vector on failure.
  static constexpr int kMaxScanResults = 16;
  std::vector<Network> scan();

  // Persist new WiFi credentials (SSID + password) and reboot into STA mode.
  // Uses the library's own settings store (the same one its config page
  // writes), so the device reconnects on boot. This call does not return.
  void save(const std::string& ssid, const std::string& password);

 private:
  ESPWifiConfig wifi_;
};

#endif  // FERROCELL_NET_WIFI_MANAGER_H
