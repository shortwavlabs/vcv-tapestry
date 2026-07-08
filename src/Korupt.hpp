#pragma once

#include "plugin.hpp"
#include "dsp/korupt-dsp.h"

#include <array>

struct Korupt : Module {
	enum ParamId {
		SQUARE_MIX_PARAM,
		SUBHARMONIC_MIX_PARAM,
		OSCILLATOR_MIX_PARAM,
		LEVEL_PARAM,
		SUBHARMONIC_PROGRAM_PARAM,
		SUBHARMONIC_ROOT_PARAM,
		FREQ_MOD_MODE_PARAM,
		RATE_PARAM,
		OSCILLATOR_PROGRAM_PARAM,
		OSCILLATOR_ROOT_PARAM,
		NUM_PARAMS
	};

	enum InputId {
		AUDIO_INPUT,
		SQUARE_MIX_CV_INPUT,
		SUBHARMONIC_MIX_CV_INPUT,
		OSCILLATOR_MIX_CV_INPUT,
		RATE_CV_INPUT,
		SUBHARMONIC_PROGRAM_CV_INPUT,
		OSCILLATOR_PROGRAM_CV_INPUT,
		NUM_INPUTS
	};

	enum OutputId {
		AUDIO_OUTPUT,
		SQUARE_OUTPUT,
		SUBHARMONIC_OUTPUT,
		OSCILLATOR_OUTPUT,
		LOCK_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightId {
		TRACK_LIGHT,
		LOCK_LIGHT,
		GLITCH_LIGHT,
		NUM_LIGHTS
	};

	Korupt();
	void onSampleRateChange() override;
	void onReset() override;
	void process(const ProcessArgs& args) override;

private:
	float normalizedParam(ParamId paramId, InputId inputId, int channel);
	int steppedParam(ParamId paramId, InputId inputId, int steps, int channel);
	ShortwavDSP::KoruptParams paramsForChannel(int channel);

	std::array<dsp::SchmittTrigger, 16> inputTriggers_;
	std::array<ShortwavDSP::KoruptInputStage, 16> inputStages_;
	std::array<ShortwavDSP::KoruptDSP, 16> engines_;
};

struct KoruptWidget : ModuleWidget {
	KoruptWidget(Korupt* module);
};
