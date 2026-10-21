// =============================================================================
//  Color — implementation (M2, issue #3). Pure C++17, no Arduino deps.
//
//  HSV -> RGB (standard algorithm, documented exactly for testability):
//    Given H in [0,360), S in [0,1], V in [0,1]:
//      C  = V * S
//      H' = H / 60
//      X  = C * (1 - | (H' mod 2) - 1 |)
//      m  = V - C
//      (r',g',b') =
//        [0<=H'<1] (C, X, 0)  [1<=H'<2] (X, C, 0)  [2<=H'<3] (0, C, X)
//        [3<=H'<4] (0, X, C)  [4<=H'<5] (X, 0, C)  [5<=H'<6] (C, 0, X)
//      channel = round((c' + m) * 255), clamped to [0,255]
// =============================================================================
#include "Color.h"

#include <cmath>

namespace ferrocell {

namespace {

double wrapHue(double h) {
  h = std::fmod(h, 360.0);
  if (h < 0.0) h += 360.0;
  return h;
}

double clamp01(double x) {
  if (x < 0.0) return 0.0;
  if (x > 1.0) return 1.0;
  return x;
}

// Round-to-nearest 8-bit (0..255) of a [0,1] value.
std::uint8_t toByte(double x) {
  long v = static_cast<long>(std::round(x * 255.0));
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return static_cast<std::uint8_t>(v);
}

// Shared HSV -> 8-bit RGB conversion (single source of truth for r/g/b).
// Returns {r, g, b} each in [0,255].
struct Rgb {
  std::uint8_t r, g, b;
};

Rgb hsvToRgb(double hue, double sat, double val) {
  if (val == 0.0) return {0, 0, 0};
  if (sat == 0.0) {
    const std::uint8_t grey = toByte(val);
    return {grey, grey, grey};
  }
  const double c = val * sat;
  const double hp = hue / 60.0;
  const double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
  const double m = val - c;
  double r1 = 0.0, g1 = 0.0, b1 = 0.0;
  if (hp < 1.0) { r1 = c; g1 = x; }
  else if (hp < 2.0) { r1 = x; g1 = c; }
  else if (hp < 3.0) { g1 = c; b1 = x; }
  else if (hp < 4.0) { g1 = x; b1 = c; }
  else if (hp < 5.0) { r1 = x; b1 = c; }
  else { r1 = c; b1 = x; }
  return {toByte(r1 + m), toByte(g1 + m), toByte(b1 + m)};
}

}  // namespace

Color::Color() : hue_(0.0), saturation_(0.0), value_(0.0) {}

Color::Color(double hue, double saturation, double value)
    : hue_(wrapHue(hue)),
      saturation_(clamp01(saturation)),
      value_(clamp01(value)) {}

double Color::hue() const { return hue_; }
double Color::saturation() const { return saturation_; }
double Color::value() const { return value_; }

std::uint8_t Color::r() const { return hsvToRgb(hue_, saturation_, value_).r; }
std::uint8_t Color::g() const { return hsvToRgb(hue_, saturation_, value_).g; }
std::uint8_t Color::b() const { return hsvToRgb(hue_, saturation_, value_).b; }

std::uint32_t Color::toRgb() const {
  const Rgb c = hsvToRgb(hue_, saturation_, value_);
  return (static_cast<std::uint32_t>(c.r) << 16) |
         (static_cast<std::uint32_t>(c.g) << 8) |
         static_cast<std::uint32_t>(c.b);
}

bool Color::isBlack() const { return value_ == 0.0; }

Color Color::withValue(double factor) const {
  return Color(hue_, saturation_, value_ * clamp01(factor));
}

bool Color::operator==(const Color& other) const {
  return hue_ == other.hue_ && saturation_ == other.saturation_ &&
         value_ == other.value_;
}

bool Color::operator!=(const Color& other) const { return !(*this == other); }

}  // namespace ferrocell
