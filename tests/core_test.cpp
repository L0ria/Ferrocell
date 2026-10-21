// =============================================================================
//  Ferrocell core unit tests (M2, issue #3).
//
//  Self-contained host-side test of the PURE core (Color + ColorEngine).
//  No Arduino, no FastLED, no hardware. Compiles & runs with:
//      g++ -std=c++17 -I ferrocell/src tests/core_test.cpp ferrocell/src/core/Color.cpp
//          ferrocell/src/core/ColorEngine.cpp -o core_test && ./core_test
//
//  A tiny assertion framework (no external deps) is used. The process exits
//  non-zero if any check fails, so it is CI-friendly.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/Color.h"
#include "core/ColorEngine.h"

using ferrocell::Color;
using ferrocell::ColorEngine;
using ferrocell::Mode;

// --- minimal test framework -------------------------------------------------
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

// --- Color tests ------------------------------------------------------------
static void test_color_rgb_primaries() {
  section("Color::toRgb primaries");
  CHECK_EQ(Color(0, 1, 1).toRgb(), 0xFF0000u);    // red
  CHECK_EQ(Color(120, 1, 1).toRgb(), 0x00FF00u);  // green
  CHECK_EQ(Color(240, 1, 1).toRgb(), 0x0000FFu);  // blue
  CHECK_EQ(Color(60, 1, 1).toRgb(), 0xFFFF00u);   // yellow
  CHECK_EQ(Color(180, 1, 1).toRgb(), 0x00FFFFu);  // cyan
  CHECK_EQ(Color(300, 1, 1).toRgb(), 0xFF00FFu);  // magenta
}

static void test_color_rgb_secondary() {
  section("Color::toRgb secondary/mid hues");
  CHECK_EQ(Color(90, 1, 1).toRgb(), 0x80FF00u);    // chartreuse (128,255,0)
  CHECK_EQ(Color(270, 1, 1).toRgb(), 0x8000FFu);   // violet (128,0,255)
  CHECK_EQ(Color(40, 1, 1).toRgb(), 0xFFAA00u);    // orange (255,170,0)
  CHECK_EQ(Color(20, 0.5, 0.8).toRgb(), 0xCC8866u); // (204,136,102)
  CHECK_EQ(Color(200, 0.5, 0.8).toRgb(), 0x66AACCu); // (102,170,204)
}

static void test_color_grey_and_black() {
  section("Color grey & black");
  CHECK_EQ(Color(0, 0, 0.5).toRgb(), 0x808080u);  // mid grey
  CHECK(Color(0, 0, 0).isBlack());
  CHECK_EQ(Color(0, 0, 0).toRgb(), 0x000000u);
  // value 0 is black regardless of hue/sat
  CHECK(Color(120, 1, 0).isBlack());
  CHECK_EQ(Color(120, 1, 0).toRgb(), 0x000000u);
}

static void test_color_clamping() {
  section("Color clamping of inputs");
  // hue wraps into [0,360)
  CHECK(near(Color(360, 1, 1).hue(), 0.0));
  CHECK(near(Color(720, 1, 1).hue(), 0.0));
  CHECK(near(Color(-120, 1, 1).hue(), 240.0));
  // saturation / value clamp to [0,1]
  CHECK(near(Color(0, 5.0, 1.0).saturation(), 1.0));
  CHECK(near(Color(0, -1.0, 1.0).saturation(), 0.0));
  CHECK(near(Color(0, 1.0, 5.0).value(), 1.0));
  CHECK(near(Color(0, 1.0, -3.0).value(), 0.0));
  // out-of-range hue still yields a valid 8-bit color
  CHECK_EQ(Color(480, 1, 1).toRgb(), Color(120, 1, 1).toRgb());
}

static void test_color_helpers() {
  section("Color helpers (withValue, equality)");
  Color c(200, 1.0, 1.0);
  Color half = c.withValue(0.5);
  CHECK(near(half.value(), 0.5));
  CHECK(near(half.hue(), 200.0));
  CHECK(c != half);
  CHECK(c == Color(200, 1.0, 1.0));
  // withValue clamps factor to [0,1]
  CHECK(near(c.withValue(5.0).value(), 1.0));
  CHECK(near(c.withValue(-1.0).value(), 0.0));
}

// --- ColorEngine tests ------------------------------------------------------
static void test_engine_solid() {
  section("ColorEngine SOLID");
  ColorEngine e(Mode::SOLID);
  auto v = e.colors(120, 0.5, 4, 1.0, 12345);
  CHECK_EQ(v.size(), 4u);
  for (auto& c : v) {
    CHECK(near(c.hue(), 120.0));
    CHECK(near(c.saturation(), 1.0));
    CHECK(near(c.value(), 0.5));
  }
  // SOLID is time-independent
  auto v2 = e.colors(120, 0.5, 4, 1.0, 999999);
  CHECK(v[0] == v2[0]);
}

static void test_engine_off() {
  section("ColorEngine OFF");
  ColorEngine e(Mode::OFF);
  auto v = e.colors(0, 1.0, 3, 2.0, 5000);
  CHECK_EQ(v.size(), 3u);
  for (auto& c : v) CHECK(c.isBlack());
}

static void test_engine_gradient() {
  section("ColorEngine GRADIENT (static, evenly spaced)");
  ColorEngine e(Mode::GRADIENT);
  auto v = e.colors(0, 1.0, 4, 1.0, 0);
  CHECK_EQ(v.size(), 4u);
  CHECK(near(v[0].hue(), 0.0));
  CHECK(near(v[1].hue(), 90.0));
  CHECK(near(v[2].hue(), 180.0));
  CHECK(near(v[3].hue(), 270.0));
  // static: time does not change GRADIENT
  auto v2 = e.colors(0, 1.0, 4, 1.0, 100000);
  for (std::size_t i = 0; i < v.size(); ++i) CHECK(near(v[i].hue(), v2[i].hue()));
}

static void test_engine_rainbow() {
  section("ColorEngine RAINBOW (gradient + time rotation)");
  ColorEngine e(Mode::RAINBOW);
  // at t=0 RAINBOW == GRADIENT
  auto t0 = e.colors(0, 1.0, 4, 1.0, 0);
  CHECK(near(t0[0].hue(), 0.0));
  CHECK(near(t0[3].hue(), 270.0));
  // speed=0.5, t=1s -> +180 deg on every LED
  auto t1 = e.colors(0, 1.0, 4, 0.5, 1000);
  CHECK(near(t1[0].hue(), 180.0));
  CHECK(near(t1[1].hue(), 270.0));
  CHECK(near(t1[2].hue(), 0.0));    // 180+180=360 -> 0
  CHECK(near(t1[3].hue(), 90.0));   // 270+180=450 -> 90
}

static void test_engine_breathing() {
  section("ColorEngine BREATHING (cosine envelope)");
  ColorEngine e(Mode::BREATHING);
  // t=0 -> factor 0 (black)
  auto v0 = e.colors(120, 1.0, 2, 0.5, 0);
  CHECK(v0[0].isBlack());
  // speed=0.5, t=1s -> factor 1 (full brightness)
  auto v1 = e.colors(120, 0.8, 2, 0.5, 1000);
  CHECK(near(v1[0].value(), 0.8));
  // speed=0.5, t=500ms -> factor 0.5
  auto vm = e.colors(120, 0.8, 2, 0.5, 500);
  CHECK(near(vm[0].value(), 0.4));
  // all LEDs identical in BREATHING
  CHECK(v1[0] == v1[1]);
}

static void test_engine_edge_cases() {
  section("ColorEngine edge cases");
  ColorEngine e(Mode::RAINBOW);
  // N == 0 -> empty
  CHECK_EQ(e.colors(0, 1.0, 0, 1.0, 0).size(), 0u);
  // N == 1 -> single LED, hue offset 0
  auto one = e.colors(30, 1.0, 1, 1.0, 0);
  CHECK_EQ(one.size(), 1u);
  CHECK(near(one[0].hue(), 30.0));
  // speed == 0 -> RAINBOW == GRADIENT
  ColorEngine r(Mode::RAINBOW);
  ColorEngine g(Mode::GRADIENT);
  auto rs = r.colors(0, 1.0, 5, 0.0, 99999);
  auto gs = g.colors(0, 1.0, 5, 0.0, 99999);
  for (std::size_t i = 0; i < rs.size(); ++i) CHECK(near(rs[i].hue(), gs[i].hue()));
}

static void test_engine_mode_names() {
  section("ColorEngine mode name parsing");
  CHECK(ColorEngine::isKnownMode("solid"));
  CHECK(ColorEngine::isKnownMode("RAINBOW"));
  CHECK(ColorEngine::isKnownMode("Breathing"));
  CHECK(ColorEngine::isKnownMode("gradient"));
  CHECK(ColorEngine::isKnownMode("off"));
  CHECK(!ColorEngine::isKnownMode("nope"));
  CHECK(!ColorEngine::isKnownMode(""));
  ColorEngine a("rainbow");
  CHECK(a.mode() == Mode::RAINBOW);
  ColorEngine b("SOLID");
  CHECK(b.mode() == Mode::SOLID);
  ColorEngine c("bogus");   // unknown -> SOLID fallback
  CHECK(c.mode() == Mode::SOLID);
  ColorEngine d(nullptr);   // null -> SOLID fallback
  CHECK(d.mode() == Mode::SOLID);
}

int main() {
  std::printf("Ferrocell core tests (M2)\n");
  test_color_rgb_primaries();
  test_color_rgb_secondary();
  test_color_grey_and_black();
  test_color_clamping();
  test_color_helpers();
  test_engine_solid();
  test_engine_off();
  test_engine_gradient();
  test_engine_rainbow();
  test_engine_breathing();
  test_engine_edge_cases();
  test_engine_mode_names();

  std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
  if (g_failures == 0) {
    std::printf("ALL TESTS PASSED\n");
    return 0;
  }
  std::printf("TESTS FAILED\n");
  return 1;
}
