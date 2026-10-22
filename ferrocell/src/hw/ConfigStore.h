// =============================================================================
//  ConfigStore — the SINGLE persistence point for user settings (M3, #4).
//
//  Wraps the ESP32 `Preferences` (NVS) so the rest of the app never touches
//  NVS directly. It loads a `Settings` value (falling back to sane defaults
//  on first boot / missing keys), saves one, and implements the "LED count
//  change -> save + reboot" rule.
//
//  NOT host-testable (depends on the Arduino core's Preferences); it is
//  verified by the firmware build (M6 CI `firmware-build`). The pure logic it
//  delegates to (Settings::defaults/normalize) IS host-tested.
// =============================================================================
#ifndef FERROCELL_HW_CONFIG_STORE_H
#define FERROCELL_HW_CONFIG_STORE_H

#include "../core/Settings.h"

class ConfigStore {
 public:
  // NVS namespace + keys (single source of truth for the on-flash layout).
  static const char* kNamespace;
  static const char* kLedCountKey;
  static const char* kHueKey;
  static const char* kSaturationKey;
  static const char* kBrightnessKey;
  static const char* kModeKey;
  static const char* kSpeedKey;

  // Load the persisted settings. Missing/invalid values fall back to
  // Settings::defaults(); the result is always Settings::normalize()'d.
  // Logs the effective settings over Serial (USB-CDC).
  ferrocell::Settings load();

  // Persist a settings value (normalized first). Returns true on success.
  bool save(const ferrocell::Settings& s);

  // Change the LED count: persist it, then reboot the device (per M3
  // decision — no live re-init of the strip). The reboot is logged first so
  // the intent is visible on the serial console.
  void setLedCount(std::uint16_t n);

  // Wipe all persisted settings and reboot (factory reset; used by the UI).
  void reset();
};

#endif  // FERROCELL_HW_CONFIG_STORE_H
