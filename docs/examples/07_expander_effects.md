# Example 7: Expander Effects Chain

**Goal**: Master the Tapestry Expander for bit crushing and analog filtering effects.

**Difficulty**: Beginner-Intermediate  
**Estimated Time**: 15 minutes

---

## Required Modules

- **Tapestry** (main module)
- **Tapestry Expander** (effects module)
- **Audio source** (any)
- **VCV Audio** interface
- **Optional**: LFO for modulation

---

## Part 1: Setting Up the Expander

### Physical Placement

**IMPORTANT**: The expander MUST be placed directly to the right of Tapestry.

```
[Tapestry] [Tapestry Expander] [Other Modules]
```

### Verifying Connection

1. **Add both modules** to your patch
2. **Position expander** immediately right of Tapestry
3. **Check CONNECTED light** on expander (should be lit)
4. **No cables needed** between them (auto-routing)

---

## Part 2: Bit Crusher Basics

### What is Bit Crushing?

Bit crushing reduces:
1. **Bit depth**: Quantization levels (16 bits → 8 bits → 4 bits, etc.)
2. **Sample rate**: Effective sampling rate (causes aliasing)

**Result**: Lo-fi, digital distortion character

### Basic Bit Crusher Setup

1. **Record or play** audio through Tapestry
2. **On Expander**, set:
   - BITS: 8 bits (50% position)
   - RATE: 70% (slight reduction)
   - MIX: 50% (equal dry/wet blend)

### What You'll Hear

- Warmer, lo-fi character
- Slight aliasing artifacts
- "Digital" quality reminiscent of vintage samplers

---

## Part 3: Extreme Bit Crushing

### Heavy Lo-Fi Effect

**Settings**:
- BITS: 4 bits (25% position)
- RATE: 40% (aggressive reduction)
- MIX: 80% (mostly wet)

**Character**:
- Heavy digital distortion
- Strong aliasing
- Retro video game / chip-tune aesthetic

### Glitch/Noise Effect

**Settings**:
- BITS: 1-2 bits (very low)
- RATE: 10-20% (extreme reduction)
- MIX: 100% (fully wet)

**Character**:
- Extreme distortion
- Noise-like, robotic
- Unusable for subtle processing, great for sound design

---

## Part 4: Moog VCF Filter Basics

### What is the Moog VCF?

- **Type**: Resonant lowpass filter
- **Slope**: 24dB/octave (4-pole)
- **Character**: Analog modeling of classic Moog ladder filter
- **Sound**: Warm, musical, can self-oscillate

### Basic Filter Setup

1. **On Expander**, set:
   - CUTOFF: 40% (~800Hz)
   - RESO: 50% (moderate resonance)
   - MIX: 70% (mostly filtered)

### What You'll Hear

- Reduced high frequencies
- Resonant peak at cutoff frequency
- Warm, analog character

---

## Part 5: Filter Sweep Techniques

### Manual Filter Sweep

1. **Start**: CUTOFF at 10% (very dark)
2. **Slowly turn** CUTOFF up to 90% (bright)
3. **Listen** for resonant peak moving through spectrum

**Use case**: Classic filter sweep, DJ mixing transitions

### LFO-Modulated Filter Sweep

**Patch**:
```
[LFO] --TRI OUT--> [Expander CUTOFF CV]
```

**Settings**:
- LFO Rate: 0.25 Hz (4-second cycle)
- LFO Waveform: Triangle
- LFO Range: 0-10V (unipolar)
- CUTOFF Base: 20%
- RESO: 70%
- MIX: 100%

**Result**: Automatic wah-wah effect

---

## Part 6: Resonance Techniques

### Moderate Resonance (Musical)

**Settings**:
- CUTOFF: 30-50%
- RESO: 40-60%
- MIX: 80%

**Character**:
- Emphasized frequencies at cutoff
- Tonal boost, musical quality
- Good for basslines, leads

### High Resonance (Vowel-Like)

**Settings**:
- CUTOFF: 25-35%
- RESO: 75-85%
- MIX: 90%

**Character**:
- Strong resonant peak
- Vowel-like formants
- "Talking" filter effect

### Self-Oscillation (Extreme)

**Settings**:
- CUTOFF: Any position
- RESO: 95-100%
- MIX: 100%

**Character**:
- Filter rings, produces sine tone
- Pitch follows CUTOFF position
- Can be played as oscillator
- Mix with input for ring mod effects

---

## Part 7: Combined Effects

### Recipe 1: Lo-Fi Filtered Texture

**Goal**: Warm, vintage sampler sound

**Settings**:
| Control | Value |
|---------|-------|
| Crush BITS | 10 bits |
| Crush RATE | 80% |
| Crush MIX | 50% |
| Filter CUTOFF | 45% |
| Filter RESO | 55% |
| Filter MIX | 70% |
| Output Level | 110% |

**Character**: Warm, lo-fi, analog-digital hybrid

### Recipe 2: Harsh Digital Grind

**Goal**: Aggressive, industrial sound

**Settings**:
| Control | Value |
|---------|-------|
| Crush BITS | 3 bits |
| Crush RATE | 30% |
| Crush MIX | 90% |
| Filter CUTOFF | 60% |
| Filter RESO | 80% |
| Filter MIX | 60% |
| Output Level | 150% |

**Character**: Harsh, distorted, aggressive

### Recipe 3: Subtle Enhancement

**Goal**: Add character without obviousness

**Settings**:
| Control | Value |
|---------|-------|
| Crush BITS | 12 bits |
| Crush RATE | 85% |
| Crush MIX | 25% |
| Filter CUTOFF | 55% |
| Filter RESO | 35% |
| Filter MIX | 40% |
| Output Level | 100% |

**Character**: Subtle warmth, slight coloration

---

## Part 8: CV Modulation Examples

### Modulation 1: Envelope-Controlled Filter

**Patch**:
```
[Tapestry ENV OUT] → [Expander CUTOFF CV]
```

**Purpose**: Filter opens and closes with grain envelope

**Settings**:
- Base CUTOFF: 20%
- RESO: 60%
- MIX: 100%

**Result**: Classic envelope-filter effect

### Modulation 2: LFO Resonance Modulation

**Patch**:
```
[LFO] --SINE--> [Expander RESO CV]
```

**Settings**:
- LFO Rate: 1 Hz
- Base RESO: 40%
- CUTOFF: 35%
- MIX: 80%

**Result**: Vowel-like sweeping, formant motion

### Modulation 3: Stepped Cutoff (Sample & Hold)

**Patch**:
```
[Random] ---> [S&H] ---> [Expander CUTOFF CV]
[Clock] -----> [S&H TRIG]
```

**Settings**:
- Clock: 1/4 notes
- Base CUTOFF: 30%
- RESO: 65%

**Result**: Rhythmic, randomized filter changes

### Modulation 4: Bit Depth Modulation

**Patch**:
```
[LFO] --SAW--> [Expander BITS CV]
```

**Settings**:
- LFO Rate: 0.5 Hz
- Base BITS: 16 (max)
- RATE: 90%
- MIX: 60%

**Result**: Bit depth sweeps from clean to crushed

---

## Part 9: Performance Techniques

### Technique 1: Live Filter Sweeps

1. **Map CUTOFF** to MIDI controller or expression pedal
2. **Set RESO** to 70%
3. **Perform** sweeps during transitions or builds
4. **Add** automation recording for repeatable moves

### Technique 2: Effect Bypass Macro

**Goal**: Quick A/B comparison

1. **Set both MIX controls** to 0% for bypass
2. **Map both MIX** to single macro control
3. **Sweep macro** from 0-100% for gradual effect fade-in

### Technique 3: Parallel Processing

**Goal**: Retain original signal while adding effect

1. **Set MIX controls** to 30-40%
2. **Use aggressive settings** (low BITS, high RESO)
3. **Result**: "New York" style parallel compression/distortion

---

## Part 10: Integration with Tapestry

### Granular + Bit Crusher

**Settings**:
| Module | Parameter | Value |
|--------|-----------|-------|
| Tapestry | Grain Size | 10% |
| Tapestry | Density | 40% |
| Expander | BITS | 6 |
| Expander | RATE | 50% |
| Expander | Crush MIX | 70% |

**Result**: Grainy, lo-fi granular texture

### Granular + Filter Sweep

**Patch**:
```
[Tapestry] → [Expander] → [Audio Out]
[LFO] → Tapestry SLIDE CV
[LFO] → Expander CUTOFF CV (inverted phase)
```

**Result**: Grain position and filter cutoff move in opposite directions

### Time Stretch + Resonant Filter

**Purpose**: Vocal/melodic time stretching with tonal emphasis

**Settings**:
- Tapestry in Time Stretch mode (Density > 50%)
- Filter CUTOFF: Follow vocal formants (vowel frequencies)
- Filter RESO: 70%

**Result**: Enhanced, resonant time-stretched audio

---

## Output Level Control

### Purpose

The OUTPUT LEVEL control provides final gain adjustment after effects.

**Ranges**:
- 0-100%: Attenuation
- 100%: Unity gain (no change)
- 100-200%: Boost (up to +6dB)

### When to Adjust

**Reduce** (< 100%) when:
- Effects add gain (bit crusher, resonance)
- Signal is clipping
- Mixing with other modules

**Increase** (> 100%) when:
- Effects reduce level (heavy filtering)
- Need extra headroom
- Matching levels with other modules

---

## Tips and Best Practices

### Effect Order

Effects are processed in this order:
1. **Bit Crusher** first
2. **Moog VCF** second
3. **Output Level** last

**Implication**: Filtered bit crusher, not crushed filter

### CPU Considerations

- Bit crusher: Minimal CPU
- Moog VCF: Moderate CPU (per-sample filtering)
- Total expander overhead: ~2-3% CPU

### Mix Control Strategy

**Parallel mixing** (MIX < 100%):
- Preserves transients and dynamics
- More natural sound
- "Enhances" rather than "replaces"

**Serial mixing** (MIX = 100%):
- Full commitment to effect
- More extreme processing
- Best for creative sound design

---

## Troubleshooting

### Issue: Expander Not Working

**Check**:
- ✅ Expander is immediately right of Tapestry
- ✅ CONNECTED light is lit
- ✅ MIX controls are above 0%
- ✅ Tapestry is generating audio

### Issue: Harsh, Clipping Sound

**Cause**: Resonance or bit crusher adding gain

**Fix**:
- Reduce OUTPUT LEVEL to 70-80%
- Lower RESO below 90%
- Reduce Crush MIX

### Issue: No Sound

**Cause**: Filter CUTOFF too low, or MIX at 0%

**Fix**:
- Increase CUTOFF above 20%
- Set Filter MIX above 0%
- Check Tapestry output with scope

---

## Next Steps

1. **Combine** with [Granular Textures](02_granular_textures.md)
2. **Integrate** into [Live Performance](05_live_performance.md) setups
3. **Experiment** with [Sound Design](06_sound_design.md) applications
4. **Read** [Advanced Usage](../ADVANCED_USAGE.md) for more techniques

---

## Learning Objectives

After completing this example, you should be able to:
- ✅ Connect and configure Tapestry Expander
- ✅ Use bit crusher for lo-fi effects
- ✅ Use Moog VCF for analog filtering
- ✅ Combine effects for complex processing
- ✅ Modulate effects with CV
- ✅ Integrate expander into performance patches

---

**Enjoy sculpting your sound!** 🎛️

[← Previous Example](02_granular_textures.md) | [Back to Examples](README.md)
