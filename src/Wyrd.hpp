#pragma once

#include "plugin.hpp"
#include "dsp/wyrd-dsp.h"

struct Wyrd : Module
{
  enum ParamIds
  {
    STRENGTH_PARAM,
    EXTERNAL_CONSTANT_PARAM,
    ACTIVATION_CONSTANT_PARAM,
    ACTIVATION_INTERFERENCE_PARAM,
    ACTIVATION_CV_AMOUNT_PARAM,
    TONIC_COARSE_PARAM,
    TONIC_FINE_PARAM,
    TONIC_MOD_AMOUNT_PARAM,
    TONES_PARAM,
    TONES_CV_AMOUNT_PARAM,
    TIME_COARSE_PARAM,
    TIME_FINE_PARAM,
    TIME_MOD_AMOUNT_PARAM,
    TIME_CV_AMOUNT_PARAM,
    DECAY_PARAM,
    DECAY_CV_AMOUNT_PARAM,
    FILTER_PARAM,
    FILTER_CV_AMOUNT_PARAM,
    ABSORB_PARAM,
    BLEND_PARAM,
    LEVEL_PARAM,
    SPEED_PARAM,
    ANGLE_PARAM,
    SPEED_CV_AMOUNT_PARAM,
    TOUCH_ACTIVATE_PARAM,
    TOUCH_TIME_PARAM,
    TOUCH_FILTER_PARAM,
    TOUCH_ABSORB_PARAM,
    TOUCH_TONIC_PARAM,
    TOUCH_DECAY_PARAM,
    REVERB_SIZE_PARAM,
    REVERB_DECAY_PARAM,
    REVERB_DIFFUSION_PARAM,
    REVERB_TONE_PARAM,
    REVERB_MOD_PARAM,
    REVERB_BLEND_PARAM,
    NUM_PARAMS
  };

  enum InputIds
  {
    EXTERNAL_INPUT,
    BEGIN_END_INPUT,
    SPEED_CV_INPUT,
    ACTIVATION_CV_INPUT,
    TONIC_MOD_INPUT,
    TONES_CV_INPUT,
    V_OCT_INPUT,
    TIME_MOD_INPUT,
    TIME_CV_INPUT,
    TIME_UNITY_CV_INPUT,
    DECAY_CV_INPUT,
    BLEND_CV_INPUT,
    FILTER_CV_INPUT,
    ABSORB_CV_INPUT,
    NUM_INPUTS
  };

  enum OutputIds
  {
    STRENGTH_OUTPUT,
    CV1_OUTPUT,
    CV2_OUTPUT,
    AGITATION_OUTPUT,
    TONE_CORE_OUTPUT,
    SUB_HARMONICS_OUTPUT,
    MODULAR_OUTPUT,
    LINE_OUTPUT,
    NUM_OUTPUTS
  };

  enum LightIds
  {
    STRENGTH_LIGHT,
    CV1_LIGHT,
    CV2_POS_LIGHT,
    CV2_NEG_LIGHT,
    AGITATION_LIGHT,
    ACTIVATION_POS_LIGHT,
    ACTIVATION_NEG_LIGHT,
    RESULT_LIGHT,
    REVERB_LIGHT,
    NUM_LIGHTS
  };

  shortwav::wyrd::WyrdEngine engine;
  shortwav::wyrd::WyrdAmbientReverb reverb;
  int touchSource = static_cast<int>(shortwav::wyrd::TouchSource::MANUAL);
  int strengthCalibration = static_cast<int>(shortwav::wyrd::StrengthCalibration::STREGA);

  Wyrd() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configParam(STRENGTH_PARAM, 0.f, 1.f, 0.f, "Strength", "%", 0.f, 100.f);
    configParam(EXTERNAL_CONSTANT_PARAM, 0.f, 1.f, 0.f, "External constant", "%", 0.f, 100.f);
    configParam(ACTIVATION_CONSTANT_PARAM, 0.f, 1.f, 0.18f, "Activation constant", "%", 0.f, 100.f);
    configParam(ACTIVATION_INTERFERENCE_PARAM, 0.f, 1.f, 0.05f, "Activation interference", "%", 0.f, 100.f);
    configParam(ACTIVATION_CV_AMOUNT_PARAM, -1.f, 1.f, 0.f, "Activation CV attenuverter", "%", 0.f, 100.f);
    configParam(TONIC_COARSE_PARAM, 0.f, 1.f, 0.36f, "Tonic coarse", " Hz", 20.f, 500.f);
    configParam(TONIC_FINE_PARAM, -0.0833333f, 0.0833333f, 0.f, "Tonic fine", " semi", 0.f, 12.f);
    configParam(TONIC_MOD_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Tonic modulation interference", "%", 0.f, 100.f);
    configParam(TONES_PARAM, 0.f, 1.f, 0.32f, "Tones", "%", 0.f, 100.f);
    configParam(TONES_CV_AMOUNT_PARAM, -1.f, 1.f, 0.f, "Tones CV attenuverter", "%", 0.f, 100.f);
    configParam(TIME_COARSE_PARAM, 0.f, 1.f, 0.42f, "Time coarse", "%", 0.f, 100.f);
    configParam(TIME_FINE_PARAM, -0.12f, 0.12f, 0.f, "Time fine", "%", 0.f, 100.f);
    configParam(TIME_MOD_AMOUNT_PARAM, 0.f, 1.f, 0.22f, "Time modulation", "%", 0.f, 100.f);
    configParam(TIME_CV_AMOUNT_PARAM, -1.f, 1.f, 0.f, "Time CV attenuverter", "%", 0.f, 100.f);
    configParam(DECAY_PARAM, 0.f, 1.f, 0.34f, "Decay", "%", 0.f, 100.f);
    configParam(DECAY_CV_AMOUNT_PARAM, -1.f, 1.f, 0.f, "Decay CV attenuverter", "%", 0.f, 100.f);
    configParam(FILTER_PARAM, 0.f, 1.f, 0.62f, "Filter", "%", 0.f, 100.f);
    configParam(FILTER_CV_AMOUNT_PARAM, -1.f, 1.f, 0.f, "Filter CV attenuverter", "%", 0.f, 100.f);
    configParam(ABSORB_PARAM, 0.f, 1.f, 0.7f, "Absorb", "%", 0.f, 100.f);
    configParam(BLEND_PARAM, 0.f, 1.f, 0.45f, "Blend", "%", 0.f, 100.f);
    configParam(LEVEL_PARAM, 0.f, 1.f, 0.f, "Level", "%", 0.f, 100.f);
    configParam(SPEED_PARAM, 0.f, 1.f, 0.32f, "Agitation speed", "%", 0.f, 100.f);
    configParam(ANGLE_PARAM, 0.f, 1.f, 0.5f, "Agitation angle", "%", 0.f, 100.f);
    configParam(SPEED_CV_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Speed CV amount", "%", 0.f, 100.f);
    configButton(TOUCH_ACTIVATE_PARAM, "Touch activation");
    configButton(TOUCH_TIME_PARAM, "Touch time");
    configButton(TOUCH_FILTER_PARAM, "Touch filter");
    configButton(TOUCH_ABSORB_PARAM, "Touch absorb");
    configButton(TOUCH_TONIC_PARAM, "Touch tonic");
    configButton(TOUCH_DECAY_PARAM, "Touch decay");
    configParam(REVERB_SIZE_PARAM, 0.f, 1.f, 0.72f, "Reverb size", "%", 0.f, 100.f);
    configParam(REVERB_DECAY_PARAM, 0.f, 1.f, 0.78f, "Reverb decay", "%", 0.f, 100.f);
    configParam(REVERB_DIFFUSION_PARAM, 0.f, 1.f, 0.82f, "Reverb diffusion", "%", 0.f, 100.f);
    configParam(REVERB_TONE_PARAM, 0.f, 1.f, 0.55f, "Reverb tone", "%", 0.f, 100.f);
    configParam(REVERB_MOD_PARAM, 0.f, 1.f, 0.34f, "Reverb modulation", "%", 0.f, 100.f);
    configParam(REVERB_BLEND_PARAM, 0.f, 1.f, 0.25f, "Reverb blend", "%", 0.f, 100.f);

    configInput(EXTERNAL_INPUT, "External substance");
    configInput(BEGIN_END_INPUT, "Begin and End agitation");
    configInput(SPEED_CV_INPUT, "Agitation speed CV");
    configInput(ACTIVATION_CV_INPUT, "Activation CV");
    configInput(TONIC_MOD_INPUT, "Tonic modulation");
    configInput(TONES_CV_INPUT, "Tones CV");
    configInput(V_OCT_INPUT, "Tonic 1V/oct");
    configInput(TIME_MOD_INPUT, "Time modulation");
    configInput(TIME_CV_INPUT, "Time CV");
    configInput(TIME_UNITY_CV_INPUT, "Time unity CV");
    configInput(DECAY_CV_INPUT, "Decay CV");
    configInput(BLEND_CV_INPUT, "Blend CV");
    configInput(FILTER_CV_INPUT, "Filter CV");
    configInput(ABSORB_CV_INPUT, "Absorb CV");

    configOutput(STRENGTH_OUTPUT, "Strength");
    configOutput(CV1_OUTPUT, "CV1");
    configOutput(CV2_OUTPUT, "CV2");
    configOutput(AGITATION_OUTPUT, "Agitation");
    configOutput(TONE_CORE_OUTPUT, "Tone core");
    configOutput(SUB_HARMONICS_OUTPUT, "Sub harmonics");
    configOutput(MODULAR_OUTPUT, "Modular result");
    configOutput(LINE_OUTPUT, "Line result");

    configLight(STRENGTH_LIGHT, "Strength");
    configLight(CV1_LIGHT, "CV1");
    configLight(CV2_POS_LIGHT, "CV2 positive");
    configLight(CV2_NEG_LIGHT, "CV2 negative");
    configLight(AGITATION_LIGHT, "Agitation");
    configLight(ACTIVATION_POS_LIGHT, "Activation positive");
    configLight(ACTIVATION_NEG_LIGHT, "Activation negative");
    configLight(RESULT_LIGHT, "Modular result");
    configLight(REVERB_LIGHT, "Reverb");

    getParamQuantity(STRENGTH_PARAM)->description =
        "Sets external input gain and saturation before Wyrd's internal CV extraction.";
    getParamQuantity(EXTERNAL_CONSTANT_PARAM)->description =
        "Sets how much processed external substance is injected into the tone core.";
    getParamQuantity(ACTIVATION_CONSTANT_PARAM)->description =
        "Sets the baseline activation amount for the core and selected tone.";
    getParamQuantity(TONIC_MOD_AMOUNT_PARAM)->description =
        "With no cable, sets Time/Filter feedback modulation of Tonic; with a cable, sets patched Tonic modulation depth.";
    getParamQuantity(ACTIVATION_INTERFERENCE_PARAM)->description =
        "Sets how much the Time/Filter experiment influences Activation.";
    getParamQuantity(ACTIVATION_CV_AMOUNT_PARAM)->description =
        "Attenuverts patched Activation CV.";
    getParamQuantity(TONIC_COARSE_PARAM)->description =
        "Sets the core tonic frequency over a wide musical range.";
    getParamQuantity(TONIC_FINE_PARAM)->description =
        "Fine tunes the tonic in semitone-scale offsets.";
    getParamQuantity(TONES_PARAM)->description =
        "Morphs the core tone palette from simple oscillator colors to folded and subharmonic material.";
    getParamQuantity(TONES_CV_AMOUNT_PARAM)->description =
        "Attenuverts patched Tones CV.";
    getParamQuantity(TIME_COARSE_PARAM)->description =
        "Sets the base Time/Filter delay range.";
    getParamQuantity(TIME_FINE_PARAM)->description =
        "Fine trims the Time/Filter delay range.";
      getParamQuantity(TIME_MOD_AMOUNT_PARAM)->description =
          "With no cable, sets subharmonic modulation of Time; with a cable, sets patched Time modulation depth.";
    getParamQuantity(TIME_CV_AMOUNT_PARAM)->description =
        "Attenuverts patched Time CV.";
    getParamQuantity(DECAY_PARAM)->description =
        "Sets Time/Filter feedback persistence.";
    getParamQuantity(DECAY_CV_AMOUNT_PARAM)->description =
        "Attenuverts patched Decay CV.";
    getParamQuantity(FILTER_PARAM)->description =
        "Sets Time/Filter feedback brightness and damping.";
    getParamQuantity(FILTER_CV_AMOUNT_PARAM)->description =
        "Attenuverts patched Filter CV.";
    getParamQuantity(BLEND_PARAM)->description =
        "Panel blend when Blend CV is unpatched; unipolar attenuator for Blend CV when patched.";
    getParamQuantity(ABSORB_PARAM)->description =
        "Panel absorb when Absorb CV is unpatched; unipolar attenuator for Absorb CV when patched.";
    getParamQuantity(LEVEL_PARAM)->description =
        "Sets final Modular and Line output level.";
    getParamQuantity(SPEED_PARAM)->description =
        "Sets internal agitation rate from long gestures to audio-rate motion.";
    getParamQuantity(ANGLE_PARAM)->description =
        "Skews agitation rise and fall shape.";
    getParamQuantity(SPEED_CV_AMOUNT_PARAM)->description =
        "Attenuates patched Agitation speed CV.";
    getParamQuantity(TOUCH_ACTIVATE_PARAM)->description = "Momentary touch bridge: injects a playable activation burst.";
    getParamQuantity(TOUCH_TIME_PARAM)->description = "Momentary touch bridge: nudges the Time/Filter delay path.";
    getParamQuantity(TOUCH_FILTER_PARAM)->description = "Momentary touch bridge: opens the feedback filter.";
    getParamQuantity(TOUCH_ABSORB_PARAM)->description = "Momentary touch bridge: absorbs and crumbles the feedback tail.";
    getParamQuantity(TOUCH_TONIC_PARAM)->description = "Momentary touch bridge: routes agitation and feedback to Tonic.";
    getParamQuantity(TOUCH_DECAY_PARAM)->description = "Momentary touch bridge: pushes Decay toward unstable persistence.";
    getParamQuantity(REVERB_SIZE_PARAM)->description = "Plateau-inspired tank size, from close haze to wide ambient space.";
    getParamQuantity(REVERB_DECAY_PARAM)->description = "Feedback decay time. Higher settings bloom toward long ambient tails.";
    getParamQuantity(REVERB_DIFFUSION_PARAM)->description = "Input and tank diffusion density.";
    getParamQuantity(REVERB_TONE_PARAM)->description = "Reverb damping brightness.";
    getParamQuantity(REVERB_MOD_PARAM)->description = "Slow delay-line modulation depth and rate.";
    getParamQuantity(REVERB_BLEND_PARAM)->description = "Blends the dry result with the ambient reverb on the Modular and Line outputs.";

    getInputInfo(EXTERNAL_INPUT)->description = "External audio or CV substance for Strength processing and core injection.";
    getInputInfo(BEGIN_END_INPUT)->description = "Normalled high. Patch a low gate or dummy cable to stop agitation.";
    getInputInfo(SPEED_CV_INPUT)->description = "0-5 V CV for Agitation speed through the Speed CV amount.";
    getInputInfo(ACTIVATION_CV_INPUT)->description = "Bipolar CV summed into Activation through the Activation CV attenuverter.";
    getInputInfo(TONIC_MOD_INPUT)->description = "Overrides the normalled Time/Filter feedback modulation of Tonic.";
    getInputInfo(TONES_CV_INPUT)->description = "Bipolar CV for Tones through the Tones CV attenuverter.";
    getInputInfo(V_OCT_INPUT)->description = "1V/oct pitch CV summed with Tonic coarse and fine controls.";
    getInputInfo(TIME_MOD_INPUT)->description = "Overrides the normalled subharmonic modulation of Time.";
    getInputInfo(TIME_CV_INPUT)->description = "Bipolar CV for Time through the Time CV attenuverter.";
    getInputInfo(TIME_UNITY_CV_INPUT)->description = "0-10 V unipolar CV added directly to Time.";
    getInputInfo(DECAY_CV_INPUT)->description = "Bipolar CV for Decay through the Decay CV attenuverter.";
    getInputInfo(BLEND_CV_INPUT)->description = "0-5 V unipolar Blend CV; Blend becomes an attenuator when patched.";
    getInputInfo(FILTER_CV_INPUT)->description = "Bipolar CV for Filter through the Filter CV attenuverter.";
    getInputInfo(ABSORB_CV_INPUT)->description = "0-10 V unipolar Absorb CV; Absorb becomes an attenuator when patched.";

    getOutputInfo(STRENGTH_OUTPUT)->description = "Processed external strength signal as audio-rate CV.";
    getOutputInfo(CV1_OUTPUT)->description = "Envelope-followed strength CV, 0-10 V.";
    getOutputInfo(CV2_OUTPUT)->description = "Time/Filter feedback CV, bipolar +/-5 V.";
    getOutputInfo(AGITATION_OUTPUT)->description = "Internal agitation gesture, 0-6 V.";
    getOutputInfo(TONE_CORE_OUTPUT)->description = "Raw tone core output before Time/Filter blend.";
    getOutputInfo(SUB_HARMONICS_OUTPUT)->description = "Subharmonic output from the tone core.";
    getOutputInfo(MODULAR_OUTPUT)->description = "Main modular-level output with integrated ambient reverb.";
    getOutputInfo(LINE_OUTPUT)->description = "Lower-level copy of the result with integrated ambient reverb.";

    getLightInfo(STRENGTH_LIGHT)->description = "Shows Strength output activity.";
    getLightInfo(CV1_LIGHT)->description = "Shows CV1 envelope level.";
    getLightInfo(CV2_POS_LIGHT)->description = "Shows positive CV2 feedback voltage.";
    getLightInfo(CV2_NEG_LIGHT)->description = "Shows negative CV2 feedback voltage.";
    getLightInfo(AGITATION_LIGHT)->description = "Shows Agitation output level.";
    getLightInfo(ACTIVATION_POS_LIGHT)->description = "Shows positive activation.";
    getLightInfo(ACTIVATION_NEG_LIGHT)->description = "Shows negative activation.";
    getLightInfo(RESULT_LIGHT)->description = "Shows Modular output activity.";
    getLightInfo(REVERB_LIGHT)->description = "Shows the reverb contribution to the main outputs.";
  }

  void process(const ProcessArgs &args) override;
  void onReset() override;
  json_t* dataToJson() override;
  void dataFromJson(json_t* rootJ) override;
};

struct WyrdWidget : ModuleWidget
{
  WyrdWidget(Wyrd *module)
  {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/Wyrd_flat.svg")));

    // Screws
    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addInput(createInputCentered<PJ301MPort>(Vec(35.f, 48.f), module, Wyrd::EXTERNAL_INPUT));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(85.f, 48.f), module, Wyrd::STRENGTH_OUTPUT));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(35.f, 92.f), module, Wyrd::STRENGTH_PARAM));
    addParam(createParamCentered<Rogan2PWhite>(Vec(85.f, 92.f), module, Wyrd::EXTERNAL_CONSTANT_PARAM));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(35.f, 136.f), module, Wyrd::CV1_OUTPUT));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(85.f, 136.f), module, Wyrd::CV2_OUTPUT));

    addParam(createParamCentered<Rogan2PWhite>(Vec(45.f, 220.f), module, Wyrd::SPEED_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(95.f, 220.f), module, Wyrd::ANGLE_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(45.f, 267.f), module, Wyrd::SPEED_CV_AMOUNT_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(95.f, 267.f), module, Wyrd::SPEED_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(45.f, 322.f), module, Wyrd::BEGIN_END_INPUT));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(95.f, 322.f), module, Wyrd::AGITATION_OUTPUT));

    addParam(createParamCentered<Rogan2PWhite>(Vec(145.f, 92.f), module, Wyrd::ACTIVATION_CONSTANT_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(190.f, 92.f), module, Wyrd::ACTIVATION_INTERFERENCE_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(145.f, 139.f), module, Wyrd::ACTIVATION_CV_AMOUNT_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(190.f, 139.f), module, Wyrd::ACTIVATION_CV_INPUT));
    addParam(createParamCentered<LEDButton>(Vec(145.f, 48.f), module, Wyrd::TOUCH_ACTIVATE_PARAM));
    addParam(createParamCentered<LEDButton>(Vec(390.f, 48.f), module, Wyrd::TOUCH_TIME_PARAM));
    addParam(createParamCentered<LEDButton>(Vec(535.f, 48.f), module, Wyrd::TOUCH_FILTER_PARAM));
    addParam(createParamCentered<LEDButton>(Vec(550.f, 194.f), module, Wyrd::TOUCH_ABSORB_PARAM));
    addParam(createParamCentered<LEDButton>(Vec(255.f, 48.f), module, Wyrd::TOUCH_TONIC_PARAM));
    addParam(createParamCentered<LEDButton>(Vec(455.f, 48.f), module, Wyrd::TOUCH_DECAY_PARAM));

    addParam(createParamCentered<Rogan3PSWhite>(Vec(255.f, 118.f), module, Wyrd::TONIC_COARSE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(310.f, 90.f), module, Wyrd::TONIC_FINE_PARAM));
    addParam(createParamCentered<Rogan2PWhite>(Vec(310.f, 135.f), module, Wyrd::TONES_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(210.f, 190.f), module, Wyrd::TONIC_MOD_AMOUNT_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(260.f, 190.f), module, Wyrd::TONES_CV_AMOUNT_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(310.f, 190.f), module, Wyrd::V_OCT_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(210.f, 238.f), module, Wyrd::TONIC_MOD_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(260.f, 238.f), module, Wyrd::TONES_CV_INPUT));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(310.f, 238.f), module, Wyrd::TONE_CORE_OUTPUT));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(310.f, 284.f), module, Wyrd::SUB_HARMONICS_OUTPUT));

    addParam(createParamCentered<Rogan3PSWhite>(Vec(390.f, 118.f), module, Wyrd::TIME_COARSE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(455.f, 90.f), module, Wyrd::TIME_FINE_PARAM));
    addParam(createParamCentered<Rogan2PWhite>(Vec(455.f, 135.f), module, Wyrd::DECAY_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(365.f, 190.f), module, Wyrd::TIME_MOD_AMOUNT_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(410.f, 190.f), module, Wyrd::TIME_CV_AMOUNT_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(455.f, 190.f), module, Wyrd::DECAY_CV_AMOUNT_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(365.f, 238.f), module, Wyrd::TIME_MOD_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(410.f, 238.f), module, Wyrd::TIME_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(455.f, 238.f), module, Wyrd::TIME_UNITY_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(455.f, 284.f), module, Wyrd::DECAY_CV_INPUT));

    addParam(createParamCentered<Rogan3PSWhite>(Vec(535.f, 118.f), module, Wyrd::FILTER_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(500.f, 190.f), module, Wyrd::FILTER_CV_AMOUNT_PARAM));
    addParam(createParamCentered<Rogan2PWhite>(Vec(550.f, 238.f), module, Wyrd::ABSORB_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(500.f, 238.f), module, Wyrd::FILTER_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(550.f, 284.f), module, Wyrd::ABSORB_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(500.f, 331.f), module, Wyrd::BLEND_CV_INPUT));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(545.f, 331.f), module, Wyrd::LINE_OUTPUT));
    addParam(createParamCentered<Rogan1PWhite>(Vec(365.f, 331.f), module, Wyrd::BLEND_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(425.f, 331.f), module, Wyrd::LEVEL_PARAM));
    addOutput(createOutputCentered<DarkPJ301MPort>(Vec(580.f, 331.f), module, Wyrd::MODULAR_OUTPUT));

    // LIGHTS
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(60.f, 48.f), module, Wyrd::STRENGTH_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(60.f, 136.f), module, Wyrd::CV1_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(105.f, 132.f), module, Wyrd::CV2_POS_LIGHT));
    addChild(createLightCentered<SmallLight<RedLight>>(Vec(105.f, 140.f), module, Wyrd::CV2_NEG_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(115.f, 322.f), module, Wyrd::AGITATION_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(165.f, 67.f), module, Wyrd::ACTIVATION_POS_LIGHT));
    addChild(createLightCentered<SmallLight<RedLight>>(Vec(173.f, 67.f), module, Wyrd::ACTIVATION_NEG_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(580.f, 307.f), module, Wyrd::RESULT_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(622.5f, 333.f), module, Wyrd::REVERB_LIGHT));

    // REVERB
    addParam(createParamCentered<Rogan1PWhite>(Vec(622.5f, 47.f), module, Wyrd::REVERB_SIZE_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(622.5f, 95.f), module, Wyrd::REVERB_DECAY_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(622.5f, 143.f), module, Wyrd::REVERB_DIFFUSION_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(622.5f, 191.f), module, Wyrd::REVERB_TONE_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(622.5f, 239.f), module, Wyrd::REVERB_MOD_PARAM));
    addParam(createParamCentered<Rogan1PWhite>(Vec(622.5f, 287.f), module, Wyrd::REVERB_BLEND_PARAM));
  }

  void appendContextMenu(Menu* menu) override
  {
    Wyrd* module = getModule<Wyrd>();
    menu->addChild(new MenuSeparator);

    menu->addChild(createIndexSubmenuItem("Touch source",
      {"Manual plate", "CV1", "CV2", "Agitation", "Sub harmonics", "Strength", "Noise"},
      [=]() {
        return module ? module->touchSource : static_cast<int>(shortwav::wyrd::TouchSource::MANUAL);
      },
      [=](int source) {
        if (module) {
          module->touchSource = clamp(source, 0, shortwav::wyrd::kNumTouchSources - 1);
        }
      }
    ));

    menu->addChild(createIndexSubmenuItem("Strength calibration",
      {"Strega", "Line gentle", "Modular hot"},
      [=]() {
        return module ? module->strengthCalibration : static_cast<int>(shortwav::wyrd::StrengthCalibration::STREGA);
      },
      [=](int mode) {
        if (module) {
          module->strengthCalibration = clamp(mode, 0, shortwav::wyrd::kNumStrengthCalibrations - 1);
        }
      }
    ));
  }
};
