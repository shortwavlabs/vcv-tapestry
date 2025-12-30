#include "Tapestry.hpp"
#include <osdialog.h>

//------------------------------------------------------------------------------
// Main Process Implementation
//------------------------------------------------------------------------------

void Tapestry::process(const ProcessArgs& args)
{
  // Read overdub toggle FIRST (before button processing needs it)
  dsp.setOverdubMode(params[OVERDUB_TOGGLE].getValue() > 0.5f);

  // Apply pending splice markers from JSON deserialization (after file is loaded)
  if (!pendingSpliceMarkers_.empty() && !fileLoading.load())
  {
    size_t totalFrames = dsp.getBuffer().getUsedFrames();
    if (totalFrames > 0)
    {
      dsp.getSpliceManager().setFromMarkerPositions(pendingSpliceMarkers_, totalFrames);
      
      // Restore the current splice index
      if (pendingSpliceIndex_ >= 0)
      {
        dsp.getSpliceManager().setCurrentIndex(pendingSpliceIndex_);
      }
      
      pendingSpliceMarkers_.clear();
      pendingSpliceIndex_ = -1;
      
      // Update organize parameter range to match loaded splice count
      updateOrganizeParamRange();
    }
  }

  // Process button inputs
  processButtons(args);
  processButtonCombos(args);

  // Process gate/trigger inputs
  processGateInputs(args);

  // Read parameter values
  dsp.setSos(params[SOS_PARAM].getValue());
  dsp.setGeneSize(params[GENE_SIZE_PARAM].getValue());
  dsp.setMorph(params[MORPH_PARAM].getValue());
  dsp.setSlide(params[SLIDE_PARAM].getValue());
  
  // Organize parameter: normalize based on splice count
  size_t numSplices = dsp.getSpliceManager().getNumSplices();
  if (numSplices > 0)
  {
    dsp.setOrganize(params[ORGANIZE_PARAM].getValue() / static_cast<float>(numSplices));
  }
  else
  {
    dsp.setOrganize(0.0f);
  }
  
  dsp.setVariSpeed(params[VARI_SPEED_PARAM].getValue());

  // Read CV inputs
  if (inputs[SOS_CV_INPUT].isConnected())
  {
    dsp.setSosCv(inputs[SOS_CV_INPUT].getVoltage());
  }
  else
  {
    dsp.setSosCv(0.0f);
  }

  if (inputs[GENE_SIZE_CV_INPUT].isConnected())
  {
    dsp.setGeneSizeCv(inputs[GENE_SIZE_CV_INPUT].getVoltage(),
                      params[GENE_SIZE_CV_ATTEN].getValue());
  }
  else
  {
    dsp.setGeneSizeCv(0.0f, 0.0f);
  }

  if (inputs[VARI_SPEED_CV_INPUT].isConnected())
  {
    dsp.setVariSpeedCv(inputs[VARI_SPEED_CV_INPUT].getVoltage(),
                       params[VARI_SPEED_CV_ATTEN].getValue());
  }
  else
  {
    dsp.setVariSpeedCv(0.0f, 0.0f);
  }

  if (inputs[MORPH_CV_INPUT].isConnected())
  {
    dsp.setMorphCv(inputs[MORPH_CV_INPUT].getVoltage());
  }
  else
  {
    dsp.setMorphCv(0.0f);
  }

  if (inputs[SLIDE_CV_INPUT].isConnected())
  {
    dsp.setSlideCv(inputs[SLIDE_CV_INPUT].getVoltage(),
                   params[SLIDE_CV_ATTEN].getValue());
  }
  else
  {
    dsp.setSlideCv(0.0f, 0.0f);
  }

  if (inputs[ORGANIZE_CV_INPUT].isConnected())
  {
    dsp.setOrganizeCv(inputs[ORGANIZE_CV_INPUT].getVoltage());
  }
  else
  {
    dsp.setOrganizeCv(0.0f);
  }

  // Read audio inputs
  float audioInL = 0.0f;
  float audioInR = 0.0f;

  if (inputs[AUDIO_IN_L].isConnected())
  {
    audioInL = inputs[AUDIO_IN_L].getVoltage() / 5.0f; // Normalize to ±1
  }
  if (inputs[AUDIO_IN_R].isConnected())
  {
    audioInR = inputs[AUDIO_IN_R].getVoltage() / 5.0f;
  }
  else if (inputs[AUDIO_IN_L].isConnected())
  {
    audioInR = audioInL; // Mono to stereo
  }

  // Process DSP
  ShortwavDSP::TapestryDSP::ProcessResult result = dsp.process(audioInL, audioInR);

  // Start with Tapestry's output
  float finalOutL = result.audioOutL;
  float finalOutR = result.audioOutR;

  // Check for TapestryExpander on the right
  if (rightExpander.module && rightExpander.module->model == modelTapestryExpander)
  {
    if (rightExpander.moduleId != lastRightExpanderModuleId_)
    {
      lastRightExpanderModuleId_ = rightExpander.moduleId;
      // When a new expander is connected, clear the state in its producer buffer
      // rather than writing to our read-only consumer buffer.
      if (rightExpander.module->leftExpander.producerMessage)
      {
        auto* initMsg = static_cast<TapestryExpanderMessage*>(rightExpander.module->leftExpander.producerMessage);
        initMsg->processedL = 0.0f;
        initMsg->processedR = 0.0f;
        initMsg->expanderConnected = false;
      }
    }

    // Send audio to the expander by writing into its leftExpander producer buffer.
    // Messages are flipped by the engine, so this incurs 1-sample latency.
    if (rightExpander.module->leftExpander.producerMessage)
    {
      auto* toExpander = static_cast<TapestryExpanderMessage*>(rightExpander.module->leftExpander.producerMessage);
      toExpander->audioL = result.audioOutL;
      toExpander->audioR = result.audioOutR;
      toExpander->sampleRate = args.sampleRate;
      rightExpander.module->leftExpander.messageFlipRequested = true;
    }

    // Receive processed audio from the expander via our consumer buffer.
    if (rightExpander.consumerMessage)
    {
      auto* fromExpander = static_cast<TapestryExpanderMessage*>(rightExpander.consumerMessage);
      if (fromExpander->expanderConnected)
      {
        finalOutL = fromExpander->processedL;
        finalOutR = fromExpander->processedR;
      }
    }
  }
  else
  {
    lastRightExpanderModuleId_ = -1;
  }

  // Write audio outputs (always write both channels)
  outputs[AUDIO_OUT_L].setVoltage(finalOutL * 5.0f);
  outputs[AUDIO_OUT_R].setVoltage(finalOutR * 5.0f);

  // Write CV output
  outputs[CV_OUTPUT].setVoltage(result.cvOut);

  // EOSG output
  if (result.endOfSpliceGene)
  {
    eosgPulse.trigger(kEosgPulseWidth);
  }
  outputs[EOSG_OUTPUT].setVoltage(eosgPulse.process(args.sampleTime) ? 10.0f : 0.0f);

  // Update lights
  updateLights(args);
}

//------------------------------------------------------------------------------
// Button Processing
//------------------------------------------------------------------------------

void Tapestry::processButtons(const ProcessArgs& args)
{
  float dt = args.sampleTime;

  // REC button - toggle on press (not release) for immediate response
  bool recPressed = params[REC_BUTTON].getValue() > 0.5f;
  if (recPressed)
  {
    if (!recButtonHeld)
    {
      recButtonHeld = true;
      recButtonHoldTime = 0.0f;
      
      // Start/stop recording immediately on button press (single press action)
      // Combo actions will override this if another button is also pressed
      if (!spliceButtonHeld && !shiftButtonHeld)
      {
        if (dsp.isRecording())
        {
          bool clockSync = inputs[CLK_INPUT].isConnected();
          dsp.stopRecordingRequest(clockSync);
        }
        else
        {
          // Clear and start fresh recording (replaces existing content)
          // In overdub mode, start from current playhead position
          bool clockSync = inputs[CLK_INPUT].isConnected();
          size_t currentPos = static_cast<size_t>(dsp.getGrainEngine().getPlayheadPosition());
          dsp.clearAndStartRecording(clockSync, currentPos);
        }
      }
    }
    recButtonHoldTime += dt;
  }
  else
  {
    recButtonHeld = false;
  }

  // SPLICE button - trigger on press for immediate response
  bool splicePressed = params[SPLICE_BUTTON].getValue() > 0.5f;
  if (splicePressed)
  {
    if (!spliceButtonHeld)
    {
      spliceButtonHeld = true;
      spliceButtonHoldTime = 0.0f;
      
      // Single press action: create splice marker (immediate response)
      // Combo actions will be handled separately if other buttons are also pressed
      if (!recButtonHeld && !shiftButtonHeld)
      {
        dsp.onSpliceTrigger(getCurrentPlaybackFrame());
        updateOrganizeParamRange();
      }
    }
    spliceButtonHoldTime += dt;
  }
  else
  {
    spliceButtonHeld = false;
  }

  // SHIFT button - trigger on press for immediate response
  bool shiftPressed = params[SHIFT_BUTTON].getValue() > 0.5f;
  if (shiftPressed)
  {
    if (!shiftButtonHeld)
    {
      shiftButtonHeld = true;
      shiftButtonHoldTime = 0.0f;
      
      // Single press action: increment splice (immediate response)
      // Combo actions will be handled separately if other buttons are also pressed
      if (!recButtonHeld && !spliceButtonHeld)
      {
        dsp.onShiftTrigger();
      }
    }
    shiftButtonHoldTime += dt;
  }
  else
  {
    shiftButtonHeld = false;
  }

  // CLEAR SPLICES button - single press to clear all splices
  bool clearSplicesPressed = params[CLEAR_SPLICES_BUTTON].getValue() > 0.5f;
  if (clearSplicesPressed && !clearSplicesButtonHeld)
  {
    clearSplicesButtonHeld = true;
    dsp.deleteAllMarkers();
    spliceCountMode = 0;  // Reset to 4 splices for next toggle
    updateOrganizeParamRange();
    params[ORGANIZE_PARAM].setValue(0.0f);  // Reset organize to 0
  }
  else if (!clearSplicesPressed)
  {
    clearSplicesButtonHeld = false;
  }

  // SPLICE COUNT TOGGLE button - cycle through 4, 8, 16
  bool spliceCountTogglePressed = params[SPLICE_COUNT_TOGGLE_BUTTON].getValue() > 0.5f;
  if (spliceCountTogglePressed && !spliceCountToggleButtonHeld)
  {
    spliceCountToggleButtonHeld = true;
    
    // Apply current mode first, then cycle to next
    setSpliceCount(kSpliceCountOptions[spliceCountMode]);
    spliceCountMode = (spliceCountMode + 1) % kNumSpliceCountOptions;
  }
  else if (!spliceCountTogglePressed)
  {
    spliceCountToggleButtonHeld = false;
  }
}

//------------------------------------------------------------------------------
// Button Combo Processing
//------------------------------------------------------------------------------

void Tapestry::processButtonCombos(const ProcessArgs& args)
{
  // REC + SHIFT = Auto-Level
  if (recButtonHeld && shiftButtonHeld)
  {
    if (!dsp.isAutoLeveling())
    {
      dsp.startAutoLevel();
    }
  }
  else if (dsp.isAutoLeveling() && !recButtonHeld)
  {
    dsp.stopAutoLevel();
  }

  // REC + SPLICE = Record into new splice
  if (recButtonHeld && spliceButtonHeld && !dsp.isRecording())
  {
    bool clockSync = inputs[CLK_INPUT].isConnected();
    dsp.startRecordingNewSplice(clockSync);
    recButtonHoldTime = kComboWindowTime + 1.0f; // Prevent single press action
  }

  // SPLICE + REC = Enter/Exit Reel Mode
  if (spliceButtonHeld && recButtonHeld &&
      spliceButtonHoldTime < kComboWindowTime)
  {
    ShortwavDSP::ModuleMode currentMode = dsp.getModuleMode();
    if (currentMode == ShortwavDSP::ModuleMode::Normal)
    {
      dsp.setModuleMode(ShortwavDSP::ModuleMode::ReelSelect);
    }
    else if (currentMode == ShortwavDSP::ModuleMode::ReelSelect)
    {
      dsp.setModuleMode(ShortwavDSP::ModuleMode::Normal);
    }
    spliceButtonHoldTime = kComboWindowTime + 1.0f;
    recButtonHoldTime = kComboWindowTime + 1.0f;
  }

  // SHIFT + SPLICE = Delete splice marker
  if (shiftButtonHeld && spliceButtonHeld)
  {
    if (spliceButtonHoldTime >= kLongPressTime)
    {
      // Long press: delete all markers
      dsp.deleteAllMarkers();
      updateOrganizeParamRange();
      spliceButtonHoldTime = 0.0f; // Reset to prevent repeated deletion
    }
    else if (spliceButtonHoldTime < kComboWindowTime)
    {
      // Short combo: delete current marker
      dsp.deleteCurrentMarker();
      updateOrganizeParamRange();
      spliceButtonHoldTime = kComboWindowTime + 1.0f;
    }
  }

  // SHIFT + REC = Delete splice audio
  if (shiftButtonHeld && recButtonHeld && !dsp.isRecording())
  {
    if (recButtonHoldTime >= kLongPressTime)
    {
      // Long press: clear entire reel
      dsp.clearReel();
      recButtonHoldTime = 0.0f;
    }
    else if (recButtonHoldTime < kComboWindowTime)
    {
      // Short combo: delete current splice audio
      dsp.deleteCurrentSpliceAudio();
      recButtonHoldTime = kComboWindowTime + 1.0f;
    }
  }
}

//------------------------------------------------------------------------------
// Gate/Trigger Input Processing
//------------------------------------------------------------------------------

void Tapestry::processGateInputs(const ProcessArgs& args)
{
  // CLK input
  if (inputs[CLK_INPUT].isConnected())
  {
    if (clkTrigger.process(inputs[CLK_INPUT].getVoltage(),
                           0.1f, ShortwavDSP::TapestryConfig::kGateTriggerThreshold))
    {
      dsp.onClockRising();
    }
  }
  else
  {
    dsp.onClockDisconnected();
  }

  // PLAY input (normalized HIGH when unconnected)
  if (inputs[PLAY_INPUT].isConnected())
  {
    bool playHigh = inputs[PLAY_INPUT].getVoltage() >= ShortwavDSP::TapestryConfig::kGateTriggerThreshold;
    dsp.onPlayGate(playHigh);
  }
  else
  {
    dsp.onPlayGate(true); // Normalized HIGH
  }

  // REC gate input
  if (inputs[REC_INPUT].isConnected())
  {
    if (recInputTrigger.process(inputs[REC_INPUT].getVoltage(),
                                 0.1f, ShortwavDSP::TapestryConfig::kGateTriggerThreshold))
    {
      // Toggle recording
      if (dsp.isRecording())
      {
        dsp.stopRecordingRequest(false);
      }
      else
      {
        // Clear and start fresh recording (replaces existing content)
        // In overdub mode, start from current playhead position
        size_t currentPos = static_cast<size_t>(dsp.getGrainEngine().getPlayheadPosition());
        dsp.clearAndStartRecording(false, currentPos);
      }
    }
  }

  // SPLICE gate input
  if (inputs[SPLICE_INPUT].isConnected())
  {
    if (spliceInputTrigger.process(inputs[SPLICE_INPUT].getVoltage(),
                                    0.1f, ShortwavDSP::TapestryConfig::kGateTriggerThreshold))
    {
      dsp.onSpliceTrigger(getCurrentPlaybackFrame());
      updateOrganizeParamRange();
    }
  }

  // SHIFT gate input
  if (inputs[SHIFT_INPUT].isConnected())
  {
    if (shiftInputTrigger.process(inputs[SHIFT_INPUT].getVoltage(),
                                   0.1f, ShortwavDSP::TapestryConfig::kGateTriggerThreshold))
    {
      dsp.onShiftTrigger();
    }
  }
}

//------------------------------------------------------------------------------
// LED Updates
//------------------------------------------------------------------------------

void Tapestry::updateLights(const ProcessArgs& args)
{
  // Reduce light update frequency
  static int lightDivider = 0;
  if (++lightDivider < 256)
    return;
  lightDivider = 0;

  // Vari-Speed activity windows
  const ShortwavDSP::VariSpeedState& vs = dsp.getVariSpeedState();

  float r = 0.0f, g = 0.0f, b = 0.0f;
  switch (vs.getLedColor())
  {
  case ShortwavDSP::VariSpeedState::LedColor::Red:
    r = 1.0f;
    break;
  case ShortwavDSP::VariSpeedState::LedColor::Green:
    g = 1.0f;
    break;
  case ShortwavDSP::VariSpeedState::LedColor::Amber:
    r = 1.0f;
    g = 0.7f;
    break;
  case ShortwavDSP::VariSpeedState::LedColor::BabyBlue:
    r = 0.4f;
    g = 0.8f;
    b = 1.0f;
    break;
  case ShortwavDSP::VariSpeedState::LedColor::Peach:
    r = 1.0f;
    g = 0.8f;
    b = 0.6f;
    break;
  }

  // Direction indicators
  if (vs.isForward || vs.isStopped)
  {
    lights[VARI_SPEED_RIGHT_LIGHT + 0].setBrightness(r);
    lights[VARI_SPEED_RIGHT_LIGHT + 1].setBrightness(g);
    lights[VARI_SPEED_RIGHT_LIGHT + 2].setBrightness(b);
    lights[VARI_SPEED_LEFT_LIGHT + 0].setBrightness(0.0f);
    lights[VARI_SPEED_LEFT_LIGHT + 1].setBrightness(0.0f);
    lights[VARI_SPEED_LEFT_LIGHT + 2].setBrightness(0.0f);
  }
  else
  {
    lights[VARI_SPEED_LEFT_LIGHT + 0].setBrightness(r);
    lights[VARI_SPEED_LEFT_LIGHT + 1].setBrightness(g);
    lights[VARI_SPEED_LEFT_LIGHT + 2].setBrightness(b);
    lights[VARI_SPEED_RIGHT_LIGHT + 0].setBrightness(0.0f);
    lights[VARI_SPEED_RIGHT_LIGHT + 1].setBrightness(0.0f);
    lights[VARI_SPEED_RIGHT_LIGHT + 2].setBrightness(0.0f);
  }

  // Morph indicator (opposite vari-speed LED)
  const ShortwavDSP::MorphState& ms = dsp.getMorphState();
  if (ms.isSeamless())
  {
    // Amber for seamless
    float morphR = 1.0f, morphG = 0.7f, morphB = 0.0f;
    if (vs.isForward || vs.isStopped)
    {
      lights[VARI_SPEED_LEFT_LIGHT + 0].setBrightness(morphR);
      lights[VARI_SPEED_LEFT_LIGHT + 1].setBrightness(morphG);
      lights[VARI_SPEED_LEFT_LIGHT + 2].setBrightness(morphB);
    }
    else
    {
      lights[VARI_SPEED_RIGHT_LIGHT + 0].setBrightness(morphR);
      lights[VARI_SPEED_RIGHT_LIGHT + 1].setBrightness(morphG);
      lights[VARI_SPEED_RIGHT_LIGHT + 2].setBrightness(morphB);
    }
  }
  else if (ms.hasGaps || ms.overlap > 1.5f)
  {
    // Red for gaps/overlaps
    if (vs.isForward || vs.isStopped)
    {
      lights[VARI_SPEED_LEFT_LIGHT + 0].setBrightness(1.0f);
      lights[VARI_SPEED_LEFT_LIGHT + 1].setBrightness(0.0f);
      lights[VARI_SPEED_LEFT_LIGHT + 2].setBrightness(0.0f);
    }
    else
    {
      lights[VARI_SPEED_RIGHT_LIGHT + 0].setBrightness(1.0f);
      lights[VARI_SPEED_RIGHT_LIGHT + 1].setBrightness(0.0f);
      lights[VARI_SPEED_RIGHT_LIGHT + 2].setBrightness(0.0f);
    }
  }

  // Reel activity window
  ShortwavDSP::ReelColors::getRGBNormalized(currentReelIndex, r, g, b);

  // Flash during reel select or clock input
  bool flash = (dsp.getModuleMode() == ShortwavDSP::ModuleMode::ReelSelect) ||
               (inputs[CLK_INPUT].isConnected() && clkTrigger.isHigh());
  if (flash && ((lightDivider / 64) % 2 == 0))
  {
    r *= 0.3f;
    g *= 0.3f;
    b *= 0.3f;
  }

  lights[REEL_LIGHT + 0].setBrightness(r);
  lights[REEL_LIGHT + 1].setBrightness(g);
  lights[REEL_LIGHT + 2].setBrightness(b);

  // Splice activity window
  int spliceIdx = dsp.getSpliceManager().getCurrentIndex();
  ShortwavDSP::ReelColors::getRGBNormalized(spliceIdx, r, g, b);
  lights[SPLICE_LIGHT + 0].setBrightness(r);
  lights[SPLICE_LIGHT + 1].setBrightness(g);
  lights[SPLICE_LIGHT + 2].setBrightness(b);

  // CV output level indicator
  float envLevel = dsp.getEnvelopeValue() / ShortwavDSP::TapestryConfig::kCvOutMax;
  lights[CV_OUT_LIGHT + 0].setBrightness(envLevel);
  lights[CV_OUT_LIGHT + 1].setBrightness(envLevel * 0.8f);
  lights[CV_OUT_LIGHT + 2].setBrightness(0.0f);

  // Button LEDs
  lights[REC_LED].setBrightness(dsp.isRecording() ? 1.0f :
                                 (dsp.isWaitingForClock() ? 0.5f : 0.0f));

  // Splice LED: light at end of splice
  lights[SPLICE_LED].setBrightness(eosgPulse.remaining > 0.0f ? 1.0f : 0.0f);

  // Shift LED: 
  // - OFF when only 1 splice (nothing to shift to)
  // - ON when multiple splices available
  // - Blink when SD busy
  // - Flash when shift button pressed
  bool sdBusy = fileLoading.load() || fileSaving.load();
  size_t numSplices = dsp.getSpliceManager().getNumSplices();
  bool hasPendingShift = dsp.getSpliceManager().hasPending();
  
  if (sdBusy)
  {
    // Blink during file operations
    lights[SHIFT_LED].setBrightness(((lightDivider / 32) % 2) ? 1.0f : 0.0f);
  }
  else if (hasPendingShift)
  {
    // Fast blink when shift is pending (waiting for end of splice)
    lights[SHIFT_LED].setBrightness(((lightDivider / 16) % 2) ? 1.0f : 0.3f);
  }
  else if (numSplices > 1)
  {
    // Solid when multiple splices available
    lights[SHIFT_LED].setBrightness(1.0f);
  }
  else
  {
    // Dim when only one splice (shift not available)
    lights[SHIFT_LED].setBrightness(0.2f);
  }

  // Clear Splices LED: dim when splices exist, off when empty
  lights[CLEAR_SPLICES_LED].setBrightness(dsp.getSpliceManager().isEmpty() ? 0.0f : 0.3f);

  // Splice Count LED: Show current mode brightness (0.33, 0.66, 1.0 for 4, 8, 16)
  float spliceCountBrightness = (spliceCountMode + 1) * 0.33f;
  lights[SPLICE_COUNT_LED].setBrightness(spliceCountBrightness);
}

//------------------------------------------------------------------------------
// Splice Count Management
//------------------------------------------------------------------------------

void Tapestry::setSpliceCount(int n)
{
  if (n < 1)
    return;

  const auto& buffer = dsp.getBuffer();
  size_t totalFrames = buffer.getUsedFrames();
  
  if (totalFrames == 0)
    return;

  // Clear existing splices
  dsp.deleteAllMarkers();

  // Create n evenly-spaced splice markers
  if (n > 1)
  {
    for (int i = 0; i < n; i++)
    {
      // Calculate position for this splice
      // Distribute evenly: position = (i * totalFrames) / n
      size_t splicePosition = (i * totalFrames) / n;
      
      // Ensure we don't place a marker at the very end
      if (splicePosition >= totalFrames)
        splicePosition = totalFrames - 1;
      
      // Add splice marker at this position
      dsp.getSpliceManager().addMarker(splicePosition);
    }
  }
  else if (n == 1)
  {
    // Single splice at the beginning
    dsp.getSpliceManager().addMarker(0);
  }
  
  // Update organize parameter range
  updateOrganizeParamRange();
}

//------------------------------------------------------------------------------
// Organize Parameter Update
//------------------------------------------------------------------------------

void Tapestry::updateOrganizeParamRange()
{
  size_t numSplices = dsp.getSpliceManager().getNumSplices();
  auto* organizeParamQuantity = paramQuantities[ORGANIZE_PARAM];
  
  // Store the current normalized position (0.0-1.0) before updating range
  float normalizedPosition = 0.0f;
  if (organizeParamQuantity->maxValue > 0.0f)
  {
    normalizedPosition = organizeParamQuantity->getValue() / organizeParamQuantity->maxValue;
  }
  
  if (numSplices > 0)
  {
    float newMax = static_cast<float>(numSplices);
    organizeParamQuantity->maxValue = newMax;
    // Set value to maintain the same proportional position
    organizeParamQuantity->setValue(normalizedPosition * newMax);
  }
  else
  {
    // No splices, set max to 1 and value to 0
    organizeParamQuantity->maxValue = 1.0f;
    organizeParamQuantity->setValue(0.0f);
  }
}

//------------------------------------------------------------------------------
// File I/O
//------------------------------------------------------------------------------

void Tapestry::loadFileAsync(const std::string& path)
{
  if (fileLoading.load() || fileSaving.load())
    return;

  fileLoading.store(true);

  std::thread([this, path]() {
    std::lock_guard<std::mutex> lock(fileMutex);

    // Load WAV file using a simple implementation
    // TODO: Implement proper WAV loading with marker support
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file)
    {
      // Basic WAV header parsing (simplified)
      // In production, use the full WavPlayer::loadFile implementation
      fseek(file, 0, SEEK_END);
      long fileSize = ftell(file);
      fseek(file, 44, SEEK_SET); // Skip WAV header

      size_t dataSize = fileSize - 44;
      size_t numSamples = dataSize / sizeof(int16_t);
      size_t numFrames = numSamples / 2; // Stereo

      std::vector<float> tempBuffer(numFrames * 2);
      std::vector<int16_t> rawData(numSamples);

      if (fread(rawData.data(), sizeof(int16_t), numSamples, file) == numSamples)
      {
        // Convert int16 to float
        for (size_t i = 0; i < numSamples; i++)
        {
          tempBuffer[i] = rawData[i] / 32768.0f;
        }

        dsp.loadReel(tempBuffer.data(), numFrames);
        currentFilePath = path;

        // Extract filename
        size_t lastSlash = path.find_last_of("/\\");
        currentFileName = (lastSlash != std::string::npos) ?
                          path.substr(lastSlash + 1) : path;
      }

      fclose(file);
    }

    fileLoading.store(false);
  }).detach();
}

void Tapestry::saveFileAsync(const std::string& path)
{
  if (fileLoading.load() || fileSaving.load())
    return;

  fileSaving.store(true);

  std::thread([this, path]() {
    std::lock_guard<std::mutex> lock(fileMutex);

    const auto& buffer = dsp.getBuffer();
    size_t numFrames = buffer.getUsedFrames();

    if (numFrames > 0)
    {
      FILE* file = std::fopen(path.c_str(), "wb");
      if (file)
      {
        // Write WAV header
        uint32_t sampleRate = 48000;
        uint16_t numChannels = 2;
        uint16_t bitsPerSample = 16;
        uint32_t dataSize = numFrames * numChannels * (bitsPerSample / 8);
        uint32_t fileSize = 36 + dataSize;

        // RIFF header
        fwrite("RIFF", 1, 4, file);
        fwrite(&fileSize, 4, 1, file);
        fwrite("WAVE", 1, 4, file);

        // fmt chunk
        fwrite("fmt ", 1, 4, file);
        uint32_t fmtSize = 16;
        fwrite(&fmtSize, 4, 1, file);
        uint16_t audioFormat = 1; // PCM
        fwrite(&audioFormat, 2, 1, file);
        fwrite(&numChannels, 2, 1, file);
        fwrite(&sampleRate, 4, 1, file);
        uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        fwrite(&byteRate, 4, 1, file);
        uint16_t blockAlign = numChannels * (bitsPerSample / 8);
        fwrite(&blockAlign, 2, 1, file);
        fwrite(&bitsPerSample, 2, 1, file);

        // data chunk
        fwrite("data", 1, 4, file);
        fwrite(&dataSize, 4, 1, file);

        // Write audio data
        const float* src = buffer.data();
        for (size_t i = 0; i < numFrames * 2; i++)
        {
          int16_t sample = static_cast<int16_t>(src[i] * 32767.0f);
          fwrite(&sample, sizeof(int16_t), 1, file);
        }

        // TODO: Write CUE chunk with splice markers

        fclose(file);
        currentFilePath = path;
      }
    }

    fileSaving.store(false);
  }).detach();
}

//------------------------------------------------------------------------------
// JSON Serialization
//------------------------------------------------------------------------------

json_t* Tapestry::dataToJson()
{
  json_t* rootJ = json_object();

  // Save current reel index
  json_object_set_new(rootJ, "reelIndex", json_integer(currentReelIndex));

  // Save file path
  if (!currentFilePath.empty())
  {
    json_object_set_new(rootJ, "filePath", json_string(currentFilePath.c_str()));
  }

  // Save auto-level gain
  json_object_set_new(rootJ, "autoLevelGain", json_real(dsp.getAutoLevelGain()));

  // Save splice markers
  std::vector<size_t> markerPositions = dsp.getMarkerPositions();
  if (!markerPositions.empty())
  {
    json_t* markersJ = json_array();
    for (size_t pos : markerPositions)
    {
      json_array_append_new(markersJ, json_integer(pos));
    }
    json_object_set_new(rootJ, "spliceMarkers", markersJ);
  }

  // Save current splice index
  json_object_set_new(rootJ, "currentSpliceIndex", json_integer(dsp.getSpliceManager().getCurrentIndex()));

  // Save splice count mode
  json_object_set_new(rootJ, "spliceCountMode", json_integer(spliceCountMode));

  // Save waveform color
  json_object_set_new(rootJ, "waveformColor", json_integer(static_cast<int>(waveformColor)));

  return rootJ;
}

void Tapestry::dataFromJson(json_t* rootJ)
{
  // Load reel index
  json_t* reelIndexJ = json_object_get(rootJ, "reelIndex");
  if (reelIndexJ)
  {
    currentReelIndex = json_integer_value(reelIndexJ);
  }

  // Load file path and reload file
  json_t* filePathJ = json_object_get(rootJ, "filePath");
  if (filePathJ)
  {
    std::string path = json_string_value(filePathJ);
    if (!path.empty())
    {
      loadFileAsync(path);
    }
  }

  // Load splice markers (after file is loaded, this will be applied in process())
  json_t* markersJ = json_object_get(rootJ, "spliceMarkers");
  if (markersJ && json_is_array(markersJ))
  {
    std::vector<size_t> markerPositions;
    size_t arraySize = json_array_size(markersJ);
    for (size_t i = 0; i < arraySize; i++)
    {
      json_t* markerJ = json_array_get(markersJ, i);
      if (json_is_integer(markerJ))
      {
        markerPositions.push_back(json_integer_value(markerJ));
      }
    }
    pendingSpliceMarkers_ = markerPositions;
  }

  // Load current splice index
  json_t* spliceIndexJ = json_object_get(rootJ, "currentSpliceIndex");
  if (spliceIndexJ)
  {
    pendingSpliceIndex_ = json_integer_value(spliceIndexJ);
  }

  // Load splice count mode
  json_t* spliceCountModeJ = json_object_get(rootJ, "spliceCountMode");
  if (spliceCountModeJ)
  {
    int mode = json_integer_value(spliceCountModeJ);
    if (mode >= 0 && mode < kNumSpliceCountOptions)
    {
      spliceCountMode = mode;
    }
  }

  // Load waveform color
  json_t* waveformColorJ = json_object_get(rootJ, "waveformColor");
  if (waveformColorJ)
  {
    int colorInt = json_integer_value(waveformColorJ);
    if (colorInt >= 0 && colorInt < static_cast<int>(WaveformColor::NUM_COLORS))
    {
      waveformColor = static_cast<WaveformColor>(colorInt);
    }
  }
}

//------------------------------------------------------------------------------
// Reel Display Implementation
//------------------------------------------------------------------------------

void ReelDisplay::draw(const DrawArgs& args)
{
  // Background
  nvgBeginPath(args.vg);
  nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
  nvgFillColor(args.vg, nvgRGB(20, 20, 25));
  nvgFill(args.vg);

  // Border
  nvgStrokeColor(args.vg, nvgRGB(60, 60, 70));
  nvgStrokeWidth(args.vg, 1.0f);
  nvgStroke(args.vg);

  if (!module)
    return;

  drawWaveform(args);
  drawSpliceMarkers(args);
  drawGeneWindow(args);
  drawPlayhead(args);
  drawHoverIndicator(args);
}

void ReelDisplay::drawWaveform(const DrawArgs& args)
{
  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return;

  size_t usedFrames = buffer.getUsedFrames();
  const float* data = buffer.data();

  float centerY = box.size.y * 0.5f;
  float maxBarHeight = box.size.y * 0.45f;
  
  // SoundCloud-style bar settings
  const float barWidth = 2.5f;
  const float barGap = 1.0f;
  const float barSpacing = barWidth + barGap;
  const float cornerRadius = 1.0f;
  
  // Calculate number of bars that fit in display
  int numBars = static_cast<int>(box.size.x / barSpacing);
  if (numBars <= 0) return;
  
  // Use member variable hoverX for hover effect (set by onHover)
  float currentHoverX = isHovering ? this->hoverX : -1.0f;
  
  // Pre-compute peak values for each bar
  std::vector<float> peaks(numBars, 0.0f);
  for (int barIdx = 0; barIdx < numBars; barIdx++)
  {
    size_t startFrame = static_cast<size_t>(barIdx * usedFrames / numBars);
    size_t endFrame = static_cast<size_t>((barIdx + 1) * usedFrames / numBars);
    endFrame = std::min(endFrame, usedFrames);
    
    float peak = 0.0f;
    for (size_t i = startFrame; i < endFrame; i++)
    {
      // Average L+R channels for mono visualization
      float sampleL = std::fabs(data[i * 2]);
      float sampleR = std::fabs(data[i * 2 + 1]);
      float sample = (sampleL + sampleR) * 0.5f;
      peak = std::max(peak, sample);
    }
    peaks[barIdx] = peak;
  }
  
  // Draw each bar with SoundCloud-style appearance
  for (int barIdx = 0; barIdx < numBars; barIdx++)
  {
    float x = barIdx * barSpacing;
    float peak = peaks[barIdx];
    
    // Apply logarithmic scaling for better visual distribution
    float barHeight = std::pow(peak, 0.7f) * maxBarHeight;
    barHeight = std::max(barHeight, 2.0f); // Minimum bar height
    
    // Check if bar is under hover
    bool isBarHovered = (currentHoverX >= x && currentHoverX < x + barSpacing);
    
    // Draw drop shadow for depth
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x + 0.5f, centerY - barHeight + 0.5f, 
                   barWidth, barHeight * 2.0f, cornerRadius);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 20));
    nvgFill(args.vg);
    
    // Get user-selected waveform color
    int r, g, b;
    module->getWaveformColorRGB(r, g, b);
    
    // Create gradient from selected color (lighter at top, darker at bottom)
    int r1 = r, g1 = g, b1 = b;  // Top color
    int r2 = static_cast<int>(r * 0.7f);  // Bottom color (darker)
    int g2 = static_cast<int>(g * 0.7f);
    int b2 = static_cast<int>(b * 0.7f);
    
    NVGpaint gradient = nvgLinearGradient(args.vg, 
                                          x, centerY - barHeight,
                                          x, centerY + barHeight,
                                          nvgRGBA(r1, g1, b1, isBarHovered ? 255 : 200),
                                          nvgRGBA(r2, g2, b2, isBarHovered ? 255 : 180));
    
    // Draw top bar (positive amplitude)
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, centerY - barHeight, barWidth, barHeight, cornerRadius);
    nvgFillPaint(args.vg, gradient);
    nvgFill(args.vg);
    
    // Draw bottom bar (negative amplitude - mirrored)
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, centerY, barWidth, barHeight, cornerRadius);
    nvgFillPaint(args.vg, gradient);
    nvgFill(args.vg);
    
    // Add subtle highlight on hover
    if (isBarHovered)
    {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, x, centerY - barHeight, barWidth, barHeight * 2.0f, cornerRadius);
      nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 80));
      nvgStrokeWidth(args.vg, 0.5f);
      nvgStroke(args.vg);
    }
    
    // Draw rounded caps for enhanced appearance
    if (barHeight > 3.0f)
    {
      // Top cap (lighter color)
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, x + barWidth * 0.5f, centerY - barHeight, barWidth * 0.5f);
      nvgFillColor(args.vg, nvgRGBA(r1, g1, b1, isBarHovered ? 255 : 220));
      nvgFill(args.vg);
      
      // Bottom cap (darker color)
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, x + barWidth * 0.5f, centerY + barHeight, barWidth * 0.5f);
      nvgFillColor(args.vg, nvgRGBA(r2, g2, b2, isBarHovered ? 255 : 200));
      nvgFill(args.vg);
    }
  }
  
  // Draw center line for reference
  nvgBeginPath(args.vg);
  nvgMoveTo(args.vg, 0, centerY);
  nvgLineTo(args.vg, box.size.x, centerY);
  nvgStrokeColor(args.vg, nvgRGBA(100, 100, 120, 60));
  nvgStrokeWidth(args.vg, 0.5f);
  nvgStroke(args.vg);
}

void ReelDisplay::drawSpliceMarkers(const DrawArgs& args)
{
  const auto& buffer = module->dsp.getBuffer();
  const auto& spliceManager = module->dsp.getSpliceManager();

  if (buffer.isEmpty())
    return;

  size_t usedFrames = buffer.getUsedFrames();
  const auto& splices = spliceManager.getAllSplices();

  for (size_t i = 0; i < splices.size(); i++)
  {
    float x = static_cast<float>(splices[i].startFrame) / usedFrames * box.size.x;

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, x, 0);
    nvgLineTo(args.vg, x, box.size.y);

    if (static_cast<int>(i) == spliceManager.getCurrentIndex())
    {
      nvgStrokeColor(args.vg, nvgRGB(255, 200, 50));
      nvgStrokeWidth(args.vg, 2.0f);
    }
    else
    {
      nvgStrokeColor(args.vg, nvgRGB(200, 150, 50));
      nvgStrokeWidth(args.vg, 1.0f);
    }
    nvgStroke(args.vg);
  }
}

void ReelDisplay::drawPlayhead(const DrawArgs& args)
{
  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return;

  size_t usedFrames = buffer.getUsedFrames();
  double playhead = module->dsp.getGrainEngine().getPlayheadPosition();

  // Clamp playhead to valid range
  if (playhead < 0.0)
    playhead = 0.0;
  if (playhead >= static_cast<double>(usedFrames))
    playhead = static_cast<double>(usedFrames) - 1.0;

  float x = static_cast<float>(playhead / usedFrames) * box.size.x;
  x = std::max(0.0f, std::min(x, box.size.x));

  nvgBeginPath(args.vg);
  nvgMoveTo(args.vg, x, 0);
  nvgLineTo(args.vg, x, box.size.y);
  nvgStrokeColor(args.vg, nvgRGB(255, 80, 80));
  nvgStrokeWidth(args.vg, 2.0f);
  nvgStroke(args.vg);
}

void ReelDisplay::drawGeneWindow(const DrawArgs& args)
{
  // Draw current gene window as a highlighted region
  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return;

  // TODO: Get gene window bounds from grain engine
  // For now, just draw a placeholder based on gene size param
}

void ReelDisplay::drawHoverIndicator(const DrawArgs& args)
{
  if (!isHovering || hoverX < 0)
    return;

  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return;

  // Draw hover indicator line
  nvgBeginPath(args.vg);
  nvgMoveTo(args.vg, hoverX, 0);
  nvgLineTo(args.vg, hoverX, box.size.y);
  
  if (hoveredSpliceIndex >= 0)
  {
    // Hovering over existing splice marker - show red indicator for deletion
    nvgStrokeColor(args.vg, nvgRGBA(255, 100, 100, 180));
    nvgStrokeWidth(args.vg, 3.0f);
  }
  else
  {
    // Hovering over waveform - show green indicator for new splice
    nvgStrokeColor(args.vg, nvgRGBA(100, 255, 100, 150));
    nvgStrokeWidth(args.vg, 2.0f);
  }
  nvgStroke(args.vg);

  // Draw a small triangle at the top to indicate click position
  float triSize = 5.0f;
  nvgBeginPath(args.vg);
  nvgMoveTo(args.vg, hoverX, 0);
  nvgLineTo(args.vg, hoverX - triSize, -triSize);
  nvgLineTo(args.vg, hoverX + triSize, -triSize);
  nvgClosePath(args.vg);
  
  if (hoveredSpliceIndex >= 0)
  {
    nvgFillColor(args.vg, nvgRGBA(255, 100, 100, 200));
  }
  else
  {
    nvgFillColor(args.vg, nvgRGBA(100, 255, 100, 200));
  }
  nvgFill(args.vg);
}

//------------------------------------------------------------------------------
// Mouse Event Handlers
//------------------------------------------------------------------------------

void ReelDisplay::onButton(const ButtonEvent& e)
{
  OpaqueWidget::onButton(e);
  
  if (!module)
    return;

  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return;

  // Only handle press events (not release)
  if (e.action != GLFW_PRESS)
    return;

  // Left click: Create splice at position OR select existing splice
  if (e.button == GLFW_MOUSE_BUTTON_LEFT)
  {
    int spliceIdx = getSpliceIndexAtPosition(e.pos.x);
    
    if (spliceIdx >= 0)
    {
      // Clicked on existing splice marker - select it
      module->dsp.getSpliceManager().setCurrentIndex(spliceIdx);
    }
    else
    {
      // Create new splice at click position
      size_t frame = xPositionToFrame(e.pos.x);
      module->dsp.onSpliceTrigger(frame);
      module->updateOrganizeParamRange();
    }
    e.consume(this);
  }
  // Right click: Delete splice marker if hovering over one
  else if (e.button == GLFW_MOUSE_BUTTON_RIGHT)
  {
    int spliceIdx = getSpliceIndexAtPosition(e.pos.x);
    
    if (spliceIdx > 0)  // Can't delete first splice (index 0)
    {
      // Delete the marker at this specific index
      module->dsp.getSpliceManager().deleteMarkerAtIndex(spliceIdx);
      module->updateOrganizeParamRange();
      e.consume(this);
    }
    // Don't consume the event if we didn't delete anything
    // This allows the context menu to open
  }
}

void ReelDisplay::onHover(const HoverEvent& e)
{
  OpaqueWidget::onHover(e);
  
  isHovering = true;
  hoverX = e.pos.x;
  
  // Check if hovering over a splice marker
  hoveredSpliceIndex = getSpliceIndexAtPosition(e.pos.x);
  
  e.consume(this);
}

void ReelDisplay::onLeave(const LeaveEvent& e)
{
  OpaqueWidget::onLeave(e);
  
  isHovering = false;
  hoverX = -1.0f;
  hoveredSpliceIndex = -1;
}

void ReelDisplay::onDragHover(const DragHoverEvent& e)
{
  OpaqueWidget::onDragHover(e);
  
  // Update hover position during drag operations
  isHovering = true;
  hoverX = e.pos.x;
  hoveredSpliceIndex = getSpliceIndexAtPosition(e.pos.x);
}

//------------------------------------------------------------------------------
// Helper Methods
//------------------------------------------------------------------------------

size_t ReelDisplay::xPositionToFrame(float x) const
{
  if (!module)
    return 0;

  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return 0;

  size_t usedFrames = buffer.getUsedFrames();
  
  // Clamp x to valid range
  x = std::max(0.0f, std::min(x, box.size.x));
  
  // Convert x position to frame
  float normalized = x / box.size.x;
  return static_cast<size_t>(normalized * usedFrames);
}

float ReelDisplay::frameToXPosition(size_t frame) const
{
  if (!module)
    return 0.0f;

  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return 0.0f;

  size_t usedFrames = buffer.getUsedFrames();
  if (usedFrames == 0)
    return 0.0f;

  float normalized = static_cast<float>(frame) / static_cast<float>(usedFrames);
  return normalized * box.size.x;
}

int ReelDisplay::getSpliceIndexAtPosition(float x) const
{
  if (!module)
    return -1;

  const auto& buffer = module->dsp.getBuffer();
  if (buffer.isEmpty())
    return -1;

  const auto& spliceManager = module->dsp.getSpliceManager();
  const auto& splices = spliceManager.getAllSplices();
  
  if (splices.empty())
    return -1;

  size_t usedFrames = buffer.getUsedFrames();

  // Check each splice marker to see if x is within hit range
  for (size_t i = 0; i < splices.size(); i++)
  {
    // Skip first splice at frame 0 (can't delete start of buffer)
    if (splices[i].startFrame == 0)
      continue;
      
    float markerX = static_cast<float>(splices[i].startFrame) / usedFrames * box.size.x;
    
    if (std::fabs(x - markerX) <= kSpliceHitWidth)
    {
      return static_cast<int>(i);
    }
  }

  return -1;
}

//------------------------------------------------------------------------------
// Widget Implementation
//------------------------------------------------------------------------------

TapestryWidget::TapestryWidget(Tapestry* module)
{
  setModule(module);
  this->module = module;

  // 20HP panel
  setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SWV_21HP_PANEL.svg")));

  // Screws
  addChild(createWidget<ScrewSilver>(Vec(0, 0)));
  addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

  // Reel display
  ReelDisplay* display = new ReelDisplay();
  display->module = module;
  display->box.pos = Vec(10, 25);
  display->box.size = Vec(box.size.x - 20, 60);
  addChild(display);
  if (module)
  {
    module->reelDisplay = display;
  }

  // Audio inputs (top left)
  float yPos = 95;
  addInput(createInputCentered<PJ301MPort>(Vec(25, yPos), module, Tapestry::AUDIO_IN_L));
  addInput(createInputCentered<PJ301MPort>(Vec(55, yPos), module, Tapestry::AUDIO_IN_R));

  // Audio outputs (top right)
  addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 55, yPos), module, Tapestry::AUDIO_OUT_L));
  addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 25, yPos), module, Tapestry::AUDIO_OUT_R));

  // S.O.S. knob and CV
  yPos = 135;
  addParam(createParamCentered<RoundBlackKnob>(Vec(35, yPos), module, Tapestry::SOS_PARAM));
  addInput(createInputCentered<PJ301MPort>(Vec(35, yPos + 35), module, Tapestry::SOS_CV_INPUT));

  // Gene Size knob, attenuverter, and CV
  addParam(createParamCentered<RoundBlackKnob>(Vec(95, yPos), module, Tapestry::GENE_SIZE_PARAM));
  addParam(createParamCentered<Trimpot>(Vec(95, yPos + 28), module, Tapestry::GENE_SIZE_CV_ATTEN));
  addInput(createInputCentered<PJ301MPort>(Vec(95, yPos + 55), module, Tapestry::GENE_SIZE_CV_INPUT));

  // Morph knob and CV
  addParam(createParamCentered<RoundBlackKnob>(Vec(155, yPos), module, Tapestry::MORPH_PARAM));
  addInput(createInputCentered<PJ301MPort>(Vec(155, yPos + 35), module, Tapestry::MORPH_CV_INPUT));

  // Slide knob, attenuverter, and CV
  addParam(createParamCentered<RoundBlackKnob>(Vec(215, yPos), module, Tapestry::SLIDE_PARAM));
  addParam(createParamCentered<Trimpot>(Vec(215, yPos + 28), module, Tapestry::SLIDE_CV_ATTEN));
  addInput(createInputCentered<PJ301MPort>(Vec(215, yPos + 55), module, Tapestry::SLIDE_CV_INPUT));

  // Vari-Speed section
  yPos = 230;

  // Activity windows (RGB LEDs)
  addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(
      Vec(55, yPos), module, Tapestry::VARI_SPEED_LEFT_LIGHT));
  addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(
      Vec(box.size.x - 55, yPos), module, Tapestry::VARI_SPEED_RIGHT_LIGHT));

  // Vari-Speed knob (center)
  addParam(createParamCentered<RoundLargeBlackKnob>(Vec(box.size.x / 2, yPos), module, Tapestry::VARI_SPEED_PARAM));

  // Vari-Speed CV
  yPos = 270;
  addParam(createParamCentered<Trimpot>(Vec(box.size.x / 2 - 30, yPos), module, Tapestry::VARI_SPEED_CV_ATTEN));
  addInput(createInputCentered<PJ301MPort>(Vec(box.size.x / 2 + 30, yPos), module, Tapestry::VARI_SPEED_CV_INPUT));

  // Organize knob and CV
  addParam(createParamCentered<RoundBlackKnob>(Vec(box.size.x - 45, yPos), module, Tapestry::ORGANIZE_PARAM));
  addInput(createInputCentered<PJ301MPort>(Vec(box.size.x - 45, yPos + 35), module, Tapestry::ORGANIZE_CV_INPUT));

  // Activity LEDs row
  yPos = 305;
  addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(Vec(40, yPos), module, Tapestry::REEL_LIGHT));
  addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(Vec(100, yPos), module, Tapestry::SPLICE_LIGHT));
  addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(Vec(160, yPos), module, Tapestry::CV_OUT_LIGHT));

  // Gate inputs row
  yPos = 335;
  addInput(createInputCentered<PJ301MPort>(Vec(25, yPos), module, Tapestry::CLK_INPUT));
  addInput(createInputCentered<PJ301MPort>(Vec(60, yPos), module, Tapestry::PLAY_INPUT));
  addInput(createInputCentered<PJ301MPort>(Vec(95, yPos), module, Tapestry::REC_INPUT));
  addInput(createInputCentered<PJ301MPort>(Vec(130, yPos), module, Tapestry::SPLICE_INPUT));
  addInput(createInputCentered<PJ301MPort>(Vec(165, yPos), module, Tapestry::SHIFT_INPUT));

  // CV and EOSG outputs
  addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 60, yPos), module, Tapestry::EOSG_OUTPUT));
  addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 25, yPos), module, Tapestry::CV_OUTPUT));

  // Buttons with LEDs
  yPos = 365;
  addParam(createParamCentered<LEDButton>(Vec(95, yPos), module, Tapestry::REC_BUTTON));
  addChild(createLightCentered<MediumLight<RedLight>>(Vec(95, yPos), module, Tapestry::REC_LED));

  addParam(createParamCentered<LEDButton>(Vec(130, yPos), module, Tapestry::SPLICE_BUTTON));
  addChild(createLightCentered<MediumLight<YellowLight>>(Vec(130, yPos), module, Tapestry::SPLICE_LED));

  addParam(createParamCentered<LEDButton>(Vec(165, yPos), module, Tapestry::SHIFT_BUTTON));
  addChild(createLightCentered<MediumLight<GreenLight>>(Vec(165, yPos), module, Tapestry::SHIFT_LED));

  // Clear Splices button to the right
  addParam(createParamCentered<LEDButton>(Vec(200, yPos), module, Tapestry::CLEAR_SPLICES_BUTTON));
  addChild(createLightCentered<MediumLight<WhiteLight>>(Vec(200, yPos), module, Tapestry::CLEAR_SPLICES_LED));

  // Splice Count Toggle button (next to Clear Splices)
  addParam(createParamCentered<LEDButton>(Vec(235, yPos), module, Tapestry::SPLICE_COUNT_TOGGLE_BUTTON));
  addChild(createLightCentered<MediumLight<BlueLight>>(Vec(235, yPos), module, Tapestry::SPLICE_COUNT_LED));

  // Overdub toggle switch (small switch near record button)
  addParam(createParamCentered<CKSS>(Vec(60, 365), module, Tapestry::OVERDUB_TOGGLE));
}

void TapestryWidget::appendContextMenu(Menu* menu)
{
  Tapestry* module = dynamic_cast<Tapestry*>(this->module);
  if (!module)
    return;

  menu->addChild(new MenuEntry);
  menu->addChild(createMenuLabel("Tapestry"));

  // Load reel
  struct LoadReelItem : MenuItem
  {
    Tapestry* module;
    void onAction(const event::Action& e) override
    {
      osdialog_filters* filters = osdialog_filters_parse("WAV files:wav,WAV");
      char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
      if (path)
      {
        module->loadFileAsync(path);
        std::free(path);
      }
      osdialog_filters_free(filters);
    }
  };

  LoadReelItem* loadItem = new LoadReelItem();
  loadItem->text = "Load Reel...";
  loadItem->module = module;
  menu->addChild(loadItem);

  // Save reel
  struct SaveReelItem : MenuItem
  {
    Tapestry* module;
    void onAction(const event::Action& e) override
    {
      osdialog_filters* filters = osdialog_filters_parse("WAV files:wav,WAV");
      char* path = osdialog_file(OSDIALOG_SAVE, NULL, "tapestry_reel.wav", filters);
      if (path)
      {
        module->saveFileAsync(path);
        std::free(path);
      }
      osdialog_filters_free(filters);
    }
  };

  SaveReelItem* saveItem = new SaveReelItem();
  saveItem->text = "Save Reel...";
  saveItem->module = module;
  menu->addChild(saveItem);

  // Clear reel
  struct ClearReelItem : MenuItem
  {
    Tapestry* module;
    void onAction(const event::Action& e) override
    {
      module->dsp.clearReel();
    }
  };

  ClearReelItem* clearItem = new ClearReelItem();
  clearItem->text = "Clear Reel";
  clearItem->module = module;
  menu->addChild(clearItem);

  // Show splice count mode
  menu->addChild(new MenuEntry);
  struct SpliceCountMenuItem : MenuItem
  {
    Tapestry* module;
    void onAction(const event::Action& e) override
    {
      // Cycle to next splice count mode
      module->spliceCountMode = (module->spliceCountMode + 1) % Tapestry::kNumSpliceCountOptions;
      module->setSpliceCount(Tapestry::kSpliceCountOptions[module->spliceCountMode]);
    }
  };
  
  SpliceCountMenuItem* spliceCountItem = new SpliceCountMenuItem();
  int currentCount = module->getCurrentSpliceCount();
  spliceCountItem->text = string::f("Splice Count: %d (click to cycle)", currentCount);
  spliceCountItem->module = module;
  menu->addChild(spliceCountItem);

  // Waveform color selection submenu
  menu->addChild(new MenuEntry);
  
  struct WaveformColorItem : MenuItem
  {
    Tapestry* module;
    Tapestry::WaveformColor color;
    
    void onAction(const event::Action& e) override
    {
      module->waveformColor = color;
    }
  };
  
  struct WaveformColorMenu : MenuItem
  {
    Tapestry* module;
    
    Menu* createChildMenu() override
    {
      Menu* submenu = new Menu;
      
      const char* colorNames[] = {"Red", "Amber", "Green", "Baby Blue", "Peach", "Pink", "White"};
      for (int i = 0; i < static_cast<int>(Tapestry::WaveformColor::NUM_COLORS); i++)
      {
        WaveformColorItem* colorItem = new WaveformColorItem();
        colorItem->text = colorNames[i];
        colorItem->module = module;
        colorItem->color = static_cast<Tapestry::WaveformColor>(i);
        colorItem->rightText = (module->waveformColor == colorItem->color) ? "✓" : "";
        submenu->addChild(colorItem);
      }
      
      return submenu;
    }
  };
  
  WaveformColorMenu* colorMenu = new WaveformColorMenu();
  colorMenu->text = "Waveform Color";
  colorMenu->rightText = RIGHT_ARROW;
  colorMenu->module = module;
  menu->addChild(colorMenu);

  // Show current file info
  if (!module->currentFileName.empty())
  {
    menu->addChild(new MenuEntry);
    menu->addChild(createMenuLabel("File: " + module->currentFileName));

    const auto& buffer = module->dsp.getBuffer();
    if (!buffer.isEmpty())
    {
      char info[128];
      float duration = buffer.getDurationSeconds();
      int numSplices = static_cast<int>(module->dsp.getSpliceManager().getNumSplices());
      snprintf(info, sizeof(info), "Duration: %.1fs, Splices: %d", duration, numSplices);
      menu->addChild(createMenuLabel(info));
    }
  }
}

//------------------------------------------------------------------------------
// Model Registration
//------------------------------------------------------------------------------

Model* modelTapestry = createModel<Tapestry, TapestryWidget>("Tapestry");
