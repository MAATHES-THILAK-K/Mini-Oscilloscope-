#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>

U8G2_SH1106_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

#define REC_LEN 200
#define MIN_TRIG_SWING 5
#define LSB_5V 0.00566826f
#define LSB_50V 0.05243212f

// ── PROGMEM tables ────────────────────────────────────────────────────────────
const char vName[10][5] PROGMEM = {
  "A50V", "A 5V", " 50V", " 20V", " 10V", "  5V", "  2V", "  1V", "0.5V", "0.2V"
};
const char hName[10][6] PROGMEM = {
  "200ms", "100ms", " 50ms", " 20ms", " 10ms", "  5ms", "  2ms", "  1ms", "500us", "200us"
};
const float H_SEC[10] PROGMEM = {
  0.2f, 0.1f, 0.05f, 0.02f, 0.01f, 0.005f, 0.002f, 0.001f, 5e-4f, 2e-4f
};

// V-range: fullScaleV (0=auto), dispMax (×100), att10x
struct VCfg {
  float fsV;
  int16_t dMax;
  uint8_t att;
};
const VCfg V_CFG[10] PROGMEM = {
  { 0, 0, 1 }, { 0, 0, 0 }, { 50, 5000, 1 }, { 20, 2000, 1 }, { 10, 1000, 1 }, { 5, 500, 0 }, { 2, 200, 0 }, { 1, 100, 0 }, { 0.5f, 50, 0 }, { 0.2f, 20, 0 }
};

// H-range: ADC prescaler bits, delay_µs, execMs
struct HCfg {
  uint8_t adcBits;
  uint16_t delayUs;
  uint16_t execMs;
};
const HCfg H_CFG[10] PROGMEM = {
  { 0x07, 7888, 1660 }, { 0x07, 3860, 860 }, { 0x07, 1880, 460 }, { 0x07, 686, 220 }, { 0x07, 287, 140 }, { 0x07, 87, 100 }, { 0x06, 23, 76 }, { 0x05, 10, 68 }, { 0x04, 0, 64 }, { 0x02, 0, 62 }
};

// ── RAM ──────────────────────────────────────────────────────────────────────
// CHANGE: int16_t → uint8_t saves 200 bytes (ADC stored as >>2, range 0-255)
uint8_t waveBuff[REC_LEN];
char chrBuff[8];

// CHANGE: tighter types for dataMin/Max (now 0-255), trigP (0-199)
volatile int8_t vRange = 3, hRange = 3, trigD = 1, scopeP = 1;
volatile bool switchPushed = false;
volatile int16_t saveTimer = 0;

uint16_t timeExec;
uint8_t dataMin, dataMax;  // 0-255 now
int16_t dataAve;
int16_t rangeMax, rangeMin;
int16_t rangeMaxDisp, rangeMinDisp;
uint8_t trigP;  // 0-199
bool trigSync;
uint8_t att10x;
// REMOVED: waveFreq, waveDuty, hold

// ── Helpers ───────────────────────────────────────────────────────────────────
inline int16_t sum3(int i) {
  return waveBuff[i - 1] + waveBuff[i] + waveBuff[i + 1];
}
inline void setADC(uint8_t bits) {
  ADCSRA = (ADCSRA & 0xF8) | bits;
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────
void setup() {
  pinMode(2,INPUT_PULLUP); pinMode(8,INPUT_PULLUP); pinMode(9,INPUT_PULLUP);
  pinMode(10,INPUT_PULLUP); pinMode(11,INPUT_PULLUP);
  pinMode(12,INPUT); pinMode(13,OUTPUT);

  oled.begin();                        // ← was oled.begin(SH1106_SWITCHCAPVCC, 0x3C)
  oled.setFont(u8g2_font_5x7_tf);     // ← set once here, reuse everywhere

  loadEEPROM();
  analogReference(INTERNAL);
  attachInterrupt(0, pin2IRQ, FALLING);
  startScreen();
}

void loop() {
  setConditions();
  digitalWrite(13, HIGH);
  readWave();
  digitalWrite(13, LOW);
  setConditions();
  dataAnalize();

  // ── Page-mode draw loop (replaces clearDisplay + display) ──
  oled.firstPage();
  do {
    writeCommonImage();
    plotData();
    dispInf();
  } while (oled.nextPage());

  saveEEPROM();
}

// ── Conditions ────────────────────────────────────────────────────────────────
void setConditions() {
  VCfg vc; memcpy_P(&vc, &V_CFG[vRange], sizeof(vc));
  att10x = vc.att;
  if (vRange >= 2) {
    float lsb = att10x ? LSB_50V : LSB_5V;
    // CHANGE: >>2 because waveBuff is now 8-bit (ADC>>2)
    rangeMax     = (int16_t)(vc.fsV / lsb) >> 2;
    rangeMaxDisp = vc.dMax;
    rangeMin = rangeMinDisp = 0;
  }
}

// ── Wave Recording ────────────────────────────────────────────────────────────
void readWave() {
  if (att10x) {
    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);
  } else {
    pinMode(12, INPUT);
  }
  switchPushed = false;

  HCfg hc;
  memcpy_P(&hc, &H_CFG[hRange], sizeof(hc));
  setADC(hc.adcBits);
  timeExec = hc.execMs;

  if (hRange <= 7) {
    for (int i = 0; i < REC_LEN; i++) {
      waveBuff[i] = (uint8_t)(analogRead(0) >> 2);
      if (hc.delayUs) delayMicroseconds(hc.delayUs);
      if (switchPushed) {
        switchPushed = false;
        break;
      }
    }
  } else if (hRange == 8) {
    for (int i = 0; i < REC_LEN; i++) {
      waveBuff[i] = (uint8_t)(analogRead(0) >> 2);
      delayMicroseconds(4);
      asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");
    }
  } else {
    for (int i = 0; i < REC_LEN; i++) {
      waveBuff[i] = (uint8_t)(analogRead(0) >> 2);
      asm volatile(
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");
    }
  }
}

// ── Data Analysis ─────────────────────────────────────────────────────────────
void dataAnalize() {
  long sum = 0;
  dataMin = 255; dataMax = 0;           // 8-bit range now

  for (int i = 0; i < REC_LEN; i++) {
    uint8_t d = waveBuff[i];
    sum += d;
    if (d < dataMin) dataMin = d;
    if (d > dataMax) dataMax = d;
  }
  dataAve = (int16_t)((sum + 10) / 20); // still 10× avg, fits int16_t

  if (vRange <= 1) {
    // CHANGE: scaled for 8-bit (divide thresholds by 4)
    rangeMin = (uint8_t)max(0,   ((dataMin - 5) / 2) * 2);
    rangeMax = (uint8_t)min(255, (((dataMax + 5) / 2) + 1) * 2);
    float lsb4 = (att10x ? LSB_50V : LSB_5V) * 4.0f; // ×4 for >>2 shift
    rangeMaxDisp = (int16_t)(100.0f * rangeMax * lsb4);
    rangeMinDisp = (int16_t)(100.0f * rangeMin * lsb4);
  }

  uint8_t mid = (dataMax + dataMin) / 2;
  trigSync = false;
  for (trigP = (REC_LEN/2-51); trigP < (REC_LEN/2+50); trigP++) {
    bool posi = !trigD && waveBuff[trigP-1] <  mid && waveBuff[trigP] >= mid;
    bool nega =  trigD && waveBuff[trigP-1] >  mid && waveBuff[trigP] <= mid;
    if (posi || nega) { trigSync = true; break; }
  }
  if (!trigSync) trigP = REC_LEN / 2;
  if ((dataMax - dataMin) <= MIN_TRIG_SWING) trigSync = false;
  // REMOVED: freqDuty() call
}

// ── Display ───────────────────────────────────────────────────────────────────
void writeCommonImage() {
  // clearDisplay() NOT needed — page loop auto-clears each pass

  oled.setCursor(85, 6);               // y: 0→6
  oled.print(F("av    v"));            // println→print

  oled.drawVLine(26,  9, 55);
  oled.drawVLine(127, 9,  3);
  oled.drawVLine(127,61,  3);
  oled.drawHLine(24, 36,  2);

  const uint8_t hx[] PROGMEM = {24,51,76,101,123};
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t x = pgm_read_byte(&hx[i]);
    uint8_t w = (i==0 || i==4) ? 7 : 3;
    oled.drawHLine(x,  9, w);
    oled.drawHLine(x, 63, w);
  }
  for (int x = 26;  x <= 128; x += 5)  oled.drawHLine(x, 36, 2);
  for (int x = 102; x > 30;   x -= 25)
    for (int y = 10; y < 63; y += 5)   oled.drawVLine(x, y, 2);
}

void plotData() {
  for (int x = 0; x <= 98; x++) {
    int y1 = constrain(map(waveBuff[x   + trigP - 50], rangeMin, rangeMax, 63, 9), 9, 63);
    int y2 = constrain(map(waveBuff[x+1 + trigP - 50], rangeMin, rangeMax, 63, 9), 9, 63);
    oled.drawLine(x+27, y1, x+28, y2);    // ← removed WHITE param
  }
}

// Print voltage value, reusing chrBuff (saves repeated dtostrf call)
static void printVoltAt(int16_t dispVal100, uint8_t col, uint8_t row) {
  float v = dispVal100 / 100.0f;
  bool sm = (vRange == 1 || vRange > 4);
  dtostrf(v, 4, sm ? 2 : 1, chrBuff);
  oled.setCursor(col, row + 6);     // ← +6 for U8g2 baseline
  oled.print(chrBuff);
}

void dispInf() {
  char buf[6];

  memcpy_P(buf, vName[vRange], 5); buf[4] = 0;
  oled.setCursor(2, 6);            // y: 0→6
  oled.print(buf);
  if (scopeP == 0) {
    oled.drawHLine(0,  7, 27);
    oled.drawVLine(0,  5,  2);
    oled.drawVLine(26, 5,  2);
  }

  memcpy_P(buf, hName[hRange], 6); buf[5] = 0;
  oled.setCursor(34, 6);           // y: 0→6
  oled.print(buf);
  if (scopeP == 1) {
    oled.drawHLine(32, 7, 33);
    oled.drawVLine(32, 5,  2);
    oled.drawVLine(64, 5,  2);
  }

  // Trigger direction: draw a small up/down arrow with lines
  // (U8g2 standard fonts don't have 0x18/0x19 glyphs)
  oled.setCursor(75, 6);           // y: 0→6
  if (trigD == 0) oled.print(F("/R"));   // Rising
  else            oled.print(F("\\F"));  // Falling
  if (scopeP == 2) {
    oled.drawHLine(71, 7, 13);
    oled.drawVLine(71, 5,  2);
    oled.drawVLine(83, 5,  2);
  }

  float lsb4 = (att10x ? LSB_50V : LSB_5V) * 4.0f;
  float vAvg  = dataAve * lsb4 / 10.0f;
  dtostrf(vAvg, 4, vAvg < 10.0f ? 2 : 1, chrBuff);
  oled.setCursor(98, 6);           // y: 0→6
  oled.print(chrBuff);

  printVoltAt(rangeMaxDisp,                         0,  9);
  printVoltAt((rangeMaxDisp + rangeMinDisp) / 2,    0, 33);
  printVoltAt(rangeMinDisp,                         0, 57);

  if (!trigSync) {
    oled.setCursor(92, 20);        // y: 14→20
    oled.print(F("~"));
  }
}

void startScreen() {
  oled.firstPage();
  do {
    oled.setFont(u8g2_font_5x7_tf);
    oled.setCursor(55,  6);   oled.print(F("Mini"));
    oled.setCursor(30, 26);   oled.print(F("Oscilloscope"));
    oled.setCursor(55, 48);   oled.print(F("v1.1"));
  } while (oled.nextPage());
  delay(1500);
}

// ── EEPROM ────────────────────────────────────────────────────────────────────
void saveEEPROM() {
  if (saveTimer <= 0) return;
  saveTimer -= timeExec;
  if (saveTimer < 0) {
    EEPROM.write(0, vRange);
    EEPROM.write(1, hRange);
    EEPROM.write(2, trigD);
    EEPROM.write(3, scopeP);
  }
}

void loadEEPROM() {
  auto clamp = [](uint8_t v, uint8_t hi, uint8_t def) -> int8_t {
    return v > hi ? def : v;
  };
  vRange = clamp(EEPROM.read(0), 9, 3);
  hRange = clamp(EEPROM.read(1), 9, 3);
  trigD = clamp(EEPROM.read(2), 1, 1);
  scopeP = clamp(EEPROM.read(3), 2, 1);
}

// ── Interrupt ─────────────────────────────────────────────────────────────────
void pin2IRQ() {
  uint8_t x = PINB;
  if ((x & 0x07) != 0x07) { saveTimer = 5000; switchPushed = true; }
  if (!(x & 0x01)) scopeP = scopeP < 2 ? scopeP + 1 : 0;
  if (!(x & 0x02)) {
    if (scopeP==0 && vRange<9) vRange++;
    if (scopeP==1 && hRange<9) hRange++;
    if (scopeP==2) trigD = 0;
  }
  if (!(x & 0x04)) {
    if (scopeP==0 && vRange>0) vRange--;
    if (scopeP==1 && hRange>0) hRange--;
    if (scopeP==2) trigD = 1;
  }
  // REMOVED: hold toggle (pin 0x08)
}