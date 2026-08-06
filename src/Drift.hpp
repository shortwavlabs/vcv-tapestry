#pragma once

#include "plugin.hpp"
#include "dsp/drif-dsp.h"

struct Drift : Module
{
  enum ParamIds
  {
    OFFSET_PARAM,
    SPEED_PARAM,
    RANGE_PARAM,
    MIX_PARAM,
    NUM_PARAMS
  };

  enum InputIds
  {
    SIGNAL_INPUT,
    NUM_INPUTS
  };

  enum OutputIds
  {
    CV_OUTPUT,
    NUM_OUTPUTS
  };

  enum LightIds
  {
    OUTPUT_POS_LIGHT,
    OUTPUT_NEG_LIGHT,
    ACTIVITY_LIGHT,
    NUM_LIGHTS
  };

  shortwav::drift::Engine engine;

  Drift() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configParam(OFFSET_PARAM, 0.f, 1.f, 0.5f, "Offset", " V", 0.f, 10.f, -5.f);
    configParam(SPEED_PARAM, 0.f, 1.f, 0.38f, "Speed", "%", 0.f, 100.f);
    configParam(RANGE_PARAM, 0.f, 1.f, 0.72f, "Range", " Vpp", 0.f, 10.f);
    configParam(MIX_PARAM, -1.f, 1.f, 0.f, "Signal B attenuverter", "%", 0.f, 100.f);

    configInput(SIGNAL_INPUT, "Signal B");
    configOutput(CV_OUTPUT, "Random CV");

    configLight(OUTPUT_POS_LIGHT, "Positive output");
    configLight(OUTPUT_NEG_LIGHT, "Negative output");
    configLight(ACTIVITY_LIGHT, "Drift activity");

    getParamQuantity(OFFSET_PARAM)->description = "Offsets the random CV from -5 V to +5 V.";
    getParamQuantity(SPEED_PARAM)->description = "Sets the random target rate from slow uncertainty to animated control motion.";
    getParamQuantity(RANGE_PARAM)->description = "Sets the bipolar depth of the random CV before Offset is added.";
    getParamQuantity(MIX_PARAM)->description =
        "With Signal B patched, center keeps Drift alone; clockwise crossfades to Signal B, counterclockwise to inverted Signal B.";
    getInputInfo(SIGNAL_INPUT)->description =
        "Signal B for the attenuverter blend. Patch CV here to replace or invert-mix Drift's internal random LFO.";
    getOutputInfo(CV_OUTPUT)->description =
        "Offset and ranged random LFO CV with subtle jitter, interference, and occasional voltage slips.";
    getLightInfo(OUTPUT_POS_LIGHT)->description = "Shows positive Random CV voltage.";
    getLightInfo(OUTPUT_NEG_LIGHT)->description = "Shows negative Random CV voltage.";
    getLightInfo(ACTIVITY_LIGHT)->description = "Shows Drift's internal jitter and glitch activity.";
  }

  void process(const ProcessArgs &args) override;
  void onReset() override { engine.reset(); }
};

struct DriftWidget : ModuleWidget
{
  DriftWidget(Drift *module)
  {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/Drift.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(22.5f, 58.f), module, Drift::OFFSET_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(22.5f, 112.f), module, Drift::SPEED_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(22.5f, 166.f), module, Drift::RANGE_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(22.5f, 222.f), module, Drift::MIX_PARAM));

    addInput(createInputCentered<PJ301MPort>(Vec(22.5f, 270.f), module, Drift::SIGNAL_INPUT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(17.5f, 304.f), module, Drift::OUTPUT_POS_LIGHT));
    addChild(createLightCentered<SmallLight<RedLight>>(Vec(27.5f, 304.f), module, Drift::OUTPUT_NEG_LIGHT));
    addChild(createLightCentered<SmallLight<YellowLight>>(Vec(22.5f, 318.f), module, Drift::ACTIVITY_LIGHT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(22.5f, 344.f), module, Drift::CV_OUTPUT));
  }
};
