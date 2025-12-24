# Tapestry Quick Start

**Tapestry** is a granular sampler module for VCV Rack inspired by the Make Noise Morphagene. Record audio, create splices, and explore granular textures with 4-voice overlapping synthesis.

## Key Features

- **Sound-on-Sound Recording**: Tape-style recording with overdubbing
- **Splice Navigation**: Create and jump between markers
- **Granular Synthesis**: Up to 4 overlapping voices with windowing
- **Varispeed Playback**: Forward, reverse, and stopped modes
- **Slide Control**: Scan through audio within splices
- **Waveform Display**: Real-time visualization with splice markers
- **Organize**: Manual splice selection via knob

## Quick Start

### 1. Record Your First Sound
1. Set **VARISPEED** to center (stopped - there's a detent)
2. Ensure **OVERDUB** toggle is OFF (down position - default)
3. Press **RECORD** button (it lights up)
4. Connect audio to **AUDIO L** and/or **AUDIO R** inputs
5. Play some audio - you'll see it appear in the waveform display
6. Press **RECORD** again to stop recording

### 2. Play It Back
1. Turn **VARISPEED** right of center to play forward
   - Further right = faster playback
   - Left of center = reverse playback
   - Center = stopped
2. You should hear your recorded audio playing
3. The red playhead cursor shows current position

### 3. Add Effects with TapestryExpander (Optional)
**TapestryExpander** is a separate 4HP module that adds bit crusher and Moog VCF effects:

1. Place **TapestryExpander** to the **right** of Tapestry (must be adjacent)
2. Connection LED turns on when properly connected
3. **BitCrusher Section**:
   - **Bits**: Reduce bit depth (1-16 bits) for digital distortion
   - **Rate**: Sample rate reduction for lo-fi effect
   - **Mix**: Blend dry/crushed signal (0-100%)
4. **Moog VCF Section**:
   - **Cutoff**: Low-pass filter frequency
   - **Reso**: Resonance amount for emphasis at cutoff
   - **Mix**: Blend dry/filtered signal (0-100%)
5. **Output Level**:
  - **Level**: Post-effects volume (0-200%) to compensate for perceived level loss
6. All parameters have CV inputs for modulation (except Output Level)
7. Effects are processed in series: Tapestry → BitCrusher → Filter → Output Level → Output

**Tip**: Start with mix knobs at 0% and gradually turn them up to hear the effects!

### 4. Create Splices
Splices are markers that divide your recording into segments:

**Method 1: Click on Waveform** (easiest)
1. **Left-click** anywhere on the waveform to create a splice at that position
2. A green vertical line appears as you hover, showing where the splice will be created
3. Blue vertical lines mark your created splices
4. **Right-click** on an existing marker to delete it (red highlight shows deletion)
5. **Left-click** on an existing marker to select/jump to that splice

**Method 2: Button/Gate** (while playing)
1. While audio is playing, press **SPLICE** button to mark the current position
2. Or send a gate to the **SPLICE** input for automated marking
3. Press **SHIFT** button to jump between splices

### 5. Explore Granular Sounds
1. Adjust **MORPH** knob clockwise:
   - 0.0-0.3 = 1 voice (clean playback)
   - 0.3-0.7 = 2 voices (gentle overlap)
   - 0.7-0.9 = 3 voices (denser texture)
   - 0.9-1.0 = 4 voices (maximum overlap)
2. Lower **GENE SIZE** to 0.2-0.5 seconds for grainier textures
3. Adjust **SLIDE** to scan through different parts of the splice
4. Result: evolving, cloud-like granular textures

## Core Controls

### Recording & Playback
- **RECORD** (button): Start/stop sound-on-sound recording
- **OVERDUB** (toggle): Recording mode selector
  - OFF (down) = Replace mode (clears buffer on new recording)
  - ON (up) = Overdub mode (preserves existing audio)
- **PLAY** (button + gate): Retrigger playback from splice start
- **VARISPEED** (knob + CV): Playback speed/direction
  - Center = stopped
  - Right = forward (faster)
  - Left = reverse (faster)

### Granular Parameters
- **MORPH** (knob): Number of overlapping voices (1-4)
  - Low = clean playback
  - High = dense granular texture
- **GENE SIZE** (knob): Length of each grain window (0.05-5 seconds)
  - Small = granular, textured
  - Large = smooth, continuous
- **SLIDE** (knob): Position offset within splice (0-100%)
  - Works even when stopped (for splice creation)

### Navigation
- **SHIFT** (button + gate): Jump to next splice immediately
- **ORGANIZE** (knob): Manual splice selection (smooth)
- **SPLICE** (button + gate): Create splice at current position
- **CLEAR SPLICES** (button): Remove all splice markers instantly
- **SPLICE COUNT TOGGLE** (button): Cycle through automatic evenly-spaced splices
  - Cycles: 4 → 8 → 16 → 4 (repeats)
  - LED brightness shows mode (dim=4, medium=8, bright=16)
  - Distributes splices evenly across entire buffer
  - Great for rhythmic divisions and organized navigation

## Common Workflows

### Basic Looping
1. Record a phrase
2. Press **PLAY** to restart from beginning
3. Turn **VARISPEED** to loop continuously
4. No splices needed for simple looping

### Overdubbing (Two Methods)

**Method 1: Replace Mode (default)**
1. Record initial material with **OVERDUB** toggle OFF
2. Press **PLAY** to restart
3. Turn **VARISPEED** to play back
4. Press **RECORD** again while playing
5. New audio overwrites at current position

**Method 2: Overdub Mode (layering)**
1. Record initial material
2. Toggle **OVERDUB** switch ON (up position)
3. Press **RECORD** to start new recording
4. Buffer is preserved - new audio merges with existing
5. Create multiple layers without clearing previous takes

### Splice-Based Navigation
1. Record longer audio (10-30 seconds)
2. Create 4-8 splices at interesting moments
3. Set **VARISPEED** to stopped (center)
4. Use **SHIFT** button to jump between splices
5. Each splice plays as isolated section
6. Press **CLEAR SPLICES** to remove all markers and start over

### Granular Clouds
1. Record sustained tones or pads
2. Set **MORPH** to 0.8-1.0 (4 voices)
3. Set **GENE SIZE** to 0.1-0.3 seconds
4. Slowly turn **VARISPEED** for gentle movement
5. Adjust **SLIDE** to explore the recording
6. Result: atmospheric, evolving textures

**With TapestryExpander**:
- Add subtle filter movement with slow LFO to **Cutoff CV**
- Use bit crushing sparingly (12-14 bits) for character
- High resonance creates singing, harmonic clouds

### Rhythmic Slicing
**Method 1: Automatic Even Spacing**
1. Record drums or rhythmic material (try 2-4 bars)
2. Press **SPLICE COUNT TOGGLE** button to set even divisions
   - 4 splices = quarter notes or main beats
   - 8 splices = eighth notes or sub-divisions
   - 16 splices = sixteenth notes or detailed slicing
3. Connect clock/sequencer to **SHIFT** gate
4. Each clock pulse jumps to next splice sequentially
5. Creates rearranged, quantized rhythms with geometric precision

**Method 2: Manual Splice Creation**
1. Record drums or rhythmic material
**With TapestryExpander**:
- Use bit crushing (6-10 bits) for harsh, digital drums
- Fast filter modulation for rhythmic emphasis
- Low-pass filter to remove high-end for darker sound

2. Use **SPLICE** trigger to manually mark each hit
3. Connect clock/sequencer to **SHIFT** gate
4. Each clock pulse jumps to next splice
5. Creates rearranged rhythms with custom timing

**Pro Tip**: Start with automatic splice count for quick setup, then add manual splices for accent points or variations.

## CV Modulation Tips

### Voltage-Controlled Speed
- Connect LFO to **VARISPEED CV** for oscillating playback
- Slow LFO = breathing effect
- Fast LFO = vibrato/tremolo

### TapestryExpander CV Modulation
- **Filter Cutoff + LFO**: Wobble bass, rhythmic filtering
- **Resonance + Envelope**: Emphasize transients, add character
- **Bit Depth + Random**: Unpredictable digital glitches
- **Rate + Sequencer**: Stepped lo-fi reduction
- **Mix controls + Envelope**: Fade effects in/out dynamically

### Triggered Playback
- Connect sequencer gates to **PLAY** input
- Each gate retriggers from splice start
- Great for rhythmic granular patterns

### Automated Splice Navigation
- Connect random CV to **SHIFT** input (through comparator)
- Creates unpredictable navigation
- Add probability control with gate processor

## Display Features

### Waveform View
- **Colored waveform**: Your recorded audio (customizable - see Context Menu below)
- **Red cursor**: Current playhead position
- **Blue lines**: Splice markers
- **Green hover line**: Shows where new splice will be created (when hovering)
- **Red hover line**: Shows marker that will be deleted (when hovering over existing splice)

### Waveform Interaction
- **Left-click**: Create new splice at click position
- **Left-click on marker**: Select/jump to that splice
- **Right-click on marker**: Delete that splice marker
- **Hover**: See green preview for new splice or red highlight for deletion
- **Visual feedback**: Triangle indicator at top shows exact click position

### Context Menu (right-click on module)
- **Load Reel...**: Import WAV file into tape buffer
- **Save Reel...**: Export current tape buffer as WAV
- **Clear Reel**: Erase all audio
- **Splice Count**: Cycle through auto-splice options (4, 8, or 16 markers)
- **Waveform Color**: Choose visual color preset
  - Red, Amber, Green, Baby Blue (default), Peach, Pink, White
  - Customize to match your patch aesthetic or improve visibility
  - Selection persists with saved patches
- **File Info**: View current file name, duration, and splice count

**With Expander**: Add subtle filter (cutoff=0.7, mix=30%) for darker ambience

**Tip**: Use the dedicated **CLEAR SPLICES** button for quick access!

## Quick Patch Ideas

### Ambient Drone Generator
```
**With Expander**: Bit crush at 4-8 bits, rate=50%, filter cutoff modulation

Audio Source → Tapestry L/R Inputs
Slow LFO → Tapestry Varispeed CV
Tapestry L/R → Reverb → Output
Settings: Morph=0.9, Gene=2.0s, Slide=0.5
```

### Glitch Percussion
```
Drum Loop → Tapestry (record)
Fast Clock → Shift Input
Random CV → Organize Knob
**With Expander**: Add resonant filter for tonal reverse echoes

Settings: Morph=0.2, Gene=0.1s, create 8 splices
```

### Reverse Echo
```
Audio → Tapestry (record short phrase)
**With Expander**: Use filter cutoff tracking varispeed for pitch-follow effect

Tapestry L/R → Delay → Output
Settings: Varispeed=-0.3 (slow reverse), Gene=1.0s
Trigger: Press Play button to restart
```

### Granular Sampler
```
Sample → Tapestry (record)
Sequencer Gates → Play Input
Sequencer CV → Varispeed CV (quantized)
Settings: Morph=0.6, Gene=0.2s, Slide=variable
```

## Common Questions

**Q: Why does the expander need to be on the right side?**  
A: Yes! TapestryExpander must be placed directly to the right of Tapestry. The connection LED will light up when properly connected.

**Q: Is there latency when using the expander?**  
A: Yes—VCV Rack expander messages are double-buffered and flipped by the engine, so processed audio returns with **1-sample latency**. This is typically inaudible and ensures the modules don’t read partially-written data.

**Q: Can I use just the filter or just the bit crusher?**  
A: Yes! Set the mix knob to 0% for any effect you don't want. Each effect can be used independently.

**Q: Why is there no sound when I press play?**  
A: Check that varispeed is not at center (stopped). Turn it right for forward playback.

**Q: How do I clear the recording?**  
A: Right-click module → "Clear Tape" or start a new recording session.

**Q: Can I overdub multiple layers?**  
A: Yes! Press RECORD again while playing back to add new layers.

**Q: What's the difference between Shift and Organize?**  
A: **Shift** jumps immediately to next splice (great for gates/triggers). **Organize** smoothly selects splices with the knob (great for manual control).

**Q: How many splices can I create?**  
A: Maximum 64 splices. Clear splices if you hit the limit.

**Q: Does Slide work when stopped?**  
A: Yes! This makes it easy to position the playhead for precise splice creation.

## Performance Tips

- Start simple: record → play → add splices gradually
- Use **MORPH** to transition from clean to granular
- **GENE SIZE** dramatically changes character - experiment!
- **SLIDE** is powerful for finding sweet spots in audio
- Combine **SHIFT** button with clock for rhythmic control
- Layer multiple Tapestry modules for complex textures
- Lower **MORPH** for better CPU performance

## Next Steps

Once comfortable with basics:
1. Read [full documentation](Tapestry.md) for advanced techniques
2. Experiment with feedback patching (output → input)
3. Try probability-based splice navigation
4. Combine with effects (reverb, delay, filters)
5. Use multiple Tapestries in parallel for stereo width

---

**Have fun weaving sonic tapestries!** 🎵
