// =============================================================================
//  Settings — implementation (M3, issue #4). Pure C++17, no Arduino deps.
//
//  normalize() is the single place the canonical ranges are enforced. It is
//  deterministic and side-effect free, so the host unit tests (M6 CI) can
//  assert on it exactly.
// =============================================================================
#include "Settings.h"

#include <cctype>
#include <cmath>
#include <string>

namespace ferrocell {

namespace {

double wrapHue(double h) {
  h = std::fmod(h, settings::kMaxHue);
  if (h < 0.0) h += settings::kMaxHue;
  return h;
}

double clamp01(double x) {
  if (x < settings::kMinBrightness) return settings::kMinBrightness;
  if (x > settings::kMaxBrightness) return settings::kMaxBrightness;
  return x;
}

double clampSpeed(double x) {
  if (x < settings::kMinSpeed) return settings::kMinSpeed;
  if (x > settings::kMaxSpeed) return settings::kMaxSpeed;
  return x;
}

std::uint16_t clampCount(std::uint16_t n) {
  if (n < settings::kMinLedCount) return settings::kMinLedCount;
  if (n > settings::kMaxLedCount) return settings::kMaxLedCount;
  return n;
}

std::string toLower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

bool isKnownMode(const std::string& m) {
  return m == "solid" || m == "rainbow" || m == "breathing" ||
         m == "gradient" || m == "off";
}

}  // namespace

Settings Settings::defaults() {
  Settings s;
  s.ledCount_ = 30;
  s.hue_ = 0.0;
  s.brightness_ = 0.5;
  s.mode_ = "solid";
  s.speed_ = 1.0;
  return s;
}

Settings Settings::normalize(Settings s) {
  s.ledCount_ = clampCount(s.ledCount_);
  s.hue_ = wrapHue(s.hue_);
  s.brightness_ = clamp01(s.brightness_);
  s.speed_ = clampSpeed(s.speed_);
  s.mode_ = toLower(s.mode_);
  if (!isKnownMode(s.mode_)) s.mode_ = "solid";
  return s;
}

std::uint16_t Settings::ledCount() const { return ledCount_; }
double Settings::hue() const { return hue_; }
double Settings::brightness() const { return brightness_; }
const std::string& Settings::mode() const { return mode_; }
double Settings::speed() const { return speed_; }

void Settings::setLedCount(std::uint16_t n) { ledCount_ = n; }
void Settings::setHue(double hue) { hue_ = hue; }
void Settings::setBrightness(double b) { brightness_ = b; }
void Settings::setMode(const std::string& mode) { mode_ = mode; }
void Settings::setSpeed(double speed) { speed_ = speed; }

bool Settings::operator==(const Settings& other) const {
  return ledCount_ == other.ledCount_ && hue_ == other.hue_ &&
         brightness_ == other.brightness_ && mode_ == other.mode_ &&
         speed_ == other.speed_;
}

bool Settings::operator!=(const Settings& other) const {
  return !(*this == other);
}

}  // namespace ferrocell
