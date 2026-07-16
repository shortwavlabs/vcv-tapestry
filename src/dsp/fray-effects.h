#ifndef SHORTWAV_DSP_FRAY_EFFECTS_H
#define SHORTWAV_DSP_FRAY_EFFECTS_H

#include "fray-core.h"

#include <rack.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ShortwavDSP {
namespace Fray {

static const float kFrayPi = 3.14159265358979323846f;
static const float kFrayTwoPi = 6.28318530717958647692f;

inline bool isFinite(float value) {
	return std::isfinite(value) != 0;
}

inline float finiteOrSilence(float value) {
	return isFinite(value) ? value : 0.f;
}

inline float flushDenormal(float value) {
	return std::fabs(value) < 1.0e-20f ? 0.f : value;
}

inline float clampFinite(float value, float minimum, float maximum, float fallback = 0.f) {
	if (!isFinite(value)) {
		value = fallback;
	}
	return std::max(minimum, std::min(value, maximum));
}

inline float clamp01(float value) {
	return clampFinite(value, 0.f, 1.f, 0.f);
}

struct StereoFrame {
	float left;
	float right;

	StereoFrame() : left(0.f), right(0.f) {}
	StereoFrame(float newLeft, float newRight) : left(newLeft), right(newRight) {}
};

inline StereoFrame sanitizeFrame(const StereoFrame& frame, float limit = 16.f) {
	return StereoFrame(
		clampFinite(frame.left, -limit, limit),
		clampFinite(frame.right, -limit, limit));
}

inline StereoFrame crossfadeFrames(const StereoFrame& dry, const StereoFrame& wet, float amount) {
	const float mix = clamp01(amount);
	return sanitizeFrame(StereoFrame(
		rack::math::crossfade(dry.left, wet.left, mix),
		rack::math::crossfade(dry.right, wet.right, mix)));
}

inline float hermite4(float xm1, float x0, float x1, float x2, float fraction) {
	const float t = clamp01(fraction);
	const float c = (x1 - xm1) * 0.5f;
	const float v = x0 - x1;
	const float w = c + v;
	const float a = w + v + (x2 - x0) * 0.5f;
	const float bNegative = w + a;
	return finiteOrSilence((((a * t - bNegative) * t + c) * t) + x0);
}

inline StereoFrame hermiteFrame(
	const StereoFrame& xm1,
	const StereoFrame& x0,
	const StereoFrame& x1,
	const StereoFrame& x2,
	float fraction) {
	return sanitizeFrame(StereoFrame(
		hermite4(xm1.left, x0.left, x1.left, x2.left, fraction),
		hermite4(xm1.right, x0.right, x1.right, x2.right, fraction)));
}

inline float normalizedParam(const EffectSettings& settings, std::size_t index, float fallback = 0.f) {
	if (index >= settings.values.size()) {
		return clamp01(fallback);
	}
	return clampFinite(settings.values[index], 0.f, 1.f, fallback);
}

inline float safeSampleRate(float sampleRate) {
	// Rack 2.6.6 supports engine rates through 768 kHz. Keep every timebase and
	// history length tied to the actual engine rate so musical timing does not
	// speed up at high sample rates.
	return clampFinite(sampleRate, 8000.f, 768000.f, 48000.f);
}

// Rack's exponential processors use an Euler step. A time constant shorter
// than one engine sample would overshoot the target, so keep their native
// response monotonic at Rack's low-fi sample rates.
inline float rackSafeTau(float seconds, float sampleTime) {
	const float dt = clampFinite(sampleTime, 1.f / 768000.f, 1.f / 8000.f, 1.f / 48000.f);
	return std::max(dt, clampFinite(seconds, 1.0e-6f, 60.f, dt));
}

inline float safeBpm(float bpm) {
	return clampFinite(bpm, 20.f, 999.f, 120.f);
}

inline float exponentialMap(float normalized, float minimum, float maximum) {
	const float safeMinimum = std::max(minimum, 1.0e-9f);
	const float safeMaximum = std::max(maximum, safeMinimum);
	return safeMinimum * std::pow(safeMaximum / safeMinimum, clamp01(normalized));
}

inline float musicalSeconds(float normalized, float bpm, float minimumBeats, float maximumBeats) {
	return (60.f / safeBpm(bpm)) * exponentialMap(normalized, minimumBeats, maximumBeats);
}

struct EffectContext {
	float sampleRate;
	float sampleTime;
	float bpm;
	bool active;
	bool blockStart;
	bool blockEnd;
	std::uint64_t sceneSeed;
	std::uint64_t eventOrdinal;

	EffectContext()
	: sampleRate(48000.f),
	  sampleTime(1.f / 48000.f),
	  bpm(120.f),
	  active(false),
	  blockStart(false),
	  blockEnd(false),
	  sceneSeed(0),
	  eventOrdinal(0) {}
};

// A vector is used only as owned storage. Its size is fixed by prepare(), and no
// operation below it can allocate. Reads are wrap-safe 4-point Hermite reads.
class StereoCircularHistory {
public:
	StereoCircularHistory() : writeIndex_(0), filled_(0) {}

	void prepare(float sampleRate, float maximumSeconds) {
		const float sr = safeSampleRate(sampleRate);
		const float seconds = clampFinite(maximumSeconds, 0.001f, 4.f, 1.f);
		const std::size_t samples = static_cast<std::size_t>(std::ceil(sr * seconds)) + 8u;
		buffer_.assign(std::max<std::size_t>(samples, 16u), StereoFrame());
		writeIndex_ = 0;
		filled_ = 0;
	}

	void reset() {
		writeIndex_ = 0;
		filled_ = 0;
	}

	void push(const StereoFrame& input) {
		if (buffer_.empty()) {
			return;
		}
		buffer_[writeIndex_] = sanitizeFrame(input);
		writeIndex_++;
		if (writeIndex_ >= buffer_.size()) {
			writeIndex_ = 0;
		}
		filled_ = std::min(buffer_.size(), filled_ + 1u);
	}

	StereoFrame readDelay(float delaySamples) const {
		if (buffer_.empty() || filled_ == 0) {
			return StereoFrame();
		}
		const std::size_t newest = (writeIndex_ + buffer_.size() - 1u) % buffer_.size();
		if (filled_ < 5u) {
			return buffer_[newest];
		}

		const float maximum = static_cast<float>(std::min(buffer_.size() - 3u, filled_ - 3u));
		const float delay = clampFinite(delaySamples, 2.f, std::max(2.f, maximum), 2.f);
		float position = static_cast<float>(newest) - delay;
		const float size = static_cast<float>(buffer_.size());
		position = std::fmod(position, size);
		if (position < 0.f) {
			position += size;
		}
		const std::size_t i0 = static_cast<std::size_t>(std::floor(position));
		const float fraction = position - static_cast<float>(i0);
		const std::size_t im1 = (i0 + buffer_.size() - 1u) % buffer_.size();
		const std::size_t i1 = (i0 + 1u) % buffer_.size();
		const std::size_t i2 = (i0 + 2u) % buffer_.size();
		return hermiteFrame(buffer_[im1], buffer_[i0], buffer_[i1], buffer_[i2], fraction);
	}

	std::size_t capacitySamples() const { return buffer_.size(); }
	std::size_t availableSamples() const { return filled_; }
	float maximumDelaySamples() const {
		return buffer_.size() > 4u ? static_cast<float>(buffer_.size() - 3u) : 2.f;
	}

private:
	std::vector<StereoFrame> buffer_;
	std::size_t writeIndex_;
	std::size_t filled_;
};

// Two fixed buffers alternate between immutable playback capture and continuous
// pre-roll. A block start swaps their roles in O(1), so adjacent starts receive
// fresh source audio without copying or allocating on the audio thread.
class StereoCapture {
public:
	StereoCapture()
	: targetLength_(0), length_(0), startIndex_(0), rollingWrite_(0), rollingFilled_(0),
	  captureBuffer_(0u), rollingBuffer_(1u), capturing_(false) {}

	void prepare(float sampleRate, float maximumSeconds) {
		const float sr = safeSampleRate(sampleRate);
		const float seconds = clampFinite(maximumSeconds, 0.001f, 2.f, 1.f);
		const std::size_t samples = static_cast<std::size_t>(std::ceil(sr * seconds)) + 8u;
		const std::size_t capacity = std::max<std::size_t>(samples, 16u);
		buffers_[0].assign(capacity, StereoFrame());
		buffers_[1].assign(capacity, StereoFrame());
		reset();
	}

	void reset() {
		targetLength_ = 0;
		length_ = 0;
		startIndex_ = 0;
		rollingWrite_ = 0;
		rollingFilled_ = 0;
		captureBuffer_ = 0u;
		rollingBuffer_ = 1u;
		capturing_ = false;
	}

	void beginCapture(std::size_t requestedLength) {
		if (buffers_[rollingBuffer_].empty()) {
			targetLength_ = 0;
			length_ = 0;
			capturing_ = false;
			return;
		}
		const std::size_t capacity = buffers_[rollingBuffer_].size();
		targetLength_ = std::max<std::size_t>(8u, std::min(requestedLength, capacity));
		captureBuffer_ = rollingBuffer_;
		if (rollingFilled_ >= 8u) {
			// Adopt the newest pre-roll in O(1). The other fixed buffer immediately
			// becomes the rolling source for the next (even adjacent) block start.
			length_ = std::min(targetLength_, rollingFilled_);
			targetLength_ = length_;
			startIndex_ = (rollingWrite_ + capacity - length_) % capacity;
			capturing_ = false;
		}
		else {
			length_ = 0;
			startIndex_ = 0;
			capturing_ = true;
		}
		rollingBuffer_ = 1u - captureBuffer_;
		rollingWrite_ = 0;
		rollingFilled_ = 0;
	}

	void capture(const StereoFrame& input) {
		if (!capturing_ || length_ >= targetLength_) {
			return;
		}
		std::vector<StereoFrame>& capture = buffers_[captureBuffer_];
		capture[(startIndex_ + length_) % capture.size()] = sanitizeFrame(input);
		length_++;
		if (length_ >= targetLength_) {
			capturing_ = false;
		}
	}

	void idleTick(const StereoFrame& input) {
		std::vector<StereoFrame>& rolling = buffers_[rollingBuffer_];
		if (rolling.empty()) {
			return;
		}
		rolling[rollingWrite_] = sanitizeFrame(input);
		rollingWrite_ = (rollingWrite_ + 1u) % rolling.size();
		rollingFilled_ = std::min(rolling.size(), rollingFilled_ + 1u);
	}

	StereoFrame readWrapped(float position) const {
		if (length_ == 0) {
			return StereoFrame();
		}
		if (length_ < 4u) {
			const float safePosition = clampFinite(
				position, 0.f, static_cast<float>(length_ - 1u), 0.f);
			return frameAt(static_cast<std::size_t>(safePosition));
		}
		const float size = static_cast<float>(length_);
		float wrapped = std::fmod(finiteOrSilence(position), size);
		if (wrapped < 0.f) {
			wrapped += size;
		}
		const std::size_t i0 = static_cast<std::size_t>(std::floor(wrapped));
		const float fraction = wrapped - static_cast<float>(i0);
		const std::size_t im1 = (i0 + length_ - 1u) % length_;
		const std::size_t i1 = (i0 + 1u) % length_;
		const std::size_t i2 = (i0 + 2u) % length_;
		return hermiteFrame(frameAt(im1), frameAt(i0), frameAt(i1), frameAt(i2), fraction);
	}

	StereoFrame readClamped(float position) const {
		if (length_ == 0) {
			return StereoFrame();
		}
		const float p = clampFinite(position, 0.f, static_cast<float>(length_ - 1u));
		const std::size_t i0 = static_cast<std::size_t>(std::floor(p));
		const float fraction = p - static_cast<float>(i0);
		const std::size_t im1 = i0 > 0u ? i0 - 1u : 0u;
		const std::size_t i1 = std::min(length_ - 1u, i0 + 1u);
		const std::size_t i2 = std::min(length_ - 1u, i0 + 2u);
		return hermiteFrame(frameAt(im1), frameAt(i0), frameAt(i1), frameAt(i2), fraction);
	}

	bool complete() const { return targetLength_ > 0u && length_ >= targetLength_ && !capturing_; }
	bool capturing() const { return capturing_; }
	std::size_t length() const { return length_; }
	std::size_t capacitySamples() const { return buffers_[0].size(); }

private:
	const StereoFrame& frameAt(std::size_t logicalIndex) const {
		const std::vector<StereoFrame>& capture = buffers_[captureBuffer_];
		return capture[(startIndex_ + logicalIndex) % capture.size()];
	}

	std::array<std::vector<StereoFrame>, 2> buffers_;
	std::size_t targetLength_;
	std::size_t length_;
	std::size_t startIndex_;
	std::size_t rollingWrite_;
	std::size_t rollingFilled_;
	std::size_t captureBuffer_;
	std::size_t rollingBuffer_;
	bool capturing_;
};

class ActivitySlew {
public:
	ActivitySlew() : sampleTime_(1.f / 48000.f) {
		limiter_.setRiseFall(250.f, 250.f);
	}
	void prepare(float sampleRate, float milliseconds = 4.f) {
		const float sr = safeSampleRate(sampleRate);
		const float seconds = clampFinite(milliseconds, 0.1f, 50.f, 4.f) * 0.001f;
		sampleTime_ = 1.f / sr;
		const float rate = 1.f / seconds;
		limiter_.setRiseFall(rate, rate);
	}
	void reset() { limiter_.reset(); }
	float process(bool active) {
		limiter_.out = clamp01(limiter_.process(sampleTime_, active ? 1.f : 0.f));
		return limiter_.out;
	}
	float value() const { return clamp01(limiter_.out); }

private:
	rack::dsp::SlewLimiter limiter_;
	float sampleTime_;
};

// Rack-backed post stage shared by every effect. It owns the common filter,
// activation slew, mix, pan, gain, and delay-tail behavior so the Rack-linked
// effect tests exercise the exact same path as the module.
class CommonStage {
public:
	CommonStage()
	: sampleRate_(48000.f), cachedFilterType_(-1), cachedCutoff_(-1.f), cachedQ_(-1.f),
	  controlsInitialized_(false), delayLatched_(false) {
		activation_.setRiseFall(1.f / 0.003f, 1.f / 0.003f);
		filterUpdateDivider_.setDivision(16u);
		configureControlSmoothers();
		// BiquadFilter's default coefficients are invalid in Rack 2.6.6. Install
		// a valid set before the first possible process() call.
		setFilterParameters(filterL_, 1, 0.1, 0.707);
		setFilterParameters(filterR_, 1, 0.1, 0.707);
		reset();
	}

	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		cachedFilterType_ = -1;
		cachedCutoff_ = -1.f;
		cachedQ_ = -1.f;
		configureControlSmoothers();
		reset();
	}

	void reset() {
		filterL_.reset();
		filterR_.reset();
		activation_.reset();
		cutoffSmoother_.reset();
		qSmoother_.reset();
		mixSmoother_.reset();
		panSmoother_.reset();
		gainSmoother_.reset();
		filterUpdateDivider_.reset();
		activation_.setRiseFall(1.f / 0.003f, 1.f / 0.003f);
		controlsInitialized_ = false;
		delayLatched_ = false;
	}

	StereoFrame process(
		const StereoFrame& localDry,
		const StereoFrame& localWet,
		const CommonSettings& settings,
		bool active,
		bool preserveTail,
		float sampleTime) {
		const StereoFrame dry = sanitizeFrame(localDry);
		const StereoFrame wet = sanitizeFrame(localWet);
		const float dt = clampFinite(sampleTime, 1.f / 768000.f, 1.f / 8000.f, 1.f / sampleRate_);
		updateControls(settings, dt);
		const int filterType = std::max(0, std::min(settings.filterType, 4));
		updateFilter(
			filterType, cutoffSmoother_.out, qSmoother_.out, filterUpdateDivider_.process());
		if (active) {
			delayLatched_ = true;
		}
		const float target = (active || (preserveTail && delayLatched_)) ? 1.f : 0.f;
		activation_.out = clamp01(activation_.process(dt, target));

		// DelayEffect returns dry + return. Always isolate and process the return
		// contribution so deactivation cannot change topology or synthesize a
		// filtered copy of the dry signal.
		StereoFrame filtered = preserveTail
			? StereoFrame(wet.left - dry.left, wet.right - dry.right)
			: wet;
		if (filterType > 0) {
			filtered.left = static_cast<float>(filterL_.process(static_cast<double>(filtered.left)));
			filtered.right = static_cast<float>(filterR_.process(static_cast<double>(filtered.right)));
		}

		const float pan = panSmoother_.out;
		if (pan > 0.f) {
			filtered.left *= 1.f - pan;
		}
		else if (pan < 0.f) {
			filtered.right *= 1.f + pan;
		}
		const float gain = gainSmoother_.out;
		filtered.left *= gain;
		filtered.right *= gain;
		const float mix = mixSmoother_.out * activation_.out;

		StereoFrame output;
		if (preserveTail) {
			output.left = dry.left + filtered.left * mix;
			output.right = dry.right + filtered.right * mix;
		}
		else {
			output.left = rack::math::crossfade(dry.left, filtered.left, mix);
			output.right = rack::math::crossfade(dry.right, filtered.right, mix);
		}
		return StereoFrame(finiteOrSilence(output.left), finiteOrSilence(output.right));
	}

private:
	void updateControls(const CommonSettings& settings, float sampleTime) {
		const float cutoff = clampFinite(settings.cutoff, 0.f, 1.f, 0.75f);
		const float q = clampFinite(settings.q, 0.f, 1.f, 0.25f);
		const float mix = clampFinite(settings.mix, 0.f, 1.f, 1.f);
		const float pan = clampFinite(settings.pan, -1.f, 1.f, 0.f);
		const float gain = clampFinite(settings.gain, 0.f, 2.f, 1.f);
		if (!controlsInitialized_) {
			cutoffSmoother_.out = cutoff;
			qSmoother_.out = q;
			mixSmoother_.out = mix;
			panSmoother_.out = pan;
			gainSmoother_.out = gain;
			controlsInitialized_ = true;
			return;
		}

		cutoffSmoother_.out = clampFinite(
			cutoffSmoother_.process(sampleTime, cutoff), 0.f, 1.f, cutoff);
		qSmoother_.out = clampFinite(qSmoother_.process(sampleTime, q), 0.f, 1.f, q);
		mixSmoother_.out = clampFinite(mixSmoother_.process(sampleTime, mix), 0.f, 1.f, mix);
		panSmoother_.out = clampFinite(panSmoother_.process(sampleTime, pan), -1.f, 1.f, pan);
		gainSmoother_.out = clampFinite(gainSmoother_.process(sampleTime, gain), 0.f, 2.f, gain);
	}

	void configureControlSmoothers() {
		cutoffSmoother_.setTau(0.005f);
		qSmoother_.setTau(0.005f);
		mixSmoother_.setTau(0.005f);
		panSmoother_.setTau(0.005f);
		gainSmoother_.setTau(0.005f);
	}

	// Rack's float coefficient builder loses pole stability for very low
	// normalized frequencies at 384/768 kHz. Keep Rack's BiquadFilter/IIR state
	// and processing, but calculate the four required coefficient sets in double.
	static void setFilterParameters(
		rack::dsp::TBiquadFilter<double>& filter,
		int filterType,
		double normalized,
		double q) {
		const double k = std::tan(3.14159265358979323846 * normalized);
		const double kSquared = k * k;
		const double norm = 1.0 / (1.0 + k / q + kSquared);
		double b[3] = {0.0, 0.0, 0.0};
		double a[2] = {
			2.0 * (kSquared - 1.0) * norm,
			(1.0 - k / q + kSquared) * norm
		};
		if (filterType == 1) {
			b[0] = kSquared * norm;
			b[1] = 2.0 * b[0];
			b[2] = b[0];
		}
		else if (filterType == 2) {
			b[0] = norm;
			b[1] = -2.0 * b[0];
			b[2] = b[0];
		}
		else if (filterType == 3) {
			b[0] = k / q * norm;
			b[2] = -b[0];
		}
		else {
			b[0] = (1.0 + kSquared) * norm;
			b[1] = 2.0 * (kSquared - 1.0) * norm;
			b[2] = b[0];
		}
		filter.setCoefficients(b, a);
	}

	void updateFilter(
		int filterType,
		float cutoffControl,
		float qControl,
		bool parameterUpdateDue) {
		if (filterType <= 0) {
			if (cachedFilterType_ != 0) {
				filterL_.reset();
				filterR_.reset();
			}
			cachedFilterType_ = filterType;
			cachedCutoff_ = cutoffControl;
			cachedQ_ = qControl;
			return;
		}
		if (filterType == cachedFilterType_ && !parameterUpdateDue) {
			return;
		}
		if (filterType == cachedFilterType_ && cutoffControl == cachedCutoff_ && qControl == cachedQ_) {
			return;
		}
		const bool topologyChanged = filterType != cachedFilterType_;

		const double cutoff = std::max(20.0, std::min(
			static_cast<double>(sampleRate_) * 0.45,
			20.0 * std::pow(1000.0, static_cast<double>(cutoffControl))));
		const double normalized = std::max(1.0e-8, std::min(
			0.45, cutoff / static_cast<double>(sampleRate_)));
		const double q = 0.5 + static_cast<double>(qControl) * 9.5;
		if (topologyChanged) {
			filterL_.reset();
			filterR_.reset();
		}
		setFilterParameters(filterL_, filterType, normalized, q);
		setFilterParameters(filterR_, filterType, normalized, q);
		cachedFilterType_ = filterType;
		cachedCutoff_ = cutoffControl;
		cachedQ_ = qControl;
	}

	rack::dsp::TBiquadFilter<double> filterL_;
	rack::dsp::TBiquadFilter<double> filterR_;
	rack::dsp::SlewLimiter activation_;
	rack::dsp::ExponentialFilter cutoffSmoother_;
	rack::dsp::ExponentialFilter qSmoother_;
	rack::dsp::ExponentialFilter mixSmoother_;
	rack::dsp::ExponentialFilter panSmoother_;
	rack::dsp::ExponentialFilter gainSmoother_;
	rack::dsp::ClockDivider filterUpdateDivider_;
	float sampleRate_;
	int cachedFilterType_;
	float cachedCutoff_;
	float cachedQ_;
	bool controlsInitialized_;
	bool delayLatched_;
};

inline std::uint64_t effectEventSeed(const EffectContext& context, std::uint64_t effectTag) {
	std::uint64_t eventMixer = context.eventOrdinal + 0x632be59bd9b4e019ULL;
	const std::uint64_t mixedEvent = detail::splitMix64(eventMixer);
	std::uint64_t combined = context.sceneSeed ^ mixedEvent ^ effectTag;
	return detail::splitMix64(combined);
}

class ModulatorEffect {
public:
	ModulatorEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), phase_(0.f),
	  cachedAttack_(-1.f), cachedRelease_(-1.f) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		phase_ = 0.f;
		envelope_.reset();
		cachedAttack_ = -1.f;
		cachedRelease_ = -1.f;
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 frequency, 1 depth, 2 oscillator level, 3 stereo spread,
		// 4 envelope-FM depth, 5 attack, 6 release, 7 carrier phase,
		// 8..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		const float detector = clampFinite(std::max(std::fabs(dry.left), std::fabs(dry.right)), 0.f, 4.f);
		const float attack = rackSafeTau(
			exponentialMap(normalizedParam(settings, 5, 0.25f), 0.001f, 0.5f), sampleTime_);
		const float release = rackSafeTau(
			exponentialMap(normalizedParam(settings, 6, 0.5f), 0.005f, 2.f), sampleTime_);
		if (attack != cachedAttack_ || release != cachedRelease_) {
			envelope_.setRiseFallTau(attack, release);
			cachedAttack_ = attack;
			cachedRelease_ = release;
		}
		envelope_.out = flushDenormal(clampFinite(
			envelope_.process(sampleTime_, detector), 0.f, 4.f));
		const float envelope = envelope_.out;

		const float baseFrequency = exponentialMap(normalizedParam(settings, 0, 0.35f), 0.05f, 2000.f);
		const float fmOctaves = normalizedParam(settings, 4, 0.f) * 4.f;
		const float frequency = clampFinite(baseFrequency * std::pow(2.f, fmOctaves * std::min(1.f, envelope)), 0.01f, sampleRate_ * 0.45f);
		phase_ += frequency / sampleRate_;
		phase_ -= std::floor(phase_);

		const float spread = (normalizedParam(settings, 3, 0.5f) * 2.f - 1.f) * kFrayPi;
		const float carrierPhase = phase_ + normalizedParam(settings, 7, 0.f);
		const float leftCarrier = std::sin(kFrayTwoPi * carrierPhase - spread * 0.5f);
		const float rightCarrier = std::sin(kFrayTwoPi * carrierPhase + spread * 0.5f);
		const float depth = normalizedParam(settings, 1, 0.5f);
		const float oscillatorLevel = normalizedParam(settings, 2, 0.f);
		const StereoFrame wet(
			dry.left * (1.f - depth + depth * leftCarrier) + oscillatorLevel * leftCarrier,
			dry.right * (1.f - depth + depth * rightCarrier) + oscillatorLevel * rightCarrier);
		return crossfadeFrames(dry, sanitizeFrame(wet), ramp_.process(context.active));
	}

private:
	float sampleRate_;
	float sampleTime_;
	float phase_;
	float cachedAttack_;
	float cachedRelease_;
	rack::dsp::ExponentialSlewLimiter envelope_;
	ActivitySlew ramp_;
};

class TapeStopEffect {
public:
	TapeStopEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), readDelay_(2.f),
	  cachedSmoothingSeconds_(-1.f), eventAge_(0u), mode_(0) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		history_.prepare(sampleRate_, 1.5f);
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		history_.reset();
		readDelay_ = 2.f;
		motorSpeed_.reset();
		motorSpeed_.out = 1.f;
		cachedSmoothingSeconds_ = -1.f;
		eventAge_ = 0u;
		mode_ = 0;
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 slowdown, 1 speed-up, 2 play mode
		// (Stop/Start/Stop-Start/Scratch), 3 motor time, 4 curve, 5 hold,
		// 6 smoothing, 7 timing range, 8..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		history_.push(dry);
		const float timing = normalizedParam(settings, 3, 0.45f) * 0.65f
			+ normalizedParam(settings, 7, 0.45f) * 0.35f;
		const float durationSeconds = musicalSeconds(timing, context.bpm, 0.125f, 4.f);
		const float durationSamples = std::max(8.f, durationSeconds * sampleRate_);
		const int selectedMode = std::min(3, static_cast<int>(normalizedParam(settings, 2, 0.f) * 4.f));
		if (context.active && context.blockStart) {
			mode_ = selectedMode;
			eventAge_ = 0u;
			readDelay_ = mode_ == 1 ? std::min(history_.maximumDelaySamples(), durationSamples * 0.5f) : 2.f;
			motorSpeed_.out = mode_ == 1 ? 0.f : 1.f;
		}

		float speed = 1.f;
		if (context.active) {
			const float phase = clamp01(static_cast<float>(eventAge_) / durationSamples);
			const float slowShape = 0.5f * normalizedParam(settings, 0, 0.5f)
				+ 0.5f * normalizedParam(settings, 4, 0.5f);
			const float slowCurve = 0.25f + slowShape * 3.75f;
			const float fastCurve = 0.25f + normalizedParam(settings, 1, 0.5f) * 3.75f;
			if (mode_ == 0) {
				speed = 1.f - std::pow(phase, slowCurve);
			}
			else if (mode_ == 1) {
				speed = std::pow(phase, fastCurve);
			}
			else if (mode_ == 2) {
				if (phase < 0.5f) {
					speed = 1.f - std::pow(phase * 2.f, slowCurve);
				}
				else {
					speed = 2.f * std::pow((phase - 0.5f) * 2.f, fastCurve);
				}
			}
			else {
				speed = std::cos(kFrayTwoPi * phase);
			}
			if (phase >= 1.f) {
				speed = mode_ == 0 ? 0.f : 1.f;
			}
			const float smoothingSeconds = rackSafeTau(
				exponentialMap(normalizedParam(settings, 6, 0.35f), 0.0001f, 0.05f), sampleTime_);
			if (smoothingSeconds != cachedSmoothingSeconds_) {
				motorSpeed_.setTau(smoothingSeconds);
				cachedSmoothingSeconds_ = smoothingSeconds;
			}
			motorSpeed_.out = flushDenormal(clampFinite(
				motorSpeed_.process(sampleTime_, speed), -1.f, 2.f, speed));
			readDelay_ += 1.f - motorSpeed_.out;
			readDelay_ = clampFinite(readDelay_, 2.f, history_.maximumDelaySamples(), 2.f);
			const std::size_t maximumAge = static_cast<std::size_t>(durationSamples) + 1u;
			eventAge_ = std::min(maximumAge, eventAge_ + 1u);
		}
		StereoFrame wet = history_.readDelay(readDelay_);
		if (mode_ == 0 && static_cast<float>(eventAge_) >= durationSamples) {
			const float holdLevel = normalizedParam(settings, 5, 0.5f);
			wet.left *= holdLevel;
			wet.right *= holdLevel;
		}
		return crossfadeFrames(dry, wet, ramp_.process(context.active));
	}

private:
	float sampleRate_;
	float sampleTime_;
	StereoCircularHistory history_;
	ActivitySlew ramp_;
	float readDelay_;
	rack::dsp::ExponentialFilter motorSpeed_;
	float cachedSmoothingSeconds_;
	std::size_t eventAge_;
	int mode_;
};

class RetriggerEffect {
public:
	RetriggerEffect()
	: sampleRate_(48000.f), playPosition_(0.f), transitionAge_(0u), repeatCount_(0u) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		capture_.prepare(sampleRate_, 1.25f);
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		capture_.reset();
		playPosition_ = 0.f;
		transitionAge_ = 0u;
		repeatCount_ = 0u;
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 initial speed, 1 final speed, 2 transition, 3 repeat decay,
		// 4 capture time, 5 pitch coupling, 6 boundary smoothing, 7 timing range,
		// 8..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		const float timing = normalizedParam(settings, 4, 0.35f) * 0.65f
			+ normalizedParam(settings, 7, 0.35f) * 0.35f;
		const float duration = musicalSeconds(timing, context.bpm, 0.0625f, 2.f);
		const std::size_t captureSamples = static_cast<std::size_t>(clampFinite(
			duration * sampleRate_, 8.f, static_cast<float>(capture_.capacitySamples()), 64.f));
		if (context.active && context.blockStart) {
			capture_.beginCapture(captureSamples);
			playPosition_ = 0.f;
			transitionAge_ = 0u;
			repeatCount_ = 0u;
		}

		StereoFrame wet = dry;
		if (context.active && capture_.capturing()) {
			capture_.capture(dry);
		}
		else if (context.active && capture_.complete()) {
			const float transitionSeconds = exponentialMap(normalizedParam(settings, 2, 0.5f), 0.05f, 4.f);
			const float transition = clamp01(static_cast<float>(transitionAge_) / (transitionSeconds * sampleRate_));
			const float initialRate = std::pow(2.f, normalizedParam(settings, 0, 0.5f) * 4.f - 2.f);
			const float finalRate = std::pow(2.f, normalizedParam(settings, 1, 0.5f) * 4.f - 2.f);
			const float eventRate = initialRate * std::pow(finalRate / initialRate, transition);
			const float pitchCoupling = normalizedParam(settings, 5, 0.5f);
			const float rate = std::pow(eventRate, pitchCoupling);
			wet = capture_.readWrapped(playPosition_);
			const float fadeSamples = std::min(
				static_cast<float>(capture_.length()) * 0.2f,
				2.f + normalizedParam(settings, 6, 0.5f) * 0.01f * sampleRate_);
			if (fadeSamples > 1.f) {
				const float edge = std::min(playPosition_, static_cast<float>(capture_.length()) - playPosition_);
				const float edgeGain = clamp01(edge / fadeSamples);
				wet.left *= edgeGain;
				wet.right *= edgeGain;
			}
			const float decayBase = 1.f - normalizedParam(settings, 3, 0.25f) * 0.6f;
			const float repeatGain = std::pow(std::max(0.01f, decayBase), static_cast<float>(repeatCount_));
			wet.left *= repeatGain;
			wet.right *= repeatGain;
			playPosition_ += rate;
			while (playPosition_ >= static_cast<float>(capture_.length())) {
				playPosition_ -= static_cast<float>(capture_.length());
				repeatCount_ = std::min<std::size_t>(1000000u, repeatCount_ + 1u);
			}
			const std::size_t maximumTransitionAge = static_cast<std::size_t>(transitionSeconds * sampleRate_) + 1u;
			transitionAge_ = std::min(maximumTransitionAge, transitionAge_ + 1u);
		}
		capture_.idleTick(dry);
		return crossfadeFrames(dry, sanitizeFrame(wet), ramp_.process(context.active));
	}

private:
	float sampleRate_;
	StereoCapture capture_;
	ActivitySlew ramp_;
	float playPosition_;
	std::size_t transitionAge_;
	std::size_t repeatCount_;
};

class ReverserEffect {
public:
	ReverserEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), position_(0.f), direction_(1.f),
	  cachedSmoothingSeconds_(-1.f) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		capture_.prepare(sampleRate_, 1.25f);
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		capture_.reset();
		position_ = 0.f;
		direction_ = 1.f;
		smoothL_.reset();
		smoothR_.reset();
		cachedSmoothingSeconds_ = -1.f;
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 time, 1 point A, 2 point B, 3 turnaround smoothing,
		// 4 capture scale, 5 playback rate, 6 wet blend, 7 timing range,
		// 8..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		const float timing = normalizedParam(settings, 0, 0.35f) * 0.65f
			+ normalizedParam(settings, 7, 0.35f) * 0.35f;
		const float baseDuration = musicalSeconds(timing, context.bpm, 0.0625f, 2.f);
		const float duration = baseDuration * (0.5f + 1.5f * normalizedParam(settings, 4, 0.5f));
		const std::size_t captureSamples = static_cast<std::size_t>(clampFinite(
			duration * sampleRate_, 8.f, static_cast<float>(capture_.capacitySamples()), 64.f));
		if (context.active && context.blockStart) {
			capture_.beginCapture(captureSamples);
			position_ = 0.f;
			direction_ = 1.f;
			smoothL_.out = dry.left;
			smoothR_.out = dry.right;
		}

		StereoFrame wet = dry;
		if (context.active && capture_.capturing()) {
			capture_.capture(dry);
		}
		else if (context.active && capture_.complete()) {
			float a = normalizedParam(settings, 1, 0.f);
			float b = normalizedParam(settings, 2, 1.f);
			if (a > b) {
				std::swap(a, b);
			}
			const float maximumIndex = static_cast<float>(capture_.length() - 1u);
			float pointA = a * maximumIndex;
			float pointB = b * maximumIndex;
			if (pointB - pointA < 4.f) {
				pointB = std::min(maximumIndex, pointA + 4.f);
				pointA = std::max(0.f, pointB - 4.f);
			}
			position_ = clampFinite(position_, pointA, pointB, pointA);
			const StereoFrame raw = capture_.readClamped(position_);
			const float smoothingTime = rackSafeTau(
				exponentialMap(normalizedParam(settings, 3, 0.35f), 0.00005f, 0.01f), sampleTime_);
			if (smoothingTime != cachedSmoothingSeconds_) {
				smoothL_.setTau(smoothingTime);
				smoothR_.setTau(smoothingTime);
				cachedSmoothingSeconds_ = smoothingTime;
			}
			smoothL_.out = flushDenormal(clampFinite(
				smoothL_.process(sampleTime_, raw.left), -16.f, 16.f, raw.left));
			smoothR_.out = flushDenormal(clampFinite(
				smoothR_.process(sampleTime_, raw.right), -16.f, 16.f, raw.right));
			wet = crossfadeFrames(
				dry, StereoFrame(smoothL_.out, smoothR_.out), normalizedParam(settings, 6, 1.f));

			const float playbackRate = exponentialMap(normalizedParam(settings, 5, 0.5f), 0.5f, 2.f);
			position_ += direction_ * playbackRate;
			if (position_ > pointB) {
				position_ = pointB - (position_ - pointB);
				direction_ = -1.f;
			}
			else if (position_ < pointA) {
				position_ = pointA + (pointA - position_);
				direction_ = 1.f;
			}
		}
		capture_.idleTick(dry);
		return crossfadeFrames(dry, sanitizeFrame(wet), ramp_.process(context.active));
	}

private:
	float sampleRate_;
	float sampleTime_;
	StereoCapture capture_;
	ActivitySlew ramp_;
	float position_;
	float direction_;
	rack::dsp::ExponentialFilter smoothL_;
	rack::dsp::ExponentialFilter smoothR_;
	float cachedSmoothingSeconds_;
};

class StretcherEffect {
public:
	StretcherEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), spawnCountdown_(0.f), sourceDelay_(2.f),
	  cachedAttack_(-1.f), cachedRelease_(-1.f) {
		clearGrains();
	}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		history_.prepare(sampleRate_, 2.f);
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		history_.reset();
		envelope_.reset();
		cachedAttack_ = -1.f;
		cachedRelease_ = -1.f;
		spawnCountdown_ = 0.f;
		sourceDelay_ = 2.f;
		clearGrains();
		rng_.reseed(0x5354524554434845ULL);
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 speed, 1 grain size, 2 jitter, 3 smoothing/overlap,
		// 4 grain-mod depth, 5 attack, 6 release, 7 voice count (2..4),
		// 8..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		history_.push(dry);
		const float detector = clampFinite(std::max(std::fabs(dry.left), std::fabs(dry.right)), 0.f, 2.f);
		const float attack = rackSafeTau(
			exponentialMap(normalizedParam(settings, 5, 0.25f), 0.001f, 0.25f), sampleTime_);
		const float release = rackSafeTau(
			exponentialMap(normalizedParam(settings, 6, 0.5f), 0.005f, 1.f), sampleTime_);
		if (attack != cachedAttack_ || release != cachedRelease_) {
			envelope_.setRiseFallTau(attack, release);
			cachedAttack_ = attack;
			cachedRelease_ = release;
		}
		envelope_.out = flushDenormal(clampFinite(
			envelope_.process(sampleTime_, detector), 0.f, 2.f));
		const float envelope = envelope_.out;

		const float speed = std::pow(2.f, normalizedParam(settings, 0, 0.5f) * 3.f - 1.5f);
		const float baseGrainSeconds = exponentialMap(normalizedParam(settings, 1, 0.45f), 0.01f, 0.25f);
		const float modulation = std::pow(2.f, normalizedParam(settings, 4, 0.f) * std::min(1.f, envelope) * 2.f);
		const std::size_t grainSamples = static_cast<std::size_t>(clampFinite(
			baseGrainSeconds * modulation * sampleRate_, 16.f, sampleRate_ * 0.5f, 256.f));
		const float smoothing = normalizedParam(settings, 3, 0.6f);
		const float hop = static_cast<float>(grainSamples) * (0.75f - 0.5f * smoothing);

		if (context.active && context.blockStart) {
			clearGrains();
			spawnCountdown_ = 0.f;
			sourceDelay_ = std::max(2.f, static_cast<float>(grainSamples));
			rng_.reseed(effectEventSeed(context, 0x5354524554434845ULL));
		}

		const std::size_t maximumVoices = 2u + static_cast<std::size_t>(
			std::floor(normalizedParam(settings, 7, 0.5f) * 2.999f));
		if (context.active) {
			if (spawnCountdown_ <= 0.f) {
				spawnGrain(grainSamples, normalizedParam(settings, 2, 0.25f), maximumVoices);
				spawnCountdown_ += std::max(4.f, hop);
				sourceDelay_ += hop * (1.f - speed);
				sourceDelay_ = clampFinite(sourceDelay_, 2.f, history_.maximumDelaySamples(), static_cast<float>(grainSamples));
			}
			spawnCountdown_ -= 1.f;
		}
		else {
			spawnCountdown_ = 0.f;
		}

		StereoFrame sum;
		float windowSum = 0.f;
		for (std::size_t i = 0; i < grains_.size(); ++i) {
			Grain& grain = grains_[i];
			if (!grain.active || grain.duration < 2u) {
				continue;
			}
			if (grain.age >= grain.duration) {
				grain.active = false;
				continue;
			}
			const float phase = static_cast<float>(grain.age)
				/ static_cast<float>(grain.duration - 1u);
			const float window = rack::dsp::hann(phase);
			const StereoFrame sample = history_.readDelay(grain.delay);
			sum.left += sample.left * window;
			sum.right += sample.right * window;
			windowSum += window;
			grain.age++;
		}
		const StereoFrame wet = windowSum > 1.0e-5f
			? StereoFrame(sum.left / windowSum, sum.right / windowSum)
			: dry;
		return crossfadeFrames(dry, sanitizeFrame(wet), ramp_.process(context.active));
	}

private:
	struct Grain {
		bool active;
		std::size_t age;
		std::size_t duration;
		float delay;
		Grain() : active(false), age(0u), duration(0u), delay(2.f) {}
	};

	void clearGrains() {
		for (std::size_t i = 0; i < grains_.size(); ++i) {
			grains_[i] = Grain();
		}
	}

	void spawnGrain(std::size_t duration, float jitterAmount, std::size_t maximumVoices) {
		maximumVoices = std::max<std::size_t>(2u, std::min(maximumVoices, grains_.size()));
		std::size_t target = 0u;
		for (std::size_t i = 0; i < maximumVoices; ++i) {
			if (!grains_[i].active) {
				target = i;
				break;
			}
			if (grains_[i].age > grains_[target].age) {
				target = i;
			}
		}
		const float jitter = (rng_.uniform() * 2.f - 1.f) * jitterAmount * static_cast<float>(duration) * 0.75f;
		grains_[target].active = true;
		grains_[target].age = 0u;
		grains_[target].duration = duration;
		grains_[target].delay = clampFinite(sourceDelay_ + jitter, 2.f, history_.maximumDelaySamples(), sourceDelay_);
	}

	float sampleRate_;
	float sampleTime_;
	StereoCircularHistory history_;
	ActivitySlew ramp_;
	DeterministicRng rng_;
	std::array<Grain, 4> grains_;
	rack::dsp::ExponentialSlewLimiter envelope_;
	float spawnCountdown_;
	float sourceDelay_;
	float cachedAttack_;
	float cachedRelease_;
};

class LofiEffect {
public:
	LofiEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), holdPhase_(0.f), held_(0.f, 0.f),
	  jitterMultiplier_(1.f), cachedAttack_(-1.f), cachedRelease_(-1.f) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		holdPhase_ = 0.f;
		held_ = StereoFrame();
		envelope_.reset();
		cachedAttack_ = -1.f;
		cachedRelease_ = -1.f;
		jitterMultiplier_ = 1.f;
		rng_.reseed(0x4c4f46495f465241ULL);
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 signed/unsigned mode, 1 bit depth, 2 held sample rate,
		// 3 envelope-FM depth, 4 attack, 5 release, 6 quantizer bias,
		// 7 clock jitter, 8..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		if (context.active && context.blockStart) {
			rng_.reseed(effectEventSeed(context, 0x4c4f46495f465241ULL));
			jitterMultiplier_ = 1.f;
		}
		const float detector = clampFinite(std::max(std::fabs(dry.left), std::fabs(dry.right)), 0.f, 2.f);
		const float attack = rackSafeTau(
			exponentialMap(normalizedParam(settings, 4, 0.25f), 0.001f, 0.25f), sampleTime_);
		const float release = rackSafeTau(
			exponentialMap(normalizedParam(settings, 5, 0.5f), 0.005f, 1.f), sampleTime_);
		if (attack != cachedAttack_ || release != cachedRelease_) {
			envelope_.setRiseFallTau(attack, release);
			cachedAttack_ = attack;
			cachedRelease_ = release;
		}
		envelope_.out = flushDenormal(clampFinite(
			envelope_.process(sampleTime_, detector), 0.f, 2.f));
		const float envelope = envelope_.out;

		const int bits = 2 + static_cast<int>(std::floor(normalizedParam(settings, 1, 0.65f) * 14.999f));
		const float baseFrequency = exponentialMap(normalizedParam(settings, 2, 0.65f), 80.f, sampleRate_ * 0.49f);
		const float fmOctaves = normalizedParam(settings, 3, 0.f) * 4.f;
		const float frequency = clampFinite(
			baseFrequency * std::pow(2.f, fmOctaves * std::min(1.f, envelope)) * jitterMultiplier_,
			1.f, sampleRate_ * 0.49f);
		holdPhase_ += frequency / sampleRate_;
		if (holdPhase_ >= 1.f) {
			holdPhase_ -= std::floor(holdPhase_);
			const bool unsignedMode = normalizedParam(settings, 0, 0.f) >= 0.5f;
			const float levelScale = static_cast<float>((1 << std::min(16, std::max(2, bits))) / 2);
			const float bias = (normalizedParam(settings, 6, 0.5f) * 2.f - 1.f) / std::max(1.f, levelScale);
			held_.left = quantize(dry.left + bias, bits, unsignedMode) - bias;
			held_.right = quantize(dry.right + bias, bits, unsignedMode) - bias;
			jitterMultiplier_ = std::pow(2.f,
				rng_.bipolar() * normalizedParam(settings, 7, 0.f) * 0.5f);
		}
		return crossfadeFrames(dry, held_, ramp_.process(context.active));
	}

private:
	static float quantize(float input, int bits, bool unsignedMode) {
		const float x = clampFinite(input, -1.f, 1.f);
		const int levels = 1 << std::min(16, std::max(2, bits));
		if (unsignedMode) {
			const float index = std::floor((x + 1.f) * 0.5f * static_cast<float>(levels));
			const float boundedIndex = std::min(static_cast<float>(levels - 1), std::max(0.f, index));
			return ((boundedIndex + 0.5f) / static_cast<float>(levels)) * 2.f - 1.f;
		}
		const float scale = static_cast<float>(levels / 2 - 1);
		return scale > 0.f ? std::round(x * scale) / scale : 0.f;
	}

	float sampleRate_;
	float sampleTime_;
	float holdPhase_;
	StereoFrame held_;
	rack::dsp::ExponentialSlewLimiter envelope_;
	float jitterMultiplier_;
	DeterministicRng rng_;
	ActivitySlew ramp_;
	float cachedAttack_;
	float cachedRelease_;
};

class DistortionEffect {
public:
	DistortionEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), cachedTone_(-1.f),
	  selectedQuality_(-1), processing_(false), lastWet_(), transitionWetFrom_() {
		activity_.prepare(sampleRate_, 3.f);
		qualityTransition_.setRiseFall(1.f / 0.003f, 1.f / 0.003f);
		toneSmoother_.setTau(0.005f);
		toneUpdateDivider_.setDivision(16u);
		configureFilters(0.5f);
		reset();
	}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		cachedTone_ = -1.f;
		activity_.prepare(sampleRate_, 3.f);
		qualityTransition_.setRiseFall(1.f / 0.003f, 1.f / 0.003f);
		toneSmoother_.setTau(0.005f);
		configureFilters(0.5f);
		reset();
	}
	void reset() {
		resetSignalPath();
		activity_.reset();
		qualityTransition_.reset();
		qualityTransition_.out = 1.f;
		toneSmoother_.reset();
		toneUpdateDivider_.reset();
		toneInitialized_ = false;
		selectedQuality_ = -1;
		processing_ = false;
		lastWet_ = StereoFrame();
		transitionWetFrom_ = StereoFrame();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		const StereoFrame dry = sanitizeFrame(input);
		const float activity = activity_.process(context.active);
		if (!context.active && activity <= 0.f) {
			if (processing_) {
				resetSignalPath();
			}
			processing_ = false;
			selectedQuality_ = -1;
			qualityTransition_.out = 1.f;
			toneInitialized_ = false;
			lastWet_ = StereoFrame();
			transitionWetFrom_ = StereoFrame();
			return StereoFrame(dry.left, dry.right);
		}
		processing_ = true;

		// Slots: 0 mode (Razor/Shape/Fold/Shift), 1 drive, 2 tone,
		// 3 wet level, 4 dry level, 5 bias, 6 shape, 7 quality hint,
		// 8..11 reserved. Quality is a discrete Raw/2x choice. Only the
		// selected path runs; a short wet-only transition prevents a hard edge
		// when scenes switch paths while keeping the live dry branch transparent.
		const int quality = normalizedParam(settings, 7, 0.f) >= 0.5f ? 1 : 0;
		bool qualityChanged = false;
		if (selectedQuality_ < 0) {
			selectedQuality_ = quality;
			qualityTransition_.out = 1.f;
		}
		else if (quality != selectedQuality_) {
			transitionWetFrom_ = lastWet_;
			selectedQuality_ = quality;
			resetOversamplingPath();
			qualityTransition_.reset();
			qualityChanged = true;
		}

		const int mode = std::min(3, static_cast<int>(std::round(normalizedParam(settings, 0, 0.f) * 3.f)));
		const float drive = 1.f + normalizedParam(settings, 1, 0.5f) * 19.f;
		const float bias = (normalizedParam(settings, 5, 0.5f) * 2.f - 1.f) * 0.5f;
		const float shapeAmount = normalizedParam(settings, 6, 0.5f);
		const float biasedLeft = finiteOrSilence(dry.left + bias);
		const float biasedRight = finiteOrSilence(dry.right + bias);
		if (qualityChanged && selectedQuality_ > 0) {
			primeOversamplingPath(
				biasedLeft, biasedRight, mode, drive, shapeAmount);
		}

		float shapedLeft;
		float shapedRight;
		if (selectedQuality_ > 0) {
			float left[2];
			float right[2];
			upL_.process(biasedLeft, left);
			upR_.process(biasedRight, right);
			for (int i = 0; i < 2; ++i) {
				left[i] = shape(left[i], mode, drive, shapeAmount);
				right[i] = shape(right[i], mode, drive, shapeAmount);
			}
			shapedLeft = downL_.process(left);
			shapedRight = downR_.process(right);
		}
		else {
			shapedLeft = shape(biasedLeft, mode, drive, shapeAmount);
			shapedRight = shape(biasedRight, mode, drive, shapeAmount);
		}

		// Bias and Shift can add DC. Rack's RC highpass keeps the cutoff fixed in
		// Hz across sample rates instead of relying on a sample-rate-dependent R.
		dcBlockL_.process(clampFinite(shapedLeft, -16.f, 16.f));
		dcBlockR_.process(clampFinite(shapedRight, -16.f, 16.f));
		const float blockedLeft = clampFinite(dcBlockL_.highpass(), -16.f, 16.f);
		const float blockedRight = clampFinite(dcBlockR_.highpass(), -16.f, 16.f);

		const float toneTarget = normalizedParam(settings, 2, 0.5f);
		const bool initializeTone = !toneInitialized_;
		if (initializeTone) {
			toneSmoother_.out = toneTarget;
			toneInitialized_ = true;
		}
		else {
			toneSmoother_.out = clamp01(toneSmoother_.process(sampleTime_, toneTarget));
		}
		const float tone = toneSmoother_.out;
		if (initializeTone || toneUpdateDivider_.process()) {
			configureFilters(tone);
		}
		const float lowLeft = flushDenormal(clampFinite(toneL_.process(blockedLeft), -16.f, 16.f));
		const float lowRight = flushDenormal(clampFinite(toneR_.process(blockedRight), -16.f, 16.f));
		const StereoFrame high(blockedLeft - lowLeft, blockedRight - lowRight);
		const StereoFrame toned(
			lowLeft * (1.5f - tone) + high.left * (0.5f + tone),
			lowRight * (1.5f - tone) + high.right * (0.5f + tone));
		const float wetLevel = normalizedParam(settings, 3, 1.f);
		const float dryLevel = normalizedParam(settings, 4, 0.f);
		const float transition = clamp01(qualityTransition_.process(sampleTime_, 1.f));
		const StereoFrame transitionedWet = transition < 1.f
			? crossfadeFrames(transitionWetFrom_, toned, transition)
			: toned;
		const StereoFrame core(
			dry.left * dryLevel + transitionedWet.left * wetLevel,
			dry.right * dryLevel + transitionedWet.right * wetLevel);
		const StereoFrame output = sanitizeFrame(core);
		lastWet_ = toned;
		return output;
	}

private:
	void resetOversamplingPath() {
		upL_.reset();
		upR_.reset();
		downL_.reset();
		downR_.reset();
	}

	void primeOversamplingPath(
		float inputLeft,
		float inputRight,
		int mode,
		float drive,
		float shapeAmount) {
		// A constant pre-roll avoids feeding seven samples of startup zero into
		// the shared DC/tone filters when Raw changes to 2x on a live signal.
		for (int frame = 0; frame < 16; ++frame) {
			float left[2];
			float right[2];
			upL_.process(inputLeft, left);
			upR_.process(inputRight, right);
			for (int i = 0; i < 2; ++i) {
				left[i] = shape(left[i], mode, drive, shapeAmount);
				right[i] = shape(right[i], mode, drive, shapeAmount);
			}
			downL_.process(left);
			downR_.process(right);
		}
	}

	void resetSignalPath() {
		resetOversamplingPath();
		dcBlockL_.reset();
		dcBlockR_.reset();
		toneL_.reset();
		toneR_.reset();
	}
	static float shape(float input, int mode, float drive, float shapeAmount) {
		const float x = clampFinite(input * drive, -64.f, 64.f);
		if (mode == 0) {
			return clampFinite(x, -1.f, 1.f);
		}
		if (mode == 1) {
			const float normalizer = std::tanh(std::max(1.f, drive));
			return normalizer > 1.0e-6f ? std::tanh(x) / normalizer : x;
		}
		if (mode == 2) {
			const float threshold = 0.35f + 0.55f * (1.f - shapeAmount);
			if (std::fabs(x) <= threshold) {
				return x;
			}
			float folded = std::fmod(x + threshold, threshold * 4.f);
			if (folded < 0.f) {
				folded += threshold * 4.f;
			}
			return std::fabs(folded - threshold * 2.f) - threshold;
		}
		return clampFinite(
			std::fabs(x) * (x >= 0.f ? 1.f : 0.6f) - 0.25f * shapeAmount, -1.f, 1.f);
	}

	void configureFilters(float tone) {
		const float dcNormalized = clampFinite(20.f / sampleRate_, 1.0e-6f, 0.01f, 20.f / 48000.f);
		dcBlockL_.setCutoffFreq(dcNormalized);
		dcBlockR_.setCutoffFreq(dcNormalized);
		tone = clamp01(tone);
		if (tone == cachedTone_) {
			return;
		}
		const float cutoff = clampFinite(
			300.f * std::pow(40.f, tone), 20.f, sampleRate_ * 0.4f, 2000.f);
		const float normalized = clampFinite(cutoff / sampleRate_, 1.0e-5f, 0.4f, 0.04f);
		toneL_.setParameters(rack::dsp::BiquadFilter::LOWPASS_1POLE, normalized, 0.707f, 1.f);
		toneR_.setParameters(rack::dsp::BiquadFilter::LOWPASS_1POLE, normalized, 0.707f, 1.f);
		cachedTone_ = tone;
	}

	float sampleRate_;
	float sampleTime_;
	float cachedTone_;
	int selectedQuality_;
	bool processing_;
	StereoFrame lastWet_;
	StereoFrame transitionWetFrom_;
	ActivitySlew activity_;
	rack::dsp::SlewLimiter qualityTransition_;
	rack::dsp::ExponentialFilter toneSmoother_;
	rack::dsp::ClockDivider toneUpdateDivider_;
	bool toneInitialized_;
	rack::dsp::Upsampler<2, 8> upL_;
	rack::dsp::Upsampler<2, 8> upR_;
	rack::dsp::Decimator<2, 8> downL_;
	rack::dsp::Decimator<2, 8> downR_;
	rack::dsp::RCFilter dcBlockL_;
	rack::dsp::RCFilter dcBlockR_;
	rack::dsp::BiquadFilter toneL_;
	rack::dsp::BiquadFilter toneR_;
};

class GaterEffect {
public:
	GaterEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), phaseSamples_(0.f), stepIndex_(0),
	  cachedSmoothingSeconds_(-1.f) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		phaseSamples_ = 0.f;
		stepIndex_ = 0;
		levelSmoother_.reset();
		levelSmoother_.out = 1.f;
		cachedSmoothingSeconds_ = -1.f;
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 step time, 1 smoothing, 2 step count, 3 depth,
		// 4 pattern phase, 5 accent, 6 swing, 7 timing range, and 8..11
		// quartet level modifiers. Sixteen levels are derived deterministically
		// from this compact set instead of requiring sixteen scene parameters.
		const StereoFrame dry = sanitizeFrame(input);
		const int steps = 1 + static_cast<int>(std::floor(normalizedParam(settings, 2, 1.f) * 15.999f));
		const float timing = 0.65f * normalizedParam(settings, 0, 0.35f)
			+ 0.35f * normalizedParam(settings, 7, 0.5f);
		const float baseStepSamples = std::max(1.f, musicalSeconds(timing, context.bpm, 0.03125f, 1.f) * sampleRate_);
		if (context.active && context.blockStart) {
			phaseSamples_ = 0.f;
			stepIndex_ = 0;
			levelSmoother_.out = levelForStep(settings, 0);
		}
		if (context.active) {
			phaseSamples_ += 1.f;
			const float swing = normalizedParam(settings, 6, 0.f) * 0.45f;
			const float stepSamples = baseStepSamples * ((stepIndex_ & 1) ? 1.f + swing : 1.f - swing);
			while (phaseSamples_ >= stepSamples) {
				phaseSamples_ -= stepSamples;
				stepIndex_ = (stepIndex_ + 1) % steps;
			}
		}
		const float targetLevel = levelForStep(settings, stepIndex_);
		const float smoothingSeconds = normalizedParam(settings, 1, 0.35f) < 0.001f
			? 0.f
			: rackSafeTau(
				exponentialMap(normalizedParam(settings, 1, 0.35f), 0.00005f, 0.08f), sampleTime_);
		if (smoothingSeconds <= 0.f) {
			levelSmoother_.out = targetLevel;
		}
		else {
			if (smoothingSeconds != cachedSmoothingSeconds_) {
				levelSmoother_.setTau(smoothingSeconds);
				cachedSmoothingSeconds_ = smoothingSeconds;
			}
			levelSmoother_.out = flushDenormal(clampFinite(
				levelSmoother_.process(sampleTime_, targetLevel), 0.f, 1.f, targetLevel));
		}
		const StereoFrame wet(dry.left * levelSmoother_.out, dry.right * levelSmoother_.out);
		return crossfadeFrames(dry, wet, ramp_.process(context.active));
	}

private:
	static float levelForStep(const EffectSettings& settings, int step) {
		static const float pattern[16] = {
			1.f, 0.18f, 0.72f, 0.04f, 0.88f, 0.34f, 0.62f, 0.12f,
			0.96f, 0.28f, 0.55f, 0.08f, 0.78f, 0.42f, 0.68f, 0.16f
		};
		const int phase = static_cast<int>(std::floor(normalizedParam(settings, 4, 0.f) * 15.999f));
		const int wrapped = ((step + phase) % 16 + 16) % 16;
		const float depth = normalizedParam(settings, 3, 1.f);
		const float modifier = 0.5f + normalizedParam(settings, static_cast<std::size_t>(8 + (wrapped & 3)), 0.5f);
		float level = (1.f - depth) + depth * pattern[wrapped] * modifier;
		if ((wrapped & 3) == 0) {
			level += normalizedParam(settings, 5, 0.5f) * 0.35f;
		}
		return clamp01(level);
	}

	float sampleRate_;
	float sampleTime_;
	float phaseSamples_;
	int stepIndex_;
	rack::dsp::ExponentialFilter levelSmoother_;
	ActivitySlew ramp_;
	float cachedSmoothingSeconds_;
};

class DelayEffect {
public:
	DelayEffect()
	: sampleRate_(48000.f), sampleTime_(1.f / 48000.f), delayInitialized_(false),
	  cachedSlewSeconds_(-1.f) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		sampleTime_ = 1.f / sampleRate_;
		history_.prepare(sampleRate_, 4.f);
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		history_.reset();
		delayLeft_.reset();
		delayRight_.reset();
		delayLeft_.out = 2.f;
		delayRight_.out = 2.f;
		delayInitialized_ = false;
		cachedSlewSeconds_ = -1.f;
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 delay time, 1 slew (high is fast), 2 stereo spread,
		// 3 send, 4 feedback, 5 return, 6 ducking, 7 timing range,
		// 8 loop drive, 9..11 reserved.
		const StereoFrame dry = sanitizeFrame(input);
		const float timing = 0.65f * normalizedParam(settings, 0, 0.35f)
			+ 0.35f * normalizedParam(settings, 7, 0.4f);
		const float baseDelay = musicalSeconds(timing, context.bpm, 0.03125f, 4.f) * sampleRate_;
		const float spread = (normalizedParam(settings, 2, 0.5f) * 2.f - 1.f) * 0.45f;
		const float targetLeft = clampFinite(baseDelay * (1.f - spread), 2.f, history_.maximumDelaySamples(), 2.f);
		const float targetRight = clampFinite(baseDelay * (1.f + spread), 2.f, history_.maximumDelaySamples(), 2.f);
		if (!delayInitialized_) {
			delayLeft_.out = targetLeft;
			delayRight_.out = targetRight;
			delayInitialized_ = true;
		}
		const float slewSeconds = rackSafeTau(
			0.002f * std::pow(500.f, 1.f - normalizedParam(settings, 1, 0.5f)), sampleTime_);
		if (slewSeconds != cachedSlewSeconds_) {
			delayLeft_.setTau(slewSeconds);
			delayRight_.setTau(slewSeconds);
			cachedSlewSeconds_ = slewSeconds;
		}
		delayLeft_.out = clampFinite(
			delayLeft_.process(sampleTime_, targetLeft), 2.f, history_.maximumDelaySamples(), targetLeft);
		delayRight_.out = clampFinite(
			delayRight_.process(sampleTime_, targetRight), 2.f, history_.maximumDelaySamples(), targetRight);
		const StereoFrame delayed(
			history_.readDelay(delayLeft_.out).left,
			history_.readDelay(delayRight_.out).right);

		const float activity = ramp_.process(context.active);
		const float send = normalizedParam(settings, 3, 0.75f) * activity;
		const float feedback = normalizedParam(settings, 4, 0.45f) * 0.98f;
		const float loopDrive = 1.f + normalizedParam(settings, 8, 0.25f) * 2.f;
		StereoFrame write(
			dry.left * send + delayed.right * feedback,
			dry.right * send + delayed.left * feedback);
		write.left = boundedLoopSample(write.left, loopDrive);
		write.right = boundedLoopSample(write.right, loopDrive);
		history_.push(write);

		// The input send fades out, but the return intentionally remains connected:
		// this is what lets an existing delay tail continue after a block ends.
		const float detector = std::max(std::fabs(dry.left), std::fabs(dry.right));
		const float duck = 1.f / (1.f + detector * normalizedParam(settings, 6, 0.f) * 4.f);
		const float returnLevel = normalizedParam(settings, 5, 0.65f) * 1.5f * duck;
		return sanitizeFrame(StereoFrame(
			dry.left + delayed.left * returnLevel,
			dry.right + delayed.right * returnLevel));
	}

private:
	static float boundedLoopSample(float value, float drive) {
		const float x = clampFinite(value, -16.f, 16.f);
		if (std::fabs(x) < 1.0e-20f) {
			return 0.f;
		}
		return std::tanh(x * drive) / std::max(1.f, drive * 0.75f);
	}

	float sampleRate_;
	float sampleTime_;
	StereoCircularHistory history_;
	ActivitySlew ramp_;
	rack::dsp::ExponentialFilter delayLeft_;
	rack::dsp::ExponentialFilter delayRight_;
	bool delayInitialized_;
	float cachedSlewSeconds_;
};

class ShufflerEffect {
public:
	ShufflerEffect()
	: sampleRate_(48000.f), sliceStartDelay_(2.f), sliceLength_(0u), slicePosition_(0u),
	  crossfadeLength_(0u), reverse_(false), hasSlice_(false), lastWet_(0.f, 0.f), boundaryFrom_(0.f, 0.f) {}
	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		history_.prepare(sampleRate_, 2.f);
		ramp_.prepare(sampleRate_);
		reset();
	}
	void reset() {
		history_.reset();
		sliceStartDelay_ = 2.f;
		sliceLength_ = 0u;
		slicePosition_ = 0u;
		crossfadeLength_ = 0u;
		reverse_ = false;
		hasSlice_ = false;
		lastWet_ = StereoFrame();
		boundaryFrom_ = StereoFrame();
		rng_.reseed(0x53485546464c4552ULL);
		ramp_.reset();
	}

	StereoFrame process(const StereoFrame& input, const EffectSettings& settings, const EffectContext& context) {
		// Slots: 0 minimum slice, 1 maximum slice, 2 history range,
		// 3 shuffle probability, 4 repeat probability, 5 reverse probability,
		// 6 boundary crossfade, 7 timing range, 8..11 reserved. Probability order is repeat,
		// then shuffle, then reverse.
		const StereoFrame dry = sanitizeFrame(input);
		history_.push(dry);
		if (context.active && context.blockStart) {
			rng_.reseed(effectEventSeed(context, 0x53485546464c4552ULL));
			hasSlice_ = false;
			slicePosition_ = 0u;
		}

		StereoFrame wet = dry;
		if (context.active && history_.availableSamples() > 16u) {
			if (!hasSlice_ || slicePosition_ >= sliceLength_) {
				selectSlice(settings, context);
			}
			if (hasSlice_ && sliceLength_ > 0u) {
				const float readDelay = reverse_
					? sliceStartDelay_ + 2.f * static_cast<float>(slicePosition_)
					: sliceStartDelay_;
				wet = history_.readDelay(readDelay);
				if (crossfadeLength_ > 0u && slicePosition_ < crossfadeLength_) {
					const float fade = static_cast<float>(slicePosition_) / static_cast<float>(crossfadeLength_);
					wet = crossfadeFrames(boundaryFrom_, wet, fade);
				}
				slicePosition_++;
				lastWet_ = wet;
			}
		}
		return crossfadeFrames(dry, sanitizeFrame(wet), ramp_.process(context.active));
	}

private:
	void selectSlice(const EffectSettings& settings, const EffectContext& context) {
		boundaryFrom_ = lastWet_;
		const float baseSeconds = musicalSeconds(normalizedParam(settings, 7, 0.3f), context.bpm, 0.03125f, 1.f);
		float minimumSeconds = baseSeconds * (0.25f + normalizedParam(settings, 0, 0.25f) * 1.75f);
		float maximumSeconds = baseSeconds * (0.25f + normalizedParam(settings, 1, 0.75f) * 1.75f);
		if (minimumSeconds > maximumSeconds) {
			std::swap(minimumSeconds, maximumSeconds);
		}
		const std::size_t minimumLength = static_cast<std::size_t>(std::max(8.f, minimumSeconds * sampleRate_));
		const std::size_t maximumLength = static_cast<std::size_t>(std::max(
			static_cast<float>(minimumLength), maximumSeconds * sampleRate_));
		const bool repeat = hasSlice_ && rng_.uniform() < normalizedParam(settings, 4, 0.2f);
		if (repeat) {
			sliceStartDelay_ += static_cast<float>(sliceLength_);
		}
		else {
			const float randomLength = rack::math::crossfade(
				static_cast<float>(minimumLength), static_cast<float>(maximumLength), clamp01(rng_.uniform()));
			sliceLength_ = std::max<std::size_t>(8u, static_cast<std::size_t>(randomLength));
			const float available = static_cast<float>(history_.availableSamples() > 3u ? history_.availableSamples() - 3u : 2u);
			const float requestedRange = (0.1f + 0.9f * normalizedParam(settings, 2, 0.75f)) * sampleRate_ * 2.f;
			const float range = std::max(2.f, std::min(available, requestedRange));
			const bool shuffle = rng_.uniform() < normalizedParam(settings, 3, 0.75f);
			sliceStartDelay_ = shuffle
				? rack::math::crossfade(2.f, range, clamp01(rng_.uniform()))
				: std::min(range, static_cast<float>(sliceLength_) + 2.f);
		}
		reverse_ = rng_.uniform() < normalizedParam(settings, 5, 0.25f);
		const float maximumRead = std::max(2.f, std::min(
			history_.maximumDelaySamples(),
			static_cast<float>(history_.availableSamples() > 3u ? history_.availableSamples() - 3u : 2u)));
		const float reverseAllowance = reverse_ ? 2.f * static_cast<float>(sliceLength_) : 0.f;
		sliceStartDelay_ = clampFinite(sliceStartDelay_, 2.f, std::max(2.f, maximumRead - reverseAllowance), 2.f);
		const std::size_t maximumPlayable = reverse_
			? static_cast<std::size_t>(std::max(8.f, (maximumRead - sliceStartDelay_) * 0.5f))
			: static_cast<std::size_t>(std::max(8.f, maximumRead - sliceStartDelay_));
		sliceLength_ = std::max<std::size_t>(8u, std::min(sliceLength_, maximumPlayable));
		crossfadeLength_ = static_cast<std::size_t>(normalizedParam(settings, 6, 0.5f) * 0.01f * sampleRate_);
		crossfadeLength_ = std::min(crossfadeLength_, sliceLength_ / 4u);
		slicePosition_ = 0u;
		hasSlice_ = true;
	}

	float sampleRate_;
	StereoCircularHistory history_;
	ActivitySlew ramp_;
	DeterministicRng rng_;
	float sliceStartDelay_;
	std::size_t sliceLength_;
	std::size_t slicePosition_;
	std::size_t crossfadeLength_;
	bool reverse_;
	bool hasSlice_;
	StereoFrame lastWet_;
	StereoFrame boundaryFrom_;
};

class FrayEffects {
public:
	FrayEffects() : sampleRate_(48000.f) {}

	void prepare(float sampleRate) {
		sampleRate_ = safeSampleRate(sampleRate);
		for (std::size_t i = 0; i < commonStages_.size(); ++i) {
			commonStages_[i].prepare(sampleRate_);
		}
		modulator_.prepare(sampleRate_);
		tapeStop_.prepare(sampleRate_);
		retrigger_.prepare(sampleRate_);
		reverser_.prepare(sampleRate_);
		stretcher_.prepare(sampleRate_);
		lofi_.prepare(sampleRate_);
		distortion_.prepare(sampleRate_);
		gater_.prepare(sampleRate_);
		delay_.prepare(sampleRate_);
		shuffler_.prepare(sampleRate_);
	}

	void reset() {
		for (std::size_t i = 0; i < commonStages_.size(); ++i) {
			commonStages_[i].reset();
		}
		modulator_.reset();
		tapeStop_.reset();
		retrigger_.reset();
		reverser_.reset();
		stretcher_.reset();
		lofi_.reset();
		distortion_.reset();
		gater_.reset();
		delay_.reset();
		shuffler_.reset();
	}

	StereoFrame process(
		EffectId effect,
		StereoFrame localInput,
		const EffectSettings& settings,
		const EffectContext& context) {
		const StereoFrame input = sanitizeFrame(localInput);
		const int effectIndex = static_cast<int>(effect);
		if (effectIndex < 0 || effectIndex >= kEffectCount) {
			return StereoFrame(input.left, input.right);
		}
		StereoFrame wet = input;
		// Switch on the underlying value so the defensive invalid-ID fallback is
		// meaningful to strict Clang diagnostics as well as at runtime.
		switch (effectIndex) {
			case MODULATOR: wet = modulator_.process(input, settings, context); break;
			case TAPE_STOP: wet = tapeStop_.process(input, settings, context); break;
			case RETRIGGER: wet = retrigger_.process(input, settings, context); break;
			case REVERSER: wet = reverser_.process(input, settings, context); break;
			case STRETCHER: wet = stretcher_.process(input, settings, context); break;
			case LOFI: wet = lofi_.process(input, settings, context); break;
			case DISTORTION: wet = distortion_.process(input, settings, context); break;
			case GATER: wet = gater_.process(input, settings, context); break;
			case DELAY: wet = delay_.process(input, settings, context); break;
			case SHUFFLER: wet = shuffler_.process(input, settings, context); break;
			default: wet = StereoFrame(input.left, input.right); break;
		}
		return commonStages_[static_cast<std::size_t>(effectIndex)].process(
			input, wet, settings.common, context.active, effectIndex == DELAY, context.sampleTime);
	}

	float sampleRate() const { return sampleRate_; }

private:
	float sampleRate_;
	std::array<CommonStage, kEffectCount> commonStages_;
	ModulatorEffect modulator_;
	TapeStopEffect tapeStop_;
	RetriggerEffect retrigger_;
	ReverserEffect reverser_;
	StretcherEffect stretcher_;
	LofiEffect lofi_;
	DistortionEffect distortion_;
	GaterEffect gater_;
	DelayEffect delay_;
	ShufflerEffect shuffler_;
};

} // namespace Fray
} // namespace ShortwavDSP

#endif // SHORTWAV_DSP_FRAY_EFFECTS_H
