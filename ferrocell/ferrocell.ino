// =============================================================================
//  Ferrocell — firmware entrypoint (M5, issue #6)
//
//  Hardware : Waveshare ESP32-S3-Zero
//  FQBN     : esp32:esp32:waveshare_esp32_s3_zero
//  Strip    : external WS2812B on GPIO7 (5V rail + level shifter).
//             The onboard WS2812 (GPIO21) is intentionally NOT used.
//
//  This file stays the THIN setup()/loop() entrypoint. All real logic is in
//  the class-based modules (object-oriented, modular):
//    core/Settings, core/Color, core/ColorEngine   (pure, host-tested)
//    core/Json, core/Api                           (pure, host-tested — the
//                                                   JSON API contract, M5)
//    hw/ConfigStore, hw/LedController              (hardware wrappers)
//    net/WifiManager                               (WiFi: AP + STA, M4)
//    net/UiServer                                  (web UI + JSON API, M5)
//
//  Boot flow:
//    1. USB-CDC serial up.
//    2. Load settings (NVS; sane defaults on first boot).
//    3. Attach the strip (FastLED) with the configured LED count.
//    4. Boot the WiFi layer (AP config page on :8080, or STA if saved).
//    5. Boot the web UI + JSON API on port 80 (UiServer).
//    6. loop(): render a frame + pump the WiFi layer + the web server.
// =============================================================================

#include <Arduino.h>

#include "src/core/Settings.h"
#include "src/hw/ConfigStore.h"
#include "src/hw/LedController.h"
#include "src/net/WifiManager.h"
#include "src/net/UiServer.h"

// App-wide singletons (composition root). Kept here so the entrypoint stays
// the only place that wires the modules together.
static ConfigStore g_config;
static LedController g_leds;
static WifiManager g_wifi;
static ferrocell::Settings g_settings;
static UiServer g_ui(g_config, g_leds, g_wifi);

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB-CDC link settle before the first print

  Serial.println("Ferrocell boot");
  Serial.print  ("board="); Serial.println("waveshare_esp32_s3_zero");
  Serial.print  ("strip_gpio="); Serial.println(LedController::kPin);
  Serial.println("milestone=M5 (web UI + JSON API: UiServer)");

  // 1. Load settings (defaults on first boot; always normalized).
  g_settings = g_config.load();

  // 2. Attach the strip with the configured LED count.
  g_leds.begin(g_settings.ledCount());
  Serial.printf("[boot] strip attached: %u LEDs\n", g_leds.ledCount());

  // 3. Render an initial frame so the strip shows the configured state.
  g_leds.render(g_settings, millis());

  // 4. Boot the WiFi layer (M4): AP config page on :8080, or STA if a saved
  //    network is in range. Non-blocking thereafter (pumped in loop()).
  g_wifi.begin();

  // 5. Boot the web UI + JSON API (M5) on port 80. The UI reads its state
  //    from the same settings the entrypoint owns, so seed the server's copy.
  g_ui.settings() = g_settings;
  g_ui.begin();
}

void loop() {
  // Pump the WiFi layer: STA auto-reconnect + serve the config page (:8080).
  g_wifi.handle();

  // Pump the web UI + JSON API (port 80).
  g_ui.handle();

  // Render the current settings each loop. g_ui.settings() is the single
  // source of truth: /api/set updates it, and the ColorEngine is a pure
  // function of (settings, time), so animated modes (rainbow/breathing)
  // advance as millis() increases.
  g_leds.render(g_ui.settings(), millis());
  delay(10);  // ~100 fps cap; keeps the loop from spinning hot
}
