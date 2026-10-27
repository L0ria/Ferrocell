# Core (pure, host-testable)

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

