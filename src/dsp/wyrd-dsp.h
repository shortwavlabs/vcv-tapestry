#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "generated/wyrd_core.h"

namespace shortwav {
namespace wyrd {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kC4Hz = 261.6255653005986f;

enum class StrengthCalibration : int {
  STREGA = 0,
  LINE_GENTLE = 1,
  MODULAR_HOT = 2,
  NUM_MODES
};

enum class TouchSource : int {
  MANUAL = 0,
  CV1 = 1,
  CV2 = 2,
  AGITATION = 3,
  SUB_HARMONICS = 4,
  STRENGTH = 5,
  NOISE = 6,
  NUM_SOURCES
};

constexpr int kNumStrengthCalibrations = static_cast<int>(StrengthCalibration::NUM_MODES);
constexpr int kNumTouchSources = static_cast<int>(TouchSource::NUM_SOURCES);

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline float safeFinite(float v, float fallback = 0.f) {
  return std::isfinite(v) ? v : fallback;
}

inline float lerpf(float a, float b, float t) {
  return a + (b - a) * clampf(t, 0.f, 1.f);
}

inline float smoothstep(float t) {
  t = clampf(t, 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

inline float softClip(float x) {
  return std::tanh(x);
}

inline float equalPowerFade(float a, float b, float t) {
  t = clampf(t, 0.f, 1.f);
  const float angle = t * (kPi * 0.5f);
  return a * std::cos(angle) + b * std::sin(angle);
}

inline float normalizedKnobToFrequency(float knob, float minHz, float maxHz) {
  knob = clampf(safeFinite(knob), 0.f, 1.f);
  minHz = std::max(0.01f, safeFinite(minHz, 20.f));
  maxHz = std::max(minHz, safeFinite(maxHz, minHz));
  return minHz * std::pow(maxHz / minHz, knob);
}

inline float frequencyToPitchVolts(float hz) {
  hz = clampf(safeFinite(hz, kC4Hz), 0.01f, 40000.f);
  return std::log2(hz / kC4Hz);
}

struct DcBlocker {
  float x1 = 0.f;
  float y1 = 0.f;

  void reset() {
    x1 = 0.f;
    y1 = 0.f;
  }

  float process(float x, float cutoffHz, float sampleRate) {
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, 384000.f);
    cutoffHz = clampf(safeFinite(cutoffHz, 8.f), 0.1f, sampleRate * 0.1f);
    const float r = std::exp(-2.f * kPi * cutoffHz / sampleRate);
    const float y = safeFinite(x) - x1 + r * y1;
    x1 = safeFinite(x);
    y1 = y + 1e-24f;
    return safeFinite(y);
  }
};

struct OnePoleLowpass {
  float y = 0.f;

  void reset() {
    y = 0.f;
  }

  float process(float x, float cutoffHz, float sampleRate) {
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, 384000.f);
    cutoffHz = clampf(safeFinite(cutoffHz, 10.f), 0.01f, sampleRate * 0.45f);
    const float a = 1.f - std::exp(-2.f * kPi * cutoffHz / sampleRate);
    y += a * (safeFinite(x) - y);
    y += 1e-24f;
    return safeFinite(y);
  }
};

struct ControlSmoother {
  bool initialized = false;
  float y = 0.f;

  void reset() {
    initialized = false;
    y = 0.f;
  }

  float process(float target, float seconds, float sampleRate) {
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, 384000.f);
    seconds = clampf(safeFinite(seconds, 0.005f), 0.0001f, 0.1f);
    target = safeFinite(target);
    if (!initialized) {
      y = target;
      initialized = true;
      return y;
    }
    const float a = 1.f - std::exp(-1.f / (seconds * sampleRate));
    y += a * (target - y);
    y += 1e-24f;
    return safeFinite(y);
  }
};

struct EnvelopeFollower {
  float env = 0.f;

  void reset() {
    env = 0.f;
  }

  float process(float x, float attackSeconds, float releaseSeconds, float sampleRate) {
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, 384000.f);
    attackSeconds = clampf(safeFinite(attackSeconds, 0.002f), 0.0001f, 1.f);
    releaseSeconds = clampf(safeFinite(releaseSeconds, 0.08f), 0.0001f, 2.f);
    const float a = std::exp(-1.f / (attackSeconds * sampleRate));
    const float r = std::exp(-1.f / (releaseSeconds * sampleRate));
    const float target = clampf(std::fabs(safeFinite(x)), 0.f, 2.f);
    const float coeff = target > env ? a : r;
    env = target + coeff * (env - target);
    env += 1e-24f;
    return clampf(safeFinite(env), 0.f, 1.f);
  }
};

struct DelayAllpass {
  std::vector<float> buffer;
  int writeIndex = 0;

  explicit DelayAllpass(int maxSamples = 32768)
      : buffer(static_cast<std::size_t>(std::max(8, maxSamples)), 0.f) {
  }

  void reset() {
    std::fill(buffer.begin(), buffer.end(), 0.f);
    writeIndex = 0;
  }

  float process(float input, float delaySamples, float feedback) {
    const int size = static_cast<int>(buffer.size());
    delaySamples = clampf(safeFinite(delaySamples), 1.f, static_cast<float>(size - 2));
    feedback = clampf(safeFinite(feedback), -0.86f, 0.86f);

    int readIndex = writeIndex - static_cast<int>(delaySamples);
    while (readIndex < 0) {
      readIndex += size;
    }

    const float delayed = buffer[readIndex];
    const float output = delayed - feedback * input;
    buffer[writeIndex] = safeFinite(input + feedback * output);
    writeIndex++;
    if (writeIndex >= size) {
      writeIndex = 0;
    }
    return clampf(safeFinite(output), -2.f, 2.f);
  }
};

struct ReverbDelayLine {
  std::vector<float> buffer;
  int writeIndex = 0;

  explicit ReverbDelayLine(int maxSamples = 262144)
      : buffer(static_cast<std::size_t>(std::max(8, maxSamples)), 0.f) {
  }

  void reset() {
    std::fill(buffer.begin(), buffer.end(), 0.f);
    writeIndex = 0;
  }

  float read(float delaySamples) const {
    const int size = static_cast<int>(buffer.size());
    delaySamples = clampf(safeFinite(delaySamples), 1.f, static_cast<float>(size - 4));
    float readPos = static_cast<float>(writeIndex) - delaySamples;
    while (readPos < 0.f) {
      readPos += static_cast<float>(size);
    }
    while (readPos >= static_cast<float>(size)) {
      readPos -= static_cast<float>(size);
    }

    const int i0 = static_cast<int>(readPos);
    const int i1 = (i0 + 1) % size;
    const float frac = readPos - static_cast<float>(i0);
    return safeFinite(buffer[i0] + (buffer[i1] - buffer[i0]) * frac);
  }

  void write(float sample) {
    buffer[writeIndex] = safeFinite(sample);
    writeIndex++;
    if (writeIndex >= static_cast<int>(buffer.size())) {
      writeIndex = 0;
    }
  }
};

struct WyrdReverbResult {
  float left = 0.f;
  float right = 0.f;
  float mono = 0.f;
};

struct WyrdReverbExpanderMessage {
  float modularVolts = 0.f;
  float cv2Volts = 0.f;
  bool active = false;
};

struct WyrdAmbientReverb {
  DelayAllpass inputDiffuserA;
  DelayAllpass inputDiffuserB;
  DelayAllpass inputDiffuserC;
  DelayAllpass inputDiffuserD;
  DelayAllpass tankDiffuserL;
  DelayAllpass tankDiffuserR;
  ReverbDelayLine tankL;
  ReverbDelayLine tankR;
  OnePoleLowpass dampL;
  OnePoleLowpass dampR;
  DcBlocker dcL;
  DcBlocker dcR;
  float modPhaseL = 0.f;
  float modPhaseR = 0.37f;

  WyrdAmbientReverb()
      : inputDiffuserA(32768),
        inputDiffuserB(32768),
        inputDiffuserC(32768),
        inputDiffuserD(32768),
        tankDiffuserL(65536),
        tankDiffuserR(65536),
        tankL(262144),
        tankR(262144) {
  }

  void reset() {
    inputDiffuserA.reset();
    inputDiffuserB.reset();
    inputDiffuserC.reset();
    inputDiffuserD.reset();
    tankDiffuserL.reset();
    tankDiffuserR.reset();
    tankL.reset();
    tankR.reset();
    dampL.reset();
    dampR.reset();
    dcL.reset();
    dcR.reset();
    modPhaseL = 0.f;
    modPhaseR = 0.37f;
  }

  WyrdReverbResult process(float input, float size, float decay, float diffusion,
                           float tone, float modulation, float sampleRate) {
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, 192000.f);
    input = clampf(safeFinite(input), -2.f, 2.f);
    size = clampf(safeFinite(size), 0.f, 1.f);
    decay = clampf(safeFinite(decay), 0.f, 1.f);
    diffusion = clampf(safeFinite(diffusion), 0.f, 1.f);
    tone = clampf(safeFinite(tone), 0.f, 1.f);
    modulation = clampf(safeFinite(modulation), 0.f, 1.f);

    const float sizeScale = lerpf(0.34f, 1.35f, smoothstep(size));
    const float diffFeedback = lerpf(0.28f, 0.72f, smoothstep(diffusion));
    const float decayGain = lerpf(0.52f, 0.985f, smoothstep(decay));
    const float toneHz = normalizedKnobToFrequency(tone, 850.f, 18000.f);

    modPhaseL += (0.035f + 0.46f * modulation) / sampleRate;
    modPhaseR += (0.049f + 0.37f * modulation) / sampleRate;
    modPhaseL -= std::floor(modPhaseL);
    modPhaseR -= std::floor(modPhaseR);
    const float modDepthSamples = sampleRate * lerpf(0.00025f, 0.0075f, modulation);
    const float modL = std::sin(2.f * kPi * modPhaseL) * modDepthSamples;
    const float modR = std::sin(2.f * kPi * modPhaseR) * modDepthSamples;

    float diffused = input * 0.38f;
    diffused = inputDiffuserA.process(diffused, sampleRate * 0.0048f * sizeScale, diffFeedback);
    diffused = inputDiffuserB.process(diffused, sampleRate * 0.0127f * sizeScale, -diffFeedback * 0.86f);
    diffused = inputDiffuserC.process(diffused, sampleRate * 0.0219f * sizeScale, diffFeedback * 0.82f);
    diffused = inputDiffuserD.process(diffused, sampleRate * 0.0307f * sizeScale, -diffFeedback * 0.76f);

    const float readL = tankL.read(sampleRate * 0.089f * sizeScale + modL);
    const float readR = tankR.read(sampleRate * 0.113f * sizeScale + modR);
    const float crossL = dampL.process(readR, toneHz, sampleRate);
    const float crossR = dampR.process(readL, toneHz, sampleRate);

    const float loopL = tankDiffuserL.process(diffused + crossL * decayGain,
                                              sampleRate * 0.026f * sizeScale + modR * 0.35f,
                                              diffFeedback * 0.84f);
    const float loopR = tankDiffuserR.process(-diffused + crossR * decayGain,
                                              sampleRate * 0.031f * sizeScale + modL * 0.35f,
                                              -diffFeedback * 0.78f);

    tankL.write(clampf(softClip(loopL * 1.12f), -1.2f, 1.2f));
    tankR.write(clampf(softClip(loopR * 1.12f), -1.2f, 1.2f));

    const float shimmerL = tankR.read(sampleRate * 0.057f * sizeScale + modR * 0.42f);
    const float shimmerR = tankL.read(sampleRate * 0.071f * sizeScale + modL * 0.42f);
    WyrdReverbResult result;
    result.left = clampf(dcL.process(softClip(readL * 0.72f + shimmerL * 0.38f), 12.f, sampleRate), -1.f, 1.f);
    result.right = clampf(dcR.process(softClip(readR * 0.72f - shimmerR * 0.38f), 12.f, sampleRate), -1.f, 1.f);
    result.mono = clampf(softClip((result.left + result.right) * 0.64f), -1.f, 1.f);
    return result;
  }
};

struct StrengthProcessor {
  DcBlocker dcBlocker;
  EnvelopeFollower follower;
  float strengthNorm = 0.f;
  float cv1Norm = 0.f;
  float pickupPhase = 0.f;
  std::uint32_t pickupRng = 0x57595244u;

  void reset() {
    dcBlocker.reset();
    follower.reset();
    strengthNorm = 0.f;
    cv1Norm = 0.f;
    pickupPhase = 0.f;
    pickupRng = 0x57595244u;
  }

  float nextPickupNoise() {
    pickupRng ^= pickupRng << 13;
    pickupRng ^= pickupRng >> 17;
    pickupRng ^= pickupRng << 5;
    return (static_cast<float>(pickupRng & 0x00ffffffu) / static_cast<float>(0x007fffffu)) - 1.f;
  }

  float process(float externalNorm, float strength, float sampleRate,
                StrengthCalibration calibration = StrengthCalibration::STREGA,
                bool externalConnected = true) {
    strength = clampf(safeFinite(strength), 0.f, 1.f);
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, 384000.f);
    const int mode = static_cast<int>(clampf(static_cast<float>(static_cast<int>(calibration)), 0.f,
                                             static_cast<float>(kNumStrengthCalibrations - 1)));
    const float cleanScale = mode == static_cast<int>(StrengthCalibration::LINE_GENTLE) ? 0.34f
                           : mode == static_cast<int>(StrengthCalibration::MODULAR_HOT) ? 0.82f
                           : 0.5f;
    const float driveCeiling = mode == static_cast<int>(StrengthCalibration::LINE_GENTLE) ? 22.f
                            : mode == static_cast<int>(StrengthCalibration::MODULAR_HOT) ? 64.f
                            : 42.f;
    const float driveExponent = mode == static_cast<int>(StrengthCalibration::LINE_GENTLE) ? 2.15f
                              : mode == static_cast<int>(StrengthCalibration::MODULAR_HOT) ? 2.55f
                              : 2.35f;
    const float attack = mode == static_cast<int>(StrengthCalibration::LINE_GENTLE) ? 0.0042f
                       : mode == static_cast<int>(StrengthCalibration::MODULAR_HOT) ? 0.00075f
                       : 0.0018f;
    const float release = mode == static_cast<int>(StrengthCalibration::LINE_GENTLE) ? 0.16f
                        : mode == static_cast<int>(StrengthCalibration::MODULAR_HOT) ? 0.12f
                        : 0.075f;
    pickupPhase += 60.f / sampleRate;
    pickupPhase -= std::floor(pickupPhase);
    const float highGain = smoothstep((strength - 0.58f) / 0.42f);
    const float openCable = externalConnected ? highGain : 0.f;
    const float cablePickup = openCable * (0.00034f * nextPickupNoise()
                               + 0.00024f * std::sin(2.f * kPi * pickupPhase)
                               + 0.00008f * std::sin(6.f * kPi * pickupPhase));
    const float ac = dcBlocker.process(clampf(safeFinite(externalNorm) + cablePickup, -4.f, 4.f),
                                       8.f, sampleRate);
    const float clean = ac * cleanScale;
    const float gain = cleanScale + driveCeiling * std::pow(strength, driveExponent);
    const float driven = ac * gain;
    const float asymmetry = mode == static_cast<int>(StrengthCalibration::MODULAR_HOT) ? 0.18f : 0.12f;
    const float biasedDrive = driven + asymmetry * strength * driven * driven;
    strengthNorm = clampf(lerpf(clean, softClip(biasedDrive), smoothstep(strength)), -1.f, 1.f);
    cv1Norm = follower.process(strengthNorm, attack, release, sampleRate);
    return strengthNorm;
  }
};

struct AgitationGenerator {
  float phase = 0.f;
  float outNorm = 0.f;
  bool previousGateHigh = false;

  void reset() {
    phase = 0.f;
    outNorm = 0.f;
    previousGateHigh = false;
  }

  float process(float sampleTime, bool beginEndConnected, float beginEndVolts,
                float speed, float angle, float speedCvVolts, float speedCvAmount) {
    sampleTime = clampf(safeFinite(sampleTime, 1.f / 48000.f), 1.f / 384000.f, 1.f / 1000.f);
    const float speedControl = clampf(safeFinite(speed) + safeFinite(speedCvAmount) * safeFinite(speedCvVolts) / 5.f,
                                      0.f, 1.f);
    const float hz = normalizedKnobToFrequency(speedControl, 1.f / 60.f, 1000.f);
    angle = clampf(safeFinite(angle), 0.f, 1.f);
    const float risePortion = lerpf(0.08f, 0.92f, angle);

    if (beginEndConnected) {
      const bool gateHigh = beginEndVolts >= 1.5f;
      if (!gateHigh) {
        previousGateHigh = false;
        phase = 0.f;
        outNorm = 0.f;
        return 0.f;
      }

      if (!previousGateHigh) {
        phase = 0.f;
      }
      previousGateHigh = true;

      const float riseSeconds = std::max(1.f / 1000.f, risePortion / hz);
      phase = clampf(phase + sampleTime / riseSeconds, 0.f, 1.f);
      outNorm = smoothstep(phase);
      return clampf(safeFinite(outNorm), 0.f, 1.f) * 6.f;
    }

    previousGateHigh = false;
    phase += hz * sampleTime;
    phase -= std::floor(phase);

    if (phase < risePortion) {
      outNorm = smoothstep(phase / std::max(0.001f, risePortion));
    }
    else {
      outNorm = 1.f - smoothstep((phase - risePortion) / std::max(0.001f, 1.f - risePortion));
    }
    outNorm = clampf(safeFinite(outNorm), 0.f, 1.f);
    return outNorm * 6.f;
  }
};

struct WyrdCoreSignals {
  float core = 0.f;
  float sub = 0.f;
  float selectedTone = 0.f;
  float activatedTone = 0.f;
  float coloredFeedback = 0.f;
  float activation = 0.f;
};

struct WyrdCoreProcessor {
  Wyrd_core_process_type context;

  WyrdCoreProcessor() {
    reset();
  }

  void reset() {
    Wyrd_core_process_init(context);
  }

  bool contextHealthy() const {
    return std::isfinite(context.phase) && std::isfinite(context.subPhase)
           && std::isfinite(context.lp1) && std::isfinite(context.lp2)
           && std::isfinite(context.hpState) && std::isfinite(context.tonesSmooth)
           && std::isfinite(context.activationSmooth) && std::isfinite(context.filterSmooth)
           && std::isfinite(context.absorbSmooth) && std::isfinite(context.decaySmooth)
           && std::fabs(context.lp1) <= 32.f && std::fabs(context.lp2) <= 32.f
           && std::fabs(context.hpState) <= 32.f
           && context.phase >= -0.01f && context.phase <= 1.01f
           && context.subPhase >= -0.01f && context.subPhase <= 1.01f;
  }

  void sanitizeContext() {
    context.phase -= std::floor(safeFinite(context.phase));
    context.subPhase -= std::floor(safeFinite(context.subPhase));
    context.lp1 = clampf(safeFinite(context.lp1), -8.f, 8.f);
    context.lp2 = clampf(safeFinite(context.lp2), -8.f, 8.f);
    context.hpState = clampf(safeFinite(context.hpState), -8.f, 8.f);
    context.tonesSmooth = clampf(safeFinite(context.tonesSmooth), 0.f, 1.f);
    context.activationSmooth = clampf(safeFinite(context.activationSmooth), -1.4f, 1.6f);
    context.filterSmooth = clampf(safeFinite(context.filterSmooth), 0.f, 1.f);
    context.absorbSmooth = clampf(safeFinite(context.absorbSmooth), 0.f, 1.f);
    context.decaySmooth = clampf(safeFinite(context.decaySmooth), 0.f, 1.f);
    context.initialized = context.initialized ? 1u : 0u;
  }

  WyrdCoreSignals process(float pitchVolts, float tonesControl, float activationConstant,
                          float activationCvNorm, float activationCvAmount,
                          float activationInterference, float feedbackNorm,
                          float feedbackAudio, float filterControl, float absorbControl,
                          float decayControl, float agitationNorm, float sampleRate) {
    if (!contextHealthy()) {
      reset();
    }

    Wyrd_core_process(context, safeFinite(pitchVolts), safeFinite(tonesControl),
                      safeFinite(activationConstant), safeFinite(activationCvNorm),
                      safeFinite(activationCvAmount), safeFinite(activationInterference),
                      safeFinite(feedbackNorm), safeFinite(feedbackAudio),
                      safeFinite(filterControl), safeFinite(absorbControl),
                      safeFinite(decayControl), safeFinite(agitationNorm),
                      clampf(safeFinite(sampleRate, 48000.f), 1000.f, 384000.f));

    if (!contextHealthy()) {
      reset();
      return {};
    }
    sanitizeContext();

    WyrdCoreSignals out;
    out.core = clampf(safeFinite(Wyrd_core_process_ret_0(context)), -1.f, 1.f);
    out.sub = clampf(safeFinite(Wyrd_core_process_ret_1(context)), -1.f, 1.f);
    out.selectedTone = clampf(safeFinite(Wyrd_core_process_ret_2(context)), -1.25f, 1.25f);
    out.activatedTone = clampf(safeFinite(Wyrd_core_process_ret_3(context)), -1.f, 1.f);
    out.coloredFeedback = clampf(safeFinite(Wyrd_core_process_ret_4(context)), -1.f, 1.f);
    out.activation = clampf(safeFinite(Wyrd_core_process_ret_5(context)), -1.f, 1.f);
    return out;
  }
};

struct TimeFilterResult {
  float wet = 0.f;
  float feedback = 0.f;
  float cv2Volts = 0.f;
};

struct TimeFilterExperiment {
  static constexpr float kMaxDelaySeconds = 2.75f;
  static constexpr float kMaxSampleRate = 384000.f;
  static constexpr int kMaxDelaySamples = static_cast<int>(kMaxDelaySeconds * kMaxSampleRate) + 8;

  std::vector<float> buffer;
  int writeIndex = 0;
  float feedbackState = 0.f;
  float lastWet = 0.f;
  float wowPhase = 0.f;
  std::uint32_t rng = 0x8373ca5bu;
  ControlSmoother delaySmoother;
  OnePoleLowpass cv2Filter;
  OnePoleLowpass feedbackFilter;
  OnePoleLowpass wetFilter;
  OnePoleLowpass ageFilter;
  DelayAllpass diffuserA;
  DelayAllpass diffuserB;
  DelayAllpass diffuserC;

  TimeFilterExperiment()
      : buffer(kMaxDelaySamples, 0.f),
        diffuserA(32768),
        diffuserB(24576),
        diffuserC(16384) {
  }

  void reset() {
    std::fill(buffer.begin(), buffer.end(), 0.f);
    writeIndex = 0;
    feedbackState = 0.f;
    lastWet = 0.f;
    wowPhase = 0.f;
    rng = 0x8373ca5bu;
    delaySmoother.reset();
    cv2Filter.reset();
    feedbackFilter.reset();
    wetFilter.reset();
    ageFilter.reset();
    diffuserA.reset();
    diffuserB.reset();
    diffuserC.reset();
  }

  float feedbackNorm() const {
    return clampf(safeFinite(feedbackState), -1.f, 1.f);
  }

  float nextNoise() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (static_cast<float>(rng & 0x00ffffffu) / static_cast<float>(0x007fffffu)) - 1.f;
  }

  float read(float delaySamples, int activeSize) const {
    activeSize = std::max(8, std::min(activeSize, static_cast<int>(buffer.size())));
    delaySamples = clampf(safeFinite(delaySamples), 1.f, static_cast<float>(activeSize - 4));
    float readPos = static_cast<float>(writeIndex) - delaySamples;
    while (readPos < 0.f) {
      readPos += static_cast<float>(activeSize);
    }
    while (readPos >= static_cast<float>(activeSize)) {
      readPos -= static_cast<float>(activeSize);
    }

    const int i0 = static_cast<int>(readPos);
    const int im1 = (i0 - 1 + activeSize) % activeSize;
    const int i1 = (i0 + 1) % activeSize;
    const int i2 = (i0 + 2) % activeSize;
    const float frac = readPos - static_cast<float>(i0);

    const float y0 = buffer[i0];
    const float ym1 = buffer[im1];
    const float y1 = buffer[i1];
    const float y2 = buffer[i2];
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return safeFinite(((c3 * frac + c2) * frac + c1) * frac + c0);
  }

  TimeFilterResult process(float input, float coloredFeedback, float timeControl,
                           float timeModNorm, float decayControl, float filterControl,
                           float absorbControl, float sampleRate) {
    sampleRate = clampf(safeFinite(sampleRate, 48000.f), 1000.f, kMaxSampleRate);
    const int activeSize = std::min(kMaxDelaySamples, static_cast<int>(sampleRate * kMaxDelaySeconds));
    if (writeIndex >= activeSize) {
      writeIndex = 0;
    }

    timeControl = clampf(safeFinite(timeControl), 0.f, 1.f);
    timeModNorm = clampf(safeFinite(timeModNorm), -1.f, 1.f);
    decayControl = clampf(safeFinite(decayControl), 0.f, 1.f);
    filterControl = clampf(safeFinite(filterControl), 0.f, 1.f);
    absorbControl = clampf(safeFinite(absorbControl), 0.f, 1.f);

    const float baseSeconds = normalizedKnobToFrequency(timeControl, 0.005f, 2.45f);
    const float wowDepth = 0.0009f + 0.0042f * decayControl * (1.f - 0.55f * absorbControl);
    wowPhase += (0.05f + 1.8f * timeControl) / sampleRate;
    wowPhase -= std::floor(wowPhase);
    const float wow = std::sin(2.f * kPi * wowPhase) * wowDepth;
    const float modulatedSeconds = clampf(baseSeconds * std::pow(2.f, timeModNorm * 0.42f) + wow,
                                          0.003f, kMaxDelaySeconds - 0.01f);
    const float targetDelaySamples = clampf(modulatedSeconds * sampleRate, 4.f,
                                            static_cast<float>(activeSize - 4));
    const float delaySamples = delaySmoother.process(targetDelaySamples, 0.012f, sampleRate);

    const float tapA = read(delaySamples * 0.37f, activeSize);
    const float tapB = read(delaySamples * 0.50f, activeSize);
    const float tapC = read(delaySamples * 0.73f, activeSize);
    const float tapD = read(delaySamples * 0.91f, activeSize);
    const float tapE = read(delaySamples, activeSize);
    const float tapMix = clampf(0.22f * tapA - 0.18f * tapB + 0.32f * tapC
                                + 0.29f * tapD + 0.46f * tapE,
                                -1.8f, 1.8f);

    const float filterPass = smoothstep(filterControl);
    const float filterHz = normalizedKnobToFrequency(filterControl, 18.f, 18500.f);
    const float dampedInput = tapMix + 0.21f * coloredFeedback + 0.09f * feedbackState;
    const float agedTap = ageFilter.process(dampedInput, lerpf(380.f, 14500.f, absorbControl), sampleRate);
    const float filteredTap = feedbackFilter.process(agedTap, filterHz, sampleRate)
                              * (0.004f + 0.996f * filterPass);
    const float allpassFeedback = 0.34f + 0.28f * smoothstep(decayControl);
    float diffusedTap = diffuserA.process(filteredTap + 0.12f * tapD,
                                          sampleRate * lerpf(0.0047f, 0.031f, 1.f - absorbControl),
                                          allpassFeedback);
    diffusedTap = diffuserB.process(diffusedTap - 0.08f * tapA,
                                    sampleRate * lerpf(0.0075f, 0.047f, filterControl),
                                    -0.42f + 0.16f * absorbControl);
    diffusedTap = diffuserC.process(diffusedTap + 0.06f * coloredFeedback,
                                    sampleRate * lerpf(0.0029f, 0.021f, decayControl),
                                    0.27f + 0.18f * filterControl);

    const float presentedTap = wetFilter.process(tapMix + 0.42f * diffusedTap, filterHz, sampleRate)
                               * lerpf(0.18f, 1.f, absorbControl);
    const float feedbackGain = 0.05f + 1.08f * smoothstep(decayControl);
    const float persistence = 0.12f + 0.88f * absorbControl;
    const float noise = nextNoise() * (0.000015f + 0.00042f * decayControl * decayControl) * (1.45f - absorbControl);
    const float feedbackWrite = diffusedTap * feedbackGain * persistence
                                + filteredTap * feedbackGain * persistence * 0.18f
                                + coloredFeedback * feedbackGain * persistence * 0.22f;
    const float asymFeedback = feedbackWrite + 0.09f * decayControl * feedbackWrite * feedbackWrite
                               - 0.035f * (1.f - absorbControl) * feedbackWrite * feedbackWrite * feedbackWrite;
    const float writeSample = clampf(softClip(input + asymFeedback + noise), -1.2f, 1.2f);

    buffer[writeIndex] = safeFinite(writeSample);
    writeIndex++;
    if (writeIndex >= activeSize) {
      writeIndex = 0;
    }

    feedbackState = clampf(softClip(diffusedTap * 0.92f + filteredTap * 0.34f + coloredFeedback * 0.2f),
                           -1.f, 1.f);
    lastWet = clampf(softClip(tapA * 0.10f + tapB * 0.15f + tapC * 0.22f
                              + tapD * 0.18f + tapE * 0.28f
                              + presentedTap * 0.68f + diffusedTap * 0.22f
                              + coloredFeedback * 0.16f),
                     -1.f, 1.f);

    TimeFilterResult result;
    result.wet = lastWet;
    result.feedback = feedbackState;
    result.cv2Volts = clampf(cv2Filter.process(feedbackState, 7.5f, sampleRate) * 5.f, -5.f, 5.f);
    return result;
  }
};

struct Frame {
  float sampleTime = 1.f / 48000.f;

  float externalVolts = 0.f;
  float beginEndVolts = 0.f;
  float speedCvVolts = 0.f;
  float activationCvVolts = 0.f;
  float tonicModVolts = 0.f;
  float tonesCvVolts = 0.f;
  float vOctVolts = 0.f;
  float timeModVolts = 0.f;
  float timeCvVolts = 0.f;
  float timeUnityCvVolts = 0.f;
  float decayCvVolts = 0.f;
  float blendCvVolts = 0.f;
  float filterCvVolts = 0.f;
  float absorbCvVolts = 0.f;

  bool externalConnected = false;
  bool beginEndConnected = false;
  bool tonicModConnected = false;
  bool tonesCvConnected = false;
  bool timeModConnected = false;
  bool timeCvConnected = false;
  bool timeUnityCvConnected = false;
  bool decayCvConnected = false;
  bool blendCvConnected = false;
  bool filterCvConnected = false;
  bool absorbCvConnected = false;
};

struct Controls {
  float strength = 0.f;
  float externalConstant = 0.f;
  float activationConstant = 0.15f;
  float activationInterference = 0.f;
  float activationCvAmount = 0.f;
  float tonicCoarse = 0.34f;
  float tonicFine = 0.f;
  float tonicModAmount = 0.f;
  float tones = 0.25f;
  float tonesCvAmount = 0.f;
  float timeCoarse = 0.42f;
  float timeFine = 0.f;
  float timeModAmount = 0.f;
  float timeCvAmount = 0.f;
  float decay = 0.28f;
  float decayCvAmount = 0.f;
  float filter = 0.65f;
  float filterCvAmount = 0.f;
  float absorb = 0.65f;
  float blend = 0.45f;
  float level = 0.55f;
  float speed = 0.32f;
  float angle = 0.5f;
  float speedCvAmount = 0.f;
  float touchActivation = 0.f;
  float touchTime = 0.f;
  float touchFilter = 0.f;
  float touchAbsorb = 0.f;
  float touchTonic = 0.f;
  float touchDecay = 0.f;
  int touchSource = static_cast<int>(TouchSource::MANUAL);
  int strengthCalibration = static_cast<int>(StrengthCalibration::STREGA);
};

struct Outputs {
  float strengthVolts = 0.f;
  float cv1Volts = 0.f;
  float cv2Volts = 0.f;
  float agitationVolts = 0.f;
  float toneCoreVolts = 0.f;
  float subHarmonicsVolts = 0.f;
  float selectedToneVolts = 0.f;
  float activation = 0.f;
  float modularVolts = 0.f;
  float lineVolts = 0.f;
};

struct WyrdEngine {
  StrengthProcessor strength;
  AgitationGenerator agitation;
  WyrdCoreProcessor core;
  TimeFilterExperiment timeFilter;
  DcBlocker tonicModAcCoupler;
  DcBlocker timeModAcCoupler;
  Outputs last;
  std::uint32_t touchRng = 0x4d595244u;

  void reset() {
    strength.reset();
    agitation.reset();
    core.reset();
    timeFilter.reset();
    tonicModAcCoupler.reset();
    timeModAcCoupler.reset();
    last = Outputs{};
    touchRng = 0x4d595244u;
  }

  float nextTouchNoise() {
    touchRng ^= touchRng << 13;
    touchRng ^= touchRng >> 17;
    touchRng ^= touchRng << 5;
    return (static_cast<float>(touchRng & 0x00ffffffu) / static_cast<float>(0x007fffffu)) - 1.f;
  }

  float touchSourceValue(const Controls& controls, float strengthNorm,
                         float cv1Norm, float agitationNorm) {
    const int source = static_cast<int>(clampf(static_cast<float>(controls.touchSource), 0.f,
                                               static_cast<float>(kNumTouchSources - 1)));
    switch (static_cast<TouchSource>(source)) {
      case TouchSource::CV1:
        return clampf(cv1Norm * 2.f - 1.f, -1.f, 1.f);
      case TouchSource::CV2:
        return timeFilter.feedbackNorm();
      case TouchSource::AGITATION:
        return clampf(agitationNorm * 2.f - 1.f, -1.f, 1.f);
      case TouchSource::SUB_HARMONICS:
        return clampf(last.subHarmonicsVolts / 5.f, -1.f, 1.f);
      case TouchSource::STRENGTH:
        return clampf(strengthNorm, -1.f, 1.f);
      case TouchSource::NOISE:
        return nextTouchNoise();
      case TouchSource::MANUAL:
      case TouchSource::NUM_SOURCES:
      default:
        return 1.f;
    }
  }

  Outputs process(const Frame& frame, const Controls& controls) {
    const float sampleTime = clampf(safeFinite(frame.sampleTime, 1.f / 48000.f), 1.f / 384000.f, 1.f / 1000.f);
    const float sampleRate = 1.f / sampleTime;

    const float externalNorm = frame.externalConnected ? frame.externalVolts / 5.f : 0.f;
    const int strengthMode = static_cast<int>(clampf(static_cast<float>(controls.strengthCalibration), 0.f,
                                                     static_cast<float>(kNumStrengthCalibrations - 1)));
    const float strengthNorm = strength.process(externalNorm, controls.strength, sampleRate,
                                                static_cast<StrengthCalibration>(strengthMode),
                                                frame.externalConnected);
    const float externalInjection = strengthNorm * clampf(safeFinite(controls.externalConstant), 0.f, 1.f);

    const float agitationVolts = agitation.process(sampleTime, frame.beginEndConnected, frame.beginEndVolts,
                                                   controls.speed, controls.angle, frame.speedCvVolts,
                                                   controls.speedCvAmount);
    const float agitationInternalNorm = clampf(agitation.outNorm * (8.f / 6.f), 0.f, 1.3333334f);
    const float touchSource = touchSourceValue(controls, strengthNorm, strength.cv1Norm, agitationInternalNorm);
    const float touchPositive = clampf(0.5f + 0.5f * touchSource, 0.f, 1.f);
    const float touchEnergy = clampf(std::fabs(touchSource), 0.f, 1.f);

    const float baseHz = normalizedKnobToFrequency(controls.tonicCoarse, 20.f, 10000.f);
    float pitchVolts = frequencyToPitchVolts(baseHz) + safeFinite(controls.tonicFine) + safeFinite(frame.vOctVolts);
    const float touchTonic = clampf(safeFinite(controls.touchTonic), 0.f, 1.f);
    const float tonicModSource = frame.tonicModConnected ? frame.tonicModVolts : timeFilter.feedbackNorm() * 5.f;
    const float tonicModNorm = tonicModAcCoupler.process(tonicModSource / 5.f, 5.f, sampleRate);
    pitchVolts += clampf(safeFinite(controls.tonicModAmount), 0.f, 1.f) * tonicModNorm * 1.35f;
    pitchVolts += touchTonic * (touchSource * 0.36f + timeFilter.feedbackNorm() * 0.3f
                                + (agitationInternalNorm - 0.5f) * 0.18f);

    const float tonesCv = frame.tonesCvConnected ? frame.tonesCvVolts : 0.f;
    const float tones = clampf(safeFinite(controls.tones) + safeFinite(controls.tonesCvAmount) * tonesCv / 10.f,
                               0.f, 1.f);

    float filter = clampf(safeFinite(controls.filter)
                          + safeFinite(controls.filterCvAmount) * (frame.filterCvConnected ? frame.filterCvVolts : 0.f) / 10.f
                          + 0.28f * clampf(safeFinite(controls.touchFilter), 0.f, 1.f) * touchPositive,
                          0.f, 1.f);
    float absorb = controls.absorb;
    if (frame.absorbCvConnected) {
      absorb = safeFinite(controls.absorb) * clampf(frame.absorbCvVolts / 10.f, 0.f, 1.f);
    }
    absorb -= 0.58f * clampf(safeFinite(controls.touchAbsorb), 0.f, 1.f) * (0.35f + 0.65f * touchEnergy);
    absorb = clampf(safeFinite(absorb), 0.f, 1.f);

    float decay = safeFinite(controls.decay);
    if (frame.decayCvConnected) {
      decay += safeFinite(controls.decayCvAmount) * frame.decayCvVolts / 10.f;
    }
    decay += 0.43f * clampf(safeFinite(controls.touchDecay), 0.f, 1.f) * (0.4f + 0.6f * touchPositive);
    decay = clampf(decay, 0.f, 1.f);

    const float activationConstant = clampf(safeFinite(controls.activationConstant)
                                            + 0.72f * clampf(safeFinite(controls.touchActivation), 0.f, 1.f)
                                              * (0.35f + 0.65f * touchPositive),
                                            -0.35f, 1.35f);

    const WyrdCoreSignals coreOut = core.process(pitchVolts, tones, activationConstant,
                                                 frame.activationCvVolts / 5.f,
                                                 safeFinite(controls.activationCvAmount),
                                                 clampf(safeFinite(controls.activationInterference), 0.f, 1.f),
                                                 timeFilter.feedbackNorm(),
                                                 timeFilter.feedbackNorm(),
                                                 filter, absorb, decay, agitationInternalNorm, sampleRate);

    float time = safeFinite(controls.timeCoarse) + safeFinite(controls.timeFine);
    if (frame.timeCvConnected) {
      time += safeFinite(controls.timeCvAmount) * frame.timeCvVolts / 10.f * 0.5f;
    }
    if (frame.timeUnityCvConnected) {
      time += frame.timeUnityCvVolts / 10.f;
    }
    time += 0.24f * clampf(safeFinite(controls.touchTime), 0.f, 1.f) * touchSource;

    const float timeModSource = frame.timeModConnected ? frame.timeModVolts : coreOut.sub * 5.f;
    const float timeModNorm = timeModAcCoupler.process(timeModSource / 5.f, 6.f, sampleRate)
                              * clampf(safeFinite(controls.timeModAmount), 0.f, 1.f);
    time = clampf(time, 0.f, 1.f);

    const float dry = clampf(externalInjection + coreOut.activatedTone, -1.5f, 1.5f);
    const TimeFilterResult timeOut = timeFilter.process(dry, coreOut.coloredFeedback, time,
                                                        timeModNorm, decay, filter, absorb, sampleRate);

    float blend = controls.blend;
    if (frame.blendCvConnected) {
      blend = safeFinite(controls.blend) * clampf(frame.blendCvVolts / 5.f, 0.f, 1.f);
    }
    blend = clampf(safeFinite(blend), 0.f, 1.f);

    const float resultNorm = equalPowerFade(dry, timeOut.wet, blend);
    const float level = clampf(safeFinite(controls.level), 0.f, 1.f);
    const float levelGain = level * (0.75f + 1.8f * level);

    last.strengthVolts = strengthNorm * 10.f;
    last.cv1Volts = strength.cv1Norm * 10.f;
    last.cv2Volts = timeOut.cv2Volts;
    last.agitationVolts = agitationVolts;
    last.toneCoreVolts = coreOut.core * 5.f;
    last.subHarmonicsVolts = coreOut.sub * 5.f;
    last.selectedToneVolts = coreOut.selectedTone * 5.f;
    last.activation = coreOut.activation;
    last.modularVolts = softClip(resultNorm * levelGain) * 5.f;
    last.lineVolts = softClip(resultNorm * levelGain) * 1.5f;

    float* values[] = {
      &last.strengthVolts, &last.cv1Volts, &last.cv2Volts, &last.agitationVolts,
      &last.toneCoreVolts, &last.subHarmonicsVolts, &last.selectedToneVolts,
      &last.activation, &last.modularVolts, &last.lineVolts
    };
    for (float* v : values) {
      *v = safeFinite(*v);
    }
    last.cv1Volts = clampf(last.cv1Volts, 0.f, 10.f);
    last.cv2Volts = clampf(last.cv2Volts, -5.f, 5.f);
    last.agitationVolts = clampf(last.agitationVolts, 0.f, 6.f);
    last.modularVolts = clampf(last.modularVolts, -5.2f, 5.2f);
    last.lineVolts = clampf(last.lineVolts, -1.6f, 1.6f);
    return last;
  }
};

} // namespace wyrd
} // namespace shortwav
