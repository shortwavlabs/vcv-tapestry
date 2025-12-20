#pragma once

#include "plugin.hpp"
#include "dsp/tapestry-dsp.h"
#include <thread>
#include <atomic>
#include <mutex>

// Forward declarations for UI components
struct ReelDisplay;
struct TapestryWidget;

/*
 * Tapestry VCV Rack Module
 *
 * A faithful recreation of the Make Noise Tapestry.
 * Combines tape music tools with granular/microsound processing.
 *
 * Features:
 * - Reels: Audio buffers up to 2.9 minutes
 * - Splices: Up to 300 markers per reel
 * - Genes: Granular particles with overlap control
 * - Vari-Speed: Bipolar speed/direction control
 * - Sound On Sound: Crossfade recording
 * - Time Stretch: Clock-synced granular playback
 */

struct Tapestry : Module
{
  //--------------------------------------------------------------------------
  // Param IDs
  //--------------------------------------------------------------------------

  enum ParamIds
  {
    // Main knobs
    SOS_PARAM,              // Sound On Sound (combo pot)
    GENE_SIZE_PARAM,        // Gene size
    GENE_SIZE_CV_ATTEN,     // Gene size CV attenuverter
    VARI_SPEED_PARAM,       // Vari-Speed bipolar knob
    VARI_SPEED_CV_ATTEN,    // Vari-Speed CV attenuverter
    MORPH_PARAM,            // Morph
    SLIDE_PARAM,            // Slide
    SLIDE_CV_ATTEN,         // Slide CV attenuverter
    ORGANIZE_PARAM,         // Organize

    // Buttons
    REC_BUTTON,
    SPLICE_BUTTON,
    SHIFT_BUTTON,
    CLEAR_SPLICES_BUTTON,  // Clear all splices

    // Toggles
    OVERDUB_TOGGLE,    // Overdub mode: 0 = replace, 1 = overdub

    NUM_PARAMS
  };

  //--------------------------------------------------------------------------
  // Input IDs
  //--------------------------------------------------------------------------

  enum InputIds
  {
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

  //--------------------------------------------------------------------------
  // Output IDs
  //--------------------------------------------------------------------------

  enum OutputIds
  {
    AUDIO_OUT_L,
    AUDIO_OUT_R,
    CV_OUTPUT,
    EOSG_OUTPUT,

    NUM_OUTPUTS
  };

  //--------------------------------------------------------------------------
  // Light IDs
  //--------------------------------------------------------------------------

  enum LightIds
  {
    // Activity windows (RGB LEDs)
    ENUMS(VARI_SPEED_LEFT_LIGHT, 3),  // Left direction indicator
    ENUMS(VARI_SPEED_RIGHT_LIGHT, 3), // Right direction indicator
    ENUMS(REEL_LIGHT, 3),             // Reel color indicator
    ENUMS(SPLICE_LIGHT, 3),           // Splice color indicator
    ENUMS(CV_OUT_LIGHT, 3),           // CV level indicator

    // Button LEDs
    REC_LED,
    SPLICE_LED,
    SHIFT_LED,
    CLEAR_SPLICES_LED,

    NUM_LIGHTS
  };

  //--------------------------------------------------------------------------
  // DSP Engine
  //--------------------------------------------------------------------------

  ShortwavDSP::TapestryDSP dsp;

  //--------------------------------------------------------------------------
  // Trigger Processors
  //--------------------------------------------------------------------------

  dsp::SchmittTrigger recButtonTrigger;
  dsp::SchmittTrigger spliceButtonTrigger;
  dsp::SchmittTrigger shiftButtonTrigger;

  dsp::SchmittTrigger clkTrigger;
  dsp::SchmittTrigger playTrigger;
  dsp::SchmittTrigger recInputTrigger;
  dsp::SchmittTrigger spliceInputTrigger;
  dsp::SchmittTrigger shiftInputTrigger;

  //--------------------------------------------------------------------------
  // Button Combo State
  //--------------------------------------------------------------------------

  float recButtonHoldTime = 0.0f;
  float spliceButtonHoldTime = 0.0f;
  float shiftButtonHoldTime = 0.0f;
  bool recButtonHeld = false;
  bool spliceButtonHeld = false;
  bool shiftButtonHeld = false;
  bool clearSplicesButtonHeld = false;

  static constexpr float kLongPressTime = 3.0f;      // 3 seconds for delete all
  static constexpr float kComboWindowTime = 0.3f;    // 300ms combo detection

  //--------------------------------------------------------------------------
  // File I/O State
  //--------------------------------------------------------------------------

  std::atomic<bool> fileLoading{false};
  std::atomic<bool> fileSaving{false};
  std::string currentFilePath;
  std::string currentFileName;
  std::mutex fileMutex;

  // Pending splice data from JSON deserialization
  std::vector<size_t> pendingSpliceMarkers_;
  int pendingSpliceIndex_ = -1;

  //--------------------------------------------------------------------------
  // Reel Management
  //--------------------------------------------------------------------------

  int currentReelIndex = 0;
  static constexpr int kMaxReels = 32;

  //--------------------------------------------------------------------------
  // UI Reference
  //--------------------------------------------------------------------------

  ReelDisplay* reelDisplay = nullptr;

  //--------------------------------------------------------------------------
  // EOSG Output State
  //--------------------------------------------------------------------------

  dsp::PulseGenerator eosgPulse;
  static constexpr float kEosgPulseWidth = 0.001f; // 1ms pulse

  //--------------------------------------------------------------------------
  // Constructor
  //--------------------------------------------------------------------------

  Tapestry()
  {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // Main knobs
    configParam(SOS_PARAM, 0.0f, 1.0f, 1.0f, "Sound On Sound", "%", 0.0f, 100.0f);
    configParam(GENE_SIZE_PARAM, 0.0f, 1.0f, 0.0f, "Gene Size", "%", 0.0f, 100.0f);
    configParam(GENE_SIZE_CV_ATTEN, -1.0f, 1.0f, 0.0f, "Gene Size CV", "%", 0.0f, 100.0f);
    configParam(VARI_SPEED_PARAM, 0.0f, 1.0f, 0.5f, "Vari-Speed");
    configParam(VARI_SPEED_CV_ATTEN, -1.0f, 1.0f, 0.0f, "Vari-Speed CV", "%", 0.0f, 100.0f);
    configParam(MORPH_PARAM, 0.0f, 1.0f, 0.3f, "Morph", "%", 0.0f, 100.0f);
    configParam(SLIDE_PARAM, 0.0f, 1.0f, 0.0f, "Slide", "%", 0.0f, 100.0f);
    configParam(SLIDE_CV_ATTEN, -1.0f, 1.0f, 0.0f, "Slide CV", "%", 0.0f, 100.0f);
    configParam(ORGANIZE_PARAM, 0.0f, 1.0f, 0.0f, "Organize", "%", 0.0f, 100.0f);

    // Buttons
    configButton(REC_BUTTON, "Record");
    configButton(SPLICE_BUTTON, "Splice");
    configButton(SHIFT_BUTTON, "Shift");
    configButton(CLEAR_SPLICES_BUTTON, "Clear Splices");

    // Toggles
    configSwitch(OVERDUB_TOGGLE, 0.0f, 1.0f, 0.0f, "Overdub Mode",
                 {"Replace (clear on record)", "Overdub (keep existing)"});

    // Audio inputs
    configInput(AUDIO_IN_L, "Audio L");
    configInput(AUDIO_IN_R, "Audio R");

    // CV inputs
    configInput(SOS_CV_INPUT, "S.O.S. CV");
    configInput(GENE_SIZE_CV_INPUT, "Gene Size CV");
    configInput(VARI_SPEED_CV_INPUT, "Vari-Speed CV");
    configInput(MORPH_CV_INPUT, "Morph CV");
    configInput(SLIDE_CV_INPUT, "Slide CV");
    configInput(ORGANIZE_CV_INPUT, "Organize CV");

    // Gate inputs
    configInput(CLK_INPUT, "Clock");
    configInput(PLAY_INPUT, "Play Gate");
    configInput(REC_INPUT, "Record Gate");
    configInput(SPLICE_INPUT, "Splice Gate");
    configInput(SHIFT_INPUT, "Shift Gate");

    // Outputs
    configOutput(AUDIO_OUT_L, "Audio L");
    configOutput(AUDIO_OUT_R, "Audio R");
    configOutput(CV_OUTPUT, "CV");
    configOutput(EOSG_OUTPUT, "End of Splice/Gene");

    // Set bypass routes
    configBypass(AUDIO_IN_L, AUDIO_OUT_L);
    configBypass(AUDIO_IN_R, AUDIO_OUT_R);

    onSampleRateChange();
  }

  //--------------------------------------------------------------------------
  // Sample Rate Change
  //--------------------------------------------------------------------------

  void onSampleRateChange() override
  {
    float sr = APP->engine->getSampleRate();
    dsp.setSampleRate(sr);
  }

  //--------------------------------------------------------------------------
  // Main Process
  //--------------------------------------------------------------------------

  void process(const ProcessArgs& args) override;

  //--------------------------------------------------------------------------
  // Button Processing
  //--------------------------------------------------------------------------

  void processButtons(const ProcessArgs& args);
  void processButtonCombos(const ProcessArgs& args);

  //--------------------------------------------------------------------------
  // Gate/Trigger Processing
  //--------------------------------------------------------------------------

  void processGateInputs(const ProcessArgs& args);

  //--------------------------------------------------------------------------
  // LED Updates
  //--------------------------------------------------------------------------

  void updateLights(const ProcessArgs& args);

  //--------------------------------------------------------------------------
  // File I/O
  //--------------------------------------------------------------------------

  void loadFileAsync(const std::string& path);
  void saveFileAsync(const std::string& path);

  //--------------------------------------------------------------------------
  // JSON Serialization
  //--------------------------------------------------------------------------

  json_t* dataToJson() override;
  void dataFromJson(json_t* rootJ) override;

  //--------------------------------------------------------------------------
  // Helpers
  //--------------------------------------------------------------------------

  // Get current playback position in frames
  size_t getCurrentPlaybackFrame() const
  {
    return static_cast<size_t>(dsp.getGrainEngine().getPlayheadPosition());
  }
};

//------------------------------------------------------------------------------
// Widget Declaration
//------------------------------------------------------------------------------

struct TapestryWidget : ModuleWidget
{
  Tapestry* module = nullptr;

  TapestryWidget(Tapestry* module);

  void appendContextMenu(Menu* menu) override;
};

//------------------------------------------------------------------------------
// Reel Display Widget
//------------------------------------------------------------------------------

struct ReelDisplay : OpaqueWidget
{
  Tapestry* module = nullptr;

  void draw(const DrawArgs& args) override;
  void drawWaveform(const DrawArgs& args);
  void drawSpliceMarkers(const DrawArgs& args);
  void drawPlayhead(const DrawArgs& args);
  void drawGeneWindow(const DrawArgs& args);
};
