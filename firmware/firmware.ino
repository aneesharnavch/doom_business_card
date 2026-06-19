// ============================================================================
//  DOOM on a business card  -  ESP32-S3 + ST7789 (HS24CG003-ARX)
//  Glue layer between Arduino and doomgeneric (full shareware Doom).
//
//  Hardware (your board):
//    Display ST7789 (SPI):  SCLK=12  MOSI/SDI=11  CS=10  DC=9  RST=8
//                           TE/framemark=13 (unused), backlight=always-on
//    Buttons (to GND, active-low, internal pull-ups):
//                           UP=19  LEFT=20  RIGHT=21  DOWN=22  FIRE=18
//
//  Controls (5 buttons + chords -> full Doom):
//    UP/DOWN .............. move forward / back
//    LEFT/RIGHT ........... turn left / right
//    FIRE ................. shoot
//    LEFT + RIGHT ......... USE (open doors, hit switches)
//    UP   + FIRE .......... ESC  (open / close menu)
//    DOWN + FIRE .......... ENTER (confirm in menu / start game)
// ============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <LittleFS.h>

extern "C" {
  #include "src/doom/doomgeneric.h"
  #include "src/doom/doomkeys.h"
}

// ---------------------------------------------------------------- pin map ----
#define PIN_TFT_SCLK 12
#define PIN_TFT_MOSI 11
#define PIN_TFT_CS   10
#define PIN_TFT_DC    9
#define PIN_TFT_RST   8
#define PIN_TFT_BL   -1      // no dedicated backlight pin on this card

#define PIN_BTN_UP    19
#define PIN_BTN_LEFT  20
#define PIN_BTN_RIGHT 21
#define PIN_BTN_DOWN  22
#define PIN_BTN_FIRE  18

// If your buttons are wired to 3V3 instead of GND, set this to 0.
#define BTN_ACTIVE_LOW 1

// --------------------------------------------------------------- display ----
// Doom renders at 320x200. The panel in landscape is 320x240, so we letterbox
// with 20px black bars top and bottom.
#define FB_W   DOOMGENERIC_RESX     // 320 (set via build flag)
#define FB_H   DOOMGENERIC_RESY     // 200
#define SCR_H  240
#define Y_OFF  ((SCR_H - FB_H) / 2) // 20

SPIClass tftSPI(FSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&tftSPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

static uint16_t* fb565 = nullptr;  // RGB565 scratch frame (in PSRAM)

// ------------------------------------------------------------ key events ----
enum {
  K_UP = 1 << 0, K_DN = 1 << 1, K_LT = 1 << 2, K_RT = 1 << 3,
  K_FR = 1 << 4, K_USE = 1 << 5, K_ESC = 1 << 6, K_ENT = 1 << 7
};

static unsigned char doomKeyOf(uint16_t bit) {
  switch (bit) {
    case K_UP:  return KEY_UPARROW;
    case K_DN:  return KEY_DOWNARROW;
    case K_LT:  return KEY_LEFTARROW;
    case K_RT:  return KEY_RIGHTARROW;
    case K_FR:  return KEY_FIRE;
    case K_USE: return KEY_USE;
    case K_ESC: return KEY_ESCAPE;
    case K_ENT: return KEY_ENTER;
  }
  return 0;
}

static inline bool rawPressed(int pin) {
  int v = digitalRead(pin);
  return BTN_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// Map the 5 physical buttons (with chords) to a bitmask of logical Doom keys.
static uint16_t scanKeys() {
  bool U = rawPressed(PIN_BTN_UP);
  bool D = rawPressed(PIN_BTN_DOWN);
  bool L = rawPressed(PIN_BTN_LEFT);
  bool R = rawPressed(PIN_BTN_RIGHT);
  bool F = rawPressed(PIN_BTN_FIRE);

  uint16_t k = 0;
  if (L && R) { k |= K_USE; L = R = false; }   // open doors / switches
  if (U && F) { k |= K_ESC; U = F = false; }   // menu toggle
  if (D && F) { k |= K_ENT; D = F = false; }   // confirm
  if (U) k |= K_UP;
  if (D) k |= K_DN;
  if (L) k |= K_LT;
  if (R) k |= K_RT;
  if (F) k |= K_FR;
  return k;
}

#define QN 32
static unsigned char qkey[QN];
static int           qpr[QN];
static int           qh = 0, qt = 0;
static uint16_t      lastKeys = 0;

static void pushEv(int pressed, unsigned char key) {
  int n = (qt + 1) % QN;
  if (n == qh) return;          // queue full, drop
  qkey[qt] = key; qpr[qt] = pressed; qt = n;
}

static void pollButtons() {
  uint16_t now = scanKeys();
  uint16_t ch  = now ^ lastKeys;
  for (int b = 0; b < 8; b++) {
    uint16_t m = 1 << b;
    if (ch & m) pushEv((now & m) ? 1 : 0, doomKeyOf(m));
  }
  lastKeys = now;
}

// =================== functions doomgeneric calls into ======================
extern "C" void DG_Init() {
  // Display + buttons are already initialised in setup().
}

extern "C" void DG_DrawFrame() {
  const pixel_t* src = DG_ScreenBuffer;   // XRGB8888, FB_W*FB_H
  const int n = FB_W * FB_H;
  for (int i = 0; i < n; i++) {
    uint32_t p = src[i];
    uint8_t r = (p >> 16) & 0xFF;
    uint8_t g = (p >> 8)  & 0xFF;
    uint8_t b =  p        & 0xFF;
    fb565[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  tft.startWrite();
  tft.setAddrWindow(0, Y_OFF, FB_W, FB_H);
  tft.writePixels(fb565, n);              // lib byte-swaps for the panel
  tft.endWrite();
}

extern "C" void DG_SleepMs(uint32_t ms) { delay(ms); }

extern "C" uint32_t DG_GetTicksMs() { return millis(); }

extern "C" int DG_GetKey(int* pressed, unsigned char* key) {
  if (qh == qt) pollButtons();            // refill when drained
  if (qh == qt) return 0;
  *pressed = qpr[qh];
  *key     = qkey[qh];
  qh = (qh + 1) % QN;
  return 1;
}

extern "C" void DG_SetWindowTitle(const char* title) { (void)title; }

// ================================ Arduino ==================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[bcard-doom] booting");

  // buttons
  uint8_t mode = BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN;
  pinMode(PIN_BTN_UP, mode);
  pinMode(PIN_BTN_DOWN, mode);
  pinMode(PIN_BTN_LEFT, mode);
  pinMode(PIN_BTN_RIGHT, mode);
  pinMode(PIN_BTN_FIRE, mode);

  // display
  tftSPI.begin(PIN_TFT_SCLK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.init(240, 320);            // native panel resolution (portrait)
  tft.setSPISpeed(40000000);     // 40 MHz; bump to 80M if your wiring is clean
  tft.setRotation(1);            // landscape -> 320 wide x 240 tall
  tft.fillScreen(ST77XX_BLACK);

  if (PIN_TFT_BL >= 0) { pinMode(PIN_TFT_BL, OUTPUT); digitalWrite(PIN_TFT_BL, HIGH); }

  // RGB565 scratch buffer in PSRAM
  fb565 = (uint16_t*) ps_malloc((size_t)FB_W * FB_H * sizeof(uint16_t));
  if (!fb565) {
    Serial.println("[bcard-doom] FATAL: ps_malloc(fb565) failed - is PSRAM enabled?");
    while (true) delay(1000);
  }

  // filesystem holding DOOM1.WAD
  if (!LittleFS.begin(false)) {
    Serial.println("[bcard-doom] FATAL: LittleFS mount failed - did you flash the WAD image?");
    while (true) delay(1000);
  }
  if (!LittleFS.exists("/DOOM1.WAD")) {
    Serial.println("[bcard-doom] FATAL: /DOOM1.WAD not found on LittleFS.");
    while (true) delay(1000);
  }
  Serial.println("[bcard-doom] WAD found, starting Doom...");

  // hand off to Doom. Path is the VFS mount point of LittleFS.
  static char arg0[] = "doom";
  static char arg1[] = "-iwad";
  static char arg2[] = "/littlefs/DOOM1.WAD";
  static char* argv[] = { arg0, arg1, arg2 };
  doomgeneric_Create(3, argv);   // runs Doom init, then returns

  Serial.println("[bcard-doom] init done, entering game loop");
}

void loop() {
  doomgeneric_Tick();            // advance one Doom frame
}
