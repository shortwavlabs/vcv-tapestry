# Tapestry Expander Module - Comprehensive Implementation Plan

## Version 1.0 - December 2025

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architecture Overview](#2-architecture-overview)
3. [VCV Rack Expander System](#3-vcv-rack-expander-system)
4. [Signal Flow Design](#4-signal-flow-design)
5. [DSP Algorithms](#5-dsp-algorithms)
6. [Parameter Specifications](#6-parameter-specifications)
7. [User Interface Design](#7-user-interface-design)
8. [Implementation Details](#8-implementation-details)
9. [File Structure](#9-file-structure)
10. [Testing Strategy](#10-testing-strategy)
11. [Debugging Guide](#11-debugging-guide)
12. [Build Instructions](#12-build-instructions)

---

## 1. Executive Summary

### Project Overview

The **Tapestry Expander** is a companion module for the Tapestry granular processor that provides two distinct audio effects:

1. **Bit Crusher** - Sample rate reduction and bit depth quantization using the musicdsp.org decimator algorithm
2. **Moog VCF Low-Pass Filter** - 24dB/octave resonant lowpass filter using the Moog VCF Variation 2 algorithm

Each effect includes individual **Dry/Wet** mixing controls for precise blending between processed and unprocessed signals.

### Key Features

- Seamless integration with Tapestry via VCV Rack's expander message system
- Near-zero-latency audio processing (1-sample expander message latency)
- CV control for all parameters
- Stereo audio path maintained throughout
- 4HP panel width for minimal rack space

---

## 2. Architecture Overview

### Module Relationship

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SIGNAL FLOW DIAGRAM                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────────────────┐      ┌───────────────────────────────────────┐   │
│   │      TAPESTRY        │      │         TAPESTRY EXPANDER             │   │
│   │    (20HP Module)     │      │            (4HP Module)               │   │
│   │                      │      │                                       │   │
│   │  ┌────────────────┐  │      │  ┌─────────────────────────────────┐  │   │
│   │  │   Granular     │  │      │  │         BIT CRUSHER             │  │   │
│   │  │   Engine       │  │      │  │  ┌─────────┐  ┌─────────────┐   │  │   │
│   │  │                │  │      │  │  │ Sample  │  │   Bit       │   │  │   │
│   │  │  ┌──────────┐  │  │      │  │  │ Rate    │──│   Depth     │   │  │   │
│   │  │  │ audioOutL│──┼──┼──────┼──┼─▶│ Reduce  │  │   Reduce    │   │  │   │
│   │  │  │ audioOutR│──┼──┼──────┼──┼─▶│         │  │             │   │  │   │
│   │  │  └──────────┘  │  │ MSG  │  │  └────┬────┘  └──────┬──────┘   │  │   │
│   │  │                │  │ BUS  │  │       │              │          │  │   │
│   │  └────────────────┘  │      │  │       └──────┬───────┘          │  │   │
│   │                      │      │  │              ▼                  │  │   │
│   │  ┌────────────────┐  │      │  │  ┌───────────────────────────┐  │  │   │
│   │  │ OUTPUT JACKS   │◀─┼──────┼──┼──│     DRY/WET MIXER 1       │  │  │   │
│   │  │ (Final Output) │  │      │  │  └───────────┬───────────────┘  │  │   │
│   │  └────────────────┘  │      │  └──────────────┼──────────────────┘  │   │
│   │                      │      │                 ▼                     │   │
│   └──────────────────────┘      │  ┌─────────────────────────────────┐  │   │
│                                 │  │          MOOG VCF               │  │   │
│                                 │  │  ┌─────────┐  ┌─────────────┐   │  │   │
│                                 │  │  │ Cutoff  │  │  Resonance  │   │  │   │
│                                 │  │  │ Freq    │──│  (Q)        │   │  │   │
│                                 │  │  │         │  │             │   │  │   │
│                                 │  │  └────┬────┘  └──────┬──────┘   │  │   │
│                                 │  │       │              │          │  │   │
│                                 │  │       └──────┬───────┘          │  │   │
│                                 │  │              ▼                  │  │   │
│                                 │  │  ┌───────────────────────────┐  │  │   │
│                                 │  │  │     DRY/WET MIXER 2       │  │  │   │
│                                 │  │  └───────────┬───────────────┘  │  │   │
│                                 │  └──────────────┼──────────────────┘  │   │
│                                 │                 ▼                     │   │
│                                 │          ┌──────────────┐             │   │
│                                 │          │ FINAL OUTPUT │             │   │
│                                 │          │ (Back to     │             │   │
│                                 │          │  Tapestry)   │             │   │
│                                 │          └──────────────┘             │   │
│                                 └───────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Expander Placement

The expander must be placed **immediately to the right** of the Tapestry module to establish communication.

---

## 3. VCV Rack Expander System

### How VCV Rack Expanders Work

VCV Rack provides a built-in mechanism for modules to communicate with adjacent modules through the `Expander` struct. Each module has:

- `leftExpander` - Communication with the module to the left
- `rightExpander` - Communication with the module to the right

### Message Structure

```cpp
// Define the message structure for Tapestry-Expander communication
struct TapestryExpanderMessage {
    // Audio from Tapestry to Expander (pre-output)
    float audioL = 0.0f;
    float audioR = 0.0f;
    
    // Processed audio from Expander back to Tapestry
    float processedL = 0.0f;
    float processedR = 0.0f;
    
    // Sync flag indicating expander is connected and active
    bool expanderConnected = false;
    
    // Sample rate for DSP coefficient calculation
    float sampleRate = 48000.0f;
};
```

### Message Passing Protocol

1. **Tapestry Module (Producer)**:
   - Checks if right expander exists and is the correct model
   - Writes audio samples to `producerMessage`
   - Requests message flip at end of process()

2. **Expander Module (Consumer)**:
   - Checks if left expander is Tapestry
   - Reads from `consumerMessage`
   - Processes audio through effects chain
   - Writes processed audio back to producerMessage
   - Requests message flip

**Important note on timing:** VCV Rack expander messaging is double-buffered and flipped at the end of the engine timestep. This means messages have **1-sample latency**: data written this sample becomes visible to the other module on the next sample. Design your protocol so that each side reads only from `consumerMessage` and writes only to `producerMessage`.

### VCV Rack Expander API Reference

```cpp
// From engine/Module.hpp
struct Expander {
    int64_t moduleId = -1;        // ID of the expander module, or -1 if nonexistent
    Module* module = NULL;         // Pointer to the expander Module
    void* producerMessage = NULL;  // Write buffer (double-buffered)
    void* consumerMessage = NULL;  // Read buffer (double-buffered)
    bool messageFlipRequested = false;
    
    void requestMessageFlip() {
        messageFlipRequested = true;
    }
};
```

---

## 4. Signal Flow Design

### Processing Chain

```
Input → Bit Crusher → Dry/Wet Mix 1 → Moog VCF → Dry/Wet Mix 2 → Output
```

### Detailed Signal Path

```cpp
// Per-sample processing flow
void process(float inputL, float inputR) {
    // Stage 1: Bit Crusher
    float crushedL = bitCrusher.process(inputL);
    float crushedR = bitCrusher.process(inputR);
    
    // Stage 1 Dry/Wet Mix
    float stage1L = inputL * (1.0f - crushMix) + crushedL * crushMix;
    float stage1R = inputR * (1.0f - crushMix) + crushedR * crushMix;
    
    // Stage 2: Moog VCF
    float filteredL = moogFilterL.process(stage1L);
    float filteredR = moogFilterR.process(stage1R);
    
    // Stage 2 Dry/Wet Mix
    outputL = stage1L * (1.0f - filterMix) + filteredL * filterMix;
    outputR = stage1R * (1.0f - filterMix) + filteredR * filterMix;
}
```

### Stereo Considerations

- Bit Crusher: Uses shared state for sample-hold (preserves stereo image)
- Moog VCF: Independent left/right filter states (true stereo filtering)

---

## 5. DSP Algorithms

### 5.1 Bit Crusher (Decimator Algorithm)

**Source**: musicdsp.org - Decimator by tobybear@web.de + Lo-Fi Crusher by David Lowenfels

The bit crusher combines two effects:
1. **Sample Rate Reduction**: Sample-and-hold to reduce effective sample rate
2. **Bit Depth Reduction**: Quantize amplitude to fewer bits

#### Algorithm Implementation

```cpp
class BitCrusher {
public:
    BitCrusher() { reset(); }
    
    void reset() {
        holdL = 0.0f;
        holdR = 0.0f;
        counter = 0.0f;
    }
    
    void setParams(float bits, float rateReduction, float sampleRate) {
        // bits: 1.0 to 16.0 (fractional allowed for smooth control)
        // rateReduction: 0.0 (original rate) to 1.0 (heavily reduced)
        
        // Calculate quantization step size
        // Using pow for smooth fractional bit depths
        quantizationStep = 1.0f / std::pow(2.0f, bits);
        
        // Calculate rate reduction factor
        // At 0.0: advance by 1.0 each sample (no reduction)
        // At 1.0: advance by minimal amount (maximum reduction)
        float minRate = 0.01f;  // Minimum rate (1% of original)
        rateIncrement = 1.0f - rateReduction * (1.0f - minRate);
    }
    
    void processStereo(float inL, float inR, float& outL, float& outR) {
        counter += rateIncrement;
        
        if (counter >= 1.0f) {
            counter -= 1.0f;
            
            // Quantize to specified bit depth
            // Using floor + 0.5 for proper rounding
            holdL = quantizationStep * std::floor(inL / quantizationStep + 0.5f);
            holdR = quantizationStep * std::floor(inR / quantizationStep + 0.5f);
        }
        
        // Output held values (sample-and-hold)
        outL = holdL;
        outR = holdR;
    }
    
private:
    float holdL = 0.0f;
    float holdR = 0.0f;
    float counter = 0.0f;
    float rateIncrement = 1.0f;
    float quantizationStep = 1.0f / 65536.0f;  // Default 16-bit
};
```

#### Parameter Details

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Bits | 1.0 - 16.0 | 16.0 | Bit depth (fractional for smooth sweep) |
| Rate | 0.0 - 1.0 | 0.0 | Sample rate reduction (0=none, 1=max) |
| Mix | 0.0 - 1.0 | 0.5 | Dry/Wet mix |

### 5.2 Moog VCF Low-Pass Filter (Variation 2)

**Source**: musicdsp.org - Moog VCF Variation 2 by Stilson/Smith CCRMA, Timo Tossavainen

This is a 4-pole (24dB/octave) resonant lowpass filter that accurately models the Moog ladder filter topology.

#### Algorithm Implementation

```cpp
class MoogVCF {
public:
    MoogVCF() { reset(); }
    
    void reset() {
        in1 = in2 = in3 = in4 = 0.0f;
        out1 = out2 = out3 = out4 = 0.0f;
    }
    
    void setParams(float cutoffNorm, float resonance, float sampleRate) {
        // cutoffNorm: 0.0 to 1.0 (maps to 20Hz - 20kHz logarithmically)
        // resonance: 0.0 to 1.0 (maps to 0.0 - 4.0 internal)
        
        // Convert normalized cutoff to frequency
        float minFreq = 20.0f;
        float maxFreq = std::min(20000.0f, sampleRate * 0.45f);  // Stay below Nyquist
        float freq = minFreq * std::pow(maxFreq / minFreq, cutoffNorm);
        
        // Calculate filter coefficient
        // fc: nearly linear [0,1] -> [0, fs/2]
        fc = freq / (sampleRate * 0.5f);
        fc = std::min(fc, 0.99f);  // Clamp for stability
        
        f = fc * 1.16f;
        
        // Map resonance 0-1 to 0-4 (self-oscillation at 4)
        res = resonance * 4.0f;
        
        // Calculate feedback amount with frequency compensation
        fb = res * (1.0f - 0.15f * f * f);
    }
    
    float process(float input) {
        // Apply resonance feedback
        input -= out4 * fb;
        
        // Scale input (empirically tuned for stability)
        input *= 0.35013f * (f * f) * (f * f);
        
        // Four cascaded one-pole filters
        // Each pole: out = in + 0.3*prev_in + (1-f)*prev_out
        out1 = input + 0.3f * in1 + (1.0f - f) * out1;
        in1 = input;
        
        out2 = out1 + 0.3f * in2 + (1.0f - f) * out2;
        in2 = out1;
        
        out3 = out2 + 0.3f * in3 + (1.0f - f) * out3;
        in3 = out2;
        
        out4 = out3 + 0.3f * in4 + (1.0f - f) * out4;
        in4 = out3;
        
        // Soft clipping to prevent runaway at high resonance
        // Band-limited sigmoid approximation
        out4 = out4 - (out4 * out4 * out4) / 6.0f;
        
        return out4;
    }
    
private:
    float fc = 1.0f;   // Normalized cutoff
    float f = 1.16f;   // Internal frequency coefficient
    float res = 0.0f;  // Resonance
    float fb = 0.0f;   // Feedback amount
    
    // Filter state (4 poles)
    float in1, in2, in3, in4;
    float out1, out2, out3, out4;
};
```

#### Parameter Details

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Cutoff | 0.0 - 1.0 | 1.0 | Cutoff frequency (log scale: 20Hz - 20kHz) |
| Resonance | 0.0 - 1.0 | 0.0 | Resonance/Q (self-oscillation at ~0.95+) |
| Mix | 0.0 - 1.0 | 0.5 | Dry/Wet mix |

#### Frequency Response Characteristics

- **Slope**: 24dB/octave (4-pole)
- **Resonance Peak**: Up to +20dB at high Q settings
- **Self-Oscillation**: Occurs when resonance approaches 1.0
- **Characteristic**: Classic "squelchy" Moog sound with warm saturation

---

## 6. Parameter Specifications

### Complete Parameter List

```cpp
enum ParamIds {
    // Bit Crusher
    CRUSH_BITS_PARAM,      // Bit depth (1-16)
    CRUSH_RATE_PARAM,      // Sample rate reduction (0-1)
    CRUSH_MIX_PARAM,       // Dry/Wet mix (0-1)
    
    // Moog VCF
    FILTER_CUTOFF_PARAM,   // Cutoff frequency (0-1, log)
    FILTER_RESO_PARAM,     // Resonance (0-1)
    FILTER_MIX_PARAM,      // Dry/Wet mix (0-1)
    
    NUM_PARAMS
};

enum InputIds {
    // CV Inputs
    CRUSH_BITS_CV_INPUT,
    CRUSH_RATE_CV_INPUT,
    CRUSH_MIX_CV_INPUT,
    
    FILTER_CUTOFF_CV_INPUT,
    FILTER_RESO_CV_INPUT,
    FILTER_MIX_CV_INPUT,
    
    NUM_INPUTS
};

enum OutputIds {
    // Optional direct outputs (for debugging or alternative routing)
    AUDIO_OUT_L,
    AUDIO_OUT_R,
    
    NUM_OUTPUTS
};

enum LightIds {
    CONNECTED_LIGHT,       // Indicates expander is connected
    CRUSH_ACTIVE_LIGHT,    // Bit crusher activity
    FILTER_ACTIVE_LIGHT,   // Filter activity
    
    NUM_LIGHTS
};
```

### Parameter Configurations

```cpp
// In constructor
configParam(CRUSH_BITS_PARAM, 1.0f, 16.0f, 16.0f, "Bit Depth", " bits");
configParam(CRUSH_RATE_PARAM, 0.0f, 1.0f, 0.0f, "Rate Reduction", "%", 0.0f, 100.0f);
configParam(CRUSH_MIX_PARAM, 0.0f, 1.0f, 0.5f, "Crusher Dry/Wet", "%", 0.0f, 100.0f);

configParam(FILTER_CUTOFF_PARAM, 0.0f, 1.0f, 1.0f, "Filter Cutoff");
configParam(FILTER_RESO_PARAM, 0.0f, 1.0f, 0.0f, "Filter Resonance", "%", 0.0f, 100.0f);
configParam(FILTER_MIX_PARAM, 0.0f, 1.0f, 0.5f, "Filter Dry/Wet", "%", 0.0f, 100.0f);
```

### CV Input Scaling

All CV inputs follow VCV Rack conventions:
- **±5V** for bipolar modulation (centered at current knob position)
- **0-10V** for unipolar modulation (adds to current knob position)

```cpp
// CV processing example
float getCvModulatedParam(int paramId, int cvInputId, float cvScale = 0.1f) {
    float baseValue = params[paramId].getValue();
    
    if (inputs[cvInputId].isConnected()) {
        float cv = inputs[cvInputId].getVoltage();
        baseValue += cv * cvScale;  // 0.1 = 10V full range
    }
    
    return clamp(baseValue, 0.0f, 1.0f);
}
```

---

## 7. User Interface Design

### Panel Layout (4HP)

```
┌─────────────────────────────┐
│     TAPESTRY EXPANDER       │  <- Title
├─────────────────────────────┤
│         [●]                 │  <- Connection LED
│      CONNECTION             │
├─────────────────────────────┤
│      BIT CRUSHER            │
│    ┌─────┐ ┌─────┐         │
│    │BITS │ │RATE │         │  <- Knobs
│    └──┬──┘ └──┬──┘         │
│       ○       ○             │  <- CV inputs
│    ┌─────────────┐         │
│    │     MIX     │         │  <- Mix knob
│    └─────┬───────┘         │
│          ○                  │  <- CV input
├─────────────────────────────┤
│       MOOG VCF              │
│    ┌─────┐ ┌─────┐         │
│    │ CUT │ │ RES │         │  <- Knobs
│    └──┬──┘ └──┬──┘         │
│       ○       ○             │  <- CV inputs
│    ┌─────────────┐         │
│    │     MIX     │         │  <- Mix knob
│    └─────┬───────┘         │
│          ○                  │  <- CV input
├─────────────────────────────┤
│      OUT L    OUT R         │
│        ○        ○           │  <- Optional outputs
└─────────────────────────────┘
```

### Visual Design Guidelines

1. **Color Scheme**: Match Tapestry's aesthetic
2. **Knob Sizes**:
   - Main parameters: RoundBlackKnob (medium)
   - Mix controls: RoundSmallBlackKnob (small)
3. **Labels**: Clear, abbreviated text above each control
4. **LED Indicators**:
   - Green: Connected and active
   - Yellow: Signal present
   - Red: Clipping/overload warning

### SVG Panel Specifications

```
Width: 4HP (20.32mm, 60.96px at 3px/mm)
Height: 128.5mm (385.5px)
Background: Match Tapestry panel color (#2D2D2D suggested)
```

---

## 8. Implementation Details

### 8.1 Complete Expander Module Code

```cpp
// TapestryExpander.hpp
#pragma once

#include "plugin.hpp"
#include <cmath>

// Forward declaration
struct Tapestry;

//------------------------------------------------------------------------------
// Message Structure for Expander Communication
//------------------------------------------------------------------------------

struct TapestryExpanderMessage {
    float audioL = 0.0f;
    float audioR = 0.0f;
    float processedL = 0.0f;
    float processedR = 0.0f;
    bool expanderConnected = false;
    float sampleRate = 48000.0f;
};

//------------------------------------------------------------------------------
// Bit Crusher DSP
//------------------------------------------------------------------------------

class BitCrusherDSP {
public:
    void reset() {
        holdL_ = holdR_ = 0.0f;
        counter_ = 0.0f;
    }
    
    void setParams(float bits, float rateReduction) {
        bits_ = std::max(1.0f, std::min(16.0f, bits));
        quantStep_ = 1.0f / std::pow(2.0f, bits_ - 1.0f);
        
        float minRate = 0.01f;
        rateIncr_ = 1.0f - rateReduction * (1.0f - minRate);
    }
    
    void processStereo(float inL, float inR, float& outL, float& outR) {
        counter_ += rateIncr_;
        
        if (counter_ >= 1.0f) {
            counter_ -= 1.0f;
            holdL_ = quantStep_ * std::floor(inL / quantStep_ + 0.5f);
            holdR_ = quantStep_ * std::floor(inR / quantStep_ + 0.5f);
        }
        
        outL = holdL_;
        outR = holdR_;
    }
    
private:
    float holdL_ = 0.0f, holdR_ = 0.0f;
    float counter_ = 0.0f;
    float bits_ = 16.0f;
    float quantStep_ = 1.0f / 32768.0f;
    float rateIncr_ = 1.0f;
};

//------------------------------------------------------------------------------
// Moog VCF DSP (Variation 2)
//------------------------------------------------------------------------------

class MoogVCFDSP {
public:
    void reset() {
        in1_ = in2_ = in3_ = in4_ = 0.0f;
        out1_ = out2_ = out3_ = out4_ = 0.0f;
    }
    
    void setParams(float cutoffNorm, float resonance, float sampleRate) {
        // Logarithmic frequency mapping
        float minFreq = 20.0f;
        float maxFreq = std::min(20000.0f, sampleRate * 0.45f);
        float freq = minFreq * std::pow(maxFreq / minFreq, cutoffNorm);
        
        fc_ = std::min(freq / (sampleRate * 0.5f), 0.99f);
        f_ = fc_ * 1.16f;
        res_ = resonance * 4.0f;
        fb_ = res_ * (1.0f - 0.15f * f_ * f_);
    }
    
    float process(float input) {
        // Resonance feedback
        input -= out4_ * fb_;
        
        // Input scaling for stability
        input *= 0.35013f * (f_ * f_) * (f_ * f_);
        
        // 4-pole cascade
        out1_ = input + 0.3f * in1_ + (1.0f - f_) * out1_;
        in1_ = input;
        
        out2_ = out1_ + 0.3f * in2_ + (1.0f - f_) * out2_;
        in2_ = out1_;
        
        out3_ = out2_ + 0.3f * in3_ + (1.0f - f_) * out3_;
        in3_ = out2_;
        
        out4_ = out3_ + 0.3f * in4_ + (1.0f - f_) * out4_;
        in4_ = out3_;
        
        // Soft saturation
        out4_ = out4_ - (out4_ * out4_ * out4_) / 6.0f;
        
        return out4_;
    }
    
private:
    float fc_ = 1.0f, f_ = 1.16f;
    float res_ = 0.0f, fb_ = 0.0f;
    float in1_, in2_, in3_, in4_;
    float out1_, out2_, out3_, out4_;
};

//------------------------------------------------------------------------------
// Tapestry Expander Module
//------------------------------------------------------------------------------

struct TapestryExpander : Module {
    enum ParamIds {
        CRUSH_BITS_PARAM,
        CRUSH_RATE_PARAM,
        CRUSH_MIX_PARAM,
        FILTER_CUTOFF_PARAM,
        FILTER_RESO_PARAM,
        FILTER_MIX_PARAM,
        NUM_PARAMS
    };
    
    enum InputIds {
        CRUSH_BITS_CV_INPUT,
        CRUSH_RATE_CV_INPUT,
        CRUSH_MIX_CV_INPUT,
        FILTER_CUTOFF_CV_INPUT,
        FILTER_RESO_CV_INPUT,
        FILTER_MIX_CV_INPUT,
        NUM_INPUTS
    };
    
    enum OutputIds {
        AUDIO_OUT_L,
        AUDIO_OUT_R,
        NUM_OUTPUTS
    };
    
    enum LightIds {
        CONNECTED_LIGHT,
        NUM_LIGHTS
    };
    
    // DSP processors
    BitCrusherDSP bitCrusherL_, bitCrusherR_;
    MoogVCFDSP moogFilterL_, moogFilterR_;
    
    // Sample rate
    float sampleRate_ = 48000.0f;
    
    TapestryExpander() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        // Bit Crusher params
        configParam(CRUSH_BITS_PARAM, 1.0f, 16.0f, 16.0f, "Bit Depth", " bits");
        configParam(CRUSH_RATE_PARAM, 0.0f, 1.0f, 0.0f, "Rate Reduction", "%", 0.0f, 100.0f);
        configParam(CRUSH_MIX_PARAM, 0.0f, 1.0f, 0.5f, "Crusher Mix", "%", 0.0f, 100.0f);
        
        // Moog VCF params
        configParam(FILTER_CUTOFF_PARAM, 0.0f, 1.0f, 1.0f, "Filter Cutoff");
        configParam(FILTER_RESO_PARAM, 0.0f, 1.0f, 0.0f, "Resonance", "%", 0.0f, 100.0f);
        configParam(FILTER_MIX_PARAM, 0.0f, 1.0f, 0.5f, "Filter Mix", "%", 0.0f, 100.0f);
        
        // CV inputs
        configInput(CRUSH_BITS_CV_INPUT, "Bits CV");
        configInput(CRUSH_RATE_CV_INPUT, "Rate CV");
        configInput(CRUSH_MIX_CV_INPUT, "Crusher Mix CV");
        configInput(FILTER_CUTOFF_CV_INPUT, "Cutoff CV");
        configInput(FILTER_RESO_CV_INPUT, "Resonance CV");
        configInput(FILTER_MIX_CV_INPUT, "Filter Mix CV");
        
        // Outputs
        configOutput(AUDIO_OUT_L, "Audio L");
        configOutput(AUDIO_OUT_R, "Audio R");
        
        // Allocate message buffers for expander communication
        leftExpander.producerMessage = new TapestryExpanderMessage;
        leftExpander.consumerMessage = new TapestryExpanderMessage;
    }
    
    ~TapestryExpander() {
        delete static_cast<TapestryExpanderMessage*>(leftExpander.producerMessage);
        delete static_cast<TapestryExpanderMessage*>(leftExpander.consumerMessage);
    }
    
    void onSampleRateChange() override {
        sampleRate_ = APP->engine->getSampleRate();
        reset();
    }
    
    void onReset() override {
        bitCrusherL_.reset();
        bitCrusherR_.reset();
        moogFilterL_.reset();
        moogFilterR_.reset();
    }
    
    void process(const ProcessArgs& args) override {
        // Check for Tapestry connection
        bool connected = false;
        float inputL = 0.0f;
        float inputR = 0.0f;
        
        // Check if left module is Tapestry
        if (leftExpander.module && 
            leftExpander.module->model == modelTapestry) {
            
            connected = true;
            
            // Read from consumer message (what Tapestry wrote last frame)
            TapestryExpanderMessage* msg = 
                static_cast<TapestryExpanderMessage*>(leftExpander.consumerMessage);
            
            inputL = msg->audioL;
            inputR = msg->audioR;
            sampleRate_ = msg->sampleRate;
        }
        
        // Update connection LED
        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.0f : 0.0f);
        
        if (!connected) {
            // No connection - output silence
            outputs[AUDIO_OUT_L].setVoltage(0.0f);
            outputs[AUDIO_OUT_R].setVoltage(0.0f);
            return;
        }
        
        // Read parameters with CV modulation
        float bits = getModulatedParam(CRUSH_BITS_PARAM, CRUSH_BITS_CV_INPUT, 1.5f);
        float rate = getModulatedParam(CRUSH_RATE_PARAM, CRUSH_RATE_CV_INPUT, 0.1f);
        float crushMix = getModulatedParam(CRUSH_MIX_PARAM, CRUSH_MIX_CV_INPUT, 0.1f);
        
        float cutoff = getModulatedParam(FILTER_CUTOFF_PARAM, FILTER_CUTOFF_CV_INPUT, 0.1f);
        float reso = getModulatedParam(FILTER_RESO_PARAM, FILTER_RESO_CV_INPUT, 0.1f);
        float filterMix = getModulatedParam(FILTER_MIX_PARAM, FILTER_MIX_CV_INPUT, 0.1f);
        
        // Clamp parameters
        bits = clamp(bits, 1.0f, 16.0f);
        rate = clamp(rate, 0.0f, 1.0f);
        crushMix = clamp(crushMix, 0.0f, 1.0f);
        cutoff = clamp(cutoff, 0.0f, 1.0f);
        reso = clamp(reso, 0.0f, 1.0f);
        filterMix = clamp(filterMix, 0.0f, 1.0f);
        
        // Update DSP parameters
        bitCrusherL_.setParams(bits, rate);
        bitCrusherR_.setParams(bits, rate);
        moogFilterL_.setParams(cutoff, reso, sampleRate_);
        moogFilterR_.setParams(cutoff, reso, sampleRate_);
        
        // Process: Bit Crusher (shared state stereo)
        float crushedL, crushedR;
        bitCrusherL_.processStereo(inputL, inputR, crushedL, crushedR);
        
        // Stage 1 mix
        float stage1L = inputL * (1.0f - crushMix) + crushedL * crushMix;
        float stage1R = inputR * (1.0f - crushMix) + crushedR * crushMix;
        
        // Process: Moog VCF
        float filteredL = moogFilterL_.process(stage1L);
        float filteredR = moogFilterR_.process(stage1R);
        
        // Stage 2 mix
        float outputL = stage1L * (1.0f - filterMix) + filteredL * filterMix;
        float outputR = stage1R * (1.0f - filterMix) + filteredR * filterMix;
        
        // Write processed audio back to producer message
        TapestryExpanderMessage* outMsg = 
            static_cast<TapestryExpanderMessage*>(leftExpander.producerMessage);
        outMsg->processedL = outputL;
        outMsg->processedR = outputR;
        outMsg->expanderConnected = true;
        
        // Request message flip
        leftExpander.requestMessageFlip();
        
        // Also set direct outputs (optional)
        outputs[AUDIO_OUT_L].setVoltage(outputL * 5.0f);
        outputs[AUDIO_OUT_R].setVoltage(outputR * 5.0f);
    }
    
private:
    float getModulatedParam(int paramId, int cvId, float cvScale) {
        float value = params[paramId].getValue();
        if (inputs[cvId].isConnected()) {
            value += inputs[cvId].getVoltage() * cvScale;
        }
        return value;
    }
};
```

### 8.2 Modifications to Tapestry Module

The Tapestry module needs to be modified to:
1. Check for expander presence
2. Send audio to expander before output
3. Receive processed audio from expander
4. Use processed audio for final output if expander is connected

```cpp
// In Tapestry.hpp - Add these lines:

// Add to includes
#include "TapestryExpander.hpp"

// In Tapestry class constructor, add:
rightExpander.producerMessage = new TapestryExpanderMessage;
rightExpander.consumerMessage = new TapestryExpanderMessage;

// In destructor:
delete static_cast<TapestryExpanderMessage*>(rightExpander.producerMessage);
delete static_cast<TapestryExpanderMessage*>(rightExpander.consumerMessage);

// In Tapestry.cpp process() function, modify the output section:

void Tapestry::process(const ProcessArgs& args)
{
    // ... existing code ...
    
    // After calculating result.audioOutL and result.audioOutR
    
    float finalOutL = result.audioOutL;
    float finalOutR = result.audioOutR;
    
    // Check for expander
    if (rightExpander.module && 
        rightExpander.module->model == modelTapestryExpander) {
        
        // Send audio to expander
        TapestryExpanderMessage* toExpander = 
            static_cast<TapestryExpanderMessage*>(rightExpander.producerMessage);
        toExpander->audioL = result.audioOutL;
        toExpander->audioR = result.audioOutR;
        toExpander->sampleRate = args.sampleRate;
        
        rightExpander.requestMessageFlip();
        
        // Read processed audio from expander (from last frame)
        TapestryExpanderMessage* fromExpander = 
            static_cast<TapestryExpanderMessage*>(rightExpander.consumerMessage);
        
        if (fromExpander->expanderConnected) {
            finalOutL = fromExpander->processedL;
            finalOutR = fromExpander->processedR;
        }
    }
    
    // Write final outputs
    outputs[AUDIO_OUT_L].setVoltage(finalOutL * 5.0f);
    outputs[AUDIO_OUT_R].setVoltage(finalOutR * 5.0f);
    
    // ... rest of existing code ...
}
```

---

## 9. File Structure

### New Files to Create

```
src/
├── TapestryExpander.hpp       # Expander module header with DSP classes
├── TapestryExpander.cpp       # Expander module implementation and widget
└── dsp/
    ├── bitcrusher.h           # Standalone bit crusher DSP (optional)
    └── moogvcf.h              # Standalone Moog VCF DSP (optional)

res/
└── TapestryExpander.svg       # Panel design
```

### Modified Files

```
src/
├── plugin.cpp                 # Register expander model
├── plugin.hpp                 # Declare expander model
└── Tapestry.cpp               # Add expander communication
└── Tapestry.hpp               # Add message buffer members
```

### plugin.json Update

```json
{
  "slug": "tapestry",
  "name": "Tapestry",
  "version": "2.1.0",
  "license": "GPL-3.0-or-later",
  "brand": "Shortwav Labs",
  "author": "Stephane Pericat",
  "authorEmail": "contact@shortwavlabs.com",
  "modules": [
    {
      "slug": "Tapestry",
      "name": "Tapestry",
      "description": "Granular microsound processor inspired by musique concrète",
      "tags": ["Sampler", "Granular", "Recording"]
    },
    {
      "slug": "TapestryExpander",
      "name": "Tapestry Expander",
      "description": "Effects expander with bit crusher and Moog VCF filter",
      "tags": ["Effect", "Filter", "Distortion", "Expander"]
    }
  ]
}
```

---

## 10. Testing Strategy

### Unit Tests

```cpp
// tests/test_expander.cpp

#include <gtest/gtest.h>
#include "../src/TapestryExpander.hpp"

class BitCrusherTest : public ::testing::Test {
protected:
    BitCrusherDSP crusher;
    
    void SetUp() override {
        crusher.reset();
    }
};

TEST_F(BitCrusherTest, FullBitDepthPassthrough) {
    crusher.setParams(16.0f, 0.0f);
    
    float inL = 0.5f, inR = -0.5f;
    float outL, outR;
    
    crusher.processStereo(inL, inR, outL, outR);
    
    // At 16-bit, quantization should be negligible
    EXPECT_NEAR(outL, inL, 0.0001f);
    EXPECT_NEAR(outR, inR, 0.0001f);
}

TEST_F(BitCrusherTest, OneBitQuantization) {
    crusher.setParams(1.0f, 0.0f);
    
    float inL = 0.3f, inR = -0.3f;
    float outL, outR;
    
    crusher.processStereo(inL, inR, outL, outR);
    
    // At 1-bit, should quantize to -1, 0, or 1
    EXPECT_TRUE(outL == 0.0f || outL == 1.0f || outL == -1.0f);
}

class MoogVCFTest : public ::testing::Test {
protected:
    MoogVCFDSP filter;
    
    void SetUp() override {
        filter.reset();
    }
};

TEST_F(MoogVCFTest, FullCutoffPassthrough) {
    filter.setParams(1.0f, 0.0f, 48000.0f);
    
    // With full cutoff and no resonance, should pass signal mostly unchanged
    float input = 0.5f;
    float output = filter.process(input);
    
    // Allow some tolerance due to filter dynamics
    EXPECT_NEAR(output, input * 0.35f, 0.1f); // Scaled by input gain
}

TEST_F(MoogVCFTest, ZeroCutoffAttenuation) {
    filter.setParams(0.0f, 0.0f, 48000.0f);
    
    // Process several samples to let filter settle
    for (int i = 0; i < 1000; i++) {
        filter.process(1.0f);
    }
    
    float output = filter.process(1.0f);
    
    // With very low cutoff, output should be very small
    EXPECT_LT(std::abs(output), 0.01f);
}
```

### Integration Tests

1. **Expander Detection Test**
   - Place expander to right of Tapestry
   - Verify connection LED lights up
   - Verify audio passes through

2. **Effect Bypass Test**
   - Set both mix knobs to 0%
   - Verify output matches input

3. **Full Wet Test**
   - Set both mix knobs to 100%
   - Verify effects are audible

4. **CV Modulation Test**
   - Connect LFO to each CV input
   - Verify parameter sweeps correctly

### Audio Quality Tests

1. **DC Offset Test**: Verify no DC offset introduced
2. **Noise Floor Test**: Measure SNR with signal and without
3. **Aliasing Test**: Sweep bit crusher rate, check for aliasing
4. **Self-Oscillation Test**: Set resonance to max, verify stable oscillation

---

## 11. Debugging Guide

### Common Issues

#### Issue: Expander Not Detected

**Symptoms**: Connection LED stays off

**Debugging Steps**:
1. Verify expander is immediately to the right of Tapestry
2. Check model comparison in process():
   ```cpp
   DEBUG("Left module: %p, Model: %s", 
         leftExpander.module, 
         leftExpander.module ? leftExpander.module->model->slug.c_str() : "null");
   ```
3. Ensure both modules are registered correctly in plugin.cpp

#### Issue: No Audio Through Expander

**Symptoms**: Connected but silent

**Debugging Steps**:
1. Add debug logging in message passing:
   ```cpp
   DEBUG("Audio to expander: L=%.4f R=%.4f", msg->audioL, msg->audioR);
   ```
2. Check message buffer allocation in constructors
3. Verify requestMessageFlip() is called

#### Issue: Filter Instability

**Symptoms**: Output explodes or oscillates wildly

**Debugging Steps**:
1. Add clipping before output:
   ```cpp
   output = clamp(output, -10.0f, 10.0f);
   ```
2. Reduce maximum resonance:
   ```cpp
   res_ = resonance * 3.5f;  // Instead of 4.0
   ```
3. Add denormal protection:
   ```cpp
   if (std::abs(out4_) < 1e-10f) out4_ = 0.0f;
   ```

#### Issue: Bit Crusher Artifacts

**Symptoms**: Clicks or pops during parameter changes

**Debugging Steps**:
1. Add parameter smoothing:
   ```cpp
   smoothedBits += 0.001f * (targetBits - smoothedBits);
   ```
2. Ensure counter doesn't overflow:
   ```cpp
   counter_ = fmod(counter_, 2.0f);
   ```

### Performance Profiling

```cpp
// Add to process() for timing
auto start = std::chrono::high_resolution_clock::now();

// ... processing code ...

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

if (frame % 48000 == 0) {  // Log once per second
    DEBUG("Process time: %ld ns", duration.count());
}
```

---

## 12. Build Instructions

### Prerequisites

- VCV Rack SDK 2.x
- C++17 compatible compiler
- CMake 3.16+ (optional, for testing)

### Building the Plugin

```bash
# Navigate to plugin directory
cd /path/to/tapestry

# Clean previous build (optional)
make clean

# Build plugin
make

# Install to Rack plugins folder
make install
```

### Development Build with Debug Symbols

```bash
# Build with debug info
RACK_DIR=dep/Rack-SDK make DEBUG=1

# Run with Rack's debug mode
/path/to/Rack -d
```

### Testing Build

```bash
# Build and run tests
make test

# Or manually
cd build
cmake ..
make
./test_tapestry_expander
```

### Release Build

```bash
# Clean build for distribution
make clean
make dist

# Creates tapestry-2.1.0-platform.vcvplugin
```

---

## Appendix A: Algorithm References

### Bit Crusher / Decimator

- **Primary Source**: musicdsp.org "Decimator" by tobybear@web.de
- **Enhanced Version**: musicdsp.org "Lo-Fi Crusher" by David Lowenfels
- **Key Insight**: Fractional bit depth allows smooth parameter sweeping

### Moog VCF Variation 2

- **Primary Source**: musicdsp.org "Moog VCF, variation 2" by Stilson/Smith CCRMA
- **Original Paper**: "Analyzing the Moog VCF with Considerations for Digital Implementation" - Tim Stilson, Julius Smith
- **Key Features**:
  - 4-pole ladder topology
  - Resonance compensation
  - Built-in soft saturation for stability

---

## Appendix B: Expander Message Timing

```
Frame N:
┌───────────────────────────────────────────────────────────────┐
│ Tapestry.process()                                            │
│  1. Calculate audio output                                    │
│  2. Write to rightExpander.producerMessage                    │
│  3. Call rightExpander.requestMessageFlip()                   │
│  4. Read from rightExpander.consumerMessage (frame N-1 data)  │
│  5. Output to jacks                                           │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌───────────────────────────────────────────────────────────────┐
│ TapestryExpander.process()                                    │
│  1. Read from leftExpander.consumerMessage                    │
│  2. Process through effects                                   │
│  3. Write to leftExpander.producerMessage                     │
│  4. Call leftExpander.requestMessageFlip()                    │
│  5. Output to optional direct jacks                           │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌───────────────────────────────────────────────────────────────┐
│ End of Frame N: VCV Rack swaps message buffers                │
│  - producerMessage ↔ consumerMessage                          │
└───────────────────────────────────────────────────────────────┘

Note: 1-sample latency is introduced by the double-buffering mechanism.
This is imperceptible at audio rates (20.8µs at 48kHz).
```

---

## Appendix C: Parameter Smoothing Recommendations

For production use, consider adding parameter smoothing to prevent zipper noise:

```cpp
class SmoothParam {
public:
    void setTarget(float target) { target_ = target; }
    
    float process() {
        current_ += smoothCoeff_ * (target_ - current_);
        return current_;
    }
    
    void setSmoothTime(float timeMs, float sampleRate) {
        smoothCoeff_ = 1.0f - std::exp(-1.0f / (sampleRate * timeMs * 0.001f));
    }
    
private:
    float current_ = 0.0f;
    float target_ = 0.0f;
    float smoothCoeff_ = 0.001f;
};
```

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | December 2025 | Initial implementation plan |

---

*This document is part of the Tapestry plugin for VCV Rack by Shortwav Labs.*
