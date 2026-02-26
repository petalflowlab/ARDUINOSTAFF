// =====================================================================
// PurpleSword.ino — Mirrored IMU-driven LED Sword (OTA SAFE)
// =====================================================================
//
// LED layout (86 total, 43 per side):
//   leds[0..42]  = Side A, hilt→tip
//   leds[43..85] = Side B, tip→hilt
//   bladeSet(pos, color) writes pos on A and the mirror on B simultaneously.
//
// IMU axes (MPU6050/9250 at 0x68):
//   az > 0   → tip pointing up   → chaser moves tip-ward
//   az < 0   → tip pointing down → chaser moves hilt-ward
//   gx/gy    → swing magnitude   → boosts chaser speed / ripple energy
//   gz       → twist rate        → shifts hue
//
//   *** If directions feel inverted, negate az/gz below in updateIMU(). ***
//
// Button (D8): cycles Chase → Pulse → Ripple → Chase
// =====================================================================

#include <FastLED.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <esp_now.h>

// ─── PIN CONFIGURATION ────────────────────────────────────────────────
#define LED_PIN       D7
#define ACCENT_PIN    D2
#define BUTTON_PIN    D3
#define SDA_PIN       D4
#define SCL_PIN       D5

// ─── LED CONFIGURATION ────────────────────────────────────────────────
#define NUM_LEDS      86
#define BLADE_HALF    43      // logical positions: 0=hilt, 42=tip
#define BRIGHTNESS    180
#define OTA_BRIGHTNESS 60

// ─── IMU ──────────────────────────────────────────────────────────────
#define IMU_ADDR      0x68

// ─── WIFI / OTA ───────────────────────────────────────────────────────
const char* ssid      = "CGN3-4400";
const char* password  = "251148015432";
const char* hostname  = "PurpleSword";
const char* otaPass   = "sword";
WebServer   server(80);

bool     otaActive   = false;
uint16_t otaProgress = 0;

// ─── GLOBAL LED ARRAY ─────────────────────────────────────────────────
CRGB leds[NUM_LEDS];

// ─── IMU STATE ────────────────────────────────────────────────────────
bool  imuOk    = false;
float swingMag = 0.0f;   // filtered transverse gyro magnitude (deg/s)
float twistRate= 0.0f;   // filtered axial gyro gz (deg/s)
float accelZ   = 0.0f;   // filtered accel along blade axis (g) — drives painter
int8_t tiltDir =  1;     // +1 = tip-up → chase toward tip; -1 = tip-down

// ─── ANIMATION STATE ──────────────────────────────────────────────────
uint8_t  effectMode   = 0;     // 0=Chase  1=Pulse  2=Ripple
uint8_t  baseHue      = 190;   // purple-blue base (~190 in FastLED HSV)

float    chasePos     = 0.0f;
uint8_t  topMode      = 0;     // 0=Chase/Pulse/Ripple  1=Painter
uint8_t  clickCount   = 0;     // button clicks waiting to be dispatched
uint32_t lastClickMs  = 0;     // millis() of most recent press
bool     lastButton   = HIGH;
#define  DCLICK_MS    350      // double-click detection window (ms)

bool     boostMode    = false; // true while D8 is held > BOOST_HOLD_MS
uint32_t buttonDownMs = 0;     // millis() when button last went LOW
#define  BOOST_HOLD_MS 500     // hold threshold (ms)
#define  SYNC_ANIM_MS  2000    // triple-click+hold animation duration (ms)
#define  SYNC_MSG_MODE   0x01    // ESP-NOW packet: mode changed
#define  SYNC_MSG_PING   0x02    // ESP-NOW packet: presence beacon
#define  SYNC_SEARCH_MS  60000   // discovery window: 60 seconds

// ─── G-FORCE BLOB PHYSICS ─────────────────────────────────────────────
// A bright violet blob is pushed toward the tip by swing force.
// Spring pulls it back to hilt; slightly underdamped so it oscillates on return.
// Rendered additively on top of whichever effect is active.
//
// Tuning knobs:
//   GBLOB_SPRING — higher = snappier return, shorter oscillation (0.010–0.025)
//   GBLOB_DAMP   — higher = less overshoot bounce on return  (0.88–0.96)
//   GBLOB_FORCE  — push strength per deg/s above deadband    (0.003–0.008)
//   GBLOB_KICK   — swing deadband: ignore below this deg/s
// ──────────────────────────────────────────────────────────────────────
#define GBLOB_SPRING  0.015f
#define GBLOB_DAMP    0.93f
#define GBLOB_FORCE   0.005f
#define GBLOB_KICK    20.0f

float gBlobPos = 0.0f;   // current position  0=hilt  BLADE_HALF-1=tip
float gBlobVel = 0.0f;   // current velocity (blade units/frame)

// ─── SWORD PAINTER STATE ──────────────────────────────────────────────
struct {
  float buf[BLADE_HALF];  // per-pixel hue (0–255 float)
  float paintCenter;      // 0=hilt, 1=tip (normalised)
  float paintVelocity;
  float centerHue;
  float domHue;           // captured hue when twist starts
  float domFade;          // 0=off, 2=full
  float domWidth;         // half-width of dominant zone (pixels)
  float shiftAccum;       // fractional band-shift accumulator
  bool  ready;
} pnt;                    // zero-init by C++ default for globals

// Compression state: driven by swing, squashes hue bands toward tip
float pntCompressPos = 0.0f;  // 0=normal  1=all bands piled at tip
float pntCompressVel = 0.0f;

// ─── BOUNCING BALL STATE ──────────────────────────────────────────────
#define NUM_BALLS 4
struct BallState {
  float   pos;    // blade position: 0=hilt  BLADE_HALF-1=tip
  float   vel;    // velocity (blade units/frame)
  uint8_t hue;    // colour hue (0=white normally, colourful after collision)
  uint8_t sat;    // saturation: 0=white → 255=full colour
  uint8_t width;  // gaussian radius (pixels)
  float   flash;  // explosion flash intensity 0–1 (decays each frame)
};
BallState balls[NUM_BALLS];
bool      ballsReady  = false;
float     whirlPhase  = 0.0f;   // accumulates twist → whirlwind hue wave
float     stabImpulse = 0.0f;   // one-shot stab push magnitude
float     prevAZ      = 0.0f;   // previous accelZ sample for derivative

// ─── ESP-NOW SYNC STATE ────────────────────────────────────────────────
bool      syncEnabled    = false;   // true = paired with another sword
bool      syncSearching  = false;   // true during 60-second discovery window
uint32_t  syncSearchStart = 0;
uint32_t  lastPingMs     = 0;
bool      syncAnimating  = false;   // true during 2-second arm animation
uint32_t  syncAnimStart  = 0;
volatile bool    syncGotPacket = false;
volatile uint8_t syncInType    = 0;
volatile uint8_t syncInMode    = 0;
struct SyncPacket { uint8_t type; uint8_t mode; };
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


// =====================================================================
// BLADE HELPERS
// =====================================================================

// Write colour to logical blade position pos (0=hilt, BLADE_HALF-1=tip)
// on BOTH sides simultaneously — this is the only function that touches leds[].
inline void bladeSet(int pos, CRGB color) {
  if (pos < 0 || pos >= BLADE_HALF) return;
  leds[pos]                  = color;
  leds[NUM_LEDS - 1 - pos]   = color;
}

void bladeClear() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}


// =====================================================================
// IMU (MPU6050 / MPU9250 raw I2C)
// =====================================================================

static void imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static int16_t imuRead16(uint8_t reg) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, (uint8_t)2);
  return (int16_t)((Wire.read() << 8) | Wire.read());
}

void setupIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);   // 100 kHz — reliable for MPU6050 initial contact
  delay(150);              // MPU6050 needs up to 100 ms after power-on

  // ── Step 1: I2C presence check — does 0x68 ACK at all? ───────────
  Wire.beginTransmission(0x68);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.printf("MPU6050: no ACK at 0x68 (I2C error %u) — check SDA/SCL wiring\n", err);
    Wire.beginTransmission(0x69);
    if (Wire.endTransmission() == 0)
      Serial.println("  → found something at 0x69 — is AD0 pin HIGH? Change IMU_ADDR to 0x69");
    imuOk = false;
    return;
  }

  // ── Step 2: Read WHO_AM_I — MPU6050 must return 0x68 ─────────────
  Wire.beginTransmission(0x68);
  Wire.write(0x75);
  err = Wire.endTransmission(false);
  if (err != 0) {
    Serial.printf("MPU6050: WHO_AM_I write failed (I2C error %u)\n", err);
    imuOk = false;
    return;
  }
  Wire.requestFrom((uint8_t)0x68, (uint8_t)1);
  uint8_t id = Wire.available() ? Wire.read() : 0xFF;
  if (id != 0x68) {
    Serial.printf("MPU6050: WHO_AM_I = 0x%02X, expected 0x68 — not an MPU6050\n", id);
    imuOk = false;
    return;
  }

  // ── Step 3: Wake from sleep (PWR_MGMT_1 = 0x00) ──────────────────
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);   // let sensor stabilise after wake

  // ── Step 4: Configure ranges ──────────────────────────────────────
  Wire.beginTransmission(0x68);
  Wire.write(0x1B);
  Wire.write(0x08);   // Gyro ±500 dps
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x00);   // Accel ±2 g
  Wire.endTransmission();

  imuOk = true;
  Serial.println("MPU6050: found and configured at 0x68");
}

void updateIMU() {
  if (!imuOk) return;

  // Accel registers 0x3B–0x40 (AX, AY, AZ)
  accelZ = imuRead16(0x3F) / 16384.0f;     // ±2g scale → g (global, used by painter)

  // Gyro registers 0x43–0x48 (GX, GY, GZ)
  float gx = imuRead16(0x43) / 65.5f;      // ±500dps scale → deg/s
  float gy = imuRead16(0x45) / 65.5f;
  float gz = imuRead16(0x47) / 65.5f;

  // Swing = rotation perpendicular to blade axis (gx, gy)
  float swing = sqrtf(gx*gx + gy*gy);
  swingMag   = swingMag   * 0.8f + swing * 0.2f;
  twistRate  = twistRate  * 0.8f + gz    * 0.2f;

  // Tilt direction with hysteresis — change only beyond ±0.2g deadband
  // Negate accelZ here if chase direction feels backwards on your hardware.
  if      (accelZ >  0.2f) tiltDir =  1;
  else if (accelZ < -0.2f) tiltDir = -1;

  // Stab detection: sharp positive derivative of accelZ (thrust tip-ward)
  float dAZ   = accelZ - prevAZ;
  stabImpulse = fmaxf(0.0f, dAZ - 0.30f);
  prevAZ      = accelZ;
}


// =====================================================================
// EFFECT 0 — CHASER
// A glowing blob races up or down the blade.
// • Tilt drives direction (tiltDir)
// • Swing boosts speed  (swingMag)
// • Twist shifts hue    (twistRate)
// =====================================================================
void effectChaser() {
  // Fade the whole strip for a trailing glow
  for (int i = 0; i < NUM_LEDS; i++) leds[i].nscale8(200);

  float speed = constrain(0.25f + swingMag * 0.035f, 0.10f, boostMode ? 4.5f : 2.5f);
  if (boostMode) speed *= 1.6f;
  chasePos += (float)tiltDir * speed;

  // Wrap at blade ends
  if (chasePos >= BLADE_HALF) chasePos = 0.0f;
  if (chasePos <  0.0f)       chasePos = BLADE_HALF - 1.0f;

  uint8_t hue = baseHue + (int8_t)(twistRate * 0.4f);

  // Gaussian blob centred on chasePos
  int   centre = (int)chasePos;
  float frac   = chasePos - centre;

  for (int d = -5; d <= 5; d++) {
    int pos = centre + d;
    if (pos < 0 || pos >= BLADE_HALF) continue;
    float dist   = fabsf((float)d + frac);
    uint8_t bri  = (uint8_t)(255.0f * expf(-0.35f * dist * dist));
    // Additive blend onto the fade layer to preserve trail
    leds[pos]                += CHSV(hue, 220, bri);
    leds[NUM_LEDS - 1 - pos] += CHSV(hue, 220, bri);
  }
}


// =====================================================================
// EFFECT 1 — PULSE WAVE
// A sine wave travels the full blade length.
// • Tilt reverses travel direction
// • Swing speeds up the wave
// • Twist shifts hue
// =====================================================================
void effectPulse() {
  static float pulsePhase = 0.0f;

  float speed = 0.04f + swingMag * 0.008f;
  pulsePhase += speed * (float)tiltDir;
  if (pulsePhase >  TWO_PI) pulsePhase -= TWO_PI;
  if (pulsePhase < -TWO_PI) pulsePhase += TWO_PI;

  uint8_t hue = baseHue + (int8_t)(twistRate * 0.4f);

  for (int i = 0; i < BLADE_HALF; i++) {
    float t    = (float)i / (float)(BLADE_HALF - 1);
    float wave = sinf(t * TWO_PI * 1.5f + pulsePhase);
    uint8_t bri = (uint8_t)(((wave + 1.0f) * 0.5f) * 210.0f + 30.0f);
    uint8_t sat = (uint8_t)(180.0f + fabsf(wave) * 70.0f);
    bladeSet(i, CHSV(hue, sat, bri));
  }
}


// =====================================================================
// EFFECT 2 — RIPPLE
// Swings launch bright pulses that travel along the blade.
// • Tilt sets which way the ripples travel
// • Swing magnitude determines ripple energy (speed + brightness)
// • Twist shifts hue
// =====================================================================

#define MAX_RIPPLES 4
static float   ripplePos[MAX_RIPPLES];
static float   rippleSpd[MAX_RIPPLES];
static uint8_t rippleHue[MAX_RIPPLES];
static int     rippleNext = 0;
static float   lastSwingForRipple = 0.0f;

void effectRipple() {
  // Fade trail
  for (int i = 0; i < NUM_LEDS; i++) leds[i].nscale8(210);

  uint8_t hue = baseHue + (int8_t)(twistRate * 0.4f);

  // Rising edge of swing threshold fires a new ripple
  if (swingMag > 25.0f && lastSwingForRipple <= 25.0f) {
    ripplePos[rippleNext] = 0.0f;
    rippleSpd[rippleNext] = constrain(0.4f + swingMag * 0.025f, 0.3f, 2.0f);
    rippleHue[rippleNext] = hue + random8(30) - 15;
    rippleNext = (rippleNext + 1) % MAX_RIPPLES;
  }
  lastSwingForRipple = swingMag;

  // Advance and draw each active ripple
  for (int r = 0; r < MAX_RIPPLES; r++) {
    if (ripplePos[r] < 0.0f) continue;

    ripplePos[r] += rippleSpd[r] * (float)tiltDir;

    if (ripplePos[r] >= BLADE_HALF || ripplePos[r] < 0.0f) {
      ripplePos[r] = -1.0f;
      continue;
    }

    int   p    = (int)ripplePos[r];
    float frac = ripplePos[r] - p;

    // Sub-pixel rendering between p and p+1
    leds[p]                  += CHSV(rippleHue[r], 235, (uint8_t)(220.0f * (1.0f - frac)));
    leds[NUM_LEDS - 1 - p]   += CHSV(rippleHue[r], 235, (uint8_t)(220.0f * (1.0f - frac)));
    if (p + 1 < BLADE_HALF) {
      leds[p + 1]              += CHSV(rippleHue[r], 235, (uint8_t)(220.0f * frac));
      leds[NUM_LEDS - 2 - p]   += CHSV(rippleHue[r], 235, (uint8_t)(220.0f * frac));
    }
  }
}


// =====================================================================
// EFFECT 3 — SWORD PAINTER
// Port of C3Staff RainbowPainterEffect, adapted for a 43-LED mirrored blade.
//
// • accelZ (tilt along blade) drives the paint-blob position
// • twistRate (gz) shifts hue bands sideways and grows a dominant-colour zone
// • Fast motion generates paint-drop splatter
// • bladeSet() ensures both sides stay in sync
// =====================================================================

void effectPainter() {
  const float dt = 0.016f;

  if (!pnt.ready) {
    for (int i = 0; i < BLADE_HALF; i++)
      pnt.buf[i] = (float)(i * 256) / BLADE_HALF;
    pnt.paintCenter   = 0.5f;
    pnt.paintVelocity = 0.0f;
    pnt.centerHue     = (float)baseHue;
    pnt.domFade       = 0.0f;
    pnt.domWidth      = 0.0f;
    pnt.shiftAccum    = 0.0f;
    pnt.ready         = true;
  }

  // ── Move paint-blob along blade via tilt (accelZ) ────────────────────
  if (imuOk) {
    float ty = constrain(accelZ * 0.4f, -1.0f, 1.0f);
    pnt.paintVelocity = pnt.paintVelocity * 0.85f + ty * dt * 6.0f;
    pnt.paintCenter  += pnt.paintVelocity * dt;
    if (pnt.paintCenter < 0.05f) { pnt.paintCenter = 0.05f; pnt.paintVelocity = -pnt.paintVelocity * 0.7f; }
    if (pnt.paintCenter > 0.95f) { pnt.paintCenter = 0.95f; pnt.paintVelocity = -pnt.paintVelocity * 0.7f; }
  } else {
    static float dp = 0.0f;
    dp += dt * 0.5f;
    pnt.paintCenter   = 0.5f + sinf(dp) * 0.25f;
    pnt.paintVelocity = cosf(dp) * 0.1f;
  }

  // ── Hue drift ─────────────────────────────────────────────────────────
  pnt.centerHue += dt * 80.0f;
  if (pnt.centerHue >= 256.0f) pnt.centerHue -= 256.0f;

  int ci = constrain((int)(pnt.paintCenter * BLADE_HALF), 0, BLADE_HALF - 1);
  int br = max(3, (int)((0.12f + fabsf(pnt.paintVelocity) * 0.15f) * BLADE_HALF));

  // ── Twist → shift hue bands sideways + capture dominant zone ─────────
  bool twisting = imuOk && fabsf(twistRate) > 30.0f;
  if (twisting) {
    if (pnt.domFade <= 0.0f) pnt.domHue = pnt.buf[ci];
    pnt.domFade    = 2.0f;
    pnt.domWidth   = constrain(pnt.domWidth + dt * fabsf(twistRate) * 0.05f,
                               0.0f, (float)(BLADE_HALF / 2 - 2));
    pnt.shiftAccum += twistRate * dt * 0.12f;
  } else {
    pnt.domFade    = constrain(pnt.domFade  - dt,      0.0f, 2.0f);
    pnt.domWidth  *= (1.0f - dt * 0.4f);
    pnt.shiftAccum *= (1.0f - dt * 3.0f);
  }
  while (pnt.shiftAccum >= 1.0f) {
    float sv = pnt.buf[BLADE_HALF - 1];
    for (int i = BLADE_HALF - 1; i > 0; i--) pnt.buf[i] = pnt.buf[i - 1];
    pnt.buf[0] = sv;
    pnt.shiftAccum -= 1.0f;
  }
  while (pnt.shiftAccum <= -1.0f) {
    float sv = pnt.buf[0];
    for (int i = 0; i < BLADE_HALF - 1; i++) pnt.buf[i] = pnt.buf[i + 1];
    pnt.buf[BLADE_HALF - 1] = sv;
    pnt.shiftAccum += 1.0f;
  }

  // ── Paint: blend pixels near brush toward centerHue ───────────────────
  for (int i = 0; i < BLADE_HALF; i++) {
    int d = abs(i - ci);
    if (d > br) continue;
    float ms = 1.0f - (float)d / (br + 1); ms = ms * ms * ms;
    float hd = pnt.centerHue - pnt.buf[i];
    if (hd >  128.0f) hd -= 256.0f;
    if (hd < -128.0f) hd += 256.0f;
    pnt.buf[i] += hd * ms * dt * (10.0f + fabsf(pnt.paintVelocity) * 15.0f);
    while (pnt.buf[i] >= 256.0f) pnt.buf[i] -= 256.0f;
    while (pnt.buf[i] <    0.0f) pnt.buf[i] += 256.0f;
  }

  // ── Diffusion: gentle hue spread to neighbours ────────────────────────
  static float tb[BLADE_HALF];
  memcpy(tb, pnt.buf, sizeof(tb));
  float diffR = 0.03f * dt;
  for (int i = 1; i < BLADE_HALF - 1; i++) {
    float ld = tb[i-1] - tb[i], rd = tb[i+1] - tb[i];
    if (ld >  128.0f) ld -= 256.0f; if (ld < -128.0f) ld += 256.0f;
    if (rd >  128.0f) rd -= 256.0f; if (rd < -128.0f) rd += 256.0f;
    pnt.buf[i] += (ld + rd) * diffR;
    while (pnt.buf[i] >= 256.0f) pnt.buf[i] -= 256.0f;
    while (pnt.buf[i] <    0.0f) pnt.buf[i] += 256.0f;
  }

  // ── Dominant zone: pull buffer toward snapped hue ─────────────────────
  if (pnt.domFade > 0.0f && pnt.domWidth > 0.5f) {
    float str = constrain(pnt.domFade / 2.0f, 0.0f, 1.0f);
    int dw    = (int)(pnt.domWidth + 0.5f);
    for (int i = max(0, ci - dw); i <= min(BLADE_HALF - 1, ci + dw); i++) {
      float dist = fabsf((float)(i - ci)) / (float)(dw + 1);
      float inf  = (1.0f - dist * dist) * str * 0.5f;
      float hd   = pnt.domHue - pnt.buf[i];
      if (hd >  128.0f) hd -= 256.0f;
      if (hd < -128.0f) hd += 256.0f;
      pnt.buf[i] += hd * inf;
      while (pnt.buf[i] >= 256.0f) pnt.buf[i] -= 256.0f;
      while (pnt.buf[i] <    0.0f) pnt.buf[i] += 256.0f;
    }
  }

  // ── Compression physics ────────────────────────────────────────────────
  // Hard swing squashes hue bands toward tip using a power-curve remap.
  // Deadband at 40 dps so casual handling doesn't fire; hard slashes slam it.
  // pnt.buf[] stays untouched — compression only changes how it is sampled.
  {
    float excess = fmaxf(0.0f, swingMag - 40.0f);
    float push   = excess * (boostMode ? 0.005f : 0.0025f);
    push        += stabImpulse * 0.8f;   // stab slams bands to tip
    float spring = -0.035f * pntCompressPos;
    pntCompressVel = pntCompressVel * 0.88f + push + spring;
    pntCompressPos += pntCompressVel;
    if (pntCompressPos > 1.0f) { pntCompressPos = 1.0f; pntCompressVel = -fabsf(pntCompressVel) * 0.25f; }
    if (pntCompressPos < 0.0f) { pntCompressPos = 0.0f; pntCompressVel =  fabsf(pntCompressVel) * 0.10f; }
  }

  // ── Render to mirrored blade ────────────────────────────────────────────
  // Power-curve remap: hilt stays at its own colour; all bands pile toward tip.
  //   power=1 (no compress) → linear, normal display
  //   power=3 (full compress) → bottom ~half shows first band; top ~half = all bands crushed together
  // "Stretching band" analogy: grip the hilt, push the tip end — bands scrunch at tip.
  uint32_t now = millis();
  float compPower = 1.0f + pntCompressPos * 2.0f + (boostMode ? 0.6f : 0.0f);

  for (int i = 0; i < BLADE_HALF; i++) {
    // Compressed sample index via power curve
    float t    = (float)i / (float)(BLADE_HALF - 1);
    float fidx = (i == 0) ? 0.0f : powf(t, compPower) * (float)(BLADE_HALF - 1);
    int   lo   = (int)fidx;
    int   hix  = min(lo + 1, BLADE_HALF - 1);
    float ifrc = fidx - lo;

    // Shortest-path hue interpolation (wraps at 0/256 boundary)
    float h0 = pnt.buf[lo], h1 = pnt.buf[hix];
    float hd = h1 - h0;
    if (hd >  128.0f) hd -= 256.0f;
    if (hd < -128.0f) hd += 256.0f;
    float fhue = h0 + hd * ifrc;
    while (fhue >= 256.0f) fhue -= 256.0f;
    while (fhue <    0.0f) fhue += 256.0f;
    uint8_t ph = (uint8_t)fhue;

    uint8_t s = 255;
    uint8_t v = (uint8_t)(120.0f * (sinf(i * 0.2f + now * 0.0008f) * 0.3f + 0.7f));

    // Compression brightens blade and slightly bleaches colour (neon crushed look)
    float c2 = pntCompressPos * pntCompressPos;
    v = (uint8_t)constrain((int)v + (int)(c2 * 110.0f), 0, 255);
    s = (uint8_t)constrain((int)s - (int)(pntCompressPos * 75.0f), 155, 255);

    // Paint brush glow
    int d = abs(i - ci);
    if (d <= br) {
      float bhi = 1.0f - (float)d / (br + 1); bhi *= bhi;
      v = (uint8_t)constrain((int)(v + bhi * 120), 0, 255);
      if (fabsf(pnt.paintVelocity) > 0.15f) {
        if (sinf(now * 0.012f + i * 0.4f) > 0.5f) { v = 255; s = (uint8_t)constrain((int)s - 80, 120, 255); }
      }
      if (d < 2) { v = 255; s = (uint8_t)constrain((int)s - 50, 150, 255); }
    }
    if (pnt.domFade > 0.0f) {
      int dw = (int)(pnt.domWidth + 0.5f);
      if (d <= dw) {
        float dist  = (float)d / (float)(dw + 1);
        float boost = (1.0f - dist * dist) * constrain(pnt.domFade / 2.0f, 0.0f, 1.0f);
        v = (uint8_t)constrain((int)(v + boost * 55.0f), 0, 255);
      }
    }
    bladeSet(i, CHSV(ph, s, v));
  }

  // ── Paint-drop splatter when moving fast ──────────────────────────────
  if (fabsf(pnt.paintVelocity) > 0.2f && random8() < 70) {
    int dd = (pnt.paintVelocity > 0) ? -1 : 1;
    int di = ci + dd * (br + (int)random(3));
    if (di >= 0 && di < BLADE_HALF) {
      float dh = pnt.centerHue + (float)random(-20, 21);
      while (dh >= 256.0f) dh -= 256.0f;
      while (dh <    0.0f) dh += 256.0f;
      CRGB drp = CHSV((uint8_t)dh, 255, 200);
      leds[di]                 = blend(leds[di],                 drp, 180);
      leds[NUM_LEDS - 1 - di]  = blend(leds[NUM_LEDS - 1 - di],  drp, 180);
      if (di > 0 && di < BLADE_HALF - 1) {
        leds[di - 1]             = blend(leds[di - 1],             drp, 120);
        leds[NUM_LEDS - di]      = blend(leds[NUM_LEDS - di],      drp, 120);
        leds[di + 1]             = blend(leds[di + 1],             drp, 120);
        leds[NUM_LEDS - 2 - di]  = blend(leds[NUM_LEDS - 2 - di],  drp, 120);
      }
    }
  }
}


// =====================================================================
// BOUNCING BALL MODE (topMode == 2)
// White balls physics-driven along the blade:
//  • Gravity from tilt (accelZ) — balls fall toward whichever end is down
//  • Swing impulse rushes balls toward tip (like g-force blob)
//  • Stab (sharp accelZ spike) slams all balls to tip
//  • Ball–ball elastic collisions with colorful explosion flash
//  • Twist → whirlwind sine-wave of hue across the background
//  • Boost: faster balls, bigger explosions, scattered sparks
// =====================================================================

static void spawnExplosion(BallState& a, BallState& b) {
  uint8_t eh = random8();
  a.hue = eh;
  b.hue = eh + 128;
  a.sat = 255;  b.sat = 255;
  a.flash = 1.0f; b.flash = 1.0f;
  a.width = 5;    b.width = 5;
}

void initBalls() {
  for (int b = 0; b < NUM_BALLS; b++) {
    balls[b].pos   = (float)(b + 1) * ((float)BLADE_HALF / (NUM_BALLS + 1.0f));
    balls[b].vel   = 0.0f;
    balls[b].hue   = 0;
    balls[b].sat   = 0;
    balls[b].width = 2;
    balls[b].flash = 0.0f;
  }
  whirlPhase = 0.0f;
  ballsReady = true;
}

void effectBalls() {
  if (!ballsReady) initBalls();

  // ── Whirlwind: accumulate twist → animated hue wave ──────────────────
  float twistAbs = fabsf(twistRate);
  float whirlStr = constrain((twistAbs - 20.0f) / 150.0f, 0.0f, 1.0f);
  whirlPhase += twistRate * 0.016f * 0.15f;
  if (whirlPhase >  TWO_PI) whirlPhase -= TWO_PI;
  if (whirlPhase < -TWO_PI) whirlPhase += TWO_PI;

  // ── Background: dark base + whirlwind hue wash ───────────────────────
  for (int i = 0; i < BLADE_HALF; i++) {
    float  wave = (sinf((float)i * 0.3f + whirlPhase) + 1.0f) * 0.5f;
    uint8_t h   = (uint8_t)((float)baseHue + wave * 60.0f);
    uint8_t s   = (uint8_t)(60.0f  + whirlStr * 200.0f);
    uint8_t v   = (uint8_t)(10.0f  + whirlStr * 45.0f);
    bladeSet(i, CHSV(h, s, v));
  }

  // ── Physics forces ────────────────────────────────────────────────────
  // Gravity: tip-up (accelZ>0) → balls fall toward hilt (vel decreases)
  float gravity   = -accelZ * 0.35f * (boostMode ? 1.5f : 1.0f);

  // Swing impulse: sustained swing above 30 dps rushes balls toward tip
  static float lastSwingBall = 0.0f;
  float swingKick = fmaxf(0.0f, swingMag - 30.0f) * 0.010f * (boostMode ? 1.8f : 1.0f);
  lastSwingBall   = swingMag * 0.95f;

  // Stab: sharp thrust rushes all balls hard toward tip
  float stabKick  = stabImpulse * 10.0f * (boostMode ? 1.5f : 1.0f);

  // ── Update each ball ──────────────────────────────────────────────────
  float vmax = boostMode ? 6.0f : 4.0f;
  for (int b = 0; b < NUM_BALLS; b++) {
    balls[b].vel += gravity + swingKick + stabKick;
    balls[b].vel  = constrain(balls[b].vel, -vmax, vmax);
    balls[b].pos += balls[b].vel;

    // Wall bounce at tip
    if (balls[b].pos >= (float)(BLADE_HALF - 1)) {
      balls[b].pos = (float)(BLADE_HALF - 1);
      balls[b].vel = -fabsf(balls[b].vel) * 0.70f;
      if (boostMode) { balls[b].flash = fmaxf(balls[b].flash, 0.5f); balls[b].sat = 180; }
    }
    // Wall bounce at hilt
    if (balls[b].pos <= 0.0f) {
      balls[b].pos = 0.0f;
      balls[b].vel =  fabsf(balls[b].vel) * 0.70f;
      if (boostMode) { balls[b].flash = fmaxf(balls[b].flash, 0.5f); balls[b].sat = 180; }
    }

    // Decay flash, saturation, and width back toward white/normal
    balls[b].flash *= 0.88f;
    balls[b].sat    = (uint8_t)((float)balls[b].sat * 0.96f);
    balls[b].width  = (uint8_t)constrain(2 + (int)(balls[b].flash * 4.0f), 2, 6);
  }

  // ── Ball–ball elastic collisions ─────────────────────────────────────
  for (int a = 0; a < NUM_BALLS - 1; a++) {
    for (int b = a + 1; b < NUM_BALLS; b++) {
      float dist = fabsf(balls[a].pos - balls[b].pos);
      if (dist < 3.0f) {
        float va      = balls[a].vel;
        float vb      = balls[b].vel;
        float closing = (balls[a].pos < balls[b].pos) ? (va - vb) : (vb - va);
        if (closing > 0.2f) {
          float bfac    = boostMode ? 1.3f : 1.0f;
          balls[a].vel  = vb * bfac;
          balls[b].vel  = va * bfac;
          // Physically separate so they don't stick
          float sep = (3.0f - dist) * 0.5f;
          if (balls[a].pos < balls[b].pos) { balls[a].pos -= sep; balls[b].pos += sep; }
          else                             { balls[a].pos += sep; balls[b].pos -= sep; }
          spawnExplosion(balls[a], balls[b]);
        }
      }
    }
  }

  // ── Render balls (Gaussian glow, additive) ───────────────────────────
  for (int b = 0; b < NUM_BALLS; b++) {
    float   bpos   = constrain(balls[b].pos, 0.0f, (float)(BLADE_HALF - 1));
    int     centre = (int)bpos;
    float   frac   = bpos - centre;
    int     radius = balls[b].width + 2;
    float   sigma  = (float)balls[b].width * 0.7f + 0.1f;

    // Whirlwind tints the balls
    float   waveH  = sinf(bpos * 0.3f + whirlPhase);
    uint8_t bHue   = (uint8_t)((float)balls[b].hue + waveH * whirlStr * 60.0f);
    uint8_t bSat   = (uint8_t)fminf((float)balls[b].sat + whirlStr * (255.0f - balls[b].sat) * 0.7f, 255.0f);
    uint8_t baseBri= (uint8_t)(180.0f + balls[b].flash * 75.0f);

    for (int d = -radius; d <= radius; d++) {
      int pos = centre + d;
      if (pos < 0 || pos >= BLADE_HALF) continue;
      float   dist    = fabsf((float)d + frac);
      float   falloff = expf(-0.5f * (dist / sigma) * (dist / sigma));
      uint8_t bri     = (uint8_t)((float)baseBri * falloff);
      if (bri < 4) continue;
      leds[pos]                += CHSV(bHue, bSat, bri);
      leds[NUM_LEDS - 1 - pos] += CHSV(bHue, bSat, bri);
    }

    // Explosion ring expands outward from collision point
    if (balls[b].flash > 0.15f) {
      int ring = (int)(balls[b].flash * 7.0f);
      for (int side = -1; side <= 1; side += 2) {
        int pos = centre + side * ring;
        if (pos >= 0 && pos < BLADE_HALF) {
          uint8_t rbri = (uint8_t)(balls[b].flash * 200.0f);
          leds[pos]                += CHSV(balls[b].hue + 128, 200, rbri);
          leds[NUM_LEDS - 1 - pos] += CHSV(balls[b].hue + 128, 200, rbri);
        }
      }
    }
  }

  // Boost mode: scatter sparks
  if (boostMode) {
    int sc = constrain(2 + (int)(swingMag * 0.04f), 2, 7);
    for (int s = 0; s < sc; s++) {
      int pos = random(BLADE_HALF);
      leds[pos]                += CHSV(random8(), random8(80, 220), random8(100, 230));
      leds[NUM_LEDS - 1 - pos] += CHSV(random8(), random8(80, 220), random8(100, 230));
    }
  }
}


// =====================================================================
// G-FORCE BLOB — physics update + render
// Called every frame AFTER the chosen effect, so it layers on top.
// =====================================================================

void updateBlobPhysics() {
  // Push force toward tip — scaled swing above deadband
  float push   = fmaxf(0.0f, swingMag - GBLOB_KICK) * GBLOB_FORCE * (boostMode ? 2.2f : 1.0f);
  push        += stabImpulse * 5.0f;   // stab gives instant tip kick

  // Spring force back toward hilt (rest position = 0)
  float spring = -GBLOB_SPRING * gBlobPos;

  // Integrate: apply damping then acceleration
  gBlobVel = gBlobVel * GBLOB_DAMP + push + spring;
  gBlobPos += gBlobVel;

  // Hard wall at tip — crush and rebound with 40% energy
  if (gBlobPos >= (float)(BLADE_HALF - 1)) {
    gBlobPos = (float)(BLADE_HALF - 1);
    gBlobVel = -fabsf(gBlobVel) * 0.40f;
  }
  // Hard wall at hilt
  if (gBlobPos < 0.0f) {
    gBlobPos = 0.0f;
    gBlobVel =  fabsf(gBlobVel) * 0.20f;
  }
}

void renderGBlob() {
  // In painter/balls mode the effect handles its own g-force visual
  if (topMode == 1 || topMode == 2) return;

  // Normalised position: 0 = hilt, 1 = tip
  float t = gBlobPos / (float)(BLADE_HALF - 1);

  // Invisible until the blob has moved meaningfully away from hilt
  if (t < 0.05f) return;

  // ── Width: wide and soft mid-blade, compressed thin at tip ──────────
  float width = fmaxf(4.5f - t * 3.0f, 1.2f);   // 4.5 → 1.5 → min 1.2

  // ── Brightness: quadratic — slow build, blazing when slammed to tip ─
  uint8_t maxBri = (uint8_t)fminf(t * t * 280.0f, 255.0f);

  // ── Colour: violet (hue 210) distinct from base (190) ───────────────
  // Saturation drops near tip → punchy neon white-violet ("crushed" look)
  uint8_t sat = (uint8_t)(255.0f - t * 95.0f);   // 255 at hilt → 160 at tip

  int   centre = (int)gBlobPos;
  float frac   = gBlobPos - centre;
  int   radius = (int)(width * 2.5f + 1);

  for (int d = -radius; d <= radius; d++) {
    int pos = centre + d;
    if (pos < 0 || pos >= BLADE_HALF) continue;
    float dist    = fabsf((float)d + frac);
    float falloff = expf(-0.5f * (dist / width) * (dist / width));
    uint8_t bri   = (uint8_t)((float)maxBri * falloff);
    if (bri < 4) continue;
    leds[pos]                += CHSV(210, sat, bri);
    leds[NUM_LEDS - 1 - pos] += CHSV(210, sat, bri);
  }
}


// =====================================================================
// BOOST MODE — spark overlay (called after main effect + gBlob)
// =====================================================================
void renderBoostSparks() {
  int count = constrain(3 + (int)(swingMag * 0.06f), 3, 10);
  for (int s = 0; s < count; s++) {
    int     pos = random(BLADE_HALF);
    uint8_t h   = baseHue + (int8_t)(random8() / 4 - 32);  // slight hue scatter
    uint8_t sat = random8(60, 200);                          // white→coloured range
    uint8_t bri = random8(160, 255);
    leds[pos]                += CHSV(h, sat, bri);
    leds[NUM_LEDS - 1 - pos] += CHSV(h, sat, bri);
  }
}


// =====================================================================
// ESP-NOW SYNC — receive callback, send helper, setup
// =====================================================================

// Called from ESP-NOW task context — only set flags, no heavy work.
void onSyncReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len < (int)sizeof(SyncPacket)) return;
  SyncPacket pkt;
  memcpy(&pkt, data, sizeof(SyncPacket));
  syncInType    = pkt.type;
  syncInMode    = pkt.mode;
  syncGotPacket = true;
}

void sendSyncPacket(uint8_t type, uint8_t mode) {
  SyncPacket pkt = { type, mode };
  esp_now_send(broadcastAddr, (uint8_t*)&pkt, sizeof(SyncPacket));
}

void setupESPNow() {
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onSyncReceive);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = 0;  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("ESP-NOW peer failed");
  else Serial.println("ESP-NOW ready");
}

// 2-second cyan sweep hilt→tip (sync-arm animation)
void renderSyncAnim(float progress) {
  bladeClear();
  int lit = constrain((int)(progress * BLADE_HALF), 0, BLADE_HALF - 1);
  for (int i = 0; i <= lit; i++) bladeSet(i, CRGB(0, 50, 80));
  bladeSet(lit, CRGB(255, 255, 255));     // bright leading-edge pixel
}

// Searching visual: alternating 10-LED red/purple bands that scroll slowly
void renderSyncSearching() {
  static float offset = 0.0f;
  offset += 0.3f;
  if (offset >= 20.0f) offset -= 20.0f;
  float   pulse = 0.55f + 0.45f * sinf((float)millis() * 0.003f);
  uint8_t val   = (uint8_t)(200.0f * pulse);
  int     off   = (int)offset;
  for (int i = 0; i < BLADE_HALF; i++) {
    uint8_t hue = (((i + off) / 10) % 2) ? 192 : 0;   // 192=purple  0=red
    bladeSet(i, CHSV(hue, 255, val));
  }
}

// Subtle cyan pulse at the hilt end — "sync active" indicator overlay
void renderSyncIndicator() {
  float   pulse = (sinf((float)millis() * 0.004f) + 1.0f) * 0.5f;
  uint8_t bri   = (uint8_t)(pulse * 70.0f + 15.0f);
  leds[0]            += CRGB(0, bri, bri);   // hilt side A
  leds[NUM_LEDS - 1] += CRGB(0, bri, bri);   // hilt side B
}


// =====================================================================
// OTA VISUAL
// =====================================================================
void renderOTAMode() {
  FastLED.setBrightness(OTA_BRIGHTNESS);
  static bool     blinkState = false;
  static uint32_t lastBlink  = 0;
  if (millis() - lastBlink > 300) { blinkState = !blinkState; lastBlink = millis(); }
  FastLED.clear();
  for (int i = 0; i < NUM_LEDS; i += 5)
    if (blinkState) leds[i] = CRGB(0, 0, 150);
  int filled = (otaProgress * BLADE_HALF) / 100;
  for (int i = 0; i < filled; i++) {
    leds[i]                = CRGB(0, 100, 255);
    leds[NUM_LEDS - 1 - i] = CRGB(0, 100, 255);
  }
  FastLED.show();
}


// =====================================================================
// WIFI + OTA SETUP
// =====================================================================
void setupWiFiOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t < 10000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi: " + WiFi.localIP().toString());
    if (MDNS.begin(hostname)) Serial.println("mDNS ready");
  }
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(otaPass);
  ArduinoOTA.onStart([]()                         { otaActive = true; otaProgress = 0; Serial.println("OTA Start"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int tot) { otaProgress = (p*100)/tot; });
  ArduinoOTA.onEnd([]()                           { Serial.println("OTA End"); });
  ArduinoOTA.onError([](ota_error_t e)            { Serial.printf("OTA Error[%u]\n", e); });
  ArduinoOTA.begin();
  setupESPNow();
}


// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  pinMode(ACCENT_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  // Initialise ripple positions to inactive
  for (int r = 0; r < MAX_RIPPLES; r++) ripplePos[r] = -1.0f;

  setupIMU();

  // Startup sweep — both sides travel together from hilt to tip
  for (int i = 0; i < BLADE_HALF; i++) {
    bladeSet(i, CRGB(100, 0, 150));
    FastLED.show();
    delay(8);
  }
  delay(200);
  bladeClear();
  FastLED.show();

  setupWiFiOTA();
  Serial.println("Ready.  DblClick=mode  TripleHold=sync  Hold=boost");
}


// =====================================================================
// MAIN LOOP
// =====================================================================
void loop() {
  ArduinoOTA.handle();
  if (otaActive) { renderOTAMode(); return; }

  // ── WiFi reconnect watchdog — restores OTA visibility if link drops ───
  static uint32_t wifiCheckMs = 0;
  if (millis() - wifiCheckMs > 30000) {
    wifiCheckMs = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost — reconnecting...");
      WiFi.reconnect();
    }
  }

  updateIMU();

  // ── Handle incoming ESP-NOW sync packet (flag set by callback) ────────
  if (syncGotPacket) {
    syncGotPacket = false;
    if (syncInType == SYNC_MSG_MODE && syncEnabled) {
      // Paired peer changed mode — apply it here
      topMode        = syncInMode % 3;
      pnt.ready      = false;
      pntCompressPos = 0.0f;
      pntCompressVel = 0.0f;
      ballsReady     = false;
      bladeClear();
      const char* names[] = { "Effects", "Painter", "Balls" };
      Serial.printf("Sync recv: %s\n", names[topMode]);
    } else if (syncInType == SYNC_MSG_PING) {
      if (syncSearching) {
        // Found a peer! Pair up and confirm back so they pair too.
        syncSearching = false;
        syncEnabled   = true;
        sendSyncPacket(SYNC_MSG_PING, topMode);
        Serial.println("Sync: PAIRED");
        CRGB fc = CRGB(0, 100, 100);
        for (int f = 0; f < 3; f++) {
          for (int i = 0; i < BLADE_HALF; i++) bladeSet(i, fc);
          FastLED.show(); delay(150);
          bladeClear(); FastLED.show(); delay(100);
        }
      } else if (syncEnabled) {
        // Already paired — respond so a newly-searching sword can find us
        sendSyncPacket(SYNC_MSG_PING, topMode);
      }
    }
  }

  // ── Button: HOLD=boost  DOUBLE-CLICK=mode  TRIPLE-CLICK+HOLD=sync arm ─
  bool btn = digitalRead(BUTTON_PIN);
  if (btn == LOW && lastButton == HIGH) {
    uint32_t now = millis();
    if (now - lastClickMs > 40) {   // debounce
      clickCount++;
      lastClickMs  = now;
      buttonDownMs = now;
    }
  }
  // Triple-click + hold → start 2-second sync-arm animation
  if (!syncAnimating && clickCount >= 3 && btn == LOW && millis() - buttonDownMs > 80) {
    syncAnimating  = true;
    syncAnimStart  = millis();
    syncSearching  = false;   // cancel any ongoing search
    clickCount     = 0;
    boostMode      = false;
  }
  // Boost: hold from a fresh press (no pending multi-click, not syncing)
  if (btn == LOW && !boostMode && !syncAnimating && clickCount < 3 &&
      millis() - buttonDownMs > BOOST_HOLD_MS) {
    boostMode  = true;
    clickCount = 0;
  }
  // Release ends boost
  if (btn == HIGH && lastButton == LOW && boostMode) {
    boostMode = false;
  }
  lastButton = btn;

  // Dispatch after click window (not held into boost/sync)
  if (!boostMode && !syncAnimating && clickCount > 0 && millis() - lastClickMs > DCLICK_MS) {
    if (clickCount == 2) {   // exactly double-click → cycle mode
      topMode        = (topMode + 1) % 3;
      pnt.ready      = false;
      pntCompressPos = 0.0f;
      pntCompressVel = 0.0f;
      ballsReady     = false;
      bladeClear();
      const char* names[] = { "Effects", "Painter", "Balls" };
      Serial.printf("Mode: %s\n", names[topMode]);
      if (syncEnabled) sendSyncPacket(SYNC_MSG_MODE, topMode);  // broadcast to peers
    }
    // Single-click and 3+ quick clicks: discard silently
    clickCount = 0;
  }

  // ── Sync-arm animation (triple-click + hold for 2 seconds) ───────────
  if (syncAnimating) {
    float progress = (float)(millis() - syncAnimStart) / (float)SYNC_ANIM_MS;
    if (btn == HIGH) {
      // Released before complete — cancel silently
      syncAnimating = false;
    } else if (progress >= 1.0f) {
      syncAnimating = false;
      if (syncEnabled) {
        // Already paired — turn sync off
        syncEnabled = false;
        Serial.println("Sync: OFF");
        CRGB fc = CRGB(100, 40, 0);
        for (int f = 0; f < 3; f++) {
          for (int i = 0; i < BLADE_HALF; i++) bladeSet(i, fc);
          FastLED.show(); delay(150);
          bladeClear(); FastLED.show(); delay(100);
        }
      } else {
        // Start 60-second discovery window
        syncSearching  = true;
        syncSearchStart = millis();
        lastPingMs     = 0;
        Serial.printf("Sync: searching 60s  MAC: %s\n", WiFi.macAddress().c_str());
        CRGB fc = CRGB(0, 80, 100);
        for (int f = 0; f < 3; f++) {
          for (int i = 0; i < BLADE_HALF; i++) bladeSet(i, fc);
          FastLED.show(); delay(150);
          bladeClear(); FastLED.show(); delay(100);
        }
      }
    } else {
      // Still animating — render sweep and return early
      renderSyncAnim(progress);
      FastLED.setBrightness(BRIGHTNESS);
      FastLED.show();
      delay(16);
      return;
    }
  }

  // ── Sync search: broadcast pings every 500 ms, timeout after 60 s ────
  if (syncSearching) {
    if (millis() - syncSearchStart >= SYNC_SEARCH_MS) {
      syncSearching = false;
      Serial.println("Sync: search timed out — no peer found");
      CRGB fc = CRGB(100, 40, 0);
      for (int f = 0; f < 3; f++) {
        for (int i = 0; i < BLADE_HALF; i++) bladeSet(i, fc);
        FastLED.show(); delay(150);
        bladeClear(); FastLED.show(); delay(100);
      }
    } else if (millis() - lastPingMs > 500) {
      sendSyncPacket(SYNC_MSG_PING, topMode);
      lastPingMs = millis();
    }
  }

  // ── Effects (or sync-searching override) ──────────────────────────────
  if (syncSearching) {
    renderSyncSearching();
  } else if (topMode == 1) {
    effectPainter();
  } else if (topMode == 2) {
    effectBalls();
  } else {
    switch (effectMode) {
      case 0: effectChaser(); break;
      case 1: effectPulse();  break;
      case 2: effectRipple(); break;
    }
  }

  // G-force blob — skip during sync search (blade is already taken)
  if (!syncSearching) {
    updateBlobPhysics();
    renderGBlob();
  }

  if (boostMode) {
    renderBoostSparks();
    // Rapid brightness pulse at ~8 Hz — "rushed energy" feel
    float pulse = 0.72f + 0.28f * sinf((float)millis() * 0.050f);
    FastLED.setBrightness((uint8_t)(BRIGHTNESS * pulse));
  } else {
    FastLED.setBrightness(BRIGHTNESS);
  }

  // Sync indicator: subtle cyan pulse at hilt while paired
  if (syncEnabled) renderSyncIndicator();

  FastLED.show();
  delay(16);   // ~60 fps
}
