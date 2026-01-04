# Tapestry Advanced Usage Guide

**Master advanced techniques and workflows**

This guide covers sophisticated techniques, performance optimization, creative workflows, and expert-level usage of Tapestry.

---

## Table of Contents

1. [Advanced Granular Techniques](#advanced-granular-techniques)
2. [Clock Synchronization Mastery](#clock-synchronization-mastery)
3. [Marker Management Strategies](#splice-management-strategies)
4. [CV Modulation Recipes](#cv-modulation-recipes)
5. [Expander Effects Chains](#expander-effects-chains)
6. [Performance Optimization](#performance-optimization)
7. [Creative Workflows](#creative-workflows)
8. [Integration Patterns](#integration-patterns)
9. [Troubleshooting Complex Issues](#troubleshooting-complex-issues)
10. [Best Practices](#best-practices)

---

## Advanced Granular Techniques

### Multi-Voice Grain Orchestration

Tapestry's grain engine supports up to 4 simultaneous voices. Understanding how they interact unlocks powerful textures.

#### Voice Overlap Mathematics

The Density parameter controls overlap using this formula:

```
overlap = 1 + (morph * 3)
morph = 0.0 → overlap = 1 voice
morph = 0.33 → overlap = 2 voices
morph = 0.66 → overlap = 3 voices
morph = 1.0 → overlap = 4 voices
```

#### Pitch and Pan Randomization

At high Density values (> 0.5), Tapestry introduces randomness:

- **Pitch Variation**: ±3 semitones maximum (at morph = 1.0)
- **Stereo Spread**: Voices distributed across stereo field
- **Algorithm**: Xorshift128+ fast random generator for real-time performance

**Use case**: Creating thick, detuned textures from monophonic sources.

**Technique – Synthetic Chorus**:
```
1. Record a single sine wave (sustained)
2. Set Grain Size to 50-100ms
3. Set Density to 100%
4. Set Scan to 10-30%
5. Result: 4-voice detuned chorus effect
```

### Grain Window Shaping

Tapestry uses Hann window function for smooth grain envelopes:

```cpp
amplitude = 0.5 * (1 - cos(2π * phase))
```

**Characteristics**:
- Smooth attack and decay
- No clicks or artifacts
- Optimal for overlapping grains

**Advanced tip**: For harder, more percussive grains, use shorter Grain Sizes (< 10ms) with low Density (< 30%).

---

### Freeze and Time-Stretch Techniques

#### The "Frozen" Effect

Create a sustained, shimmering pad from any transient sound:

1. **Record a short sound** (cymbal crash, vocal stab, drum hit)
2. **Set parameters**:
   - Grain Size: 30-50% (100-200ms)
   - Density: 80-100% (high overlap)
   - Speed: 0% (no playback advancement)
   - Scan: 20-40%
3. **Optional**: Modulate Scan slowly with LFO for evolution

**Why it works**: With zero Speed, grains continuously read the same region, creating a sustained "freeze" effect. High overlap creates smooth texture.

#### Granular Time Compression/Expansion

**Goal**: Change duration without changing pitch

**Technique – Manual Time Stretch**:
1. Record material at original tempo
2. Create splices at beat boundaries
3. Set Grain Size to 15-30% of splice length
4. Set Density to 60-80% (3+ voices for smoothness)
5. Connect clock at target tempo
6. Result: Tempo-shifted playback maintaining pitch

**Math**: 
```
Grain Size (samples) ≈ (Clock Period / Overlap Factor)
For smooth results: Overlap ≥ 2-3 voices
```

---

### Micro-Rhythmic Granulation

Create complex polyrhythms from simple material:

**Setup**:
```
[Clock/4] → Tapestry CLK (quarter notes)
[Clock/3] → Tapestry SPLICE (triplets)
[Clock] → Tapestry NEXT (16th notes)
```

1. Record rhythmic material (drums, percussion)
2. SPLICE input creates markers on triplet grid
3. NEXT input navigates at 16th note rate
4. CLK input triggers grains at quarter notes
5. Result: Polyrhythmic granular mayhem

---

## Clock Synchronization Mastery

### Understanding Time Stretch Modes

Tapestry has two clock-sync behaviors based on Density:

#### Grain Next Mode (Density < 50%)

**Behavior**:
- Clock triggers individual grains
- No playback rate adaptation
- Grains are independent events

**Best for**:
- Percussive material
- Stutter effects
- Rhythmic gating

**Example – Kick Drum Multiplier**:
```
1. Record a kick drum pattern
2. Create splice around one kick hit
3. Connect fast clock (1/16 notes)
4. Set Density to 20%
5. Set Grain Size to 5%
6. Result: Kick drum rolls and fills
```

#### Time Stretch Mode (Density > 50%)

**Behavior**:
- Playback speed adapts to clock period
- Multiple overlapping voices
- Smooth pitch preservation

**Best for**:
- Melodic content
- Pads and textures
- Tempo-synced loops

**Algorithm**:
```
playback_rate = splice_length / (clock_period * overlap)
```

**Example – Tempo-Synced Vocal Loop**:
```
1. Record 1-bar vocal phrase
2. Create splice around the phrase
3. Connect clock at 1/bar rate
4. Set Density to 70%
5. Change clock tempo → vocal tempo follows
```

---

### Multi-Clock Techniques

Use multiple clocks for advanced rhythm manipulation:

**Technique – Euclidean Granulator**:
```
[Clock] → Euclidean Rhythm → Tapestry CLK
         Pattern: [5,8] (5 hits in 8 steps)
```

1. Record melodic loop
2. Euclidean rhythm creates irregular grain triggers
3. Set Density to 40% (Grain Next mode)
4. Result: Glitchy, syncopated textures

**Technique – Clock Division Exploration**:
```
[Clock] → Clock Divider:
    /1 → Tapestry CLK (grain trigger)
    /4 → Tapestry NEXT (splice advance)
    /8 → Tapestry SPLICE (marker creation)
```

Result: Self-organizing polyrhythmic structures

---

### Sync Drift and Creative Timing

**Intentional Desync**:
1. Use two slightly different clock rates
2. Clock A → CLK input
3. Clock A (slightly detuned) → NEXT input
4. Result: Slowly drifting phase relationships

**Musical Application**: Creating evolving variations of loop-based material

---

## Marker Management Strategies

### Architectural Approaches

#### 1. Beat-Aligned Splices (Rhythmic Material)

**Purpose**: Clean loop points and beat-accurate navigation

**Method**:
1. Record loop synced to master clock
2. Create splices exactly at beat boundaries
3. Use Select to jump to specific beats
4. Use NEXT for sequential playback

**Pro tip**: Use VCV Recorder to record directly from another module with known timing.

#### 2. Feature-Based Splices (Non-Rhythmic Material)

**Purpose**: Mark interesting sonic events

**Method**:
1. Record field recordings, vocals, sound design
2. Create splices at:
   - Transients (attacks)
   - Timbral changes
   - Silence gaps
   - Harmonic shifts
3. Use Select to create narratives

**Example**: Marker a speech recording at word boundaries for granular "word salad" effects.

#### 3. Equal-Division Splices (Experimental)

**Purpose**: Predictable, algorithmic organization

**Method**:
1. Record any material
2. Create splices at regular intervals (e.g., every 1 second)
3. Use CV sequencer on Select for programmed jumps
4. Result: Structured randomness

---

### Marker Count Optimization

**Trade-offs**:

| Marker Count | Navigation | CPU | Use Case |
|--------------|------------|-----|----------|
| 2-4 | Coarse | Low | Simple A/B switching |
| 8-16 | Musical | Medium | Beat/bar navigation |
| 32-64 | Fine | Medium | Detailed arrangement |
| 100-300 | Precise | Higher | Micro-editing, analysis |

**Performance Note**: More markers = more CPU for Select parameter updates. Optimize for your needs.

---

### Dynamic Marker Automation

**Technique – Marker Sequence Programming**:

```
[CV Sequencer] → Tapestry ORGANIZE CV
    Step 1: 0V (splice 0)
    Step 2: 1.25V (splice ~3)
    Step 3: 2.5V (splice ~6)
    Step 4: 0V (splice 0)
    [Clock] → Sequencer CLK
```

Result: Programmed splice navigation in time with your patch tempo

**Scaling CV**:
- ORGANIZE expects 0-5V for full range
- If you have 8 splices: Each splice ≈ 0.625V apart
- Use offset/scale modules for precise control

---

## CV Modulation Recipes

### Grain Size Modulation

#### Recipe 1: Envelope-Controlled Grain Size

**Goal**: Grains get larger over time (creates evolving texture)

```
[ADSR] ENV OUT → [VCA/Attenuverter] → Tapestry GENE SIZE CV
Set ADSR: A=1s, D=0, S=100%, R=2s
Set CV Atten: +1.0 (positive)
Base Grain Size: 10%
```

**Result**: Grains start small (rhythmic), grow to large (smooth) as envelope rises.

#### Recipe 2: LFO Pulsing Grains

**Goal**: Rhythmic grain size pulsation

```
[LFO] TRI OUT → Tapestry GENE SIZE CV
LFO Rate: 1Hz
CV Atten: ±0.5
Base Grain Size: 40%
```

**Result**: Grains breathe in size at 1Hz rate.

---

### Density Sweeps

#### Recipe 3: Slow Density Evolution

**Goal**: Fade from sparse to dense textures

```
[Bogaudio LLFO] SIN OUT → [8VERT] → Tapestry MORPH CV
LLFO Period: 32 bars
8VERT: Unipolar (0-5V)
Base Density: 0%
```

**Result**: Over 32 bars, grain density slowly increases from 1 to 4 voices.

---

### Scan Position Modulation

#### Recipe 4: Wavetable-Style Scanning

**Goal**: Treat splice like a wavetable, scan with LFO

```
[LFO] SAW OUT → Tapestry SLIDE CV
LFO Rate: 2Hz
CV Atten: +1.0
Base Scan: 0%
```

**Setup**:
1. Record harmonic series or evolving texture
2. Create one long splice
3. LFO scans through splice content
4. Result: Wavetable-style timbral scanning

#### Recipe 5: Random Walk Exploration

**Goal**: Organic, non-repetitive position modulation

```
[S&H] OUT → [Slew Limiter] → Tapestry SLIDE CV
[Random] → S&H IN
[Clock/4] → S&H TRIG
Slew Time: 200ms
```

**Result**: Smoothly wandering grain positions, non-looping behavior.

---

### Speed Modulation

#### Recipe 6: Pitch Vibrato

**Goal**: Add vibrato to granular playback

```
[LFO] SIN OUT → [Attenuverter] → Tapestry VARI-SPEED CV
LFO Rate: 5-7Hz (typical vibrato)
CV Atten: ±0.1 (subtle)
Base Speed: 0% (center)
```

**Result**: Natural vibrato effect on grains (±1-2 semitones).

#### Recipe 7: Envelope-Based Pitch Dive

**Goal**: Pitch drops after trigger (like a tom or synth voice)

```
[AD Envelope] OUT → [Inverter] → Tapestry VARI-SPEED CV
Trigger: Gate input
A: 10ms, D: 500ms
CV Atten: +1.0
Base Speed: +50%
```

**Result**: Pitch starts high, dives down over 500ms.

---

### Multi-Parameter Macro Control

**Technique – Single Knob Performance Control**:

```
[Manual CV] OUT → [Multiple Destinations]:
    → Tapestry GENE SIZE CV (direct)
    → Tapestry MORPH CV (via attenuverter, inverted)
    → Tapestry FILTER CUTOFF CV (on expander)
```

**Result**: One knob controls grain size (up), morph density (down), and brightness simultaneously.

---

## Expander Effects Chains

### Serial Effect Routing

While Tapestry Expander has parallel effects (bit crusher + filter), you can route externally for serial chains:

**Technique – External Serial Routing**:
```
Tapestry OUT → Bit Crusher Module → Filter Module → Mixer → Tapestry IN
                                                              (Overdub Mode)
```

**Use case**: Build up layered, multi-effect textures through feedback.

---

### CV-Controlled Effect Sweeps

#### Sweep 1: Filter Cutoff Envelope

**Goal**: Filter opens with grain envelope

```
Tapestry ENV OUT → Expander FILTER CUTOFF CV
Base Cutoff: 20%
Filter Mix: 100%
```

**Result**: Filter opens and closes with each grain.

#### Sweep 2: Resonance Modulation

**Goal**: Sweeping resonance for vowel-like effects

```
[LFO] TRI OUT → Expander FILTER RESO CV
LFO Rate: 0.5Hz
Base Reso: 40%
Base Cutoff: 35%
Filter Mix: 80%
```

**Result**: Formant-like sweeping (vocal quality).

---

### Parallel Mix Techniques

**Technique – New York Compression Style**:
1. Set both Crush Mix and Filter Mix to 30-40%
2. Heavy crush/filter settings
3. Result: Parallel compression effect (thick, dense)

**Technique – Selective Processing**:
1. Crush Mix: 0% (bypass)
2. Filter Mix: 100% (full wet)
3. Use filter as primary tone shaper

---

### Feedback Loops with Expander

**Setup**:
```
Tapestry OUT → Expander → External Delay → Tapestry IN (Overdub)
                          ↓
                      Delay Mix: 30%
                      Delay Time: 1/4 note
```

**Safety**: Keep Overdub Mix at 50% or lower to prevent runaway feedback.

**Result**: Granular echoes building up in reel over time.

---

## Performance Optimization

### CPU Usage Profiling

**Baseline Metrics** (48kHz, tested on 2020 Apple M1):
- Idle (no grains): ~1.5%
- 1 grain voice: ~3.2%
- 4 grain voices: ~9.8%
- With expander: +2.3%

**Factors Affecting CPU**:
1. **Number of active voices** (Density)
2. **Marker count** (Select updates)
3. **Sample rate** (higher = more CPU)
4. **Buffer size** (larger = more memory cache misses)

---

### Optimization Strategies

#### Strategy 1: Reduce Voice Count

```
Density at 100% (4 voices) → Density at 50% (2-3 voices)
CPU reduction: ~40%
```

**Trade-off**: Less dense texture, but still smooth.

#### Strategy 2: Increase Grain Size

```
Grain Size at 5% (10ms) → Grain Size at 20% (50ms)
CPU reduction: ~30%
```

**Why**: Fewer grain retriggerings per second.

#### Strategy 3: Limit Marker Count

```
100 splices → 16 splices
CPU reduction: ~15%
```

**Why**: Less overhead in splice management.

#### Strategy 4: Use Lower Sample Rates

```
48kHz → 44.1kHz
CPU reduction: ~8%
```

**Minimal quality loss** for most applications.

---

### Real-Time Safe Operations

**Safe** (can be called in process()):
- All parameter setters
- All CV inputs
- Grain processing
- Marker navigation

**Unsafe** (background thread only):
- File I/O (saveCurrentReel, loadReel)
- Buffer resizing
- JSON serialization

**Implementation**: File operations use atomic flags (`fileLoading`, `fileSaving`) to prevent race conditions.

---

### Memory Management

**Buffer Allocation**:
```
32-bit float × 2 channels × 8,352,000 frames = 64MB per reel
32 reels × 64MB = 2GB maximum memory usage
```

**Optimization**:
- Use fewer reel slots if memory-constrained
- Clear unused reels (hold REC to clear)
- Limit buffer length to what you actually need

---

## Creative Workflows

### Workflow 1: Generative Ambient

**Goal**: Self-evolving ambient soundscape

**Patch**:
```
[Noise] → Tapestry IN
[LLFO 1] → Tapestry SPLICE (creates splices over time)
[LLFO 2] → Tapestry SLIDE CV (slow scanning)
[LLFO 3] → Expander FILTER CUTOFF CV (timbral evolution)
[Clock] → Tapestry NEXT (periodic splice jumps)
```

**Parameters**:
- Grain Size: 40-60%
- Density: 70-80%
- Speed: Center
- Recording: Continuous (overdub mode at 30% mix)

**Result**: Ever-changing ambient clouds, never repeating.

---

### Workflow 2: Live Performance Looper

**Goal**: Real-time live instrument processing

**Patch**:
```
[Mic/Instrument] → Tapestry IN
[Foot Pedal/MIDI] → Tapestry REC (toggle)
[Foot Pedal/MIDI] → Tapestry SPLICE (mark)
[Expression Pedal] → Tapestry MORPH CV (density)
```

**Technique**:
1. Record phrases live
2. Create splices at phrase boundaries
3. Use NEXT to recall phrases
4. Modulate Density for texture
5. Layer with overdub

**Performance Tips**:
- Keep Mix at 40-60% for balanced overdub
- Use Expander filter for live tone shaping
- Pre-create splice count mode (4/8/16) for predictable navigation

---

### Workflow 3: Rhythmic Sound Design

**Goal**: Create complex drum variations from simple sources

**Patch**:
```
[Kick] → Tapestry IN (record 1-2 bars)
[Hi-Hat] → Tapestry IN (overdub)
[Snare] → Tapestry IN (overdub)
[Clock/16] → Tapestry CLK (16th note grains)
[Euclidean] → Tapestry NEXT (pattern-based splice jumps)
```

**Parameters**:
- Grain Size: 5-10%
- Density: 20-30% (Grain Next mode)
- Create 8-16 splices (one per drum hit)

**Result**: Algorithmic drum variations, glitch percussion

---

### Workflow 4: Experimental Musique Concrète

**Goal**: Classic tape music techniques in modular

**Technique – The Pierre Schaeffer Method**:
1. **Collect materials**: Record diverse sound sources (nature, voices, instruments)
2. **Create marker taxonomy**: Select sounds by timbre, rhythm, pitch
3. **Granular manipulation**: Use grains to reveal inner structures
4. **Montage**: Use Select + NEXT to sequence sounds
5. **Transform**: Apply speed changes, filters, distortion

**Example Process**:
```
Day 1: Record 30+ sound sources into different reel slots
Day 2: Create splices marking interesting segments
Day 3: Create sequences using CV control of Select
Day 4: Apply granular transformations (grain size, morph)
Day 5: Record final composition using overdub layering
```

---

### Workflow 5: Sample-Based Synthesis

**Goal**: Use Tapestry as a advanced sampler/synthesizer voice

**Patch**:
```
[Sampled Waveforms] → Reel (pre-loaded)
[MIDI-CV] PITCH → Tapestry VARI-SPEED CV (pitch tracking)
[MIDI-CV] GATE → Tapestry PLAY (note on/off)
[MIDI-CV] VEL → Tapestry GENE SIZE CV (velocity sensitivity)
```

**Setup**:
1. Pre-load multi-samples into different reels
2. Create splices around each note
3. Use Select to select samples
4. Track keyboard with Speed for pitch
5. Result: Polyphonic (with multiple Tapestry instances)

---

## Integration Patterns

### Pattern 1: Master/Slave Recording

**Scenario**: Record multiple Tapestry instances simultaneously

```
[Audio Source] → Split:
    → Tapestry A IN
    → Tapestry B IN
    → Tapestry C IN
[Master Gate] → All Tapestry REC inputs
```

**Use case**: Create variations of same source with different grain settings.

---

### Pattern 2: Cross-Modulation

**Scenario**: One Tapestry modulates another

```
Tapestry A ENV OUT → Tapestry B GENE SIZE CV
Tapestry A EOG OUT → Tapestry B SPLICE (trigger)
Tapestry B OUT → Tapestry A IN (feedback)
```

**Result**: Coupled granular systems with emergent behavior.

---

### Pattern 3: Sidechain Analysis

**Scenario**: Use Tapestry as effect processor with sidechain control

```
[Drums] → Tapestry IN (record/process)
[Synth] → Envelope Follower → Tapestry MORPH CV
```

**Result**: Drum granulation density follows synth dynamics.

---

### Pattern 4: Quad Panning with Multiple Outputs

**Scenario**: Create spatial granular textures

```
Tapestry 1 (Left material) → Pan LEFT
Tapestry 2 (Right material) → Pan RIGHT
Tapestry 3 (Center material) → Pan CENTER
[LFO] → All SLIDE CV (synchronized scanning)
```

**Result**: Synchronized multi-channel granular spatialization.

---

## Troubleshooting Complex Issues

### Issue: Grains Sound Uneven/Clicking

**Symptoms**: Artifacts, clicks, or uneven amplitude

**Causes**:
1. Grain Size too small (< 5ms)
2. Buffer too short (not enough material)
3. Marker boundaries misaligned

**Solutions**:
- Increase Grain Size to 20ms+
- Ensure at least 1 second of recorded material
- Create splices away from zero-crossings
- Check for DC offset in source material

---

### Issue: Time Stretch Not Working

**Symptoms**: No tempo adaptation when clock connected

**Diagnosis**:
1. Check Density value: Must be > 50% for time stretch mode
2. Verify clock is actually triggering (check with scope)
3. Ensure splice length is reasonable (> 1000 samples)
4. Check Grain Size isn't too large (> 80%)

**Solution**:
```
Set Density to 70%
Set Grain Size to 30%
Verify CLK input has 0-10V gate signals
```

---

### Issue: Expander Not Passing Audio

**Symptoms**: CONNECTED light on, but no effect

**Diagnosis**:
1. Check Mix parameters (must be > 0%)
2. Verify Tapestry is actually generating audio
3. Check Output Level on expander
4. Ensure expander is on RIGHT side of Tapestry

**Solution**:
- Set Crush Mix or Filter Mix to 50%
- Check Tapestry output with scope before expander
- Verify expander placement (must be immediate right neighbor)

---

### Issue: High CPU Usage

**Symptoms**: Audio dropouts, glitches, system slowdown

**Diagnosis**:
1. Check number of Tapestry instances (each uses CPU)
2. Check Density setting (high = more voices)
3. Check grain size (small = more retriggerings)
4. Check overall patch complexity

**Optimization**:
```
Reduce Density to 50% (2-3 voices instead of 4)
Increase Grain Size to 30%+
Reduce splice count to < 32
Lower VCV Rack sample rate (Engine → Sample Rate)
```

---

### Issue: Splices Not Saving/Loading

**Symptoms**: Splices disappear after closing patch

**Diagnosis**:
1. Check if patch was saved (File → Save)
2. Verify JSON contains `spliceMarkers` field
3. Ensure buffer has content when splices created

**Solution**:
- Always save patch after creating splices
- Verify splices are visible before saving
- Check JSON manually if needed:
  ```json
  "spliceMarkers": [0.0, 0.25, 0.5, 0.75]
  ```

---

## Best Practices

### Recording Best Practices

1. **Use High-Quality Sources**: 48kHz recordings match internal rate
2. **Monitor Levels**: Keep peaks around -6dB to avoid clipping
3. **Record Longer Than Needed**: Easier to splice shorter than expand
4. **Save Incrementally**: Save reel after good takes

### Marker Organization Best Practices

1. **Name Your Reels**: Use descriptive filenames (e.g., `vocal_phrase_A.wav`)
2. **Document Marker Positions**: Keep notes on what each marker contains
3. **Use Consistent Spacing**: Equal divisions or beat-aligned for predictability
4. **Backup Regularly**: Export reels as WAV files frequently

### Performance Best Practices

1. **Pre-Load Content**: Load reels before performance, not during
2. **Test CPU Load**: Monitor CPU meter, optimize before performing
3. **Use MIDI Controllers**: Map foot pedals to REC, SPLICE, NEXT
4. **Create Presets**: Save patches with different grain recipes

### Patch Design Best Practices

1. **Label Everything**: Use VCV Rack's cable colors and notes
2. **Group Related Controls**: Keep modulation sources near destinations
3. **Document CV Ranges**: Note expected voltages for each input
4. **Save Variations**: Create multiple patches for different applications

---

## Advanced DSP Considerations

### Interpolation Quality

Tapestry uses linear interpolation for grain playback. For the highest quality:

- **Keep Speed near unity** (0% = 1.0x) when possible
- **Use longer grains** (> 50ms) for pitched material
- **Higher pitch shifting** (> +12 ST) may introduce aliasing

### Resampling Behavior

Internal processing runs at 48kHz:

- **Input resampling**: Linear interpolation if module rate ≠ 48kHz
- **Output resampling**: Linear interpolation back to module rate
- **Quality**: Sufficient for most creative applications

For critical applications, run VCV Rack at 48kHz (Engine → Sample Rate → 48000 Hz).

### Phase Coherence

Grain voices are phase-independent:

- **Benefit**: Rich, complex textures from simple sources
- **Trade-off**: No phase coherence guarantee
- **Musical impact**: Minimal for most applications, natural for granular synthesis

---

## Conclusion

You now have expert-level knowledge of Tapestry's capabilities. Experiment with these techniques, combine them, and create your own workflows.

### Further Resources

- [API Reference](API_REFERENCE.md) – Technical deep dive
- [Quick Start Guide](QUICKSTART.md) – Basics review
- [Examples](examples/) – Real-world patches
- [FAQ](FAQ.md) – Common questions

---

**Happy patching, and enjoy exploring the sonic possibilities of Tapestry! 🎵**

*Questions? Contact: contact@shortwavlabs.com*
