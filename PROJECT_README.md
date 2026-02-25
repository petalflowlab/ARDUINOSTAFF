# Petal Flow LED Staff Project

## Overview
A battery-powered LED staff featuring 30+ audio-reactive visual effects across 6 themed pages. The staff uses an XIAO ESP32-C3 microcontroller with web-based controls and phone microphone integration for real-time music synchronization.

## Hardware Configuration

### Core Components
- **Microcontroller**: XIAO ESP32-C3
- **LEDs**: WS2812B addressable LED strip
- **Motion Sensor**: MPU6050 IMU (6-axis accelerometer + gyroscope)
- **Audio Input**: ADMP401 MEMS microphone + phone web audio
- **Power**: 18650 lithium-ion battery with MCP73871 charging IC
- **Protection**: DW01A/FS8205A battery protection circuit

### LED Layout Philosophy

**CRITICAL DESIGN PRINCIPLE**: The staff uses a **center-radiating mirrored layout** where effects radiate outward from the physical center point. This is NOT a linear strip - it's a symmetric staff with matching head and tail.

```
[HEAD FLOWER] ←─── [STAFF CENTER] ───→ [TAIL FLOWER]
    4 LEDs          119 LEDs mirror          8 LEDs
                    from center              (2x head)
```

#### LED Configuration (from `C3STAFFMAIN.ino`)
```cpp
// *** SET THESE THREE VALUES ***
#define HEAD_LENGTH     4       // LEDs in head flower
#define TAIL_START      134     // Where tail begins (sets staff length)
#define TAIL_OFFSET     10      // Black gap before tail flower

// Auto-calculated values
#define STAFF_LENGTH         (TAIL_START - HEAD_LENGTH)           // 130 LEDs
#define STAFF_START          HEAD_LENGTH                          // Staff starts after head
#define TAIL_FLOWER_LENGTH   (HEAD_LENGTH * 2)                    // 8 LEDs (tail mirrors head)
#define TAIL_LENGTH          TAIL_FLOWER_LENGTH                   // Tail flower only
#define NUM_LEDS             (TAIL_START + TAIL_OFFSET + TAIL_FLOWER_LENGTH) // Total physical LEDs
```

#### Effect Design Considerations
When creating effects, always remember:
- **Center index**: `STAFF_LENGTH/2` is the physical center of the staff
- **Mirror patterns**: Effects should be symmetric - what happens at +N from center should happen at -N
- **Radiate outward**: Particles, waves, and animations flow FROM center TO ends
- **Head & Tail match**: Visual intensity should be balanced between head (4 LEDs) and tail (8 LEDs)

**⚠️ IMPORTANT - Head/Tail Brightness Warning**: 
Setting `headVal` and `tailVal` to 255 constantly can cause the head/tail flowers to render as pure white, overriding their intended colors. Effects should modulate brightness dynamically based on audio/motion or use values in the 180-240 range to preserve color saturation.

```cpp
// ❌ BAD - May render as white
headVal = 255; tailVal = 255;

// ✅ GOOD - Preserves color while staying bright
headVal = 220 + (audioLevel * 35);  // Range: 220-255
tailVal = 220 + (audioLevel * 35);

// ✅ GOOD - Audio reactive
headVal = 180 + (beatIntensity * 75); // Range: 180-255
```

## Software Architecture

### File Structure
```
C3STAFFMAIN/
├── C3STAFFMAIN.ino          # Main setup, globals, LED config
├── effects_dispatch.ino      # Effect routing and page management
├── effects_p1.ino           # Page 1: Purple & Elemental (6 effects)
├── effects_p2.ino           # Page 2: Audio Reactive (5 effects)
├── effects_p3.ino           # Page 3: Cosmic (5 effects)
├── effects_p4.ino           # Page 4: Calming (5 effects)
├── effects_p5.ino           # Page 5: Fruity (6 effects)
├── effects_p6.ino           # Page 6: Physics & Phenomena (6 effects)
├── blob_particles.ino       # Blob rendering + head/tail animations
├── imu_audio.ino           # IMU processing + audio simulation
├── web_ui.ino              # Web server + HTML interface
└── LED_CONFIGURATION.md     # LED layout detailed guide
```

### Boot Sequence (Optimized for Speed)
Target: <500ms from power-on to LED activation

1. **Immediate LED power-up** (0ms)
   - Enable LED power via GPIO
   - Initialize FastLED
   - Show startup animation

2. **Background initialization** (50-200ms)
   - WiFi connection (non-blocking)
   - Web server setup
   - IMU initialization
   - Button setup

3. **First effect render** (~500ms)
   - System fully operational
   - All features available

## Audio System

### Dual Audio Sources
The staff supports two audio input methods:

#### 1. Simulated Audio (Default)
- Algorithmic beat generation with melody, bass, and hi-hat layers
- Used when no external audio available
- Provides consistent demo mode

#### 2. Phone Microphone (Web Audio API)
- Real-time FFT analysis (256-sample window)
- 5 frequency bands extracted:
  - **Bass**: 0-690Hz (kick drums, sub-bass)
  - **Mid-Low**: 690-1550Hz (snare, toms)
  - **Mid**: 1550-3010Hz (vocals, melody)
  - **High**: 3010-5590Hz (cymbals, hi-hats)
  - **Overall**: Full spectrum average

#### Audio Processing Chain
```
Phone Mic → Web Audio API → FFT Analysis → Frequency Bands
    ↓
Gain Boost + Floor:
  - Bass:    2.5x gain + 30% floor
  - Mid-Low: 2.5x gain + 30% floor
  - Mid:     3.0x gain + 40% floor
  - High:    4.0x gain + 50% floor
  - Overall: 2.0x gain + 30% floor
    ↓
ESP32 (/setaudio endpoint) → Global Variables
    ↓
Effect Rendering (50ms update rate)
```

#### Beat Detection Algorithm
Uses variance-based dynamic thresholding:
```javascript
// 20-sample history for variance calculation
dynamicThreshold = avgHistory + √variance × 1.2
beatDetected = (bassLevel > dynamicThreshold) && (timeSinceLast > 200ms)
beatStrength = (bassLevel - avgHistory) / (avgHistory + 0.01)
```

### Visual Audio Monitor
The web UI displays real-time audio levels when phone microphone is enabled:
- 3 frequency bar graphs (Bass, Mid, High)
- Beat indicator with strength percentage
- Updates every 50ms

## Effect Design Principles

### Baseline-First Philosophy
**CRITICAL**: Effects must NEVER go completely dark. Every effect has a strong baseline that's always visible, with audio adding excitement on top.

#### Baseline Requirements
- **Minimum brightness**: 100-160 for staff, 180-240 for head/tail
- **Always visible color**: Even in silence, the staff glows
- **Audio is additive**: Music enhances, doesn't create from nothing
- **Gentle fading**: Use slow fadeToBlackBy (15-25) to maintain presence

#### Example - Beat Sparkle Effect Structure
```cpp
// 1. Gentle fade (keeps baseline visible)
fadeToBlackBy(leds+HEAD_LENGTH, STAFF_LENGTH, 15);

// 2. Strong baseline (always visible)
for(int i=0; i<STAFF_LENGTH; i++) {
    float d = fabs((float)i - ci) / (float)ci;
    uint8_t val = (uint8_t)(160 - d*50 + audioLevel*95);  // 110-255 range
    leds[HEAD_LENGTH+i] = CHSV(220, 200, val);
}

// 3. Audio enhancements (additive on baseline)
if(pseudoBeat && beatIntensity > 0.15f) {
    // Add sparks, flashes, particles ON TOP
    leds[pos] += CHSV(hue, sat, val);  // Note the +=
}
```

### Center-Radiating Patterns
```cpp
int ci = STAFF_LENGTH/2;  // Center index

// Symmetric mirrored pattern
for(int i=0; i<ci; i++) {
    float distance = i / (float)ci;  // 0.0 at center, 1.0 at ends
    
    // Apply same effect symmetrically
    leds[HEAD_LENGTH + ci + i] = effect;  // Right side
    leds[HEAD_LENGTH + ci - i] = effect;  // Left side (mirror)
}
```

### Head & Tail Reactivity
The `updateHeadTailReactivity()` function creates breathing animations that respond to audio and motion:

```cpp
struct HeadTailState {
    float headImpactIntensity;    // 0-1, set by effects on beats
    float tailImpactIntensity;    // 0-1, usually matches head
    float headBreathPhase;        // Autonomous breathing animation
    float tailBreathPhase;
};

// Called by effects to trigger reactions
htState.headImpactIntensity = beatIntensity;
htState.tailImpactIntensity = beatIntensity;
updateHeadTailReactivity(dt, breathSpeed, imuInfluence);
```

## Web Interface

### Control Features
- **6 page navigation** - Switch between themed effect pages
- **Effect grid** - 3×2 grid showing all effects on current page
- **Mode toggle** - Switch between Effects mode and Custom Color mode
- **Phone microphone** - Enable/disable web audio with visual monitor
- **Brightness presets** - 70% (180) or 100% (255)
- **Effect On / Blackout** - Main power toggle

### Custom Color Mode
Allows manual control of staff appearance:
- **Head & Tail Color**: HSV sliders (Hue, Saturation, Brightness)
- **Staff Gradient**: Position + Hue sliders to place color stops
- **Add Color**: Create gradient stops along staff
- **Reset Gradient**: Clear to simple head→tail gradient

### Live Staff Preview
Visual representation shows:
- Head flower (left)
- Center staff bar with gradient
- Tail flower (right)
- Real-time color updates as you adjust controls

### OTA Updates
Firmware updates via web interface:
- Upload `.bin` file through web UI
- Progress indicator during upload
- Automatic reboot after successful update

## Effect Pages

### Page 1: Purple & Elemental (6 effects)
0. Purple Blob - Smooth traveling blob with trail
1. Purple Rain - Falling particles
2. Purple Waves - Sine wave patterns
3. Fire - Heat-based fire simulation
4. Ocean Waves - Blue wave patterns
5. Lightning - Electric discharge effects

### Page 2: Audio Reactive (5 effects)
0. Audio Pulse - Breathing pulses synced to music
1. Beat Sparkle - Explosive sparks on bass hits with frequency layers
2. Sound Wave - Audio waveform visualization
3. Color Shift - Rainbow that speeds up with audio
4. Audio Fire - Fire that flares with beats

### Page 3: Cosmic (5 effects)
0. Starfield - Twinkling stars
1. Galaxy Swirl - Rotating nebula patterns
2. Comet Trail - Fast-moving comet
3. Supernova - Explosive burst
4. Wormhole - Spiraling vortex

### Page 4: Calming (5 effects)
0. Gentle Breath - Slow breathing animation
1. Soft Glow - Ambient pulsing
2. Twilight - Sunset gradient
3. Zen Garden - Peaceful ripples
4. Northern Lights - Aurora patterns

### Page 5: Fruity (6 effects)
0. Strawberry - Red with green accents
1. Orange - Orange gradient
2. Banana - Yellow glow
3. Lime - Bright green
4. Blueberry - Deep blue
5. Grape - Purple clusters

### Page 6: Physics & Phenomena (6 effects)
0. Gravity Wells - Particle attraction
1. Quantum Foam - Flickering quantum states
2. Heat Diffusion - Temperature spreading
3. Wave Interference - Intersecting waves
4. Magnetic Field - Field line visualization
5. Aurora Physics - Particle collision glow

## Physical Buttons

### Single Click
Cycles to next effect on current page:
- White flash feedback
- Wraps around at end of page
- Works in both Effects and Custom modes

### Double Click
Changes to next page:
- Two cyan flashes feedback
- Cycles through all 6 pages
- Resets to first effect on new page

## Network Configuration

### WiFi Connection
```cpp
const char *ssid = "CGN3-4400";
const char *password = "XXXXXXXXXX";  // Hidden in actual code
```

### Access Points
- **HTTP**: `http://[IP_ADDRESS]`
- **mDNS**: `http://purplestaff.local` (may work for secure context)

### API Endpoints
- `GET /` - Main web interface
- `GET /api/status` - JSON status (page, effect, mode)
- `POST /seteffect?page=X&effect=Y` - Change effect
- `POST /setbrightness?v=X` - Set brightness (0-255)
- `POST /togglecustom` - Toggle custom color mode
- `POST /setheadcolor?h=X&s=Y&v=Z` - Set head HSV
- `POST /settailcolor?h=X&s=Y&v=Z` - Set tail HSV
- `POST /toggleeffect` - Toggle effect enabled
- `POST /toggleblackout` - Toggle blackout mode
- `POST /setaudio?level=X&bass=Y&mid=Z&high=W&beat=B&strength=S` - Audio data
- `POST /addgradientstop?pos=X&h=Y&s=Z&v=W` - Add gradient color
- `POST /cleargradient` - Reset gradient to default

## Development Notes

### Adding New Effects

1. **Choose the appropriate page file** (`effects_pX.ino`)
2. **Follow center-radiating pattern**:
```cpp
void renderMyNewEffect(float dt) {
    int ci = STAFF_LENGTH/2;
    fadeToBlackBy(leds+HEAD_LENGTH, STAFF_LENGTH, 20);
    
    // Always provide baseline
    for(int i=0; i<STAFF_LENGTH; i++) {
        float d = fabs((float)i - ci) / (float)ci;
        leds[HEAD_LENGTH+i] = CHSV(hue, 200, 120 - d*40);  // Never dark
    }
    
    // Add audio/motion reactivity on top
    if(pseudoBeat && beatIntensity > 0.2f) {
        // Symmetric additions from center
        leds[HEAD_LENGTH+ci] += CHSV(255, 200, beatIntensity*200);
    }
    
    // Update head/tail
    if(!customColorMode) {
        headHue = myHue; headSat = 200; headVal = 220;  // Not 255!
        tailHue = myHue; tailSat = 200; tailVal = 220;
    }
    updateHeadTailReactivity(dt, 0.6f, 0);
    renderHeadAndTail();
}
```

3. **Add to dispatch** in `effects_dispatch.ino`
4. **Add metadata** to web UI pages array

### Performance Optimization
- Boot time target: <500ms
- LED update rate: 60 FPS (16.67ms frame time)
- Audio update rate: 50ms (20 Hz)
- Web UI polling: 3 seconds

### Memory Considerations
- ESP32-C3 has limited RAM (~400KB)
- Avoid dynamic allocation in render loops
- Use `static` for effect state variables
- Pre-allocate arrays at compile time

## Troubleshooting

### Phone Audio Not Working
1. Check browser console for errors
2. Ensure microphone permissions granted
3. Try accessing via mDNS: `http://purplestaff.local`
4. HTTPS may be required - some browsers restrict `getUserMedia()` to secure contexts
5. Watch audio monitor bars - they should show 30-50% minimum even in silence

### Effects Too Dim
- Check global brightness (should be 180-255)
- Verify effect has proper baseline (100-160 range)
- Ensure fadeToBlackBy value isn't too high (keep 15-25)
- Head/tail should be 180-240, not always 255

### LEDs Not Responding
1. Check LED power enable GPIO
2. Verify `NUM_LEDS` matches physical strip
3. Check data line connection
4. Ensure adequate power supply (WS2812 can draw 60mA per LED at full white)

### Web UI Not Loading
1. Check WiFi credentials in code
2. Verify ESP32 is connected to network
3. Check serial monitor for IP address
4. Try mDNS if IP access fails
5. Hard refresh browser (Ctrl+Shift+R)

## Future Enhancements

### Potential Features
- [ ] Bluetooth audio streaming
- [ ] Persistent settings in EEPROM
- [ ] Custom effect upload via web UI
- [ ] Multi-staff synchronization
- [ ] Battery level indicator
- [ ] Sleep mode with wake-on-motion
- [ ] MQTT integration for smart home control
- [ ] SD card for effect storage
- [ ] More sophisticated beat detection (BPM tracking)

### Hardware Upgrades
- [ ] Larger battery capacity
- [ ] USB-C charging
- [ ] RGB status LED
- [ ] Capacitive touch controls
- [ ] Better microphone placement

## Credits & License

**Project**: Petal Flow LED Staff  
**Hardware**: XIAO ESP32-C3, WS2812B LEDs, MPU6050  
**Libraries**: FastLED, ESP32 WiFi, ArduinoJson  
**Developed**: 2025  

This is a custom personal project. Code and documentation provided as-is.

---

## Quick Reference

### Important Constants
```cpp
#define HEAD_LENGTH 4
#define TAIL_START 134
#define TAIL_OFFSET 10
#define NUM_LEDS 152          // Total physical LEDs
#define STAFF_LENGTH 130      // Active animation area
#define DEFAULT_BRIGHTNESS 170
```

### Key Functions
```cpp
void renderEffectX(float dt);                          // Effect render function
void updateHeadTailReactivity(float dt, float speed, float imu);
void renderHeadAndTail();                              // Draw head/tail flowers
void processAudio();                                   // Audio processing
void updateButton(ButtonState &btn, int pin);          // Button handling
```

### Color Tips
- Purple theme: Hue 220-240
- Use CHSV for smooth color transitions
- Reserve pure white (sat=0) for special accents
- Keep head/tail val in 180-240 range to preserve color

### Center-Radiating Math
```cpp
int ci = STAFF_LENGTH/2;           // Center index
float d = abs(i - ci);              // Distance from center
float normalized = d / (float)ci;   // 0.0-1.0 range
```
