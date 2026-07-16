#include "../dsp/korupt-dsp.h"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>

struct TestContext {
	int passed = 0;
	int failed = 0;

	void run(void (*test)()) {
		test();
		++passed;
	}

	void summary() const {
		std::printf("\n[TEST SUMMARY] passed=%d failed=%d\n", passed, failed);
	}
};

static void testIntervalTables() {
	for (int i = 0; i < 8; i++) {
		assert(ShortwavDSP::KoruptDSP::oscillatorMultiplierForProgram(i) == i + 1);
		assert(ShortwavDSP::KoruptDSP::subharmonicDivisorForProgram(i) == i + 2);
	}
	assert(ShortwavDSP::KoruptDSP::oscillatorMultiplierForProgram(-8) == 1);
	assert(ShortwavDSP::KoruptDSP::oscillatorMultiplierForProgram(99) == 8);
	assert(ShortwavDSP::KoruptDSP::subharmonicDivisorForProgram(-8) == 2);
	assert(ShortwavDSP::KoruptDSP::subharmonicDivisorForProgram(99) == 9);
}

static void testAudioPotLaw() {
	assert(ShortwavDSP::KoruptDSP::audioPotLaw(0.f) == 0.f);
	assert(ShortwavDSP::KoruptDSP::audioPotLaw(1.f) == 1.f);
	assert(ShortwavDSP::KoruptDSP::audioPotLaw(0.5f) < 0.5f);
	assert(ShortwavDSP::KoruptDSP::audioPotLaw(1.f) * 3.f > ShortwavDSP::KoruptDSP::audioPotLaw(1.f));
}

static void testInputStageConditioning() {
	ShortwavDSP::KoruptInputStage inputStage;
	inputStage.setSampleRate(48000.f);
	inputStage.reset();

	float minComparator = 1.f;
	float maxComparator = -1.f;
	float maxEnvelope = 0.f;

	for (int i = 0; i < 48000; i++) {
		const float phase = static_cast<float>(i) * 6.28318530718f * 110.f / 48000.f;
		const float input = std::sin(phase) * 0.65f;
		const ShortwavDSP::KoruptConditionedInput conditioned = inputStage.process(input);
		assert(std::isfinite(conditioned.audio));
		assert(std::isfinite(conditioned.comparator));
		assert(std::isfinite(conditioned.envelope));
		minComparator = std::min(minComparator, conditioned.comparator);
		maxComparator = std::max(maxComparator, conditioned.comparator);
		maxEnvelope = std::max(maxEnvelope, conditioned.envelope);
	}

	assert(minComparator < -0.2f);
	assert(maxComparator > 0.2f);
	assert(maxEnvelope > 0.1f);
}

static void testCmos4024CounterBitsAndRisingEdges() {
	ShortwavDSP::Cmos4024Counter counter;
	counter.reset();

	assert(!counter.outputHigh(0));
	assert(!counter.outputRising(0));

	counter.clock();
	assert(counter.outputHigh(0));
	assert(counter.outputRising(0));
	assert(!counter.outputHigh(1));

	counter.clock();
	assert(!counter.outputHigh(0));
	assert(!counter.outputRising(0));
	assert(counter.outputHigh(1));
	assert(counter.outputRising(1));

	for (int i = 0; i < 2; i++) {
		counter.clock();
	}
	assert(counter.outputHigh(2));
	assert(counter.outputRising(2));
}

static void testCmos4017CounterPulseAndResetChange() {
	ShortwavDSP::Cmos4017Counter counter;
	counter.reset();

	assert(counter.decodedOutput() < 0.f);
	assert(!counter.clock(3));
	assert(counter.decodedOutput() < 0.f);
	assert(!counter.clock(3));
	assert(counter.decodedOutput() < 0.f);
	assert(counter.clock(3));
	assert(counter.decodedOutput() > 0.f);
	assert(!counter.clock(3));
	assert(counter.decodedOutput() < 0.f);

	assert(!counter.clock(5));
	assert(!counter.clock(5));
	assert(!counter.clock(5));
	assert(!counter.clock(5));
	assert(counter.clock(5));
	assert(counter.decodedOutput() > 0.f);
}

static void testSilenceStaysFinite() {
	ShortwavDSP::KoruptDSP engine;
	engine.setSampleRate(48000.f);
	engine.reset();

	ShortwavDSP::KoruptParams params;
	for (int i = 0; i < 48000; i++) {
		const ShortwavDSP::KoruptResult result = engine.process(0.f, false, false, params);
		assert(std::isfinite(result.mixed));
		assert(std::isfinite(result.lock));
		assert(std::abs(result.mixed) < 0.05f);
	}
}

static ShortwavDSP::KoruptResult runSquareInput(ShortwavDSP::KoruptDSP& engine, const ShortwavDSP::KoruptParams& params, int sample) {
	const int phase = sample % 120;
	const bool high = phase < 60;
	const bool rising = phase == 0;
	const float input = high ? 1.f : -1.f;
	return engine.process(input, rising, high, params);
}

static void testDrivenInputProducesVoices() {
	ShortwavDSP::KoruptDSP engine;
	engine.setSampleRate(48000.f);
	engine.reset();

	ShortwavDSP::KoruptParams params;
	params.oscillatorProgram = 4;
	params.subharmonicProgram = 2;
	params.rate = 0.65f;

	int oscillatorTransitions = 0;
	int subTransitions = 0;
	float lastOscillator = 0.f;
	float lastSubharmonic = 0.f;

	for (int i = 0; i < 48000; i++) {
		const int phase = i % 120;
		const bool high = phase < 60;
		const bool rising = phase == 0;
		const float input = high ? 1.f : -1.f;
		const ShortwavDSP::KoruptResult result = engine.process(input, rising, high, params);
		if (i > 0 && result.oscillator * lastOscillator < 0.f) {
			oscillatorTransitions++;
		}
		if (i > 0 && result.subharmonic * lastSubharmonic < 0.f) {
			subTransitions++;
		}
		lastOscillator = result.oscillator;
		lastSubharmonic = result.subharmonic;
	}

	assert(oscillatorTransitions > 20);
	assert(subTransitions > 5);
}

static void testVoiceMixControlsGateMixedOutput() {
	ShortwavDSP::KoruptParams mutedParams;
	mutedParams.squareMix = 0.f;
	mutedParams.subharmonicMix = 0.f;
	mutedParams.oscillatorMix = 0.f;
	mutedParams.level = 1.f;

	ShortwavDSP::KoruptParams squareParams = mutedParams;
	squareParams.squareMix = 1.f;

	ShortwavDSP::KoruptDSP mutedEngine;
	mutedEngine.setSampleRate(48000.f);
	mutedEngine.reset();

	ShortwavDSP::KoruptDSP squareEngine;
	squareEngine.setSampleRate(48000.f);
	squareEngine.reset();

	float mutedPeak = 0.f;
	float squarePeak = 0.f;
	float isolatedSquarePeak = 0.f;
	for (int i = 0; i < 48000; i++) {
		const ShortwavDSP::KoruptResult muted = runSquareInput(mutedEngine, mutedParams, i);
		const ShortwavDSP::KoruptResult square = runSquareInput(squareEngine, squareParams, i);
		mutedPeak = std::max(mutedPeak, std::abs(muted.mixed));
		squarePeak = std::max(squarePeak, std::abs(square.mixed));
		isolatedSquarePeak = std::max(isolatedSquarePeak, std::abs(square.square));
	}

	assert(mutedPeak < 0.001f);
	assert(isolatedSquarePeak > 0.75f);
	assert(squarePeak > 0.2f);
}

static void testExtremeParamsStayFiniteAndBounded() {
	ShortwavDSP::KoruptDSP engine;
	engine.setSampleRate(48000.f);
	engine.reset();

	ShortwavDSP::KoruptParams params;
	params.level = 12.f;
	params.squareMix = -4.f;
	params.oscillatorMix = 9.f;
	params.subharmonicMix = 6.f;
	params.rate = 5.f;
	params.oscillatorProgram = 99;
	params.oscillatorRoot = 99;
	params.subharmonicProgram = -99;
	params.subharmonicRoot = 99;
	params.vibratoMode = true;

	for (int i = 0; i < 48000; i++) {
		const ShortwavDSP::KoruptResult result = runSquareInput(engine, params, i);
		assert(std::isfinite(result.mixed));
		assert(std::isfinite(result.square));
		assert(std::isfinite(result.oscillator));
		assert(std::isfinite(result.subharmonic));
		assert(std::isfinite(result.lock));
		assert(std::isfinite(result.tracking));
		assert(std::isfinite(result.glitch));
		assert(std::abs(result.mixed) <= 1.3501f);
		assert(result.lock >= 0.f && result.lock <= 1.f);
		assert(result.tracking >= 0.f && result.tracking <= 1.f);
		assert(result.glitch >= 0.f && result.glitch <= 1.f);
	}
}

static void testInputRootChoiceAffectsSubharmonicVoice() {
	ShortwavDSP::KoruptParams inputRootParams;
	inputRootParams.subharmonicMix = 1.f;
	inputRootParams.subharmonicProgram = 0;
	inputRootParams.subharmonicRoot = 0;
	inputRootParams.oscillatorProgram = 7;
	inputRootParams.rate = 0.8f;

	ShortwavDSP::KoruptParams oscillatorRootParams = inputRootParams;
	oscillatorRootParams.subharmonicRoot = 1;

	ShortwavDSP::KoruptDSP inputRootEngine;
	inputRootEngine.setSampleRate(48000.f);
	inputRootEngine.reset();

	ShortwavDSP::KoruptDSP oscillatorRootEngine;
	oscillatorRootEngine.setSampleRate(48000.f);
	oscillatorRootEngine.reset();

	float differenceSum = 0.f;
	for (int i = 0; i < 48000; i++) {
		const ShortwavDSP::KoruptResult inputRoot = runSquareInput(inputRootEngine, inputRootParams, i);
		const ShortwavDSP::KoruptResult oscillatorRoot = runSquareInput(oscillatorRootEngine, oscillatorRootParams, i);
		if (i > 24000) {
			differenceSum += std::abs(inputRoot.subharmonic - oscillatorRoot.subharmonic);
		}
	}

	assert(differenceSum > 500.f);
}

static void testTrackingFollowsInputEnvelope() {
	ShortwavDSP::KoruptDSP engine;
	engine.setSampleRate(48000.f);
	engine.reset();

	ShortwavDSP::KoruptParams params;

	float drivenTracking = 0.f;
	for (int i = 0; i < 240000; i++) {
		ShortwavDSP::KoruptResult result;
		if (i < 48000) {
			result = runSquareInput(engine, params, i);
			if (i > 24000) {
				drivenTracking = std::max(drivenTracking, result.tracking);
			}
		}
		else {
			result = engine.process(0.f, false, false, params);
		}

		assert(std::isfinite(result.tracking));
	}

	const ShortwavDSP::KoruptResult silentResult = engine.process(0.f, false, false, params);
	assert(drivenTracking > 0.95f);
	assert(silentResult.tracking < 0.05f);
}

int main() {
	TestContext ctx;

	std::printf("\n=== Korupt DSP Unit Tests ===\n\n");

	std::printf("--- Korupt Utility Tests ---\n");
	ctx.run(testIntervalTables);
	ctx.run(testAudioPotLaw);

	std::printf("--- Korupt Input Stage Tests ---\n");
	ctx.run(testInputStageConditioning);

	std::printf("--- Korupt CMOS Counter Tests ---\n");
	ctx.run(testCmos4024CounterBitsAndRisingEdges);
	ctx.run(testCmos4017CounterPulseAndResetChange);

	std::printf("--- Korupt Engine Stability Tests ---\n");
	ctx.run(testSilenceStaysFinite);
	ctx.run(testExtremeParamsStayFiniteAndBounded);

	std::printf("--- Korupt Voice Behavior Tests ---\n");
	ctx.run(testDrivenInputProducesVoices);
	ctx.run(testVoiceMixControlsGateMixedOutput);
	ctx.run(testInputRootChoiceAffectsSubharmonicVoice);
	ctx.run(testTrackingFollowsInputEnvelope);

	std::printf("\n");
    ctx.summary();
    std::printf("\n");
	return 0;
}
