#include "../dsp/korupt-dsp.h"

#include <cassert>
#include <cmath>
#include <iostream>

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

int main() {
	testIntervalTables();
	testSilenceStaysFinite();
	testDrivenInputProducesVoices();
	std::cout << "Korupt DSP tests passed\n";
	return 0;
}
