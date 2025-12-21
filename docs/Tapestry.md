# Tapestry Module - Technical Documentation

## Overview

**Tapestry** is a granular sampler module for VCV Rack inspired by the Make Noise Morphagene. It provides sound-on-sound recording, splice-based navigation, and advanced granular synthesis with 4-voice overlapping grains. Perfect for creating evolving textures, rhythmic variations, and experimental soundscapes.

**TapestryExpander** is an optional companion module that adds BitCrusher and Moog VCF effects to the Tapestry signal path.

---

## Architecture

### DSP Engine
The module integrates three core DSP components:
- **TapestryGrainEngine**: 4-voice granular processor with Hann windowing and pitch/pan modulation
- **TapeBufferManager**: Circular tape buffer with lock-free audio recording
- **SpliceManager**: Dynamic splice marker management with organize functionality
- High-quality cubic interpolation for smooth varispeed playback
- Real-time waveform display with playhead cursor and splice visualization

### Module Features
- 20HP panel with intuitive control layout
- 10 parameters: record, play, varispeed, morph, organize, gene size, slide, shift, splice trigger
- 7 inputs: audio L/R, clock, play/splice/shift gates, varispeed CV
- 2 outputs: stereo audio (L/R)
- Custom waveform display with zoom controls and splice management
- JSON state persistence with tape buffer serialization
- Expander support for optional effects processing (BitCrusher, Moog VCF)

---

## TapestryExpander Module

### Overview
**TapestryExpander** is a 4HP companion module that adds two effects to Tapestry's audio output:
- **BitCrusher**: Bit depth reduction (1-16 bits) and sample rate reduction for digital distortion
- **Moog VCF**: 4-pole 24dB/octave resonant low-pass filter

The expander must be placed **directly to the right** of Tapestry. A connection LED indicates when properly connected.

### Architecture
- **Signal Flow**: Tapestry → BitCrusher → Moog VCF → Output
- **Processing**: Stereo throughout (independent L/R channels for filter)
- **Message Passing**: Single shared buffer for zero-latency communication
- **Parameter Smoothing**: 5ms smoothing on all parameters to prevent zipper noise
- **DC Blocking**: High-pass filter at ~20Hz on input to prevent clicks

### Parameters

#### BitCrusher Section
- **CRUSH_BITS_PARAM** (1-16 bits, default 16): Bit depth quantization
  - 16 bits = clean (no effect)
  - 8 bits = lo-fi character
  - 4 bits = harsh digital distortion
  - 1 bit = extreme, square-wave-like
- **CRUSH_RATE_PARAM** (0-100%, default 0%): Sample rate reduction
  - 0% = no reduction (full sample rate)
  - 50% = half sample rate (aliasing begins)
  - 100% = maximum reduction (extreme lo-fi)
- **CRUSH_MIX_PARAM** (0-100%, default 0%): Dry/wet blend
  - 0% = bypass (clean signal)
  - 100% = fully crushed signal

#### Moog VCF Section
- **FILTER_CUTOFF_PARAM** (0-1, default 1.0): Filter cutoff frequency
  - Exponential mapping: 20Hz to 20kHz
  - 0.0 = ~20Hz (removes almost everything)
  - 0.5 = ~632Hz (telephone-like)
  - 1.0 = 20kHz (fully open, no filtering)
- **FILTER_RESO_PARAM** (0-100%, default 0%): Resonance amount
  - 0% = flat response (no emphasis)
  - 50% = moderate resonance
  - 100% = self-oscillation (ringing)
- **FILTER_MIX_PARAM** (0-100%, default 0%): Dry/wet blend
  - 0% = bypass (unfiltered signal)
  - 100% = fully filtered signal

### CV Inputs
All parameters have dedicated CV inputs with the following scaling:
- **CRUSH_BITS_CV**: 1V → +1.5 bits offset
- **CRUSH_RATE_CV**: 1V → +10% rate offset
- **CRUSH_MIX_CV**: 1V → +10% mix offset
- **FILTER_CUTOFF_CV**: 1V → +10% cutoff offset
- **FILTER_RESO_CV**: 1V → +10% resonance offset
- **FILTER_MIX_CV**: 1V → +10% mix offset

### Connection LED
- **Brightness**: 1.0 when connected to Tapestry, 0.0 when disconnected
- **Location**: Top center of panel
- **Purpose**: Visual confirmation of proper expander connection

### DSP Details

#### BitCrusher Algorithm
1. **Sample-and-Hold**: Holds input for N samples based on rate parameter
2. **Bit Quantization**: Reduces bit depth using `floor((x * scale) + 0.5) / scale`
3. **Output**: Quantized signal with aliasing artifacts from rate reduction

#### Moog VCF Algorithm
- **Topology**: 4-pole cascade (four 1-pole filters in series)
- **Formula per stage**: `y = y + cutoff * (x - y)`
- **Resonance**: Feedback from output to input with scaling
- **Response**: 24dB/octave rolloff (6dB per pole)
- **Stereo**: Independent left/right filter instances

#### Parameter Smoothing
- **Time Constant**: 5ms (adjusts to sample rate)
- **Method**: Exponential smoothing
- **Purpose**: Eliminate zipper noise from parameter changes
- **All Parameters**: Bits, rate, crush mix, cutoff, resonance, filter mix

#### Soft Clipping
- **Function**: `tanh(x * 0.5) * 2.0`
- **Purpose**: Gentle saturation to prevent harsh peaks
- **Hard Limit**: `clamp(x, -1.5, 1.5)` as safety

### Usage Tips

#### BitCrusher
- **Subtle Character**: 12-14 bits with low rate reduction
- **Lo-Fi Aesthetic**: 8 bits with 30-50% rate reduction
- **Harsh Digital**: 4-6 bits with 70%+ rate reduction
- **Extreme Glitch**: 1-2 bits with maximum rate

#### Moog VCF
- **Warm Low-Pass**: Cutoff 0.3-0.5, resonance 20-40%
- **Resonant Sweep**: Modulate cutoff with LFO, resonance 60-80%
- **Self-Oscillation**: Cutoff 0.1-0.3, resonance 90-100%
- **Subtle Tone**: High cutoff (0.7-0.9), mix 30-50%

#### Combined Effects
- **BitCrush → Filter**: Tame harsh digital artifacts with low-pass
- **Filter → BitCrush**: Smooth first, then add digital character
- **Modulated Sweep**: Both cutoff and bits via CV for evolving textures
- **Mix Automation**: Fade effects in/out with envelopes

### Performance
- **CPU Usage**: Low (simple DSP algorithms)
- **Latency**: Zero (same-frame processing via shared buffer)
- **Voice Count**: Stereo processing (2 channels)

---

## Core Concepts

### The Tape
**Tapestry** uses a circular tape buffer (default 300 seconds) that continuously loops. Audio is recorded into this buffer using sound-on-sound technique, allowing overdubbing and layering.

### Splices
**Splices** are markers that divide the tape into segments. Think of them as bookmarks or loop points:
- Navigate between splices using the **SHIFT** button or gate input
- Create splices manually with **SPLICE** trigger or automatically during recording
- Splices define the playback boundaries for the granular engine

### Grains
**Grains** are short audio fragments played back with windowing (Hann envelope). The **GENE SIZE** parameter determines grain length. Multiple grains overlap (controlled by **MORPH**) to create smooth, evolving textures.

### Varispeed
**VARISPEED** controls playback speed and direction:
- Center = stopped (playback paused)
- Right of center = forward playback (faster as you turn right)
- Left of center = reverse playback (faster as you turn left)

---

## Parameters

### Recording & Playback

#### RECORD_BUTTON (Range: 0-1)
- **Type**: Toggle button
- **Function**: Enable/disable sound-on-sound recording
- **Behavior**:
  - Press once → start recording (button lights up)
  - Record into current splice position
  - Buffer clearing depends on OVERDUB_TOGGLE state (see below)
  - Automatic splice creation when first input arrives
  - Recording stops at splice boundary or when button pressed again
- **Audio Routing**: Sums L/R inputs, records mono into buffer

#### OVERDUB_TOGGLE (Range: 0-1)
- **Type**: Toggle switch (CKSS)
- **Default**: 0 (OFF - Replace mode)
- **Function**: Controls recording buffer behavior
- **Modes**:
  - **OFF (0)**: Replace mode - `clearAndStartRecording()` clears existing buffer content before recording. This is the traditional behavior for starting a fresh recording.
  - **ON (1)**: Overdub mode - `clearAndStartRecording()` preserves existing buffer content. New audio is layered/merged with what's already recorded, allowing non-destructive multi-take recording.
- **Use Cases**:
  - Replace mode: Clean slate for new recordings, single-take performance capture
  - Overdub mode: Building up layers, adding parts to existing material, loop-based composition
- **Note**: This only affects the initial buffer clearing behavior. Once recording is active, both modes write audio to the buffer at the current position.

#### PLAY_BUTTON (Range: 0-1)
- **Type**: Trigger button with gate input
- **Function**: Retrigger playback from beginning of current splice
- **Behavior**:
  - Button press or gate input rising edge → restart grain engine
  - Resets playhead to current splice start
  - Works even when varispeed is at zero (stopped)
- **Use Case**: Rhythmic retriggering, live performance control

### Granular Controls

#### VARISPEED_PARAM (Range: -1 to +1)
- **Units**: Bipolar speed control
- **Default**: 0.0 (stopped)
- **CV Modulation**: Multiplicative from VARISPEED_CV input (0-10V → -1 to +1)
- **DSP Mapping**:
  - -1.0 → maximum reverse speed
  - -0.5 → half speed reverse
  - 0.0 → stopped (center detent, ±0.05 deadzone)
  - +0.5 → half speed forward
  - +1.0 → maximum forward speed
- **Speed Range**: ±4 octaves (0.0625x to 16x)
- **Effect**: Controls playback direction and rate; at zero, slide knob remains functional for splice creation

#### MORPH_PARAM (Range: 0-1)
- **Units**: Normalized overlap amount
- **Default**: 0.0 (single voice)
- **DSP Mapping**:
  - 0.0-0.33 → 1 voice (no overlap)
  - 0.33-0.66 → 2 voices (50% overlap)
  - 0.66-0.9 → 3 voices (66% overlap)
  - 0.9-1.0 → 4 voices (75% overlap)
- **Voice Behavior**:
  - Single voice: clean playback, no granular effect
  - Multiple voices: overlapping grains with pitch/pan randomization
  - Higher morph = denser, more "swarm-like" texture
- **Trigger Spacing**: Voices triggered at equal phase intervals within gene window

#### ORGANIZE_PARAM (Range: 0-1)
- **Units**: Normalized splice selector
- **Default**: 0.0 (first splice)
- **Function**: Manual splice navigation via knob
- **DSP Mapping**: Maps knob position to splice array index
  - 0.0 → first splice
  - 0.5 → middle splice
  - 1.0 → last splice
- **Behavior**: Smoothly transitions between splices as you turn the knob
- **Priority**: Lower than SHIFT button (shift overrides organize)

#### GENE_SIZE_PARAM (Range: 0.05-5.0 seconds)
- **Units**: Seconds
- **Default**: 1.0 second
- **Function**: Sets the length of each grain window
- **DSP Mapping**: Direct mapping to grain duration
  - 0.05s → very short grains, granular texture
  - 0.5s → medium grains, rhythmic variations
  - 2.0s → long grains, smooth transitions
  - 5.0s → very long grains, almost continuous playback
- **Interaction**: Works with SLIDE to create window within splice
- **Note**: Gene size cannot exceed splice length (automatically clamped)

#### SLIDE_PARAM (Range: 0-1)
- **Units**: Normalized position within splice
- **Default**: 0.0 (beginning)
- **Function**: Offsets grain start position within current splice
- **DSP Mapping**:
  ```cpp
  slideOffset = slide * (spliceLength - geneSamples)
  grainStart = spliceStart + slideOffset
  ```
- **Effect**: 
  - 0.0 → grains start at beginning of splice
  - 0.5 → grains start halfway through splice
  - 1.0 → grains start at end of splice (minus gene size)
- **Special Feature**: Functional even when varispeed is at zero, enabling precise splice creation

### Navigation Controls

#### SHIFT_BUTTON (Range: 0-1)
- **Type**: Trigger button with gate input
- **Function**: Immediate jump to next splice
- **Behavior**:
  - Button press or gate rising edge → advance to next splice
  - Wraps around: last splice → first splice
  - Updates playhead position instantly
  - Overrides ORGANIZE knob
  - Retriggers grain engine at new splice start
- **Use Case**: Sequential splice navigation, live performance switching
- **Works When Stopped**: Yes, allows splice selection while varispeed is zero

#### SPLICE_BUTTON (Range: 0-1)
- **Type**: Trigger button with gate input
- **Function**: Create splice marker at current playhead position
- **Behavior**:
  - Button press or gate rising edge → insert new splice
  - Splice created at current grain playhead position (includes slide offset)
  - **Alternative**: Left-click on waveform display to create splice at any position
  - Maximum 64 splices supported
  - Only active in Normal mode (not while recording)
  - Visual feedback: new marker appears in waveform display
- **Use Case**: Marking interesting moments, creating rhythmic divisions
- **Note**: Waveform clicking provides more precise control than button/gate for manual splice creation

#### CLEAR_SPLICES_BUTTON (Range: 0-1)
- **Type**: Momentary button with LED indicator
- **Function**: Remove all splice markers from the tape
- **Behavior**:
  - Single button press → deletes all splices
  - Buffer content is preserved (audio not affected)
  - Creates single splice spanning entire used buffer
  - Resets current splice index to 0
  - LED: dim when splices exist, off when empty
- **Use Case**: Quick reset of splice structure, starting fresh navigation
- **Note**: Same as "Clear All Splices" in context menu

---

## Inputs

### Audio Inputs

#### AUDIO_L_INPUT & AUDIO_R_INPUT
- **Type**: Audio rate (polyphonic capable, uses channel 0)
- **Range**: ±5V (Eurorack standard)
- **Function**: Stereo audio input for recording
- **Processing**: Summed to mono before recording to tape buffer
- **Recording Level**: No automatic gain control, records at input level

### Control Inputs

#### CLOCK_INPUT
- **Type**: Gate/trigger
- **Function**: External clock for granular sync
- **Status**: Declared but not currently implemented in DSP
- **Future Use**: Tempo-sync for grain triggers

#### PLAY_INPUT
- **Type**: Gate/trigger
- **Voltage**: Schmitt trigger with 0.1V/2.0V thresholds
- **Function**: External control for PLAY button
- **Behavior**: Rising edge retriggers playback

#### VARISPEED_CV_INPUT
- **Type**: CV (control voltage)
- **Range**: 0-10V → maps to -1 to +1
- **Function**: Voltage control of playback speed/direction
- **Scaling**: `finalSpeed = varispeParam + (cvInput - 5V) / 5V`
- **Use Case**: Modulate playback rate with LFO, envelope, or sequencer

#### SPLICE_INPUT
- **Type**: Gate/trigger
- **Voltage**: Schmitt trigger with 0.1V/2.0V thresholds
- **Function**: External control for splice creation
- **Behavior**: Rising edge creates splice at current position

#### SHIFT_INPUT
- **Type**: Gate/trigger
- **Voltage**: Schmitt trigger with 0.1V/2.0V thresholds
- **Function**: External control for splice navigation
- **Behavior**: Rising edge advances to next splice

---

## Outputs

#### AUDIO_L_OUTPUT & AUDIO_R_OUTPUT
- **Type**: Audio rate stereo pair
- **Range**: ±5V (Eurorack standard)
- **Content**: Processed granular audio
- **Processing Chain**:
  1. Grain engine generates 4 overlapping voices
  2. Each voice applies Hann windowing
  3. Pitch randomization (high morph)
  4. Stereo panning (high morph, 3+ voices)
  5. Mix down with voice normalization
  6. Smooth crossfades between splice boundaries
  7. Output scaling to Eurorack levels

---

## Display & Interaction

### Waveform Display
- **Size**: 320x180 pixels
- **Features**:
  - Real-time waveform rendering with automatic zoom
  - Red playhead cursor showing current position
  - Blue splice markers (vertical lines)
  - Interactive hover feedback with visual indicators
  - Grayscale waveform with peak detection
- **Interaction**:
  - **Left-click on waveform** → create new splice at click position
  - **Left-click on splice marker** → select/jump to that splice
  - **Right-click on splice marker** → delete that marker
  - **Hover over waveform** → green line preview shows where splice will be created
  - **Hover over marker** → red line highlight indicates marker can be deleted
  - **Triangle indicator** → appears at top of hover line showing exact position
  - **Hit detection** → 6-pixel tolerance on each side of markers for easy clicking

### Zoom Controls
- **Zoom In** (+): Increase waveform magnification
- **Zoom Out** (−): Decrease waveform magnification
- **Range**: 0.1x to 10x
- **Effect**: Zoom centers on current playhead position

### Context Menu
- **Clear Tape**: Erase all recorded audio and reset splices
- **Clear All Splices**: Remove all splice markers (keeps audio, same as Clear Splices button)
- **Remove Last Splice**: Delete most recently created splice
- **Tape Info**: Display buffer status and recording time

### Button Controls
- **Clear Splices Button**: Dedicated button with white LED indicator for quick splice clearing
  - LED dim (0.3 brightness) when splices exist
  - LED off when no splices or empty buffer
  - Provides instant visual feedback of splice structure state

---

## Workflow Examples

### Basic Recording & Playback
1. Set **VARISPEED** to center (stopped)
2. Press **RECORD** button (lights up)
3. Play audio into **AUDIO L/R** inputs
4. First audio creates initial splice automatically
5. Press **RECORD** again to stop
6. **Click on waveform** to create additional splice markers at interesting points
7. Turn **VARISPEED** right to play forward
8. Adjust **SLIDE** to explore different positions
9. **Right-click markers** to remove unwanted splices

### Live Looping with Overdubs
1. Record initial phrase with **OVERDUB** toggle OFF (replace mode)
### Expander + Modulation
- **Filter Sweep**: LFO to cutoff, resonance at 70%, mix 100%
- **Rhythmic Crushing**: Clock-synced envelope to rate and bits
- **Random Glitch**: Random CV to bit depth, S&H to rate
- **Dynamic Mix**: Envelope follower to both mix parameters
- **Pitch-Follow Filter**: Varispeed CV mult to filter cutoff (via attenuator)

2. Toggle **OVERDUB** switch ON (up position) to enable layering
3. Press **PLAY** to return to start
4. Turn **VARISPEED** to play back loop
5. Press **RECORD** - buffer is preserved, ready for layering
6. New audio merges with existing content (non-destructive)
7. Repeat steps 3-6 to build up multiple layers
8. Use **SPLICE** trigger to mark sections
9. Navigate with **SHIFT** button

### Granular Texture Creation
1. Record source material (sustained tones work well)
2. Set **MORPH** to 0.8-1.0 (4 voices)
3. Set **GENE SIZE** to 0.1-0.5 seconds (short grains)
4. Slowly modulate **VARISPEED** with LFO
5. Use **SLIDE** to scan through audio
6. Result: evolving, cloud-like texture

### Rhythmic Slicing
1. Record rhythmic material (drums, percussion)
2. Create splices at each hit using **SPLICE** trigger
3. Set **VARISPEED** to stopped (center)
4. Use **SHIFT** gate input with clock to step through slices
5. Each slice plays as complete rhythmic fragment
6. Vary **ORGANIZE** knob for non-linear playback order

### Reverse Echo Effect
1. Record short phrase
2. Set **VARISPEED** slightly left of center (slow reverse)
3. Set **GENE SIZE** to match phrase duration
4. Press **PLAY** to retrigger
5. Result: reverse echo/time-reverse effect
6. Modulate **SLIDE** for variations

---

## Technical Details

### Buffer Management
- **Default Size**: 300 seconds at 44.1kHz (13,230,000 samples)
- **Format**: Mono float32, circular buffer
- **Recording**: Lock-free atomic write pointer
- **Playback**: Multiple read heads (one per grain voice)
- **Persistence**: Full buffer saved with patch (can be large)

### Grain Engine
- **Voices**: Up to 4 simultaneous grains
- **Window**: Hann (raised cosine) for smooth envelope
- **Interpolation**: Cubic for high-quality pitch shifting
- **Voice Spacing**: Phase-based triggers at morph-dependent intervals
- **Pitch Randomization**: ±10% at high morph settings
- **Stereo Width**: Panning spread for 3+ voices

### Splice System
- **Maximum**: 64 splices
- **Storage**: Array of frame positions with metadata
- **Navigation**: Linear or knob-based selection
- **Creation**: Manual (button/gate) or automatic (recording start)
- **Wraparound**: Playback loops within splice boundaries

### Sample Rate Adaptation
- **Design**: All DSP runs at Rack engine sample rate
- **Ratio**: Calculated as `moduleSampleRate / 44100.0`
- **Affected Parameters**: Varispeed, grain triggers, window phase
- **Buffer**: Fixed 44.1kHz regardless of engine rate

---

## Performance Tips

### CPU Optimization
- **Lower Morph** = fewer active voices = less CPU
- Single voice mode (morph = 0) is most efficient
- Gene size doesn't significantly affect CPU usage
- Recording is lightweight (simple buffer write)

### Memory Considerations
- 300-second buffer ≈ 50MB RAM
- Patch files include full buffer (can be large)
- Consider clearing tape before saving if empty
- Each splice adds minimal overhead (~16 bytes)

### Musical Tips
- **Smooth Transitions**: Use high morph + medium gene size
- **Glitchy Textures**: Low gene size + fast varispeed changes
- **Natural Loops**: Align splices with musical phrases
- **Drone Synthesis**: Long gene size + slow varispeed + high morph
- **Rhythmic Stutter**: Fast shift triggering with short splices

---

## Advanced Techniques

### Probability-Based Navigation
- Patch random CV to quantizer → shift input
- Creates unpredictable splice navigation
- Set range to control which splices are accessible

### Envelope-Controlled Grains
- Route envelope to varispeed CV
- Creates attack/decay on grain playback
- Combine with fast shift triggers for percussive effects

### Feedback Recording
- Route tapestry output back to input (with mixer level control)
- Creates evolving, degrading tape loops
- Add delay/reverb in feedback path for complex textures

### Multi-Splice Sequencing
- Create 8/16 splices at equal intervals
- Use sequencer to send gates to shift input
- Synchronize with clock for rhythmic patterns
- Vary organize knob while sequencing for variations

---

## Troubleshooting

### No Sound Output
- Check varispeed is not at center (stopped)
- Verify audio was actually recorded (check waveform display)
- Ensure splice exists (recording creates initial splice)
- Check output connections

### Recording Not Working
- Press record button (should light up)
- Send audio to inputs (waveform should appear)
- Check recording mode is Normal (not playback only)
- Verify inputs are connected and sending signal

### Playhead Stuck
- This was fixed in v2.0 - update to latest version
- If using old version: restart playback or shift to new splice

### Splices Not Creating
- **Try clicking directly on waveform** - most reliable method
- Ensure not in record mode when using splice trigger button/gate
- Check that playhead position is valid (within buffer)
- Maximum 64 splices - clear some if full
- When hovering, green line should appear showing where splice will be created
- **TapestryExpander**: DC blocking should prevent most clicks

### Expander Not Connecting
- Ensure TapestryExpander is placed **directly to right** of Tapestry
- No space between modules (must be adjacent)
- Connection LED should light up when properly connected
- Restart VCV Rack if connection fails
- Check that both modules are latest version

### Effects Not Audible
- Verify mix knobs are turned up (start at 50-100%)
- Check that effect parameters are set (not at default bypass values)
- BitCrusher: needs bits < 16 or rate > 0 to hear effect
- Filter: needs cutoff < 1.0 to hear filtering
- Connection LED must be lit for expander to work
- Cannot create splice at frame 0 (beginning) - this is always a splice marker

### Audio Glitches/Clicks
- Increase gene size for smoother playback
- Raise morph value for more overlap
- Check CPU usage (lower voice count if needed)
- Ensure varispeed changes are gradual
 (Tapestry)
- **Digital**: Pure software implementation (no tape saturation/character)
- **Stereo I/O**: Full stereo recording and playback paths
- **Visual Feedback**: Real-time waveform display with splice markers
- **Longer Buffer**: 300 seconds vs hardware limitations
- **CV Inputs**: More extensive CV control options
- **No Vari-Tone**: Pitch shifting not implemented separately
- **No SC Mode**: Simplified to normal playback mode only
- **4 Voices Max**: Hardware may have different voice architecture
---
### Key Differences (TapestryExpander)
- **Optional Module**: Separate 4HP expander vs built-in effects
- **BitCrusher**: Digital distortion effect (not in Morphagene)
- **Moog VCF**: Software emulation of classic filter
- **Independent Mix**: Separate dry/wet for each effect
- **CV Control**: All parameters voltage-controllable
- **Varispeed Playback**: Forward and reverse operation
- **Effects DSP**: BitCrusher and Moog VCF emulation (TapestryExpander)
- **UI/Display**: NanoVG waveform rendering

---

## Version History

### v2.0.0 (2025-12-21)
- **TapestryExpander module added**: BitCrusher and Moog VCF effects
- Expander features:
  - Bit depth reduction (1-16 bits) with sample rate reduction
  - 4-pole resonant low-pass filter (Moog emulation)
  - Independent dry/wet mix controls for each effect
  - Full CV control over all parameters
  - Parameter smoothing to prevent zipper noise
  - DC blocking to prevent clicks
  - Zero-latency processing via shared message buffer
- Bug fixes and stability improvements
- Documentation updates
### Key Differences
- **Digital**: Pure software implementation (no tape saturation/character)
- **Stereo I/O**: Full stereo recording and playback paths
- **Visual Feedback**: Real-time waveform display with splice markers
- **Longer Buffer**: 300 seconds vs hardware limitations
- **CV Inputs**: More extensive CV control options
- **No Vari-Tone**: Pitch shifting not implemented separately
- **No SC Mode**: Simplified to normal playback mode only
- **4 Voices Max**: Hardware may have different voice architecture

---

## Credits

**Module Design**: Based on Make Noise Morphagene concept  
**Implementation**: Shortwav Labs  
**DSP Components**: Custom granular engine with overlap synthesis  
**UI/Display**: NanoVG waveform rendering  

---

## Version History

### v2.0.0 (2025-12-19)
- Initial release
- Core granular engine with 4-voice overlap
- Sound-on-sound recording with overdub toggle
  - Replace mode (default): clears buffer on new recording
  - Overdub mode: preserves existing audio for layering
- Splice management system
- Waveform display with zoom
- Full CV control

---

## See Also

- [Tapestry Quick Start Guide](Tapestry_QuickStart.md)
- [WavPlayer Module](WavPlayer.md) - For traditional sample playback
- Make Noise Morphagene Manual - Original hardware inspiration
