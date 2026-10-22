// =============================================================================
//  Api — implementation (M5, issue #6). Pure C++17, no Arduino deps.
// =============================================================================
#include "Api.h"

#include <cctype>
#include <string>

namespace ferrocell {
namespace api {

namespace {

using json::Value;

bool isKnownMode(const std::string& m) {
  return m == "solid" || m == "rainbow" || m == "breathing" ||
         m == "gradient" || m == "off";
}

// Is `v` an integer-valued number (for ledCount)?
bool isIntegralNumber(const Value& v) {
  if (!v.isNumber()) return false;
  double n = v.number;
  return n == static_cast<long long>(n) && n >= 0.0 && n <= 1e9;
}

}  // namespace

json::Value stateJson(const Settings& s, const WifiInfo& wifi, const char* fw) {
  Value out = Value::makeObject();

  Value settings = Value::makeObject();
  settings.set("ledCount", Value::makeNumber(static_cast<double>(s.ledCount())));
  settings.set("hue", Value::makeNumber(s.hue()));
  settings.set("saturation", Value::makeNumber(s.saturation()));
  settings.set("brightness", Value::makeNumber(s.brightness()));
  settings.set("mode", Value::makeString(s.mode()));
  settings.set("speed", Value::makeNumber(s.speed()));
  out.set("settings", std::move(settings));

  Value w = Value::makeObject();
  w.set("mode", Value::makeString(wifi.mode));
  w.set("connected", Value::makeBool(wifi.connected));
  w.set("ip", Value::makeString(wifi.ip));
  w.set("ssid", Value::makeString(wifi.ssid));
  w.set("rssi", Value::makeNumber(static_cast<double>(wifi.rssi)));
  w.set("apName", Value::makeString(wifi.apName));
  w.set("configUrl", Value::makeString(wifi.configUrl));
  out.set("wifi", std::move(w));

  out.set("fw", Value::makeString(fw ? fw : kFirmwareVersion));
  out.set("ok", Value::makeBool(true));
  return out;
}

SetResult applySet(const Value& doc, const Settings& current) {
  SetResult r;

  if (!doc.isObject()) {
    r.error = "body must be a JSON object";
    return r;
  }

  Settings s = current;
  bool ledCountPresent = false;
  double ledCountValue = 0.0;

  for (const auto& kv : doc.object) {
    const std::string& key = kv.first;
    const Value& v = kv.second;

    if (key == fields::kHue) {
      if (!v.isNumber()) { r.error = "hue must be a number"; return r; }
      s.setHue(v.number);
    } else if (key == fields::kSaturation) {
      if (!v.isNumber()) { r.error = "saturation must be a number"; return r; }
      s.setSaturation(v.number);
    } else if (key == fields::kBrightness) {
      if (!v.isNumber()) { r.error = "brightness must be a number"; return r; }
      s.setBrightness(v.number);
    } else if (key == fields::kMode) {
      if (!v.isString()) { r.error = "mode must be a string"; return r; }
      std::string m = v.string;
      for (char& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (!isKnownMode(m)) { r.error = "unknown mode: " + m; return r; }
      s.setMode(m);
    } else if (key == fields::kSpeed) {
      if (!v.isNumber()) { r.error = "speed must be a number"; return r; }
      s.setSpeed(v.number);
    } else if (key == fields::kLedCount) {
      if (!isIntegralNumber(v)) { r.error = "ledCount must be an integer"; return r; }
      ledCountPresent = true;
      ledCountValue = v.number;
    } else {
      r.error = "unknown field: " + key;
      return r;
    }
  }

  s = Settings::normalize(s);

  if (ledCountPresent) {
    s.setLedCount(static_cast<std::uint16_t>(ledCountValue));
    s = Settings::normalize(s);
    r.rebooting = (static_cast<std::uint16_t>(ledCountValue) != current.ledCount());
  }

  r.ok = true;
  r.settings = s;
  return r;
}

json::Value errorJson(const std::string& msg) {
  Value v = Value::makeObject();
  v.set("ok", Value::makeBool(false));
  v.set("error", Value::makeString(msg));
  return v;
}

json::Value okJson() {
  Value v = Value::makeObject();
  v.set("ok", Value::makeBool(true));
  return v;
}

}  // namespace api
}  // namespace ferrocell
