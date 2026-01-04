# Example 3: Rhythmic Processing and Clock Sync

**Goal**: Master clock-synchronized granulation for rhythmic and percussive material.

**Difficulty**: Intermediate  
**Estimated Time**: 25 minutes

---

## Required Modules

- **Tapestry**
- **Clock/Sequencer** (e.g., Impromptu Clocked, VCV CLKD)
- **Drum/Percussion source** (or pre-recorded sample)
- **VCV Audio** interface

---

## Part 1: Grain Next Mode (Density < 50%)

### Concept

When Density is below 50% and a clock is connected, Tapestry enters **Grain Next Mode**:
- Each clock pulse triggers a new grain
- Grains are independent events (no playback adaptation)
- Perfect for rhythmic, stuttering effects

### Basic Setup

```
[Clock] --CLK OUT--> [Tapestry CLK IN]
[Drums] ------------> [Tapestry AUDIO IN]
```

### Step 1: Record Drum Loop

1. **Source**: Use a drum machine or load a drum sample
2. **Record** a 1-4 bar drum loop
3. **Create splices** at each beat or hit:
   - Play the recording
   - Click SPLICE at each kick, snare, hat, etc.
   - Aim for 8-16 splices

### Step 2: Configure for Grain Next

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Grain Size | 5-15% | Short grains (10-50ms) |
| Density | 20-30% | Grain Next mode, low overlap |
| Speed | 0% | Normal playback |
| Scan | 0% | No offset |

### Step 3: Connect Clock

1. **Add clock module** (e.g., Clocked)
2. Set clock rate: **1/16 notes** (fast)
3. **Connect**: Clock 1/16 OUT → Tapestry CLK

### What You'll Hear

- Drum hits retriggered at 16th note rate
- Rhythmic stuttering effect
- Each grain is independent

**This creates rhythmic grain triggering!**

---

## Part 2: Time Stretch Mode (Density > 50%)

### Concept

When Density is above 50% and clock is connected, Tapestry enters **Time Stretch Mode**:
- Playback adapts to match clock tempo
- Multiple overlapping voices
- Smooth pitch preservation
- Perfect for melodic content, pads

### Setup

```
[Clock] --CLK OUT--> [Tapestry CLK IN]
[Synth] ------------> [Tapestry AUDIO IN]
(Record a melodic phrase)
```

### Step 1: Record Melodic Loop

1. **Record a 1-bar melodic phrase** (synth, vocal, etc.)
2. **Create one splice** around the entire phrase
3. This splice will be time-stretched

### Step 2: Configure for Time Stretch

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Grain Size | 25-35% | Medium grains for smoothness |
| Density | 70-80% | Time Stretch mode, high overlap |
| Speed | 0% | Normal playback |
| Scan | 0% | No offset |

### Step 3: Tempo Sync

1. **Connect clock** at 1/bar rate
2. **Start playback**
3. **Change clock tempo** → phrase tempo follows!

### Results

- Phrase plays back at clock tempo
- Pitch remains constant
- Smooth time compression/expansion
- 3-4 overlapping voices for smoothness

---

## Part 3: Advanced Rhythmic Techniques

### Technique 1: Euclidean Grain Triggers

**Goal**: Create complex polyrhythms

**Patch**:
```
[Clock] → [Euclidean Rhythm] → [Tapestry CLK]
          Pattern: [5, 8]
          (5 hits in 8 steps)
```

**Settings**:
- Grain Size: 8%
- Density: 25%
- Record: Drum hits or melodic stabs

**Result**: Irregular, syncopated grain triggering

### Technique 2: Multi-Clock Chaos

**Goal**: Independent rhythmic control

**Patch**:
```
[Clock] → Divider:
  /1 → Tapestry CLK (grain trigger, fast)
  /4 → Tapestry NEXT (splice advance, slow)
  /8 → Tapestry SPLICE (create markers, very slow)
```

**Result**: Self-organizing polyrhythmic structures

### Technique 3: Swing and Groove

**Goal**: Add swing to grain triggering

**Patch**:
```
[Clock with Swing] → [Tapestry CLK]
Set swing amount to 60-70%
```

**Result**: Grains have swung timing, groovy feel

---

## Part 4: Percussive Sound Design

### Kick Drum Multiplier

**Goal**: Create kick drum rolls and variations

1. **Record**: Single kick drum hit
2. **Create splice**: Around the kick (tight boundaries)
3. **Settings**:
   - Grain Size: 5%
   - Density: 15%
   - Speed: -20% (pitched down)
4. **Clock**: Connect at 1/16 or 1/32 rate
5. **Result**: Rapid kick drum rolls

**Variation**: Modulate Speed with envelope for pitch dives

### Hi-Hat Variations

**Goal**: Generate hi-hat patterns from one sample

1. **Record**: Single hi-hat sound
2. **Create 4-8 splices** at different positions
3. **Settings**:
   - Grain Size: 3%
   - Density: 10%
4. **Use Select CV**: 
   - Connect step sequencer to Select CV
   - Program different splice selections
5. **Clock**: 1/8 or 1/16 notes
6. **Result**: Programmed hi-hat patterns

### Snare Scatter

**Goal**: Create snare fills and variations

1. **Record**: 1-bar snare pattern
2. **Create 16 splices** (one per 16th note)
3. **Settings**:
   - Grain Size: 10%
   - Density: 20%
4. **Clock**: Random triggers (e.g., Bernoulli gate)
5. **Result**: Probabilistic snare scatters

---

## Part 5: Performance Patterns

### Pattern 1: Build-Up Effect

**Goal**: Increase grain density for build-up

**Patch**:
```
[Clock] → Tapestry CLK
[Envelope] → Tapestry MORPH CV
```

1. **Trigger envelope** (rise over 8-16 bars)
2. **Density increases** from 10% to 80%
3. **Grain density builds** creating tension

### Pattern 2: Drop Effect

**Goal**: Rhythmic stuttering drop

**Technique**:
1. At the drop, **reduce Grain Size** to 3-5%
2. **Clock rate**: Very fast (1/32 notes)
3. **Density**: 15-20%
4. **Result**: Machine-gun stutter effect

### Pattern 3: Breakdown

**Goal**: Sparse, glitchy breakdown section

1. **Set Grain Size**: 8%
2. **Set Density**: 10% (single voice, sparse)
3. **Clock**: Irregular (use random gates)
4. **Use NEXT**: Manual or clock-triggered splice jumps
5. **Result**: Fragmented, glitchy texture

---

## Complete Rhythmic Patch Example

### Setup

```
[Drums] ---------> [Tapestry] ---------> [Audio Out]
                    ↑     ↑     ↑
[Clock/1] ---------+     |     |
                         |     |
[Clock/4] ---------------+     |
                               |
[Sequencer] --ORGANIZE CV------+
```

### Roles

- **Clock/1**: Grain triggering (16th notes)
- **Clock/4**: Marker advancement (quarter notes)
- **Sequencer**: Programmed splice selection

### Parameters

| Parameter | Value |
|-----------|-------|
| Grain Size | 10% |
| Density | 25% |
| Speed | 0% |
| Scan | 0% |

### What It Does

- Grains trigger at 16th note rate
- Every quarter note, advance to next splice
- Sequencer programs which splices play
- Result: Programmed rhythmic variations

---

## Clock Rate Guide

### Common Clock Rates for Grain Shift Mode

| Rate | Effect |
|------|--------|
| 1/4 notes | Sparse, punctuated grains |
| 1/8 notes | Moderate rhythmic density |
| 1/16 notes | Busy, stuttering effect |
| 1/32 notes | Machine-gun, very dense |
| 1/64 notes | Extreme stutter, noise-like |

### Common Clock Rates for Time Stretch Mode

| Rate | Effect |
|------|--------|
| 1/bar | Match phrase to bar length |
| 1/2 notes | Double-time playback |
| 1/4 notes | Phrase plays at quarter note rate |
| 1/8 notes | Very fast playback |

---

## Tips and Best Practices

### Recording for Rhythmic Processing

**Do**:
- ✅ Record tight, punchy material
- ✅ Use clean, dry sounds (add FX later)
- ✅ Record at consistent level
- ✅ Create splices at meaningful boundaries

**Don't**:
- ❌ Record with reverb (adds artifacts)
- ❌ Use overly compressed material
- ❌ Record too quietly (low SNR)

### Marker Placement for Drums

**Best practices**:
- Place splices at transient peaks (kick, snare hits)
- Avoid placing splices in decay tails
- Use 8, 16, or 32 splices for musical divisions
- Create splices at zero-crossings for cleanest results

### Grain Size for Percussive Material

| Material | Grain Size |
|----------|-----------|
| Kick drums | 5-10% |
| Snares | 8-15% |
| Hi-hats | 3-8% |
| Cymbals | 10-20% |
| Drum loops | 5-15% |

---

## Troubleshooting

### Issue: Grains Sound Clicky

**Cause**: Grain Size too small

**Fix**:
- Increase Grain Size to 8%+
- Ensure splices are on zero-crossings

### Issue: Clock Sync Not Working

**Diagnosis**:
1. Check Density value (must be appropriate for mode)
2. Verify clock signal (0-10V gates)
3. Check Grain Size (not too large, < 50%)

**Fix**:
- Set Density to 25% (Grain Shift) or 75% (Time Stretch)
- Use scope to verify clock pulses
- Reduce Grain Size

### Issue: Tempo Not Matching

**For Time Stretch Mode**:
- Ensure Density > 50%
- Check splice length (must be reasonable)
- Verify clock is stable

---

## Advanced Concepts

### Clock Divisions and Musical Time

```
Master Clock (1 bar) = 4 beats
  /1 = 1 bar (whole note)
  /2 = half note
  /4 = quarter note (beat)
  /8 = 8th note
  /16 = 16th note
  /32 = 32nd note
```

### Calculating Grain Size for Musical Timing

```
Grain Size (samples) = (60 / BPM) * Sample Rate * Note Division
Example: 120 BPM, 1/16 note
= (60 / 120) * 48000 * 0.25
= 6000 samples
= ~125ms
= ~18% Grain Size (at 48kHz)
```

---

## Next Steps

1. **Experiment** with different clock sources
2. **Combine** with [Expander Effects](07_expander_effects.md)
3. **Explore** [Live Performance](05_live_performance.md) techniques
4. **Read** [Advanced Usage](../ADVANCED_USAGE.md) for more ideas

---

## Learning Objectives

After completing this example, you should be able to:
- ✅ Use Grain Next mode for rhythmic effects
- ✅ Use Time Stretch mode for tempo sync
- ✅ Create complex polyrhythms
- ✅ Process drums and percussion granularly
- ✅ Build performance-ready rhythmic patches

---

**Master the rhythm!** 🥁

[← Previous Example](02_granular_textures.md) | [Back to Examples](README.md) | [Next Example →](04_vocal_processing.md)
