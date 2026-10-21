// =============================================================================
//  Ferrocell — firmware entrypoint (M3, issue #4)
//
//  Hardware : Waveshare ESP32-S3-Zero
//  FQBN     : esp32:esp32:waveshare_esp32_s3_zero
//  Strip    : external WS2812B on GPIO7 (5V rail + level shifter).
//             The onboard WS2812 (GPIO21) is intentionally NOT used.
//
//  This file stays the THIN setup()/loop() entrypoint. All real logic is in
//  the class-based modules (object-oriented, modular):
//    core/Settings, core/Color, core/ColorEngine   (pure, host-tested)
//    hw/ConfigStore, hw/LedController              (hardware wrappers)
//    net/WifiManager                               (WiFi: AP + STA, M4)
//  The web UI (UiServer) lands in M5 (issue #6).
//
//  Boot flow:
//    1. USB-CDC serial up.
//    2. Load settings (NVS; sane defaults on first boot).
//    3. Attach the strip (FastLED) with the configured LED count.
//    4. Boot the WiFi layer (AP config page, or STA if a saved network is up).
//    5. loop(): render a frame + pump the WiFi layer (reconnect + config page).
// =============================================================================

#include <Arduino.h>

#include "src/core/Settings.h"
#include "src/hw/ConfigStore.h"
#include "src/hw/LedController.h"
#include "src/net/WifiManager.h"

// App-wide singletons (composition root). Kept here so the entrypoint stays
// the only place that wires the modules together.
static ConfigStore g_config;
static LedController g_leds;
static WifiManager g_wifi;
static ferrocell::Settings g_settings;

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB-CDC link settle before the first print

  Serial.println("Ferrocell boot");
  Serial.print  ("board="); Serial.println("waveshare_esp32_s3_zero");
  Serial.print  ("strip_gpio="); Serial.println(LedController::kPin);
  Serial.println("milestone=M4 (WiFi layer: WifiManager AP + STA)");

  // 1. Load settings (defaults on first boot; always normalized).
  g_settings = g_config.load();

  // 2. Attach the strip with the configured LED count.
  g_leds.begin(g_settings.ledCount());
  Serial.printf("[boot] strip attached: %u LEDs\n", g_leds.ledCount());

  // 3. Render an initial frame so the strip shows the configured state.
  g_leds.render(g_settings, millis());

  // 4. Boot the WiFi layer (M4): AP config page, or STA if a saved network
  //    is in range. Non-blocking thereafter (pumped in loop()).
  g_wifi.begin();
}

void loop() {
  // Pump the WiFi layer: STA auto-reconnect + serve the config page.
  // Non-blocking; cheap when nothing is pending.
  g_wifi.handle();

  // Render the current settings each loop. The ColorEngine is a pure function
  // of (settings, time), so animated modes (rainbow/breathing) advance as
  // millis() increases. (M5 adds the web UI to change g_settings.)
  g_leds.render(g_settings, millis());
  delay(10);  // ~100 fps cap; keeps the loop from spinning hot
}
