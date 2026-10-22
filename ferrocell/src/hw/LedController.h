// =============================================================================
//  LedController — the ONLY class that talks to FastLED (M3, issue #4).
//
//  Owns the WS2812B strip on a single data pin (fixed GPIO7) and the LED
//  count (supplied from ConfigStore). It renders a frame by delegating the
//  per-LED color math to the pure core `ColorEngine` — this class does NO
//  HSV/RGB math itself (M3 acceptance: "no color math in the hw layer").
//
//  Brightness model (single source of truth, no double-application):
//    * The USER brightness (Settings.brightness) is applied by the ColorEngine
//      (the pure, host-tested core) — it is part of the computed color.
//    * FastLED's global scale is a separate HARDWARE ceiling (power limiting),
//      set via setBrightness(); it defaults to 255 (full) so it does not
//      re-dim the core's brightness.
//
//  Dirty-check (issue #14): the loop renders every ~10 ms, but FastLED.show()
//  is a blocking SPI/DMA wait. In static modes (solid/off/gradient) the frame
//  is constant, so re-showing it 100x/sec is pure waste that starves the web
//  server. showIfChanged() compares the computed frame to the last-shown
//  buffer and only drives the strip when it actually differs.
//
//  Depends on FastLED, so it is compiled into the firmware only — NOT part of
//  the host test harness (the color math it delegates to IS host-tested).
// =============================================================================
#ifndef FERROCELL_HW_LED_CONTROLLER_H
#define FERROCELL_HW_LED_CONTROLLER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "FastLED.h"

#include "../core/Settings.h"
#include "../core/Color.h"

class LedController {
 public:
  // Strip data pin (Waveshare ESP32-S3-Zero; free of strapping/PSRAM).
  // FastLED needs the pin at compile time, so it is a constant.
  static constexpr std::uint8_t kPin = 7;
  // FastLED single-strip limit (matches Settings::kMaxLedCount).
  static constexpr std::size_t kMaxLeds = 512;

  // Attach to the strip (on kPin) with `count` LEDs. `count` is clamped to
  // [1, kMaxLeds]. Call once at boot.
  void begin(std::size_t count);

  // Render one frame for the given settings at the given time.
  //   timeMs is INJECTED (deterministic): pass millis() on the device.
  // Uses ColorEngine(settings.mode) to compute every LED (core does the math),
  // writes the result into the strip buffer, and pushes it to the strip ONLY
  // if the frame differs from the last one shown (issue #14 dirty-check).
  void render(const ferrocell::Settings& s, std::int64_t timeMs);

  // Push the current buffer to the strip ONLY if it differs from the last
  // frame actually shown. Returns true if the strip was driven, false if the
  // frame was unchanged (and the show was skipped).
  bool showIfChanged();

  // Set the FastLED global brightness scale (0..255) — a hardware ceiling
  // applied on every show(). Default 255 (full) so it does not re-dim the
  // core's user brightness.
  void setBrightness(std::uint8_t scale);

  // Push the current buffer to the strip unconditionally (marks it shown).
  void show();

  // Turn every LED off (and push).
  void clear();

  // The number of LEDs this controller is driving.
  std::size_t ledCount() const;

 private:
  std::size_t count_ = 0;
  // Reusable per-frame buffer (avoids a heap allocation on every render).
  std::vector<ferrocell::Color> frame_;
  // The last frame actually driven to the strip (for the dirty-check).
  std::vector<CRGB> lastShown_;
};

#endif  // FERROCELL_HW_LED_CONTROLLER_H
