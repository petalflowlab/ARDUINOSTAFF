# LED Strip Configuration Guide

## The Simple Truth

**You set where the tail starts. The physical LED count calculates from that.**

```cpp
#define HEAD_LENGTH     4       // LEDs in head flower
#define TAIL_START      123     // Where you want tail to begin
#define TAIL_OFFSET     10      // Black gap before tail
```

**The code tells you how many LEDs you need to build:**
- NUM_LEDS = TAIL_START + TAIL_OFFSET + (HEAD_LENGTH × 2)
- With values above: **141 LEDs needed**

## How It Works

1. **You decide:** "I want the tail to start at LED 123"
2. **Code calculates:**
   - Staff = 123 - 4 = **119 LEDs**
   - Tail gap = **10 LEDs**  
   - Tail flower = 4 × 2 = **8 LEDs**
   - **Total needed: 141 LEDs**
3. **You build:** A strip with exactly 141 LEDs

## Examples

### Want 100 LED Staff?
```cpp
#define HEAD_LENGTH     4
#define TAIL_START      104     // 4 + 100
#define TAIL_OFFSET     10
// Result: Need 122 LED strip (4 + 100 + 10 + 8)
```

### Want 150 LED Staff?
```cpp
#define HEAD_LENGTH     4
#define TAIL_START      154     // 4 + 150
#define TAIL_OFFSET     10
// Result: Need 172 LED strip (4 + 150 + 10 + 8)
```

### Short 50 LED Staff?
```cpp
#define HEAD_LENGTH     4
#define TAIL_START      54      // 4 + 50
#define TAIL_OFFSET     5
// Result: Need 67 LED strip (4 + 50 + 5 + 8)
```

### Small Gap?
```cpp
#define HEAD_LENGTH     4
#define TAIL_START      123
#define TAIL_OFFSET     3       // Small gap
// Result: Need 134 LED strip (4 + 119 + 3 + 8)
```

## Configuration Steps

1. **Decide staff length** - how long you want the middle section
2. **Calculate TAIL_START** = 4 + desired_staff_length
3. **Set TAIL_OFFSET** - gap size you want (5-15 typical)
4. **Upload and check Serial Monitor** - it tells you how many LEDs needed
5. **Build strip** with that exact LED count

## Serial Monitor Shows Required Count

On boot at 115200 baud:
```
LED Configuration:
  HEAD: 4 LEDs (idx 0-3)
  STAFF: 119 LEDs (idx 4-122)
  TAIL GAP: 10 black LEDs (idx 123-132)
  TAIL FLOWER: 8 LEDs (idx 133-140)
  >>> PHYSICAL STRIP NEEDS: 141 LEDs total <<<
```

**That last line tells you how many LEDs to use!**

## The Philosophy

- **TAIL_START** defines your design intent
- **NUM_LEDS** is the consequence
- You design the layout, the code tells you the requirement
- No counting needed - just set where you want things to be

## Quick Reference

```
TAIL_START = HEAD_LENGTH + desired_staff_length
NUM_LEDS = TAIL_START + TAIL_OFFSET + (HEAD_LENGTH × 2)

Example: Want 100 LED staff with 10 LED gap?
TAIL_START = 4 + 100 = 104
NUM_LEDS = 104 + 10 + 8 = 122 LEDs needed
```

