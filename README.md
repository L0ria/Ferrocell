# Ferrocell

Firmware for a Waveshare **ESP32-S3-Zero** that drives a **WS2812B** LED strip
in a ferrocell (a device that visualizes magnetic fields). The ESP32 exists
solely to control the strip's color so the field visualization can be changed
at runtime.

> Status: **M4 — WiFi layer** (issue #5) on top of the M3 hardware layer
> (issue #4), the M2 pure core (issue #3) and the M1 scaffold (issue #2).
> The firmware boots over USB-CDC, loads settings (NVS; sane defaults on
> first boot), attaches the WS2812B strip on GPIO7 via FastLED, renders
> frames from the `ColorEngine`, and brings up the WiFi layer (`WifiManager`):
> an AP config page or a STA connection with auto-reconnect. The web UI
> (`UiServer`) lands in M5 (issue #6); the full test harness + CI in M6
> (issue #7).

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
```

`build.sh` is self-contained: it locates (or installs) arduino-cli, registers
the esp32 board index, installs the **pinned core**, installs the **pinned
FastLED**, and compiles. Re-running is a no-op for the installs.

### Pins (single source of truth)

| What | Value |
|---|---|
| Core | `esp32:esp32` **3.3.12** |
| FQBN | `esp32:esp32:waveshare_esp32_s3_zero` |
| FastLED | **3.10.8** |
| ESP-Wifi-Config | **2.2.6** (vendored in `lib/ESP-Wifi-Config`) |
| AP IP | `192.168.1.1` (library default) — config page at `http://192.168.1.1:8080/` |

The same pins are recorded in `ferrocell/sketch.yaml` (IDE 2.x) and
`libraries.json` (lib pins).

## Core (pure, host-testable)

The lighting logic lives in a **pure C++ core with no Arduino / FastLED /
hardware dependencies**, implemented as classes (object-oriented, modular):

| Class | File | Responsibility |
|---|---|---|
| `ferrocell::Color` | `ferrocell/src/core/Color.{h,cpp}` | HSV value type; `toRgb()` (HSV→RGB), clamping, small helpers |
| `ferrocell::ColorEngine` | `ferrocell/src/core/ColorEngine.{h,cpp}` | Pure per-LED color generator for modes `SOLID` / `RAINBOW` / `BREATHING` / `GRADIENT` / `OFF`; time is **injected** (`timeMs`) so it is deterministic |
| `ferrocell::Settings` | `ferrocell/src/core/Settings.{h,cpp}` | User-config value type (ledCount, hue, brightness, mode, speed); `defaults()` + `normalize()` (clamping/wrapping) |

Because the core has zero hardware dependencies it compiles and runs on the
host with plain `g++ -std=c++17`, which is what the unit tests target.

### Hardware layer (M3, firmware-only)

These classes wrap the hardware and are compiled into the firmware (not part
of the host test harness). They delegate all color math to the pure core.

| Class | File | Responsibility |
|---|---|---|
| `LedController` | `ferrocell/src/hw/LedController.{h,cpp}` | The only class that talks to FastLED; owns the WS2812B strip on GPIO7 + the LED count; `render()` delegates per-LED colors to `ColorEngine` (no color math here) |
| `ConfigStore` | `ferrocell/src/hw/ConfigStore.{h,cpp}` | The single persistence point over `Preferences` (NVS); `load()` (sane defaults on first boot), `save()`, `setLedCount()` (save + `ESP.restart()`), `reset()` |
| `WifiManager` | `ferrocell/src/net/WifiManager.{h,cpp}` | The only class that talks to WiFi; wraps `ESP-Wifi-Config` (AP config page + STA auto-reconnect); exposes mode/IP/SSID/RSSI/AP name + reset (M4) |

`ferrocell.ino` stays the thin `setup()`/`loop()` entrypoint (composition
root): it loads settings, attaches the strip, and renders frames.

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

### Running the core tests (no Arduino, no hardware)

```bash
tests/run_tests.sh          # build + run all pure-core unit tests
                            #   (core_test: Color + ColorEngine;
                            #    settings_test: Settings defaults/normalize)
```

This is the same command the `core-tests` CI job runs on every PR.

## Repository layout

```
├── build.sh              # idempotent arduino-cli build/flash (single source of truth)
├── libraries.json        # external library pins (FastLED, ESP-Wifi-Config)
├── ferrocell/
│   ├── ferrocell.ino     # thin setup()/loop() entrypoint (M1 boot sketch)
│   ├── sketch.yaml       # IDE 2.x project record (FQBN + core + lib pins)
│   └── src/
│       ├── core/         # pure, host-testable core (no Arduino/FastLED)
│       │   ├── Color.{h,cpp}
│       │   ├── ColorEngine.{h,cpp}
│       │   └── Settings.{h,cpp}
│       ├── hw/           # hardware wrappers (firmware-only)
│       │   ├── LedController.{h,cpp}
│       │   └── ConfigStore.{h,cpp}
│       └── net/          # network layer (firmware-only)
│           └── WifiManager.{h,cpp}   # WiFi AP + STA (M4)
├── lib/
│   └── ESP-Wifi-Config/  # vendored @ v2.2.6 (tabahi/ESP-Wifi-Config)
├── tests/
│   ├── core_test.cpp     # pure-core unit tests (Color + ColorEngine)
│   ├── settings_test.cpp # Settings defaults + normalize/clamping (M3)
│   └── run_tests.sh      # host test runner (g++ -std=c++17, no Arduino)
├── web/                  # UI assets (M5, issue #6)
├── tests/                # host-side test harness (M6, issue #7)
└── .github/workflows/
    └── build.yml         # CI: firmware-build guard
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

## Roadmap

| Milestone | Issue | Scope |
|---|---|---|
| M1 | #2 | Scaffold, build system, project config (this PR) |
| M2 | #3 | Pure core: `Color` + `ColorEngine` (host-testable, no Arduino deps) |
| M3 | #4 | Hardware layer: `LedController` + `ConfigStore` (this PR) |
| M4 | #5 | WiFi: `WifiManager` (AP + STA) |
| M5 | #6 | Web UI (color wheel) + JSON API: `UiServer` |
| M6 | #7 | Test harness + full CI (3 jobs) + docs |
