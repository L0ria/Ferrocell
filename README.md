# Ferrocell

Firmware for a Waveshare **ESP32-S3-Zero** that drives a **WS2812B** LED strip
in a ferrocell (a device that visualizes magnetic fields). The ESP32 exists
solely to control the strip's color so the field visualization can be changed
at runtime.

> Status: **M1 — scaffold + build system** (issue #2). The firmware currently
> boots, prints over USB-CDC, and holds the strip data pin LOW. The class-based
> application (color engine, LED controller, WiFi, web UI) lands in M2–M5
> (issues #3–#6); the test harness + full CI in M6 (issue #7).

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
| AP IP | `192.168.1.1` (library default) — config page at `http://192.168.1.1/` |

The same pins are recorded in `ferrocell/sketch.yaml` (IDE 2.x) and
`libraries.json` (lib pins).

## Repository layout

```
├── build.sh              # idempotent arduino-cli build/flash (single source of truth)
├── libraries.json        # external library pins (FastLED, ESP-Wifi-Config)
├── ferrocell/
│   ├── ferrocell.ino     # thin setup()/loop() entrypoint (M1 boot sketch)
│   └── sketch.yaml       # IDE 2.x project record (FQBN + core + lib pins)
├── lib/
│   └── ESP-Wifi-Config/  # vendored @ v2.2.6 (tabahi/ESP-Wifi-Config)
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
| M3 | #4 | Hardware layer: `LedController` + `ConfigStore` |
| M4 | #5 | WiFi: `WifiManager` (AP + STA) |
| M5 | #6 | Web UI (color wheel) + JSON API: `UiServer` |
| M6 | #7 | Test harness + full CI (3 jobs) + docs |
