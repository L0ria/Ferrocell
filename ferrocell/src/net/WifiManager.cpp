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
