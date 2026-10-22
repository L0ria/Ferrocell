// =============================================================================
//  Api — the pure, host-testable JSON contract for the Ferrocell web UI (M5, #6).
//
//  This is the SINGLE place the JSON API shapes are defined. It has NO Arduino
//  / WebServer / hardware dependencies: it takes a parsed `json::Value` and
//  returns `json::Value` / `Settings`, so the exact same code runs on the host
//  (unit-tested in tests/api_test.cpp) and in the firmware (where `UiServer`
//  calls it). Keeping the contract pure + host-tested is what makes it "stable
//  for the Node harness (M6)" (issue #6 acceptance).
//
//  Endpoints (served by UiServer on port 80):
//    GET  /api/state            -> stateJson(...)
//    POST /api/set              -> applySet(doc, current)   (partial update)
//    POST /api/wifi  {scan}     -> (handled in UiServer; needs WiFi)
//    POST /api/wifi  {save}     -> (handled in UiServer; needs WiFi)
//    POST /api/reboot           -> (handled in UiServer)
//    POST /api/reset            -> (handled in UiServer)
// =============================================================================
#ifndef FERROCELL_CORE_API_H
#define FERROCELL_CORE_API_H

#include <cstdint>
#include <string>

#include "Json.h"
#include "Settings.h"

namespace ferrocell {
namespace api {

// Firmware version reported by /api/state (bumped per milestone).
constexpr const char* kFirmwareVersion = "0.5.0";

// The known /api/set fields (single source of truth for validation).
namespace fields {
constexpr const char* kHue = "hue";
constexpr const char* kSaturation = "saturation";
constexpr const char* kBrightness = "brightness";
constexpr const char* kMode = "mode";
constexpr const char* kSpeed = "speed";
constexpr const char* kLedCount = "ledCount";
}  // namespace fields

// WiFi status snapshot for /api/state (populated by UiServer from WifiManager).
struct WifiInfo {
  std::string mode = "ap";        // "ap" | "sta"
  bool connected = false;
  std::string ip;                 // dotted IP (STA) or AP IP
  std::string ssid;               // connected SSID (STA) — empty in AP
  int rssi = 0;                   // dBm (STA) — 0 in AP
  std::string apName;             // "Ferrocell_<mac>"
  std::string configUrl;          // "http://192.168.1.1:8080"
};

// Result of a POST /api/set.
struct SetResult {
  bool ok = false;
  std::string error;              // set when ok == false
  Settings settings;              // the resulting (normalized) settings
  bool rebooting = false;         // true iff ledCount was present AND changed
};

// Build the GET /api/state document:
//   { ok, settings:{...}, wifi:{...}, fw }
json::Value stateJson(const Settings& s, const WifiInfo& wifi,
                      const char* fw = kFirmwareVersion);

// Apply a POST /api/set document (a JSON object of partial updates) to
// `current`, returning the normalized result. Rules:
//   * The document MUST be a JSON object (else error).
//   * Every key MUST be a known field (else error "unknown field: X").
//   * hue/saturation/brightness/speed MUST be numbers; mode MUST be a string;
//     ledCount MUST be a number (integer). Wrong type -> error.
//   * Ranges are enforced by Settings::normalize (hue wraps; the rest clamp).
//   * mode MUST be a known mode name (else error).
//   * ledCount triggers a reboot ONLY when it actually differs from `current`.
SetResult applySet(const json::Value& doc, const Settings& current);

// A generic error document: { ok:false, error:"..." }.
json::Value errorJson(const std::string& msg);

// A generic { ok:true } document.
json::Value okJson();

}  // namespace api
}  // namespace ferrocell

#endif  // FERROCELL_CORE_API_H
