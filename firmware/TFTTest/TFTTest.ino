// ============================================================================
//  TFTTest - bring-up test for the DOOM business card (ESP32-S3 + ST7789)
//
//  Run this FIRST, before flashing the full game. It proves three things:
//    1. The display pins / rotation are correct  -> you see color bars + text
//    2. PSRAM is enabled                          -> reported on screen + serial
//    3. The 5 buttons + chords are wired right    -> labels light up when pressed
//
//  Same pin map as Firmware.ino. If anything here is wrong, fix it here first,
//  then copy the corrected pins into Firmware.ino.
//
//  Flash it:
//    arduino-cli compile -b "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,CDCOnBoot=cdc" -u -p COM7 .
//  (no custom partition needed for this test)
// ============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ---- pin map (MUST match Firmware.ino) --------------------------------------
#define PIN_TFT_SCLK 12
#define PIN_TFT_MOSI 11
#define PIN_TFT_CS   10
#define PIN_TFT_DC    9
#define PIN_TFT_RST   8
#define PIN_TFT_BL   -1

#define PIN_BTN_UP    19
#define PIN_BTN_LEFT  20
#define PIN_BTN_RIGHT 21
#define PIN_BTN_DOWN  22
#define PIN_BTN_FIRE  18

#define BTN_ACTIVE_LOW 1     // set 0 if buttons go to 3V3 instead of GND

SPIClass tftSPI(FSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&tftSPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

static inline bool pressed(int pin) {
  int v = digitalRead(pin);
  return BTN_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// draw 8 vertical color bars across the 320px-wide (landscape) screen
void drawColorBars() {
  const uint16_t bars[8] = {
    ST77XX_WHITE, ST77XX_YELLOW, ST77XX_CYAN, ST77XX_GREEN,
    ST77XX_MAGENTA, ST77XX_RED, ST77XX_BLUE, ST77XX_BLACK
  };
  int w = tft.width() / 8;
  for (int i = 0; i < 8; i++)
    tft.fillRect(i * w, 0, w, tft.height(), bars[i]);
}

void label(int x, int y, const char* txt, bool on) {
  tft.fillRect(x, y, 96, 22, on ? ST77XX_GREEN : ST77XX_BLACK);
  tft.drawRect(x, y, 96, 22, ST77XX_WHITE);
  tft.setTextColor(on ? ST77XX_BLACK : ST77XX_WHITE);
  tft.setCursor(x + 6, y + 5);
  tft.print(txt);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[TFTTest] start");

  uint8_t mode = BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN;
  pinMode(PIN_BTN_UP, mode);
  pinMode(PIN_BTN_DOWN, mode);
  pinMode(PIN_BTN_LEFT, mode);
  pinMode(PIN_BTN_RIGHT, mode);
  pinMode(PIN_BTN_FIRE, mode);

  tftSPI.begin(PIN_TFT_SCLK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);                // landscape: 320 x 240
  if (PIN_TFT_BL >= 0) { pinMode(PIN_TFT_BL, OUTPUT); digitalWrite(PIN_TFT_BL, HIGH); }

  // PSRAM check
  bool psram = psramFound();
  Serial.printf("[TFTTest] screen %dx%d, PSRAM=%s (%u bytes)\n",
                tft.width(), tft.height(), psram ? "YES" : "NO", (unsigned)ESP.getPsramSize());

  // 1) color bars for ~2s so you can confirm colors/orientation
  drawColorBars();
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
  tft.setCursor(4, 4);
  tft.print(psram ? "PSRAM OK" : "NO PSRAM!");
  delay(2000);

  // 2) switch to button-test screen
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 4);
  tft.print("Press buttons:");
  tft.setTextSize(1);
  tft.setCursor(4, 220);
  tft.print("L+R=USE  UP+FIRE=ESC  DOWN+FIRE=ENTER");
}

void loop() {
  bool U = pressed(PIN_BTN_UP),  D = pressed(PIN_BTN_DOWN);
  bool L = pressed(PIN_BTN_LEFT),R = pressed(PIN_BTN_RIGHT);
  bool F = pressed(PIN_BTN_FIRE);

  tft.setTextSize(2);
  // cross layout
  label(112,  40, "UP",    U);
  label( 12, 110, "LEFT",  L);
  label(212, 110, "RIGHT", R);
  label(112, 150, "DOWN",  D);
  label(112,  95, "FIRE",  F);   // center

  // chords
  tft.setTextSize(2);
  label( 12,  40, "USE",   L && R);
  label(212,  40, "ESC",   U && F);
  label(212, 150, "ENTER", D && F);

  static uint8_t last = 0xFF;
  uint8_t state = (U<<0)|(D<<1)|(L<<2)|(R<<3)|(F<<4);
  if (state != last) {
    Serial.printf("[btn] UP=%d DOWN=%d LEFT=%d RIGHT=%d FIRE=%d\n", U, D, L, R, F);
    last = state;
  }
  delay(40);
}
