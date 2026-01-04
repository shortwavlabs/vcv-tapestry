# Example 1: Basic Recording and Playback

**Goal**: Learn the fundamentals of recording audio, creating splices, and basic playback.

**Difficulty**: Beginner  
**Estimated Time**: 10 minutes

---

## Required Modules

- **Tapestry** (main module)
- **VCV Audio** (audio interface)
- **VCO-1** or any audio source (for testing)

---

## Patch Diagram

```
[VCO-1] --SAW OUT--> [Tapestry] --L/R OUT--> [Audio Interface]
                      AUDIO IN L
```

---

## Step-by-Step Instructions

### Part 1: Basic Recording

1. **Add modules** to your patch:
   - Tapestry
   - VCO-1 (or similar oscillator)
   - Audio interface

2. **Make connections**:
   ```
   VCO-1 SAW OUT → Tapestry AUDIO IN L
   Tapestry AUDIO OUT L → Audio 1
   Tapestry AUDIO OUT R → Audio 2
   ```

3. **Set VCO-1 parameters**:
   - Frequency: 220 Hz (A3)
   - Waveform: Sawtooth

4. **Set Tapestry parameters**:
   - Mix: 50% (center position)
   - Overdub Toggle: OFF (down)

5. **Start recording**:
   - Click **REC** button (turns red)
   - Let it record for 10 seconds
   - Click **REC** again to stop

6. **Verify recording**:
   - Waveform should appear in display
   - REC light should be off

### Part 2: Simple Playback

1. **Enable playback**:
   - Click **PLAY** button
   - Or connect a gate signal to PLAY input

2. **Listen**: You should hear the recorded sawtooth

3. **Try different playback speeds**:
   - Turn **Speed** knob left: slower/reverse
   - Turn **Speed** knob right: faster playback
   - Center position: normal speed

---

## Parameter Settings

| Parameter | Value | Notes |
|-----------|-------|-------|
| Mix | 50% | Balanced recording blend |
| Grain Size | 50% | Medium grain size |
| Speed | 0% (center) | Normal playback |
| Density | 0% | Single voice (no overlap) |
| Scan | 0% | No position offset |
| Overdub | OFF | Replace mode |

---

## What's Happening?

### Recording Process

1. Audio flows into Tapestry's input
2. REC button activates recording state
3. Audio is written to internal buffer (48kHz stereo)
4. Waveform display updates in real-time
5. Stopping REC freezes the buffer

### Playback Process

1. PLAY gate enables playback
2. Playback head reads from buffer
3. Speed controls playback rate
4. Audio outputs to L/R jacks

---

## Tips and Variations

### Variation 1: Overdub Recording

1. Record an initial sound (as above)
2. **Set Overdub Toggle to ON** (up position)
3. Click **REC** again
4. New audio mixes with existing audio
5. **Mix knob** controls the blend:
   - 0%: Only old audio audible
   - 50%: Equal blend
   - 100%: Only new audio audible

**Use case**: Build up layered textures

### Variation 2: Long Recording

1. Set up a drone or evolving sound
2. Start recording
3. Let it record for 60+ seconds
4. Create material for granular processing

**Tip**: Maximum recording time is 2.9 minutes (174 seconds)

### Variation 3: Stereo Recording

1. Use two audio sources
2. Connect left source to AUDIO IN L
3. Connect right source to AUDIO IN R
4. Record as usual
5. Stereo image is preserved

**Example**:
```
[VCO-1] → Tapestry L
[VCO-2] → Tapestry R
(Different frequencies for stereo effect)
```

---

## Common Issues

### No Sound During Playback

**Check**:
- ✅ Recording contains audio (waveform visible)
- ✅ PLAY is enabled (button on or gate high)
- ✅ Audio outputs are connected
- ✅ VCV Audio interface is configured

### Recording Not Working

**Check**:
- ✅ Audio inputs are connected and active
- ✅ Input signal is present (check with scope)
- ✅ REC button is lit (red)
- ✅ Buffer has available space

### Distorted Recording

**Cause**: Input level too high

**Fix**:
- Reduce source module volume
- Use attenuator before Tapestry input
- Keep peaks around -6dB

---

## Next Steps

Once comfortable with basic recording:

1. Try [Example 2: Creating Markers](02_creating_splices.md)
2. Experiment with [Granular Textures](02_granular_textures.md)
3. Explore [Quick Start Guide](../QUICKSTART.md) for more techniques

---

## Learning Objectives

After completing this example, you should be able to:
- ✅ Record audio into Tapestry
- ✅ Start and stop playback
- ✅ Understand overdub vs. replace modes
- ✅ Control playback speed with Speed
- ✅ Navigate the waveform display

---

**Congratulations! You've completed Example 1.** 🎵

[← Back to Examples](README.md) | [Next Example →](02_granular_textures.md)
