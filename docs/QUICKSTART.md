# Tapestry Quick Start Guide

**Get up and running with Tapestry in minutes!**

This guide will walk you through your first recording, splicing, and granular processing session with Tapestry.

---

## Table of Contents

1. [Installation](#installation)
2. [First Patch](#first-patch)
3. [Basic Recording](#basic-recording)
4. [Creating Markers](#creating-splices)
5. [Granular Processing](#granular-processing)
6. [Adding Effects](#adding-effects-with-expander)
7. [Saving and Loading](#saving-and-loading)
8. [Quick Reference](#quick-reference)
9. [Next Steps](#next-steps)

---

## Installation

### Via VCV Library (Recommended)

1. Open VCV Rack
2. Click **View → Library** (or press `F1`)
3. Search for **"Tapestry"**
4. Click **Install** next to Shortwav Labs Tapestry
5. Restart VCV Rack

### Manual Build

```bash
# Clone and build
git clone https://github.com/shortwavlabs/tapestry.git
cd tapestry
make install
```

---

## First Patch

Let's create a simple patch to test Tapestry.

### Step 1: Add Modules

1. **Right-click** in your patch → **Add Module**
2. Search for **"Tapestry"** and add it
3. Add a **VCV Audio** interface module
4. Add a simple oscillator (e.g., **VCV VCO-1**)

### Step 2: Make Connections

```
VCO-1 SAW OUT → Tapestry AUDIO IN L
Tapestry AUDIO OUT L → Audio AUDIO OUT 1
Tapestry AUDIO OUT R → Audio AUDIO OUT 2
```

Your first patch should look like this:

```
[VCO-1] --SAW--> [Tapestry] --L/R--> [Audio Interface]
```

---

## Basic Recording

Now let's record some audio into Tapestry's reel.

### Recording Your First Reel

1. **Start the oscillator**: Set VCO-1 to ~220Hz (A3)
2. **Set Mix control** to middle position (50%)
3. **Click REC button** (top row, leftmost button)
   - Button lights up RED when recording
4. **Let it record** for 5-10 seconds
5. **Click REC again** to stop recording

🎉 **Congratulations!** You've just recorded your first reel.

### What Just Happened?

- Tapestry recorded stereo audio to its internal buffer
- The waveform appears in the display
- The REC light turned off, indicating recording stopped
- The audio is now ready for playback and manipulation

### Recording Modes

#### Replace Mode (Default)
- **Overdub Toggle**: OFF (down position)
- New recording replaces existing audio
- Clean slate for each take

#### Overdub Mode
- **Overdub Toggle**: ON (up position)
- New audio mixes with existing audio
- Use **Mix knob** to control blend:
  - 0% = 100% old audio (new audio silent)
  - 50% = Equal mix
  - 100% = 100% new audio (old audio silent)

### Pro Tips

- **Clear the reel**: Hold REC button for 1 second
- **Monitor while recording**: Audio passes through even while recording
- **Use longer recordings**: Up to 2.9 minutes per reel!

---

## Creating Markers

Markers are indicators that divide your reel into segments. They're essential for navigation and organizing your audio.

### Creating Your First Marker

1. **Make sure you have audio** recorded
2. **Click PLAY** (or connect a gate to PLAY input) to start playback
3. **While playing**, click **SPLICE** button when you want to mark a position
4. **Create more splices** by clicking SPLICE at different positions

### Marker Navigation

#### Using the NEXT Button

1. **Click NEXT** to advance to the next splice
2. Playback jumps to the start of the next segment
3. Wraps around to first splice after the last one

#### Using the Select Knob

1. **Turn the Select knob** (bottom row, rightmost)
2. Each position selects a specific splice
3. Display shows current splice number
4. Range: 0 to (number of splices - 1)

### Managing Markers

#### Delete Current Marker
1. **Hold SPLICE button** for 1 second
2. Current splice is removed
3. Playback continues to previous splice

#### Delete All Markers
1. **Hold NEXT button**
2. While holding NEXT, **click CLEAR button**
3. All splices are removed instantly

#### Marker Count Mode
1. **Hold NEXT button**
2. While holding NEXT, **click COUNT button**
3. Cycles through modes:
   - **4**: NEXT advances 4 splices at a time
   - **8**: NEXT advances 8 splices at a time
   - **16**: NEXT advances 16 splices at a time
   - Back to **1** (default)

### Example Workflow

**Creating a 4-bar drum loop:**

1. Record 4 bars of drums
2. Click SPLICE at the start of each bar (4 splices total)
3. Use NEXT to jump between bars
4. Use Select to select specific bars for arrangement

---

## Granular Processing

Now for the fun part – transforming your audio with granular synthesis!

### Understanding the Controls

#### Grain Size (Grain Size)
- **What it does**: Controls the length of each grain
- **Range**: 1ms to full reel length
- **Sweet spot**: 10-100ms for classic granular textures

**Try it:**
1. Set Grain Size to ~25% (50-100ms grains)
2. Play your reel
3. Slowly increase – notice longer grains sound more like the original
4. Decrease – shorter grains create denser, more abstract textures

#### Density (Grain Density/Overlap)
- **What it does**: Controls how many grain voices play simultaneously
- **Range**: 1 to 4 overlapping voices
- **Effect**: More overlap = denser, richer texture

**Try it:**
1. Set Density to 0% (1 voice)
2. Gradually increase Density
3. At ~50%, grains start overlapping (2-3 voices)
4. At 100%, maximum density (4 voices) with pitch/pan randomization

#### Scan (Position)
- **What it does**: Offsets where grains read from within the current splice
- **Range**: 0-100% of current splice length
- **Use**: Scan through your audio like a playhead offset

**Try it:**
1. Make sure you have at least one splice
2. Set Scan to 0% – grains read from current position
3. Increase Scan to 50% – grains read from middle of segment
4. Automate Scan with CV for sweeping effects

#### Speed (Playback Speed)
- **What it does**: Controls playback rate and direction
- **Range**: -26 semitones (reverse) to +12 semitones (forward)
- **Center position**: Normal forward playback (0 semitones)

**Try it:**
1. Set Speed to center (0%)
2. Turn left for slower/reverse playback
3. Turn right for faster playback (up to 1 octave up)

### Your First Granular Patch

**Goal**: Create an evolving granular texture

1. **Record a 20-second melodic phrase** (synth, vocal, anything!)
2. **Set these parameters**:
   - Grain Size: 20% (~50ms)
   - Density: 60% (2-3 voices)
   - Scan: Start at 0%
   - Speed: Center (0%)

3. **Slowly increase Scan** from 0% to 100%
   - Listen as the grains scan through your audio
   - Creates a "frozen" time-stretching effect

4. **Try different Grain Sizes**:
   - 5%: Very grainy, textural
   - 30%: Recognizable but processed
   - 80%: Almost normal playback with slight grain artifacts

5. **Experiment with Density**:
   - Low: Sparse, rhythmic grains
   - High: Dense, shimmering clouds

---

## Time Stretch Mode

Tapestry can sync its granulation to an external clock for rhythmic effects.

### Setting Up Clock Sync

1. **Add a clock module** (e.g., Clocked, Impromptu Clocked)
2. **Connect CLK OUT** to Tapestry's **CLK input**
3. **Set Density** to determine the sync mode:
   - **< 50%**: Grain Shift mode (grains triggered on clock)
   - **> 50%**: Time Stretch mode (playback synced to clock)

### Grain Next Mode (Density < 50%)

**What it does**: Triggers new grains on each clock pulse

```
[Clock] --CLK--> [Tapestry]
         Set Density to 30%
```

- Each clock pulse triggers a grain
- Grain size determines length
- Creates rhythmic, stuttering effects
- Perfect for drum loops and percussive material

### Time Stretch Mode (Density > 50%)

**What it does**: Stretches playback to match clock tempo

```
[Clock] --CLK--> [Tapestry]
         Set Density to 70%
```

- Playback speed adapts to clock period
- Maintains pitch while changing tempo
- Multiple overlapping grains for smooth stretching
- Excellent for melodic content and pads

### Example: Clock-Synced Drum Mangler

1. Record a drum loop (4-8 bars)
2. Create splices at each bar
3. Connect clock at 1/4 note rate
4. Set Density to 40% (Grain Shift mode)
5. Set Grain Size to 10-20%
6. Use NEXT to jump between bars rhythmically

Result: **Clock-synced stutter effects on your drums!**

---

## Adding Effects with Expander

The Tapestry Expander adds two powerful effects to your signal chain.

### Adding the Expander

1. **Right-click** and add **"Tapestry Expander"**
2. **Place it directly to the right** of Tapestry
3. The **CONNECTED light** turns on when properly connected
4. Audio automatically routes through the expander

### Bit Crusher Effect

**Purpose**: Lo-fi degradation, sample rate reduction

#### Controls
- **BITS**: Bit depth (1-16 bits)
  - 16: Clean (no effect)
  - 8: Classic lo-fi sound
  - 4: Heavy distortion
  - 1: Extreme noise
  
- **RATE**: Sample rate reduction (0-100%)
  - 100%: No reduction
  - 50%: Half sample rate (aliasing/artifacts)
  - 0%: Maximum reduction (robotic/glitchy)
  
- **MIX**: Dry/wet blend
  - 0%: Bypass
  - 50%: Equal mix
  - 100%: Fully crushed signal

**Try it**:
1. Set BITS to 8
2. Set RATE to 50%
3. Set MIX to 50%
4. Result: Warm lo-fi character

### Moog VCF Filter

**Purpose**: 24dB/octave resonant lowpass filter (analog modeling)

#### Controls
- **CUTOFF**: Cutoff frequency (20Hz-20kHz, logarithmic)
  - Left: Low frequencies only
  - Center: ~1kHz cutoff
  - Right: Full bandwidth
  
- **RESO**: Resonance/Q (0-100%)
  - 0%: No emphasis
  - 50%: Moderate resonance
  - 90%+: Self-oscillation territory
  
- **MIX**: Dry/wet blend
  - 0%: Bypass
  - 50%: Equal mix
  - 100%: Fully filtered signal

**Try it**:
1. Set CUTOFF to 30%
2. Set RESO to 70%
3. Set MIX to 100%
4. Slowly sweep CUTOFF up and down
5. Result: Classic analog filter sweep

### Output Level

**OUTPUT LEVEL**: Final gain control (0-200%)
- 100%: Unity gain
- 50%: -6dB attenuation
- 200%: +6dB boost

### Combined Effects Example

**Goal**: Gritty, filtered granular texture

1. **On Tapestry**:
   - Grain Size: 15%
   - Density: 50%
   - Record some melodic content

2. **On Expander**:
   - Crush BITS: 6
   - Crush RATE: 60%
   - Crush MIX: 40%
   - Filter CUTOFF: 40%
   - Filter RESO: 60%
   - Filter MIX: 70%
   - Output Level: 120%

3. **Result**: Lo-fi, filtered granular clouds

### CV Modulation

All expander parameters have CV inputs (0-10V):

**Example: Filter sweep automation**
```
[LFO] --OUT--> [Expander CUTOFF CV]
      Set LFO to 0.1Hz triangle wave
      Set Filter MIX to 100%
```

Result: Slow, sweeping filter modulation

---

## Saving and Loading

Tapestry can save and load audio reels as WAV files.

### Saving Your Reel

1. **Right-click Tapestry** module
2. Select **"Save Current Reel"**
3. Choose a location and filename
4. Click **Save**

**File format**: 32-bit float stereo WAV at 48kHz

### Loading a Reel

1. **Right-click Tapestry** module
2. Select **"Load Reel"**
3. Choose a WAV file
4. Click **Open**

**Supported formats**:
- Mono or stereo
- Any sample rate (resampled automatically)
- 16-bit, 24-bit, 32-bit int, 32-bit float

### Reel Slots

Tapestry supports up to **32 reel slots**:

1. **Right-click** module
2. Hover over **"Select Reel"**
3. Choose a slot (0-31)
4. Each slot stores up to 2.9 minutes

**Use cases**:
- Store multiple recordings per patch
- Create a palette of audio materials
- Switch between reels during performance

### Saving Splices

**Good news**: Splices are saved with your VCV Rack patch!

When you save your patch:
- Marker positions are stored
- Current splice index is preserved
- Marker count mode is saved

When you load a patch:
- Splices restore automatically
- File path is remembered (if you saved the reel)
- Playback position returns to saved state

---

## Quick Reference

### Button Quick Actions

| Button | Click | Hold (1s) | Next + Click |
|--------|-------|-----------|---------------|
| **REC** | Toggle recording | Clear reel | - |
| **SPLICE** | Create splice | Delete current splice | - |
| **NEXT** | Next splice | - | (hold for combos) |
| **CLEAR** | - | - | Clear all splices |
| **COUNT** | - | - | Cycle count mode |

### Parameter Ranges

| Parameter | Range | CV Input | Description |
|-----------|-------|----------|-------------|
| Mix | 0-100% | 0-8V | Recording blend |
| Grain Size | 1ms-174s | ±8V | Grain length |
| Speed | -26 to +12 ST | ±4V | Playback rate |
| Density | 1-4 voices | 0-5V | Grain overlap |
| Scan | 0-100% | 0-8V | Position offset |
| Select | 0-N | 0-5V | Marker selection |

### Signal Flow

```
Input → Recording → Buffer → Granular Engine → Expander → Output
         ↑              ↑         ↑                  ↑        ↑
         Mix         Splices   Grain Size      Bit Crusher  CV Outs
                              Density/Scan        Moog VCF
                              Speed
```

### CV Voltage Standards

| Type | Range | Example |
|------|-------|---------|
| Audio | ±5V | Oscillators, audio signals |
| CV (unipolar) | 0-8V | Envelopes, LFOs (unipolar) |
| CV (bipolar) | ±5V | LFOs (bipolar), mod sources |
| Gate/Trigger | 0-10V | Sequencers, clocks |

---

## Next Steps

### Explore More

Now that you've mastered the basics, check out:

1. **[Advanced Usage](ADVANCED_USAGE.md)** – Complex techniques and workflows
2. **[API Reference](API_REFERENCE.md)** – Technical deep dive
3. **[Examples](examples/)** – Real-world patch examples
4. **[FAQ](FAQ.md)** – Common questions and solutions

### Patch Ideas to Try

1. **Granular Reverb**: Record room ambience, use high Density with small Grain Size
2. **Vocal Freezer**: Record voice, set large Grain Size, freeze with zero Speed
3. **Rhythmic Glitcher**: Clock-sync with short grains, jump between splices
4. **Texture Generator**: Record noise/texture, scan with Scan CV
5. **Lo-Fi Sampler**: Record drums, use bit crusher at 8 bits, filter sweep

### Join the Community

- **Report bugs**: [GitHub Issues](https://github.com/shortwavlabs/tapestry/issues)
- **Share patches**: [Patchstorage](https://patchstorage.com/platform/vcv-rack/)
- **Get support**: contact@shortwavlabs.com

---

## Troubleshooting

### No Sound Output

1. ✅ Check audio inputs are connected
2. ✅ Recording button should be OFF (not red)
3. ✅ PLAY button or gate input should be active
4. ✅ Check Mix knob is not at 0%
5. ✅ Verify VCV Audio interface is configured

### Grains Sound Clicking/Glitchy

1. ✅ Increase Grain Size (longer grains = smoother)
2. ✅ Reduce Density (fewer overlapping voices)
3. ✅ Check CPU usage (reduce voices if too high)
4. ✅ Ensure audio buffer has enough content

### Expander Not Working

1. ✅ Place expander **directly to the right** of Tapestry
2. ✅ Check CONNECTED light is illuminated
3. ✅ Verify mix controls are not at 0%
4. ✅ Restart VCV Rack if connection fails

### Splices Not Appearing

1. ✅ Make sure you've recorded audio first
2. ✅ Click SPLICE while playing or at current position
3. ✅ Check you haven't exceeded 300 splice limit
4. ✅ Verify buffer has sufficient length for markers

---

**Happy patching! 🎵**

*For more help, see the [FAQ](FAQ.md) or contact support.*
