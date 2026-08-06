#pragma once

#include "plugin.hpp"
#include "dsp/wyrd-dsp.h"

struct Wyrdexpander : Module
{
  enum ParamIds
  {
    SIZE_PARAM,
    DECAY_PARAM,
    DIFFUSION_PARAM,
    TONE_PARAM,
    MOD_PARAM,
    LEVEL_PARAM,
    NUM_PARAMS
  };

  enum InputIds
  {
    NUM_INPUTS
  };

  enum OutputIds
  {
    REVERB_OUTPUT,
    NUM_OUTPUTS
  };

  enum LightIds
  {
    LINK_LIGHT,
    REVERB_LIGHT,
    NUM_LIGHTS
  };

  shortwav::wyrd::WyrdAmbientReverb reverb;

  Wyrdexpander() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(SIZE_PARAM, 0.f, 1.f, 0.72f, "Size", "%", 0.f, 100.f);
    configParam(DECAY_PARAM, 0.f, 1.f, 0.78f, "Decay", "%", 0.f, 100.f);
    configParam(DIFFUSION_PARAM, 0.f, 1.f, 0.82f, "Diffusion", "%", 0.f, 100.f);
    configParam(TONE_PARAM, 0.f, 1.f, 0.55f, "Tone", "%", 0.f, 100.f);
    configParam(MOD_PARAM, 0.f, 1.f, 0.34f, "Modulation", "%", 0.f, 100.f);
    configParam(LEVEL_PARAM, 0.f, 1.f, 0.82f, "Wet level", "%", 0.f, 100.f);

      configOutput(REVERB_OUTPUT, "Wyrd reverb");

    configLight(LINK_LIGHT, "Wyrd link");
    configLight(REVERB_LIGHT, "Wyrd reverb");

      getParamQuantity(SIZE_PARAM)->description = "Plateau-inspired tank size, from close haze to wide ambient space.";
    getParamQuantity(DECAY_PARAM)->description = "Feedback decay time. Higher settings bloom toward long ambient tails.";
    getParamQuantity(DIFFUSION_PARAM)->description = "Input and tank diffusion density.";
    getParamQuantity(TONE_PARAM)->description = "Reverb damping brightness.";
      getParamQuantity(MOD_PARAM)->description = "Slow delay-line modulation depth and rate.";
      getParamQuantity(LEVEL_PARAM)->description = "Wet-only expander output level.";
      getOutputInfo(REVERB_OUTPUT)->description = "Wet ambient reverb copy of Wyrd's Modular output when placed directly to the right.";
    getLightInfo(LINK_LIGHT)->description = "Shows when the expander is receiving audio from Wyrd.";
    getLightInfo(REVERB_LIGHT)->description = "Shows wet reverb output activity.";
    }

  void process(const ProcessArgs &args) override;
  void onReset() override { reverb.reset(); }
};

struct WyrdexpanderWidget : ModuleWidget
{
  WyrdexpanderWidget(Wyrdexpander *module)
  {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/Wyrdexpander.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addParam(createParamCentered<Trimpot>(Vec(22.5f, 48.f), module, Wyrdexpander::SIZE_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(22.5f, 92.f), module, Wyrdexpander::DECAY_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(22.5f, 136.f), module, Wyrdexpander::DIFFUSION_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(22.5f, 180.f), module, Wyrdexpander::TONE_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(22.5f, 224.f), module, Wyrdexpander::MOD_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(22.5f, 268.f), module, Wyrdexpander::LEVEL_PARAM));
    addOutput(createOutputCentered<PJ301MPort>(Vec(22.5f, 326.f), module, Wyrdexpander::REVERB_OUTPUT));

    addChild(createLightCentered<SmallLight<YellowLight>>(Vec(14.f, 356.f), module, Wyrdexpander::LINK_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(31.f, 356.f), module, Wyrdexpander::REVERB_LIGHT));
  }
};
