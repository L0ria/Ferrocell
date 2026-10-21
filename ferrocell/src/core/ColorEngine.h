// =============================================================================
//  ColorEngine — pure, deterministic per-LED color generator (M2, issue #3).
//
//  Given (mode, hue, brightness, ledCount, speed, timeMs) it produces the
//  color of every LED. It is a PURE function of its inputs: time is INJECTED
//  (timeMs), never read from a clock, so the output is fully deterministic
//  and unit-testable on the host.
//
//  No Arduino / FastLED / hardware dependencies. Compiles with g++ -std=c++17.
//
//  Modes (each documented exactly in ColorEngine.cpp so tests can assert on
//  precise values):
//    SOLID      every LED = Color(hue, 1.0, brightness)
//    RAINBOW    LED i hue = (hue + i*360/N + 360*speed*timeSec) mod 360
//    GRADIENT   LED i hue = (hue + i*360/N) mod 360          (static)
//    BREATHING  every LED = Color(hue, 1.0, brightness * (1-cos(2*pi*speed*timeSec))/2)
//    OFF        every LED = black
// =============================================================================
#ifndef FERROCELL_CORE_COLOR_ENGINE_H
#define FERROCELL_CORE_COLOR_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Color.h"

namespace ferrocell {

// The set of lighting modes the engine can render.
enum class Mode {
  SOLID,
  RAINBOW,
  BREATHING,
  GRADIENT,
  OFF,
};

class ColorEngine {
 public:
  // Construct with an explicit mode.
  explicit ColorEngine(Mode mode);

  // Construct from a textual mode name (case-insensitive). Unknown names
  // fall back to SOLID.
  explicit ColorEngine(const char* modeName);

  // Set the mode (also via name).
  void setMode(Mode mode);
  void setMode(const char* modeName);

  // --- pure rendering -------------------------------------------------------
  // Compute the color of every LED for the given state.
  //   hue        base hue (degrees); used as the anchor for SOLID/BREATHING
  //              and the starting hue for RAINBOW/GRADIENT.
  //   brightness overall brightness in [0,1].
  //   ledCount   number of LEDs on the strip (>= 0).
  //   speed      animation speed factor (>= 0); 0 = no motion.
  //   timeMs     current time in milliseconds (INJECTED; deterministic).
  // Returns a vector of exactly `ledCount` colors (empty if ledCount == 0).
  std::vector<Color> colors(double hue,
                            double brightness,
                            std::size_t ledCount,
                            double speed,
                            std::int64_t timeMs) const;

  // The current mode.
  Mode mode() const;

  // True if the mode name is recognized (case-insensitive).
  static bool isKnownMode(const char* modeName);

 private:
  Mode mode_;
};

}  // namespace ferrocell

#endif  // FERROCELL_CORE_COLOR_ENGINE_H
