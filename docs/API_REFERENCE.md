# Tapestry API Reference

**Version**: 2.0.0  
**Last Updated**: January 4, 2026

Complete technical reference for the Tapestry VCV Rack plugin.

---

## Table of Contents

1. [Module Overview](#module-overview)
2. [Tapestry Main Module](#tapestry-main-module)
3. [Tapestry Expander Module](#tapestry-expander-module)
4. [DSP Core Classes](#dsp-core-classes)
5. [Data Structures](#data-structures)
6. [Configuration Constants](#configuration-constants)
7. [Expander Communication](#expander-communication)

---

## Module Overview

The Tapestry plugin consists of two VCV Rack modules:

- **Tapestry**: Main granular processor with recording, splicing, and playback
- **TapestryExpander**: Effects processor with bit crusher and Moog VCF filter

Both modules communicate via VCV Rack's expander protocol for seamless audio routing.

---

## Tapestry Main Module

### Class: `Tapestry`

Main VCV Rack module implementing the granular processor.

**Inherits**: `rack::Module`

**Header**: `src/Tapestry.hpp`

---

### Parameters

#### Enum: `ParamIds`

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `SOS_PARAM` | Knob | 0.0 - 1.0 | 0.5 | Recording mix/crossfade control |
| `GENE_SIZE_PARAM` | Knob | 0.0 - 1.0 | 0.5 | Grain size (1ms to full reel) |
| `GENE_SIZE_CV_ATTEN` | Knob | -1.0 - 1.0 | 0.0 | Grain Size CV attenuverter |
| `VARI_SPEED_PARAM` | Knob | -1.0 - 1.0 | 0.0 | Playback speed/direction |
| `VARI_SPEED_CV_ATTEN` | Knob | -1.0 - 1.0 | 0.0 | Speed CV attenuverter |
| `MORPH_PARAM` | Knob | 0.0 - 1.0 | 0.0 | Grain overlap/density |
| `SLIDE_PARAM` | Knob | 0.0 - 1.0 | 0.0 | Grain position offset |
| `SLIDE_CV_ATTEN` | Knob | -1.0 - 1.0 | 0.0 | Scan CV attenuverter |
| `ORGANIZE_PARAM` | Knob | 0.0 - N-1 | 0.0 | Manual splice selection |
| `REC_BUTTON` | Button | - | - | Record toggle |
| `SPLICE_BUTTON` | Button | - | - | Create/delete marker |
| `NEXT_BUTTON` | Button | - | - | Advance to next splice |
| `CLEAR_SPLICES_BUTTON` | Button | - | - | Clear all splices |
| `SPLICE_COUNT_TOGGLE_BUTTON` | Button | - | - | Toggle marker count (4/8/16) |
| `OVERDUB_TOGGLE` | Switch | 0/1 | 0 | Overdub mode enable |

---

### Inputs

#### Enum: `InputIds`

| Input | Type | Range | Description |
|-------|------|-------|-------------|
| `AUDIO_IN_L` | Audio | ±5V | Left audio input |
| `AUDIO_IN_R` | Audio | ±5V | Right audio input |
| `SOS_CV_INPUT` | CV | 0-8V | Mix CV input (unipolar) |
| `GENE_SIZE_CV_INPUT` | CV | ±8V | Grain Size CV (bipolar) |
| `VARI_SPEED_CV_INPUT` | CV | ±4V | Speed CV (bipolar) |
| `MORPH_CV_INPUT` | CV | 0-5V | Density CV (unipolar) |
| `SLIDE_CV_INPUT` | CV | 0-8V | Scan CV (unipolar) |
| `ORGANIZE_CV_INPUT` | CV | 0-5V | Select CV (unipolar) |
| `CLK_INPUT` | Gate | 0-10V | Clock input for time stretch |
| `PLAY_INPUT` | Gate | 0-10V | Play/pause trigger |
| `REC_INPUT` | Gate | 0-10V | Record gate |
| `SPLICE_INPUT` | Gate | 0-10V | Marker creation trigger |
| `NEXT_INPUT` | Gate | 0-10V | Next marker trigger |

---

### Outputs

#### Enum: `OutputIds`

| Output | Type | Range | Description |
|--------|------|-------|-------------|
| `AUDIO_OUT_L` | Audio | ±5V | Left audio output |
| `AUDIO_OUT_R` | Audio | ±5V | Right audio output |
| `ENVELOPE_CV_OUT` | CV | 0-8V | Grain envelope output |
| `EOG_OUT` | Gate | 0-10V | End-of-Grain gate |
| `EOS_OUT` | Gate | 0-10V | End-of-Marker gate |

---

### Public Methods

#### `void onReset()`

Resets the module to initial state.

- Clears audio buffer
- Deletes all splice markers
- Resets file state
- Resets button states
- Resets waveform color to default

**Called**: When user resets module or initializes new instance

---

#### `void process(const ProcessArgs& args)`

Main audio processing callback. Called once per sample by VCV Rack.

**Parameters**:
- `args.sampleRate`: Current sample rate
- `args.sampleTime`: Current sample time

**Processing Order**:
1. Read overdub toggle state
2. Apply pending splice markers (from file load)
3. Process button inputs and combinations
4. Process gate/trigger inputs
5. Read parameter values
6. Read CV inputs
7. Update DSP state
8. Process audio
9. Update expander communication
10. Set output values
11. Update lights

---

#### `json_t* dataToJson()`

Serializes module state to JSON for patch saving.

**Returns**: `json_t*` - JSON object containing:
- Current reel index
- Waveform color
- Marker count mode
- Marker positions
- Current splice index
- File path (if reel saved)

**Example JSON**:
```json
{
  "currentReelIndex": 0,
  "waveformColor": 3,
  "spliceCountMode": 0,
  "spliceMarkers": [0.0, 0.25, 0.5, 0.75],
  "currentMarkerIndex": 2,
  "currentFilePath": "/path/to/reel_00.wav"
}
```

---

#### `void dataFromJson(json_t* rootJ)`

Deserializes module state from JSON when loading patch.

**Parameters**:
- `rootJ`: JSON object from patch file

**Note**: Markers are stored as pending and applied after file loads.

---

#### `void saveCurrentReel()`

Saves current reel to WAV file.

**Behavior**:
- Opens file dialog if no current path
- Writes 32-bit float WAV at 48kHz
- Updates file name and path
- Thread-safe with `fileSaving` atomic flag

---

#### `void loadReel()`

Loads a reel from WAV file.

**Behavior**:
- Opens file dialog for selection
- Supports mono and stereo WAV files
- Resamples to 48kHz if needed
- Clears existing splices
- Thread-safe with `fileLoading` atomic flag

---

### Private Methods

#### `void processButtons(const ProcessArgs& args)`

Processes button press/hold detection.

**Handles**:
- REC button: Record toggle, hold to clear reel
- SPLICE button: Create splice, hold to delete
- NEXT button: Advance to next splice
- Button hold timers and state tracking

---

#### `void processButtonCombos(const ProcessArgs& args)`

Processes multi-button combinations.

**Combinations**:
- NEXT + CLEAR: Clear all splices
- NEXT + COUNT: Cycle splice count mode

---

#### `void processGateInputs(const ProcessArgs& args)`

Processes gate and trigger inputs.

**Inputs**:
- CLK: Grain triggering for time stretch
- PLAY: Play/pause state
- REC: Recording gate
- SPLICE: Marker creation trigger
- NEXT: Next splice trigger

---

#### `void updateSelectParamRange()`

Updates the Select parameter range based on current marker count.

**Behavior**:
- Sets max value to `numMarkers - 1`
- Called after splice creation/deletion
- Called after loading splices from file

---

### Waveform Colors

#### Enum: `WaveformColor`

Available waveform display colors:

| Color | Index | RGB Value |
|-------|-------|-----------|
| Red | 0 | #FF0000 |
| Amber | 1 | #FFBF00 |
| Green | 2 | #00FF00 |
| BabyBlue | 3 | #89CFF0 |
| Peach | 4 | #FFE5B4 |
| Pink | 5 | #FF69B4 |
| White | 6 | #FFFFFF |

**Access**: Right-click module → Waveform Color submenu

---

## Tapestry Expander Module

### Class: `TapestryExpander`

Effects expander module for Tapestry.

**Inherits**: `rack::Module`

**Header**: `src/TapestryExpander.hpp`

---

### Parameters

#### Enum: `ParamIds`

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `CRUSH_BITS_PARAM` | Knob | 1.0 - 16.0 | 16.0 | Bit depth (bits) |
| `CRUSH_RATE_PARAM` | Knob | 0.0 - 1.0 | 1.0 | Sample rate reduction |
| `CRUSH_MIX_PARAM` | Knob | 0.0 - 1.0 | 0.0 | Bit crusher dry/wet |
| `FILTER_CUTOFF_PARAM` | Knob | 0.0 - 1.0 | 1.0 | Filter cutoff (log scale) |
| `FILTER_RESO_PARAM` | Knob | 0.0 - 1.0 | 0.0 | Filter resonance |
| `FILTER_MIX_PARAM` | Knob | 0.0 - 1.0 | 0.0 | Filter dry/wet |
| `OUTPUT_LEVEL_PARAM` | Knob | 0.0 - 2.0 | 1.0 | Output gain |

---

### Inputs

#### Enum: `InputIds`

| Input | Type | Range | Description |
|-------|------|-------|-------------|
| `CRUSH_BITS_CV_INPUT` | CV | 0-10V | Bit depth CV |
| `CRUSH_RATE_CV_INPUT` | CV | 0-10V | Rate CV |
| `CRUSH_MIX_CV_INPUT` | CV | 0-10V | Mix CV |
| `FILTER_CUTOFF_CV_INPUT` | CV | 0-10V | Cutoff CV |
| `FILTER_RESO_CV_INPUT` | CV | 0-10V | Resonance CV |
| `FILTER_MIX_CV_INPUT` | CV | 0-10V | Mix CV |

---

### Lights

#### Enum: `LightIds`

| Light | Type | Description |
|-------|------|-------------|
| `CONNECTED_LIGHT` | LED | Expander connection status |

---

### Public Methods

#### `void process(const ProcessArgs& args)`

Main processing loop for effects.

**Processing Chain**:
1. Check expander connection
2. Read audio from Tapestry via expander message
3. Apply bit crusher (if mix > 0)
4. Apply Moog VCF filter (if mix > 0)
5. Apply output level
6. Write processed audio back to Tapestry
7. Update connection LED

---

### DSP Components

#### `BitCrusherDSP bitCrusher_`

Bit crusher effect processor (per-sample).

**Methods**:
- `process(float input, float bits, float rate)`: Process one sample

---

#### `MoogVCFDSP moogFilterL_`, `moogFilterR_`

Moog VCF 24dB/octave lowpass filters (one per channel).

**Methods**:
- `setCutoff(float cutoff)`: Set cutoff frequency (Hz)
- `setResonance(float resonance)`: Set resonance (0-1)
- `setSampleRate(float sampleRate)`: Update sample rate
- `process(float input)`: Process one sample

---

## DSP Core Classes

### Class: `TapestryDSP`

Top-level DSP engine coordinating all processing.

**Header**: `src/dsp/tapestry-dsp.h`

---

### Public Methods

#### `void setSampleRate(float sampleRate)`

Sets the module's sample rate.

**Parameters**:
- `sampleRate`: Target sample rate (typically 44.1kHz or 48kHz)

**Note**: Internal processing always runs at 48kHz; resampling is applied automatically.

---

#### `void clearReel()`

Clears the entire audio buffer.

**Effects**:
- Zeros all audio data
- Resets playback position
- Does not affect splice markers

---

#### `void process(float inL, float inR, float* outL, float* outR)`

Processes one sample through the engine.

**Parameters**:
- `inL`: Left input sample
- `inR`: Right input sample
- `outL`: Pointer to left output sample
- `outR`: Pointer to right output sample

**Processing Flow**:
1. Resampling (if needed)
2. Recording (if active)
3. Grain generation and playback
4. Marker management
5. Output envelope generation
6. Resampling back to module rate

---

#### Parameter Setters

| Method | Parameter | Range | Description |
|--------|-----------|-------|-------------|
| `setSos(float)` | Mix | 0.0-1.0 | Recording crossfade |
| `setGeneSize(float)` | Grain Size | 0.0-1.0 | Grain size (exponential) |
| `setVariSpeed(float)` | Speed | -1.0-1.0 | Playback rate |
| `setDensity(float)` | Density | 0.0-1.0 | Grain overlap |
| `setScan(float)` | Scan | 0.0-1.0 | Position offset |
| `setSelect(float)` | Select | 0.0-1.0 | Marker selection (normalized) |

---

#### CV Input Methods

| Method | Parameters | Description |
|--------|------------|-------------|
| `setSosCv(float cv)` | cv: 0-8V | Mix CV input |
| `setGeneSizeCv(float cv, float atten)` | cv: ±8V, atten: ±1 | Grain Size CV with attenuverter |
| `setVariSpeedCv(float cv, float atten)` | cv: ±4V, atten: ±1 | Speed CV with attenuverter |
| `setDensityCv(float cv)` | cv: 0-5V | Density CV input |
| `setScanCv(float cv, float atten)` | cv: 0-8V, atten: ±1 | Scan CV with attenuverter |
| `setSelectCv(float cv)` | cv: 0-5V | Select CV input |

---

#### State Getters

| Method | Return Type | Description |
|--------|-------------|-------------|
| `isRecording()` | `bool` | Recording active |
| `isPlaying()` | `bool` | Playback active |
| `getBuffer()` | `TapestryBuffer&` | Buffer reference |
| `getSpliceManager()` | `MarkerManager&` | Marker manager reference |
| `getCurrentPosition()` | `size_t` | Current playback frame |
| `getEnvelopeOut()` | `float` | Current envelope (0-1) |
| `getEogGate()` | `bool` | End-of-Grain gate state |
| `getEosGate()` | `bool` | End-of-Marker gate state |

---

### Class: `TapestryBuffer`

Circular audio buffer for reel storage.

**Header**: `src/dsp/tapestry-buffer.h`

---

### Public Methods

#### `void write(float left, float right)`

Writes one stereo frame to the buffer.

**Parameters**:
- `left`: Left channel sample
- `right`: Right channel sample

**Behavior**:
- Wraps at buffer end
- Updates used frame count

---

#### `void read(size_t frame, float& left, float& right) const`

Reads one stereo frame from the buffer.

**Parameters**:
- `frame`: Frame index (0 to usedFrames-1)
- `left`: Reference to left output
- `right`: Reference to right output

**Note**: Reading beyond used frames returns zeros.

---

#### `void readInterpolated(double position, float& left, float& right) const`

Reads with linear interpolation for smooth pitch shifting.

**Parameters**:
- `position`: Fractional frame position
- `left`: Reference to left output
- `right`: Reference to right output

---

#### `size_t getUsedFrames() const`

Returns number of frames containing audio.

**Returns**: Frame count (0 to `kMaxReelFrames`)

---

#### `void clear()`

Zeros all audio data and resets frame count.

---

#### `bool loadFromFile(const std::string& path)`

Loads audio from WAV file.

**Parameters**:
- `path`: Full path to WAV file

**Returns**: `true` on success, `false` on error

**Supported Formats**:
- Mono and stereo
- Any sample rate (resampled to 48kHz)
- 16-bit, 24-bit, 32-bit int, 32-bit float

---

#### `bool saveToFile(const std::string& path) const`

Saves current buffer to WAV file.

**Parameters**:
- `path`: Full path for output file

**Returns**: `true` on success, `false` on error

**Format**: 32-bit float stereo WAV at 48kHz

---

### Class: `MarkerManager`

Manages splice markers and navigation.

**Header**: `src/dsp/tapestry-splice.h`

---

### Public Methods

#### `void addMarker(size_t position, size_t totalFrames)`

Adds a new splice marker at the specified position.

**Parameters**:
- `position`: Frame position for marker
- `totalFrames`: Total buffer frames (for validation)

**Behavior**:
- Inserts in sorted order
- Limits to max 300 splices
- Creates segments between markers

---

#### `void deleteMarker(size_t index)`

Removes a splice marker by index.

**Parameters**:
- `index`: Marker index (0 to numMarkers-1)

---

#### `void deleteAllMarkers()`

Removes all splice markers.

---

#### `void setCurrentIndex(size_t index)`

Sets the current splice for playback.

**Parameters**:
- `index`: Target splice index

**Behavior**:
- Clamps to valid range
- Updates playback position

---

#### `void advanceToNext()`

Advances to the next splice (wraps at end).

---

#### `size_t getNumMarkers() const`

Returns current splice count.

**Returns**: Number of splices (0-300)

---

#### `Marker getCurrentMarker() const`

Returns the current splice marker.

**Returns**: `Marker` with start/end frames

---

#### `void setFromMarkerPositions(const std::vector<float>& positions, size_t totalFrames)`

Sets splices from normalized position array (used for JSON loading).

**Parameters**:
- `positions`: Normalized positions (0.0-1.0)
- `totalFrames`: Buffer size for denormalization

---

#### `std::vector<float> getMarkerPositions(size_t totalFrames) const`

Gets normalized splice positions for JSON saving.

**Parameters**:
- `totalFrames`: Buffer size for normalization

**Returns**: Vector of normalized positions

---

### Class: `GrainEngine`

Multi-voice granular synthesis engine.

**Header**: `src/dsp/tapestry-grain.h`

---

### Public Methods

#### `void setGeneSize(float geneSizeSamples)`

Sets grain length in samples.

**Parameters**:
- `geneSizeSamples`: Length (48 to 8352000)

---

#### `void setDensityState(const MorphState& state)`

Sets morph parameters (overlap, pitch variation, pan).

**Parameters**:
- `state`: Density state struct

---

#### `void setSlide(float slide)`

Sets grain position offset (0-1).

**Parameters**:
- `slide`: Offset amount (0.0-1.0)

---

#### `void onClockRising()`

Notifies engine of clock pulse for time stretch mode.

**Behavior**:
- Measures clock period
- Enables time-stretched granulation
- Triggers grain based on morph setting

---

#### `void process(const TapestryBuffer& buffer, const SpliceMarker& splice, double& position, float& outL, float& outR)`

Processes one sample of granular output.

**Parameters**:
- `buffer`: Audio buffer to read from
- `splice`: Current splice boundaries
- `position`: Playback position (updated)
- `outL`: Left output sample
- `outR`: Right output sample

---

## Data Structures

### `Marker`

Defines audio segment boundaries.

**Header**: `src/dsp/tapestry-core.h`

```cpp
struct SpliceMarker {
    size_t startFrame;  // Start frame index
    size_t endFrame;    // End frame index (exclusive)
    
    size_t length() const;  // Returns segment length
    bool isValid() const;   // Returns true if end > start
};
```

---

### `GrainVoice`

State for one grain playback voice.

**Header**: `src/dsp/tapestry-core.h`

```cpp
struct GrainVoice {
    double position;     // Sample position in buffer
    float phase;         // Envelope phase (0-1)
    float pitchRatio;    // Playback speed multiplier
    float panL, panR;    // Stereo pan coefficients
    bool active;         // Voice is playing
    
    void reset();        // Deactivate and reset state
};
```

---

### `TapestryExpanderMessage`

Communication payload between Tapestry and Expander.

**Header**: `src/TapestryExpanderMessage.hpp`

```cpp
struct TapestryExpanderMessage {
    // Tapestry → Expander
    float audioL;                // Pre-output audio (left)
    float audioR;                // Pre-output audio (right)
    float sampleRate;            // Current sample rate
    
    // Expander → Tapestry
    float processedL;            // Post-effects audio (left)
    float processedR;            // Post-effects audio (right)
    bool expanderConnected;      // Connection flag
};
```

**Usage**:
1. Tapestry writes audio to expander message
2. VCV Rack flips buffers
3. Expander reads audio, processes effects
4. Expander writes processed audio
5. VCV Rack flips buffers
6. Tapestry reads processed audio for output

---

### `MorphState`

Granular synthesis morph parameters.

**Header**: `src/dsp/tapestry-core.h`

```cpp
struct MorphState {
    float overlap;          // Voice overlap (1-4)
    float pitchVariation;   // Random pitch amount (0-1)
    float stereoSpread;     // Random pan amount (0-1)
    
    void calculate(float morph);  // Derives from morph (0-1)
};
```

---

### `VariSpeedState`

Playback speed/direction state.

**Header**: `src/dsp/tapestry-core.h`

```cpp
struct VariSpeedState {
    float speed;          // Speed multiplier (e.g., 0.5, 1.0, 2.0)
    int direction;        // 1 = forward, -1 = reverse
    float semitones;      // Semitone offset
    
    void calculate(float variSpeed);  // Derives from param (-1 to +1)
};
```

**Calculation**:
- `variSpeed < 0`: Reverse playback, speed from 0 to 1.0
- `variSpeed = 0`: Forward at 1.0x
- `variSpeed > 0`: Forward, speed from 1.0 to 2.0

**Semitone Range**:
- Down: 0 to -26 semitones
- Up: 0 to +12 semitones

---

## Configuration Constants

### `TapestryConfig`

System-wide configuration constants.

**Header**: `src/dsp/tapestry-core.h`

```cpp
struct TapestryConfig {
    // Audio specifications
    static constexpr float kInternalSampleRate = 48000.0f;
    static constexpr size_t kMaxReelFrames = 8352000;  // ~2.9 min
    static constexpr size_t kMaxSplices = 300;
    static constexpr size_t kMaxGrainVoices = 4;
    static constexpr size_t kMaxReels = 32;
    
    // Gene size limits (samples @ 48kHz)
    static constexpr float kMinGeneSamples = 48.0f;    // ~1ms
    static constexpr float kMaxGeneSamples = 8352000.0f;
    
    // CV voltage ranges
    static constexpr float kSosCvMax = 8.0f;
    static constexpr float kGeneSizeCvMax = 8.0f;
    static constexpr float kVariSpeedCvMax = 4.0f;
    static constexpr float kMorphCvMax = 5.0f;
    static constexpr float kSlideCvMax = 8.0f;
    static constexpr float kOrganizeCvMax = 5.0f;
    static constexpr float kGateTriggerThreshold = 2.5f;
    
    // Output levels
    static constexpr float kAudioOutLevel = 5.0f;      // ±5V audio
    static constexpr float kCvOutMax = 8.0f;           // 0-8V CV
    static constexpr float kGateOutLevel = 10.0f;      // 0-10V gates
};
```

---

## Expander Communication

### Protocol

VCV Rack's expander system uses double-buffered messaging:

1. **Producer Module** (Tapestry):
   - Writes to `rightExpander.producerMessage`
   - VCV Rack flips buffers after process()

2. **Consumer Module** (Expander):
   - Reads from `leftExpander.consumerMessage`
   - Writes back to `leftExpander.producerMessage`
   - VCV Rack flips buffers after process()

3. **Producer Reads Response** (Tapestry):
   - Reads from `rightExpander.consumerMessage`

### Message Flow

```
Frame N:
  Tapestry.process()
    - Write audio to rightExpander.producerMessage
  [Buffer Flip]
  
  Expander.process()
    - Read audio from leftExpander.consumerMessage
    - Process effects
    - Write processed audio to leftExpander.producerMessage
  [Buffer Flip]

Frame N+1:
  Tapestry.process()
    - Read processed audio from rightExpander.consumerMessage
    - Output processed audio
```

### Latency

Total latency: **2 samples** (one frame in each direction)

---

## Example Usage

### Creating a Marker Programmatically

```cpp
// In your module's process() method
TapestryDSP& dsp = tapestryModule->dsp;
size_t totalFrames = dsp.getBuffer().getUsedFrames();
size_t position = totalFrames / 2;  // Middle of buffer

dsp.getSpliceManager().addSplice(position, totalFrames);
```

---

### Reading Current Grain Envelope

```cpp
float envelopeValue = tapestryModule->dsp.getEnvelopeOut();
// envelopeValue is 0.0 to 1.0
// Use for modulation, visualization, etc.
```

---

### Checking Expander Connection

```cpp
TapestryExpander* expander = /* ... */;
bool connected = expander->leftExpander.module &&
                 expander->leftExpander.module->model == modelTapestry;

lights[CONNECTED_LIGHT].setBrightness(connected ? 1.0f : 0.0f);
```

---

### Implementing Custom Grain Window

```cpp
// In GrainEngine::process()
float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * phase));  // Hann
// Replace with custom window function:
// float window = std::sin(M_PI * phase);  // Sine window
// float window = phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);  // Triangle
```

---

## Thread Safety

### Audio Thread (process())
- **Safe**: All DSP methods
- **Unsafe**: File I/O, JSON serialization

### UI Thread (widgets, menus)
- **Safe**: File I/O, parameter changes
- **Unsafe**: Direct DSP state modification

### Atomic Flags
```cpp
std::atomic<bool> fileLoading;  // Protects splice application
std::atomic<bool> fileSaving;   // Protects save operations
```

---

## Performance Considerations

### CPU Usage

Typical CPU usage (at 48kHz):
- Idle (no grains): ~1-2%
- 1 grain voice: ~3-5%
- 4 grain voices: ~8-12%
- With expander: +2-4%

### Memory Usage

- Audio buffer: ~32 MB (2.9 min stereo @ 32-bit float)
- Markers: ~2.4 KB (300 markers)
- Total module footprint: ~35 MB

### Optimization Tips

1. **Reduce grain voices**: Lower morph for fewer active voices
2. **Increase grain size**: Larger grains = fewer retriggerings
3. **Disable expander**: Remove if not using effects
4. **Use lower sample rates**: 44.1kHz vs 48kHz (minimal quality loss)

---

## See Also

- [Quick Start Guide](QUICKSTART.md) - Getting started tutorial
- [Advanced Usage](ADVANCED_USAGE.md) - Complex techniques
- [Examples](examples/) - Real-world patches
- [FAQ](FAQ.md) - Common questions

---

**Last Updated**: January 4, 2026  
**Version**: 2.0.0
