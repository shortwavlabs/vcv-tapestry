# Morphagene VCV Rack Implementation Plan

## Executive Summary

This document outlines a comprehensive technical implementation plan for creating a VCV Rack module that faithfully replicates the Make Noise Morphagene's functionality. The Morphagene is a next-generation tape and microsound music module that combines tape music tools (recording, splicing, Sound-On-Sound) with granular synthesis (Gene-Size, Slide, Morph).

**Target Module Name:** `Morphagene` (slug: `Morphagene`)  
**Panel Size:** 20HP (matching original hardware's visual density)  
**Sample Rate:** 48kHz internal, 32-bit float (matching hardware specs)  
**Maximum Reel Length:** 2.9 minutes (~8,352,000 stereo samples at 48kHz)  
**Maximum Splices per Reel:** 300

---

## Table of Contents

1. [Feature Mapping](#1-feature-mapping)
2. [Module Architecture](#2-module-architecture)
3. [DSP Algorithm Specifications](#3-dsp-algorithm-specifications)
4. [Parameter Mapping](#4-parameter-mapping)
5. [Audio Buffer Management](#5-audio-buffer-management)
6. [Code Reuse Analysis](#6-code-reuse-analysis)
7. [UI/UX Implementation](#7-uiux-implementation)
8. [Testing Methodology](#8-testing-methodology)
9. [Performance Optimization](#9-performance-optimization)
10. [Implementation Roadmap](#10-implementation-roadmap)

---

## 1. Feature Mapping

### 1.1 Core Functional Components

| Morphagene Feature | VCV Rack Implementation | Priority |
|-------------------|------------------------|----------|
| **Reels** (audio buffers up to 2.9 min) | `AudioBuffer` class with 32-bit float storage | P0 |
| **Splices** (up to 300 markers per reel) | `SpliceManager` with marker vector | P0 |
| **Genes** (granular particles) | `GrainEngine` with overlap/window | P0 |
| **Vari-Speed** (bipolar speed/direction) | Resampling engine with cubic interpolation | P0 |
| **Sound On Sound** (S.O.S.) | Crossfade mixer with record/playback blend | P0 |
| **Morph** (gene overlap/staggering) | Multi-grain voice manager | P1 |
| **Time Stretch/Compression** | Clock-synced granular playback | P1 |
| **Gene Shift** (clock-synced granulation) | Clock-triggered grain stepping | P1 |
| **Reel Management** (microSD simulation) | File I/O with .wav marker support | P2 |
| **Auto-Leveling** | Envelope follower with normalization | P2 |

### 1.2 Input/Output Mapping

#### Audio I/O
```
INPUTS:
- Audio In L (mono) [AC-coupled, line-to-modular level]
- Audio In R [AC-coupled]

OUTPUTS:
- Audio Out L (mono) [~10Vpp, AC-coupled]
- Audio Out R [~10Vpp, AC-coupled]
- CV Output [Envelope follower, 0-8V DC]
- EOSG Output [End of Splice/Gene gate, 0-10V]
```

#### Control Voltage Inputs
```
- S.O.S. CV IN (0-8V unipolar, normalized to +8V)
- Gene-Size CV IN (bipolar ±8V with attenuverter)
- Vari-Speed CV IN (bipolar ±4V with attenuverter)
- Morph CV IN (0-5V unipolar unity)
- Slide CV IN (0-8V unipolar with attenuverter)
- Organize CV IN (0-5V unipolar)
- CLK IN (clock/gate ≥2.5V)
- Play Gate IN (gate, normalized HIGH)
- REC Gate IN (clock/gate ≥2.5V)
- Splice Gate IN (gate ≥2.5V)
- Shift Gate IN (gate ≥2.5V)
```

### 1.3 Panel Controls

| Control | Type | Range | Default | Notes |
|---------|------|-------|---------|-------|
| S.O.S. | Combo pot | 0-1 | 1.0 | Crossfade: CCW=live, CW=loop |
| Gene-Size | Knob | 0-1 | 0.0 | CCW=full splice, CW=micro |
| Gene-Size CV Atten | Attenuverter | -1 to +1 | 0 | Bipolar CV attenuator |
| Vari-Speed | Bipolar knob | -1 to +1 | 0 | 12:00=stopped |
| Vari-Speed CV Atten | Attenuverter | -1 to +1 | 0 | Bipolar CV attenuator |
| Morph | Knob | 0-1 | ~0.3 | Gene overlap amount |
| Slide | Knob | 0-1 | 0 | Scrub through gene positions |
| Slide CV Atten | Attenuverter | -1 to +1 | 0 | Bipolar CV attenuator |
| Organize | Knob | 0-1 | 0 | Splice selection |

### 1.4 Button Functions

| Button | Single Press | Combo Actions |
|--------|--------------|---------------|
| **REC** | Record into current splice / Stop | +Shift: Auto-level, +Splice: Record new splice |
| **Splice** | Create splice marker | +Shift(hold): Delete marker, +Shift(3s): Delete all markers |
| **Shift** | Increment splice | +Splice+REC: Enter Reel mode |

---

## 2. Module Architecture

### 2.1 High-Level Class Structure

```
Morphagene (VCV Module)
├── MorphageneDSP
│   ├── AudioBuffer (Reel storage)
│   ├── SpliceManager (Marker management)
│   ├── GrainEngine (Granular synthesis)
│   ├── VariSpeedEngine (Pitch/speed control)
│   ├── RecordEngine (Recording + S.O.S.)
│   └── EnvelopeFollower (CV output)
├── MorphageneWidget (UI)
│   ├── ReelDisplay (Waveform visualization)
│   ├── ActivityWindows (LED indicators)
│   └── Panel controls/ports
└── FileManager (WAV I/O with markers)
```

### 2.2 Module State Machine

```
States:
- IDLE (playback, no recording)
- RECORDING_SAME (TLA - Time Lag Accumulation)
- RECORDING_NEW (Record into new splice)
- REEL_SELECT (Choosing reel from storage)
- SD_BUSY (Writing to storage - flash Shift LED)

Transitions:
- IDLE -> RECORDING_SAME: REC button press
- IDLE -> RECORDING_NEW: REC+Splice combo
- IDLE -> REEL_SELECT: Splice+REC combo
- RECORDING_* -> IDLE: REC button / gate / timeout (2.9 min)
- REEL_SELECT -> IDLE: Splice+REC combo / SD removal
```

### 2.3 Core Data Structures

```cpp
// src/dsp/morphagene-core.h

namespace ShortwavDSP {

// Audio buffer configuration
struct MorphageneConfig {
    static constexpr float kSampleRate = 48000.0f;
    static constexpr size_t kMaxReelSamples = 8352000; // ~2.9 min stereo
    static constexpr size_t kMaxSplices = 300;
    static constexpr size_t kMaxGrainVoices = 4;
    static constexpr float kMinGeneMs = 1.0f;     // ~48 samples
    static constexpr float kMaxGeneMs = 174000.0f; // Full reel
};

// Splice marker with position and metadata
struct SpliceMarker {
    size_t startSample;
    size_t endSample;
    bool isValid;
};

// Grain voice state for multi-grain Morph processing
struct GrainVoice {
    double position;      // Fractional sample position
    float amplitude;      // Envelope amplitude
    float pan;            // Stereo pan (-1 to +1)
    float pitchMod;       // Pitch randomization factor
    int phase;            // Envelope phase (attack/sustain/release)
    bool active;
};

// Playback state
struct PlaybackState {
    double playheadPosition;     // Current fractional sample
    int currentSplice;           // Active splice index
    int pendingSplice;           // Next splice (set by Organize)
    float variSpeed;             // Current speed/direction
    float geneSize;              // Current gene size in samples
    float morph;                 // Overlap/stagger amount
    float slide;                 // Position offset within splice
    bool isPlaying;
    bool isReversed;
};

} // namespace ShortwavDSP
```

---

## 3. DSP Algorithm Specifications

### 3.1 Granular Synthesis Engine

The Morphagene's granular engine uses overlapping "Genes" (grains) with dynamic enveloping.

#### Gene Window Function
```cpp
// Hann window for smooth grain transitions
inline float grainWindow(float phase) noexcept {
    // phase: 0.0 to 1.0 through grain
    return 0.5f * (1.0f - std::cos(2.0f * M_PI * phase));
}
```

#### Gene Size Calculation
```cpp
// Gene size is time-based, not sample-based
// This ensures temporal consistency across Vari-Speed changes
float calculateGeneSizeSamples(float geneSizeParam, float spliceLengthSamples) {
    // Full CCW (0.0) = full splice
    // Full CW (1.0) = minimum gene (~1ms = 48 samples at 48kHz)
    const float minGene = 48.0f;
    const float maxGene = spliceLengthSamples;
    
    // Exponential mapping for musical response
    float normalized = 1.0f - geneSizeParam; // Invert: 0=small, 1=full
    float exponent = 4.0f; // Adjust curve shape
    return minGene + std::pow(normalized, exponent) * (maxGene - minGene);
}
```

#### Morph Algorithm (Gene Overlap)
```cpp
// Morph parameter mapping:
// 0.0 (CCW): Gap between genes (pointillist effect)
// ~0.3 (9:00): Seamless loop (1/1 ratio)
// 0.5 (12:00): 2/1 overlap (2 voices)
// 0.7 (1:00): 3/1 overlap (3 voices, panning introduced)
// 1.0 (CW): 4 voices with pitch randomization + panning

struct MorphState {
    float overlap;        // 0.0 to 4.0 (voice count)
    int activeVoices;     // 1-4
    bool enablePanning;   // true when overlap > 2.5
    bool enablePitchRand; // true when overlap > 3.0
};

MorphState calculateMorphState(float morphParam) {
    MorphState state;
    
    // Map 0-1 parameter to overlap behavior
    if (morphParam < 0.15f) {
        // Gap mode
        state.overlap = -0.5f + morphParam * 3.33f; // -0.5 to 0.0
        state.activeVoices = 1;
    } else if (morphParam < 0.35f) {
        // Transition to seamless
        state.overlap = (morphParam - 0.15f) * 5.0f; // 0.0 to 1.0
        state.activeVoices = 1;
    } else if (morphParam < 0.65f) {
        // 1/1 to 2/1 overlap
        state.overlap = 1.0f + (morphParam - 0.35f) * 3.33f; // 1.0 to 2.0
        state.activeVoices = 2;
    } else if (morphParam < 0.85f) {
        // 2/1 to 3/1 overlap with panning
        state.overlap = 2.0f + (morphParam - 0.65f) * 5.0f; // 2.0 to 3.0
        state.activeVoices = 3;
        state.enablePanning = true;
    } else {
        // 3/1 to 4/1 with pitch randomization
        state.overlap = 3.0f + (morphParam - 0.85f) * 6.67f; // 3.0 to 4.0
        state.activeVoices = 4;
        state.enablePanning = true;
        state.enablePitchRand = true;
    }
    
    return state;
}
```

### 3.2 Vari-Speed Engine

The Vari-Speed control provides continuous speed and direction control.

```cpp
// Vari-Speed parameter mapping:
// -1.0 (full CCW): Maximum reverse speed (~26 semitones down)
// -0.5 (~9:30): Reverse at original speed (1x)
// 0.0 (12:00): Stopped
// +0.5 (~2:30): Forward at original speed (1x)
// +1.0 (full CW): Maximum forward speed (~12 semitones up)

struct VariSpeedState {
    float speedRatio;     // Playback speed multiplier
    bool isForward;       // Direction
    bool isAtUnity;       // True when at 1x speed (green LED)
    int octaveShift;      // -2 to +1 octaves
};

VariSpeedState calculateVariSpeed(float param, float cvInput, float cvAtten) {
    VariSpeedState state;
    
    // Combine param with CV
    float combined = param + (cvInput / 4.0f) * cvAtten; // ±4V range
    combined = clamp(combined, -1.0f, 1.0f);
    
    // Dead zone around center for clean stop
    const float deadZone = 0.02f;
    if (std::fabs(combined) < deadZone) {
        state.speedRatio = 0.0f;
        state.isForward = true;
        state.isAtUnity = false;
        return state;
    }
    
    state.isForward = combined > 0.0f;
    float absParam = std::fabs(combined);
    
    // Asymmetric range: more range slowing down than speeding up
    // Forward: 0 to +12 semitones
    // Reverse: 0 to -26 semitones (wider range)
    float semitones;
    if (state.isForward) {
        semitones = absParam * 12.0f; // 0 to +12 st
    } else {
        semitones = absParam * 26.0f; // 0 to -26 st (in reverse)
    }
    
    state.speedRatio = std::pow(2.0f, semitones / 12.0f);
    if (!state.isForward) {
        state.speedRatio = -state.speedRatio; // Negative for reverse
    }
    
    // Check for unity speed (±0.5 semitone tolerance)
    state.isAtUnity = std::fabs(semitones) < 0.5f;
    state.octaveShift = static_cast<int>(std::round(semitones / 12.0f));
    
    return state;
}
```

### 3.3 Resampling Algorithm

Use high-quality cubic interpolation matching the existing WavPlayer implementation.

```cpp
// From existing wav-player.h - can be reused directly
inline float cubicInterpolate(float y0, float y1, float y2, float y3, float t) noexcept {
    const float t2 = t * t;
    const float t3 = t2 * t;
    
    const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float a2 = -0.5f * y0 + 0.5f * y2;
    const float a3 = y1;
    
    return a0 * t3 + a1 * t2 + a2 * t + a3;
}

// Stereo sample fetch with interpolation
void fetchSampleStereo(const AudioBuffer& buffer, double position, 
                       float& outL, float& outR) {
    size_t idx = static_cast<size_t>(position);
    float frac = static_cast<float>(position - idx);
    
    // Get 4 sample frames for cubic interpolation
    size_t i0 = (idx > 0) ? idx - 1 : 0;
    size_t i1 = idx;
    size_t i2 = std::min(idx + 1, buffer.numFrames - 1);
    size_t i3 = std::min(idx + 2, buffer.numFrames - 1);
    
    // Left channel
    outL = cubicInterpolate(
        buffer.data[i0 * 2], buffer.data[i1 * 2],
        buffer.data[i2 * 2], buffer.data[i3 * 2], frac);
    
    // Right channel
    outR = cubicInterpolate(
        buffer.data[i0 * 2 + 1], buffer.data[i1 * 2 + 1],
        buffer.data[i2 * 2 + 1], buffer.data[i3 * 2 + 1], frac);
}
```

### 3.4 Time Stretch Algorithm

When CLK input is connected and Morph ≥ ~0.5 (2/1 overlap), enter Time Stretch mode.

```cpp
// Time stretch: clock controls playback rate, Vari-Speed controls pitch
struct TimeStretchEngine {
    float clockPeriodSamples;
    float lastClockTime;
    double grainPosition;
    float grainPhase;
    
    void onClockRising(float currentSampleTime) {
        if (lastClockTime > 0.0f) {
            clockPeriodSamples = currentSampleTime - lastClockTime;
        }
        lastClockTime = currentSampleTime;
        
        // Advance to next grain on clock
        grainPosition += geneSizeSamples;
        grainPhase = 0.0f;
    }
    
    void processSample(float variSpeed, float& positionOut, float& windowOut) {
        // Position advances based on Vari-Speed (pitch)
        // But new grains only trigger on clock (time)
        grainPhase += std::fabs(variSpeed) / geneSizeSamples;
        if (grainPhase >= 1.0f) {
            grainPhase = 1.0f; // Hold until next clock
        }
        
        positionOut = grainPosition + grainPhase * geneSizeSamples * variSpeed;
        windowOut = grainWindow(grainPhase);
    }
};
```

### 3.5 Sound-On-Sound (S.O.S.) Recording

```cpp
struct RecordEngine {
    enum Mode { IDLE, TLA, NEW_SPLICE };
    Mode mode = IDLE;
    size_t recordPosition;
    float sosParam; // 0=live only, 1=loop only
    
    void processRecord(AudioBuffer& buffer, float liveL, float liveR,
                       float playbackL, float playbackR) {
        if (mode == IDLE) return;
        
        // S.O.S. crossfade: mix live input with playback
        float mixL = liveL * (1.0f - sosParam) + playbackL * sosParam;
        float mixR = liveR * (1.0f - sosParam) + playbackR * sosParam;
        
        // Write to buffer
        buffer.data[recordPosition * 2] = mixL;
        buffer.data[recordPosition * 2 + 1] = mixR;
        
        // Advance record position
        recordPosition++;
        
        // Handle wrap/stop based on mode
        if (mode == TLA) {
            // Loop within current splice
            if (recordPosition >= currentSpliceEnd) {
                recordPosition = currentSpliceStart;
            }
        } else if (mode == NEW_SPLICE) {
            // Stop at max reel length
            if (recordPosition >= MorphageneConfig::kMaxReelSamples) {
                mode = IDLE;
            }
        }
    }
};
```

### 3.6 Envelope Follower (CV Output)

```cpp
// Based on existing patterns in the codebase
class CVEnvelopeFollower {
    float envelope = 0.0f;
    float attackCoeff;
    float releaseCoeff;
    
public:
    void setSampleRate(float sr) {
        // Fast attack (~1ms), slow release (~100ms)
        attackCoeff = 1.0f - std::exp(-1.0f / (sr * 0.001f));
        releaseCoeff = 1.0f - std::exp(-1.0f / (sr * 0.1f));
    }
    
    float process(float inputL, float inputR) {
        float inputPeak = std::max(std::fabs(inputL), std::fabs(inputR));
        
        if (inputPeak > envelope) {
            envelope += attackCoeff * (inputPeak - envelope);
        } else {
            envelope += releaseCoeff * (inputPeak - envelope);
        }
        
        // Scale to 0-8V range
        return envelope * 8.0f;
    }
};
```

---

## 4. Parameter Mapping

### 4.1 Param IDs Enumeration

```cpp
enum ParamIds {
    // Main controls
    SOS_PARAM,              // Sound On Sound (combo pot)
    GENE_SIZE_PARAM,        // Gene size knob
    GENE_SIZE_CV_ATTEN,     // Gene size CV attenuverter
    VARI_SPEED_PARAM,       // Vari-Speed bipolar knob
    VARI_SPEED_CV_ATTEN,    // Vari-Speed CV attenuverter
    MORPH_PARAM,            // Morph knob
    SLIDE_PARAM,            // Slide knob
    SLIDE_CV_ATTEN,         // Slide CV attenuverter
    ORGANIZE_PARAM,         // Organize knob
    
    // Buttons
    REC_BUTTON,
    SPLICE_BUTTON,
    SHIFT_BUTTON,
    
    NUM_PARAMS
};
```

### 4.2 Input IDs Enumeration

```cpp
enum InputIds {
    // Audio inputs
    AUDIO_IN_L,
    AUDIO_IN_R,
    
    // CV inputs
    SOS_CV_INPUT,
    GENE_SIZE_CV_INPUT,
    VARI_SPEED_CV_INPUT,
    MORPH_CV_INPUT,
    SLIDE_CV_INPUT,
    ORGANIZE_CV_INPUT,
    
    // Gate/Clock inputs
    CLK_INPUT,
    PLAY_INPUT,
    REC_INPUT,
    SPLICE_INPUT,
    SHIFT_INPUT,
    
    NUM_INPUTS
};
```

### 4.3 Output IDs Enumeration

```cpp
enum OutputIds {
    AUDIO_OUT_L,
    AUDIO_OUT_R,
    CV_OUTPUT,
    EOSG_OUTPUT,
    
    NUM_OUTPUTS
};
```

### 4.4 Light IDs Enumeration

```cpp
enum LightIds {
    // Activity windows (RGB LEDs)
    ENUMS(VARI_SPEED_LEFT_LIGHT, 3),   // Direction/Morph indicator
    ENUMS(VARI_SPEED_RIGHT_LIGHT, 3),  // Direction indicator
    ENUMS(REEL_LIGHT, 3),              // Reel color indicator
    ENUMS(SPLICE_LIGHT, 3),            // Splice color indicator
    ENUMS(CV_OUT_LIGHT, 3),            // CV level indicator
    
    // Button LEDs
    REC_LED,
    SPLICE_LED,
    SHIFT_LED,
    
    NUM_LIGHTS
};
```

### 4.5 CV Voltage Conventions

| Input | Range | Behavior |
|-------|-------|----------|
| S.O.S. CV | 0-8V | Unipolar, normalized to +8V when unpatched |
| Gene-Size CV | ±8V | Bipolar with attenuverter |
| Vari-Speed CV | ±4V | Bipolar with attenuverter |
| Morph CV | 0-5V | Unipolar, unity gain |
| Slide CV | 0-8V | Unipolar with attenuverter |
| Organize CV | 0-5V | Unipolar, smaller range for sequencer compat |
| CLK IN | ≥2.5V | Trigger on rising edge |
| Play IN | ≥2.5V | Gate HIGH = play, normalized HIGH |
| REC IN | ≥2.5V | Toggle record on trigger |
| Splice IN | ≥2.5V | Create marker on trigger |
| Shift IN | ≥2.5V | Increment splice on trigger |

---

## 5. Audio Buffer Management

### 5.1 Buffer Architecture

```cpp
class AudioBuffer {
public:
    static constexpr size_t kMaxFrames = 8352000; // 2.9 min @ 48kHz
    
    // Interleaved stereo data [L0, R0, L1, R1, ...]
    std::vector<float> data;
    size_t numFrames = 0;
    size_t usedFrames = 0;
    
    AudioBuffer() {
        // Pre-allocate maximum size for real-time safety
        data.resize(kMaxFrames * 2, 0.0f);
    }
    
    void clear() {
        std::fill(data.begin(), data.end(), 0.0f);
        usedFrames = 0;
    }
    
    // Non-allocating write for real-time safety
    bool writeStereo(size_t frame, float left, float right) {
        if (frame >= kMaxFrames) return false;
        data[frame * 2] = left;
        data[frame * 2 + 1] = right;
        usedFrames = std::max(usedFrames, frame + 1);
        return true;
    }
    
    // Interpolated read
    void readStereoInterpolated(double position, float& outL, float& outR) const;
};
```

### 5.2 Splice Manager

```cpp
class SpliceManager {
public:
    static constexpr size_t kMaxSplices = 300;
    
    struct Splice {
        size_t startFrame;
        size_t endFrame;
        
        size_t length() const { return endFrame - startFrame; }
    };
    
    std::vector<Splice> splices;
    int currentIndex = 0;
    int pendingIndex = -1; // Set by Organize, applied at end of current
    
    void addMarker(size_t framePosition) {
        if (splices.size() >= kMaxSplices) return;
        if (splices.empty()) {
            splices.push_back({0, framePosition});
        } else {
            // Insert marker at current position
            // Split current splice
            auto& current = splices[currentIndex];
            size_t oldEnd = current.endFrame;
            current.endFrame = framePosition;
            splices.insert(splices.begin() + currentIndex + 1, 
                          {framePosition, oldEnd});
        }
    }
    
    void deleteCurrentMarker() {
        if (splices.size() <= 1) return;
        // Merge with next splice
        size_t nextIdx = (currentIndex + 1) % splices.size();
        splices[currentIndex].endFrame = splices[nextIdx].endFrame;
        splices.erase(splices.begin() + nextIdx);
    }
    
    void deleteAllMarkers() {
        if (splices.empty()) return;
        size_t totalEnd = splices.back().endFrame;
        splices.clear();
        splices.push_back({0, totalEnd});
        currentIndex = 0;
    }
    
    // Called at end of splice/gene - check for pending change
    void onEndOfSplice() {
        if (pendingIndex >= 0 && pendingIndex < (int)splices.size()) {
            currentIndex = pendingIndex;
            pendingIndex = -1;
        }
    }
    
    // Map Organize parameter (0-1) to splice index
    int organizeToIndex(float param) const {
        if (splices.empty()) return 0;
        return static_cast<int>(param * (splices.size() - 1) + 0.5f);
    }
};
```

### 5.3 Memory Considerations

- **Pre-allocation:** Allocate full 2.9 minutes on module creation to avoid real-time allocations
- **Memory footprint:** ~64MB per reel (8.35M samples × 2 channels × 4 bytes)
- **Multiple reels:** Support 32 reels = ~2GB max (use file-backed storage, load one at a time)
- **Real-time safety:** No allocations in `process()` method

---

## 6. Code Reuse Analysis

### 6.1 Components to Reuse from Existing Codebase

| Component | Source File | Reuse Strategy |
|-----------|-------------|----------------|
| **Cubic interpolation** | `dsp/wav-player.h` | Direct reuse of `cubicInterpolate()` |
| **Sample format conversion** | `dsp/wav-player.h` | Reuse `int16ToFloat()`, etc. |
| **WAV file I/O** | `dsp/wav-player.h` | Extend with marker chunk support |
| **Slice management patterns** | `WavPlayer.cpp` | Adapt `SliceInfo` struct and methods |
| **Trigger processing** | `WavPlayer.cpp` | Reuse `dsp::SchmittTrigger` patterns |
| **Thread-safe file loading** | `WavPlayer.hpp` | Adapt async loading pattern |
| **Random LFO for drift** | `dsp/random-lfo.h` | Use for pitch randomization in Morph |
| **Envelope follower** | New, based on `drift.h` patterns | Adapt leaky integrator |

### 6.2 New Components Required

| Component | Description | Complexity |
|-----------|-------------|------------|
| **GrainEngine** | Multi-voice granular with overlap | High |
| **MorphProcessor** | Gene overlap and staggering logic | High |
| **TimeStretchEngine** | Clock-synced granular playback | Medium |
| **ReelManager** | File I/O with reel selection | Medium |
| **MarkerChunkParser** | WAV cue/marker reading/writing | Medium |
| **ActivityWindowDisplay** | RGB LED state machine | Low |
| **ButtonComboDetector** | Multi-button gesture detection | Low |

### 6.3 WAV Marker Format Extension

```cpp
// Extend existing WAV parser to handle CUE and LABL chunks
// Used by Reaper, Audacity, etc. for splice markers

struct CuePoint {
    uint32_t id;
    uint32_t position;     // Sample offset
    char dataChunkId[4];   // "data"
    uint32_t chunkStart;
    uint32_t blockStart;
    uint32_t sampleOffset;
};

struct LabelChunk {
    uint32_t cuePointId;
    std::string text;
};

WavError parseMarkers(FILE* file, std::vector<size_t>& markerPositions) {
    // Search for "cue " chunk after data chunk
    // Parse CUE points and associated LABL/LTXT chunks
    // Convert to frame positions
}

WavError writeMarkers(FILE* file, const std::vector<size_t>& markerPositions) {
    // Append CUE chunk with marker positions
    // Optionally add LABL chunks with names like "Splice 1", etc.
}
```

---

## 7. UI/UX Implementation

### 7.1 Panel Layout (20HP)

```
┌─────────────────────────────────────────┐
│ ○                    MORPHAGENE      ○  │  <- Screws
├─────────────────────────────────────────┤
│   [IN L]  [IN R]        [OUT L] [OUT R] │  <- Audio I/O
│                                         │
│   ┌─────┐                    ┌─────┐    │
│   │S.O.S│   [GENE]    [MORPH]│SLIDE│    │  <- Main knobs
│   │○    │     ○         ○    │    ○│    │
│   └──●──┘                    └──●──┘    │
│   [CV]      [CV]●     [CV]     [CV]●    │  <- CV inputs + atten
│                                         │
│   ┌─────────────────────────────────┐   │
│   │        ●VARI-SPEED●             │   │  <- Bipolar knob
│   │   ◐          ○          ◑       │   │     Activity windows
│   └─────────────────────────────────┘   │
│   [CV]●              ●[ORGANIZE]        │  <- CV inputs
│                                         │
│   ● REEL    ● SPLICE   ● CV OUT         │  <- Activity LEDs
│                                         │
│   [CLK] [PLAY] [REC●] [SPLICE●] [SHIFT●]│  <- Gate inputs + buttons
│   [REC] [SPLIC][SHIFT] [EOSG]  [CV]     │  <- I/O row
│                                         │
│ ○                                    ○  │  <- Screws
└─────────────────────────────────────────┘
```

### 7.2 LED Activity Windows

```cpp
// Activity window state machine for visual feedback
struct ActivityWindow {
    enum Color { RED, AMBER, GREEN, BABY_BLUE, PEACH, PINK, WHITE };
    
    Color getVariSpeedColor(float speed, bool isForward) {
        float semitones = std::log2(std::fabs(speed)) * 12.0f;
        
        if (speed == 0.0f) return RED;           // Stopped
        if (std::fabs(semitones) < 0.5f) return GREEN;      // Unity
        if (semitones > 11.5f) return BABY_BLUE;            // Octave up
        if (semitones < -11.5f) return PEACH;               // Octave down
        return AMBER;                                        // Other
    }
    
    Color getMorphColor(float morph, bool hasGaps) {
        if (hasGaps) return RED;                 // Gene gaps/overlaps
        if (morph > 0.28f && morph < 0.32f) return AMBER;  // 1/1 seamless
        return RED;
    }
    
    Color getReelColor(int reelIndex) {
        // 8-color cycle matching hardware
        static const Color cycle[] = {
            /* 0 */ Color(0, 0, 255),    // Blue
            /* 1 */ Color(0, 255, 0),    // Green
            /* 2 */ Color(128, 255, 0),  // Light green
            /* 3 */ Color(255, 255, 0),  // Yellow
            /* 4 */ Color(255, 128, 0),  // Orange
            /* 5 */ Color(255, 0, 0),    // Red
            /* 6 */ Color(255, 0, 128),  // Pink
            /* 7 */ Color(255, 255, 255) // White
        };
        return cycle[reelIndex % 8];
    }
};
```

### 7.3 Waveform Display

```cpp
// Reusable waveform display widget (adapt from WavPlayer)
struct ReelDisplay : OpaqueWidget {
    Morphagene* module;
    
    void draw(const DrawArgs& args) override {
        if (!module) return;
        
        const auto& buffer = module->dsp.audioBuffer;
        const auto& splices = module->dsp.spliceManager;
        
        // Draw waveform
        nvgBeginPath(args.vg);
        for (size_t i = 0; i < box.size.x; i++) {
            size_t sampleIdx = i * buffer.usedFrames / box.size.x;
            float sample = buffer.data[sampleIdx * 2]; // Left channel
            float y = box.size.y * 0.5f * (1.0f - sample);
            if (i == 0) nvgMoveTo(args.vg, i, y);
            else nvgLineTo(args.vg, i, y);
        }
        nvgStrokeColor(args.vg, nvgRGB(100, 200, 100));
        nvgStroke(args.vg);
        
        // Draw splice markers
        for (const auto& splice : splices.splices) {
            float x = (float)splice.startFrame / buffer.usedFrames * box.size.x;
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 0);
            nvgLineTo(args.vg, x, box.size.y);
            nvgStrokeColor(args.vg, nvgRGB(255, 200, 0));
            nvgStroke(args.vg);
        }
        
        // Draw playhead
        float playX = module->dsp.playbackState.playheadPosition / 
                      buffer.usedFrames * box.size.x;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, playX, 0);
        nvgLineTo(args.vg, playX, box.size.y);
        nvgStrokeColor(args.vg, nvgRGB(255, 50, 50));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
        
        // Draw current gene window
        // ...
    }
};
```

### 7.4 Context Menu

```cpp
void MorphageneWidget::appendContextMenu(Menu* menu) {
    Morphagene* module = dynamic_cast<Morphagene*>(this->module);
    if (!module) return;
    
    menu->addChild(new MenuEntry);
    menu->addChild(createMenuLabel("Morphagene"));
    
    // Load reel from file
    menu->addChild(createMenuItem("Load Reel...", "", [=]() {
        osdialog_filters* filters = osdialog_filters_parse("WAV:wav");
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
        if (path) {
            module->loadReelAsync(path);
            free(path);
        }
        osdialog_filters_free(filters);
    }));
    
    // Save reel to file
    menu->addChild(createMenuItem("Save Reel...", "", [=]() {
        osdialog_filters* filters = osdialog_filters_parse("WAV:wav");
        char* path = osdialog_file(OSDIALOG_SAVE, NULL, "morphagene_reel.wav", filters);
        if (path) {
            module->saveReelAsync(path);
            free(path);
        }
        osdialog_filters_free(filters);
    }));
    
    // Clear reel
    menu->addChild(createMenuItem("Clear Reel", "", [=]() {
        module->clearReel();
    }));
    
    // Reel info
    if (module->dsp.audioBuffer.usedFrames > 0) {
        float duration = (float)module->dsp.audioBuffer.usedFrames / 48000.0f;
        char info[64];
        snprintf(info, sizeof(info), "Duration: %.1fs, Splices: %zu",
                 duration, module->dsp.spliceManager.splices.size());
        menu->addChild(createMenuLabel(info));
    }
}
```

---

## 8. Testing Methodology

### 8.1 Unit Tests (DSP Components)

```cpp
// tests/test_morphagene_dsp.cpp

#include "catch2/catch.hpp"
#include "dsp/morphagene-core.h"

TEST_CASE("GeneSize calculation", "[morphagene][dsp]") {
    SECTION("Full CCW returns full splice length") {
        float result = calculateGeneSizeSamples(0.0f, 48000.0f);
        REQUIRE(result == Approx(48000.0f));
    }
    
    SECTION("Full CW returns minimum gene") {
        float result = calculateGeneSizeSamples(1.0f, 48000.0f);
        REQUIRE(result == Approx(48.0f)); // ~1ms at 48kHz
    }
    
    SECTION("Exponential curve provides musical response") {
        float mid = calculateGeneSizeSamples(0.5f, 48000.0f);
        // Should be closer to min than max due to exponential curve
        REQUIRE(mid < 24000.0f);
    }
}

TEST_CASE("Vari-Speed mapping", "[morphagene][dsp]") {
    SECTION("Center position is stopped") {
        auto state = calculateVariSpeed(0.0f, 0.0f, 0.0f);
        REQUIRE(state.speedRatio == Approx(0.0f));
    }
    
    SECTION("Unity speed positions") {
        auto fwd = calculateVariSpeed(0.5f, 0.0f, 0.0f);
        REQUIRE(std::fabs(fwd.speedRatio) == Approx(1.0f).margin(0.1f));
        REQUIRE(fwd.isAtUnity);
        
        auto rev = calculateVariSpeed(-0.5f, 0.0f, 0.0f);
        REQUIRE(std::fabs(rev.speedRatio) == Approx(1.0f).margin(0.1f));
        REQUIRE(rev.isAtUnity);
    }
    
    SECTION("CV modulation") {
        auto base = calculateVariSpeed(0.5f, 0.0f, 1.0f);
        auto modulated = calculateVariSpeed(0.5f, 2.0f, 1.0f);
        REQUIRE(modulated.speedRatio > base.speedRatio);
    }
}

TEST_CASE("Morph state calculation", "[morphagene][dsp]") {
    SECTION("Low morph has gaps") {
        auto state = calculateMorphState(0.1f);
        REQUIRE(state.overlap < 0.5f);
        REQUIRE(state.activeVoices == 1);
    }
    
    SECTION("Seamless at ~9:00") {
        auto state = calculateMorphState(0.3f);
        REQUIRE(state.overlap == Approx(1.0f).margin(0.2f));
    }
    
    SECTION("High morph has 4 voices with effects") {
        auto state = calculateMorphState(0.95f);
        REQUIRE(state.activeVoices == 4);
        REQUIRE(state.enablePanning);
        REQUIRE(state.enablePitchRand);
    }
}

TEST_CASE("Splice manager", "[morphagene][splice]") {
    SpliceManager mgr;
    
    SECTION("Initial state") {
        REQUIRE(mgr.splices.empty());
    }
    
    SECTION("Adding markers") {
        mgr.splices.push_back({0, 48000});
        mgr.addMarker(24000);
        REQUIRE(mgr.splices.size() == 2);
        REQUIRE(mgr.splices[0].endFrame == 24000);
        REQUIRE(mgr.splices[1].startFrame == 24000);
    }
    
    SECTION("Maximum splice limit") {
        for (int i = 0; i < 350; i++) {
            mgr.addMarker(i * 100);
        }
        REQUIRE(mgr.splices.size() <= SpliceManager::kMaxSplices);
    }
}
```

### 8.2 Integration Tests

```cpp
TEST_CASE("Morphagene full signal path", "[morphagene][integration]") {
    MorphageneDSP dsp;
    dsp.setSampleRate(48000.0f);
    
    SECTION("Record and playback") {
        // Record 1 second of sine wave
        for (int i = 0; i < 48000; i++) {
            float sample = std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
            dsp.setInput(sample, sample);
            dsp.setRecording(true);
            dsp.process();
        }
        dsp.setRecording(false);
        
        // Playback should match
        dsp.setPlaying(true);
        float outL, outR;
        for (int i = 0; i < 48000; i++) {
            dsp.process();
            dsp.getOutput(outL, outR);
            float expected = std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
            REQUIRE(outL == Approx(expected).margin(0.01f));
        }
    }
    
    SECTION("Vari-speed pitch shifting") {
        // Pre-load buffer with known content
        // Set vari-speed to 2x
        // Verify pitch is doubled
    }
    
    SECTION("Granular gene-size") {
        // Set gene-size to small value
        // Verify output is fragmented appropriately
    }
}
```

### 8.3 Performance Benchmarks

```cpp
TEST_CASE("Performance benchmarks", "[morphagene][performance]") {
    MorphageneDSP dsp;
    dsp.setSampleRate(48000.0f);
    
    // Pre-fill buffer
    for (size_t i = 0; i < 48000 * 30; i++) {
        dsp.audioBuffer.writeStereo(i, 0.5f, 0.5f);
    }
    
    BENCHMARK("Single sample processing") {
        float outL, outR;
        dsp.process();
        dsp.getOutput(outL, outR);
        return outL;
    };
    
    BENCHMARK("4-voice grain processing") {
        dsp.setMorph(0.95f); // Enable 4 voices
        float outL, outR;
        dsp.process();
        dsp.getOutput(outL, outR);
        return outL;
    };
    
    // Target: < 1% CPU at 48kHz
}
```

---

## 9. Performance Optimization

### 9.1 Real-Time Safety Checklist

- [ ] No allocations in `process()` method
- [ ] No mutex locks in audio path
- [ ] No file I/O in audio path
- [ ] Pre-allocated maximum buffer size
- [ ] Atomic operations for cross-thread state
- [ ] Lock-free parameter updates

### 9.2 SIMD Optimization Opportunities

```cpp
// Vectorized grain window calculation (4 grains at once)
#include <simd/vector.hpp>

void processGrainsVectorized(const GrainVoice* voices, int numVoices,
                             const AudioBuffer& buffer,
                             float* outL, float* outR) {
    // Use Rack's SIMD helpers for 4-wide processing
    using namespace rack::simd;
    
    if (numVoices == 4) {
        float4 positions = {voices[0].position, voices[1].position,
                           voices[2].position, voices[3].position};
        float4 phases = {voices[0].amplitude, voices[1].amplitude,
                        voices[2].amplitude, voices[3].amplitude};
        
        // Vectorized Hann window
        float4 windows = 0.5f * (1.0f - cos(2.0f * M_PI * phases));
        
        // Sum contributions
        float4 samplesL, samplesR;
        // ... fetch and accumulate
        
        *outL = horizontal_sum(samplesL * windows);
        *outR = horizontal_sum(samplesR * windows);
    }
}
```

### 9.3 Cache Optimization

```cpp
// Ensure audio buffer is cache-friendly
class AudioBuffer {
    // Align to cache line boundary
    alignas(64) std::vector<float> data;
    
    // Prefetch upcoming samples for playback
    void prefetchAhead(size_t currentFrame, size_t lookAhead = 256) {
        size_t prefetchFrame = currentFrame + lookAhead;
        if (prefetchFrame < usedFrames) {
            __builtin_prefetch(&data[prefetchFrame * 2], 0, 3);
        }
    }
};
```

### 9.4 Branch Reduction

```cpp
// Branchless clamp
inline float fastClamp(float x, float min, float max) {
    x = x < min ? min : x;
    x = x > max ? max : x;
    return x;
}

// Branchless sign
inline float fastSign(float x) {
    return (x > 0.0f) - (x < 0.0f);
}
```

---

## 10. Implementation Roadmap

### Phase 1: Core Infrastructure (2-3 weeks)
**Goal:** Basic recording and playback

1. **Week 1:**
   - [ ] Create `Morphagene.hpp` / `Morphagene.cpp` module skeleton
   - [ ] Implement `AudioBuffer` class with pre-allocation
   - [ ] Basic audio passthrough (input → output)
   - [ ] REC button functionality (record into buffer)

2. **Week 2:**
   - [ ] Implement `SpliceManager` class
   - [ ] Splice button for marker creation
   - [ ] Basic playback with loop
   - [ ] S.O.S. crossfade control

3. **Week 3:**
   - [ ] Vari-Speed implementation (speed/direction)
   - [ ] Cubic interpolation integration
   - [ ] Reverse playback support
   - [ ] Basic UI panel layout

### Phase 2: Granular Engine (2-3 weeks)
**Goal:** Gene-Size and Slide functionality

4. **Week 4:**
   - [ ] `GrainEngine` class implementation
   - [ ] Gene-Size parameter with exponential mapping
   - [ ] Hann windowing for grains
   - [ ] Single-voice granular playback

5. **Week 5:**
   - [ ] Slide parameter for position scrubbing
   - [ ] Gene position offsetting
   - [ ] EOSG (End of Splice/Gene) output
   - [ ] Play gate input (retrigger)

6. **Week 6:**
   - [ ] Multi-voice grain engine (Morph parameter)
   - [ ] Gene overlap algorithm
   - [ ] Panning for high Morph values
   - [ ] Pitch randomization for extreme Morph

### Phase 3: Clock Sync & Recording Modes (2 weeks)
**Goal:** Time stretch and advanced recording

7. **Week 7:**
   - [ ] CLK input implementation
   - [ ] Gene Shift mode (clock-stepped grains)
   - [ ] Time Stretch mode
   - [ ] Recording synchronization to clock

8. **Week 8:**
   - [ ] Time Lag Accumulation (TLA) recording
   - [ ] Record Into New Splice mode
   - [ ] Shift button/input for splice increment
   - [ ] Organize parameter with pending splice system

### Phase 4: File I/O & Polish (2 weeks)
**Goal:** Persistence and final features

9. **Week 9:**
   - [ ] WAV file loading with marker support
   - [ ] WAV file saving with marker export
   - [ ] Reel management (32 reels max)
   - [ ] Reel mode selection UI

10. **Week 10:**
    - [ ] Auto-leveling with envelope analysis
    - [ ] CV output (envelope follower)
    - [ ] Activity window LED logic
    - [ ] Button combination detection
    - [ ] Final UI polish and testing

### Phase 5: Optimization & Testing (1-2 weeks)

11. **Week 11-12:**
    - [ ] Performance profiling
    - [ ] SIMD optimization where beneficial
    - [ ] Comprehensive test suite
    - [ ] Documentation
    - [ ] Beta testing

---

## Appendix A: File Structure

```
src/
├── Morphagene.cpp          # Module process() and UI
├── Morphagene.hpp          # Module class definition
├── dsp/
│   ├── morphagene-core.h   # Core types and config
│   ├── morphagene-buffer.h # AudioBuffer class
│   ├── morphagene-splice.h # SpliceManager class
│   ├── morphagene-grain.h  # GrainEngine class
│   ├── morphagene-varispeed.h # VariSpeed processing
│   └── morphagene-record.h # RecordEngine class
├── tests/
│   └── test_morphagene.cpp # Unit tests
res/
└── Morphagene.svg          # Panel design (20HP)
docs/
└── Morphagene.md           # User manual
```

## Appendix B: Hardware Reference

| Specification | Value |
|--------------|-------|
| Sample Rate | 48,000 Hz |
| Bit Depth | 32-bit float |
| Channels | Stereo |
| Max Reel Duration | 2.9 minutes (174 seconds) |
| Max Splices | 300 per reel |
| Max Reels | 32 per SD card |
| Panel Size | 20HP |
| Power Draw | 165mA +12V, 20mA -12V |

## Appendix C: Color Codes

| LED State | RGB Value | Meaning |
|-----------|-----------|---------|
| Red | (255, 0, 0) | Stopped / Gene gaps |
| Amber | (255, 180, 0) | Seamless loop |
| Green | (0, 255, 0) | Unity speed |
| Baby Blue | (100, 200, 255) | Octave up |
| Peach | (255, 200, 150) | Octave down |
| Pink | (255, 100, 200) | New reel indicator |
| White | (255, 255, 255) | Last reel slot |

---

*Document Version: 1.0*  
*Created: December 2024*  
*For: Shortwav Labs VCV Rack Plugin*
