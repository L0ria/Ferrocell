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
//  WiFi (WifiManager) and the web UI (UiServer) land in M4–M5 (issues #5–#6).
//
//  Boot flow:
//    1. USB-CDC serial up.
//    2. Load settings (NVS; sane defaults on first boot).
//    3. Attach the strip (FastLED) with the configured LED count.
//    4. loop(): render a frame from the ColorEngine at the current time.
// =============================================================================

#include <Arduino.h>

#include "src/core/Settings.h"
#include "src/hw/ConfigStore.h"
#include "src/hw/LedController.h"

// App-wide singletons (composition root). Kept here so the entrypoint stays
// the only place that wires the modules together.
static ConfigStore g_config;
static LedController g_leds;
static ferrocell::Settings g_settings;

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB-CDC link settle before the first print

  Serial.println("Ferrocell boot");
  Serial.print  ("board="); Serial.println("waveshare_esp32_s3_zero");
  Serial.print  ("strip_gpio="); Serial.println(LedController::kPin);
  Serial.println("milestone=M3 (hardware layer: LedController + ConfigStore)");

  // 1. Load settings (defaults on first boot; always normalized).
  g_settings = g_config.load();

  // 2. Attach the strip with the configured LED count.
  g_leds.begin(g_settings.ledCount());
  Serial.printf("[boot] strip attached: %u LEDs\n", g_leds.ledCount());

  // 3. Render an initial frame so the strip shows the configured state.
  g_leds.render(g_settings, millis());
}

void loop() {
  // Render the current settings each loop. The ColorEngine is a pure function
  // of (settings, time), so animated modes (rainbow/breathing) advance as
  // millis() increases. (M4–M5 add WiFi + the web UI to change g_settings.)
  g_leds.render(g_settings, millis());
  delay(10);  // ~100 fps cap; keeps the loop from spinning hot
}
