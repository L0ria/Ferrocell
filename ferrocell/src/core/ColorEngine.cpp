// =============================================================================
//  ColorEngine — implementation (M2, issue #3). Pure C++17, no Arduino deps.
//
//  Exact, documented formulas (the contract the unit tests assert against):
//    Let N = ledCount, t = timeMs / 1000.0 (seconds).
//
//    SOLID:      for all i:   Color(hue, 1.0, brightness)
//
//    RAINBOW:    for all i:   h_i = (hue + i * 360 / N + 360 * speed * t) mod 360
//                            Color(h_i, 1.0, brightness)
//
//    GRADIENT:   for all i:   h_i = (hue + i * 360 / N) mod 360
//                            Color(h_i, 1.0, brightness)
//
//    BREATHING:  factor = (1 - cos(2 * pi * speed * t)) / 2     (in [0,1])
//                for all i:   Color(hue, 1.0, brightness * factor)
//
//    OFF:        for all i:   Color(0, 0, 0)   (black)
//
//  Edge cases:
//    N == 0      -> empty vector.
//    N == 1      -> RAINBOW/GRADIENT hue offset is 0 (i*360/1 with i=0 -> 0).
//    speed == 0  -> RAINBOW == GRADIENT (no time term); BREATHING factor = 0.
// =============================================================================
#include "ColorEngine.h"

#include <cmath>
#include <cstring>

namespace ferrocell {

namespace {

constexpr double kPi = 3.14159265358979323846;

double mod360(double x) {
  x = std::fmod(x, 360.0);
  if (x < 0.0) x += 360.0;
  return x;
}

bool iequals(const char* a, const char* b) {
  while (*a && *b) {
    int ca = *a;
    int cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

}  // namespace

ColorEngine::ColorEngine(Mode mode) : mode_(mode) {}

ColorEngine::ColorEngine(const char* modeName) : mode_(Mode::SOLID) {
  setMode(modeName);
}

void ColorEngine::setMode(Mode mode) { mode_ = mode; }

void ColorEngine::setMode(const char* modeName) {
  if (modeName == nullptr) {
    mode_ = Mode::SOLID;
    return;
  }
  if (iequals(modeName, "solid")) mode_ = Mode::SOLID;
  else if (iequals(modeName, "rainbow")) mode_ = Mode::RAINBOW;
  else if (iequals(modeName, "breathing")) mode_ = Mode::BREATHING;
  else if (iequals(modeName, "gradient")) mode_ = Mode::GRADIENT;
  else if (iequals(modeName, "off")) mode_ = Mode::OFF;
  else mode_ = Mode::SOLID;
}

Mode ColorEngine::mode() const { return mode_; }

bool ColorEngine::isKnownMode(const char* modeName) {
  if (modeName == nullptr) return false;
  return iequals(modeName, "solid") || iequals(modeName, "rainbow") ||
         iequals(modeName, "breathing") || iequals(modeName, "gradient") ||
         iequals(modeName, "off");
}

void ColorEngine::colors(double hue,
                         double brightness,
                         std::size_t ledCount,
                         double speed,
                         std::int64_t timeMs,
                         std::vector<Color>& out) const {
  out.clear();
  if (ledCount == 0) return;
  out.resize(ledCount);

  const double t = static_cast<double>(timeMs) / 1000.0;

  switch (mode_) {
    case Mode::OFF:
      for (std::size_t i = 0; i < ledCount; ++i) out[i] = Color(0.0, 0.0, 0.0);
      break;

    case Mode::SOLID:
      for (std::size_t i = 0; i < ledCount; ++i) out[i] = Color(hue, 1.0, brightness);
      break;

    case Mode::RAINBOW:
    case Mode::GRADIENT: {
      const bool animate = (mode_ == Mode::RAINBOW);
      const double base = hue;
      for (std::size_t i = 0; i < ledCount; ++i) {
        double h = base + static_cast<double>(i) * (360.0 / static_cast<double>(ledCount));
        if (animate) h += 360.0 * speed * t;
        out[i] = Color(mod360(h), 1.0, brightness);
      }
      break;
    }

    case Mode::BREATHING: {
      const double factor = (1.0 - std::cos(2.0 * kPi * speed * t)) / 2.0;
      for (std::size_t i = 0; i < ledCount; ++i)
        out[i] = Color(hue, 1.0, brightness * factor);
      break;
    }
  }
}

std::vector<Color> ColorEngine::colors(double hue,
                                       double brightness,
                                       std::size_t ledCount,
                                       double speed,
                                       std::int64_t timeMs) const {
  std::vector<Color> out;
  colors(hue, brightness, ledCount, speed, timeMs, out);
  return out;
}

}  // namespace ferrocell
