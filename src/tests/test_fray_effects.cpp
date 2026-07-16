#include "../dsp/fray-effects.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>

using namespace ShortwavDSP::Fray;

namespace {

bool frameIsFinite(const StereoFrame& frame) {
	return std::isfinite(frame.left) && std::isfinite(frame.right);
}

bool frameIsBounded(const StereoFrame& frame) {
	return frameIsFinite(frame) && std::fabs(frame.left) <= 16.001f && std::fabs(frame.right) <= 16.001f;
}

bool nearlyEqual(float a, float b, float tolerance = 1.0e-6f) {
	return std::fabs(a - b) <= tolerance;
}

float testSignal(std::size_t sample, float sampleRate) {
	const float t = static_cast<float>(sample) / sampleRate;
	const float saw = static_cast<float>(sample % 997u) / 498.5f - 1.f;
	return 0.52f * std::sin(kFrayTwoPi * 173.f * t) + 0.21f * saw;
}

EffectSettings settingsFor(EffectId effect) {
	EffectSettings settings;
	settings.values.fill(0.5f);
	if (effect == MODULATOR) {
		settings.values[0] = 0.35f;
		settings.values[1] = 0.8f;
		settings.values[2] = 0.15f;
		settings.values[3] = 0.75f;
	}
	else if (effect == TAPE_STOP) {
		settings.values[0] = 0.5f;
		settings.values[1] = 0.65f;
		settings.values[2] = 0.55f;
		settings.values[3] = 0.2f;
		settings.values[7] = 0.2f;
	}
	else if (effect == RETRIGGER) {
		settings.values[0] = 0.45f;
		settings.values[1] = 0.75f;
		settings.values[2] = 0.4f;
		settings.values[3] = 0.15f;
		settings.values[4] = 0.05f;
		settings.values[7] = 0.05f;
	}
	else if (effect == REVERSER) {
		settings.values[0] = 0.02f;
		settings.values[1] = 0.1f;
		settings.values[2] = 0.9f;
		settings.values[3] = 0.4f;
		settings.values[4] = 0.f;
		settings.values[6] = 1.f;
		settings.values[7] = 0.02f;
	}
	else if (effect == STRETCHER) {
		settings.values[0] = 0.65f;
		settings.values[1] = 0.18f;
		settings.values[2] = 0.9f;
		settings.values[3] = 0.75f;
		settings.values[4] = 0.3f;
	}
	else if (effect == LOFI) {
		settings.values[0] = 0.f;
		settings.values[1] = 0.25f;
		settings.values[2] = 0.35f;
		settings.values[3] = 0.2f;
	}
	else if (effect == DISTORTION) {
		settings.values[0] = 0.35f;
		settings.values[1] = 0.65f;
		settings.values[2] = 0.55f;
		settings.values[3] = 1.f;
		settings.values[4] = 0.f;
	}
	else if (effect == GATER) {
		settings.values[0] = 0.15f;
		settings.values[1] = 0.2f;
		settings.values[2] = 1.f;
		settings.values[3] = 0.9f;
		settings.values[4] = 0.1f;
		settings.values[5] = 0.6f;
		settings.values[6] = 0.25f;
		settings.values[7] = 0.1f;
	}
	else if (effect == DELAY) {
		settings.values[0] = 0.f;
		settings.values[1] = 1.f;
		settings.values[2] = 0.5f;
		settings.values[3] = 0.9f;
		settings.values[4] = 0.7f;
		settings.values[5] = 0.8f;
		settings.values[6] = 0.f;
		settings.values[7] = 0.f;
		settings.values[8] = 0.f;
	}
	else if (effect == SHUFFLER) {
		settings.values[0] = 0.f;
		settings.values[1] = 0.2f;
		settings.values[2] = 1.f;
		settings.values[3] = 1.f;
		settings.values[4] = 0.2f;
		settings.values[5] = 0.5f;
		settings.values[6] = 0.25f;
		settings.values[7] = 0.05f;
	}
	return settings;
}

void testFiniteHelpersAndHistory() {
	const float nan = std::numeric_limits<float>::quiet_NaN();
	assert(finiteOrSilence(nan) == 0.f);
	assert(clampFinite(nan, -1.f, 1.f, 0.25f) == 0.25f);
	assert(clamp01(-10.f) == 0.f);
	assert(clamp01(10.f) == 1.f);
	assert(frameIsBounded(sanitizeFrame(StereoFrame(nan, 1.0e20f))));

	StereoCircularHistory history;
	history.prepare(48000.f, 0.01f);
	for (std::size_t i = 0; i < history.capacitySamples() * 3u; ++i) {
		const float value = std::sin(static_cast<float>(i) * 0.07f);
		history.push(StereoFrame(value, -value));
		const StereoFrame read = history.readDelay(2.25f + static_cast<float>(i % 200u));
		assert(frameIsBounded(read));
	}
	history.reset();
	assert(history.availableSamples() == 0u);
	assert(history.readDelay(20.f).left == 0.f);

	StereoCapture capture;
	capture.prepare(48000.f, 0.01f);
	capture.beginCapture(8u);
	capture.capture(StereoFrame(0.25f, -0.25f));
	assert(frameIsBounded(capture.readWrapped(std::numeric_limits<float>::infinity())));
	assert(frameIsBounded(capture.readWrapped(-std::numeric_limits<float>::infinity())));

	StereoCapture adjacent;
	adjacent.prepare(8000.f, 0.01f);
	for (int i = 0; i < 16; ++i)
		adjacent.idleTick(StereoFrame(static_cast<float>(i), -static_cast<float>(i)));
	adjacent.beginCapture(8u);
	assert(std::fabs(adjacent.readWrapped(0.f).left - 8.f) < 1e-6f);
	for (int i = 0; i < 8; ++i)
		adjacent.idleTick(StereoFrame(4.f + static_cast<float>(i) * 0.1f, 0.f));
	adjacent.beginCapture(8u);
	assert(std::fabs(adjacent.readWrapped(0.f).left - 4.f) < 1e-6f);
}

void testRackSlewAndExponentialPrimitives() {
	const float sampleRates[] = {
		8000.f, 11025.f, 12000.f, 22050.f, 24000.f,
		44100.f, 48000.f, 96000.f, 192000.f, 384000.f, 768000.f
	};
	for (std::size_t rateIndex = 0; rateIndex < sizeof(sampleRates) / sizeof(sampleRates[0]); ++rateIndex) {
		const float sampleRate = sampleRates[rateIndex];
		const float sampleTime = 1.f / sampleRate;
		const float rampSamples = sampleRate * 0.004f;
		const int rampLength = static_cast<int>(std::ceil(rampSamples));
		ActivitySlew ramp;
		ramp.prepare(sampleRate, 4.f);
		for (int i = 0; i < rampLength; ++i) {
			const float expected = std::min(1.f, static_cast<float>(i + 1) / rampSamples);
			assert(nearlyEqual(ramp.process(true), expected, 1.0e-4f));
		}
		assert(ramp.value() == 1.f);
		for (int i = 0; i < rampLength; ++i) {
			const float expected = std::max(0.f, 1.f - static_cast<float>(i + 1) / rampSamples);
			assert(nearlyEqual(ramp.process(false), expected, 1.0e-4f));
		}
		assert(ramp.value() <= 1.0e-6f);

		const float tau = 0.017f;
		const float coefficient = sampleTime / tau;
		rack::dsp::ExponentialFilter smoother;
		smoother.setTau(tau);
		float expected = 0.f;
		for (int i = 0; i < 512; ++i) {
			expected += coefficient * (1.f - expected);
			assert(nearlyEqual(smoother.process(sampleTime, 1.f), expected, 2.0e-6f));
		}

		const float attack = 0.003f;
		const float release = 0.025f;
		const float attackCoefficient = sampleTime / attack;
		const float releaseCoefficient = sampleTime / release;
		rack::dsp::ExponentialSlewLimiter envelope;
		envelope.setRiseFallTau(attack, release);
		expected = 0.f;
		for (int i = 0; i < 256; ++i) {
			expected += attackCoefficient * (1.f - expected);
			assert(nearlyEqual(envelope.process(sampleTime, 1.f), expected, 2.0e-6f));
		}
		for (int i = 0; i < 512; ++i) {
			expected += releaseCoefficient * (0.f - expected);
			assert(nearlyEqual(envelope.process(sampleTime, 0.f), expected, 2.0e-6f));
		}
	}

	const float lowRates[] = {8000.f, 11025.f, 12000.f, 22050.f, 24000.f};
	for (std::size_t rateIndex = 0; rateIndex < sizeof(lowRates) / sizeof(lowRates[0]); ++rateIndex) {
		const float sampleTime = 1.f / lowRates[rateIndex];
		const float taus[] = {0.00005f, 0.0001f};
		for (std::size_t tauIndex = 0; tauIndex < sizeof(taus) / sizeof(taus[0]); ++tauIndex) {
			rack::dsp::ExponentialFilter smoother;
			smoother.setTau(rackSafeTau(taus[tauIndex], sampleTime));
			float previous = 0.f;
			for (int frame = 0; frame < 32; ++frame) {
				const float current = smoother.process(sampleTime, 1.f);
				assert(current >= previous);
				assert(current <= 1.f);
				previous = current;
			}
		}
	}
}

void testRackCrossfadeHannAndCommonStage() {
	const StereoFrame dry(-0.5f, 0.25f);
	const StereoFrame wet(0.75f, -0.5f);
	assert(crossfadeFrames(dry, wet, -1.f).left == dry.left);
	assert(crossfadeFrames(dry, wet, 2.f).right == wet.right);
	const StereoFrame midpoint = crossfadeFrames(dry, wet, 0.5f);
	assert(nearlyEqual(midpoint.left, rack::math::crossfade(dry.left, wet.left, 0.5f)));
	assert(nearlyEqual(midpoint.right, rack::math::crossfade(dry.right, wet.right, 0.5f)));
	assert(frameIsFinite(crossfadeFrames(
		StereoFrame(std::numeric_limits<float>::quiet_NaN(), 0.f), wet, 0.5f)));

	const float phases[] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
	for (std::size_t i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
		const float expected = 0.5f - 0.5f * std::cos(kFrayTwoPi * phases[i]);
		assert(nearlyEqual(rack::dsp::hann(phases[i]), expected, 1.0e-6f));
	}

	CommonSettings settings;
	CommonStage stage;
	stage.prepare(48000.f);
	const int rampLength = 144;
	for (int i = 0; i < rampLength; ++i) {
		const StereoFrame output = stage.process(
			StereoFrame(), StereoFrame(1.f, -1.f), settings, true, false, 1.f / 48000.f);
		const float expected = static_cast<float>(i + 1) / static_cast<float>(rampLength);
		assert(nearlyEqual(output.left, expected, 2.0e-6f));
		assert(nearlyEqual(output.right, -expected, 2.0e-6f));
	}
	for (int i = 0; i < rampLength; ++i) {
		const StereoFrame output = stage.process(
			StereoFrame(), StereoFrame(1.f, -1.f), settings, false, false, 1.f / 48000.f);
		const float expected = std::max(0.f,
			1.f - static_cast<float>(i + 1) / static_cast<float>(rampLength));
		assert(nearlyEqual(output.left, expected, 2.0e-6f));
	}

	// The common continuous controls use Rack's 5 ms exponential filters.
	stage.prepare(48000.f);
	settings = CommonSettings();
	for (int i = 0; i < rampLength; ++i) {
		stage.process(StereoFrame(), StereoFrame(1.f, 1.f),
			settings, true, false, 1.f / 48000.f);
	}
	settings.mix = 0.f;
	settings.gain = 0.f;
	const StereoFrame smoothedControlStep = stage.process(
		StereoFrame(), StereoFrame(1.f, 1.f), settings, true, false, 1.f / 48000.f);
	const float onePoleStep = 1.f - (1.f / (0.005f * 48000.f));
	assert(nearlyEqual(smoothedControlStep.left, onePoleStep * onePoleStep, 2.0e-5f));
	assert(nearlyEqual(smoothedControlStep.right, onePoleStep * onePoleStep, 2.0e-5f));

	settings = CommonSettings();
	for (int filterType = 1; filterType <= 4; ++filterType) {
		stage.prepare(48000.f);
		settings.filterType = filterType;
		for (int i = 0; i < 512; ++i) {
			const StereoFrame input(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f);
			const StereoFrame output = stage.process(
				StereoFrame(), input, settings, true, false, 1.f / 48000.f);
			assert(frameIsFinite(output));
			assert(nearlyEqual(output.left, output.right, 1.0e-6f));
		}
	}

	stage.prepare(48000.f);
	settings = CommonSettings();
	for (int i = 0; i < rampLength; ++i) {
		stage.process(StereoFrame(0.2f, 0.2f), StereoFrame(0.5f, 0.5f),
			settings, true, true, 1.f / 48000.f);
	}
	const StereoFrame tail = stage.process(
		StereoFrame(0.2f, 0.2f), StereoFrame(0.3f, 0.3f), settings, false, true, 1.f / 48000.f);
	assert(nearlyEqual(tail.left, 0.3f, 1.0e-6f));

	// Filter bypass/type changes must not revive frozen state.
	stage.prepare(48000.f);
	settings = CommonSettings();
	settings.filterType = 1;
	stage.process(StereoFrame(), StereoFrame(1.f, 1.f), settings, true, false, 1.f / 48000.f);
	settings.filterType = 0;
	for (int i = 0; i < 256; ++i) {
		stage.process(StereoFrame(), StereoFrame(), settings, true, false, 1.f / 48000.f);
	}
	settings.filterType = 1;
	const StereoFrame reenabled = stage.process(
		StereoFrame(), StereoFrame(), settings, true, false, 1.f / 48000.f);
	assert(std::fabs(reenabled.left) < 1.0e-7f);
	assert(std::fabs(reenabled.right) < 1.0e-7f);

	// Delay uses one additive-return topology on both sides of the active edge,
	// including non-default common pan/gain/mix settings.
	stage.prepare(48000.f);
	settings = CommonSettings();
	settings.mix = 0.7f;
	settings.pan = 0.35f;
	settings.gain = 1.4f;
	StereoFrame activeDelay;
	for (int i = 0; i < 1000; ++i) {
		activeDelay = stage.process(
			StereoFrame(0.2f, -0.1f), StereoFrame(0.32f, 0.08f),
			settings, true, true, 1.f / 48000.f);
	}
	const StereoFrame inactiveDelay = stage.process(
		StereoFrame(0.2f, -0.1f), StereoFrame(0.32f, 0.08f),
		settings, false, true, 1.f / 48000.f);
	assert(nearlyEqual(activeDelay.left, inactiveDelay.left, 1.0e-6f));
	assert(nearlyEqual(activeDelay.right, inactiveDelay.right, 1.0e-6f));

	// Rack's float coefficient helper becomes unstable near DC at very high
	// sample rates. Fray keeps Rack's BiquadFilter state/processor in double;
	// exercise the worst-case 20 Hz, Q=10 settings for a full second.
	const float highRates[] = {384000.f, 768000.f};
	for (std::size_t rateIndex = 0; rateIndex < sizeof(highRates) / sizeof(highRates[0]); ++rateIndex) {
		const float sampleRate = highRates[rateIndex];
		for (int filterType = 1; filterType <= 4; ++filterType) {
			stage.prepare(sampleRate);
			settings = CommonSettings();
			settings.filterType = filterType;
			settings.cutoff = 0.f;
			settings.q = 1.f;
			const int sampleCount = static_cast<int>(sampleRate);
			for (int frame = 0; frame < sampleCount; ++frame) {
				const StereoFrame impulse(frame == 0 ? 1.f : 0.f, frame == 0 ? 1.f : 0.f);
				const StereoFrame output = stage.process(
					StereoFrame(), impulse, settings, true, false, 1.f / sampleRate);
				assert(frameIsFinite(output));
				assert(std::fabs(output.left) < 32.f);
				assert(std::fabs(output.right) < 32.f);
			}
		}
	}
}

void testRackBackedDistortionLatencyAndDcBlock() {
	const float sampleRates[] = {
		8000.f, 11025.f, 12000.f, 22050.f, 24000.f,
		44100.f, 48000.f, 96000.f, 192000.f, 384000.f, 768000.f
	};
	for (std::size_t rateIndex = 0; rateIndex < sizeof(sampleRates) / sizeof(sampleRates[0]); ++rateIndex) {
		const float sampleRate = sampleRates[rateIndex];
		FrayEffects raw;
		FrayEffects oversampled;
		raw.prepare(sampleRate);
		oversampled.prepare(sampleRate);
		EffectSettings rawSettings;
		rawSettings.values[0] = 0.f;
		rawSettings.values[1] = 0.f;
		rawSettings.values[2] = 1.f;
		rawSettings.values[3] = 1.f;
		rawSettings.values[4] = 0.f;
		rawSettings.values[5] = 0.5f;
		rawSettings.values[7] = 0.f;
		EffectSettings oversampledSettings = rawSettings;
		oversampledSettings.values[7] = 1.f;
		EffectContext context;
		context.active = true;
		context.sampleRate = sampleRate;
		context.sampleTime = 1.f / sampleRate;
		const int settlingSamples = static_cast<int>(std::ceil(sampleRate * 0.005f));
		for (int frame = 0; frame < settlingSamples; ++frame) {
			raw.process(DISTORTION, StereoFrame(), rawSettings, context);
			oversampled.process(DISTORTION, StereoFrame(), oversampledSettings, context);
		}

		int rawPeakIndex = -1;
		int oversampledPeakIndex = -1;
		float rawPeak = 0.f;
		float oversampledPeak = 0.f;
		for (int frame = 0; frame < 32; ++frame) {
			const StereoFrame input(frame == 0 ? 0.1f : 0.f, 0.f);
			const float rawValue = std::fabs(
				raw.process(DISTORTION, input, rawSettings, context).left);
			const float oversampledValue = std::fabs(
				oversampled.process(DISTORTION, input, oversampledSettings, context).left);
			if (rawValue > rawPeak) {
				rawPeak = rawValue;
				rawPeakIndex = frame;
			}
			if (oversampledValue > oversampledPeak) {
				oversampledPeak = oversampledValue;
				oversampledPeakIndex = frame;
			}
		}
		assert(rawPeakIndex == 0);
		assert(oversampledPeakIndex == 7);
	}

	DistortionEffect dcBlock;
	dcBlock.prepare(48000.f);
	EffectSettings settings;
	settings.values[0] = 0.f;
	settings.values[1] = 0.f;
	settings.values[2] = 0.5f;
	settings.values[3] = 1.f;
	settings.values[4] = 0.f;
	settings.values[5] = 0.75f;
	settings.values[7] = 0.f;
	EffectContext context;
	context.active = true;
	StereoFrame output;
	for (int i = 0; i < 48000; ++i) {
		output = dcBlock.process(StereoFrame(), settings, context);
		assert(frameIsFinite(output));
	}
	assert(std::fabs(output.left) < 1.0e-3f);
	dcBlock.reset();
	settings.values[5] = 0.5f;
	output = dcBlock.process(StereoFrame(), settings, context);
	assert(std::fabs(output.left) < 1.0e-7f);

	// A quality switch transitions only the wet contribution. A deliberately
	// dry-only Distortion remains an exact live stereo path throughout.
	DistortionEffect transparent;
	transparent.prepare(48000.f);
	settings = EffectSettings();
	settings.values[3] = 0.f;
	settings.values[4] = 1.f;
	settings.values[7] = 0.f;
	for (int frame = 0; frame < 512; ++frame) {
		const StereoFrame input(
			std::sin(static_cast<float>(frame) * 0.13f),
			std::cos(static_cast<float>(frame) * 0.17f));
		const StereoFrame dryOutput = transparent.process(input, settings, context);
		assert(nearlyEqual(dryOutput.left, input.left, 1.0e-7f));
		assert(nearlyEqual(dryOutput.right, input.right, 1.0e-7f));
	}
	settings.values[7] = 1.f;
	for (int frame = 0; frame < 256; ++frame) {
		const StereoFrame input(
			std::sin(static_cast<float>(frame + 512) * 0.13f),
			std::cos(static_cast<float>(frame + 512) * 0.17f));
		const StereoFrame dryOutput = transparent.process(input, settings, context);
		assert(nearlyEqual(dryOutput.left, input.left, 1.0e-7f));
		assert(nearlyEqual(dryOutput.right, input.right, 1.0e-7f));
	}

	// Keep the shared DC/tone state alive across Raw/2x changes and pre-roll the
	// newly selected FIR path so a static bias does not create a long transient.
	DistortionEffect biasedSwitch;
	biasedSwitch.prepare(48000.f);
	settings = EffectSettings();
	settings.values[0] = 0.f;
	settings.values[1] = 0.f;
	settings.values[3] = 1.f;
	settings.values[4] = 0.f;
	settings.values[5] = 0.75f;
	settings.values[7] = 0.f;
	for (int frame = 0; frame < 48000; ++frame)
		biasedSwitch.process(StereoFrame(), settings, context);
	settings.values[7] = 1.f;
	float switchPeak = 0.f;
	for (int frame = 0; frame < 4096; ++frame) {
		const StereoFrame switched = biasedSwitch.process(StereoFrame(), settings, context);
		switchPeak = std::max(switchPeak, std::max(
			std::fabs(switched.left), std::fabs(switched.right)));
	}
	assert(switchPeak < 0.05f);
}

void testInactiveIsDry() {
	FrayEffects effects;
	effects.prepare(48000.f);
	EffectContext context;
	context.active = false;
	for (int effect = 0; effect < kEffectCount; ++effect) {
		effects.reset();
		const StereoFrame input(0.37f, -0.21f);
		const StereoFrame output = effects.process(
			static_cast<EffectId>(effect), input, settingsFor(static_cast<EffectId>(effect)), context);
		assert(std::fabs(output.left - input.left) < 1.0e-6f);
		assert(std::fabs(output.right - input.right) < 1.0e-6f);
	}
}

void testActiveSilenceStaysFinite() {
	FrayEffects effects;
	effects.prepare(48000.f);
	for (int effect = 0; effect < kEffectCount; ++effect) {
		effects.reset();
		EffectContext context;
		context.active = true;
		context.sceneSeed = 123u;
		context.eventOrdinal = 7u;
		const EffectId id = static_cast<EffectId>(effect);
		const EffectSettings settings = settingsFor(id);
		for (std::size_t i = 0; i < 6000u; ++i) {
			context.blockStart = i == 0u;
			context.blockEnd = i == 5999u;
			const StereoFrame output = effects.process(id, StereoFrame(), settings, context);
			assert(frameIsBounded(output));
		}
	}
}

void testAllEffectsAtRackSampleRates() {
	const float sampleRates[] = {
		8000.f, 11025.f, 12000.f, 22050.f, 24000.f,
		44100.f, 48000.f, 96000.f, 192000.f, 384000.f, 768000.f
	};
	for (std::size_t rateIndex = 0; rateIndex < sizeof(sampleRates) / sizeof(sampleRates[0]); ++rateIndex) {
		const float sampleRate = sampleRates[rateIndex];
		FrayEffects effects;
		effects.prepare(sampleRate);
		assert(effects.sampleRate() == sampleRate);
		for (int effect = 0; effect < kEffectCount; ++effect) {
			effects.reset();
			const EffectId id = static_cast<EffectId>(effect);
			EffectSettings settings = settingsFor(id);
			EffectContext context;
			context.sampleRate = sampleRate;
			context.sampleTime = 1.f / sampleRate;
			context.bpm = 137.f;
			context.sceneSeed = 0x12345678u;
			context.eventOrdinal = 9u;

			for (std::size_t i = 0; i < 1024u; ++i) {
				context.active = false;
				const StereoFrame output = effects.process(id, StereoFrame(), settings, context);
				assert(frameIsBounded(output));
			}
			for (std::size_t i = 0; i < 8192u; ++i) {
				context.active = true;
				context.blockStart = i == 0u;
				context.blockEnd = i == 8191u;
				const float signal = testSignal(i, sampleRate);
				const StereoFrame output = effects.process(id, StereoFrame(signal, -0.73f * signal), settings, context);
				assert(frameIsBounded(output));
			}
			context.blockStart = false;
			context.blockEnd = false;
			context.active = false;
			for (std::size_t i = 0; i < 1024u; ++i) {
				const StereoFrame output = effects.process(id, StereoFrame(), settings, context);
				assert(frameIsBounded(output));
			}
		}
	}
}

void testLongRunAndHostileParameters() {
	FrayEffects effects;
	effects.prepare(48000.f);
	EffectContext context;
	context.sampleRate = 48000.f;
	context.sampleTime = 1.f / 48000.f;
	context.bpm = std::numeric_limits<float>::infinity();
	context.active = true;
	context.sceneSeed = 0xfeedbeefu;
	context.eventOrdinal = 42u;
	for (int effect = 0; effect < kEffectCount; ++effect) {
		effects.reset();
		EffectSettings settings = settingsFor(static_cast<EffectId>(effect));
		settings.values[0] = std::numeric_limits<float>::quiet_NaN();
		settings.values[1] = std::numeric_limits<float>::infinity();
		settings.values[2] = -std::numeric_limits<float>::infinity();
		for (std::size_t i = 0; i < 30000u; ++i) {
			context.blockStart = i == 0u || i == 15000u;
			context.blockEnd = i == 14999u || i == 29999u;
			const float signal = testSignal(i, 48000.f) * 3.f;
			const StereoFrame output = effects.process(
				static_cast<EffectId>(effect), StereoFrame(signal, signal * -0.37f), settings, context);
			assert(frameIsBounded(output));
		}
	}
}

void testResetClearsFeedbackAndCaptureState() {
	FrayEffects effects;
	effects.prepare(48000.f);
	EffectSettings settings = settingsFor(DELAY);
	EffectContext context;
	context.active = true;
	context.blockStart = true;
	for (std::size_t i = 0; i < 2500u; ++i) {
		const StereoFrame input(i == 300u ? 1.f : 0.f, 0.f);
		const StereoFrame output = effects.process(DELAY, input, settings, context);
		assert(frameIsBounded(output));
		context.blockStart = false;
	}
	effects.reset();
	context.active = false;
	for (std::size_t i = 0; i < 5000u; ++i) {
		const StereoFrame output = effects.process(DELAY, StereoFrame(), settings, context);
		assert(output.left == 0.f);
		assert(output.right == 0.f);
	}
}

void testDelayTailContinuesAfterBlock() {
	FrayEffects effects;
	effects.prepare(48000.f);
	EffectSettings settings = settingsFor(DELAY);
	settings.values[3] = 1.f;
	settings.values[4] = 0.85f;
	settings.values[5] = 1.f;
	settings.values[6] = 0.f;
	settings.values[7] = 0.f;
	settings.values[8] = 0.f;
	EffectContext context;
	context.sampleRate = 48000.f;
	context.sampleTime = 1.f / 48000.f;
	context.bpm = 120.f;
	context.active = true;
	context.blockStart = true;

	for (std::size_t i = 0; i < 300u; ++i) {
		effects.process(DELAY, StereoFrame(), settings, context);
		context.blockStart = false;
	}
	effects.process(DELAY, StereoFrame(1.f, 0.f), settings, context);
	float firstEchoPeak = 0.f;
	for (std::size_t i = 0; i < 1000u; ++i) {
		const StereoFrame output = effects.process(DELAY, StereoFrame(), settings, context);
		firstEchoPeak = std::max(firstEchoPeak, std::max(std::fabs(output.left), std::fabs(output.right)));
	}
	assert(firstEchoPeak > 0.05f);

	context.active = false;
	context.blockEnd = true;
	float inactiveTailPeak = 0.f;
	for (std::size_t i = 0; i < 1200u; ++i) {
		context.blockEnd = i == 0u;
		const StereoFrame output = effects.process(DELAY, StereoFrame(), settings, context);
		inactiveTailPeak = std::max(inactiveTailPeak, std::max(std::fabs(output.left), std::fabs(output.right)));
	}
	assert(inactiveTailPeak > 0.01f);
}

void testCaptureEffectsAreAudibleInOneCell() {
	const EffectId captureEffects[] = {RETRIGGER, REVERSER};
	for (std::size_t effectIndex = 0; effectIndex < 2u; ++effectIndex) {
		FrayEffects effects;
		effects.prepare(48000.f);
		EffectSettings settings;
		EffectContext context;
		context.sampleRate = 48000.f;
		context.sampleTime = 1.f / 48000.f;
		context.bpm = 120.f;

		// Give inactive capture processors the same rolling pre-history they have
		// in the module before a user-authored block reaches the playhead.
		for (std::size_t i = 0; i < 12000u; ++i) {
			const float signal = testSignal(i, 48000.f);
			effects.process(captureEffects[effectIndex], StereoFrame(signal, -signal), settings, context);
		}

		context.active = true;
		context.blockStart = true;
		double wetDifference = 0.0;
		// One default Fray cell at 120 BPM with four divisions is 6000 samples.
		for (std::size_t i = 0; i < 6000u; ++i) {
			const float signal = testSignal(i + 12000u, 48000.f);
			const StereoFrame input(signal, -signal);
			const StereoFrame output = effects.process(captureEffects[effectIndex], input, settings, context);
			wetDifference += std::fabs(static_cast<double>(output.left - input.left));
			wetDifference += std::fabs(static_cast<double>(output.right - input.right));
			context.blockStart = false;
		}
		assert(wetDifference > 1.0);
	}
}

void testDeterministicEffect(EffectId effect) {
	FrayEffects first;
	FrayEffects second;
	FrayEffects different;
	first.prepare(48000.f);
	second.prepare(48000.f);
	different.prepare(48000.f);
	EffectSettings settings = settingsFor(effect);
	if (effect == STRETCHER) {
		settings.values[2] = 1.f;
	}
	else {
		settings.values[3] = 1.f;
		settings.values[4] = 0.f;
		settings.values[5] = 0.5f;
	}

	EffectContext firstContext;
	EffectContext secondContext;
	EffectContext differentContext;
	firstContext.sceneSeed = secondContext.sceneSeed = 0xabcddcbaULL;
	differentContext.sceneSeed = 0xdeadbeefULL;
	firstContext.eventOrdinal = secondContext.eventOrdinal = differentContext.eventOrdinal = 17u;

	for (std::size_t i = 0; i < 22000u; ++i) {
		const float signal = testSignal(i, 48000.f);
		const StereoFrame input(signal, testSignal(i + 123u, 48000.f));
		first.process(effect, input, settings, firstContext);
		second.process(effect, input, settings, secondContext);
		different.process(effect, input, settings, differentContext);
	}

	firstContext.active = secondContext.active = differentContext.active = true;
	firstContext.blockStart = secondContext.blockStart = differentContext.blockStart = true;
	double differentSeedDifference = 0.0;
	for (std::size_t i = 0; i < 30000u; ++i) {
		const float signal = testSignal(i + 22000u, 48000.f);
		const StereoFrame input(signal, testSignal(i + 22777u, 48000.f));
		const StereoFrame a = first.process(effect, input, settings, firstContext);
		const StereoFrame b = second.process(effect, input, settings, secondContext);
		const StereoFrame c = different.process(effect, input, settings, differentContext);
		assert(frameIsBounded(a));
		assert(frameIsBounded(b));
		assert(std::fabs(a.left - b.left) < 1.0e-7f);
		assert(std::fabs(a.right - b.right) < 1.0e-7f);
		differentSeedDifference += std::fabs(static_cast<double>(a.left - c.left));
		differentSeedDifference += std::fabs(static_cast<double>(a.right - c.right));
		firstContext.blockStart = secondContext.blockStart = differentContext.blockStart = false;
	}
	assert(differentSeedDifference > 0.01);
}

void testDeterministicStretcherAndShuffler() {
	testDeterministicEffect(STRETCHER);
	testDeterministicEffect(SHUFFLER);
}

void testOverlappingEffectOrderChangesAudio() {
	FrayEffects modThenDist;
	FrayEffects distThenMod;
	modThenDist.prepare(48000.f);
	distThenMod.prepare(48000.f);
	const EffectSettings modulator = settingsFor(MODULATOR);
	const EffectSettings distortion = settingsFor(DISTORTION);
	EffectContext context;
	context.sampleRate = 48000.f;
	context.sampleTime = 1.f / 48000.f;
	context.active = true;
	double difference = 0.0;
	for (std::size_t frame = 0; frame < 8192u; ++frame) {
		context.blockStart = frame == 0u;
		const StereoFrame input(
			testSignal(frame, 48000.f), testSignal(frame + 137u, 48000.f));
		const StereoFrame modFirst = modThenDist.process(
			MODULATOR, input, modulator, context);
		const StereoFrame a = modThenDist.process(
			DISTORTION, modFirst, distortion, context);
		const StereoFrame distFirst = distThenMod.process(
			DISTORTION, input, distortion, context);
		const StereoFrame b = distThenMod.process(
			MODULATOR, distFirst, modulator, context);
		difference += std::fabs(static_cast<double>(a.left - b.left));
		difference += std::fabs(static_cast<double>(a.right - b.right));
	}
	assert(difference > 1.0);
}

} // namespace

int main() {
	testFiniteHelpersAndHistory();
	testRackSlewAndExponentialPrimitives();
	testRackCrossfadeHannAndCommonStage();
	testRackBackedDistortionLatencyAndDcBlock();
	testInactiveIsDry();
	testActiveSilenceStaysFinite();
	testAllEffectsAtRackSampleRates();
	testLongRunAndHostileParameters();
	testResetClearsFeedbackAndCaptureState();
	testDelayTailContinuesAfterBlock();
	testCaptureEffectsAreAudibleInOneCell();
	testDeterministicStretcherAndShuffler();
	testOverlappingEffectOrderChangesAudio();
	std::cout << "Fray effect tests passed\n";
	return 0;
}
