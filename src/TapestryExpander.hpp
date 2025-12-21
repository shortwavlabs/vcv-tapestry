#pragma once

#include "plugin.hpp"
#include <cmath>

/*
 * Tapestry Expander Module
 *
 * A companion module for the Tapestry granular processor that provides
 * two distinct audio effects:
 * 1. Bit Crusher - Sample rate reduction and bit depth quantization
 * 2. Moog VCF Low-Pass Filter - 24dB/octave resonant lowpass filter
 *
 * Each effect includes individual Dry/Wet mixing controls.
 */

//------------------------------------------------------------------------------
// Message Structure for Expander Communication
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// Bit Crusher DSP
// Based on musicdsp.org Decimator by tobybear + Lo-Fi Crusher by David Lowenfels
//------------------------------------------------------------------------------

class BitCrusherDSP {
public:
    BitCrusherDSP() { reset(); }
    
    void reset() {
        holdL_ = 0.0f;
        holdR_ = 0.0f;
        counter_ = 0.0f;
    }
    
    void setParams(float bits, float rateReduction) {
        // bits: 1.0 to 16.0 (fractional allowed for smooth control)
        bits_ = std::max(1.0f, std::min(16.0f, bits));
        
        // Calculate quantization step size
        // At 16 bits: step = 1/32768 = 0.00003
        // At 1 bit: step = 1 (full range quantization)
        float levels = std::pow(2.0f, bits_);
        quantStep_ = 2.0f / levels;  // Range is -1 to 1, so 2.0 total
        
        // Calculate rate reduction: how many samples to hold
        // At 0.0: hold for 1 sample (no reduction)
        // At 1.0: hold for up to 64 samples (extreme reduction)
        holdSamples_ = 1.0f + rateReduction * 63.0f;
    }
    
    void processStereo(float inL, float inR, float& outL, float& outR) {
        counter_ += 1.0f;
        
        if (counter_ >= holdSamples_) {
            counter_ = 0.0f;
            
            // Quantize to specified bit depth
            if (quantStep_ > 0.0001f) {
                holdL_ = std::round(inL / quantStep_) * quantStep_;
                holdR_ = std::round(inR / quantStep_) * quantStep_;
            } else {
                // Very high bit depth, just pass through
                holdL_ = inL;
                holdR_ = inR;
            }
        }
        
        // Output held values (sample-and-hold)
        outL = holdL_;
        outR = holdR_;
    }
    
private:
    float holdL_ = 0.0f;
    float holdR_ = 0.0f;
    float counter_ = 0.0f;
    float bits_ = 16.0f;
    float quantStep_ = 2.0f / 65536.0f;  // Default 16-bit
    float holdSamples_ = 1.0f;           // How many samples to hold
};

//------------------------------------------------------------------------------
// Moog VCF DSP (Variation 2)
// Based on musicdsp.org - Stilson/Smith CCRMA, Timo Tossavainen
// 4-pole (24dB/octave) resonant lowpass filter
//------------------------------------------------------------------------------

class MoogVCFDSP {
public:
    MoogVCFDSP() { reset(); }
    
    void reset() {
        stage_[0] = stage_[1] = stage_[2] = stage_[3] = 0.0f;
        delay_[0] = delay_[1] = delay_[2] = delay_[3] = 0.0f;
    }
    
    void setParams(float cutoffNorm, float resonance, float sampleRate) {
        // cutoffNorm: 0.0 to 1.0 (maps to 20Hz - 20kHz logarithmically)
        // resonance: 0.0 to 1.0
        
        // Logarithmic frequency mapping
        float minFreq = 20.0f;
        float maxFreq = std::min(20000.0f, sampleRate * 0.45f);  // Stay below Nyquist
        float freq = minFreq * std::pow(maxFreq / minFreq, cutoffNorm);
        
        // Calculate normalized frequency (0 to 1, where 1 = Nyquist)
        float wc = 2.0f * M_PI * freq / sampleRate;
        
        // Attempt bilinear pre-warp for better high frequency response
        float g = std::tan(wc * 0.5f);
        g = std::min(g, 0.99f);  // Stability limit
        
        // One-pole filter coefficient
        cutoff_ = g / (1.0f + g);
        
        // Resonance with slight reduction at high frequencies for stability
        resonance_ = resonance * 3.99f * (1.0f - 0.15f * cutoffNorm);
    }
    
    float process(float input) {
        // Feedback with soft clipping
        float feedback = resonance_ * stage_[3];
        input -= feedback;
        
        // Soft clip the input to prevent blowup
        input = std::tanh(input);
        
        // Four cascaded one-pole lowpass filters
        // Using the "cheap" Moog formula: y = y + cutoff * (x - y)
        stage_[0] += cutoff_ * (input - stage_[0]);
        stage_[1] += cutoff_ * (stage_[0] - stage_[1]);
        stage_[2] += cutoff_ * (stage_[1] - stage_[2]);
        stage_[3] += cutoff_ * (stage_[2] - stage_[3]);
        
        // Denormal protection
        for (int i = 0; i < 4; i++) {
            if (!std::isfinite(stage_[i]) || std::fabs(stage_[i]) < 1e-15f) {
                stage_[i] = 0.0f;
            }
        }
        
        return stage_[3];
    }
    
private:
    float cutoff_ = 0.5f;      // Filter coefficient
    float resonance_ = 0.0f;   // Feedback amount (0-4)
    
    // Filter state (4 poles)
    float stage_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float delay_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

//------------------------------------------------------------------------------
// Parameter Smoother for zipper-free control changes
//------------------------------------------------------------------------------

class SmoothParam {
public:
    void setTarget(float target) { target_ = target; }
    float getTarget() const { return target_; }
    
    float process() {
        current_ += smoothCoeff_ * (target_ - current_);
        return current_;
    }
    
    void setSmoothTime(float timeMs, float sampleRate) {
        smoothCoeff_ = 1.0f - std::exp(-1.0f / (sampleRate * timeMs * 0.001f));
    }
    
    void setImmediate(float value) {
        current_ = target_ = value;
    }
    
private:
    float current_ = 0.0f;
    float target_ = 0.0f;
    float smoothCoeff_ = 0.001f;
};

//------------------------------------------------------------------------------
// Tapestry Expander Module
//------------------------------------------------------------------------------

struct TapestryExpander : Module {
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
        // Direct outputs (for standalone use or alternative routing)
        AUDIO_OUT_L,
        AUDIO_OUT_R,
        
        NUM_OUTPUTS
    };
    
    enum LightIds {
        CONNECTED_LIGHT,       // Indicates expander is connected to Tapestry
        
        NUM_LIGHTS
    };
    
    //--------------------------------------------------------------------------
    // DSP Processors
    //--------------------------------------------------------------------------
    
    BitCrusherDSP bitCrusher_;
    MoogVCFDSP moogFilterL_;
    MoogVCFDSP moogFilterR_;
    
    //--------------------------------------------------------------------------
    // Parameter Smoothers
    //--------------------------------------------------------------------------
    
    SmoothParam smoothBits_;
    SmoothParam smoothRate_;
    SmoothParam smoothCrushMix_;
    SmoothParam smoothCutoff_;
    SmoothParam smoothReso_;
    SmoothParam smoothFilterMix_;
    
    //--------------------------------------------------------------------------
    // State
    //--------------------------------------------------------------------------
    
    float sampleRate_ = 48000.0f;
    
    // DC blocking filters (to prevent pops from DC offset)
    float dcBlockerInL_ = 0.0f;
    float dcBlockerInR_ = 0.0f;
    float dcBlockerOutL_ = 0.0f;
    float dcBlockerOutR_ = 0.0f;
    
    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    
    TapestryExpander();
    ~TapestryExpander();
    
    //--------------------------------------------------------------------------
    // Module Interface
    //--------------------------------------------------------------------------
    
    void onSampleRateChange() override;
    void onReset() override;
    void process(const ProcessArgs& args) override;
    
    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------
    
    float getModulatedParam(int paramId, int cvId, float cvScale);
};

//------------------------------------------------------------------------------
// Widget Declaration
//------------------------------------------------------------------------------

struct TapestryExpanderWidget : ModuleWidget {
    TapestryExpanderWidget(TapestryExpander* module);
};
