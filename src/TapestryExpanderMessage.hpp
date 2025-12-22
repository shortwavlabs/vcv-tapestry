#pragma once

// Message payload exchanged between Tapestry (left) and TapestryExpander (right).
// VCV Rack expander messaging is double-buffered and flipped by the engine.
// Producer buffers are write-only; consumer buffers are read-only.
struct TapestryExpanderMessage {
    // Audio from Tapestry to Expander (pre-output)
    float audioL = 0.0f;
    float audioR = 0.0f;

    // Processed audio from Expander back to Tapestry
    float processedL = 0.0f;
    float processedR = 0.0f;

    // Flag indicating expander has written valid processed audio
    bool expanderConnected = false;

    // Sample rate for DSP coefficient calculation
    float sampleRate = 48000.0f;
};
