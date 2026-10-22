// =============================================================================
//  ConfigStore — implementation (M3, issue #4).
//
//  Depends on the Arduino core (Preferences/NVS, Serial, ESP.restart), so it
//  is compiled into the firmware only — NOT part of the host test harness.
//  The pure settings logic it relies on lives in core/Settings (host-tested).
// =============================================================================
#include "ConfigStore.h"

#include <Arduino.h>
#include <Esp.h>
#include <Preferences.h>

#include <string>

const char* ConfigStore::kNamespace = "ferrocell";
const char* ConfigStore::kLedCountKey = "ledcount";
const char* ConfigStore::kHueKey = "hue";
const char* ConfigStore::kSaturationKey = "sat";
const char* ConfigStore::kBrightnessKey = "bright";
const char* ConfigStore::kModeKey = "mode";
const char* ConfigStore::kSpeedKey = "speed";

ferrocell::Settings ConfigStore::load() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    Serial.println("[config] NVS begin failed — using defaults");
    return ferrocell::Settings::defaults();
  }

  const auto d = ferrocell::Settings::defaults();
  ferrocell::Settings s;
  s.setLedCount(prefs.getUShort(kLedCountKey, d.ledCount()));
  s.setHue(prefs.getDouble(kHueKey, d.hue()));
  s.setSaturation(prefs.getDouble(kSaturationKey, d.saturation()));
  s.setBrightness(prefs.getDouble(kBrightnessKey, d.brightness()));
  s.setSpeed(prefs.getDouble(kSpeedKey, d.speed()));

  char modeBuf[32] = {0};
  const size_t got = prefs.getString(kModeKey, modeBuf, sizeof(modeBuf));
  if (got > 0 && got < sizeof(modeBuf)) {
    s.setMode(std::string(modeBuf, got));
  } else {
    s.setMode(d.mode());
  }
  prefs.end();

  s = ferrocell::Settings::normalize(s);

  Serial.printf("[config] loaded: leds=%u hue=%.0f sat=%.2f bright=%.2f mode=%s speed=%.2f\n",
                s.ledCount(), s.hue(), s.saturation(), s.brightness(), s.mode().c_str(), s.speed());
  return s;
}

bool ConfigStore::save(const ferrocell::Settings& raw) {
  const ferrocell::Settings s = ferrocell::Settings::normalize(raw);

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    Serial.println("[config] NVS begin (write) failed");
    return false;
  }
  bool ok = true;
  ok = prefs.putUShort(kLedCountKey, s.ledCount()) == 2 && ok;
  ok = prefs.putDouble(kHueKey, s.hue()) == 8 && ok;
  ok = prefs.putDouble(kSaturationKey, s.saturation()) == 8 && ok;
  ok = prefs.putDouble(kBrightnessKey, s.brightness()) == 8 && ok;
  ok = prefs.putDouble(kSpeedKey, s.speed()) == 8 && ok;
  ok = prefs.putString(kModeKey, s.mode().c_str()) == s.mode().size() && ok;
  prefs.end();

  Serial.printf("[config] saved: leds=%u hue=%.0f sat=%.2f bright=%.2f mode=%s speed=%.2f (%s)\n",
                s.ledCount(), s.hue(), s.saturation(), s.brightness(), s.mode().c_str(), s.speed(),
                ok ? "ok" : "FAILED");
  return ok;
}

void ConfigStore::setLedCount(std::uint16_t n) {
  // Only the count key changes; the other settings are left untouched.
  const std::uint16_t count =
      (n < ferrocell::settings::kMinLedCount) ? ferrocell::settings::kMinLedCount
                                              : (n > ferrocell::settings::kMaxLedCount ? ferrocell::settings::kMaxLedCount : n);
  Preferences prefs;
  if (prefs.begin(kNamespace, false)) {
    prefs.putUShort(kLedCountKey, count);
    prefs.end();
  }
  Serial.printf("[config] LED count -> %u; rebooting to apply (no live re-init)\n", count);
  ESP.restart();
}

void ConfigStore::reset() {
  Preferences prefs;
  if (prefs.begin(kNamespace, false)) {
    prefs.clear();
    prefs.end();
  }
  Serial.println("[config] factory reset (NVS cleared); rebooting");
  ESP.restart();
}
