// =============================================================================
//  Ferrocell API unit tests (M5, issue #6).
//
//  Self-contained host-side test of the PURE JSON layer + the API contract
//  (Json + Api). No Arduino, no WebServer, no hardware. It is built and run
//  by tests/run_tests.sh (g++ -std=c++17) alongside core_test and
//  settings_test.
//
//  This is the "JS contract (endpoints + payload shapes) is stable for the
//  Node harness (M6)" acceptance: the exact shapes the web UI + the C++ core
//  agree on are asserted here on the host.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/Api.h"
#include "core/Json.h"
#include "core/Settings.h"

using ferrocell::Settings;
using ferrocell::json::Value;
using ferrocell::json::parse;
using ferrocell::json::dump;

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
      std::printf("  FAIL [%s] %s:%d: expected %s == %s\n", g_current.c_str(), \
                  __FILE__, __LINE__, #a, #b);                                 \
    }                                                                          \
  } while (0)

static void section(const char* name) { g_current = name; std::printf("== %s\n", name); }
static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

// --- Json round-trip --------------------------------------------------------
static void test_json_roundtrip() {
  section("Json round-trip");
  Value v = Value::makeObject();
  v.set("ok", Value::makeBool(true));
  v.set("n", Value::makeNumber(42));
  v.set("pi", Value::makeNumber(3.14159));
  v.set("s", Value::makeString("hello \"world\" \n tab\t"));
  Value arr = Value::makeArray();
  arr.array.push_back(Value::makeNumber(1));
  arr.array.push_back(Value::makeString("x"));
  v.set("arr", std::move(arr));

  const std::string text = dump(v);
  Value back;
  std::string err;
  CHECK(parse(text, back, &err));
  CHECK(back.isObject());
  CHECK(back.getBool("ok", false));
  CHECK(near(back.getNumber("n", 0.0), 42.0));
  CHECK(near(back.getNumber("pi", 0.0), 3.14159));
  CHECK(back.getString("s", "") == "hello \"world\" \n tab\t");
  const Value* a = back.find("arr");
  CHECK(a && a->isArray() && a->array.size() == 2);
  CHECK(a && near(a->array[0].asNumber(), 1.0));
  CHECK(a && a->array[1].asString() == "x");
}

static void test_json_errors() {
  section("Json parse errors");
  Value v;
  std::string err;
  CHECK(!parse("", v, &err));
  CHECK(!parse("{", v, &err));
  CHECK(!parse("{\"a\":}", v, &err));
  CHECK(!parse("[1,2", v, &err));
  CHECK(!parse("tru", v, &err));
  CHECK(!parse("{\"a\" 1}", v, &err));
  // trailing junk
  CHECK(!parse("1 2", v, &err));
}

static void test_json_numbers() {
  section("Json numbers");
  Value v;
  std::string err;
  CHECK(parse("0", v, &err) && near(v.asNumber(), 0.0));
  CHECK(parse("-5", v, &err) && near(v.asNumber(), -5.0));
  CHECK(parse("1.5", v, &err) && near(v.asNumber(), 1.5));
  CHECK(parse("1e3", v, &err) && near(v.asNumber(), 1000.0));
  CHECK(parse("1.5e2", v, &err) && near(v.asNumber(), 150.0));
}

// --- Api::stateJson ---------------------------------------------------------
static void test_state_json_shape() {
  section("Api::stateJson shape");
  Settings s = Settings::defaults();
  s.setHue(210.0);
  s.setSaturation(0.5);
  s.setBrightness(0.75);
  s.setMode("rainbow");
  s.setSpeed(2.0);
  s.setLedCount(64);
  s = Settings::normalize(s);

  ferrocell::api::WifiInfo w;
  w.mode = "sta";
  w.connected = true;
  w.ip = "192.168.1.50";
  w.ssid = "net";
  w.rssi = -55;
  w.apName = "Ferrocell_123456789";
  w.configUrl = "http://192.168.1.1:8080";

  const Value doc = ferrocell::api::stateJson(s, w);
  const std::string text = dump(doc);

  Value back;
  std::string err;
  CHECK(parse(text, back, &err));
  CHECK(back.getBool("ok", false));
  CHECK(back.getString("fw", "") == ferrocell::api::kFirmwareVersion);

  const Value* st = back.find("settings");
  CHECK(st && st->isObject());
  CHECK(st && st->getNumber("ledCount", 0) == 64.0);
  CHECK(st && near(st->getNumber("hue", 0), 210.0));
  CHECK(st && near(st->getNumber("saturation", 0), 0.5));
  CHECK(st && near(st->getNumber("brightness", 0), 0.75));
  CHECK(st && st->getString("mode", "") == "rainbow");
  CHECK(st && near(st->getNumber("speed", 0), 2.0));

  const Value* wf = back.find("wifi");
  CHECK(wf && wf->getString("mode", "") == "sta");
  CHECK(wf && wf->getBool("connected", false));
  CHECK(wf && wf->getString("ip", "") == "192.168.1.50");
  CHECK(wf && wf->getString("ssid", "") == "net");
  CHECK(wf && near(wf->getNumber("rssi", 0), -55.0));
  CHECK(wf && wf->getString("apName", "") == "Ferrocell_123456789");
  CHECK(wf && wf->getString("configUrl", "") == "http://192.168.1.1:8080");
}

// --- Api::applySet ----------------------------------------------------------
static Value obj(const char* json, std::string* err = nullptr) {
  Value v;
  if (!parse(json, v, err)) return Value::makeNull();
  return v;
}

static void test_apply_set_partial() {
  section("Api::applySet partial update");
  Settings cur = Settings::defaults();  // ledCount 30, hue 0, sat 1, bright 0.5, solid, speed 1
  Value doc = obj("{\"hue\": 120}");
  ferrocell::api::SetResult r = ferrocell::api::applySet(doc, cur);
  CHECK(r.ok);
  CHECK(near(r.settings.hue(), 120.0));
  // untouched fields preserved
  CHECK(near(r.settings.brightness(), 0.5));
  CHECK(r.settings.mode() == "solid");
  CHECK(near(r.settings.speed(), 1.0));
  CHECK_EQ(r.settings.ledCount(), (std::uint16_t)30);
  CHECK(!r.rebooting);
}

static void test_apply_set_all_fields() {
  section("Api::applySet all fields");
  Settings cur = Settings::defaults();
  Value doc = obj("{\"hue\":720,\"saturation\":0.25,\"brightness\":2,\"mode\":\"BREATHING\",\"speed\":-1,\"ledCount\":64}");
  ferrocell::api::SetResult r = ferrocell::api::applySet(doc, cur);
  CHECK(r.ok);
  CHECK(near(r.settings.hue(), 0.0));          // 720 wraps to 0
  CHECK(near(r.settings.saturation(), 0.25));
  CHECK(near(r.settings.brightness(), 1.0));   // clamped to 1
  CHECK(r.settings.mode() == "breathing");     // lower-cased
  CHECK(near(r.settings.speed(), 0.0));        // clamped to 0
  CHECK_EQ(r.settings.ledCount(), (std::uint16_t)64);
  CHECK(r.rebooting);  // ledCount changed (30 -> 64)
}

static void test_apply_set_ledcount_noop() {
  section("Api::applySet ledCount unchanged = no reboot");
  Settings cur = Settings::defaults();  // ledCount 30
  Value doc = obj("{\"ledCount\":30}");
  ferrocell::api::SetResult r = ferrocell::api::applySet(doc, cur);
  CHECK(r.ok);
  CHECK_EQ(r.settings.ledCount(), (std::uint16_t)30);
  CHECK(!r.rebooting);  // identical value -> no-op, no reboot
}

static void test_apply_set_validation() {
  section("Api::applySet validation");
  Settings cur = Settings::defaults();

  // unknown field
  Value d1 = obj("{\"bogus\":1}");
  CHECK(!ferrocell::api::applySet(d1, cur).ok);

  // wrong type: hue must be a number
  Value d2 = obj("{\"hue\":\"red\"}");
  CHECK(!ferrocell::api::applySet(d2, cur).ok);

  // wrong type: mode must be a string
  Value d3 = obj("{\"mode\":5}");
  CHECK(!ferrocell::api::applySet(d3, cur).ok);

  // unknown mode
  Value d4 = obj("{\"mode\":\"disco\"}");
  CHECK(!ferrocell::api::applySet(d4, cur).ok);

  // ledCount must be an integer
  Value d5 = obj("{\"ledCount\":1.5}");
  CHECK(!ferrocell::api::applySet(d5, cur).ok);

  // body must be an object
  Value d6 = obj("[1,2,3]");
  CHECK(!ferrocell::api::applySet(d6, cur).ok);
}

// --- error / ok helpers -----------------------------------------------------
static void test_error_ok_json() {
  section("Api error/ok helpers");
  Value e = ferrocell::api::errorJson("boom");
  CHECK(e.getBool("ok", true) == false);
  CHECK(e.getString("error", "") == "boom");
  Value o = ferrocell::api::okJson();
  CHECK(o.getBool("ok", false));
}

int main() {
  std::printf("Ferrocell API tests (Json + Api) — M5\n");
  test_json_roundtrip();
  test_json_errors();
  test_json_numbers();
  test_state_json_shape();
  test_apply_set_partial();
  test_apply_set_all_fields();
  test_apply_set_ledcount_noop();
  test_apply_set_validation();
  test_error_ok_json();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures) {
    std::printf("FAILURES\n");
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
