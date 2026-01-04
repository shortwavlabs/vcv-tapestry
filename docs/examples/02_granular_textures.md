# Example 2: Granular Textures and Ambient Soundscapes

**Goal**: Create lush, evolving ambient textures using granular synthesis techniques.

**Difficulty**: Intermediate  
**Estimated Time**: 20 minutes

---

## Required Modules

- **Tapestry**
- **Audio source** (synth voice, sample, or field recording)
- **LFO** (e.g., VCV LFO or Bogaudio LLFO)
- **VCV Audio** interface
- **Optional**: Reverb module for additional depth

---

## Patch Diagram

```
[Synth/Sample] --> [Tapestry] --> [Reverb] --> [Audio Out]
                    ↑
[LFO] ----SLIDE CV--+
[LFO] ---MORPH CV---+
```

---

## Part 1: Recording Source Material

### Step 1: Choose Your Source

**Option A: Single Note Pad**
```
[VCV VCO-1] → Tapestry IN
Set VCO to sine or triangle wave
Frequency: ~C3 (130.8 Hz)
```

**Option B: Melodic Phrase**
```
[Plaits] → Tapestry IN
Set to granular cloud model
Record a 15-second improvisation
```

**Option C: Field Recording**
```
Load a WAV file with natural sounds
(wind, water, voices, etc.)
```

### Step 2: Record

1. Set **Mix** to 50%
2. Set **Overdub** to OFF
3. Click **REC**
4. Record 20-30 seconds of material
5. Stop recording

**Tip**: For best results, record sustained or slowly evolving sounds.

---

## Part 2: Basic Granular Texture

### Parameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Grain Size | 25% | Medium grains (~50-100ms) |
| Density | 60% | 3 overlapping voices |
| Speed | 0% | Normal speed |
| Scan | 0% | No offset (yet) |

### Results

You should hear:
- Smooth, continuous texture
- 2-3 grain voices overlapping
- Recognizable source material

**This is your baseline granular sound.**

---

## Part 3: Adding Motion with Scan

### Setup

1. **Add LFO module** (e.g., VCV LFO)
2. **Connect**: LFO TRI OUT → Tapestry SLIDE CV
3. **Set LFO**: 
   - Frequency: 0.1 Hz (10-second cycle)
   - Waveform: Triangle
   - Unipolar (0-5V output)

### Scan CV Attenuverter

1. Set **Scan knob** to 0%
2. Set **Scan CV Atten** to +1.0 (full positive)

### What You'll Hear

- Grains slowly scan through the recorded material
- Position sweeps from start to end and back
- Creates "frozen time" effect
- Very suitable for pads and drones

**Variation**: Try different LFO rates:
- 0.05 Hz: Glacial evolution (20s cycle)
- 0.25 Hz: Faster motion (4s cycle)
- Use SAW wave for one-directional scanning

---

## Part 4: Evolving Density Parameter

### Setup

1. **Add second LFO** (or use multiple outputs from same LFO)
2. **Connect**: LFO SINE OUT → Tapestry MORPH CV
3. **Set LFO**:
   - Frequency: 0.05 Hz (20-second cycle)
   - Waveform: Sine
   - Unipolar (0-5V)

### Results

- Grain density slowly rises and falls
- Texture gets thicker (more voices) and thinner (fewer voices)
- Combined with Scan modulation = complex evolution

---

## Part 5: Advanced Ambient Techniques

### Technique 1: Frozen Slice

**Goal**: Create a sustained pad from any transient sound

1. **Record a short sound** (cymbal, vocal stab, etc.)
2. **Set parameters**:
   - Grain Size: 35%
   - Density: 80%
   - Speed: 0% (important: no advancement)
   - Scan: 30%
3. **Result**: Sound "freezes" in time, creates sustained pad

**Why it works**: With zero playback speed, grains continuously read the same region, overlapping to create sustained output.

### Technique 2: Wavetable Scanning

**Goal**: Treat your recording like a wavetable

1. **Record harmonic content** (chord, overtone series, etc.)
2. **Create one long splice** (covers entire recording)
3. **Set parameters**:
   - Grain Size: 15%
   - Density: 50%
   - Scan: 0%
4. **Connect LFO SAW** to Scan CV (1 Hz rate)
5. **Result**: Rapid scanning creates timbral motion

### Technique 3: Pitch Shimmer

**Goal**: Subtle pitch variation for richness

1. **Set Density to 100%**
2. Tapestry adds ±3 semitones pitch randomization
3. **Set Grain Size to 40%**
4. **Result**: Chorus-like detuning effect

---

## Complete Ambient Patch

### Full Setup

```
[Synth] ---> [Tapestry] ---> [Reverb] ---> [Audio Out]
              ↑        ↑
[LLFO 1] ----+        |
  (Scan, 0.1 Hz)     |
                      |
[LLFO 2] ------------+
  (Density, 0.05 Hz)
```

### Parameters

| Parameter | Value | Modulation |
|-----------|-------|------------|
| Grain Size | 30% | None |
| Density | 40% (base) | ±30% via LFO |
| Speed | 0% | None |
| Scan | 0% (base) | 0-100% via LFO |
| Mix | 50% | None |

### Reverb Settings (if used)

- Decay: 3-5 seconds
- Mix: 40-60%
- Pre-delay: 20-50ms

---

## Tips and Variations

### Variation 1: Stereo Width Enhancement

1. Record mono source
2. Set Density to 80%+
3. Tapestry's stereo spread creates wide image
4. Pan randomization activated at high Density

### Variation 2: Multi-Layer Textures

**Patch**:
```
[Source] → Split:
    → Tapestry A (small grains, fast Scan LFO)
    → Tapestry B (large grains, slow Scan LFO)
    → Mix both outputs
```

**Result**: Complex, multi-scale granular texture

### Variation 3: Generative Ambient Loop

1. **Enable Overdub** at 30% mix
2. **Record initial texture**
3. **Let it loop while overdubbing**
4. **Slowly modulate parameters**
5. **Result**: Self-evolving, never-repeating soundscape

---

## Creative Applications

### Use Case 1: Film Scoring

- Record dialog or foley
- Granulate with large grains (50%+)
- Add reverb
- Result: Atmospheric underscore

### Use Case 2: Live Ambient Performance

- Record live instruments in real-time
- Modulate Scan and Density with expression pedals
- Create evolving textures on the fly

### Use Case 3: Meditation/Yoga Music

- Record nature sounds (ocean, rain, wind)
- Very slow modulation (LLFO at 0.02 Hz)
- High Density (80%) for smooth texture
- Result: Calming, non-intrusive background

---

## Sound Design Tips

### Choosing Source Material

**Best sources for ambient**:
- Sustained notes (pads, drones)
- Field recordings (nature, urban)
- Voice (vowels, harmonics)
- Noise with filtering

**Avoid**:
- Percussive transients (unless frozen)
- Clicky, rhythmic material
- Very short sounds (unless using large Grain Size)

### Grain Size Selection

| Grain Size | Character |
|-----------|-----------|
| 5-10% | Grainy, textural, abstract |
| 20-30% | Balanced, recognizable yet processed |
| 40-60% | Smooth, pad-like, natural |
| 70%+ | Almost unchanged, subtle granulation |

### Density Parameter

| Density | Character |
|-------|-----------|
| 0-30% | Sparse, rhythmic grains |
| 40-60% | Medium density, clear voices |
| 70-90% | Thick, cloud-like texture |
| 90-100% | Dense, shimmering, detuned |

---

## Troubleshooting

### Issue: Texture Sounds Too Static

**Fix**:
- Add Scan modulation
- Use slower Grain Size modulation
- Try different Density values

### Issue: Too Glitchy/Grainy

**Fix**:
- Increase Grain Size (30%+)
- Increase Density (60%+)
- Ensure source material is long enough

### Issue: Not Enough Movement

**Fix**:
- Increase LFO modulation depth
- Use faster LFO rates
- Combine multiple modulation sources

---

## Recommended Further Exploration

1. **Example 6**: [Sound Design](06_sound_design.md) - More experimental techniques
2. **Advanced Usage**: [CV Modulation Recipes](../ADVANCED_USAGE.md#cv-modulation-recipes)
3. **Expander**: Add filtering for additional timbral control

---

## Learning Objectives

After completing this example, you should be able to:
- ✅ Create ambient textures from any source material
- ✅ Use Scan modulation for scanning effects
- ✅ Control density with Density parameter
- ✅ Combine multiple modulation sources
- ✅ Design evolving soundscapes

---

**Enjoy creating lush ambient textures!** 🌊

[← Previous Example](01_basic_recording.md) | [Back to Examples](README.md) | [Next Example →](03_rhythmic_processing.md)
