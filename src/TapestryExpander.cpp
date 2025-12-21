#include "TapestryExpander.hpp"

//------------------------------------------------------------------------------
// Constructor / Destructor
//------------------------------------------------------------------------------

TapestryExpander::TapestryExpander()
{
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    
    // Bit Crusher parameters
    configParam(CRUSH_BITS_PARAM, 1.0f, 16.0f, 16.0f, "Bit Depth", " bits");
    configParam(CRUSH_RATE_PARAM, 0.0f, 1.0f, 0.0f, "Rate Reduction", "%", 0.0f, 100.0f);
    configParam(CRUSH_MIX_PARAM, 0.0f, 1.0f, 0.0f, "Crusher Mix", "%", 0.0f, 100.0f);
    
    // Moog VCF parameters
    configParam(FILTER_CUTOFF_PARAM, 0.0f, 1.0f, 1.0f, "Filter Cutoff");
    configParam(FILTER_RESO_PARAM, 0.0f, 1.0f, 0.0f, "Resonance", "%", 0.0f, 100.0f);
    configParam(FILTER_MIX_PARAM, 0.0f, 1.0f, 0.0f, "Filter Mix", "%", 0.0f, 100.0f);
    
    // CV inputs
    configInput(CRUSH_BITS_CV_INPUT, "Bits CV");
    configInput(CRUSH_RATE_CV_INPUT, "Rate CV");
    configInput(CRUSH_MIX_CV_INPUT, "Crusher Mix CV");
    configInput(FILTER_CUTOFF_CV_INPUT, "Cutoff CV");
    configInput(FILTER_RESO_CV_INPUT, "Resonance CV");
    configInput(FILTER_MIX_CV_INPUT, "Filter Mix CV");
    
    // Audio outputs
    configOutput(AUDIO_OUT_L, "Audio L");
    configOutput(AUDIO_OUT_R, "Audio R");
    
    // Allocate message buffers for expander communication
    TapestryExpanderMessage* prodMsg = new TapestryExpanderMessage;
    TapestryExpanderMessage* consMsg = new TapestryExpanderMessage;
    
    // Initialize both messages to zero/default state
    prodMsg->expanderConnected = false;
    prodMsg->audioL = prodMsg->audioR = 0.0f;
    prodMsg->processedL = prodMsg->processedR = 0.0f;
    
    consMsg->expanderConnected = false;
    consMsg->audioL = consMsg->audioR = 0.0f;
    consMsg->processedL = consMsg->processedR = 0.0f;
    
    leftExpander.producerMessage = prodMsg;
    leftExpander.consumerMessage = consMsg;
    
    // Initialize sample rate
    onSampleRateChange();
}

TapestryExpander::~TapestryExpander()
{
    delete static_cast<TapestryExpanderMessage*>(leftExpander.producerMessage);
    delete static_cast<TapestryExpanderMessage*>(leftExpander.consumerMessage);
}

//------------------------------------------------------------------------------
// Sample Rate Change
//------------------------------------------------------------------------------

void TapestryExpander::onSampleRateChange()
{
    sampleRate_ = APP->engine->getSampleRate();
    
    // Initialize parameter smoothers (5ms smoothing time)
    float smoothTimeMs = 5.0f;
    smoothBits_.setSmoothTime(smoothTimeMs, sampleRate_);
    smoothRate_.setSmoothTime(smoothTimeMs, sampleRate_);
    smoothCrushMix_.setSmoothTime(smoothTimeMs, sampleRate_);
    smoothCutoff_.setSmoothTime(smoothTimeMs, sampleRate_);
    smoothReso_.setSmoothTime(smoothTimeMs, sampleRate_);
    smoothFilterMix_.setSmoothTime(smoothTimeMs, sampleRate_);
    
    // Initialize with current param values
    smoothBits_.setImmediate(params[CRUSH_BITS_PARAM].getValue());
    smoothRate_.setImmediate(params[CRUSH_RATE_PARAM].getValue());
    smoothCrushMix_.setImmediate(params[CRUSH_MIX_PARAM].getValue());
    smoothCutoff_.setImmediate(params[FILTER_CUTOFF_PARAM].getValue());
    smoothReso_.setImmediate(params[FILTER_RESO_PARAM].getValue());
    smoothFilterMix_.setImmediate(params[FILTER_MIX_PARAM].getValue());
}

//------------------------------------------------------------------------------
// Reset
//------------------------------------------------------------------------------

void TapestryExpander::onReset()
{
    bitCrusher_.reset();
    moogFilterL_.reset();
    moogFilterR_.reset();
    
    // Reset DC blockers
    dcBlockerInL_ = dcBlockerInR_ = 0.0f;
    dcBlockerOutL_ = dcBlockerOutR_ = 0.0f;
    
    // Reset smoothers to default values
    smoothBits_.setImmediate(16.0f);
    smoothRate_.setImmediate(0.0f);
    smoothCrushMix_.setImmediate(0.0f);
    smoothCutoff_.setImmediate(1.0f);
    smoothReso_.setImmediate(0.0f);
    smoothFilterMix_.setImmediate(0.0f);
}

//------------------------------------------------------------------------------
// Get Modulated Parameter Value
//------------------------------------------------------------------------------

float TapestryExpander::getModulatedParam(int paramId, int cvId, float cvScale)
{
    float value = params[paramId].getValue();
    
    if (inputs[cvId].isConnected()) {
        float cv = inputs[cvId].getVoltage();
        value += cv * cvScale;
    }
    
    return value;
}

//------------------------------------------------------------------------------
// Main Process
//------------------------------------------------------------------------------

void TapestryExpander::process(const ProcessArgs& args)
{
    // Check for Tapestry connection on the left
    bool connected = false;
    float inputL = 0.0f;
    float inputR = 0.0f;
    
    // Check if left module is Tapestry
    if (leftExpander.module && leftExpander.module->model == modelTapestry) {
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
    
    // If not connected, output silence
    if (!connected) {
        outputs[AUDIO_OUT_L].setVoltage(0.0f);
        outputs[AUDIO_OUT_R].setVoltage(0.0f);
        return;
    }
    
    //--------------------------------------------------------------------------
    // Read and smooth parameters with CV modulation
    //--------------------------------------------------------------------------
    
    // Bit Crusher parameters
    float bitsTarget = getModulatedParam(CRUSH_BITS_PARAM, CRUSH_BITS_CV_INPUT, 1.5f);
    float rateTarget = getModulatedParam(CRUSH_RATE_PARAM, CRUSH_RATE_CV_INPUT, 0.1f);
    float crushMixTarget = getModulatedParam(CRUSH_MIX_PARAM, CRUSH_MIX_CV_INPUT, 0.1f);
    
    // Filter parameters
    float cutoffTarget = getModulatedParam(FILTER_CUTOFF_PARAM, FILTER_CUTOFF_CV_INPUT, 0.1f);
    float resoTarget = getModulatedParam(FILTER_RESO_PARAM, FILTER_RESO_CV_INPUT, 0.1f);
    float filterMixTarget = getModulatedParam(FILTER_MIX_PARAM, FILTER_MIX_CV_INPUT, 0.1f);
    
    // Clamp targets
    bitsTarget = clamp(bitsTarget, 1.0f, 16.0f);
    rateTarget = clamp(rateTarget, 0.0f, 1.0f);
    crushMixTarget = clamp(crushMixTarget, 0.0f, 1.0f);
    cutoffTarget = clamp(cutoffTarget, 0.0f, 1.0f);
    resoTarget = clamp(resoTarget, 0.0f, 1.0f);
    filterMixTarget = clamp(filterMixTarget, 0.0f, 1.0f);
    
    // Set targets for smoothing
    smoothBits_.setTarget(bitsTarget);
    smoothRate_.setTarget(rateTarget);
    smoothCrushMix_.setTarget(crushMixTarget);
    smoothCutoff_.setTarget(cutoffTarget);
    smoothReso_.setTarget(resoTarget);
    smoothFilterMix_.setTarget(filterMixTarget);
    
    // Get smoothed values
    float bits = smoothBits_.process();
    float rate = smoothRate_.process();
    float crushMix = smoothCrushMix_.process();
    float cutoff = smoothCutoff_.process();
    float reso = smoothReso_.process();
    float filterMix = smoothFilterMix_.process();
    
    //--------------------------------------------------------------------------
    // Update DSP parameters
    //--------------------------------------------------------------------------
    
    bitCrusher_.setParams(bits, rate);
    moogFilterL_.setParams(cutoff, reso, sampleRate_);
    moogFilterR_.setParams(cutoff, reso, sampleRate_);
    
    //--------------------------------------------------------------------------
    // DC Blocking on input (high-pass at ~20Hz to remove DC offset)
    //--------------------------------------------------------------------------
    
    float dcCoeff = 0.995f;  // ~20Hz cutoff at 48kHz
    float blockedInL = inputL - dcBlockerInL_ + dcCoeff * dcBlockerOutL_;
    float blockedInR = inputR - dcBlockerInR_ + dcCoeff * dcBlockerOutR_;
    dcBlockerInL_ = inputL;
    dcBlockerInR_ = inputR;
    dcBlockerOutL_ = blockedInL;
    dcBlockerOutR_ = blockedInR;
    
    //--------------------------------------------------------------------------
    // Stage 1: Bit Crusher
    //--------------------------------------------------------------------------
    
    float crushedL, crushedR;
    bitCrusher_.processStereo(blockedInL, blockedInR, crushedL, crushedR);
    
    // Dry/Wet mix for bit crusher
    float stage1L = inputL * (1.0f - crushMix) + crushedL * crushMix;
    float stage1R = inputR * (1.0f - crushMix) + crushedR * crushMix;
    
    //--------------------------------------------------------------------------
    // Stage 2: Moog VCF
    //--------------------------------------------------------------------------
    
    float filteredL = moogFilterL_.process(stage1L);
    float filteredR = moogFilterR_.process(stage1R);
    
    // Dry/Wet mix for filter
    float outputL = stage1L * (1.0f - filterMix) + filteredL * filterMix;
    float outputR = stage1R * (1.0f - filterMix) + filteredR * filterMix;
    
    // Gentle soft clipping (tanh for smooth saturation)
    outputL = std::tanh(outputL * 0.5f) * 2.0f;
    outputR = std::tanh(outputR * 0.5f) * 2.0f;
    
    // Hard limit to prevent any extreme spikes
    outputL = clamp(outputL, -1.5f, 1.5f);
    outputR = clamp(outputR, -1.5f, 1.5f);
    
    //--------------------------------------------------------------------------
    // Write processed audio back to producer message (for Tapestry to read)
    //--------------------------------------------------------------------------
    
    TapestryExpanderMessage* outMsg = 
        static_cast<TapestryExpanderMessage*>(leftExpander.producerMessage);
    outMsg->processedL = outputL;
    outMsg->processedR = outputR;
    outMsg->expanderConnected = true;
    
    // Request message flip at end of frame
    leftExpander.requestMessageFlip();
    
    //--------------------------------------------------------------------------
    // Set direct outputs (for standalone use or parallel routing)
    //--------------------------------------------------------------------------
    
    outputs[AUDIO_OUT_L].setVoltage(outputL * 5.0f);
    outputs[AUDIO_OUT_R].setVoltage(outputR * 5.0f);
}

//------------------------------------------------------------------------------
// Widget Implementation
//------------------------------------------------------------------------------

TapestryExpanderWidget::TapestryExpanderWidget(TapestryExpander* module)
{
    setModule(module);
    
    // 4HP panel (60.96px wide at 3px/mm)
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TapestryExpander.svg")));
    
    // Screws
    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    
    // Layout constants
    float colCenter = box.size.x / 2.0f;
    float colLeft = 10.0f;
    float colRight = box.size.x - 10.0f;
    
    //--------------------------------------------------------------------------
    // Connection LED (top)
    //--------------------------------------------------------------------------
    
    float yPos = 25.0f;
    addChild(createLightCentered<MediumLight<GreenLight>>(
        Vec(colCenter, yPos), module, TapestryExpander::CONNECTED_LIGHT));
    
    //--------------------------------------------------------------------------
    // Bit Crusher Section
    //--------------------------------------------------------------------------
    
    // BITS knob
    yPos = 60.0f;
    addParam(createParamCentered<RoundSmallBlackKnob>(
        Vec(colLeft + 5, yPos), module, TapestryExpander::CRUSH_BITS_PARAM));
    
    // RATE knob
    addParam(createParamCentered<RoundSmallBlackKnob>(
        Vec(colRight - 5, yPos), module, TapestryExpander::CRUSH_RATE_PARAM));
    
    // BITS CV input
    yPos = 90.0f;
    addInput(createInputCentered<PJ301MPort>(
        Vec(colLeft + 5, yPos), module, TapestryExpander::CRUSH_BITS_CV_INPUT));
    
    // RATE CV input
    addInput(createInputCentered<PJ301MPort>(
        Vec(colRight - 5, yPos), module, TapestryExpander::CRUSH_RATE_CV_INPUT));
    
    // CRUSH MIX knob
    yPos = 125.0f;
    addParam(createParamCentered<RoundSmallBlackKnob>(
        Vec(colCenter, yPos), module, TapestryExpander::CRUSH_MIX_PARAM));
    
    // CRUSH MIX CV input
    yPos = 155.0f;
    addInput(createInputCentered<PJ301MPort>(
        Vec(colCenter, yPos), module, TapestryExpander::CRUSH_MIX_CV_INPUT));
    
    //--------------------------------------------------------------------------
    // Moog VCF Section
    //--------------------------------------------------------------------------
    
    // CUTOFF knob
    yPos = 200.0f;
    addParam(createParamCentered<RoundSmallBlackKnob>(
        Vec(colLeft + 5, yPos), module, TapestryExpander::FILTER_CUTOFF_PARAM));
    
    // RESO knob
    addParam(createParamCentered<RoundSmallBlackKnob>(
        Vec(colRight - 5, yPos), module, TapestryExpander::FILTER_RESO_PARAM));
    
    // CUTOFF CV input
    yPos = 230.0f;
    addInput(createInputCentered<PJ301MPort>(
        Vec(colLeft + 5, yPos), module, TapestryExpander::FILTER_CUTOFF_CV_INPUT));
    
    // RESO CV input
    addInput(createInputCentered<PJ301MPort>(
        Vec(colRight - 5, yPos), module, TapestryExpander::FILTER_RESO_CV_INPUT));
    
    // FILTER MIX knob
    yPos = 265.0f;
    addParam(createParamCentered<RoundSmallBlackKnob>(
        Vec(colCenter, yPos), module, TapestryExpander::FILTER_MIX_PARAM));
    
    // FILTER MIX CV input
    yPos = 295.0f;
    addInput(createInputCentered<PJ301MPort>(
        Vec(colCenter, yPos), module, TapestryExpander::FILTER_MIX_CV_INPUT));
    
    //--------------------------------------------------------------------------
    // Audio Outputs (bottom)
    //--------------------------------------------------------------------------
    
    yPos = 345.0f;
    addOutput(createOutputCentered<PJ301MPort>(
        Vec(colLeft + 5, yPos), module, TapestryExpander::AUDIO_OUT_L));
    addOutput(createOutputCentered<PJ301MPort>(
        Vec(colRight - 5, yPos), module, TapestryExpander::AUDIO_OUT_R));
}

//------------------------------------------------------------------------------
// Model Registration
//------------------------------------------------------------------------------

Model* modelTapestryExpander = createModel<TapestryExpander, TapestryExpanderWidget>("TapestryExpander");
