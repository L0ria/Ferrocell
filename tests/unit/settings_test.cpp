// =============================================================================
//  Ferrocell Settings unit tests (M3, issue #4).
//
//  Self-contained host-side test of the PURE `Settings` value type (defaults +
//  normalize/clamping). No Arduino, no FastLED, no hardware. The same
//  assertion framework as core_test.cpp is reused inline.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <string>

#include "core/Settings.h"

using ferrocell::Settings;

static int g_checks = 0;
static int g_failures = 0;
static std::string g_current;

#define CHECK(cond)                                                            \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::printf("  FAIL [%s] %s:%d: %s\n", g_current.c_str(), __FILE__,      \
                  __LINE__, #cond);                                            \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!((a) == (b))) {                                                       \
      ++g_failures;                                                            \
      std::printf("  FAIL [%s] %s:%d: expected %s == %s\n",                   \
                  g_current.c_str(), __FILE__, __LINE__, #a, #b);              \
    }                                                                          \
  } while (0)

static void section(const char* name) { g_current = name; std::printf("== %s\n", name); }
static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

static void test_defaults() {
  section("Settings::defaults");
  const Settings d = Settings::defaults();
  CHECK(d.ledCount() >= 1);
  CHECK(d.ledCount() <= 512);
  CHECK(near(d.hue(), 0.0));
  CHECK(d.brightness() >= 0.0 && d.brightness() <= 1.0);
  CHECK(d.mode() == "solid");
  CHECK(d.speed() >= 0.0 && d.speed() <= 10.0);
}

static void test_normalize_counts() {
  section("Settings::normalize ledCount clamping");
  Settings s = Settings::defaults();
  s.setLedCount(0);
  CHECK_EQ(Settings::normalize(s).ledCount(), (std::uint16_t)1);

  Settings t = Settings::defaults();
  t.setLedCount(9999);
  CHECK_EQ(Settings::normalize(t).ledCount(), (std::uint16_t)512);

  Settings u = Settings::defaults();
  u.setLedCount(120);
  CHECK_EQ(Settings::normalize(u).ledCount(), (std::uint16_t)120);
}

static void test_normalize_hue() {
  section("Settings::normalize hue wrapping");
  Settings s = Settings::defaults();
  s.setHue(360.0);
  CHECK(near(Settings::normalize(s).hue(), 0.0));

  Settings t = Settings::defaults();
  t.setHue(720.5);
  CHECK(near(Settings::normalize(t).hue(), 0.5));

  Settings u = Settings::defaults();
  u.setHue(-90.0);
  CHECK(near(Settings::normalize(u).hue(), 270.0));

  Settings v = Settings::defaults();
  v.setHue(123.0);
  CHECK(near(Settings::normalize(v).hue(), 123.0));
}

static void test_normalize_brightness_speed() {
  section("Settings::normalize brightness + speed clamping");
  Settings s = Settings::defaults();
  s.setBrightness(5.0);
  CHECK(near(Settings::normalize(s).brightness(), 1.0));

  Settings t = Settings::defaults();
  t.setBrightness(-1.0);
  CHECK(near(Settings::normalize(t).brightness(), 0.0));

  Settings u = Settings::defaults();
  u.setBrightness(0.25);
  CHECK(near(Settings::normalize(u).brightness(), 0.25));

  Settings a = Settings::defaults();
  a.setSpeed(99.0);
  CHECK(near(Settings::normalize(a).speed(), 10.0));

  Settings b = Settings::defaults();
  b.setSpeed(-3.0);
  CHECK(near(Settings::normalize(b).speed(), 0.0));

  Settings c = Settings::defaults();
  c.setSpeed(2.5);
  CHECK(near(Settings::normalize(c).speed(), 2.5));
}

static void test_normalize_mode() {
  section("Settings::normalize mode");
  Settings s = Settings::defaults();
  s.setMode("RAINBOW");
  CHECK(Settings::normalize(s).mode() == "rainbow");

  Settings t = Settings::defaults();
  t.setMode("Breathing");
  CHECK(Settings::normalize(t).mode() == "breathing");

  Settings u = Settings::defaults();
  u.setMode("Off");
  CHECK(Settings::normalize(u).mode() == "off");

  Settings v = Settings::defaults();
  v.setMode("bogus");
  CHECK(Settings::normalize(v).mode() == "solid");

  Settings w = Settings::defaults();
  w.setMode("");
  CHECK(Settings::normalize(w).mode() == "solid");
}

static void test_equality() {
  section("Settings equality");
  const Settings a = Settings::defaults();
  const Settings b = Settings::defaults();
  CHECK(a == b);
  Settings c = Settings::defaults();
  c.setHue(10.0);
  CHECK(a != c);
}

int main() {
  std::printf("Ferrocell Settings tests\n");
  test_defaults();
  test_normalize_counts();
  test_normalize_hue();
  test_normalize_brightness_speed();
  test_normalize_mode();
  test_equality();
  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
