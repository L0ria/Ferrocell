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
//  Deferred persistence (issue #14): a drag fires 60-120 /api/set events/sec.
//  Persisting to NVS (flash) on every one of them is the bottleneck. So
//  /api/set applies settings in RAM immediately (LEDs update at once) and
//  marks a save as PENDING; the actual NVS write is flushed by handle() only
//  once the settings are quiescent (>= kSaveQuiescentMs since the last set).
//  A ledCount change still saves + reboots immediately (no loss window).
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

  // Quiescent window before a pending NVS save is flushed (issue #14). One
  // drag = one save, not one save per pointer event.
  static constexpr std::int64_t kSaveQuiescentMs = 500;

  // Wire the server to the app modules. `config` persists settings, `leds`
  // renders the strip after a change, `wifi` provides status + scan/save.
  UiServer(ConfigStore& config, LedController& leds, WifiManager& wifi);

  // Start the HTTP server on kPort and register all routes. Call once in
  // setup() (after WifiManager::begin()).
  void begin();

  // Non-blocking pump: serve one client, then flush a pending NVS save if the
  // settings have been quiescent long enough. Call every loop() iteration.
  void handle();

  // The live settings (kept in sync with the app's g_settings by the
  // entrypoint after each /api/set).
  ferrocell::Settings& settings();

  // The underlying WebServer (issue #16). Exposed so the entrypoint can hand
  // it to ElegantOTA (ElegantOTA.begin(&g_ui.rawServer())); UiServer remains
  // the only class that OWNS the server — ElegantOTA only registers routes on
  // it. Call AFTER begin() (the server must be listening on kPort).
  WebServer& rawServer();

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
  // Flush a pending NVS save if quiescent (issue #14). No-op when nothing is
  // pending or the quiescent window has not elapsed.
  void flushPendingSave();

  ConfigStore& config_;
  LedController& leds_;
  WifiManager& wifi_;
  ferrocell::Settings settings_;
  WebServer server_;
  // Deferred-persistence state (issue #14).
  bool pendingSave_ = false;
  std::int64_t lastSetMs_ = 0;
};

#endif  // FERROCELL_NET_UI_SERVER_H
