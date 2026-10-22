// =============================================================================
//  LedController — implementation (M3, issue #4).
//
//  Depends on FastLED, so this file is compiled into the firmware only (NOT
//  part of the host test harness). All color math is delegated to the pure
//  core (ColorEngine + Color); this class only owns the strip buffer and the
//  FastLED calls.
// =============================================================================
#include "LedController.h"

#include <Arduino.h>
#include <FastLED.h>

#include <vector>

#include "../core/ColorEngine.h"

namespace {

// The FastLED strip buffer. Sized to the maximum strip; only `count_` LEDs are
// driven. A fixed allocation avoids a per-frame heap allocation on the device.
CRGB kLeds[LedController::kMaxLeds];

}  // namespace

void LedController::begin(std::size_t count) {
  if (count < 1) count = 1;
  if (count > kMaxLeds) count = kMaxLeds;
  count_ = count;

  // Attach the WS2812B strip on the fixed data pin (GRB order).
  FastLED.addLeds<WS2812B, kPin, GRB>(kLeds, static_cast<int>(count_));
  FastLED.setBrightness(255);  // hardware ceiling full; core applies user brightness
  FastLED.show();              // start with all LEDs off
}

void LedController::render(const ferrocell::Settings& s, std::int64_t timeMs) {
  // The pure core computes every LED color (the ONLY place HSV->RGB happens),
  // writing into our reusable buffer (no per-frame allocation).
  const ferrocell::ColorEngine engine(s.mode().c_str());
  engine.colors(s.hue(), s.saturation(), s.brightness(), count_, s.speed(), timeMs, frame_);

  // Copy the computed colors into the strip buffer (r/g/b are 0..255).
  for (std::size_t i = 0; i < count_; ++i) {
    const ferrocell::Color& c = frame_[i];
    kLeds[i] = CRGB(static_cast<std::uint8_t>(c.r()),
                    static_cast<std::uint8_t>(c.g()),
                    static_cast<std::uint8_t>(c.b()));
  }
  FastLED.show();
}

void LedController::setBrightness(std::uint8_t scale) {
  FastLED.setBrightness(scale);
}

void LedController::show() {
  FastLED.show();
}

void LedController::clear() {
  for (std::size_t i = 0; i < count_; ++i) kLeds[i] = CRGB(0, 0, 0);
  FastLED.show();
}

std::size_t LedController::ledCount() const { return count_; }
