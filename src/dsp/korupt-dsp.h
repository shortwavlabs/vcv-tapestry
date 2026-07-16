#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#include <dsp/filter.hpp>
#include <math.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ShortwavDSP {

inline float normalizedCutoff(float frequency, float sampleRate) {
	return rack::math::clamp(frequency / std::max(1.f, sampleRate), 0.f, 0.49f);
}

struct KoruptParams {
	float level = 0.8f;
	float squareMix = 0.7f;
	float oscillatorMix = 0.7f;
	float subharmonicMix = 0.7f;
	float rate = 0.35f;
	int oscillatorProgram = 0;
	int oscillatorRoot = 0;
	int subharmonicProgram = 0;
	int subharmonicRoot = 0;
	bool vibratoMode = false;
};

struct KoruptResult {
	float mixed = 0.f;
	float square = 0.f;
	float oscillator = 0.f;
	float subharmonic = 0.f;
	float lock = 0.f;
	float tracking = 0.f;
	float glitch = 0.f;
};

struct KoruptConditionedInput {
	float audio = 0.f;
	float comparator = 0.f;
	float envelope = 0.f;
};

class KoruptInputStage {
public:
	void setSampleRate(float sampleRate) {
		sampleRate_ = std::max(1.f, sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		inputCoupling_.setCutoffFreq(normalizedCutoff(18.f, sampleRate_));
		preFilter_.setCutoffFreq(normalizedCutoff(5200.f, sampleRate_));
		inverter1Filter_.setCutoffFreq(normalizedCutoff(18000.f, sampleRate_));
		inverter2Filter_.setCutoffFreq(normalizedCutoff(18000.f, sampleRate_));
		inverter3Filter_.setCutoffFreq(normalizedCutoff(18000.f, sampleRate_));
		envelope_.setRiseFallTau(0.0015f, 0.13f);
	}

	void reset() {
		inputCoupling_.reset();
		preFilter_.reset();
		inverter1Filter_.reset();
		inverter2Filter_.reset();
		inverter3Filter_.reset();
		envelope_.reset();
	}

	KoruptConditionedInput process(float rackAudio) {
		const float input = rack::math::clamp(rackAudio, -2.f, 2.f);

		// Coupling cap into the pedal input, followed by a practical guitar-band limiter.
		inputCoupling_.process(input);
		const float coupled = inputCoupling_.highpass();
		preFilter_.process(coupled);
		const float preFiltered = preFilter_.lowpass();

		const float driven = rack::math::clamp(preFiltered * 3.4f, -3.5f, 3.5f);
		const float inverter1 = cmosInverter(driven, inverter1Filter_, 2.6f, -0.035f);
		const float inverter2 = cmosInverter(inverter1, inverter2Filter_, 3.1f, 0.018f);
		const float inverter3 = cmosInverter(inverter2, inverter3Filter_, 4.2f, -0.006f);

		const float envTarget = std::min(1.f, std::abs(preFiltered) * 1.8f);
		const float env = envelope_.process(sampleTime_, envTarget);

		KoruptConditionedInput result;
		result.audio = rack::math::clamp(inverter2, -1.25f, 1.25f);
		result.comparator = rack::math::clamp(inverter3, -1.25f, 1.25f);
		result.envelope = rack::math::clamp(env);
		return result;
	}

private:
	float cmosInverter(float input, rack::dsp::RCFilter& filter, float drive, float bias) {
		const float transfer = -std::tanh((input + bias) * drive);
		const float skew = transfer + 0.055f * transfer * transfer - 0.025f;
		filter.process(rack::math::clamp(skew, -1.12f, 1.08f));
		return filter.lowpass();
	}

	float sampleRate_ = 44100.f;
	float sampleTime_ = 1.f / 44100.f;
	rack::dsp::RCFilter inputCoupling_;
	rack::dsp::RCFilter preFilter_;
	rack::dsp::RCFilter inverter1Filter_;
	rack::dsp::RCFilter inverter2Filter_;
	rack::dsp::RCFilter inverter3Filter_;
	rack::dsp::ExponentialSlewLimiter envelope_;
};

class Cmos4024Counter {
public:
	void reset() {
		count_ = 0;
		previous_ = 0;
	}

	void clock() {
		previous_ = count_;
		count_ = static_cast<uint8_t>((count_ + 1) & 0x7f);
	}

	bool outputHigh(int bit) const {
		const uint8_t mask = static_cast<uint8_t>(1u << rack::math::clamp(bit, 0, 6));
		return (count_ & mask) != 0;
	}

	bool outputRising(int bit) const {
		const uint8_t mask = static_cast<uint8_t>(1u << rack::math::clamp(bit, 0, 6));
		return (previous_ & mask) == 0 && (count_ & mask) != 0;
	}

private:
	uint8_t count_ = 0;
	uint8_t previous_ = 0;
};

class Cmos4017Counter {
public:
	void reset() {
		count_ = 0;
		resetCount_ = 10;
		decodedHigh_ = false;
	}

	bool clock(int resetCount) {
		resetCount = rack::math::clamp(resetCount, 1, 10);
		if (resetCount != resetCount_) {
			count_ = 0;
			decodedHigh_ = false;
			resetCount_ = resetCount;
		}

		if (decodedHigh_) {
			decodedHigh_ = false;
		}

		count_++;
		if (count_ >= resetCount_) {
			count_ = 0;
			decodedHigh_ = true;
			return true;
		}
		return false;
	}

	float decodedOutput() const {
		return decodedHigh_ ? 1.f : -1.f;
	}

private:
	int count_ = 0;
	int resetCount_ = 10;
	bool decodedHigh_ = false;
};

class KoruptDSP {
public:
	void setSampleRate(float sampleRate) {
		sampleRate_ = std::max(1.f, sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		squareSmoother_.setCutoffFreq(normalizedCutoff(18500.f, sampleRate_));
		oscillatorSmoother_.setCutoffFreq(normalizedCutoff(18500.f, sampleRate_));
		subharmonicSmoother_.setCutoffFreq(normalizedCutoff(18500.f, sampleRate_));
		outputDcBlock_.setCutoffFreq(normalizedCutoff(8.8f, sampleRate_));
		outputLowpass1_.setCutoffFreq(normalizedCutoff(8200.f, sampleRate_));
		outputLowpass2_.setCutoffFreq(normalizedCutoff(8200.f, sampleRate_));
		inputEnvelope_.setRiseFallTau(0.0018f, 0.16f);
		pfdMagnitudeFilter_.setTau(0.018f);
		lockFilter_.setTau(0.08f);
	}

	void reset() {
		rootCounter_.reset();
		feedbackCounter_.reset();
		subCounter_.reset();
		squareSmoother_.reset();
		oscillatorSmoother_.reset();
		subharmonicSmoother_.reset();
		outputDcBlock_.reset();
		outputLowpass1_.reset();
		outputLowpass2_.reset();
		inputEnvelope_.reset();
		controlNodeFilter_.reset();
		pfdMagnitudeFilter_.reset();
		lockFilter_.reset();
		samplesSinceRoot_ = 0.f;
		rootPeriodSamples_ = 0.f;
		rootFrequency_ = 0.f;
		vcoPhase_ = 0.f;
		vcoControl_ = 0.12f;
		loopCapacitor_ = 0.12f;
		controlNodeFilter_.out = 0.12f;
		vcoFrequency_ = vcoFrequencyFromControl(vcoControl_);
		lfoPhase_ = 0.f;
		pfdUp_ = false;
		pfdDown_ = false;
		lastPfdMagnitude_ = 0.f;
	}

	KoruptResult process(float input, bool inputRisingEdge, bool inputHigh, const KoruptParams& rawParams) {
		KoruptParams params = normalizedParams(rawParams);

		samplesSinceRoot_ += 1.f;
		const float envelope = updateEnvelope(input);

		bool rootEdge = false;
		if (inputRisingEdge) {
			rootCounter_.clock();
			if (params.oscillatorRoot == 0) {
				rootEdge = true;
			}
			else if (params.oscillatorRoot == 1) {
				rootEdge = rootCounter_.outputRising(0);
			}
			else {
				rootEdge = rootCounter_.outputRising(1);
			}
		}

		if (rootEdge) {
			updateRootEstimate();
			pfdUp_ = true;
		}

		const int oscillatorMultiplier = oscillatorMultiplierForProgram(params.oscillatorProgram);
		bool oscillatorEdge = false;
		const float rawOscillator = tickVco(&oscillatorEdge);

		bool feedbackEdge = false;
		if (oscillatorEdge) {
			feedbackEdge = feedbackCounter_.clock(oscillatorMultiplier);
			if (feedbackEdge) {
				pfdDown_ = true;
			}
		}

		if (pfdUp_ && pfdDown_) {
			pfdUp_ = false;
			pfdDown_ = false;
		}
		update4046Loop(params);

		const int subharmonicDivisor = subharmonicDivisorForProgram(params.subharmonicProgram);
		const bool subSourceEdge = params.subharmonicRoot == 0 ? inputRisingEdge : oscillatorEdge;
		if (subSourceEdge) {
			subCounter_.clock(subharmonicDivisor);
		}

		const float tracking = envelope > 0.012f ? 1.f : envelope * 83.333f;
		squareSmoother_.process(inputHigh ? 1.f : -1.f);
		oscillatorSmoother_.process(rawOscillator);
		subharmonicSmoother_.process(subCounter_.decodedOutput());
		const float square = tracking * squareSmoother_.lowpass();
		const float oscillator = tracking * oscillatorSmoother_.lowpass();
		const float subharmonic = tracking * subharmonicSmoother_.lowpass();

		float mixed = mixAndOutput(square, oscillator, subharmonic, params);

		const float lockTarget = lockConfidence(params, oscillatorMultiplier, tracking);
		lockFilter_.process(sampleTime_, lockTarget);

		if (!std::isfinite(mixed)) {
			mixed = 0.f;
		}

		KoruptResult result;
		result.mixed = mixed;
		result.square = square;
		result.oscillator = oscillator;
		result.subharmonic = subharmonic;
		result.lock = rack::math::clamp(lockFilter_.out);
		result.tracking = rack::math::clamp(tracking);
		result.glitch = rack::math::clamp(tracking * (1.f - result.lock + lastPfdMagnitude_ * 0.18f));
		return result;
	}

	static int oscillatorMultiplierForProgram(int program) {
		return rack::math::clamp(program + 1, 1, 8);
	}

	static int subharmonicDivisorForProgram(int program) {
		return rack::math::clamp(program + 2, 2, 9);
	}

	static float audioPotLaw(float value) {
		value = rack::math::clamp(value);
		return 0.035f * value + 0.965f * value * value;
	}

private:
	KoruptParams normalizedParams(const KoruptParams& rawParams) const {
		KoruptParams params = rawParams;
		params.level = rack::math::clamp(params.level);
		params.squareMix = rack::math::clamp(params.squareMix);
		params.oscillatorMix = rack::math::clamp(params.oscillatorMix);
		params.subharmonicMix = rack::math::clamp(params.subharmonicMix);
		params.rate = rack::math::clamp(params.rate);
		params.oscillatorProgram = rack::math::clamp(params.oscillatorProgram, 0, 7);
		params.oscillatorRoot = rack::math::clamp(params.oscillatorRoot, 0, 2);
		params.subharmonicProgram = rack::math::clamp(params.subharmonicProgram, 0, 7);
		params.subharmonicRoot = rack::math::clamp(params.subharmonicRoot, 0, 1);
		return params;
	}

	float updateEnvelope(float input) {
		const float target = std::min(1.f, std::abs(input));
		return inputEnvelope_.process(sampleTime_, target);
	}

	void updateRootEstimate() {
		if (samplesSinceRoot_ > 2.f && samplesSinceRoot_ < sampleRate_ * 2.f) {
			if (rootPeriodSamples_ <= 0.f) {
				rootPeriodSamples_ = samplesSinceRoot_;
			}
			else {
				rootPeriodSamples_ += 0.18f * (samplesSinceRoot_ - rootPeriodSamples_);
			}
			rootFrequency_ = sampleRate_ / std::max(1.f, rootPeriodSamples_);
		}
		samplesSinceRoot_ = 0.f;
	}

	float vcoFrequencyFromControl(float control) const {
		control = rack::math::clamp(control);
		const float vcoMin = 18.f;
		const float vcoMax = std::min(9800.f, sampleRate_ * 0.36f);
		const float bentControl = std::pow(control, 1.22f);
		return vcoMin + bentControl * (vcoMax - vcoMin);
	}

	float tickVco(bool* oscillatorEdge) {
		*oscillatorEdge = false;
		vcoPhase_ += vcoFrequency_ * sampleTime_;
		if (vcoPhase_ >= 1.f) {
			vcoPhase_ -= std::floor(vcoPhase_);
			*oscillatorEdge = true;
		}
		return vcoPhase_ < 0.5f ? 1.f : -1.f;
	}

	void update4046Loop(const KoruptParams& params) {
		const float pump = (pfdUp_ ? 1.f : 0.f) - (pfdDown_ ? 1.f : 0.f);
		lastPfdMagnitude_ = pfdMagnitudeFilter_.process(sampleTime_, std::abs(pump));

		const float pumpRate = 0.95f + params.rate * params.rate * 16.f;
		loopCapacitor_ += pump * pumpRate * sampleTime_;

		const float leakageHz = params.vibratoMode ? 0.42f : 0.045f;
		loopLeakageFilter_.out = loopCapacitor_;
		loopLeakageFilter_.setLambda(6.28318530718f * leakageHz);
		loopCapacitor_ = rack::math::clamp(loopLeakageFilter_.process(sampleTime_, 0.12f));

		float targetControl = loopCapacitor_;
		if (params.vibratoMode) {
			const float vibratoHz = 0.55f + params.rate * 18.f;
			lfoPhase_ += vibratoHz * sampleTime_;
			if (lfoPhase_ >= 1.f) {
				lfoPhase_ -= std::floor(lfoPhase_);
			}
			const float tri = lfoPhase_ < 0.5f ? (lfoPhase_ * 4.f - 1.f) : (3.f - lfoPhase_ * 4.f);
			targetControl += tri * 0.028f;
		}

		const float glideHz = params.vibratoMode ? 650.f : (0.45f + params.rate * params.rate * 95.f);
		controlNodeFilter_.setLambda(6.28318530718f * glideHz);
		vcoControl_ = rack::math::clamp(controlNodeFilter_.process(sampleTime_, targetControl));
		vcoFrequency_ = vcoFrequencyFromControl(vcoControl_);
	}

	float lockConfidence(const KoruptParams& params, int oscillatorMultiplier, float tracking) const {
		if (tracking <= 0.f || rootFrequency_ <= 1.f) {
			return 0.f;
		}

		const float desiredFrequency = rootFrequency_ * static_cast<float>(oscillatorMultiplier);
		const float vcoMin = 18.f;
		const float vcoMax = std::min(9800.f, sampleRate_ * 0.36f);
		if (desiredFrequency < vcoMin || desiredFrequency > vcoMax) {
			return 0.f;
		}

		const float ratioError = std::abs(vcoFrequency_ - desiredFrequency) / std::max(1.f, desiredFrequency);
		const float holdWindow = 0.16f + params.rate * 0.22f;
		const float phasePenalty = rack::math::clamp(lastPfdMagnitude_ * 0.55f);
		return tracking * rack::math::clamp(1.f - ratioError / holdWindow) * (1.f - phasePenalty);
	}

	float asymmetricClip(float input) const {
		if (input >= 0.f) {
			return 1.04f * std::tanh(input / 1.04f);
		}
		return -1.14f * std::tanh((-input) / 1.14f);
	}

	float mixAndOutput(float square, float oscillator, float subharmonic, const KoruptParams& params) {
		const float squareGain = audioPotLaw(params.squareMix);
		const float oscillatorGain = audioPotLaw(params.oscillatorMix);
		const float subharmonicGain = audioPotLaw(params.subharmonicMix);

		float mixed = square * squareGain + oscillator * oscillatorGain + subharmonic * subharmonicGain;
		mixed = asymmetricClip(mixed * 1.25f);

		outputDcBlock_.process(mixed);
		mixed = outputDcBlock_.highpass();
		outputLowpass1_.process(mixed);
		outputLowpass2_.process(outputLowpass1_.lowpass());

		const float levelGain = 0.02f + params.level * 1.72f;
		return rack::math::clamp(outputLowpass2_.lowpass() * levelGain, -1.35f, 1.35f);
	}

	float sampleRate_ = 44100.f;
	float sampleTime_ = 1.f / 44100.f;
	Cmos4024Counter rootCounter_;
	Cmos4017Counter feedbackCounter_;
	Cmos4017Counter subCounter_;
	rack::dsp::RCFilter squareSmoother_;
	rack::dsp::RCFilter oscillatorSmoother_;
	rack::dsp::RCFilter subharmonicSmoother_;
	rack::dsp::RCFilter outputDcBlock_;
	rack::dsp::RCFilter outputLowpass1_;
	rack::dsp::RCFilter outputLowpass2_;
	rack::dsp::ExponentialSlewLimiter inputEnvelope_;
	rack::dsp::ExponentialFilter loopLeakageFilter_;
	rack::dsp::ExponentialFilter controlNodeFilter_;
	rack::dsp::ExponentialFilter pfdMagnitudeFilter_;
	rack::dsp::ExponentialFilter lockFilter_;
	float samplesSinceRoot_ = 0.f;
	float rootPeriodSamples_ = 0.f;
	float rootFrequency_ = 0.f;
	float vcoPhase_ = 0.f;
	float vcoControl_ = 0.12f;
	float loopCapacitor_ = 0.12f;
	float vcoFrequency_ = 55.f;
	float lfoPhase_ = 0.f;
	bool pfdUp_ = false;
	bool pfdDown_ = false;
	float lastPfdMagnitude_ = 0.f;
};

} // namespace ShortwavDSP
