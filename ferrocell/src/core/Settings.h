// =============================================================================
//  Settings — the user-facing lighting configuration (M3, issue #4).
//
//  A small, PURE value type: it holds the settings the user can change (LED
//  count, hue, brightness, mode, speed) and defines the canonical ranges and
//  the sane first-boot defaults. It has NO Arduino / FastLED / hardware
//  dependencies, so it compiles and is unit-tested on the host with
//  `g++ -std=c++17` (see tests/settings_test.cpp).
//
//  The hardware layer (ConfigStore) persists a Settings value; the UI layer
//  (M5) serializes one to / from JSON. Keeping the value type pure means the
//  clamping/defaults logic is testable without a device.
//
//  Ranges (canonical):
//    ledCount     1 .. 512     (0 is invalid; FastLED supports up to 512)
//    hue          0 .. 360     degrees (wrapped, not clamped)
//    brightness   0 .. 1       (0 = off, 1 = full)
//    mode         one of: "solid" "rainbow" "breathing" "gradient" "off"
//    speed        0 .. 10      (0 = no animation; 1 = normal)
// =============================================================================
#ifndef FERROCELL_CORE_SETTINGS_H
#define FERROCELL_CORE_SETTINGS_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace ferrocell {

// Canonical bounds (single source of truth for clamping + tests).
namespace settings {
constexpr std::uint16_t kMinLedCount = 1;
constexpr std::uint16_t kMaxLedCount = 512;  // FastLED single-strip limit
constexpr double kMaxHue = 360.0;
constexpr double kMinBrightness = 0.0;
constexpr double kMaxBrightness = 1.0;
constexpr double kMinSpeed = 0.0;
constexpr double kMaxSpeed = 10.0;
}  // namespace settings

class Settings {
 public:
  // Sane first-boot defaults: a visible, calm, predictable state.
  static Settings defaults();

  // Normalize an arbitrary Settings into the canonical ranges:
  //   ledCount   -> clamped to [kMinLedCount, kMaxLedCount]
  //   hue        -> wrapped into [0, 360)
  //   brightness -> clamped to [0, 1]
  //   mode       -> lower-cased; unknown/empty -> "solid"
  //   speed      -> clamped to [0, kMaxSpeed]
  // Pure and deterministic (no I/O), so it is unit-testable on the host.
  static Settings normalize(Settings s);

  // --- accessors ---
  std::uint16_t ledCount() const;
  double hue() const;
  double brightness() const;
  const std::string& mode() const;
  double speed() const;

  // --- mutators (no clamping; call normalize() to canonicalize) ---
  void setLedCount(std::uint16_t n);
  void setHue(double hue);
  void setBrightness(double b);
  void setMode(const std::string& mode);
  void setSpeed(double speed);

  // Equality (all fields).
  bool operator==(const Settings& other) const;
  bool operator!=(const Settings& other) const;

 private:
  std::uint16_t ledCount_ = settings::kMinLedCount;
  double hue_ = 0.0;
  double brightness_ = 1.0;
  std::string mode_ = "solid";
  double speed_ = 1.0;
};

}  // namespace ferrocell

#endif  // FERROCELL_CORE_SETTINGS_H
