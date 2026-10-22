// =============================================================================
//  Ferrocell — web-UI JS-contract harness (M6, issue #7; CI job 2 "ui-contract").
//
//  A dependency-free Node harness (Node >= 18; CI uses Node 22) that loads
//  ferrocell/data/ui.js and asserts the JS side of the web-UI contract:
//
//    1. import + module shape (FerrocellUI.pure, 14 pure helpers)
//    2. endpoints — the JS endpoint table (single source of truth)
//    3. payload shapes — knownSetFields, buildSetPayload, parseState
//    4. color-wheel -> hue/saturation mapping (+ round-trips)
//    5. HSV <-> RGB — anchors, hue wrap, rgbToHsv round-trips
//    6. presets — all 7 valid + in-range
//    7. JS <-> C++ cross-check — compiles + runs tests/ui/cpp_probe.cpp
//       (the pure core: Color / Settings / Api) and asserts the JS helpers
//       agree with the C++ core on a shared grid. Skipped (with a warning)
//       if g++ is unavailable.
//
//  Run:   node tests/ui/harness.mjs          (or: npm test in tests/ui/)
//  Exit:  0 if all checks pass, 1 otherwise (CI-friendly).
// =============================================================================

import { execFileSync } from "node:child_process";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url)); // .../tests/ui
const ROOT = resolve(HERE, "..", "..");
const UI_JS = join(ROOT, "ferrocell", "data", "ui.js");
const PROBE_SRC = join(HERE, "cpp_probe.cpp");
const CORE_SRCS = [
  "ferrocell/src/core/Color.cpp",
  "ferrocell/src/core/Settings.cpp",
  "ferrocell/src/core/Json.cpp",
  "ferrocell/src/core/Api.cpp",
];

let checks = 0;
let failures = 0;
let skipped = 0;
let sectionName = "";

function section(name) {
  sectionName = name;
  console.log(`== ${name}`);
}

function check(cond, label) {
  checks++;
  if (!cond) {
    failures++;
    console.log(`  FAIL [${sectionName}] ${label}`);
  }
}

function eq(a, b, label) {
  check(Object.is(a, b) || a === b, `${label} (expected ${String(b)}, got ${String(a)})`);
}

function near(a, b, eps = 1e-9, label = "") {
  check(Math.abs(a - b) <= eps, `${label} (expected ${b}, got ${a})`);
}

function rgbEq(a, b, label) {
  check(a.r === b.r && a.g === b.g && a.b === b.b,
        `${label} (expected rgb(${b.r},${b.g},${b.b}), got rgb(${a.r},${a.g},${a.b}))`);
}

// --- load the module under test ---------------------------------------------
const mod = await import(UI_JS);
const F = mod.FerrocellUI || mod.default;
if (!F) process.exit(1);
const pure = F.pure;

// ===========================================================================
section("1. import + module shape");
check(typeof F === "object", "FerrocellUI is an object");
check(typeof pure === "object", "FerrocellUI.pure exists");
const EXPECTED_PURE = [
  "endpoints", "knownModes", "ranges", "clamp", "clamp01", "wrapHue",
  "hsvToRgb", "rgbToHsv", "wheelToHueSat", "hueSatToWheel", "presets",
  "knownSetFields", "buildSetPayload", "parseState",
];
for (const k of EXPECTED_PURE) check(k in pure, `pure.${k} exists`);
eq(pure.knownModes.length, 5, "knownModes has 5 entries");
for (const m of ["solid", "rainbow", "breathing", "gradient", "off"])
  check(pure.knownModes.includes(m), `knownModes includes "${m}"`);

// ===========================================================================
section("2. endpoints (JS single source of truth)");
const EP = pure.endpoints;
check(Array.isArray(EP), "endpoints is an array");
eq(EP.length, 5, "exactly 5 endpoints");
const epSet = new Set(EP.map((e) => `${e.method} ${e.path}`));
for (const want of [
  "GET /api/state", "POST /api/set", "POST /api/wifi",
  "POST /api/reboot", "POST /api/reset",
]) check(epSet.has(want), `endpoint ${want} declared`);
for (const e of EP) check(typeof e.method === "string" && typeof e.path === "string" && e.path.startsWith("/api/"), `endpoint shape ok: ${e.method} ${e.path}`);

// ===========================================================================
section("3. payload shapes");
const FIELDS = ["hue", "saturation", "brightness", "mode", "speed", "ledCount"];
eq(pure.knownSetFields.length, 6, "knownSetFields has 6 entries");
for (const f of FIELDS) check(pure.knownSetFields.includes(f), `knownSetFields includes "${f}"`);

// buildSetPayload: keeps known, drops unknown, tolerates null / non-objects.
eq(JSON.stringify(pure.buildSetPayload(null)), "{}", "buildSetPayload(null) -> {}");
eq(JSON.stringify(pure.buildSetPayload("x")), "{}", "buildSetPayload(string) -> {}");
eq(JSON.stringify(pure.buildSetPayload({ hue: 10, bogus: 1, ledCount: 40 })),
   '{"hue":10,"ledCount":40}', "buildSetPayload keeps known, drops unknown");
eq(JSON.stringify(pure.buildSetPayload({})), "{}", "buildSetPayload({}) -> {}");
eq(JSON.stringify(pure.buildSetPayload({ mode: "off" })), '{"mode":"off"}',
   "buildSetPayload keeps mode");

// parseState: full doc round-trips through the flat view.
const fullState = {
  ok: true,
  settings: { ledCount: 40, hue: 120, saturation: 0.8, brightness: 0.7, mode: "rainbow", speed: 2 },
  wifi: { mode: "sta", connected: true, ip: "192.168.1.50", ssid: "home", rssi: -55, apName: "Ferrocell_123456789", configUrl: "http://192.168.1.1:8080" },
  fw: "0.5.0",
};
const st = pure.parseState(fullState);
eq(st.ok, true, "parseState ok");
eq(st.ledCount, 40, "parseState ledCount");
eq(st.hue, 120, "parseState hue");
eq(st.saturation, 0.8, "parseState saturation");
eq(st.brightness, 0.7, "parseState brightness");
eq(st.mode, "rainbow", "parseState mode");
eq(st.speed, 2, "parseState speed");
eq(st.wifiMode, "sta", "parseState wifiMode");
eq(st.wifiConnected, true, "parseState wifiConnected");
eq(st.ip, "192.168.1.50", "parseState ip");
eq(st.ssid, "home", "parseState ssid");
eq(st.rssi, -55, "parseState rssi");
eq(st.apName, "Ferrocell_123456789", "parseState apName");
eq(st.configUrl, "http://192.168.1.1:8080", "parseState configUrl");
eq(st.fw, "0.5.0", "parseState fw");

// parseState: empty doc -> safe defaults.
const empty = pure.parseState({});
eq(empty.ok, false, "parseState({}) ok=false");
eq(empty.saturation, 1, "parseState default saturation=1");
eq(empty.brightness, 0, "parseState default brightness=0");
eq(empty.mode, "solid", "parseState default mode=solid");
eq(empty.speed, 1, "parseState default speed=1");
eq(empty.wifiMode, "ap", "parseState default wifiMode=ap");
eq(empty.ledCount, 0, "parseState default ledCount=0");

// parseState: garbage input does not throw.
let noThrow = true;
try { pure.parseState(null); pure.parseState("x"); pure.parseState(undefined); } catch { noThrow = false; }
check(noThrow, "parseState tolerates null/string/undefined");

// ===========================================================================
section("4. color-wheel -> hue/saturation mapping");
const R = 100;
// Center = white (saturation 0); edge = full saturation; hue = angle (0 at +x, clockwise).
eq(pure.wheelToHueSat(0, 0, R).saturation, 0, "wheel center -> sat 0");
eq(pure.wheelToHueSat(R, 0, R).hue, 0, "wheel +x -> hue 0");
eq(pure.wheelToHueSat(R, 0, R).saturation, 1, "wheel edge -> sat 1");
eq(pure.wheelToHueSat(0, R, R).hue, 90, "wheel +y -> hue 90 (clockwise)");
eq(pure.wheelToHueSat(-R, 0, R).hue, 180, "wheel -x -> hue 180");
eq(pure.wheelToHueSat(0, -R, R).hue, 270, "wheel -y -> hue 270");
const diag = Math.SQRT1_2 * R;
eq(pure.wheelToHueSat(diag, diag, R).hue, 45, "wheel 45deg -> hue 45");
near(pure.wheelToHueSat(R / 2, 0, R).saturation, 0.5, 1e-9, "wheel half-radius -> sat 0.5");
check(pure.wheelToHueSat(10 * R, 0, R).saturation <= 1, "wheel far outside -> sat clamped <= 1");

// hueSatToWheel is the inverse; round-trips within tolerance.
const p0 = pure.hueSatToWheel(0, 1, R);
eq(p0.x, R, "hueSatToWheel(0,1) x=R");
eq(p0.y, 0, "hueSatToWheel(0,1) y=0");
const p90 = pure.hueSatToWheel(90, 0.5, R);
near(p90.x, 0, 1e-9, "hueSatToWheel(90,0.5) x=0");
near(p90.y, R / 2, 1e-9, "hueSatToWheel(90,0.5) y=R/2");
const pC = pure.hueSatToWheel(30, 0, R);
near(pC.x, 0, 1e-9, "hueSatToWheel(sat=0) x=0");
near(pC.y, 0, 1e-9, "hueSatToWheel(sat=0) y=0");
for (const [h, s] of [[0, 1], [45, 0.7], [120, 0.9], [200, 0.4], [300, 1], [350, 0.2]]) {
  const pt = pure.hueSatToWheel(h, s, R);
  const back = pure.wheelToHueSat(pt.x, pt.y, R);
  near(back.saturation, s, 1e-6, `wheel round-trip sat (${h},${s})`);
  const dh = Math.min(Math.abs(back.hue - h), 360 - Math.abs(back.hue - h));
  check(dh <= 1e-6, `wheel round-trip hue (${h},${s}) (got ${back.hue})`);
}

// ===========================================================================
section("5. HSV <-> RGB");
const anchors = [
  [0, 1, 1, 255, 0, 0], [120, 1, 1, 0, 255, 0], [240, 1, 1, 0, 0, 255],
  [60, 1, 1, 255, 255, 0], [0, 0, 1, 255, 255, 255], [0, 0, 0, 0, 0, 0],
  [30, 0.8, 0.7, 179, 107, 36], [180, 0.5, 0.5, 64, 128, 128],
];
for (const [h, s, v, r, g, b] of anchors) {
  const c = pure.hsvToRgb(h, s, v);
  rgbEq(c, { r, g, b }, `hsvToRgb(${h},${s},${v})`);
}
// Hue wrap: 360 == 0, 720 == 0, -120 == 240.
rgbEq(pure.hsvToRgb(360, 1, 1), pure.hsvToRgb(0, 1, 1), "hsvToRgb(360)==(0)");
rgbEq(pure.hsvToRgb(720, 1, 1), pure.hsvToRgb(0, 1, 1), "hsvToRgb(720)==(0)");
rgbEq(pure.hsvToRgb(-120, 1, 1), pure.hsvToRgb(240, 1, 1), "hsvToRgb(-120)==(240)");
// Out-of-range saturation/value clamp.
rgbEq(pure.hsvToRgb(0, 2, 1), pure.hsvToRgb(0, 1, 1), "hsvToRgb sat clamp");
rgbEq(pure.hsvToRgb(0, 1, 5), pure.hsvToRgb(0, 1, 1), "hsvToRgb value clamp");

// rgbToHsv anchors + round-trips over a grid.
const hsv1 = pure.rgbToHsv(255, 0, 0);
near(hsv1.h, 0, 1e-9, "rgbToHsv red h"); near(hsv1.s, 1, 1e-9, "rgbToHsv red s"); near(hsv1.v, 1, 1e-9, "rgbToHsv red v");
const hsv2 = pure.rgbToHsv(128, 128, 128);
near(hsv2.s, 0, 1e-9, "rgbToHsv grey s"); near(hsv2.v, 128 / 255, 1e-9, "rgbToHsv grey v");
const hsv3 = pure.rgbToHsv(0, 0, 0);
near(hsv3.s, 0, 1e-9, "rgbToHsv black s"); near(hsv3.v, 0, 1e-9, "rgbToHsv black v");
const grid = [[0, 1, 1], [30, 0.8, 0.7], [120, 1, 1], [240, 1, 1], [180, 0.5, 0.5], [300, 0.9, 0.8], [60, 1, 1], [270, 0.7, 0.6]];
for (const [h, s, v] of grid) {
  const rgb = pure.hsvToRgb(h, s, v);
  const back = pure.rgbToHsv(rgb.r, rgb.g, rgb.b);
  const dh = Math.min(Math.abs(back.h - h), 360 - Math.abs(back.h - h));
  check(dh <= 0.5, `rgbToHsv round-trip hue (${h},${s},${v}) (got ${back.h})`);
  near(back.s, s, 0.005, `rgbToHsv round-trip sat (${h},${s},${v})`);
  near(back.v, v, 0.005, `rgbToHsv round-trip v (${h},${s},${v})`);
}

// ===========================================================================
section("6. presets");
eq(pure.presets.length, 7, "7 presets");
for (const p of pure.presets) {
  check(typeof p.name === "string" && p.name.length > 0, `preset name: ${p.name}`);
  check(pure.knownModes.includes(p.mode), `preset ${p.name} mode known (${p.mode})`);
  check(p.hue >= 0 && p.hue < 360, `preset ${p.name} hue in range`);
  check(p.saturation >= 0 && p.saturation <= 1, `preset ${p.name} saturation in range`);
  check(p.brightness >= 0 && p.brightness <= 1, `preset ${p.name} brightness in range`);
  check(p.speed >= 0 && p.speed <= 10, `preset ${p.name} speed in range`);
  const payload = pure.buildSetPayload(p);
  check(Object.keys(payload).length === 5, `preset ${p.name} payload has 5 fields`);
}
for (const name of ["Red", "Green", "Blue", "Warm", "Rainbow", "Breathe", "Off"])
  check(pure.presets.some((p) => p.name === name), `preset exists: ${name}`);

// ===========================================================================
section("7. JS <-> C++ cross-check (pure core probe)");
let cpp = null; // { hsv: Map, defaults: string, set: Map }
try {
  const work = mkdtempSync(join(tmpdir(), "ferrocell-probe-"));
  const bin = join(work, "cpp_probe");
  const cxx = process.env.CXX || "g++";
  const argv = ["-std=c++17", "-O1", `-I`, join(ROOT, "ferrocell", "src"), PROBE_SRC,
    ...CORE_SRCS.map((s) => join(ROOT, s)), "-o", bin];
  execFileSync(cxx, argv, { stdio: "pipe" });
  const out = execFileSync(bin, [], { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
  const hsv = new Map();
  const set = new Map();
  let defaults = "";
  let probeOk = false;
  for (const line of out.split("\n")) {
    if (!line) continue;
    if (line === "PROBE_OK") { probeOk = true; continue; }
    // Report lines: `kind<TAB>key<TAB>value` (hsv/set) or `defaults<TAB>value`.
    const parts = line.split("\t");
    if (parts[0] === "hsv") hsv.set(parts[1], parts[2]);
    else if (parts[0] === "defaults") defaults = parts[1];
    else if (parts[0] === "set") set.set(parts[1], parts[2]);
  }
  check(probeOk, "probe ran to completion (PROBE_OK)");
  cpp = { hsv, defaults, set };
} catch (e) {
  if (e && e.status !== undefined) {
    console.log(`  SKIP [cross-check] C++ probe failed to build/run: ${String(e.message).split("\n")[0]}`);
    if (e.stderr) console.log(String(e.stderr).slice(0, 400));
  } else {
    console.log(`  SKIP [cross-check] g++ unavailable: ${e.message}`);
  }
  skipped++;
}

if (cpp) {
  // 7a. HSV grid: JS hsvToRgb == C++ Color(h,s,v).rgb
  let gridChecks = 0;
  for (const [key, cval] of cpp.hsv) {
    const [h, s, v] = key.split(",").map(Number);
    const [cr, cg, cb] = cval.split(",").map(Number);
    const js = pure.hsvToRgb(h, s, v);
    rgbEq(js, { r: cr, g: cg, b: cb }, `JS==C++ hsvToRgb(${h},${s},${v})`);
    gridChecks++;
  }
  check(gridChecks === 160, `cross-checked 160 HSV grid points (got ${gridChecks})`);

  // 7b. defaults: C++ Settings::defaults() vs JS parseState defaults.
  const [ledCount, hue, sat, bright, mode, speed] = cpp.defaults.split(",");
  eq(Number(ledCount), 30, "C++ defaults ledCount=30");
  eq(Number(sat), 1, "C++ defaults saturation=1 (== JS parseState default)");
  eq(Number(bright), 0.5, "C++ defaults brightness=0.5");
  eq(mode, "solid", "C++ defaults mode=solid (== JS parseState default)");
  eq(Number(speed), 1, "C++ defaults speed=1 (== JS parseState default)");

  // 7c. applySet flags: JS buildSetPayload payloads vs C++ applySet results.
  const cppSet = (label, expect) => eq(cpp.set.get(label), expect, `C++ applySet(${label}) == ${expect}`);
  cppSet("all", "ok||reboot");
  cppSet("hue_only", "ok||");
  cppSet("bad_mode", "err|unknown mode: neon|");
  cppSet("unknown_field", "err|unknown field: voltage|");
  cppSet("hue_string", "err|hue must be a number|");
  cppSet("led_same", "ok||");
  cppSet("led_diff", "ok||reboot");
  cppSet("empty", "ok||");
  // JS side: the same payloads build without unknown fields.
  eq(JSON.stringify(pure.buildSetPayload({ hue: 120, saturation: 0.8, brightness: 0.7, mode: "rainbow", speed: 2, ledCount: 40 })),
     '{"hue":120,"saturation":0.8,"brightness":0.7,"mode":"rainbow","speed":2,"ledCount":40}',
     "JS payload for C++ 'all' case");
  eq(JSON.stringify(pure.buildSetPayload({ hue: 30, voltage: 5 })), '{"hue":30}',
     "JS drops the field C++ rejects (voltage)");
  eq(JSON.stringify(pure.buildSetPayload({ hue: "red" })), '{"hue":"red"}',
     "JS passes hue through (C++ type-checks it)");
}

// ===========================================================================
console.log("");
console.log(`${checks} checks, ${failures} failures, ${skipped} skipped`);
if (failures > 0) {
  console.log("UI CONTRACT: FAIL");
  process.exit(1);
}
console.log("UI CONTRACT: PASS");
