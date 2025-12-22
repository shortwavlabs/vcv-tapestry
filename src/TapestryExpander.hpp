#pragma once

#include "plugin.hpp"
#include <cmath>

#include "TapestryExpanderMessage.hpp"

#include "dsp/tapestry-effects.h"
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

        // Output
        OUTPUT_LEVEL_PARAM,    // Post-effects gain (0-2)
        
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
    SmoothParam smoothOutputLevel_;
    
    //--------------------------------------------------------------------------
    // State
    //--------------------------------------------------------------------------
    
    float sampleRate_ = 48000.0f;
    
    // DC blocking filters (to prevent pops from DC offset)
    float dcBlockerInL_ = 0.0f;
    float dcBlockerInR_ = 0.0f;
    float dcBlockerOutL_ = 0.0f;
    float dcBlockerOutR_ = 0.0f;
    float dcBlockCoeff_ = 0.995f;  // Calculated based on sample rate
    
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
