#pragma once

#include "plugin.hpp"
#include "dsp/fray-core.h"
#include "dsp/fray-effects.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

struct Fray : Module {
	enum { EFFECT_PARAM_COUNT = 14 };

	enum ParamId {
		TEMPO_PARAM,
		EXT_CLOCK_MODE_PARAM,
		BEATS_PARAM,
		DIVISIONS_PARAM,
		SCENE_PARAM,
		LOOP_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		RANDOMIZE_PARAM,
		MUTATE_PARAM,
		SELECT_EFFECT_PARAM,
		MASTER_MIX_PARAM,
		MASTER_PAN_PARAM,
		MASTER_GAIN_PARAM,
		MACRO_A_PARAM,
		MACRO_B_PARAM,
		MACRO_C_PARAM,
		MACRO_D_PARAM,
		EFFECT_PARAM_BASE,
		NUM_PARAMS = EFFECT_PARAM_BASE + ShortwavDSP::Fray::kEffectCount * EFFECT_PARAM_COUNT
	};

	enum InputId {
		AUDIO_L_INPUT,
		AUDIO_R_INPUT,
		CLOCK_INPUT,
		TEMPO_CV_INPUT,
		RESET_INPUT,
		RUN_INPUT,
		SCENE_INPUT,
		SCENE_GATE_INPUT,
		NEXT_INPUT,
		PREV_INPUT,
		RANDOMIZE_INPUT,
		MUTATE_INPUT,
		MIX_CV_INPUT,
		MOD_A_INPUT,
		MOD_B_INPUT,
		MOD_C_INPUT,
		MOD_D_INPUT,
		NUM_INPUTS
	};

	enum OutputId {
		AUDIO_L_OUTPUT,
		AUDIO_R_OUTPUT,
		STEP_OUTPUT,
		EOC_OUTPUT,
		ACTIVITY_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightId {
		RUN_LIGHT,
		CLOCK_LIGHT,
		RANDOMIZE_LIGHT,
		ENUMS(EFFECT_LIGHT, ShortwavDSP::Fray::kEffectCount),
		NUM_LIGHTS
	};

	struct GridCommand {
		int scene;
		int lane;
		uint64_t activeMask;
		uint64_t startMask;
		uint64_t expectedActiveMask;
		uint64_t expectedStartMask;
		uint32_t epoch;
		bool conditional;
	};

	ShortwavDSP::Fray::ProgramState program;
	ShortwavDSP::Fray::Transport transport;
	ShortwavDSP::Fray::FrayEffects effects;

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger sceneGateTrigger;
	dsp::SchmittTrigger nextTrigger;
	dsp::SchmittTrigger prevTrigger;
	dsp::SchmittTrigger randomizeTrigger;
	dsp::SchmittTrigger mutateTrigger;
	dsp::BooleanTrigger resetButtonTrigger;
	dsp::BooleanTrigger randomizeButtonTrigger;
	dsp::BooleanTrigger mutateButtonTrigger;
	dsp::PulseGenerator stepPulse;
	dsp::PulseGenerator eocPulse;
	dsp::PulseGenerator randomizePulse;
	dsp::ClockDivider controlDivider;
	dsp::SlewLimiter chainTransition;
	ShortwavDSP::Fray::StereoFrame chainTransitionFrom;
	ShortwavDSP::Fray::StereoFrame lastChainOutput;
	bool chainOutputInitialized = false;

	dsp::RingBuffer<GridCommand, 1024> gridCommands;

	std::array<std::atomic<uint64_t>, ShortwavDSP::Fray::kLaneCount> uiActiveMasks;
	std::array<std::atomic<uint64_t>, ShortwavDSP::Fray::kLaneCount> uiStartMasks;
	std::atomic<int> uiCurrentCell;
	std::atomic<int> uiStepCount;
	std::atomic<int> uiActiveScene;
	std::atomic<int> uiBpmTimes10;
	std::atomic<int> uiRevision;
	std::atomic<int> sceneCvMode;
	std::atomic<bool> resetOnRun;
	std::atomic<uint64_t> uiEffectOrderPacked;
	std::atomic<uint32_t> gridEpoch;
	// Authored program state is shared with Rack's serialization callbacks. The
	// audio thread only try-locks this flag, so it can never block.
	std::atomic_flag authoredStateLock = ATOMIC_FLAG_INIT;
	std::atomic<bool> suppressDeferredCapture;

	int activeScene = 0;
	int previousCell = -1;
	int randomOverlayEffect = -1;
	uint32_t randomEventOrdinal = 0;
	uint64_t transportEventOrdinal = 0;
	uint32_t stateRevision = 0;
	bool previousRun = true;
	bool deferredResetEdge = false;
	bool deferredClockEdge = false;
	bool deferredRandomizeEdge = false;
	bool deferredMutateEdge = false;
	bool deferredNextEdge = false;
	bool deferredPrevEdge = false;
	bool deferredSceneGateEdge = false;
	std::array<bool, ShortwavDSP::Fray::kEffectCount> effectActive;
	std::array<bool, ShortwavDSP::Fray::kEffectCount> effectStart;
	std::array<bool, ShortwavDSP::Fray::kEffectCount> effectEnd;
	std::array<std::atomic<int>, 4> macroTargets;

	Fray();

	static int effectParamId(int effect, int slot) {
		return EFFECT_PARAM_BASE + effect * EFFECT_PARAM_COUNT + slot;
	}

	void process(const ProcessArgs& args) override;
	void processBypass(const ProcessArgs& args) override;
	void onReset(const ResetEvent& e) override;
	void onRandomize(const RandomizeEvent& e) override;
	void onSampleRateChange(const SampleRateChangeEvent& e) override;
	json_t* dataToJson() override;
	void dataFromJson(json_t* rootJ) override;

	bool enqueueLaneState(int scene, int lane, uint64_t activeMask, uint64_t startMask);
	bool enqueueLaneStateAtEpoch(
		int scene, int lane, uint64_t activeMask, uint64_t startMask, uint32_t expectedEpoch);
	bool enqueueConditionalLaneState(
		int scene, int lane,
		uint64_t expectedActiveMask, uint64_t expectedStartMask,
		uint64_t activeMask, uint64_t startMask);
	void publishUiState();
	void selectScene(int scene, bool resetTransport = true);
	void syncParamsToScene();
	void loadSceneToParams();
	void randomizeCurrentScene(bool mutateOnly);

private:
	void configureEffectParams();
	void latchDeferredEdges();
	void applyGridCommands(int limit = 32);
	void updateSceneSelection(bool resetEdge);
	void updateEffectGates(bool forceStarts);
	ShortwavDSP::Fray::EffectSettings modulatedSettings(int effect);
	ShortwavDSP::Fray::StereoFrame processEffectChain(
		const ShortwavDSP::Fray::StereoFrame& input,
		const ProcessArgs& args,
		bool cellAdvanced);
	void resetRuntime(bool clearEffectMemory = true);
};

struct FrayWidget : ModuleWidget {
	std::array<std::vector<ParamWidget*>, ShortwavDSP::Fray::kEffectCount> effectWidgets;
	int visibleEffect = -1;

	FrayWidget(Fray* module);
	void step() override;
	void appendContextMenu(Menu* menu) override;
};
