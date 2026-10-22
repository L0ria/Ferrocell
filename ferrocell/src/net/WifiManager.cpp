// =============================================================================
//  WifiManager — implementation (M4, issue #5).
//
//  Thin, faithful wrapper over `ESPWifiConfig` (vendored v2.2.6). No WiFi
//  policy is invented here: it delegates mode selection, (re)connect, the
//  config-page HTTP server, and credential persistence to the library, and
//  only adds a stable, app-facing API + logging.
// =============================================================================
#include "WifiManager.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

namespace {
// The library's fallback-SSID fields are `char*` (non-const); it only reads
// them (and only when non-empty), so static empty buffers are a safe "no
// fallback". Non-const to match the library's `char*` signature.
char kNoFallbackSsid[] = "";
char kNoFallbackPass[] = "";
}  // namespace

WifiManager::WifiManager()
    : wifi_(kSysName, kConfigPort, /*reset_btn=*/-1, /*fix_ssid=*/false,
            kNoFallbackSsid, kNoFallbackPass, /*debug=*/true) {}

void WifiManager::begin() {
  // initialize(): reads the saved credentials, scans, then enters STA mode
  // (saved network in range) or AP mode (nothing saved / out of range).
  const int mode = wifi_.initialize();

  // Start the config-page HTTP server PERPETUALLY (0 = no auto-stop) so the
  // device is re-configurable at any time, in either mode.
  wifi_.Start_HTTP_Server(/*client_mode_active_time=*/0);

  if (mode == AP_MODE) {
    Serial.println("[wifi] mode=AP — no saved WiFi in range; config page is up");
    Serial.print  ("[wifi] AP SSID     : "); Serial.println(apName().c_str());
    Serial.print  ("[wifi] config page : "); Serial.println(configUrl().c_str());
    Serial.println("[wifi] login       : admin / pass_ESP");
    Serial.println("[wifi] save a network there -> device reboots into STA");
  } else {
    Serial.println("[wifi] mode=STA — saved WiFi selected; auto-reconnect on");
    if (connected()) {
      Serial.print  ("[wifi] connected   : "); Serial.println(ssid().c_str());
      Serial.print  ("[wifi] IP          : "); Serial.println(ip().c_str());
      Serial.print  ("[wifi] RSSI        : "); Serial.print(rssi()); Serial.println(" dBm");
    } else {
      Serial.println("[wifi] not connected yet — will keep retrying (handle())");
    }
  }
}

void WifiManager::handle() {
  // Non-blocking: STA reconnect attempts + serve the config page + DNS.
  wifi_.handle(kReconnectDelayMs);
}

std::string WifiManager::mode() const {
  return wifi_.ESP_mode == AP_MODE ? "ap" : "sta";
}

bool WifiManager::connected() const {
  return wifi_.ESP_mode != AP_MODE && wifi_.wifi_connected &&
         WiFi.status() == WL_CONNECTED;
}

std::string WifiManager::ip() const {
  if (wifi_.ESP_mode == AP_MODE) {
    return std::string(kApIp);  // the AP's fixed IP (config page host)
  }
  if (WiFi.status() == WL_CONNECTED) {
    return std::string(WiFi.localIP().toString().c_str());
  }
  return "";
}

std::string WifiManager::ssid() const {
  if (wifi_.ESP_mode == AP_MODE || WiFi.status() != WL_CONNECTED) {
    return "";
  }
  return std::string(WiFi.SSID().c_str());
}

int WifiManager::rssi() const {
  if (wifi_.ESP_mode == AP_MODE || WiFi.status() != WL_CONNECTED) {
    return 0;
  }
  return WiFi.RSSI();
}

std::string WifiManager::apName() {
  return std::string(wifi_.get_AP_name().c_str());
}

std::string WifiManager::configUrl() const {
  return std::string("http://") + kApIp + ":" + std::to_string(kConfigPort);
}

void WifiManager::resetConfig() {
  Serial.println("[wifi] resetting saved WiFi credentials (reboot follows)");
  wifi_.ESP_reset_settings();
  ESP.restart();  // the library's own reset path does the same
}

std::vector<WifiManager::Network> WifiManager::scan() {
  std::vector<Network> out;
  // A live scan. On the ESP32 the base radio can scan while the softAP is up,
  // so this does NOT tear down the AP (which would drop the in-flight request
  // and the config page). If the scan finds nothing, the caller reports an
  // empty list and the user can fall back to the :8080 config page.
  const int n = WiFi.scanNetworks();
  if (n > 0) {
    for (int i = 0; i < n && static_cast<int>(out.size()) < kMaxScanResults; ++i) {
      Network net;
      net.ssid = std::string(WiFi.SSID(i).c_str());
      net.rssi = WiFi.RSSI(i);
      net.locked = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      out.push_back(std::move(net));
    }
  }
  Serial.printf("[wifi] scan: %d network(s) found\n", static_cast<int>(out.size()));
  return out;
}

void WifiManager::save(const std::string& ssid, const std::string& password) {
  Serial.printf("[wifi] saving credentials ssid=%s (reboot follows)\n", ssid.c_str());
  // Persist into the library's own settings store (the same fields its config
  // page writes) so the device reconnects on boot. Truncate to the library's
  // fixed field size to stay within its layout.
  char* ssidBuf = wifi_.setting[WIFI_SSID].value;
  char* passBuf = wifi_.setting[WIFI_PASS].value;
  const size_t maxLen = sizeof(wifi_.setting[WIFI_SSID].value);
  std::snprintf(ssidBuf, maxLen, "%s", ssid.c_str());
  std::snprintf(passBuf, maxLen, "%s", password.c_str());
  wifi_.ESP_save_settings();
  ESP.restart();
}
