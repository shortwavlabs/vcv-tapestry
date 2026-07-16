#include "../Fray.hpp"

#include <cassert>
#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>

Plugin* pluginInstance = NULL;

int main() {
	Fray module;
	Module::SampleRateChangeEvent sampleRateEvent;
	sampleRateEvent.sampleRate = 48000.f;
	sampleRateEvent.sampleTime = 1.f / sampleRateEvent.sampleRate;
	module.onSampleRateChange(sampleRateEvent);

	Module::ProcessArgs args;
	args.sampleRate = sampleRateEvent.sampleRate;
	args.sampleTime = sampleRateEvent.sampleTime;
	args.frame = 0;
	module.inputs[Fray::AUDIO_L_INPUT].channels = 1;
	module.inputs[Fray::AUDIO_L_INPUT].setVoltage(1.f);

	// With an empty pattern, Fray is an exact dry stereo normal.
	module.process(args);
	assert(std::fabs(module.outputs[Fray::AUDIO_L_OUTPUT].getVoltage() - 1.f) < 1.0e-5f);
	assert(std::fabs(module.outputs[Fray::AUDIO_R_OUTPUT].getVoltage() - 1.f) < 1.0e-5f);

	// Distortion Quality is a discrete low-latency/oversampled choice. Values on
	// the same side of the switch threshold must be identical, never a blend of
	// the raw path with Rack's seven-sample FIR path.
	ShortwavDSP::Fray::DistortionEffect rawA;
	ShortwavDSP::Fray::DistortionEffect rawB;
	ShortwavDSP::Fray::DistortionEffect oversampledA;
	ShortwavDSP::Fray::DistortionEffect oversampledB;
	rawA.prepare(48000.f);
	rawB.prepare(48000.f);
	oversampledA.prepare(48000.f);
	oversampledB.prepare(48000.f);
	ShortwavDSP::Fray::EffectSettings rawSettingsA;
	ShortwavDSP::Fray::EffectSettings rawSettingsB;
	ShortwavDSP::Fray::EffectSettings oversampledSettingsA;
	ShortwavDSP::Fray::EffectSettings oversampledSettingsB;
	rawSettingsA.values[7] = 0.f;
	rawSettingsB.values[7] = 0.49f;
	oversampledSettingsA.values[7] = 0.5f;
	oversampledSettingsB.values[7] = 1.f;
	ShortwavDSP::Fray::EffectContext distortionContext;
	distortionContext.active = true;
	for (int frame = 0; frame < 64; ++frame) {
		const float signal = frame == 0 ? 0.5f : 0.2f * std::sin(frame * 0.19f);
		const ShortwavDSP::Fray::StereoFrame input(signal, -signal);
		const ShortwavDSP::Fray::StereoFrame rawOutA = rawA.process(input, rawSettingsA, distortionContext);
		const ShortwavDSP::Fray::StereoFrame rawOutB = rawB.process(input, rawSettingsB, distortionContext);
		const ShortwavDSP::Fray::StereoFrame oversampledOutA = oversampledA.process(input, oversampledSettingsA, distortionContext);
		const ShortwavDSP::Fray::StereoFrame oversampledOutB = oversampledB.process(input, oversampledSettingsB, distortionContext);
		assert(std::fabs(rawOutA.left - rawOutB.left) < 1.0e-7f);
		assert(std::fabs(rawOutA.right - rawOutB.right) < 1.0e-7f);
		assert(std::fabs(oversampledOutA.left - oversampledOutB.left) < 1.0e-7f);
		assert(std::fabs(oversampledOutA.right - oversampledOutB.right) < 1.0e-7f);
	}

	// Keep the documented Rack 2.6.6 FIR latency explicit and regression-tested.
	dsp::Upsampler<2, 8> latencyUpsampler;
	dsp::Decimator<2, 8> latencyDecimator;
	int impulsePeakIndex = -1;
	float impulsePeak = 0.f;
	for (int frame = 0; frame < 24; ++frame) {
		float highRate[2];
		latencyUpsampler.process(frame == 0 ? 1.f : 0.f, highRate);
		const float value = std::fabs(latencyDecimator.process(highRate));
		if (value > impulsePeak) {
			impulsePeak = value;
			impulsePeakIndex = frame;
		}
	}
	assert(impulsePeakIndex == 7);

	// The UI-to-engine grid queue activates a lane without allocating or locking.
	assert(module.enqueueLaneState(0, 1, UINT64_C(1), UINT64_C(1)));
	module.process(args);
	assert(module.program.scenes[0].lanes[1].activeMask == UINT64_C(1));
	assert(std::isfinite(module.outputs[Fray::AUDIO_L_OUTPUT].getVoltage()));
	assert(std::isfinite(module.outputs[Fray::AUDIO_R_OUTPUT].getVoltage()));

	// Internal transport advances and the panel reset returns to cell zero.
	module.params[Fray::TEMPO_PARAM].setValue(std::log2(300.f / 120.f));
	module.params[Fray::DIVISIONS_PARAM].setValue(8.f);
	for (int frame = 1; frame < 1400; ++frame) {
		args.frame = frame;
		module.process(args);
	}
	assert(module.transport.currentCell() > 0);
	module.params[Fray::RESET_PARAM].setValue(1.f);
	module.process(args);
	module.params[Fray::RESET_PARAM].setValue(0.f);
	module.process(args);
	assert(module.transport.currentCell() == 0);

	// Reducing a scene length hides later authored cells without deleting them.
	const uint64_t farCell = UINT64_C(1) << 63;
	module.program.scenes[0].lanes[1].activeMask |= farCell;
	module.program.scenes[0].lanes[1].startMask |= farCell;
	module.params[Fray::BEATS_PARAM].setValue(1.f);
	module.params[Fray::DIVISIONS_PARAM].setValue(2.f);
	module.syncParamsToScene();
	assert(module.program.scenes[0].lanes[1].activeMask & farCell);

	// A Rack module snapshot carries the complete 128-scene bank.
	json_t* state = module.dataToJson();
	assert(state && json_is_object(state));
	json_t* scenes = json_object_get(state, "scenes");
	assert(scenes && json_is_array(scenes));
	assert(json_array_size(scenes) == ShortwavDSP::Fray::kSceneCount);

	Fray restored;
	restored.onSampleRateChange(sampleRateEvent);
	restored.dataFromJson(state);
	assert(restored.program.scenes[0].lanes[1].activeMask & UINT64_C(1));
	assert(restored.program.scenes[0].lanes[1].activeMask & farCell);
	assert(restored.program.scenes[0].lanes[1].startMask & UINT64_C(1));
	json_decref(state);

	// A complete effect-order permutation is published to the engine atomically.
	uint64_t swappedOrder = 0;
	const int order[ShortwavDSP::Fray::kEffectCount] = {1, 0, 2, 3, 4, 5, 6, 7, 8, 9};
	for (int position = 0; position < ShortwavDSP::Fray::kEffectCount; ++position)
		swappedOrder |= static_cast<uint64_t>(order[position]) << (position * 4);
	restored.uiEffectOrderPacked.store(swappedOrder);
	for (int frame = 0; frame < 64; ++frame)
		restored.process(args);
	bool seen[ShortwavDSP::Fray::kEffectCount] = {};
	for (int position = 0; position < ShortwavDSP::Fray::kEffectCount; ++position) {
		const int effect = restored.program.effectOrder[position];
		assert(effect >= 0 && effect < ShortwavDSP::Fray::kEffectCount);
		assert(!seen[effect]);
		seen[effect] = true;
	}

	// A one-shot scene closes lanes on its final cell instead of holding the
	// last effect gate forever.
	Fray oneShot;
	oneShot.onSampleRateChange(sampleRateEvent);
	oneShot.params[Fray::TEMPO_PARAM].setValue(std::log2(300.f / 120.f));
	oneShot.params[Fray::BEATS_PARAM].setValue(1.f);
	oneShot.params[Fray::DIVISIONS_PARAM].setValue(2.f);
	oneShot.params[Fray::LOOP_PARAM].setValue(0.f);
	assert(oneShot.enqueueLaneState(0, 1, UINT64_C(2), UINT64_C(2)));
	for (int frame = 0; frame < 11000; ++frame) {
		args.frame = frame;
		oneShot.process(args);
	}
	assert(oneShot.transport.hasEnded());
	for (int effect = 0; effect < ShortwavDSP::Fray::kEffectCount; ++effect) {
		assert(!oneShot.effectActive[effect]);
		assert(oneShot.outputs[Fray::ACTIVITY_OUTPUT].getVoltage(effect) == 0.f);
	}

	// Malformed state restores every non-Param mirror. Rack Initialize exercises
	// the same assignments after Rack's base reset callback in a running host.
	oneShot.sceneCvMode.store(1);
	oneShot.resetOnRun.store(true);
	oneShot.macroTargets[0].store(42);
	oneShot.uiEffectOrderPacked.store(swappedOrder);
	oneShot.dataFromJson(NULL);
	assert(oneShot.sceneCvMode.load() == 0);
	assert(!oneShot.resetOnRun.load());
	assert(oneShot.macroTargets[0].load() == -1);

	// Epoch changes reject pre-randomization grid edits and restore optimistic UI
	// mirrors instead of letting stale commands overwrite the new authored state.
	Fray epochTest;
	epochTest.onSampleRateChange(sampleRateEvent);
	const uint32_t oldEpoch = epochTest.gridEpoch.load();
	assert(epochTest.enqueueLaneStateAtEpoch(0, 1, UINT64_C(4), UINT64_C(4), oldEpoch));
	epochTest.gridEpoch.fetch_add(1);
	epochTest.process(args);
	assert(epochTest.program.scenes[0].lanes[1].activeMask == 0);
	assert(epochTest.uiActiveMasks[1].load() == 0);
	assert(!epochTest.enqueueLaneStateAtEpoch(0, 1, UINT64_C(8), UINT64_C(8), oldEpoch));
	assert(epochTest.enqueueConditionalLaneState(
		0, 1, UINT64_C(4), UINT64_C(4), UINT64_C(8), UINT64_C(8)));
	epochTest.process(args);
	assert(epochTest.program.scenes[0].lanes[1].activeMask == 0);
	assert(epochTest.enqueueConditionalLaneState(
		0, 1, 0, 0, UINT64_C(8), UINT64_C(8)));
	epochTest.process(args);
	assert(epochTest.program.scenes[0].lanes[1].activeMask == UINT64_C(8));
	epochTest.gridEpoch.fetch_add(1);
	assert(epochTest.enqueueConditionalLaneState(
		0, 1, UINT64_C(8), UINT64_C(8), 0, 0));
	epochTest.process(args);
	assert(epochTest.program.scenes[0].lanes[1].activeMask == 0);

	// Even when a short authored-state snapshot owns the guard, a one-sample
	// external clock pulse is latched and consumed on the next full callback.
	Fray deferredClock;
	deferredClock.onSampleRateChange(sampleRateEvent);
	deferredClock.params[Fray::EXT_CLOCK_MODE_PARAM].setValue(0.f);
	deferredClock.inputs[Fray::CLOCK_INPUT].channels = 1;
	deferredClock.inputs[Fray::CLOCK_INPUT].setVoltage(0.f);
	deferredClock.process(args);
	deferredClock.inputs[Fray::CLOCK_INPUT].setVoltage(10.f);
	assert(!deferredClock.authoredStateLock.test_and_set());
	deferredClock.process(args);
	assert(deferredClock.deferredClockEdge);
	deferredClock.authoredStateLock.clear();
	deferredClock.inputs[Fray::CLOCK_INPUT].setVoltage(0.f);
	deferredClock.process(args);
	assert(deferredClock.transport.currentCell() == 1);

	// Rack can serialize under a shared engine lock while the audio callback is
	// active. Exercise that overlap, including handoff of the grid queue's single
	// consumer between process() and dataToJson().
	Fray concurrent;
	concurrent.onSampleRateChange(sampleRateEvent);
	std::atomic<bool> start(false);
	std::thread audioThread([&]() {
		Module::ProcessArgs audioArgs = args;
		while (!start.load(std::memory_order_acquire)) {}
		for (int frame = 0; frame < 20000; ++frame) {
			audioArgs.frame = frame;
			concurrent.process(audioArgs);
		}
	});
	start.store(true, std::memory_order_release);
	for (int snapshot = 0; snapshot < 8; ++snapshot) {
		const uint64_t mask = UINT64_C(1) << snapshot;
		assert(concurrent.enqueueLaneState(0, 1, mask, mask));
		json_t* concurrentState = concurrent.dataToJson();
		assert(concurrentState && json_is_object(concurrentState));
		json_t* snapshotScenes = json_object_get(concurrentState, "scenes");
		json_t* snapshotScene = json_array_get(snapshotScenes, 0);
		json_t* snapshotLanes = json_object_get(snapshotScene, "lanes");
		json_t* snapshotLane = json_array_get(snapshotLanes, 1);
		assert(static_cast<uint64_t>(json_integer_value(json_array_get(snapshotLane, 0))) == mask);
		json_decref(concurrentState);
	}
	audioThread.join();
	assert(concurrent.transport.currentCell() > 0);

	std::cout << "Fray Rack adapter tests passed\n";
	return 0;
}
