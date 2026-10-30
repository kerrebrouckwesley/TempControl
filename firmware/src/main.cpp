// TempControl - rond PC-systeemmonitor
// Bord: Waveshare ESP32-S3-Touch-LCD-1.46 (SPD2010, 412x412)
//
// De pc stuurt elke seconde een regel over USB-C, bijvoorbeeld:
//   CPU=42.5;GPU=48.0;FPS=165\n
// Boven HOT_THRESHOLD kleuren de waarde en de ring rood, anders blauw.

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>

// ---------- Instellingen ----------
static const char *BRAND = "QPRINTS";
static const float HOT_THRESHOLD = 50.0f;      // > 50 °C = rood, anders blauw
static const uint32_t DATA_TIMEOUT_MS = 3000;  // geen data -> "wachten op pc"
static const uint8_t ROTATION = 0;             // 0..3, draai het beeld indien nodig

// ---------- Hardware (Waveshare ESP32-S3-Touch-LCD-1.46) ----------
#define PIN_LCD_BL 5
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define TCA9554_ADDR 0x20  // IO-expander: EXIO1 = touch reset, EXIO2 = LCD reset

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    21 /* CS */, 40 /* SCK */, 46 /* D0 */, 45 /* D1 */, 42 /* D2 */, 41 /* D3 */);
Arduino_GFX *panel = new Arduino_SPD2010(bus, GFX_NOT_DEFINED /* RST via TCA9554 */);
Arduino_Canvas *gfx = new Arduino_Canvas(412, 412, panel, 0, 0, ROTATION);

static const int W = 412;
static const int CX = W / 2;
static const int CY = W / 2;

// ---------- Kleuren ----------
struct RGB { uint8_t r, g, b; };

static const RGB RING_COOL_EDGE = {0, 70, 255};
static const RGB RING_COOL_CORE = {120, 215, 255};
static const RGB RING_HOT_EDGE = {255, 10, 20};
static const RGB RING_HOT_CORE = {255, 150, 120};
static const RGB VALUE_COOL = {60, 190, 255};
static const RGB VALUE_HOT = {255, 70, 60};
static const RGB BRAND_RED = {230, 35, 40};
static const RGB GREY = {150, 150, 160};
static const RGB DIVIDER = {55, 55, 65};
static const RGB OFFLINE = {100, 100, 110};

static RGB blend(RGB a, RGB b, float t) {
  t = constrain(t, 0.0f, 1.0f);
  return {(uint8_t)(a.r + (b.r - a.r) * t),
          (uint8_t)(a.g + (b.g - a.g) * t),
          (uint8_t)(a.b + (b.b - a.b) * t)};
}

static uint16_t to565(RGB c, float k = 1.0f) {
  uint8_t r = c.r * k, g = c.g * k, b = c.b * k;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static bool isHot(float t) { return !isnan(t) && t > HOT_THRESHOLD; }

// ---------- Data ----------
static float cpuTemp = NAN;
static float gpuTemp = NAN;
static float fps = NAN;
static uint32_t lastRx = 0;
static bool everReceived = false;
static bool dirty = true;

static float themeMix = 0.0f;  // 0 = blauw, 1 = rood (vloeiende overgang)

static float parseValue(const char *s) {
  char *end;
  float v = strtof(s, &end);
  return (end == s) ? NAN : v;
}

static void handleLine(char *line) {
  if (strcmp(line, "PING") == 0) {
    Serial.println("TEMPMON");
    return;
  }

  float cpu = NAN, gpu = NAN, f = NAN;
  char *save;
  for (char *tok = strtok_r(line, ";", &save); tok; tok = strtok_r(nullptr, ";", &save)) {
    if (strncmp(tok, "CPU=", 4) == 0) cpu = parseValue(tok + 4);
    else if (strncmp(tok, "GPU=", 4) == 0) gpu = parseValue(tok + 4);
    else if (strncmp(tok, "FPS=", 4) == 0) f = parseValue(tok + 4);
  }
  if (isnan(cpu) && isnan(gpu) && isnan(f)) return;

  cpuTemp = cpu;
  gpuTemp = gpu;
  fps = f;
  lastRx = millis();
  everReceived = true;
  dirty = true;
}

static void readSerial() {
  static char buf[128];
  static size_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len) {
        buf[len] = 0;
        handleLine(buf);
        len = 0;
      }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
}

// ---------- Tekenen ----------
enum Align { LEFT, CENTER, RIGHT };

// Tekent tekst verticaal gecentreerd op cy; geeft de breedte terug.
static int drawText(const char *s, int x, int cy, Align align) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 100, &x1, &y1, &w, &h);
  int left = align == LEFT ? x : align == CENTER ? x - w / 2 : x - w;
  gfx->setCursor(left - x1, cy - h / 2 - (y1 - 100));
  gfx->print(s);
  return w;
}

static int textWidth(const char *s) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 100, &x1, &y1, &w, &h);
  return w;
}

// Tekst met extra ruimte tussen de letters ("S Y S T E M").
static void drawSpaced(const char *s, int cx, int cy, int spacing) {
  char ch[2] = {0, 0};
  int total = 0;
  for (const char *p = s; *p; p++) {
    ch[0] = *p;
    total += (*p == ' ' ? 6 : textWidth(ch)) + spacing;
  }
  int x = cx - (total - spacing) / 2;
  for (const char *p = s; *p; p++) {
    ch[0] = *p;
    if (*p != ' ') drawText(ch, x, cy, LEFT);
    x += (*p == ' ' ? 6 : textWidth(ch)) + spacing;
  }
}

static void drawRing(float mix) {
  static const int R_IN = 166, R_OUT = 206;
  static const float R_MID = 190.0f;
  RGB edge = blend(RING_COOL_EDGE, RING_HOT_EDGE, mix);
  RGB core = blend(RING_COOL_CORE, RING_HOT_CORE, mix);

  uint16_t shade[R_OUT - R_IN + 1];
  for (int r = R_IN; r <= R_OUT; r++) {
    // smalle felle kern met zachte gloed naar binnen en buiten
    float d = (r - R_MID) / (r < R_MID ? 16.0f : 9.0f);
    float glow = expf(-d * d * 2.2f);
    float hotCore = expf(-d * d * 14.0f);
    shade[r - R_IN] = to565(blend(edge, core, hotCore), glow);
  }

  const int steps = 1700;
  for (int i = 0; i < steps; i++) {
    float ang = i * TWO_PI / steps;
    float ca = cosf(ang), sa = sinf(ang);
    for (int r = R_IN; r <= R_OUT; r++) {
      gfx->drawPixel(CX + lroundf(r * ca), CY + lroundf(r * sa), shade[r - R_IN]);
    }
  }
}

// --- iconen (lijntekeningen, 2 px dik) ---
static void thickRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
  gfx->drawRoundRect(x, y, w, h, r, c);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, r > 0 ? r - 1 : 0, c);
}

static void iconCpu(int cx, int cy, uint16_t c) {
  thickRoundRect(cx - 12, cy - 12, 24, 24, 3, c);
  gfx->drawRect(cx - 5, cy - 5, 10, 10, c);
  for (int i = 0; i < 4; i++) {
    int o = -8 + i * 5;
    gfx->drawFastVLine(cx + o, cy - 17, 5, c);  // boven
    gfx->drawFastVLine(cx + o, cy + 12, 5, c);  // onder
    gfx->drawFastHLine(cx - 17, cy + o, 5, c);  // links
    gfx->drawFastHLine(cx + 12, cy + o, 5, c);  // rechts
  }
}

static void iconGpu(int cx, int cy, uint16_t c) {
  thickRoundRect(cx - 16, cy - 11, 34, 22, 4, c);
  gfx->drawFastVLine(cx - 20, cy - 11, 26, c);  // slotbeugel
  gfx->drawFastVLine(cx - 19, cy - 11, 26, c);
  gfx->drawFastHLine(cx - 16, cy + 14, 14, c);  // PCIe-connector
  for (int fx : {cx - 6, cx + 9}) {
    gfx->drawCircle(fx, cy, 7, c);
    gfx->drawCircle(fx, cy, 6, c);
    gfx->fillCircle(fx, cy, 2, c);
    gfx->drawLine(fx - 4, cy - 4, fx + 4, cy + 4, c);
    gfx->drawLine(fx - 4, cy + 4, fx + 4, cy - 4, c);
  }
}

static void iconMonitor(int cx, int cy, uint16_t c) {
  thickRoundRect(cx - 16, cy - 13, 32, 21, 3, c);
  gfx->fillRect(cx - 1, cy + 8, 3, 5, c);
  gfx->fillRect(cx - 8, cy + 12, 17, 2, c);
}

static const int VALUE_RIGHT = 300;  // rechterrand van de getallen

static void drawRowLabel(void (*icon)(int, int, uint16_t), const char *label, int cy) {
  icon(102, cy, to565({235, 235, 240}));
  gfx->setFont(u8g2_font_fub17_tr);
  gfx->setTextColor(to565({240, 240, 245}));
  drawText(label, 136, cy, LEFT);
}

static void drawValue(const char *val, RGB color, int cy, bool unit) {
  gfx->setFont(strlen(val) > 3 ? u8g2_font_fub35_tn : u8g2_font_fub42_tn);
  gfx->setTextColor(to565(color));
  drawText(val, VALUE_RIGHT, cy, RIGHT);

  if (unit) {  // °C, bovenaan uitgelijnd zoals in het ontwerp
    uint16_t c = to565(color);
    gfx->drawCircle(VALUE_RIGHT + 8, cy - 14, 3, c);
    gfx->drawCircle(VALUE_RIGHT + 8, cy - 14, 2, c);
    gfx->setFont(u8g2_font_fub17_tr);
    gfx->setTextColor(c);
    drawText("C", VALUE_RIGHT + 13, cy - 6, LEFT);
  }
}

static void drawTemp(void (*icon)(int, int, uint16_t), const char *label, float t, bool live, int cy) {
  drawRowLabel(icon, label, cy);
  char val[8];
  if (live && !isnan(t)) snprintf(val, sizeof(val), "%ld", lroundf(t));
  else strcpy(val, "--");
  drawValue(val, live ? (isHot(t) ? VALUE_HOT : VALUE_COOL) : OFFLINE, cy, true);
}

static void render(bool live, float mix) {
  gfx->fillScreen(RGB565_BLACK);
  drawRing(mix);

  // Merk: "QPRINTS //"
  gfx->setFont(u8g2_font_fub20_tr);
  const char *slashes = " //";
  int wBrand = textWidth(BRAND), wSlash = textWidth(slashes);
  int x = CX - (wBrand + wSlash) / 2;
  gfx->setTextColor(to565({245, 245, 250}));
  drawText(BRAND, x, 108, LEFT);
  gfx->setTextColor(to565(BRAND_RED));
  drawText(slashes, x + wBrand, 108, LEFT);

  gfx->setFont(u8g2_font_fur11_tr);
  gfx->setTextColor(to565(GREY));
  if (live) drawSpaced("SYSTEM MONITOR", CX, 136, 4);
  else drawSpaced(everReceived ? "VERBINDING VERLOREN" : "WACHTEN OP PC", CX, 136, 3);

  // Rijen
  drawTemp(iconCpu, "CPU", cpuTemp, live, 184);
  gfx->drawFastHLine(84, 218, 244, to565(DIVIDER));
  drawTemp(iconGpu, "GPU", gpuTemp, live, 251);
  gfx->drawFastHLine(84, 285, 244, to565(DIVIDER));

  drawRowLabel(iconMonitor, "FPS", 316);
  char val[8];
  if (live && !isnan(fps)) snprintf(val, sizeof(val), "%ld", min(lroundf(fps), 9999L));
  else strcpy(val, "--");
  drawValue(val, live ? blend(VALUE_COOL, VALUE_HOT, mix) : OFFLINE, 316, false);

  gfx->flush();
}

// Reset LCD + touch via de TCA9554 IO-expander (het bord heeft geen directe RST-pin).
static void resetPanel() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  auto writeReg = [](uint8_t reg, uint8_t val) {
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  };
  writeReg(0x03, 0xFC);  // EXIO1 + EXIO2 als uitgang
  writeReg(0x01, 0x00);  // laag = reset
  delay(20);
  writeReg(0x01, 0x03);  // hoog = vrijgeven
  delay(120);
}

void setup() {
  Serial.begin(115200);

  resetPanel();

  if (!gfx->begin()) {
    Serial.println("Display init mislukt");
  }
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
}

void loop() {
  readSerial();

  static uint32_t lastFrame = 0;
  static bool wasLive = false;
  if (millis() - lastFrame < 40) return;  // max ~25 fps
  lastFrame = millis();

  const bool live = everReceived && (millis() - lastRx < DATA_TIMEOUT_MS);
  const bool hot = live && (isHot(cpuTemp) || isHot(gpuTemp));
  if (live != wasLive) {
    wasLive = live;
    dirty = true;
  }

  // vloeiende kleurovergang blauw <-> rood
  const float target = hot ? 1.0f : 0.0f;
  if (fabsf(target - themeMix) > 0.005f) {
    themeMix += (target - themeMix) * 0.15f;
    dirty = true;
  } else {
    themeMix = target;
  }

  if (dirty) {
    dirty = false;
    render(live, themeMix);
  }
}
