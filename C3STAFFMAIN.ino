// =====================================================================
// C3STAFFMAIN.ino — MAIN ENTRY POINT
// Globals, setup(), loop()
// LEDs start within ~500ms; WiFi/OTA in background
// =====================================================================

#include <FastLED.h>
#include <Wire.h>
#include "MPU9250.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
// Function prototypes (defined in web_ui.ino)
void setupOTA();
void setupWebServer();
String getHTML();
// =====================================================================
// LED CONFIGURATION
// =====================================================================
#define LED_PIN         D7



#define HEAD_LENGTH     4       // LEDs in head flower
#define TAIL_START      137     // Where you want tail to begin (sets staff length)
#define TAIL_OFFSET     10      // Black gap before tail flower

#define STAFF_LENGTH         (TAIL_START - HEAD_LENGTH)                    // Staff fills head-to-tail
#define STAFF_START          HEAD_LENGTH                                   // Staff starts after head
#define TAIL_FLOWER_LENGTH   (HEAD_LENGTH * 2)                             // Tail mirrors head
#define TAIL_LENGTH          TAIL_FLOWER_LENGTH                            // Tail flower ONLY (no offset)
#define NUM_LEDS             (TAIL_START + TAIL_OFFSET + TAIL_FLOWER_LENGTH) // PHYSICAL LED COUNT
#define DEFAULT_BRIGHTNESS   170

CRGB leds[NUM_LEDS];

// BUTTON CONFIGURATION
// =====================================================================
#define BUTTON_PIN_1        D3
#define BUTTON_PIN_2        D9
#define DEBOUNCE_DELAY      50
#define DOUBLE_CLICK_WINDOW 400

struct ButtonState {
  bool lastState;
  unsigned long lastDebounceTime;
  unsigned long lastClickTime;
  int clickCount;
  bool processed;
};
ButtonState btn1 = {HIGH,0,0,0,false};
ButtonState btn2 = {HIGH,0,0,0,false};

// IMU CONFIGURATION
// =====================================================================
#define SDA_PIN               D4
#define SCL_PIN               D5
#define IMU_TILT_COMPENSATION 2.0f

MPU9250 mpu;
bool  mpu_ok    = false;
bool  mpu_ready = false;
float accel[3]    = {0};
float gyro[3]     = {0};
float accelRaw[3] = {0};

// Roll detection — populated by updateIMU(), consumed by renderVortexOverlay()
float rollRate        = 0.0f;   // Smoothed Y-axis gyro velocity (deg/sec)
float vortexPhase     = 0.0f;   // Accumulated rotation phase for spiral animation
float vortexIntensity = 0.0f;   // 0-1 blend: 0=blob only, 1=full vortex

// AUDIO STATE
// =====================================================================
#define MIC_PIN A0
int   audioBias      = 2048;
float audioLevel     = 0.0f;
float lastAudioLevel = 0.0f;
bool  pseudoBeat     = false;
float audioEnergy    = 0.0f;
float beatIntensity  = 0.0f;
uint32_t lastWebAudio = 0;       // Timestamp of last web audio data
static uint32_t lastAudioPrint = 0;
static float    audioLevelPeak = 0.0f;

// Frequency bands from web audio
float audioBass   = 0.0f;    // 0-860Hz
float audioMidLow = 0.0f;    // 860-1720Hz
float audioMid    = 0.0f;    // 1720-3440Hz
float audioHigh   = 0.0f;    // 3440-6020Hz

// WIFI / SERVER
// =====================================================================
const char *ssid        = "CGN3-4400";
const char *password    = "251148015432";
const char *hostname    = "PurpleStaff";
const char *otaPassword = "staff";
WebServer server(80);
bool wifiConnected = false;
bool otaInProgress = false;

// EFFECT PAGES
// =====================================================================
#define NUM_PAGES 6

enum EffectPage { PAGE_1=0, PAGE_2=1, PAGE_3=2, PAGE_4=3, PAGE_5=4, PAGE_6=5 };

int currentPage   = PAGE_1;
int currentEffect = 0;

const char* page1Effects[] = {"Purple Blob","Rainbow Painter","Fire Storm","Ocean Waves","Crystal Pulse","Ping Pong"};
const char* page2Effects[] = {"Audio Pulse","Beat Sparkle","Sound Wave","Color Shift","Audio Fire"};
const char* page3Effects[] = {"Plasma Storm","Lightning Strike","Comet Trail","Aurora Flow","Galaxy Swirl"};
const char* page4Effects[] = {"Ocean Breeze","Sunset Fade","Forest Mist","Aurora Dreams","Lava Flow"};
const char* page5Effects[] = {"Watermelon","Citrus Burst","Berry Blast","Mango Swirl","Kiwi Spark"};
const char* page6Effects[] = {"Newton's Cradle","Slinky","Ink Drop","Ripple Tank","Supernova","Bioluminescence","Plasma Waves","Electron Orbit","DNA Helix","Meteor Shower","Magnetic Pull","Solar Flare","Molecular Vibe","Black Hole"};

// GLOBAL CONTROLS
// =====================================================================
bool    autoCycle        = false;
float   autoCycleTimer   = 0.0f;
float   autoCycleSeconds = 10.0f;
uint8_t globalBrightness = DEFAULT_BRIGHTNESS;
uint8_t globalSpeed      = 128;
bool    effectEnabled    = true;
bool    blackout         = false;
bool    customColorMode  = false;

uint8_t headHue=220, headSat=200, headVal=255;
uint8_t tailHue=220, tailSat=200, tailVal=240;

// HEAD/TAIL REACTIVITY STATE
// =====================================================================
struct HeadTailState {
  float headImpactIntensity, tailImpactIntensity;
  float headSustainedGlow,   tailSustainedGlow;
  float headPulsePhase,      tailPulsePhase;
  float headBreathePhase,    tailBreathePhase;
  bool  blobAtHead,          blobAtTail;
  float centerImpactIntensity;    // Center zone reactivity
  float centerBreathePhase;        // Center zone breathing
};
HeadTailState htState = {0,0,0,0,0,0,0,0,false,false,0,0};

// PARTICLE 
// =====================================================================
#define MAX_RED_PARTICLES 40
struct RedParticle {
  float position,velocity,charge,displacement,originalPos,sparklePhase;
  bool isPushed,isPulled;
  float recoilVelocity;
};
RedParticle redParticles[MAX_RED_PARTICLES];

#define MAX_TRAIL_SEGMENTS 60
struct TrailSegment {
  float position,intensity,hue,size,age;
  bool  active,isPullTrail;
  float redInfluence;
};
TrailSegment trailSegments[MAX_TRAIL_SEGMENTS];

struct FluidBlob {
  float position,velocity,mass,size,pulsePhase,breathePhase,heat,lastPosition;
  struct Wave { float position,amplitude,wavelength,speed,age; bool active; };
  Wave waves[5];
};
FluidBlob blob;
uint32_t lastUpdate = 0;

struct RainbowPainter {
  float paintCenter,paintVelocity,centerHue;
  float mixingBuffer[144];
  bool  initialized;
  float dominantHue,dominantZoneWidth,rollShiftAccum,dominantFade;
};
RainbowPainter rainbowPainter = {0.5f,0.0f,0.0f,{0},false,0.0f,0.0f,0.0f,0.0f};

// INK DISPERSION PARTICLES
// =====================================================================
// Triggered by: sudden blob stop OR strong lateral accel (accel[0])
// Particles burst outward from blob position, diffuse, and fizzle white→purple→black
#define MAX_INK_PARTICLES 30
struct InkParticle {
  float position;   // 0-1 along staff
  float velocity;   // units/sec, fades with drag
  float life;       // 1 = fresh, 0 = dead
  float hue;        // starts near 235 (purple-white), shifts warmer as it fades
  bool  active;
};
InkParticle inkParticles[MAX_INK_PARTICLES];
float lastBlobVelocity = 0.0f;   // for stop-detection

// UTILITY
// =====================================================================
static inline float clampf(float v,float lo,float hi){ return (v<lo)?lo:((v>hi)?hi:v); }
static inline float sign(float v){ return (v>0.0f)?1.0f:((v<0.0f)?-1.0f:0.0f); }
static inline float smoothstep(float e0,float e1,float x){
  x=clampf((x-e0)/(e1-e0),0.0f,1.0f); return x*x*(3.0f-2.0f*x);
}
static inline float expDecay(float x){ return 1.0f/(1.0f+x+0.48f*x*x+0.235f*x*x*x); }

// SETUP  — LEDs first, WiFi after
// =====================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LED Staff Starting ===");
  Serial.printf("LED Configuration:\n");
  Serial.printf("  HEAD: %d LEDs (idx 0-%d)\n", HEAD_LENGTH, HEAD_LENGTH-1);
  Serial.printf("  STAFF: %d LEDs (idx %d-%d)\n", STAFF_LENGTH, STAFF_START, TAIL_START-1);
  Serial.printf("  TAIL GAP: %d black LEDs (idx %d-%d)\n", TAIL_OFFSET, TAIL_START, TAIL_START+TAIL_OFFSET-1);
  Serial.printf("  TAIL FLOWER: %d LEDs (idx %d-%d)\n", TAIL_FLOWER_LENGTH, TAIL_START+TAIL_OFFSET, NUM_LEDS-1);
  Serial.printf("  >>> PHYSICAL STRIP NEEDS: %d LEDs total <<<\n", NUM_LEDS);

  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);

  // ---- LEDs: highest priority — on before anything else ----
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(globalBrightness);
  FastLED.clear(true);
  fill_solid(leds, NUM_LEDS, CRGB(80, 0, 120));   // startup colour
  FastLED.show();

  initFluidBlob();
  lastUpdate = millis();

  // ---- Audio ----
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(MIC_PIN, ADC_11db);
  pinMode(MIC_PIN, INPUT);
  long biasSum = 0;
  for (int i=0; i<20; i++) { biasSum += analogRead(MIC_PIN); delay(1); }
  audioBias = (int)(biasSum/20);

  // ---- IMU ----
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  mpu_ok = mpu.setup(0x68);
  Serial.println(mpu_ok ? "MPU OK" : "MPU not found");

  // ---- WiFi — max 3 s wait so LEDs stay responsive ----
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  unsigned long wStart = millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-wStart<3000) delay(100);

  if (WiFi.status()==WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("WiFi: " + WiFi.localIP().toString());
    setupOTA();
    if (MDNS.begin(hostname)) Serial.println("mDNS OK");
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("PurpleStaff","staff1234");
    wifiConnected = true;
    Serial.println("AP mode: 192.168.4.1");
  }
  setupWebServer();
  Serial.println("=== Staff Ready ===");
}

// CUSTOM COLOR MODE (separate from effects)
// =====================================================================
#define MAX_GRADIENT_STOPS 8
struct GradientStop {
  float position;  // 0.0 to 1.0
  uint8_t hue;
  uint8_t sat;
  uint8_t val;
};
GradientStop gradientStops[MAX_GRADIENT_STOPS];
int numGradientStops = 2;  // Start with 2 stops (head and tail colors)

void initCustomColorMode() {
  // Initialize with current head/tail colors
  gradientStops[0].position = 0.0f;
  gradientStops[0].hue = headHue;
  gradientStops[0].sat = headSat;
  gradientStops[0].val = headVal;
  
  gradientStops[1].position = 1.0f;
  gradientStops[1].hue = tailHue;
  gradientStops[1].sat = tailSat;
  gradientStops[1].val = tailVal;
  
  numGradientStops = 2;
}

void renderCustomColorMode() {
  // Render staff gradient
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos = (float)i / (STAFF_LENGTH - 1);
    
    // Find the two gradient stops to interpolate between
    int leftIdx = 0;
    int rightIdx = 1;
    
    for (int s = 0; s < numGradientStops - 1; s++) {
      if (pos >= gradientStops[s].position && pos <= gradientStops[s + 1].position) {
        leftIdx = s;
        rightIdx = s + 1;
        break;
      }
    }
    
    float localPos = (pos - gradientStops[leftIdx].position) / 
                     (gradientStops[rightIdx].position - gradientStops[leftIdx].position);
    
    // Interpolate HSV
    uint8_t h, s, v;
    
    // Hue interpolation (handle wraparound)
    int hDiff = gradientStops[rightIdx].hue - gradientStops[leftIdx].hue;
    if (hDiff > 128) hDiff -= 256;
    if (hDiff < -128) hDiff += 256;
    h = gradientStops[leftIdx].hue + (uint8_t)(hDiff * localPos);
    
    s = gradientStops[leftIdx].sat + (uint8_t)((gradientStops[rightIdx].sat - gradientStops[leftIdx].sat) * localPos);
    v = gradientStops[leftIdx].val + (uint8_t)((gradientStops[rightIdx].val - gradientStops[leftIdx].val) * localPos);
    
    leds[HEAD_LENGTH + i] = CHSV(h, s, v);
  }
  
  // Render head and tail with current custom colors
  renderHeadAndTail();
}

// MAIN LOOP
// =====================================================================
void loop() {
  if (wifiConnected) { ArduinoOTA.handle(); server.handleClient(); }

  // WiFi reconnection watchdog — if STA drops, try to reconnect every 30 s
  if (WiFi.getMode() == WIFI_STA) {
    static uint32_t lastWiFiCheck = 0;
    uint32_t wNow = millis();
    if (wNow - lastWiFiCheck > 30000UL) {
      lastWiFiCheck = wNow;
      if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }
  }

  // Periodic invisible physics reset every 30 min — prevents any accumulated state
  // from floating-point drift making the staff look wrong over long runs
  {
    static uint32_t lastStateReset = 0;
    uint32_t rNow = millis();
    if (rNow - lastStateReset > 1800000UL) {
      lastStateReset = rNow;
      initFluidBlob();
      lastUpdate = rNow;
    }
  }

  updateButton(btn1, BUTTON_PIN_1);
  updateButton(btn2, BUTTON_PIN_2);
  updateIMU();
  processAudio();

  // Auto-cycle (only in effect mode, not custom mode)
  if (autoCycle && !blackout && !otaInProgress && !customColorMode) {
    static uint32_t lastLoopMs = 0;
    uint32_t now = millis();
    float dt = clampf((now-lastLoopMs)/1000.0f, 0.0f, 0.1f);
    lastLoopMs = now;
    autoCycleTimer += dt;
    if (autoCycleTimer >= autoCycleSeconds) {
      autoCycleTimer = 0.0f;
      currentEffect++;
      int maxE = 5; // Default
      if (currentPage == 0) maxE = 6;  // Page 1: 6 effects
      if (currentPage == 3) maxE = 10; // Page 4: 10 effects
      if (currentPage == 5) maxE = 14; // Page 6: 14 effects
      if (currentEffect>=maxE) { currentEffect=0; currentPage=(currentPage+1)%NUM_PAGES; }
    }
  }

  if (!blackout && !otaInProgress) {
    if (customColorMode) {
      // Custom color mode - solid gradient
      renderCustomColorMode();
    } else {
      // Effect mode - only fade staff area, preserve head/tail
      fadeToBlackBy(leds+HEAD_LENGTH, STAFF_LENGTH, 15);
      if (effectEnabled) renderCurrentEffect();
      else { fill_solid(leds+HEAD_LENGTH, STAFF_LENGTH, CRGB::Black); renderHeadAndTail(); }
    }
    FastLED.show();
  }
}
