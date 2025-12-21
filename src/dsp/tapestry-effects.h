#pragma once

#include <cmath>
#include <algorithm>

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
};