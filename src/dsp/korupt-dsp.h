#pragma once

#include <algorithm>
#include <cmath>

namespace ShortwavDSP {

inline float koruptClamp(float value, float minValue, float maxValue) {
	return std::max(minValue, std::min(maxValue, value));
}

inline float koruptClamp01(float value) {
	return koruptClamp(value, 0.f, 1.f);
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

class ToggleDivider {
public:
	void reset() {
		high_ = false;
		count_ = 0;
	}

	bool processEdge(int frequencyDivision) {
		if (frequencyDivision <= 1) {
			high_ = true;
			return true;
		}

		const int edgesPerToggle = std::max(1, frequencyDivision / 2);
		count_++;
		if (count_ < edgesPerToggle) {
			return false;
		}

		count_ = 0;
		const bool wasHigh = high_;
		high_ = !high_;
		return !wasHigh && high_;
	}

	bool high() const {
		return high_;
	}

private:
	bool high_ = false;
	int count_ = 0;
};

class PulseDivider {
public:
	void reset() {
		high_ = false;
		count_ = 0;
	}

	bool processEdge(int division) {
		division = std::max(1, division);
		const bool wasHigh = high_;

		if (high_) {
			high_ = false;
		}

		count_++;
		if (count_ >= division) {
			count_ = 0;
			high_ = true;
		}

		return !wasHigh && high_;
	}

	float value() const {
		return high_ ? 1.f : -1.f;
	}

private:
	bool high_ = false;
	int count_ = 0;
};

class KoruptDSP {
public:
	void setSampleRate(float sampleRate) {
		sampleRate_ = std::max(1.f, sampleRate);
		sampleTime_ = 1.f / sampleRate_;
	}

	void reset() {
		rootDiv2_.reset();
		rootDiv4_.reset();
		feedbackDivider_.reset();
		subDivider_.reset();
		samplesSinceRoot_ = 0.f;
		rootPeriodSamples_ = 0.f;
		rootFrequency_ = 0.f;
		centerFrequency_ = 55.f;
		pllFrequency_ = 55.f;
		pllPhase_ = 0.f;
		lfoPhase_ = 0.f;
		loopControl_ = 0.f;
		pfdUp_ = false;
		pfdDown_ = false;
		envelope_ = 0.f;
		outputDc_ = 0.f;
		outputLp_ = 0.f;
		lockEnvelope_ = 0.f;
		lastOscillator_ = -1.f;
	}

	KoruptResult process(float input, bool inputRisingEdge, bool inputHigh, const KoruptParams& rawParams) {
		KoruptParams params = rawParams;
		params.level = koruptClamp01(params.level);
		params.squareMix = koruptClamp01(params.squareMix);
		params.oscillatorMix = koruptClamp01(params.oscillatorMix);
		params.subharmonicMix = koruptClamp01(params.subharmonicMix);
		params.rate = koruptClamp01(params.rate);
		params.oscillatorProgram = std::max(0, std::min(7, params.oscillatorProgram));
		params.oscillatorRoot = std::max(0, std::min(2, params.oscillatorRoot));
		params.subharmonicProgram = std::max(0, std::min(7, params.subharmonicProgram));
		params.subharmonicRoot = std::max(0, std::min(1, params.subharmonicRoot));

		samplesSinceRoot_ += 1.f;
		updateEnvelope(input);

		bool rootEdge = false;
		if (inputRisingEdge) {
			const bool div2Edge = rootDiv2_.processEdge(2);
			const bool div4Edge = rootDiv4_.processEdge(4);

			if (params.oscillatorRoot == 0) {
				rootEdge = true;
			}
			else if (params.oscillatorRoot == 1) {
				rootEdge = div2Edge;
			}
			else {
				rootEdge = div4Edge;
			}
		}

		if (rootEdge) {
			updateRootEstimate();
			pfdUp_ = true;
		}

		const int oscillatorMultiplier = oscillatorMultiplierForProgram(params.oscillatorProgram);
		updatePll(params, oscillatorMultiplier);

		bool oscillatorEdge = false;
		const float oscillator = tickOscillator(oscillatorMultiplier, &oscillatorEdge);

		if (oscillatorEdge && feedbackDivider_.processEdge(oscillatorMultiplier)) {
			pfdDown_ = true;
		}
		if (pfdUp_ && pfdDown_) {
			pfdUp_ = false;
			pfdDown_ = false;
		}

		const int subharmonicDivisor = subharmonicDivisorForProgram(params.subharmonicProgram);
		const bool subSourceEdge = params.subharmonicRoot == 0 ? inputRisingEdge : oscillatorEdge;
		if (subSourceEdge) {
			subDivider_.processEdge(subharmonicDivisor);
		}

		const float tracking = envelope_ > 0.01f ? 1.f : envelope_ * 100.f;
		const float square = tracking * (inputHigh ? 1.f : -1.f);
		const float oscillatorVoice = tracking * oscillator;
		const float subharmonic = tracking * subDivider_.value();

		const float mixSum = std::max(0.0001f, params.squareMix + params.oscillatorMix + params.subharmonicMix);
		float mixed = (
			square * params.squareMix +
			oscillatorVoice * params.oscillatorMix +
			subharmonic * params.subharmonicMix
		) / mixSum;
		mixed = std::tanh(2.4f * mixed) * params.level;

		const float dcCoeff = coefficientForTime(0.02f);
		outputDc_ += dcCoeff * (mixed - outputDc_);
		mixed -= outputDc_ * 0.96f;

		const float lpCoeff = coefficientForFrequency(9500.f);
		outputLp_ += lpCoeff * (mixed - outputLp_);
		mixed = koruptClamp(outputLp_, -1.f, 1.f);

		const float lockTarget = tracking * koruptClamp01(1.f - std::abs(loopControl_) * 0.85f);
		lockEnvelope_ += coefficientForTime(0.08f) * (lockTarget - lockEnvelope_);

		KoruptResult result;
		result.mixed = mixed;
		result.square = square;
		result.oscillator = oscillatorVoice;
		result.subharmonic = subharmonic;
		result.lock = koruptClamp01(lockEnvelope_);
		result.tracking = koruptClamp01(tracking);
		result.glitch = koruptClamp01(tracking * (1.f - result.lock));
		return result;
	}

	static int oscillatorMultiplierForProgram(int program) {
		return std::max(1, std::min(8, program + 1));
	}

	static int subharmonicDivisorForProgram(int program) {
		return std::max(2, std::min(9, program + 2));
	}

private:
	float coefficientForFrequency(float frequency) const {
		const float omega = 6.28318530718f * std::max(0.f, frequency) * sampleTime_;
		return koruptClamp01(1.f - std::exp(-omega));
	}

	float coefficientForTime(float seconds) const {
		seconds = std::max(0.000001f, seconds);
		return koruptClamp01(1.f - std::exp(-sampleTime_ / seconds));
	}

	void updateEnvelope(float input) {
		const float target = std::min(1.f, std::abs(input));
		const float coeff = target > envelope_ ? coefficientForTime(0.002f) : coefficientForTime(0.18f);
		envelope_ += coeff * (target - envelope_);
	}

	void updateRootEstimate() {
		if (samplesSinceRoot_ > 2.f && samplesSinceRoot_ < sampleRate_ * 2.f) {
			if (rootPeriodSamples_ <= 0.f) {
				rootPeriodSamples_ = samplesSinceRoot_;
			}
			else {
				rootPeriodSamples_ += 0.12f * (samplesSinceRoot_ - rootPeriodSamples_);
			}
			rootFrequency_ = sampleRate_ / std::max(1.f, rootPeriodSamples_);
		}
		samplesSinceRoot_ = 0.f;
	}

	void updatePll(const KoruptParams& params, int oscillatorMultiplier) {
		const float pfd = (pfdUp_ ? 1.f : 0.f) - (pfdDown_ ? 1.f : 0.f);
		const float loopHz = 0.8f + params.rate * params.rate * 85.f;
		loopControl_ += coefficientForFrequency(loopHz) * (pfd - loopControl_);
		loopControl_ = koruptClamp(loopControl_, -1.f, 1.f);

		const float validRootFrequency = rootFrequency_ > 1.f ? rootFrequency_ : 55.f;
		centerFrequency_ = koruptClamp(validRootFrequency * oscillatorMultiplier, 8.f, sampleRate_ * 0.42f);

		float targetFrequency = centerFrequency_ * (1.f + loopControl_ * 0.35f);
		if (params.vibratoMode) {
			const float vibratoHz = 0.6f + params.rate * 18.f;
			lfoPhase_ += vibratoHz * sampleTime_;
			if (lfoPhase_ >= 1.f) {
				lfoPhase_ -= std::floor(lfoPhase_);
			}
			targetFrequency *= 1.f + std::sin(lfoPhase_ * 6.28318530718f) * 0.055f;
		}

		const float slewTime = params.vibratoMode ? 0.006f : (0.45f - params.rate * 0.42f);
		pllFrequency_ += coefficientForTime(std::max(0.004f, slewTime)) * (targetFrequency - pllFrequency_);
		pllFrequency_ = koruptClamp(pllFrequency_, 8.f, sampleRate_ * 0.42f);
	}

	float tickOscillator(int oscillatorMultiplier, bool* oscillatorEdge) {
		(void) oscillatorMultiplier;
		*oscillatorEdge = false;
		pllPhase_ += pllFrequency_ * sampleTime_;
		if (pllPhase_ >= 1.f) {
			pllPhase_ -= std::floor(pllPhase_);
			*oscillatorEdge = true;
		}

		float oscillator = pllPhase_ < 0.5f ? 1.f : -1.f;
		if (*oscillatorEdge && lastOscillator_ < 0.f) {
			oscillator = 1.f;
		}
		lastOscillator_ = oscillator;
		return oscillator;
	}

	float sampleRate_ = 44100.f;
	float sampleTime_ = 1.f / 44100.f;
	ToggleDivider rootDiv2_;
	ToggleDivider rootDiv4_;
	PulseDivider feedbackDivider_;
	PulseDivider subDivider_;
	float samplesSinceRoot_ = 0.f;
	float rootPeriodSamples_ = 0.f;
	float rootFrequency_ = 0.f;
	float centerFrequency_ = 55.f;
	float pllFrequency_ = 55.f;
	float pllPhase_ = 0.f;
	float lfoPhase_ = 0.f;
	float loopControl_ = 0.f;
	bool pfdUp_ = false;
	bool pfdDown_ = false;
	float envelope_ = 0.f;
	float outputDc_ = 0.f;
	float outputLp_ = 0.f;
	float lockEnvelope_ = 0.f;
	float lastOscillator_ = -1.f;
};

} // namespace ShortwavDSP
