// =============================================================================
//  UiServer — the ONLY class that talks to WebServer (M5, issue #6).
//
//  Serves the self-contained web UI (index.html + ui.css + ui.js, embedded in
//  PROGMEM from ferrocell/data/) and the JSON API on port 80, in BOTH AP and
//  STA mode:
//      http://<ip>/            (STA)
//      http://192.168.1.1/     (AP)
//
//  The JSON contract is defined in the PURE core (core/Api.{h,cpp}, host-
//  tested in tests/api_test.cpp); this class only does the HTTP plumbing:
//    GET  /            -> index.html
//    GET  /ui.css      -> ui.css
//    GET  /ui.js       -> ui.js
//    GET  /api/state   -> core::Api::stateJson(...)
//    POST /api/set     -> core::Api::applySet(...)   (+ save + reboot on ledCount)
//    POST /api/wifi    -> scan | save (via WifiManager)
//    POST /api/reboot  -> ESP.restart()
//    POST /api/reset   -> WifiManager::resetConfig()
//
//  The M4 config page (ESP-Wifi-Config) stays up PERPETUALLY on port 8080 as
//  the fallback configurator; UiServer owns port 80.
//
//  Depends on the Arduino core (WebServer), so it is compiled into the
//  firmware only — NOT part of the host test harness (verified by the
//  `firmware-build` CI job). The JSON logic it delegates to IS host-tested.
// =============================================================================
#ifndef FERROCELL_NET_UI_SERVER_H
#define FERROCELL_NET_UI_SERVER_H

#include <cstddef>
#include <cstdint>
#include <string>

#include <WebServer.h>

#include "../core/Api.h"
#include "../core/Settings.h"

class ConfigStore;
class LedController;
class WifiManager;

class UiServer {
 public:
  // Port 80 in both AP and STA mode (issue #6 acceptance). The M4 config page
  // is on 8080, so there is no collision.
  static constexpr int kPort = 80;

  // Wire the server to the app modules. `config` persists settings, `leds`
  // renders the strip after a change, `wifi` provides status + scan/save.
  UiServer(ConfigStore& config, LedController& leds, WifiManager& wifi);

  // Start the HTTP server on kPort and register all routes. Call once in
  // setup() (after WifiManager::begin()).
  void begin();

  // Non-blocking pump: serve one client. Call every loop() iteration.
  void handle();

  // The live settings (kept in sync with the app's g_settings by the
  // entrypoint after each /api/set).
  ferrocell::Settings& settings();

 private:
  // --- route handlers ------------------------------------------------------
  void handleRoot();
  void handleUiCss();
  void handleUiJs();
  void handleState();
  void handleSet();
  void handleWifi();
  void handleReboot();
  void handleReset();
  void handleNotFound();

  // --- helpers -------------------------------------------------------------
  void sendAsset(const char* asset, size_t len, const char* mime);
  void sendJson(const std::string& body, int code = 200);
  std::string readBody();
  ferrocell::api::WifiInfo wifiInfo() const;

  ConfigStore& config_;
  LedController& leds_;
  WifiManager& wifi_;
  ferrocell::Settings settings_;
  WebServer server_;
};

#endif  // FERROCELL_NET_UI_SERVER_H
