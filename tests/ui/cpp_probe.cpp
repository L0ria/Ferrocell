// =============================================================================
//  Ferrocell — JS<->C++ cross-check probe (M6, issue #7).
//
//  A TINY, self-contained pure-core probe compiled + run by the Node harness
//  (tests/ui/harness.mjs) to prove the web UI's JS contract agrees with the
//  C++ core on the host. It has NO Arduino / FastLED / hardware dependencies
//  and links only the pure core (Color, Settings, Api, Json).
//
//  It prints a compact, machine-readable report (one `name<TAB>value` per
//  line) that the harness asserts against the equivalent JS helpers:
//    * hsvGrid   — Color(h,s,v).toRgb() over an HSV grid  (vs JS hsvToRgb)
//    * defaults  — Settings::defaults() fields            (vs JS parseState defaults)
//    * setOk     — api::applySet() result flags per case  (vs JS buildSetPayload)
//
//  Build (what the harness does):
//    g++ -std=c++17 -O1 -I ferrocell/src tests/ui/cpp_probe.cpp \
//        ferrocell/src/core/Color.cpp ferrocell/src/core/Settings.cpp \
//        ferrocell/src/core/Json.cpp ferrocell/src/core/Api.cpp -o cpp_probe
// =============================================================================
#include <cstdio>
#include <string>

#include "core/Api.h"
#include "core/Color.h"
#include "core/Json.h"
#include "core/Settings.h"

using ferrocell::Color;
using ferrocell::Settings;
using ferrocell::api::SetResult;
using ferrocell::api::applySet;
using ferrocell::json::Value;

// One line of the report: `name<TAB>value`.
static void line(const std::string& name, const std::string& value) {
  std::printf("%s\t%s\n", name.c_str(), value.c_str());
}

int main() {
  // --- 1. HSV -> RGB grid (vs JS pure.hsvToRgb) ---------------------------
  // h in {0,30,60,90,120,180,240,270,300,330}, s in {0,0.5,0.8,1}, v in {0,0.5,0.7,1}
  const double hs[] = {0, 30, 60, 90, 120, 180, 240, 270, 300, 330};
  const double ss[] = {0.0, 0.5, 0.8, 1.0};
  const double vs[] = {0.0, 0.5, 0.7, 1.0};
  for (double h : hs)
    for (double s : ss)
      for (double v : vs) {
        Color c(h, s, v);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%u,%u,%u",
                      static_cast<unsigned>(c.r()), static_cast<unsigned>(c.g()),
                      static_cast<unsigned>(c.b()));
        char key[48];
        std::snprintf(key, sizeof(key), "hsv\t%.1f,%.2f,%.2f", h, s, v);
        line(key, buf);
      }

  // --- 2. Settings defaults (vs JS parseState fallbacks) ------------------
  Settings d = Settings::defaults();
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%u,%.1f,%.2f,%.2f,%s,%.1f",
                static_cast<unsigned>(d.ledCount()), d.hue(), d.saturation(),
                d.brightness(), d.mode().c_str(), d.speed());
  line("defaults", buf);

  // --- 3. applySet result flags (vs JS buildSetPayload + knownSetFields) --
  // Each case: (label, json body) -> print ok|error|rebooting.
  struct Case {
    const char* label;
    const char* body;
  };
  const Case cases[] = {
      {"all", R"({"hue":120,"saturation":0.8,"brightness":0.7,"mode":"rainbow","speed":2,"ledCount":40})"},
      {"hue_only", R"({"hue":30})"},
      {"bad_mode", R"({"mode":"neon"})"},
      {"unknown_field", R"({"hue":30,"voltage":5})"},
      {"hue_string", R"({"hue":"red"})"},
      {"led_same", R"({"ledCount":30})"},   // 30 == defaults -> no reboot
      {"led_diff", R"({"ledCount":50})"},   // 50 != 30 -> reboot
      {"empty", R"({})"},
  };
  for (const Case& c : cases) {
    Value doc;
    std::string err;
    if (!ferrocell::json::parse(c.body, doc, &err)) {
      line(std::string("set\t") + c.label, "parse_error");
      continue;
    }
    SetResult r = applySet(doc, Settings::defaults());
    char out[64];
    std::snprintf(out, sizeof(out), "%s|%s|%s", r.ok ? "ok" : "err",
                  r.ok ? "" : r.error.c_str(), r.rebooting ? "reboot" : "");
    line(std::string("set\t") + c.label, out);
  }

  std::printf("PROBE_OK\n");
  return 0;
}
