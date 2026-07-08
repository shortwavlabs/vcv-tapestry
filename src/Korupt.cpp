#include "Korupt.hpp"

namespace {

struct KoruptProgramKnob : RoundLargeBlackKnob {
	KoruptProgramKnob() {
		snap = true;
	}
};

constexpr float panelWidth = 300.f;

} // namespace

Korupt::Korupt() {
	config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

	configInput(AUDIO_INPUT, "Audio");
	configInput(SQUARE_MIX_CV_INPUT, "Square mix CV");
	configInput(SUBHARMONIC_MIX_CV_INPUT, "Subharmonic mix CV");
	configInput(OSCILLATOR_MIX_CV_INPUT, "Oscillator mix CV");
	configInput(RATE_CV_INPUT, "Rate CV");
	configInput(SUBHARMONIC_PROGRAM_CV_INPUT, "Subharmonic program CV");
	configInput(OSCILLATOR_PROGRAM_CV_INPUT, "Oscillator program CV");

	configOutput(AUDIO_OUTPUT, "Audio");
	configOutput(SQUARE_OUTPUT, "Square voice");
	configOutput(SUBHARMONIC_OUTPUT, "Subharmonic voice");
	configOutput(OSCILLATOR_OUTPUT, "Oscillator voice");
	configOutput(LOCK_OUTPUT, "PLL lock");
	configBypass(AUDIO_INPUT, AUDIO_OUTPUT);

	configParam(SQUARE_MIX_PARAM, 0.f, 1.f, 0.7f, "Square voice level", "%", 0.f, 100.f);
	configParam(SUBHARMONIC_MIX_PARAM, 0.f, 1.f, 0.7f, "Subharmonic voice level", "%", 0.f, 100.f);
	configParam(OSCILLATOR_MIX_PARAM, 0.f, 1.f, 0.7f, "Oscillator voice level", "%", 0.f, 100.f);
	configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Output level", "%", 0.f, 100.f);
	configParam(RATE_PARAM, 0.f, 1.f, 0.35f, "Frequency modulator rate", "%", 0.f, 100.f);

	configSwitch(SUBHARMONIC_PROGRAM_PARAM, 0.f, 7.f, 0.f, "Subharmonic interval", {
		"1 octave down / unison",
		"1 octave down / fifth",
		"2 octaves down / unison",
		"2 octaves down / major third",
		"2 octaves down / fifth",
		"2 octaves down / minor seventh",
		"3 octaves down / unison",
		"3 octaves down / major second"
	});
	configSwitch(SUBHARMONIC_ROOT_PARAM, 0.f, 1.f, 0.f, "Subharmonic root", {
		"Input square",
		"Master oscillator"
	});
	configSwitch(FREQ_MOD_MODE_PARAM, 0.f, 1.f, 0.f, "Frequency modulator mode", {
		"Glide",
		"Vibrato"
	});
	configSwitch(OSCILLATOR_PROGRAM_PARAM, 0.f, 7.f, 0.f, "Master oscillator interval", {
		"Unison",
		"1 octave up / unison",
		"1 octave up / fifth",
		"2 octaves up / unison",
		"2 octaves up / major third",
		"2 octaves up / fifth",
		"2 octaves up / minor seventh",
		"3 octaves up / unison"
	});
	configSwitch(OSCILLATOR_ROOT_PARAM, 0.f, 2.f, 0.f, "Master oscillator root", {
		"Unison",
		"1 octave down",
		"2 octaves down"
	});

	onSampleRateChange();
}

void Korupt::onSampleRateChange() {
	const float sampleRate = APP ? APP->engine->getSampleRate() : 44100.f;
	for (ShortwavDSP::KoruptDSP& engine : engines_) {
		engine.setSampleRate(sampleRate);
	}
}

void Korupt::onReset() {
	for (ShortwavDSP::KoruptDSP& engine : engines_) {
		engine.reset();
	}
	for (dsp::SchmittTrigger& trigger : inputTriggers_) {
		trigger.reset();
	}
}

float Korupt::normalizedParam(ParamId paramId, InputId inputId, int channel) {
	float value = params[paramId].getValue();
	if (inputs[inputId].isConnected()) {
		value += inputs[inputId].getPolyVoltage(channel) / 10.f;
	}
	return clamp(value, 0.f, 1.f);
}

int Korupt::steppedParam(ParamId paramId, InputId inputId, int steps, int channel) {
	float value = params[paramId].getValue();
	if (inputs[inputId].isConnected()) {
		value += inputs[inputId].getPolyVoltage(channel) * static_cast<float>(steps - 1) / 10.f;
	}
	return clamp(static_cast<int>(std::round(value)), 0, steps - 1);
}

ShortwavDSP::KoruptParams Korupt::paramsForChannel(int channel) {
	ShortwavDSP::KoruptParams dspParams;
	dspParams.level = params[LEVEL_PARAM].getValue();
	dspParams.squareMix = normalizedParam(SQUARE_MIX_PARAM, SQUARE_MIX_CV_INPUT, channel);
	dspParams.subharmonicMix = normalizedParam(SUBHARMONIC_MIX_PARAM, SUBHARMONIC_MIX_CV_INPUT, channel);
	dspParams.oscillatorMix = normalizedParam(OSCILLATOR_MIX_PARAM, OSCILLATOR_MIX_CV_INPUT, channel);
	dspParams.rate = normalizedParam(RATE_PARAM, RATE_CV_INPUT, channel);
	dspParams.subharmonicProgram = steppedParam(SUBHARMONIC_PROGRAM_PARAM, SUBHARMONIC_PROGRAM_CV_INPUT, 8, channel);
	dspParams.oscillatorProgram = steppedParam(OSCILLATOR_PROGRAM_PARAM, OSCILLATOR_PROGRAM_CV_INPUT, 8, channel);
	dspParams.subharmonicRoot = clamp(static_cast<int>(std::round(params[SUBHARMONIC_ROOT_PARAM].getValue())), 0, 1);
	dspParams.oscillatorRoot = clamp(static_cast<int>(std::round(params[OSCILLATOR_ROOT_PARAM].getValue())), 0, 2);
	dspParams.vibratoMode = params[FREQ_MOD_MODE_PARAM].getValue() >= 0.5f;
	return dspParams;
}

void Korupt::process(const ProcessArgs& args) {
	const int channels = std::max(1, inputs[AUDIO_INPUT].getChannels());

	float trackLight = 0.f;
	float lockLight = 0.f;
	float glitchLight = 0.f;

	for (int channel = 0; channel < channels; channel++) {
		const float inputVoltage = inputs[AUDIO_INPUT].getPolyVoltage(channel);
		const float normalizedInput = clamp(inputVoltage / 5.f, -1.5f, 1.5f);
		const float comparatorInput = std::tanh(normalizedInput * 6.f);
		const dsp::SchmittTrigger::Event event = inputTriggers_[channel].processEvent(comparatorInput, -0.05f, 0.05f);
		const bool inputRisingEdge = event == dsp::SchmittTrigger::TRIGGERED;
		const bool inputHigh = inputTriggers_[channel].isHigh();

		const ShortwavDSP::KoruptResult result = engines_[channel].process(
			normalizedInput,
			inputRisingEdge,
			inputHigh,
			paramsForChannel(channel)
		);

		outputs[AUDIO_OUTPUT].setVoltage(clamp(result.mixed * 5.f, -10.f, 10.f), channel);
		outputs[SQUARE_OUTPUT].setVoltage(result.square * 5.f, channel);
		outputs[SUBHARMONIC_OUTPUT].setVoltage(result.subharmonic * 5.f, channel);
		outputs[OSCILLATOR_OUTPUT].setVoltage(result.oscillator * 5.f, channel);
		outputs[LOCK_OUTPUT].setVoltage(result.lock * 10.f, channel);

		trackLight = std::max(trackLight, result.tracking);
		lockLight = std::max(lockLight, result.lock);
		glitchLight = std::max(glitchLight, result.glitch);
	}

	for (int output = 0; output < NUM_OUTPUTS; output++) {
		outputs[output].setChannels(channels);
	}

	lights[TRACK_LIGHT].setSmoothBrightness(trackLight, args.sampleTime);
	lights[LOCK_LIGHT].setSmoothBrightness(lockLight, args.sampleTime);
	lights[GLITCH_LIGHT].setSmoothBrightness(glitchLight, args.sampleTime);
}

KoruptWidget::KoruptWidget(Korupt* module) {
	setModule(module);
	setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KORUPT.svg")));

	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewSilver>(Vec(panelWidth - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createWidget<ScrewSilver>(Vec(panelWidth - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParamCentered<RoundBlackKnob>(Vec(49.f, 70.f), module, Korupt::SQUARE_MIX_PARAM));
	addParam(createParamCentered<RoundBlackKnob>(Vec(105.f, 70.f), module, Korupt::SUBHARMONIC_MIX_PARAM));
	addParam(createParamCentered<RoundBlackKnob>(Vec(161.f, 70.f), module, Korupt::OSCILLATOR_MIX_PARAM));
	addParam(createParamCentered<RoundLargeBlackKnob>(Vec(251.f, 72.f), module, Korupt::LEVEL_PARAM));

	addParam(createParamCentered<KoruptProgramKnob>(Vec(62.f, 166.f), module, Korupt::SUBHARMONIC_PROGRAM_PARAM));
	addParam(createParamCentered<CKSS>(Vec(100.f, 250.f), module, Korupt::SUBHARMONIC_ROOT_PARAM));

	addParam(createParamCentered<CKSS>(Vec(150.f, 151.f), module, Korupt::FREQ_MOD_MODE_PARAM));
	addParam(createParamCentered<RoundBlackKnob>(Vec(150.f, 219.f), module, Korupt::RATE_PARAM));

	addParam(createParamCentered<KoruptProgramKnob>(Vec(235.f, 166.f), module, Korupt::OSCILLATOR_PROGRAM_PARAM));
	addParam(createParamCentered<CKSSThree>(Vec(247.f, 250.f), module, Korupt::OSCILLATOR_ROOT_PARAM));

	addInput(createInputCentered<PJ301MPort>(Vec(24.f, 321.f), module, Korupt::AUDIO_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(24.f, 354.f), module, Korupt::AUDIO_OUTPUT));

	addInput(createInputCentered<PJ301MPort>(Vec(67.f, 321.f), module, Korupt::SQUARE_MIX_CV_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(67.f, 354.f), module, Korupt::SQUARE_OUTPUT));

	addInput(createInputCentered<PJ301MPort>(Vec(111.f, 321.f), module, Korupt::SUBHARMONIC_MIX_CV_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(111.f, 354.f), module, Korupt::SUBHARMONIC_OUTPUT));

	addInput(createInputCentered<PJ301MPort>(Vec(155.f, 321.f), module, Korupt::OSCILLATOR_MIX_CV_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(155.f, 354.f), module, Korupt::OSCILLATOR_OUTPUT));

	addInput(createInputCentered<PJ301MPort>(Vec(199.f, 321.f), module, Korupt::RATE_CV_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(199.f, 354.f), module, Korupt::LOCK_OUTPUT));

	addInput(createInputCentered<PJ301MPort>(Vec(243.f, 321.f), module, Korupt::SUBHARMONIC_PROGRAM_CV_INPUT));
	addInput(createInputCentered<PJ301MPort>(Vec(276.f, 321.f), module, Korupt::OSCILLATOR_PROGRAM_CV_INPUT));

	addChild(createLightCentered<MediumLight<GreenLight>>(Vec(243.f, 354.f), module, Korupt::TRACK_LIGHT));
	addChild(createLightCentered<MediumLight<BlueLight>>(Vec(260.f, 354.f), module, Korupt::LOCK_LIGHT));
	addChild(createLightCentered<MediumLight<RedLight>>(Vec(277.f, 354.f), module, Korupt::GLITCH_LIGHT));
}

Model* modelKorupt = createModel<Korupt, KoruptWidget>("Korupt");
