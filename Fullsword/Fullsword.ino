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
#define BUTTON_PIN    D3
#define SDA_PIN       D4
#define SCL_PIN       D5

// ─── LED CONFIGURATION ────────────────────────────────────────────────
#define NUM_LEDS      114
#define BLADE_LENGTH  110     // single stripped blade (no mirror)
#define BRIGHTNESS    180
#define OTA_BRIGHTNESS 60

// ─── IMU ──────────────────────────────────────────────────────────────
#define IMU_ADDR      0x68

// ─── WIFI / OTA ───────────────────────────────────────────────────────
const char* ssid      = "CGN3-4400";
const char* password  = "251148015432";
const char* hostname  = "Fullsword";
const char* otaHash   = "f3b462d93b24cb0538f5d864546bc3e0"; // MD5 hash of "sword"
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

// ─── EFFECTS STATE ────────────────────────────────────────────────────
uint8_t  effectMode   = 0;     // Cycle: Painter, PingPong, FireStorm, OceanWaves, PlasmaStorm
uint8_t  baseHue      = 190;   // purple-blue base (~190 in FastLED HSV)

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

// ─── ROLLING OVERLAY PHYSICS ──────────────────────────────────────────
float rollPhase = 0.0f;
float lastGyroZ = 0.0f;

// ─── SWORD PAINTER STATE ──────────────────────────────────────────────
struct {
  float buf[BLADE_LENGTH];  // per-pixel hue (0–255 float)
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
float     pullImpulse = 0.0f;   // one-shot pull push magnitude
float     prevAZ      = 0.0f;   // previous accelZ sample for derivative

// ─── IMPACT WAVES ─────────────────────────────────────────────────────
#define MAX_IMPACTS 4
struct ImpactWave {
  float pos;
  float vel;
  float intensity;  // fades to 0
  bool active;
};
ImpactWave impacts[MAX_IMPACTS];
int impactNext = 0;

void spawnImpact(float startPos, float vel, float intensity) {
  impacts[impactNext].pos = startPos;
  impacts[impactNext].vel = vel;
  impacts[impactNext].intensity = intensity;
  impacts[impactNext].active = true;
  impactNext = (impactNext + 1) % MAX_IMPACTS;
}

void updateAndRenderImpacts() {
  for (int i = 0; i < MAX_IMPACTS; i++) {
    if (!impacts[i].active) continue;
    impacts[i].pos += impacts[i].vel;
    impacts[i].intensity *= 0.88f; 
    if (impacts[i].intensity < 0.05f || impacts[i].pos < -20.0f || impacts[i].pos > BLADE_HALF + 20.0f) {
      impacts[i].active = false;
      continue;
    }
    int p = (int)impacts[i].pos;
    float frac = impacts[i].pos - p;
    float intensity = constrain(impacts[i].intensity, 0.0f, 1.0f);
    for (int d = -4; d <= 4; d++) {
      int pos = p + d;
      if (pos >= 0 && pos < BLADE_HALF) {
        float dist = fabsf((float)d + frac);
        float falloff = expf(-0.3f * dist * dist);
        uint8_t bri = (uint8_t)(intensity * falloff * 255.0f);
        if (bri > 0) {
          uint8_t sat = (uint8_t)(255.0f * (1.0f - falloff));
          leds[pos] += CHSV(130, sat, bri); 
          leds[NUM_LEDS - 1 - pos] += CHSV(130, sat, bri);
        }
      }
    }
  }
}

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

// Write colour to physical blade position pos (0=hilt, BLADE_LENGTH-1=tip)
inline void bladeSet(int pos, CRGB color) {
  if (pos < 0 || pos >= BLADE_LENGTH) return;
  leds[4 + pos] = color;
}

// Read colour from physical blade position pos (0=hilt, BLADE_LENGTH-1=tip)
inline CRGB bladeGet(int pos) {
  if (pos < 0 || pos >= BLADE_LENGTH) return CRGB::Black;
  return leds[4 + pos];
}

void bladeClear() {
  fill_solid(leds + 4, NUM_LEDS - 4, CRGB::Black);
}

void renderAccents() {
  // Slowly pulse the 4 accent LEDs at the base of the strip
  float pulse = (sinf((float)millis() * 0.002f) + 1.0f) * 0.5f;
  uint8_t bri = (uint8_t)(pulse * 150.0f + 50.0f);
  CRGB accentColor = CHSV(baseHue + 20, 200, bri); // Slightly shifted hue
  
  leds[0] = accentColor;
  leds[1] = accentColor;
  leds[2] = accentColor;
  leds[3] = accentColor;
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
  Wire.setClock(100000);   // 100 kHz
  delay(150);              

  // ── Step 1: I2C presence check — does 0x68 ACK at all? ───────────
  Wire.beginTransmission(0x68);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.printf("MPU9250: no ACK at 0x68 (I2C error %u) — check SDA/SCL wiring\n", err);
    Wire.beginTransmission(0x69);
    if (Wire.endTransmission() == 0)
      Serial.println("  → found something at 0x69 — is AD0 pin HIGH? Change IMU_ADDR to 0x69");
    imuOk = false;
    return;
  }

  // ── Step 2: Read WHO_AM_I — MPU9250 returns 0x71 or 0x73 (or 0x70 for MPU6500) ─────────────
  Wire.beginTransmission(0x68);
  Wire.write(0x75);
  err = Wire.endTransmission(false);
  if (err != 0) {
    Serial.printf("MPU9250: WHO_AM_I write failed (I2C error %u)\n", err);
    imuOk = false;
    return;
  }
  Wire.requestFrom((uint8_t)0x68, (uint8_t)1);
  uint8_t id = Wire.available() ? Wire.read() : 0xFF;
  if (id != 0x71 && id != 0x73 && id != 0x70 && id != 0x68) {
    Serial.printf("MPU9250: WHO_AM_I = 0x%02X, expected 0x71/0x73 (MPU9250)\n", id);
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
  Serial.println("MPU9250: found and configured at 0x68");
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
  // Pull detection: sharp negative derivative of accelZ (yank hilt-ward)
  float dAZ   = accelZ - prevAZ;
  stabImpulse = fmaxf(0.0f, dAZ - 0.30f);
  pullImpulse = fmaxf(0.0f, -dAZ - 0.30f);
  prevAZ      = accelZ;
  
  if (stabImpulse > 0.2f) {
    spawnImpact(0.0f, 3.5f + stabImpulse * 1.5f, fminf(1.8f, 0.6f + stabImpulse * 2.5f));
  }
  if (pullImpulse > 0.2f) {
    spawnImpact((float)(BLADE_HALF - 1), -(3.5f + pullImpulse * 1.5f), fminf(1.8f, 0.6f + pullImpulse * 2.5f));
  }
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

  for (int i = 0; i < BLADE_LENGTH; i++) {
    float t    = (float)i / (float)(BLADE_LENGTH - 1);
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

    if (ripplePos[r] >= BLADE_LENGTH || ripplePos[r] < 0.0f) {
      ripplePos[r] = -1.0f;
      continue;
    }

    int   p    = (int)ripplePos[r];
    float frac = ripplePos[r] - p;

    // Sub-pixel rendering between p and p+1
    if (p >= 0 && p < BLADE_LENGTH) {
      bladeSet(p, bladeGet(p) + CHSV(rippleHue[r], 235, (uint8_t)(220.0f * (1.0f - frac))));
    }
    
    if (p + 1 >= 0 && p + 1 < BLADE_LENGTH) {
      bladeSet(p + 1, bladeGet(p + 1) + CHSV(rippleHue[r], 235, (uint8_t)(220.0f * frac)));
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
    for (int i = 0; i < BLADE_LENGTH; i++)
      pnt.buf[i] = (float)(i * 256) / BLADE_LENGTH;
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

  int ci = constrain((int)(pnt.paintCenter * BLADE_LENGTH), 0, BLADE_LENGTH - 1);
  int br = max(3, (int)((0.12f + fabsf(pnt.paintVelocity) * 0.15f) * BLADE_LENGTH));

  // ── Twist → shift hue bands sideways + capture dominant zone ─────────
  bool twisting = imuOk && fabsf(twistRate) > 30.0f;
  if (twisting) {
    if (pnt.domFade <= 0.0f) pnt.domHue = pnt.buf[ci];
    pnt.domFade    = 2.0f;
    pnt.domWidth   = constrain(pnt.domWidth + dt * fabsf(twistRate) * 0.05f,
                               0.0f, (float)(BLADE_LENGTH / 2 - 2));
    pnt.shiftAccum += twistRate * dt * 0.12f;
  } else {
    pnt.domFade    = constrain(pnt.domFade  - dt,      0.0f, 2.0f);
    pnt.domWidth  *= (1.0f - dt * 0.4f);
    pnt.shiftAccum *= (1.0f - dt * 3.0f);
  }
  while (pnt.shiftAccum >= 1.0f) {
    float sv = pnt.buf[BLADE_LENGTH - 1];
    for (int i = BLADE_LENGTH - 1; i > 0; i--) pnt.buf[i] = pnt.buf[i - 1];
    pnt.buf[0] = sv;
    pnt.shiftAccum -= 1.0f;
  }
  while (pnt.shiftAccum <= -1.0f) {
    float sv = pnt.buf[0];
    for (int i = 0; i < BLADE_LENGTH - 1; i++) pnt.buf[i] = pnt.buf[i + 1];
    pnt.buf[BLADE_LENGTH - 1] = sv;
    pnt.shiftAccum += 1.0f;
  }

  // ── Paint: blend pixels near brush toward centerHue ───────────────────
  for (int i = 0; i < BLADE_LENGTH; i++) {
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
  static float tb[BLADE_LENGTH];
  memcpy(tb, pnt.buf, sizeof(tb));
  float diffR = 0.03f * dt;
  for (int i = 1; i < BLADE_LENGTH - 1; i++) {
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
    for (int i = max(0, ci - dw); i <= min(BLADE_LENGTH - 1, ci + dw); i++) {
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
    push        -= pullImpulse * 0.8f;   // pull slams bands to hilt
    float spring = -0.035f * pntCompressPos;
    pntCompressVel = pntCompressVel * 0.88f + push + spring;
    pntCompressPos += pntCompressVel;
    if (pntCompressPos > 1.0f) { pntCompressPos = 1.0f; pntCompressVel = -fabsf(pntCompressVel) * 0.25f; }
    if (pntCompressPos < -1.0f) { pntCompressPos = -1.0f; pntCompressVel =  fabsf(pntCompressVel) * 0.25f; }
  }

  // ── Render to mirrored blade ────────────────────────────────────────────
  // Power-curve remap: compress > 0 crushes bands towards tip; compress < 0 toward hilt.
  uint32_t now = millis();
  float compPower = 1.0f;
  if (pntCompressPos >= 0.0f) {
    compPower = 1.0f + pntCompressPos * 2.0f + (boostMode ? 0.6f : 0.0f);
  } else {
    // pntCompressPos is negative. 
    compPower = 1.0f / (1.0f - pntCompressPos * 2.0f + (boostMode ? 0.6f : 0.0f));
  }

  for (int i = 0; i < BLADE_LENGTH; i++) {
    // Compressed sample index via power curve
    float t    = (float)i / (float)(BLADE_LENGTH - 1);
    float fidx = (i == 0) ? 0.0f : powf(t, compPower) * (float)(BLADE_LENGTH - 1);
    int   lo   = (int)fidx;
    int   hix  = min(lo + 1, BLADE_LENGTH - 1);
    float ifrc = fidx - lo;

    // Shortest-path hue interpolation (wraps at 0/256 boundary)
    float h0 = pnt.buf[lo];
    float h1 = pnt.buf[hix];
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
    s = (uint8_t)constrain((int)s - (int)(fabsf(pntCompressPos) * 75.0f), 155, 255);

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
    if (di >= 0 && di < BLADE_LENGTH) {
      float dh = pnt.centerHue + (float)random(-20, 21);
      while (dh >= 256.0f) dh -= 256.0f;
      while (dh <    0.0f) dh += 256.0f;
      CRGB drp = CHSV((uint8_t)dh, 255, 200);
      bladeSet(di, blend(bladeGet(di), drp, 180));
      if (di > 0 && di < BLADE_LENGTH - 1) {
        bladeSet(di - 1, blend(bladeGet(di - 1), drp, 120));
        bladeSet(di + 1, blend(bladeGet(di + 1), drp, 120));
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
    balls[b].pos   = (float)(b + 1) * ((float)BLADE_LENGTH / (NUM_BALLS + 1.0f));
    balls[b].vel   = 0.0f;
    balls[b].hue   = 0;
    balls[b].sat   = 0;
    balls[b].width = 2;
    balls[b].flash = 0.0f;
  }
  whirlPhase = 0.0f;
  ballsReady = true;
}

void effectPingPong() {
  if (!ballsReady) initBalls();

  // ── Whirlwind: accumulate twist → animated hue wave ──────────────────
  float twistAbs = fabsf(twistRate);
  float whirlStr = constrain((twistAbs - 20.0f) / 150.0f, 0.0f, 1.0f);
  whirlPhase += twistRate * 0.016f * 0.15f;
  if (whirlPhase >  TWO_PI) whirlPhase -= TWO_PI;
  if (whirlPhase < -TWO_PI) whirlPhase += TWO_PI;

  // ── Background: constant red and blue animation ───────────────────────
  uint32_t ms = millis();
  for (int i = 0; i < BLADE_LENGTH; i++) {
    float wave = (sinf((float)i * 0.15f + (float)ms * 0.003f) + 1.0f) * 0.5f;
    // wave = 0 -> Red, wave = 1 -> Blue. Peak brightness 150.
    uint8_t r = (uint8_t)((1.0f - wave) * 150.0f);
    uint8_t b = (uint8_t)(wave * 150.0f);
    bladeSet(i, CRGB(r, 0, b));
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
  // Pull: sharp pullback rushes all balls hard toward hilt
  float pullKick  = pullImpulse * 10.0f * (boostMode ? 1.5f : 1.0f);

  // ── Update each ball ──────────────────────────────────────────────────
  float vmax = boostMode ? 6.0f : 4.0f;
  for (int b = 0; b < NUM_BALLS; b++) {
    balls[b].vel += gravity + swingKick + stabKick - pullKick;
    balls[b].vel  = constrain(balls[b].vel, -vmax, vmax);
    balls[b].pos += balls[b].vel;

    // Wall bounce at tip
    if (balls[b].pos >= (float)(BLADE_LENGTH - 1)) {
      balls[b].pos = (float)(BLADE_LENGTH - 1);
      balls[b].vel = -fabsf(balls[b].vel) * 0.70f;
      if (boostMode) { balls[b].flash = fmaxf(balls[b].flash, 0.5f); balls[b].sat = 180; }
    }
    // Wall bounce at hilt
    if (balls[b].pos <= 0.0f) {
      balls[b].pos = 0.0f;
      balls[b].vel =  fabsf(balls[b].vel) * 0.70f;
      if (boostMode) { balls[b].flash = fmaxf(balls[b].flash, 0.5f); balls[b].sat = 180; }
    }

    // Impulses cause a bright flash
    if (stabImpulse > 0.3f || pullImpulse > 0.3f) {
      balls[b].flash = fmaxf(balls[b].flash, fmaxf(stabImpulse, pullImpulse));
      balls[b].sat   = (uint8_t)fmaxf(0.0f, (float)balls[b].sat - 100.0f);
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
    float   bpos   = constrain(balls[b].pos, 0.0f, (float)(BLADE_LENGTH - 1));
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
      if (pos < 0 || pos >= BLADE_LENGTH) continue;
      float   dist    = fabsf((float)d + frac);
      float   falloff = expf(-0.5f * (dist / sigma) * (dist / sigma));
      uint8_t bri     = (uint8_t)((float)baseBri * falloff);
      if (bri < 4) continue;
      bladeSet(pos, bladeGet(pos) + CHSV(bHue, bSat, bri));
    }

    // Explosion ring expands outward from collision point
    if (balls[b].flash > 0.15f) {
      int ring = (int)(balls[b].flash * 7.0f);
      for (int side = -1; side <= 1; side += 2) {
        int pos = centre + side * ring;
        if (pos >= 0 && pos < BLADE_LENGTH) {
          uint8_t rbri = (uint8_t)(balls[b].flash * 200.0f);
          bladeSet(pos, bladeGet(pos) + CHSV(balls[b].hue + 128, 200, rbri));
        }
      }
    }
  }

  // Boost mode: scatter sparks
  if (boostMode) {
    int sc = constrain(2 + (int)(swingMag * 0.04f), 2, 7);
    for (int s = 0; s < sc; s++) {
      int pos = random(BLADE_LENGTH);
      bladeSet(pos, bladeGet(pos) + CHSV(random8(), random8(80, 220), random8(100, 230)));
    }
  }
}

// =====================================================================
// EFFECT 3 — FIRE STORM
// =====================================================================
void effectFireStorm() {
  const float dt = 0.016f;
  static byte heat[114];
  static uint32_t lu = 0;
  static float fb = 0, fbv = 0;
  static float rPos[5] = {0}; 
  static unsigned long rT[5] = {0}; 
  static bool rA[5] = {false};
  
  uint32_t now = millis();
  if (now - lu < 50) return; 
  lu = now;

  if (imuOk) {
    float ty = constrain(accelZ * 0.5f, -1.0f, 1.0f);
    fbv = fbv * 0.88f + ty * dt * 8.0f; 
    fb += fbv * dt * 2.0f;
    if (fb < 0.1f) { 
      fb = 0.1f; fbv = -fbv * 0.7f; 
      for (int i = 0; i < 5; i++) if (!rA[i]) { rA[i] = true; rPos[i] = 0.0f; rT[i] = now; break; }
    }
    if (fb > 0.9f) { 
      fb = 0.9f; fbv = -fbv * 0.7f; 
      for (int i = 0; i < 5; i++) if (!rA[i]) { rA[i] = true; rPos[i] = 1.0f; rT[i] = now; break; }
    }
  } else { 
    static float sp = 0; sp += dt * 0.5f; fb = 0.5f + sinf(sp) * 0.3f; fbv = cosf(sp) * 0.2f; 
  }
  
  if (random(100) < 3)
    for (int i = 0; i < 5; i++) 
      if (!rA[i]) { rA[i] = true; rPos[i] = 0.5f; rT[i] = now; break; }

  for (int i = 0; i < BLADE_LENGTH; i++) { 
    int cd = random(10, 25); 
    heat[i] = (heat[i] > cd) ? heat[i] - cd : 0; 
  }
  for(int k = BLADE_LENGTH - 1; k >= 2; k--) 
    heat[k] = (byte)((heat[k-1] * 0.6f + heat[k-2] * 0.4f));

  int bi = (int)(fb * BLADE_LENGTH); 
  int bri = 8 + (int)(fabsf(fbv) * 15.0f);
  for(int i = -bri; i <= bri; i++) {
    int idx = bi + i;
    if (idx >= 0 && idx < BLADE_LENGTH) {
      float d = fabsf((float)i) / (float)bri;
      int nh = heat[idx] + (int)((1.0f - d) * 200.0f);
      heat[idx] = (nh > 255) ? 255 : (byte)nh;
    }
  }

  for (int i = 0; i < BLADE_LENGTH; i++) {
    byte t = heat[i]; 
    uint8_t r, g, b;
    if(t > 200) { r = 255; g = 255; b = random(30, 80); }
    else if(t > 150) { r = 255; g = random(80, 120); b = 0; }
    else if(t > 100) { r = 255; g = random(30, 80);  b = 0; }
    else if(t > 50)  { r = 200 + random(55); g = random(20, 40); b = 0; }
    else { r = 100 + random(50); g = 0; b = 0; }
    bladeSet(i, CRGB(r, g, b));
  }

  for (int r = 0; r < 5; r++) {
    if(!rA[r]) continue;
    float rr = (float)(now - rT[r]) / 800.0f * 0.5f;
    if (rr > 0.5f) { rA[r] = false; continue; }
    for (int i = 0; i < BLADE_LENGTH; i++) {
      float pos = (float)i / (float)BLADE_LENGTH;
      float dc = fabsf(pos - rPos[r]);
      float dr2 = fabsf(dc - rr);
      if(dr2 < 0.05f) {
        float ints = (1.0f - dr2 / 0.05f) * (1.0f - rr * 2.0f);
        bladeSet(i, bladeGet(i) + CRGB((uint8_t)(ints * 255.0f), (uint8_t)(ints * 100.0f), 0));
      }
    }
  }
}

// =====================================================================
// EFFECT 4 — OCEAN WAVES
// =====================================================================
void effectOceanWaves() {
  const float dt = 0.016f;
  static float wp = 0, ob = 0.5f, obv = 0;
  static float wgP[3] = {0.2f, 0.5f, 0.8f};
  static float wgS[3] = {0.15f, 0.2f, 0.18f};
  static float wgI[3] = {0.8f, 1.0f, 0.9f};
  static unsigned long wgT[3] = {0};
  
  uint32_t now = millis();
  if (imuOk) {
    float ty = constrain(accelZ * 0.4f, -1.0f, 1.0f);
    obv = obv * 0.9f + ty * dt * 6.0f; 
    ob += obv * dt * 2.0f;
    if (ob < 0.1f) { 
      ob = 0.1f; obv = -obv * 0.8f; 
      wgP[0] = 0; wgS[0] = 0.1f; wgI[0] = 1.0f; wgT[0] = now; 
    }
    if (ob > 0.9f) { 
      ob = 0.9f; obv = -obv * 0.8f; 
      wgP[2] = 1; wgS[2] = 0.1f; wgI[2] = 1.0f; wgT[2] = now; 
    }
  } else { 
    static float sp = 0; sp += dt * 0.4f; 
    ob = 0.5f + sinf(sp) * 0.25f; obv = cosf(sp) * 0.15f; 
  }
  
  wp += dt * (2.0f + fabsf(obv) * 3.0f); 
  if (wp > 6.283f) wp -= 6.283f;

  for (int w = 0; w < 3; w++) {
    if(wgI[w] > 0.1f) {
      wgS[w] += dt * 0.3f;
      if(wgP[w] < 0.5f) wgP[w] -= dt * 0.4f; else wgP[w] += dt * 0.4f;
      wgI[w] *= 0.97f;
      if(wgP[w] < -0.2f || wgP[w] > 1.2f) wgI[w] = 0;
    }
  }

  if (random(100) < 2) {
    for (int w = 0; w < 3; w++) {
      if (wgI[w] < 0.2f) {
        wgP[w] = 0.4f + (float)random(20) / 100.0f;
        wgS[w] = 0.08f + (float)random(10) / 100.0f;
        wgI[w] = 0.6f + (float)random(40) / 100.0f;
        wgT[w] = now;
        break;
      }
    }
  }

  for (int i = 0; i < BLADE_LENGTH; i++) {
    float pos = (float)i / (float)BLADE_LENGTH;
    uint8_t bh = 140 + (uint8_t)(pos * 30.0f);
    uint8_t bs = 255;
    uint8_t bv = 60 + (uint8_t)(pos * 80.0f);
    
    float wave = sinf(pos * 20.0f + wp) * 0.3f + 0.7f; 
    bv = (uint8_t)((float)bv * wave);
    
    for (int w = 0; w < 3; w++) {
      float dw = fabsf(pos - wgP[w]);
      if(dw < wgS[w] && wgI[w] > 0.1f) {
        float wi = (wgS[w] - dw) / wgS[w]; 
        wi *= wi; wi *= wgI[w];
        bv = (uint8_t)constrain((int)bv + (int)(wi * 180.0f), 0, 255); 
        bs = (uint8_t)constrain((int)bs - (int)(wi * 120.0f), 100, 255); 
        bh = 150 + (uint8_t)(wi * 20.0f);
        if(wi > 0.7f) { bs = 100; bv = 255; }
      }
    }
    
    float db = fabsf(pos - ob);
    if(db < 0.15f) {
      float bi = (0.15f - db) / 0.15f; 
      bi *= bi;
      bv = (uint8_t)constrain((int)bv + (int)(bi * 150.0f), 0, 255);
      bs = (uint8_t)constrain((int)bs - (int)(bi * 100.0f), 150, 255);
      if(fabsf(obv) > 0.3f && bi > 0.7f) { bv = 255; bs = 150; }
    }
    
    bladeSet(i, CHSV(bh, bs, bv));
  }
  
  if (fabsf(obv) > 0.2f && random8() < 50) {
    int rndPos = random(BLADE_LENGTH);
    bladeSet(rndPos, CHSV(180, 200, 220));
  }
}

// =====================================================================
// EFFECT 5 — PLASMA STORM
// =====================================================================
void effectPlasmaStorm() {
  const float dt = 0.016f;
  static float p1 = 0, p2 = 0, p3 = 0;
  
  p1 += dt * 2.5f; if(p1 > 6.283f) p1 -= 6.283f;
  p2 += dt * 1.8f; if(p2 > 6.283f) p2 -= 6.283f;
  p3 += dt * 3.2f; if(p3 > 6.283f) p3 -= 6.283f;
  
  for (int i = 0; i < BLADE_LENGTH; i++) {
    float pos = (float)i / (float)BLADE_LENGTH;
    float c = (sinf(pos * 10.0f + p1) + sinf(pos * 15.0f - p2) + sinf(pos * 8.0f + p3)) / 3.0f;
    bladeSet(i, CHSV(200 + (uint8_t)(c * 40.0f), 255, 120 + (uint8_t)(c * 135.0f)));
  }
  
  if (random8() < 30) {
    int ap = random(BLADE_LENGTH - 5);
    for (int j = 0; j < 3; j++) {
      bladeSet(ap + j, CHSV(180, 200, 255));
    }
  }
}

// ─── ROLLING SPARKLE OVERLAY (Replaces gBlob)
void updateAndRenderRollOverlay() {
  // If the sword is being twisted significantly along its axis (Z)...
  float gyroZ_abs = fabsf(twistRate);
  
  if (gyroZ_abs > 30.0f) {
    rollPhase += (twistRate * 0.005f);
    
    // Create traveling spiraling sparks along the whole blade based on twist
    int numSparks = constrain((int)(gyroZ_abs / 20.0f), 1, 8);
    for (int i = 0; i < numSparks; i++) {
        int target = random(0, BLADE_LENGTH);
        float shift = sinf(rollPhase + (target * 0.1f)) + 1.0f;
        CRGB sparkColor = CHSV(baseHue + (twistRate > 0 ? 30 : -30), 120, (uint8_t)(shift * 127.0f));
        bladeSet(target, bladeGet(target) + sparkColor); 
    }
  }
}


// =====================================================================
// BOOST MODE — spark overlay (called after main effect + gBlob)
// =====================================================================
void renderBoostSparks() {
  int count = constrain(3 + (int)(swingMag * 0.06f), 3, 10);
  for (int s = 0; s < count; s++) {
    int     pos = random(BLADE_LENGTH);
    uint8_t h   = baseHue + (int8_t)(random8() / 4 - 32);  // slight hue scatter
    uint8_t sat = random8(60, 200);                          // white→coloured range
    uint8_t bri = random8(160, 255);
    bladeSet(pos, bladeGet(pos) + CHSV(h, sat, bri));
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
  int lit = constrain((int)(progress * BLADE_LENGTH), 0, BLADE_LENGTH - 1);
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
  for (int i = 0; i < BLADE_LENGTH; i++) {
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
  int filled = (otaProgress * BLADE_LENGTH) / 100;
  for (int i = 0; i < filled; i++) {
    bladeSet(i, CRGB(0, 100, 255));
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
    server.on("/", handleRoot);
    server.on("/mode", handleMode);
    server.begin();
  }
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPasswordHash(otaHash);
  ArduinoOTA.onStart([]()                         { otaActive = true; otaProgress = 0; Serial.println("OTA Start"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int tot) { otaProgress = (p*100)/tot; });
  ArduinoOTA.onEnd([]()                           { Serial.println("OTA End"); });
  ArduinoOTA.onError([](ota_error_t e)            { Serial.printf("OTA Error[%u]\n", e); });
  ArduinoOTA.begin();
  setupESPNow();
}

// =====================================================================
// WEB SERVER HANDLERS
// =====================================================================
void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{background:#111;color:#eee;font-family:sans-serif;text-align:center;}";
  html += "button{display:block;width:90%;margin:15px auto;padding:20px;font-size:20px;background:#334;color:#fff;border:none;border-radius:10px;cursor:pointer;}";
  html += "button:active{background:#557;}</style></head><body>";
  html += "<h1>Fullsword Control</h1>";
  html += "<h3>Active Mode: " + String(effectMode) + "</h3>";
  
  html += "<form action='/mode' method='GET'><button name='m' value='0'>Painter</button></form>";
  html += "<form action='/mode' method='GET'><button name='m' value='1'>Ping Pong</button></form>";
  html += "<form action='/mode' method='GET'><button name='m' value='2'>Fire Storm</button></form>";
  html += "<form action='/mode' method='GET'><button name='m' value='3'>Ocean Waves</button></form>";
  html += "<form action='/mode' method='GET'><button name='m' value='4'>Plasma Storm</button></form>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleMode() {
  if (server.hasArg("m")) {
    int m = server.arg("m").toInt();
    if (m >= 0 && m <= 4) {
      effectMode = m;
      pnt.ready = false;
      ballsReady = false;
      bladeClear();
      if (syncEnabled) sendSyncPacket(SYNC_MSG_MODE, effectMode);
    }
  }
  // Redirect back to root
  server.sendHeader("Location", "/");
  server.send(303);
}


// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);



  setupIMU();

  // Startup sweep — travels from hilt to tip
  for (int i = 0; i < BLADE_LENGTH; i++) {
    bladeSet(i, CRGB(100, 0, 150));
    FastLED.show();
    delay(4);
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
  server.handleClient();
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
      effectMode     = syncInMode % 5;
      pnt.ready      = false;
      pntCompressPos = 0.0f;
      pntCompressVel = 0.0f;
      ballsReady     = false;
      bladeClear();
      const char* names[] = { "Painter", "PingPong", "FireStorm", "OceanWaves", "PlasmaStorm" };
      Serial.printf("Sync recv: %s\n", names[effectMode]);
    } else if (syncInType == SYNC_MSG_PING) {
      if (syncSearching) {
        // Found a peer! Pair up and confirm back so they pair too.
        syncSearching = false;
        syncEnabled   = true;
        sendSyncPacket(SYNC_MSG_PING, effectMode);
        Serial.println("Sync: PAIRED");
        CRGB fc = CRGB(0, 100, 100);
        for (int f = 0; f < 3; f++) {
          for (int i = 0; i < BLADE_LENGTH; i++) bladeSet(i, fc);
          FastLED.show(); delay(150);
          bladeClear(); FastLED.show(); delay(100);
        }
      } else if (syncEnabled) {
        // Already paired — respond so a newly-searching sword can find us
        sendSyncPacket(SYNC_MSG_PING, effectMode);
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
      effectMode     = (effectMode + 1) % 5;
      pnt.ready      = false;
      pntCompressPos = 0.0f;
      pntCompressVel = 0.0f;
      ballsReady     = false;
      bladeClear();
      const char* names[] = { "Painter", "PingPong", "FireStorm", "OceanWaves", "PlasmaStorm" };
      Serial.printf("Mode: %s\n", names[effectMode]);
      if (syncEnabled) sendSyncPacket(SYNC_MSG_MODE, effectMode);  // broadcast to peers
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
          for (int i = 0; i < BLADE_LENGTH; i++) bladeSet(i, fc);
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
          for (int i = 0; i < BLADE_LENGTH; i++) bladeSet(i, fc);
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
        for (int i = 0; i < BLADE_LENGTH; i++) bladeSet(i, fc);
        FastLED.show(); delay(150);
        bladeClear(); FastLED.show(); delay(100);
      }
    } else if (millis() - lastPingMs > 500) {
      sendSyncPacket(SYNC_MSG_PING, effectMode);
      lastPingMs = millis();
    }
  }

  // ── Effects (or sync-searching override) ──────────────────────────────
  if (syncSearching) {
    renderSyncSearching();
  } else {
    // Primary Modes Dispatched Here
    switch (effectMode) {
      case 0: effectPainter(); break;
      case 1: effectPingPong(); break;
      case 2: effectFireStorm(); break;
      case 3: effectOceanWaves(); break;
      case 4: effectPlasmaStorm(); break;
    }
  }

  // Roll Overlay — skip during sync search (blade is already taken)
  if (!syncSearching) {
    updateAndRenderRollOverlay();
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

  // Impact waves layer over everything
  if (!syncSearching) {
    updateAndRenderImpacts();
  }
  
  // Render accent LEDs base animation last
  renderAccents();

  FastLED.show();
  delay(16);   // ~60 fps
}
