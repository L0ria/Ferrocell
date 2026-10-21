// =============================================================================
//  Ferrocell — M1 minimal boot sketch
//
//  Hardware : Waveshare ESP32-S3-Zero
//  FQBN     : esp32:esp32:waveshare_esp32_s3_zero
//  Strip    : external WS2812B on GPIO7 (5V rail + level shifter).
//             The onboard WS2812 (GPIO21) is intentionally NOT used.
//
//  This milestone only proves the arduino-cli toolchain + build system and a
//  working USB-CDC serial link. The class-based application (Color /
//  ColorEngine / LedController / ConfigStore / WifiManager / UiServer) lands
//  in M2–M5 (issues #3–#6). Keep this file the thin setup()/loop() entrypoint;
//  real logic goes into the .cpp/.h modules added by the later milestones.
// =============================================================================

// External WS2812B data pin (single GPIO, free of strapping/PSRAM).
static const uint8_t kLedStripPin = 7;

void setup() {
  // USB-CDC serial (board default USBMode=hwcdc) at 115200.
  Serial.begin(115200);
  delay(300);  // let the USB-CDC link settle before the first print

  // Strip data pin: output, driven LOW at boot (all LEDs off).
  pinMode(kLedStripPin, OUTPUT);
  digitalWrite(kLedStripPin, LOW);

  Serial.println("Ferrocell boot");
  Serial.print  ("board=");      Serial.println("waveshare_esp32_s3_zero");
  Serial.print  ("strip_gpio="); Serial.println(kLedStripPin);
  Serial.println("milestone=M1 (scaffold + build)");
}

void loop() {
  // M2+ : drive the strip from the ColorEngine, serve the web UI, manage WiFi.
  // Intentionally idle for M1 — the boot print above is the contract.
  delay(1000);
}
