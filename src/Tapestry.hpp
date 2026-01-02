#pragma once

#include "plugin.hpp"
#include "dsp/tapestry-dsp.h"
#include "TapestryExpanderMessage.hpp"
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
 * - Mix: Crossfade recording
 * - Time Stretch: Clock-synced granular playback
 */

struct Tapestry : Module
{
  //--------------------------------------------------------------------------
  // Waveform Color Presets
  //--------------------------------------------------------------------------

  enum class WaveformColor
  {
    Red = 0,
    Amber,
    Green,
    BabyBlue,
    Peach,
    Pink,
    White,
    NUM_COLORS
  };

  //--------------------------------------------------------------------------
  // Param IDs
  //--------------------------------------------------------------------------

  enum ParamIds
  {
    // Main knobs
    SOS_PARAM,              // Mix (combo pot)
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
    SPLICE_COUNT_TOGGLE_BUTTON,  // Toggle splice count (4/8/16)

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
    CLEAR_SPLICES_INPUT,
    SPLICE_COUNT_TOGGLE_INPUT,

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
    SPLICE_COUNT_LED,

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
  dsp::SchmittTrigger clearSplicesInputTrigger;
  dsp::SchmittTrigger spliceCountToggleInputTrigger;

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
  bool spliceCountToggleButtonHeld = false;

  static constexpr float kLongPressTime = 3.0f;      // 3 seconds for delete all
  static constexpr float kComboWindowTime = 0.3f;    // 300ms combo detection

  //--------------------------------------------------------------------------
  // Splice Count Toggle State
  //--------------------------------------------------------------------------

  int spliceCountMode = 0;  // 0=4, 1=8, 2=16
  static constexpr int kSpliceCountOptions[] = {4, 8, 16};
  static constexpr int kNumSpliceCountOptions = 3;

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

  // Track expander changes to avoid consuming stale processed audio
  int64_t lastRightExpanderModuleId_ = -1;

  //--------------------------------------------------------------------------
  // Waveform Display Settings
  //--------------------------------------------------------------------------

  WaveformColor waveformColor = WaveformColor::BabyBlue;

  // Get RGB values for current waveform color (0-255 range)
  void getWaveformColorRGB(int& r, int& g, int& b) const
  {
    switch (waveformColor)
    {
    case WaveformColor::Red:
      r = 255; g = 0; b = 0;
      break;
    case WaveformColor::Amber:
      r = 255; g = 180; b = 0;
      break;
    case WaveformColor::Green:
      r = 0; g = 255; b = 0;
      break;
    case WaveformColor::BabyBlue:
      r = 100; g = 200; b = 255;
      break;
    case WaveformColor::Peach:
      r = 255; g = 200; b = 150;
      break;
    case WaveformColor::Pink:
      r = 255; g = 100; b = 200;
      break;
    case WaveformColor::White:
      r = 255; g = 255; b = 255;
      break;
    default:
      r = 100; g = 200; b = 255; // Default to Baby Blue
      break;
    }
  }

  // Get color name as string
  const char* getWaveformColorName() const
  {
    switch (waveformColor)
    {
    case WaveformColor::Red: return "Red";
    case WaveformColor::Amber: return "Amber";
    case WaveformColor::Green: return "Green";
    case WaveformColor::BabyBlue: return "Baby Blue";
    case WaveformColor::Peach: return "Peach";
    case WaveformColor::Pink: return "Pink";
    case WaveformColor::White: return "White";
    default: return "Baby Blue";
    }
  }

  //--------------------------------------------------------------------------
  // Constructor
  //--------------------------------------------------------------------------

  Tapestry()
  {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // Main knobs
    configParam(SOS_PARAM, 0.0f, 1.0f, 1.0f, "Mix", "%", 0.0f, 100.0f);
    configParam(GENE_SIZE_PARAM, 0.0f, 1.0f, 0.0f, "Grain Size", "%", 0.0f, 100.0f);
    configParam(GENE_SIZE_CV_ATTEN, -1.0f, 1.0f, 0.0f, "Grain Size CV", "%", 0.0f, 100.0f);
    configParam(VARI_SPEED_PARAM, 0.0f, 1.0f, 0.5f, "Speed");
    configParam(VARI_SPEED_CV_ATTEN, -1.0f, 1.0f, 0.0f, "Speed CV", "%", 0.0f, 100.0f);
    configParam(MORPH_PARAM, 0.0f, 1.0f, 0.3f, "Density", "%", 0.0f, 100.0f);
    configParam(SLIDE_PARAM, 0.0f, 1.0f, 0.0f, "Scan", "%", 0.0f, 100.0f);
    configParam(SLIDE_CV_ATTEN, -1.0f, 1.0f, 0.0f, "Scan CV", "%", 0.0f, 100.0f);
    auto* organizeParam = configParam(ORGANIZE_PARAM, 0.0f, 1.0f, 0.0f, "Select");
    organizeParam->snapEnabled = true;
    organizeParam->displayOffset = 1.0f; // Display as 1-based instead of 0-based

    // Buttons
    configButton(REC_BUTTON, "Record");
    configButton(SPLICE_BUTTON, "Marker");
    configButton(SHIFT_BUTTON, "Next");
    configButton(CLEAR_SPLICES_BUTTON, "Clear Markers");
    configButton(SPLICE_COUNT_TOGGLE_BUTTON, "Auto Markers");

    // Toggles
    configSwitch(OVERDUB_TOGGLE, 0.0f, 1.0f, 0.0f, "Overdub Mode",
                 {"Replace (clear on record)", "Overdub (keep existing)"});

    // Audio inputs
    configInput(AUDIO_IN_L, "Audio L");
    configInput(AUDIO_IN_R, "Audio R");

    // CV inputs
    configInput(SOS_CV_INPUT, "S.O.S. CV");
    configInput(GENE_SIZE_CV_INPUT, "Grain Size CV");
    configInput(VARI_SPEED_CV_INPUT, "Speed CV");
    configInput(MORPH_CV_INPUT, "Density CV");
    configInput(SLIDE_CV_INPUT, "Scan CV");
    configInput(ORGANIZE_CV_INPUT, "Select CV");

    // Gate inputs
    configInput(CLK_INPUT, "Clock");
    configInput(PLAY_INPUT, "Play Gate");
    configInput(REC_INPUT, "Record Gate");
    configInput(SPLICE_INPUT, "Marker Gate");
    configInput(SHIFT_INPUT, "Next Gate");
    configInput(CLEAR_SPLICES_INPUT, "Clear Markers Gate");
    configInput(SPLICE_COUNT_TOGGLE_INPUT, "Auto Markers Gate");

    // Outputs
    configOutput(AUDIO_OUT_L, "Audio L");
    configOutput(AUDIO_OUT_R, "Audio R");
    configOutput(CV_OUTPUT, "CV");
    configOutput(EOSG_OUTPUT, "End of Marker/Grain");

    // Set bypass routes
    configBypass(AUDIO_IN_L, AUDIO_OUT_L);
    configBypass(AUDIO_IN_R, AUDIO_OUT_R);

    // Allocate expander message buffers (double-buffered, flipped by Rack engine)
    rightExpander.producerMessage = new TapestryExpanderMessage();
    rightExpander.consumerMessage = new TapestryExpanderMessage();

    onSampleRateChange();
  }

  ~Tapestry() {
    delete static_cast<TapestryExpanderMessage*>(rightExpander.producerMessage);
    delete static_cast<TapestryExpanderMessage*>(rightExpander.consumerMessage);
    rightExpander.producerMessage = nullptr;
    rightExpander.consumerMessage = nullptr;
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

  // Set marker count and distribute evenly across buffer
  void setSpliceCount(int n);

  // Get current marker count value
  int getCurrentSpliceCount() const
  {
    return kSpliceCountOptions[spliceCountMode];
  }

  // Update select parameter range based on current marker count
  void updateOrganizeParamRange();
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
  
  // Mouse interaction state
  float hoverX = -1.0f;           // X position of hover cursor (-1 if not hovering)
  bool isHovering = false;        // Whether mouse is currently over the widget
  int hoveredSpliceIndex = -1;    // Index of splice marker being hovered (-1 if none)
  
  // Constants for splice marker hit detection
  static constexpr float kSpliceHitWidth = 6.0f;  // Pixels on each side of marker for hit detection

  void draw(const DrawArgs& args) override;
  void drawWaveform(const DrawArgs& args);
  void drawSpliceMarkers(const DrawArgs& args);
  void drawPlayhead(const DrawArgs& args);
  void drawGeneWindow(const DrawArgs& args);
  void drawHoverIndicator(const DrawArgs& args);
  
  // Mouse event handlers
  void onButton(const ButtonEvent& e) override;
  void onHover(const HoverEvent& e) override;
  void onLeave(const LeaveEvent& e) override;
  void onDragHover(const DragHoverEvent& e) override;
  
  // Helper methods
  size_t xPositionToFrame(float x) const;
  float frameToXPosition(size_t frame) const;
  int getSpliceIndexAtPosition(float x) const;
};
