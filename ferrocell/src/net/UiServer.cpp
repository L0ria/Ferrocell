// =============================================================================
//  UiServer — implementation (M5, issue #6).
//
//  Thin HTTP glue over the PURE core (core/Api.{h,cpp}). All JSON shapes and
//  validation live in the core (host-tested); this file only maps HTTP
//  requests/responses to that contract and serves the embedded UI assets.
// =============================================================================
#include "UiServer.h"

#include <Arduino.h>
#include <Esp.h>

#include "../core/Api.h"
#include "../core/Json.h"
#include "../hw/ConfigStore.h"
#include "../hw/LedController.h"
#include "WifiManager.h"
#include "ui_assets.h"

using ferrocell::api::WifiInfo;
using ferrocell::json::Value;

UiServer::UiServer(ConfigStore& config, LedController& leds, WifiManager& wifi)
    : config_(config), leds_(leds), wifi_(wifi), settings_(ferrocell::Settings::defaults()) {}

ferrocell::Settings& UiServer::settings() { return settings_; }

void UiServer::begin() {
  server_.begin(kPort);

  // Static UI.
  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/ui.css", HTTP_GET, [this] { handleUiCss(); });
  server_.on("/ui.js", HTTP_GET, [this] { handleUiJs(); });

  // JSON API.
  server_.on("/api/state", HTTP_GET, [this] { handleState(); });
  server_.on("/api/set", HTTP_POST, [this] { handleSet(); });
  server_.on("/api/wifi", HTTP_POST, [this] { handleWifi(); });
  server_.on("/api/reboot", HTTP_POST, [this] { handleReboot(); });
  server_.on("/api/reset", HTTP_POST, [this] { handleReset(); });

  server_.onNotFound([this] { handleNotFound(); });

  Serial.printf("[ui] web UI + JSON API listening on port %d\n", kPort);
}

void UiServer::handle() { server_.handleClient(); }

// --- helpers ---------------------------------------------------------------
void UiServer::sendAsset(const char* asset, size_t len, const char* mime) {
  server_.send_P(200, mime, asset, len);
}

void UiServer::sendJson(const std::string& body, int code) {
  // WebServer::send has no std::string overload; JSON is null-free so a
  // const char* (strlen-terminated) is safe.
  server_.send(code, "application/json", body.c_str());
}

std::string UiServer::readBody() {
  // WebServer (esp32 core) stores a plain (non-form) POST body as the
  // argument "plain".
  if (server_.hasArg("plain")) {
    return std::string(server_.arg("plain").c_str());
  }
  return "";
}

WifiInfo UiServer::wifiInfo() const {
  WifiInfo w;
  w.mode = wifi_.mode();
  w.connected = wifi_.connected();
  w.ip = wifi_.ip();
  w.ssid = wifi_.ssid();
  w.rssi = wifi_.rssi();
  w.apName = wifi_.apName();
  w.configUrl = wifi_.configUrl();
  return w;
}

// --- static UI -------------------------------------------------------------
void UiServer::handleRoot() { sendAsset(kIndexHtml, kIndexHtmlLen, "text/html"); }
void UiServer::handleUiCss() { sendAsset(kUiCss, kUiCssLen, "text/css"); }
void UiServer::handleUiJs() { sendAsset(kUiJs, kUiJsLen, "application/javascript"); }

// --- JSON API --------------------------------------------------------------
void UiServer::handleState() {
  const Value doc = ferrocell::api::stateJson(settings_, wifiInfo());
  sendJson(ferrocell::json::dump(doc), 200);
}

void UiServer::handleSet() {
  const std::string body = readBody();
  Value doc;
  std::string err;
  if (!ferrocell::json::parse(body, doc, &err)) {
    sendJson(ferrocell::json::dump(ferrocell::api::errorJson("invalid JSON: " + err)), 400);
    return;
  }

  const ferrocell::api::SetResult r = ferrocell::api::applySet(doc, settings_);
  if (!r.ok) {
    sendJson(ferrocell::json::dump(ferrocell::api::errorJson(r.error)), 400);
    return;
  }

  // Apply the new settings (the entrypoint's render loop reads settings()
  // every frame, so the strip updates immediately) + persist. The reboot
  // below only happens when the LED count actually changed.
  settings_ = r.settings;
  config_.save(settings_);

  Value resp = ferrocell::api::stateJson(settings_, wifiInfo());
  if (r.rebooting) {
    resp.set("rebooting", Value::makeBool(true));
    // Give the response a chance to flush before the reboot.
    sendJson(ferrocell::json::dump(resp), 200);
    delay(150);
    Serial.println("[ui] ledCount changed — saving + rebooting");
    ESP.restart();
  } else {
    sendJson(ferrocell::json::dump(resp), 200);
  }
}

void UiServer::handleWifi() {
  const std::string body = readBody();
  Value doc;
  std::string err;
  if (!ferrocell::json::parse(body, doc, &err) || !doc.isObject()) {
    sendJson(ferrocell::json::dump(ferrocell::api::errorJson("invalid JSON")), 400);
    return;
  }

  const std::string action = doc.getString("action", "");

  if (action == "scan") {
    const auto nets = wifi_.scan();
    Value out = Value::makeObject();
    out.set("ok", Value::makeBool(true));
    Value arr = Value::makeArray();
    for (const auto& n : nets) {
      Value net = Value::makeObject();
      net.set("ssid", Value::makeString(n.ssid));
      net.set("rssi", Value::makeNumber(static_cast<double>(n.rssi)));
      net.set("locked", Value::makeBool(n.locked));
      arr.array.push_back(std::move(net));
    }
    out.set("networks", std::move(arr));
    sendJson(ferrocell::json::dump(out), 200);
    return;
  }

  if (action == "save") {
    const std::string ssid = doc.getString("ssid", "");
    const std::string password = doc.getString("password", "");
    if (ssid.empty()) {
      sendJson(ferrocell::json::dump(ferrocell::api::errorJson("ssid is required")), 400);
      return;
    }
    Value out = Value::makeObject();
    out.set("ok", Value::makeBool(true));
    out.set("rebooting", Value::makeBool(true));
    sendJson(ferrocell::json::dump(out), 200);
    delay(150);
    wifi_.save(ssid, password);  // persists + reboots (does not return)
    return;
  }

  sendJson(ferrocell::json::dump(ferrocell::api::errorJson("unknown wifi action (use scan|save)")), 400);
}

void UiServer::handleReboot() {
  Value out = Value::makeObject();
  out.set("ok", Value::makeBool(true));
  out.set("rebooting", Value::makeBool(true));
  sendJson(ferrocell::json::dump(out), 200);
  delay(150);
  Serial.println("[ui] reboot requested");
  ESP.restart();
}

void UiServer::handleReset() {
  Value out = Value::makeObject();
  out.set("ok", Value::makeBool(true));
  out.set("rebooting", Value::makeBool(true));
  sendJson(ferrocell::json::dump(out), 200);
  delay(150);
  wifi_.resetConfig();  // wipes credentials + reboots (does not return)
}

void UiServer::handleNotFound() {
  const std::string body = ferrocell::json::dump(ferrocell::api::errorJson("not found"));
  server_.send(404, "application/json", body.c_str());
}
