// =============================================================================
//  Color — a pure, hardware-independent HSV color value (M2, issue #3).
//
//  No Arduino / FastLED / hardware dependencies. Compiles standalone with
//  `g++ -std=c++17` and is the unit-test target of the host-side harness.
//
//  Color space (canonical, clamped):
//    hue         0..360   degrees (0 = red, 120 = green, 240 = blue)
//    saturation  0..1     0 = grey, 1 = fully saturated
//    value       0..1     0 = black, 1 = full brightness
//
//  toRgb() converts to 8-bit R/G/B (0..255) using the standard HSV->RGB
//  algorithm (see Color.cpp for the exact, documented formula).
// =============================================================================
#ifndef FERROCELL_CORE_COLOR_H
#define FERROCELL_CORE_COLOR_H

#include <cstdint>

namespace ferrocell {

class Color {
 public:
  // Default: black (hue 0, no saturation, no value).
  Color();

  // Construct from HSV. All inputs are clamped into the canonical ranges:
  //   hue -> wrapped into [0, 360)
  //   saturation -> clamped to [0, 1]
  //   value -> clamped to [0, 1]
  Color(double hue, double saturation, double value);

  // --- accessors (canonical, clamped) ---
  double hue() const;        // 0..360
  double saturation() const; // 0..1
  double value() const;      // 0..1

  // --- conversion ---
  // 8-bit red channel (0..255) of the HSV->RGB conversion.
  std::uint8_t r() const;
  // 8-bit green channel (0..255).
  std::uint8_t g() const;
  // 8-bit blue channel (0..255).
  std::uint8_t b() const;

  // Convenience: the full 24-bit RGB packed as (r<<16)|(g<<8)|b.
  std::uint32_t toRgb() const;

  // --- small helpers ---
  // True if this color is fully black (value 0, regardless of hue/sat).
  bool isBlack() const;

  // Scale the value (brightness) by a factor in [0,1]; hue/saturation kept.
  // factor is clamped to [0,1].
  Color withValue(double factor) const;

  // Equality (compares canonical hue/saturation/value exactly).
  bool operator==(const Color& other) const;
  bool operator!=(const Color& other) const;

 private:
  double hue_;        // 0..360
  double saturation_; // 0..1
  double value_;      // 0..1
};

}  // namespace ferrocell

#endif  // FERROCELL_CORE_COLOR_H
