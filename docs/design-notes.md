# Design notes

- **Modular & object-oriented.** The application is a class-based composition —
  `Color`, `ColorEngine`, `LedController`, `ConfigStore`, `WifiManager`,
  `UiServer` (M2–M5) — not a bag of free functions. `ferrocell.ino` stays the
  thin `setup()`/`loop()` entrypoint.
- **One GPIO for the strip.** GPIO7 is free of strapping pins and the Octal
  PSRAM range (GPIO33–37), so it is the clean choice for the external WS2812B
  data line.
- **Reproducible builds.** Core and libraries are pinned; the build is
  idempotent and runs unchanged in CI (`.github/workflows/build.yml`).
- **4-job CI, no hardware.** `core-tests` (unit, host g++), `ui-contract`
  (Node JS-contract harness + JS<->C++ cross-check), `firmware-build`
  (arduino-cli), `firmware-build-c3` (arduino-cli, RISC-V) — see the CI section
  in the README.
- **Responsive UI (issue #14).** Dragging the wheel / sliders updates the UI
  optimistically and coalesces the network + flash-write into one request per
  drag (debounced `/api/set` + deferred NVS save + a `showIfChanged()` dirty
  check that skips the blocking `FastLED.show()` on unchanged frames).
- **OTA over WiFi (issue #16).** `ElegantOTA` (4.0.0) shares the UI's
  `WebServer` (port 80, STA + AP) for the `/update` portal; `otanofs` gives two
  1.94 MiB OTA slots; the saved WiFi config (NVS) is untouched by an update.
