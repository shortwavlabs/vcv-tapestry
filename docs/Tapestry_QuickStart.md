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

### 3. Create Splices
Splices are markers that divide your recording into segments:

1. While audio is playing, press **SPLICE** button to mark the current position
2. Blue vertical lines appear in the display showing splice markers
3. Create 3-4 splices to divide your audio into sections
4. Press **SHIFT** button to jump between splices

### 4. Explore Granular Sounds
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

### Rhythmic Slicing
1. Record drums or rhythmic material
2. Use **SPLICE** trigger to mark each hit
3. Connect clock/sequencer to **SHIFT** gate
4. Each clock pulse jumps to next slice
5. Creates rearranged rhythms

## CV Modulation Tips

### Voltage-Controlled Speed
- Connect LFO to **VARISPEED CV** for oscillating playback
- Slow LFO = breathing effect
- Fast LFO = vibrato/tremolo

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
- **Gray waveform**: Your recorded audio
- **Red cursor**: Current playhead position
- **Blue lines**: Splice markers
- **Green highlight**: Current splice boundaries

### Zoom Controls
- **+** button: Zoom in for detail
- **−** button: Zoom out for overview
- Click and drag on waveform to jump to position (when stopped)

### Context Menu (right-click)
- **Clear Tape**: Erase all audio
- **Clear All Splices**: Remove all markers (same as Clear Splices button)
- **Remove Last Splice**: Undo last splice
- **Tape Info**: View buffer status

**Tip**: Use the dedicated **CLEAR SPLICES** button for quick access!

## Quick Patch Ideas

### Ambient Drone Generator
```
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
Settings: Morph=0.2, Gene=0.1s, create 8 splices
```

### Reverse Echo
```
Audio → Tapestry (record short phrase)
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

**Q: Why no sound when I press play?**  
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
