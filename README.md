# Ferrocell

Firmware for a Waveshare **ESP32-S3-Zero** that drives a **WS2812B** LED strip
in a ferrocell (a device that visualizes magnetic fields). The ESP32 exists
solely to control the strip's color so the field visualization can be changed
at runtime.

> Status: **M1–M6 complete.** M6 — test harness + full CI (3 jobs) + final
> docs (issue #7) on top of the M5 web UI + JSON API (issue #6), the M4 WiFi
> layer (issue #5), the M3 hardware layer (issue #4), the M2 pure core
> (issue #3) and the M1 scaffold (issue #2). The firmware boots over USB-CDC,
> loads settings (NVS; sane defaults on first boot), attaches the WS2812B
> strip on GPIO7 via FastLED, renders frames from the `ColorEngine`, brings
> up the WiFi layer (`WifiManager`: AP config page or STA with
> auto-reconnect), and serves the self-contained web UI + JSON API
> (`UiServer`) on port 80.

## Hardware

| Item | Detail |
|---|---|
| Board | Waveshare ESP32-S3-Zero (ESP32-S3, 8 MB flash) |
| LED strip | external **WS2812B**, data on **GPIO7** (5 V rail + level shifter) |
| Onboard WS2812 | on GPIO21 — **intentionally not used** |
| Serial | USB-CDC (board default `USBMode=hwcdc`) |

## Build & flash

Everything is driven by `arduino-cli` through a single idempotent script — no
Arduino IDE required.

```bash
./build.sh                 # build only  ->  build/ferrocell.ino.bin
./build.sh --upload        # build + flash to the first ESP32 on serial
./build.sh --port /dev/ttyACM0   # flash to a specific port
./build.sh --clean         # wipe build/ + the global sketch cache first
./build.sh --fqbn esp32:esp32:esp32s3   # build for a different board/chip
FQBN=esp32:esp32:esp32s3 ./build.sh    # same, via env var
```

### Flash (no rebuild)

`flash.sh` uploads the image `build.sh` already exported — no recompile — so it
is the quick tool for re-flashing and for baud/chip experiments (the S3-Zero
can brownout from the 5 V rail at higher flash speeds, issue #19):

```bash
./flash.sh                            # flash build/ to the first ESP32 @ 115200
./flash.sh --port /dev/ttyACM0        # specific port
./flash.sh --speed 921600             # faster link (if the rail can take it)
./flash.sh --fqbn esp32:esp32:esp32s3 # different chip/board
./flash.sh --erase                    # erase all flash (factory reset)
```

Parameters (flag > env > default): `--port`/`PORT` (default: auto-detect the
first ESP32/tty), `--speed`/`SPEED` (default **115200** — brownout-safe),
`--fqbn`/`FQBN` (default: the S3-Zero FQBN above). `--erase` is a simple
`arduino-cli board erase` — it needs no image. Without an image, `flash.sh`
tells you to run `./build.sh` first.


`build.sh` is self-contained: it locates (or installs) arduino-cli, registers
the esp32 board index, installs the **pinned core**, installs the **pinned
FastLED**, and compiles. Re-running is a no-op for the installs.

### Pins (single source of truth)

| What | Value |
|---|---|
| Core | `esp32:esp32` **3.3.12** |
| FQBN | `esp32:esp32:waveshare_esp32_s3_zero:PartitionScheme=otanofs` (default — override with `--fqbn` / `FQBN` in `build.sh`/`flash.sh`) |
| Partition | `otanofs` — two **1.94 MiB** OTA slots, no SPIFFS (issue #16; we mount no runtime FS) |
| FastLED | **3.10.6** (pinned down: 3.10.7+/3.10.8 have a RISC-V C3/C6 compile regression — issue #23) |
| ElegantOTA | **4.0.0** (OTA over WiFi, issue #16) |
| ESP-Wifi-Config | **2.2.6** (vendored in `lib/ESP-Wifi-Config`) |
| AP IP | `192.168.1.1` (library default) — config page at `http://192.168.1.1:8080/` |

The same pins are recorded in `ferrocell/sketch.yaml` (IDE 2.x) and
`libraries.json` (lib pins).

## Usage

1. **Build + flash** (one machine, USB): `./build.sh --upload` (or
   `--port /dev/ttyACM0`).
2. **First boot — AP mode.** With no saved WiFi the device starts an access
   point `Ferrocell_<mac>`. Join it and open **`http://192.168.1.1/`** — the
   web UI (color wheel, sliders, modes, presets, LED count, WiFi tab).
   Save a network from the WiFi tab (or use the config page at
   `http://192.168.1.1:8080/`, login **admin / pass_ESP**).
3. **After saving — STA mode.** The device reboots, connects to your network
   (auto-reconnects on drop), and the UI is reachable at **`http://<ip>/`**.
   The status bar shows the mode + IP.
4. **Reset** (UI → WiFi tab, or `POST /api/reset`) wipes the saved
   credentials and reboots back into AP mode.
5. **Update over WiFi (OTA, issue #16).** Open **`http://<ip>/update`** (STA)
   or **`http://192.168.1.1/update`** (AP) — or the UI's **Update** tab — and
   upload the application image (`build/ferrocell.ino.bin`, *not* the
   `.merged.bin`). The device reboots into the new firmware a moment after a
   successful upload; the saved WiFi config is kept (it lives in NVS, which
   OTA never touches).

## CI (4 jobs, no hardware)

`.github/workflows/build.yml` runs on every PR (and on `main`):

| Job | What it does | Local command |
|---|---|---|
| `core-tests` | **Unit (host)** — builds + runs the pure-core unit tests (`tests/unit/*.cpp`: `Color`, `ColorEngine`, `Settings`, `Json`, `Api`) with `g++ -std=c++17` | `bash tests/run_tests.sh` |
| `ui-contract` | **UI contract (Node)** — runs the dependency-free harness against `ferrocell/data/ui.js` (endpoints, payload shapes, wheel mapping, HSV<->RGB, presets) **plus the JS<->C++ cross-check** (compiles + runs the pure-core probe) | `node tests/ui/harness.mjs` |
| `firmware-build` | **Firmware build** — the real `./build.sh` (arduino-cli + core 3.3.12 + FastLED 3.10.6 + ElegantOTA 4.0.0 + vendored ESP-Wifi-Config) for the S3-Zero FQBN (`PartitionScheme=otanofs`) | `./build.sh` |
| `firmware-build-c3` | **Firmware build (RISC-V, issue #23)** — the same sketch for an ESP32-C3 (`nologo_esp32c3_super_mini`, `PartitionScheme=no_ota`); catches the C3/C6-class failures (FastLED RISC-V regression, stale-library versions) that only surface on a non-default chip | `FQBN=esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=no_ota ./build.sh` |

A PR that breaks `Color` / `ColorEngine` fails `core-tests` (and the
cross-check); one that breaks `ui.js` fails `ui-contract`; one that breaks
the S3 build fails `firmware-build`; one that breaks the RISC-V (C3/C6)
build path fails `firmware-build-c3`.

## Core (pure, host-testable)

The lighting logic lives in a **pure C++ core with no Arduino / FastLED /
hardware dependencies**, implemented as classes (object-oriented, modular):

| Class | File | Responsibility |
|---|---|---|
| `ferrocell::Color` | `ferrocell/src/core/Color.{h,cpp}` | HSV value type; `toRgb()` (HSV→RGB), clamping, small helpers |
| `ferrocell::ColorEngine` | `ferrocell/src/core/ColorEngine.{h,cpp}` | Pure per-LED color generator for modes `SOLID` / `RAINBOW` / `BREATHING` / `GRADIENT` / `OFF`; time is **injected** (`timeMs`) so it is deterministic |
| `ferrocell::Settings` | `ferrocell/src/core/Settings.{h,cpp}` | User-config value type (ledCount, hue, saturation, brightness, mode, speed); `defaults()` + `normalize()` (clamping/wrapping) |
| `ferrocell::json` | `ferrocell/src/core/Json.{h,cpp}` | Minimal dependency-free JSON value + parser + serializer (the wire format for the API) |
| `ferrocell::api` | `ferrocell/src/core/Api.{h,cpp}` | The JSON API contract: `stateJson()` (GET /api/state) + `applySet()` (POST /api/set) — the single place the shapes are defined |

Because the core has zero hardware dependencies it compiles and runs on the
host with plain `g++ -std=c++17`, which is what the unit tests target (the
`api_test` target asserts the exact JSON shapes the web UI + the C++ core
agree on — the stable contract for the M6 Node harness).

### Hardware layer (M3, firmware-only)

These classes wrap the hardware and are compiled into the firmware (not part
of the host test harness). They delegate all color math to the pure core.

| Class | File | Responsibility |
|---|---|---|
| `LedController` | `ferrocell/src/hw/LedController.{h,cpp}` | The only class that talks to FastLED; owns the WS2812B strip on GPIO7 + the LED count; `render()` delegates per-LED colors to `ColorEngine` (no color math here) |
| `ConfigStore` | `ferrocell/src/hw/ConfigStore.{h,cpp}` | The single persistence point over `Preferences` (NVS); `load()` (sane defaults on first boot), `save()`, `setLedCount()` (save + `ESP.restart()`), `reset()` |
| `WifiManager` | `ferrocell/src/net/WifiManager.{h,cpp}` | The only class that talks to WiFi; wraps `ESP-Wifi-Config` (AP config page + STA auto-reconnect); exposes mode/IP/SSID/RSSI/AP name + `scan()` + `save()` + reset (M4/M5) |
| `UiServer` | `ferrocell/src/net/UiServer.{h,cpp}` | The only class that talks to `WebServer`; serves the embedded web UI + the JSON API on port 80 (M5) |

`ferrocell.ino` stays the thin `setup()`/`loop()` entrypoint (composition
root): it loads settings, attaches the strip, boots WiFi + the web UI, and
renders frames.

### WiFi (M4, firmware-only)

`WifiManager` (`ferrocell/src/net/WifiManager.{h,cpp}`) is the **only** class
that talks to WiFi — it wraps the vendored `tabahi/ESP-Wifi-Config` (v2.2.6)
and the application never calls `WiFi.*` directly.

| What | Value |
|---|---|
| AP IP | `192.168.1.1` (library default) |
| AP SSID | `Ferrocell_<9-digit-mac>` (unique per device; the lib appends the MAC) |
| Config page | `http://192.168.1.1:8080/` (port **8080** — root `80` stays free for the M5 web UI) |
| Config-page login | **admin / pass_ESP** (library defaults) |
| STA | saved network in range → connect + **auto-reconnect** (10 s interval) |

**How it works:**
- **No saved WiFi (or the saved one is out of range)** → the device starts an
  access point. Join `Ferrocell_<mac>`, open `http://192.168.1.1:8080/`
  (login **admin / pass_ESP**), pick the network (or type an unbroadcast SSID),
  and hit **Save**. Saving persists the credentials to flash and **reboots**
  the device into STA mode.
- **Saved WiFi in range** → the device connects in STA mode and auto-reconnects
  if the link drops. The device gets an IP; the (M5) web UI will be reachable
  at `http://<ip>/`.
- The config page stays up **perpetually** (in both modes) so the device is
  always re-configurable.
- **Reset:** `WifiManager::resetConfig()` wipes the saved credentials and
  reboots the device back into AP mode.

`WifiManager` is a thin wrapper (little pure logic) and is compiled into the
firmware only — verified by the `firmware-build` CI job, consistent with the
other hw-layer classes.

### Web UI + JSON API (M5, firmware-only + pure core)

`UiServer` (`ferrocell/src/net/UiServer.{h,cpp}`) is the **only** class that
talks to `WebServer`. It serves the self-contained web UI (embedded in PROGMEM)
and the JSON API on **port 80** in both AP and STA mode:

| What | Value |
|---|---|
| UI (STA) | `http://<ip>/` |
| UI (AP) | `http://192.168.1.1/` |
| Config page (fallback) | `http://192.168.1.1:8080/` (M4, login admin / pass_ESP) |

**The JSON API** (the contract is defined in the pure core, `core/Api.{h,cpp}`,
and host-tested by `api_test`):

| Endpoint | Method | Body | Response |
|---|---|---|---|
| `/api/state` | GET | — | `{ ok, settings:{ledCount,hue,saturation,brightness,mode,speed}, wifi:{mode,connected,ip,ssid,rssi,apName,configUrl}, fw }` |
| `/api/set` | POST | any of `{hue,saturation,brightness,mode,speed,ledCount}` | `{ ok, settings:{…}, rebooting? }` — partial update; `ledCount` → save + reboot **only when changed** |
| `/api/wifi` | POST | `{action:"scan"}` | `{ ok, networks:[{ssid,rssi,locked}] }` |
| `/api/wifi` | POST | `{action:"save",ssid,password}` | `{ ok, rebooting:true }` |
| `/api/reboot` | POST | — | `{ ok, rebooting:true }` |
| `/api/reset` | POST | — | `{ ok, rebooting:true }` (wipes WiFi credentials) |

**The web UI** (`ferrocell/data/index.html` + `ui.css` + `ui.js`) is
mobile-first, dark, thumb-friendly, and self-contained (no CDN). It has a
**color wheel** (hue + saturation), brightness + speed sliders, mode chips
(Solid / Rainbow / Breathing / Gradient / Off), one-tap presets, an LED-count
stepper (saves + reboots), and a **WiFi tab** (scan / save / reboot / reset).
The pure helpers in `ui.js` (`FerrocellUI.pure`) are importable by Node so the
M6 harness can assert the JS↔C++ contract.

**Responsiveness (issue #14):** dragging the color wheel / sliders used to
fire one `POST /api/set` per pointer event (60-120/sec), and the server
persisted to NVS (flash) on every one of them — the UI fell behind the
pointer. The fix (contract-compatible, 3 parts):

- **UI** — `pushSet()` now does an *optimistic* local update (knob / sliders /
  readouts track the pointer 1:1 instantly) and sends the `/api/set`
  **debounced** (one request per drag, ~120 ms after the pointer goes quiet;
  `ledCount` stays immediate because it reboots). The policy is the pure,
  host-tested `pure.debounceMs(field)` (M6 harness section 8).
- **Server** — `/api/set` applies settings in RAM immediately (the strip
  updates at once) and marks a save as *pending*; the NVS write is flushed by
  `handle()` only once the settings are quiescent (≥500 ms). One drag = one
  flash save. A `ledCount` change still saves + reboots immediately.
- **Render loop** — `LedController::showIfChanged()` dirty-check: the
  blocking `FastLED.show()` only fires when the frame actually differs, so
  static modes no longer drive the strip 100x/sec and starve the web server.

**Build note:** the assets are embedded into PROGMEM by
`tools/make_ui_assets.py` (→ `ferrocell/src/net/ui_assets.h`, committed).
Re-run it after editing `ferrocell/data/`, then `./build.sh`.

`UiServer` is compiled into the firmware only (verified by `firmware-build`);
the JSON logic it delegates to is host-tested.

### OTA over WiFi (issue #16, firmware-only)

`ElegantOTA` (v4.0.0) is attached to the **same** `WebServer` the UI uses
(via `UiServer::rawServer()`), so its `/update` portal + `/ota/*` routes share
**port 80** in **both** AP and STA mode — no new port, no collision with the
M4 config page on 8080. `UiServer` remains the only class that owns the
server; the entrypoint just hands `rawServer()` to the library.

| What | Value |
|---|---|
| Portal | `http://<ip>/update` (STA) · `http://192.168.1.1/update` (AP) — also the UI's **Update** tab |
| Routes | `GET /update`, `GET /ota/metadata`, `GET /ota/start`, `POST /ota/upload` |
| Auth | open for v1 (local network; HTTP Basic auth is issue #17) |
| Auto-reboot | on — the device reboots into the new app ~2 s after a successful upload |
| Config retention | the saved WiFi config lives in **NVS**, which OTA never touches — it survives every update |
| Partition | `PartitionScheme=otanofs` → two 1.94 MiB OTA slots (≈836 KB headroom each); we mount no filesystem at runtime, so the `default` scheme's 1.5 MB SPIFFS was dead weight |

**One-time note (issue #16, A4):** the first firmware under `otanofs` must be
flashed **over USB** (`./build.sh --upload`) — a USB flash writes `app0`/factory
and replaces the old `default` layout. Every update after that can be OTA.

`ElegantOTA` is compiled into the firmware only (verified by `firmware-build`);
its route table is asserted by the `ui-contract` harness (section 9).

### Running the tests (no Arduino, no hardware)

```bash
tests/run_tests.sh          # job 1 — build + run all pure-core unit tests
                            #   (tests/unit/core_test: Color + ColorEngine;
                            #    tests/unit/settings_test: defaults/normalize;
                            #    tests/unit/api_test: Json + Api contract)
node tests/ui/harness.mjs   # job 2 — web-UI JS-contract harness (Node,
                            #   zero deps) incl. the JS<->C++ cross-check
```

These are the same commands the `core-tests` and `ui-contract` CI jobs run on
every PR.

## Repository layout

```
├── build.sh              # idempotent arduino-cli build (single source of truth; --fqbn / FQBN to override)
├── flash.sh              # flash-only: upload build/ (no recompile) — --port/--speed/--fqbn/--erase
├── libraries.json        # external library pins (FastLED, ElegantOTA, ESP-Wifi-Config)
├── ferrocell/
│   ├── ferrocell.ino     # thin setup()/loop() entrypoint (M1 boot sketch)
│   ├── sketch.yaml       # IDE 2.x project record (FQBN + core + lib pins)
│   ├── data/             # web UI source of truth (M5): index.html + ui.css + ui.js
│   └── src/
│       ├── core/         # pure, host-testable core (no Arduino/FastLED)
│       │   ├── Color.{h,cpp}
│       │   ├── ColorEngine.{h,cpp}
│       │   ├── Settings.{h,cpp}
│       │   ├── Json.{h,cpp}    # JSON value + parser + serializer (M5)
│       │   └── Api.{h,cpp}     # the JSON API contract (M5)
│       ├── hw/           # hardware wrappers (firmware-only)
│       │   ├── LedController.{h,cpp}
│       │   └── ConfigStore.{h,cpp}
│       └── net/          # network layer (firmware-only)
│           ├── WifiManager.{h,cpp}   # WiFi AP + STA (M4) + scan/save (M5)
│           ├── UiServer.{h,cpp}     # web UI + JSON API, port 80 (M5)
│           └── ui_assets.h          # GENERATED: data/ embedded in PROGMEM
├── lib/
│   └── ESP-Wifi-Config/  # vendored @ v2.2.6 (tabahi/ESP-Wifi-Config)
├── tools/
│   └── make_ui_assets.py # regenerates ui_assets.h from ferrocell/data/ (M5)
├── tests/
│   ├── unit/             # pure-core unit tests (job 1 "Unit (host)")
│   │   ├── core_test.cpp     # Color + ColorEngine (M2)
│   │   ├── settings_test.cpp # Settings defaults + normalize/clamping (M3)
│   │   └── api_test.cpp      # Json + Api — the JSON API contract (M5)
│   ├── ui/               # web-UI JS-contract harness (job 2 "UI contract")
│   │   ├── harness.mjs       # Node, zero deps (M6) — incl. JS<->C++ cross-check
│   │   ├── cpp_probe.cpp     # tiny pure-core probe the harness compiles/runs
│   │   └── package.json      # metadata only (type: module; npm test)
│   ├── run_tests.sh      # host test runner (g++ -std=c++17, no Arduino)
│   └── README.md         # how to run both harnesses
└── .github/workflows/
    └── build.yml         # CI (3 jobs): core-tests + ui-contract + firmware-build
```

## Design notes

- **Modular & object-oriented.** The application is a class-based composition —
  `Color`, `ColorEngine`, `LedController`, `ConfigStore`, `WifiManager`,
  `UiServer` (M2–M5) — not a bag of free functions. `ferrocell.ino` stays the
  thin `setup()`/`loop()` entrypoint.
- **One GPIO for the strip.** GPIO7 is free of strapping pins and the Octal
  PSRAM range (GPIO33–37), so it is the clean choice for the external WS2812B
  data line.
- **Reproducible builds.** Core and libraries are pinned; the build is
  idempotent and runs unchanged in CI (`.github/workflows/build.yml`).
- **3-job CI, no hardware.** `core-tests` (unit, host g++), `ui-contract`
  (Node JS-contract harness + JS<->C++ cross-check), `firmware-build`
  (arduino-cli) — see "CI" above.
- **Responsive UI (issue #14).** Dragging the wheel / sliders updates the UI
  optimistically and coalesces the network + flash-write into one request per
  drag (debounced `/api/set` + deferred NVS save + a `showIfChanged()` dirty
  check that skips the blocking `FastLED.show()` on unchanged frames).
- **OTA over WiFi (issue #16).** `ElegantOTA` (4.0.0) shares the UI's
  `WebServer` (port 80, STA + AP) for the `/update` portal; `otanofs` gives two
  1.94 MiB OTA slots; the saved WiFi config (NVS) is untouched by an update.

## Roadmap

| Milestone | Issue | Scope |
|---|---|---|
| M1 | #2 | Scaffold, build system, project config (this PR) |
| M2 | #3 | Pure core: `Color` + `ColorEngine` (host-testable, no Arduino deps) |
| M3 | #4 | Hardware layer: `LedController` + `ConfigStore` (this PR) |
| M4 | #5 | WiFi: `WifiManager` (AP + STA) |
| M5 | #6 | Web UI (color wheel) + JSON API: `UiServer` |
| M6 | #7 | Test harness + full CI (3 jobs) + final docs (this PR) — **done** |
| — | #16 | OTA over WiFi (`ElegantOTA` 4.0.0 + `otanofs` partition scheme) — **done** |
