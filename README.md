# Ferrocell

Firmware for a Waveshare **ESP32-S3-Zero** that drives a **WS2812B** LED strip
in a ferrocell (a device that visualizes magnetic fields). The ESP32 exists
solely to control the strip's color so the field visualization can be changed
at runtime.

Status: **M1–M6 complete** — pure core (issue #3), hardware layer (issue #4),
WiFi (issue #5), web UI + JSON API (issue #6), test harness + CI + docs
(issue #7), plus OTA over WiFi (issue #16). The firmware boots over USB-CDC,
loads settings (NVS; sane defaults on first boot), attaches the WS2812B strip
on GPIO7 via FastLED, renders frames from the `ColorEngine`, brings up the
WiFi layer (`WifiManager`: AP config page or STA with auto-reconnect), and
serves the self-contained web UI + JSON API (`UiServer`) on port 80.

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
```

`build.sh` is self-contained: it locates (or installs) arduino-cli, registers
the esp32 board index, installs the **pinned core** (esp32 3.3.12), the
**pinned FastLED** (3.10.6), and compiles. Re-running is a no-op for the
installs.

`flash.sh` uploads the image `build.sh` already exported — no recompile — so
it is the quick tool for re-flashing and for baud/chip experiments (the
S3-Zero can brownout from the 5 V rail at higher flash speeds, issue #19):

```bash
./flash.sh                            # flash build/ to the first ESP32 @ 115200
./flash.sh --port /dev/ttyACM0        # specific port
./flash.sh --speed 921600             # faster link (if the rail can take it)
./flash.sh --erase                    # erase all flash (factory reset)
```

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

## Docs

Deep-dive documentation lives in [`docs/`](docs/README.md): the pure core +
hardware layer (`core.md`), WiFi (`wifi.md`), web UI + JSON API (`web-ui-api.md`),
OTA (`ota.md`), and design notes (`design-notes.md`).

## Tests

Host-side tests, no Arduino and no hardware — see [`tests/README.md`](tests/README.md).

## CI (4 jobs, no hardware)

`.github/workflows/build.yml` runs on every PR (and on `main`): `core-tests`
(pure-core unit tests, host `g++`), `ui-contract` (web-UI JS-contract harness
+ JS<->C++ cross-check), `firmware-build` (the real `./build.sh` for the
S3-Zero), and `firmware-build-c3` (same sketch for an ESP32-C3, RISC-V —
issue #23).
