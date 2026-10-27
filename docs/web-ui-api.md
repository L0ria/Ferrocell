# Web UI + JSON API (M5, firmware-only + pure core)

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
stepper, and a **WiFi tab** (scan / save / reboot / reset).
The pure helpers in `ui.js` (`FerrocellUI.pure`) are importable by Node so the
M6 harness can assert the JS↔C++ contract.

**LED count (issue #27):** the `−`/`+` buttons (or typing a value) only change
the displayed count **locally** — no network, no reboot — so it can be stepped
repeatedly. **Save** (or Enter in the field) is the only commit: exactly one
`POST /api/set`, and the device reboots **once** (only when the value actually
changed) to re-initialize the strip. The stepper clamps to 1..512
(`Settings.h`), host-tested in the M6 harness (section 11).

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
