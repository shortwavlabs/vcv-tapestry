#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "dsp/wyrd-dsp.h"

namespace TestSuite
{
  struct TestContext
  {
    int passed = 0;
    int failed = 0;

    void assertTrue(bool cond, const char *name, const char *file, int line)
    {
      if (cond)
      {
        ++passed;
      }
      else
      {
        ++failed;
        std::printf("[FAIL] %s (%s:%d)\n", name, file, line);
      }
    }

    void assertNear(float actual, float expected, float tol,
                    const char *name, const char *file, int line)
    {
      const float diff = std::fabs(actual - expected);
      if (diff <= tol)
      {
        ++passed;
      }
      else
      {
        ++failed;
        std::printf("[FAIL] %s: expected=%g actual=%g tol=%g (%s:%d)\n",
                    name, (double)expected, (double)actual, (double)tol, file, line);
      }
    }

    void summary() const
    {
      std::printf("[TEST SUMMARY] passed=%d failed=%d\n", passed, failed);
    }
  };

  #define T_ASSERT(ctx, cond) (ctx).assertTrue((cond), #cond, __FILE__, __LINE__)
  #define T_ASSERT_NEAR(ctx, actual, expected, tol) \
    (ctx).assertNear((actual), (expected), (tol), #actual " ~= " #expected, __FILE__, __LINE__)

  using namespace shortwav::wyrd;

  static float spectralCentroid(const std::vector<float>& samples, float sampleRate)
  {
    const int n = static_cast<int>(samples.size());
    float weighted = 0.f;
    float total = 0.f;
    for (int bin = 1; bin <= 96; ++bin)
    {
      float re = 0.f;
      float im = 0.f;
      const float w = 2.f * kPi * static_cast<float>(bin) / static_cast<float>(n);
      for (int i = 0; i < n; ++i)
      {
        re += samples[static_cast<std::size_t>(i)] * std::cos(w * static_cast<float>(i));
        im -= samples[static_cast<std::size_t>(i)] * std::sin(w * static_cast<float>(i));
      }
      const float mag = std::sqrt(re * re + im * im);
      const float hz = static_cast<float>(bin) * sampleRate / static_cast<float>(n);
      weighted += hz * mag;
      total += mag;
    }
    return total > 1e-8f ? weighted / total : 0.f;
  }

  static Controls defaultControls()
  {
    Controls controls;
    controls.strength = 0.35f;
    controls.externalConstant = 0.2f;
    controls.activationConstant = 0.25f;
    controls.activationInterference = 0.15f;
    controls.tonicCoarse = 0.36f;
    controls.tones = 0.35f;
    controls.timeCoarse = 0.18f;
    controls.timeModAmount = 0.3f;
    controls.decay = 0.45f;
    controls.filter = 0.7f;
    controls.absorb = 0.7f;
    controls.blend = 0.65f;
    controls.level = 0.85f;
    controls.speed = 0.32f;
    controls.angle = 0.5f;
    return controls;
  }

  static void test_strength_and_cv1_ranges(TestContext &ctx)
  {
    StrengthProcessor low;
    StrengthProcessor high;
    const float sampleRate = 48000.f;
    float lowAbs = 0.f;
    float highAbs = 0.f;
    float highCv = 0.f;

    for (int i = 0; i < 48000; ++i)
    {
      const float x = 0.16f * std::sin(2.f * kPi * 220.f * static_cast<float>(i) / sampleRate);
      const float lowOut = low.process(x, 0.f, sampleRate);
      const float highOut = high.process(x, 1.f, sampleRate);
      lowAbs += std::fabs(lowOut);
      highAbs += std::fabs(highOut);
      highCv = std::max(highCv, high.cv1Norm);

      T_ASSERT(ctx, std::isfinite(lowOut));
      T_ASSERT(ctx, std::isfinite(highOut));
      T_ASSERT(ctx, std::fabs(highOut) <= 1.001f);
      T_ASSERT(ctx, high.cv1Norm >= -1e-6f && high.cv1Norm <= 1.001f);
    }

    T_ASSERT(ctx, highAbs > lowAbs * 3.f);
    T_ASSERT(ctx, highCv > 0.2f);

    StrengthProcessor unity;
    const float unityOut = unity.process(0.25f, 0.f, sampleRate) * 10.f;
    T_ASSERT_NEAR(ctx, unityOut, 1.25f, 0.01f);
  }

  static void test_strength_calibration_modes(TestContext &ctx)
  {
    StrengthProcessor gentle;
    StrengthProcessor strega;
    StrengthProcessor hot;
    const float sampleRate = 48000.f;
    float gentlePeak = 0.f;
    float stregaPeak = 0.f;
    float hotPeak = 0.f;
    float gentleCv = 0.f;
    float hotCv = 0.f;

    for (int i = 0; i < 48000; ++i)
    {
      const float x = 0.1f + 0.22f * std::sin(2.f * kPi * 110.f * static_cast<float>(i) / sampleRate);
      const float g = gentle.process(x, 0.48f, sampleRate, StrengthCalibration::LINE_GENTLE);
      const float s = strega.process(x, 0.48f, sampleRate, StrengthCalibration::STREGA);
      const float h = hot.process(x, 0.48f, sampleRate, StrengthCalibration::MODULAR_HOT);
      gentlePeak = std::max(gentlePeak, std::fabs(g));
      stregaPeak = std::max(stregaPeak, std::fabs(s));
      hotPeak = std::max(hotPeak, std::fabs(h));
      gentleCv = std::max(gentleCv, gentle.cv1Norm);
      hotCv = std::max(hotCv, hot.cv1Norm);
      T_ASSERT(ctx, std::isfinite(g));
      T_ASSERT(ctx, std::isfinite(s));
      T_ASSERT(ctx, std::isfinite(h));
    }

    T_ASSERT(ctx, stregaPeak > gentlePeak * 1.12f);
    T_ASSERT(ctx, hotPeak > stregaPeak * 1.02f);
    T_ASSERT(ctx, hotCv > gentleCv * 1.05f);
  }

  static void test_external_dummy_cable_pickup(TestContext &ctx)
  {
    StrengthProcessor unpatched;
    StrengthProcessor patched;
    const float sampleRate = 48000.f;
    float unpatchedEnergy = 0.f;
    float patchedEnergy = 0.f;
    float patchedCv = 0.f;

    for (int i = 0; i < 48000; ++i)
    {
      const float silent = unpatched.process(0.f, 0.86f, sampleRate,
                                             StrengthCalibration::STREGA, false);
      const float pickup = patched.process(0.f, 0.86f, sampleRate,
                                           StrengthCalibration::STREGA, true);
      if (i > 4096)
      {
        unpatchedEnergy += std::fabs(silent);
        patchedEnergy += std::fabs(pickup);
      }
      patchedCv = std::max(patchedCv, patched.cv1Norm);
      T_ASSERT(ctx, std::isfinite(silent));
      T_ASSERT(ctx, std::isfinite(pickup));
    }

    T_ASSERT(ctx, unpatchedEnergy < 0.0001f);
    T_ASSERT(ctx, patchedEnergy > 0.8f);
    T_ASSERT(ctx, patchedCv > 0.002f);
  }

  static int countRisingZeroCrossings(WyrdCoreProcessor& core, float pitchVolts)
  {
    const float sampleRate = 48000.f;
    int crossings = 0;
    float previous = 0.f;
    for (int i = 0; i < 48000; ++i)
    {
      const WyrdCoreSignals out = core.process(pitchVolts, 0.2f, 0.2f, 0.f, 0.f, 0.f,
                                               0.f, 0.f, 0.8f, 0.7f, 0.2f, 0.f,
                                               sampleRate);
      if (previous < 0.f && out.core >= 0.f)
      {
        ++crossings;
      }
      previous = out.core;
    }
    return crossings;
  }

  static void test_vult_core_pitch_and_tone_palette(TestContext &ctx)
  {
    WyrdCoreProcessor lowCore;
    WyrdCoreProcessor highCore;
    const int lowCrossings = countRisingZeroCrossings(lowCore, 0.f);
    const int highCrossings = countRisingZeroCrossings(highCore, 1.f);
    T_ASSERT(ctx, lowCrossings > 200 && lowCrossings < 320);
    T_ASSERT(ctx, highCrossings > lowCrossings * 18 / 10);

    WyrdCoreProcessor dark;
    WyrdCoreProcessor bright;
    float darkAbs = 0.f;
    float brightAbs = 0.f;
    float subMax = 0.f;
    for (int i = 0; i < 2048; ++i)
    {
      const WyrdCoreSignals d = dark.process(0.f, 0.05f, 0.6f, 0.f, 0.f, 0.f,
                                             0.f, 0.f, 0.7f, 0.7f, 0.4f, 0.f,
                                             48000.f);
      const WyrdCoreSignals b = bright.process(0.f, 1.f, 0.6f, 0.f, 0.f, 0.f,
                                               0.f, 0.f, 0.7f, 0.7f, 0.4f, 0.f,
                                               48000.f);
      darkAbs += std::fabs(d.selectedTone);
      brightAbs += std::fabs(b.selectedTone);
      subMax = std::max(subMax, std::fabs(b.sub));
      T_ASSERT(ctx, std::isfinite(d.core));
      T_ASSERT(ctx, std::isfinite(b.activatedTone));
      T_ASSERT(ctx, std::fabs(d.core) <= 1.001f);
      T_ASSERT(ctx, std::fabs(b.sub) <= 1.001f);
    }

    T_ASSERT(ctx, std::fabs(brightAbs - darkAbs) > 1.f);
    T_ASSERT(ctx, subMax > 0.5f);

    WyrdCoreProcessor positiveCv;
    WyrdCoreProcessor negativeCv;
    const WyrdCoreSignals pos = positiveCv.process(0.f, 0.45f, 0.f, 1.f, 1.f, 0.f,
                                                   0.f, 0.f, 0.7f, 0.7f, 0.4f, 0.f,
                                                   48000.f);
    const WyrdCoreSignals neg = negativeCv.process(0.f, 0.45f, 0.f, -1.f, 1.f, 0.f,
                                                   0.f, 0.f, 0.7f, 0.7f, 0.4f, 0.f,
                                                   48000.f);
    T_ASSERT(ctx, pos.activation > 0.5f);
    T_ASSERT(ctx, neg.activation < -0.5f);
    T_ASSERT(ctx, pos.activatedTone * neg.activatedTone < 0.f);

    WyrdCoreProcessor unactivated;
    float silentTone = 0.f;
    for (int i = 0; i < 2048; ++i)
    {
      const WyrdCoreSignals out = unactivated.process(0.f, 0.45f, 0.f, 0.f, 0.f, 0.f,
                                                      0.f, 0.f, 0.7f, 0.7f, 0.4f, 0.f,
                                                      48000.f);
      silentTone += std::fabs(out.activatedTone);
    }
    T_ASSERT(ctx, silentTone < 1e-4f);

    WyrdCoreProcessor aliasTamed;
    float highMax = 0.f;
    for (int i = 0; i < 4096; ++i)
    {
      const WyrdCoreSignals out = aliasTamed.process(5.f, 1.f, 0.65f, 0.f, 0.f, 0.f,
                                                     0.f, 0.f, 0.95f, 0.7f, 0.4f, 0.f,
                                                     48000.f);
      highMax = std::max(highMax, std::fabs(out.selectedTone));
      T_ASSERT(ctx, std::isfinite(out.selectedTone));
      T_ASSERT(ctx, std::fabs(out.selectedTone) <= 1.251f);
    }
    T_ASSERT(ctx, highMax > 0.25f);
  }

  static void test_agitation_normals_and_range(TestContext &ctx)
  {
    AgitationGenerator agitation;
    float maxOut = 0.f;
    float minOut = 10.f;
    for (int i = 0; i < 72000; ++i)
    {
      const float out = agitation.process(1.f / 48000.f, false, 0.f, 0.42f, 0.25f, 0.f, 0.f);
      maxOut = std::max(maxOut, out);
      minOut = std::min(minOut, out);
      T_ASSERT(ctx, std::isfinite(out));
      T_ASSERT(ctx, out >= -1e-6f && out <= 6.001f);
    }

    T_ASSERT(ctx, maxOut > 5.f);
    T_ASSERT(ctx, minOut < 1.f);
    T_ASSERT(ctx, agitation.outNorm > 0.f && agitation.outNorm <= 1.001f);
    T_ASSERT_NEAR(ctx, agitation.process(1.f / 48000.f, true, 0.f, 0.42f, 0.25f, 0.f, 0.f), 0.f, 1e-6f);

    AgitationGenerator gated;
    float held = 0.f;
    for (int i = 0; i < 96000; ++i)
    {
      held = gated.process(1.f / 48000.f, true, 8.f, 0.32f, 0.5f, 0.f, 0.f);
      T_ASSERT(ctx, held >= -1e-6f && held <= 6.001f);
    }
    T_ASSERT(ctx, held > 5.9f);
    T_ASSERT_NEAR(ctx, gated.process(1.f / 48000.f, true, 0.f, 0.32f, 0.5f, 0.f, 0.f), 0.f, 1e-6f);
  }

  static void test_time_filter_impulse_and_feedback(TestContext &ctx)
  {
    TimeFilterExperiment delay;
    const float sampleRate = 48000.f;
    float maxWet = 0.f;
    float maxCv2 = 0.f;

    for (int i = 0; i < 24000; ++i)
    {
      const float input = i == 0 ? 1.f : 0.f;
      const float colored = delay.feedbackNorm();
      const TimeFilterResult out = delay.process(input, colored, 0.f, 0.f, 0.55f, 0.7f, 0.7f, sampleRate);
      maxWet = std::max(maxWet, std::fabs(out.wet));
      maxCv2 = std::max(maxCv2, std::fabs(out.cv2Volts));
      T_ASSERT(ctx, std::isfinite(out.wet));
      T_ASSERT(ctx, std::isfinite(out.feedback));
      T_ASSERT(ctx, std::isfinite(out.cv2Volts));
      T_ASSERT(ctx, std::fabs(out.cv2Volts) <= 5.001f);
    }

    T_ASSERT(ctx, maxWet > 0.2f);
    T_ASSERT(ctx, maxCv2 > 0.002f);

    TimeFilterExperiment hot;
    for (int i = 0; i < 120000; ++i)
    {
      const float input = i == 0 ? 1.f : 0.f;
      const TimeFilterResult out = hot.process(input, hot.feedbackNorm(), 0.08f, 0.8f, 1.f, 1.f, 1.f, sampleRate);
      T_ASSERT(ctx, std::isfinite(out.wet));
      T_ASSERT(ctx, std::fabs(out.feedback) <= 1.001f);
    }

    TimeFilterExperiment closedFilter;
    TimeFilterExperiment openFilter;
    float closedTail = 0.f;
    float openTail = 0.f;
    for (int i = 0; i < 96000; ++i)
    {
      const float impulse = i == 0 ? 1.f : 0.f;
      const TimeFilterResult closed = closedFilter.process(impulse, closedFilter.feedbackNorm(),
                                                           0.02f, 0.f, 0.85f, 0.f, 0.9f, sampleRate);
      const TimeFilterResult open = openFilter.process(impulse, openFilter.feedbackNorm(),
                                                       0.02f, 0.f, 0.85f, 1.f, 0.9f, sampleRate);
      if (i > 8000)
      {
        closedTail += std::fabs(closed.feedback);
        openTail += std::fabs(open.feedback);
      }
    }
    T_ASSERT(ctx, openTail > closedTail * 1.35f);
  }

  static void test_time_filter_v2_tail_density(TestContext &ctx)
  {
    TimeFilterExperiment delay;
    const float sampleRate = 48000.f;
    int activeTailSamples = 0;
    int signChanges = 0;
    float previous = 0.f;
    float tailEnergy = 0.f;

    for (int i = 0; i < 144000; ++i)
    {
      const float impulse = i == 0 ? 1.f : 0.f;
      const TimeFilterResult out = delay.process(impulse, delay.feedbackNorm(),
                                                 0.035f, 0.17f, 0.92f, 0.62f, 0.86f, sampleRate);
      if (i > 9000)
      {
        const float wet = out.wet;
        tailEnergy += std::fabs(wet);
        if (std::fabs(wet) > 0.0004f)
        {
          ++activeTailSamples;
        }
        if ((previous < 0.f && wet >= 0.f) || (previous > 0.f && wet <= 0.f))
        {
          ++signChanges;
        }
        previous = wet;
      }
      T_ASSERT(ctx, std::isfinite(out.wet));
      T_ASSERT(ctx, std::fabs(out.feedback) <= 1.001f);
    }

    T_ASSERT(ctx, tailEnergy > 8.f);
    T_ASSERT(ctx, activeTailSamples > 1800);
    T_ASSERT(ctx, signChanges > 80);
  }

  static void test_raw_feedback_tonic_normal(TestContext &ctx)
  {
    Controls controls = defaultControls();
    controls.tonicModAmount = 1.f;
    controls.level = 0.f;
    controls.activationConstant = 0.f;
    controls.activationInterference = 0.f;

    Frame frame;
    frame.sampleTime = 1.f / 48000.f;

    WyrdEngine clean;
    WyrdEngine interfered;
    interfered.timeFilter.feedbackState = 1.f;

    float diff = 0.f;
    for (int i = 0; i < 512; ++i)
    {
      const Outputs a = clean.process(frame, controls);
      const Outputs b = interfered.process(frame, controls);
      diff += std::fabs(a.toneCoreVolts - b.toneCoreVolts);
    }
    T_ASSERT(ctx, diff > 0.1f);
  }

  static Outputs runEngine(WyrdEngine& engine, Controls controls, Frame frame, int samples)
  {
    Outputs out;
    for (int i = 0; i < samples; ++i)
    {
      out = engine.process(frame, controls);
    }
    return out;
  }

  static void test_engine_manual_ranges_and_normals(TestContext &ctx)
  {
    WyrdEngine engine;
    Controls controls = defaultControls();
    Frame frame;
    frame.sampleTime = 1.f / 48000.f;
    frame.externalConnected = true;

    float maxStrength = 0.f;
    float maxCv1 = 0.f;
    float maxAgitation = 0.f;
    float maxCore = 0.f;
    float maxSub = 0.f;

    for (int i = 0; i < 96000; ++i)
    {
      frame.externalVolts = 1.5f * std::sin(2.f * kPi * 110.f * static_cast<float>(i) / 48000.f);
      const Outputs out = engine.process(frame, controls);
      maxStrength = std::max(maxStrength, std::fabs(out.strengthVolts));
      maxCv1 = std::max(maxCv1, out.cv1Volts);
      maxAgitation = std::max(maxAgitation, out.agitationVolts);
      maxCore = std::max(maxCore, std::fabs(out.toneCoreVolts));
      maxSub = std::max(maxSub, std::fabs(out.subHarmonicsVolts));

      T_ASSERT(ctx, std::isfinite(out.modularVolts));
      T_ASSERT(ctx, out.cv1Volts >= -1e-6f && out.cv1Volts <= 10.001f);
      T_ASSERT(ctx, std::fabs(out.cv2Volts) <= 5.001f);
      T_ASSERT(ctx, out.agitationVolts >= -1e-6f && out.agitationVolts <= 6.001f);
      T_ASSERT(ctx, std::fabs(out.toneCoreVolts) <= 5.2f);
      T_ASSERT(ctx, std::fabs(out.subHarmonicsVolts) <= 5.2f);
      T_ASSERT(ctx, std::fabs(out.lineVolts) <= 1.601f);
    }

    T_ASSERT(ctx, maxStrength > 1.f);
    T_ASSERT(ctx, maxStrength <= 10.001f);
    T_ASSERT(ctx, maxCv1 > 0.5f);
    T_ASSERT(ctx, maxAgitation > 1.f);
    T_ASSERT(ctx, maxCore > 4.f);
    T_ASSERT(ctx, maxSub > 3.f);

    WyrdEngine stoppedAgitation;
    frame = Frame{};
    frame.sampleTime = 1.f / 48000.f;
    frame.beginEndConnected = true;
    frame.beginEndVolts = 0.f;
    const Outputs stopped = runEngine(stoppedAgitation, controls, frame, 2048);
    T_ASSERT_NEAR(ctx, stopped.agitationVolts, 0.f, 1e-6f);
  }

  static void test_engine_blend_and_absorb_cv_behaviour(TestContext &ctx)
  {
    Controls controls = defaultControls();
    controls.level = 1.f;
    controls.blend = 1.f;
    controls.timeCoarse = 0.f;
    controls.decay = 0.35f;

    Frame dryFrame;
    dryFrame.sampleTime = 1.f / 48000.f;
    dryFrame.blendCvConnected = true;
    dryFrame.blendCvVolts = 0.f;

    Frame wetFrame = dryFrame;
    wetFrame.blendCvVolts = 5.f;

    WyrdEngine dryEngine;
    WyrdEngine wetEngine;
    float diff = 0.f;
    for (int i = 0; i < 24000; ++i)
    {
      const Outputs dry = dryEngine.process(dryFrame, controls);
      const Outputs wet = wetEngine.process(wetFrame, controls);
      diff += std::fabs(dry.modularVolts - wet.modularVolts);
    }
    T_ASSERT(ctx, diff / 24000.f > 0.01f);

    WyrdEngine absorbClosed;
    WyrdEngine absorbOpen;
    Frame absorbClosedFrame;
    absorbClosedFrame.sampleTime = 1.f / 48000.f;
    absorbClosedFrame.absorbCvConnected = true;
    absorbClosedFrame.absorbCvVolts = 0.f;
    Frame absorbOpenFrame = absorbClosedFrame;
    absorbOpenFrame.absorbCvVolts = 10.f;
    controls.absorb = 1.f;
    float closedAbs = 0.f;
    float openAbs = 0.f;
    for (int i = 0; i < 48000; ++i)
    {
      const Outputs closed = absorbClosed.process(absorbClosedFrame, controls);
      const Outputs open = absorbOpen.process(absorbOpenFrame, controls);
      if (i > 12000)
      {
        closedAbs += std::fabs(closed.cv2Volts);
        openAbs += std::fabs(open.cv2Volts);
      }
    }
    T_ASSERT(ctx, std::fabs(openAbs - closedAbs) > 0.05f);
  }

  static void test_touch_routings_affect_core_destinations(TestContext &ctx)
  {
    Controls base = defaultControls();
    base.externalConstant = 0.f;
    base.activationConstant = 0.f;
    base.activationInterference = 0.f;
    base.level = 1.f;
    base.blend = 0.f;
    base.decay = 0.2f;
    base.absorb = 0.8f;

    Frame frame;
    frame.sampleTime = 1.f / 48000.f;

    WyrdEngine silent;
    WyrdEngine activated;
    Controls activatedControls = base;
    activatedControls.touchActivation = 1.f;
    const Outputs silentOut = runEngine(silent, base, frame, 2048);
    const Outputs activatedOut = runEngine(activated, activatedControls, frame, 2048);
    T_ASSERT(ctx, std::fabs(activatedOut.modularVolts) > std::fabs(silentOut.modularVolts) + 0.05f);

    WyrdEngine tonicA;
    WyrdEngine tonicB;
    Controls touchTonic = defaultControls();
    touchTonic.touchTonic = 1.f;
    float tonicDiff = 0.f;
    for (int i = 0; i < 2048; ++i)
    {
      const Outputs a = tonicA.process(frame, defaultControls());
      const Outputs b = tonicB.process(frame, touchTonic);
      tonicDiff += std::fabs(a.toneCoreVolts - b.toneCoreVolts);
    }
    T_ASSERT(ctx, tonicDiff > 0.05f);

    WyrdEngine normalTail;
    WyrdEngine absorbedTail;
    Controls normal = defaultControls();
    normal.decay = 0.85f;
    normal.absorb = 0.9f;
    normal.timeCoarse = 0.02f;
    Controls absorbed = normal;
    absorbed.touchAbsorb = 1.f;
    float normalCv = 0.f;
    float absorbedCv = 0.f;
    for (int i = 0; i < 48000; ++i)
    {
      const Outputs n = normalTail.process(frame, normal);
      const Outputs a = absorbedTail.process(frame, absorbed);
      if (i > 6000)
      {
        normalCv += std::fabs(n.cv2Volts);
        absorbedCv += std::fabs(a.cv2Volts);
      }
    }
    T_ASSERT(ctx, normalCv > absorbedCv * 1.05f);

    WyrdEngine decayNormal;
    WyrdEngine decayTouched;
    Controls decayBase = defaultControls();
    decayBase.decay = 0.2f;
    Controls decayTouch = defaultControls();
    decayTouch.decay = 0.2f;
    decayTouch.touchDecay = 1.f;
    float normalResult = 0.f;
    float touchedResult = 0.f;
    for (int i = 0; i < 48000; ++i)
    {
      const Outputs n = decayNormal.process(frame, decayBase);
      const Outputs t = decayTouched.process(frame, decayTouch);
      if (i > 6000)
      {
        normalResult += std::fabs(n.cv2Volts);
        touchedResult += std::fabs(t.cv2Volts);
      }
    }
    T_ASSERT(ctx, touchedResult > normalResult * 1.02f);
  }

  static void test_touch_matrix_sources(TestContext &ctx)
  {
    Frame frame;
    frame.sampleTime = 1.f / 48000.f;
    frame.externalConnected = true;
    frame.externalVolts = 4.f;

    Controls base = defaultControls();
    base.activationConstant = 0.f;
    base.externalConstant = 0.f;
    base.blend = 0.f;
    base.level = 1.f;
    base.touchActivation = 1.f;

    WyrdEngine manualEngine;
    WyrdEngine cv1Engine;
    WyrdEngine noiseEngineA;
    WyrdEngine noiseEngineB;
    Controls manual = base;
    manual.touchSource = static_cast<int>(TouchSource::MANUAL);
    Controls cv1 = base;
    cv1.touchSource = static_cast<int>(TouchSource::CV1);
    Controls noiseTonic = defaultControls();
    noiseTonic.touchSource = static_cast<int>(TouchSource::NOISE);
    noiseTonic.touchTonic = 1.f;
    Controls manualTonic = defaultControls();
    manualTonic.touchSource = static_cast<int>(TouchSource::MANUAL);
    manualTonic.touchTonic = 1.f;

    float activationDiff = 0.f;
    float tonicDiff = 0.f;
    for (int i = 0; i < 12000; ++i)
    {
      frame.externalVolts = 4.f * std::sin(2.f * kPi * 140.f * static_cast<float>(i) / 48000.f);
      const Outputs m = manualEngine.process(frame, manual);
      const Outputs c = cv1Engine.process(frame, cv1);
      const Outputs n = noiseEngineA.process(frame, noiseTonic);
      const Outputs mt = noiseEngineB.process(frame, manualTonic);
      if (i > 2048)
      {
        activationDiff += std::fabs(m.modularVolts - c.modularVolts);
        tonicDiff += std::fabs(n.toneCoreVolts - mt.toneCoreVolts);
      }
    }

    T_ASSERT(ctx, activationDiff > 0.5f);
    T_ASSERT(ctx, tonicDiff > 0.5f);
  }

  static void test_touch_source_value_variants(TestContext &ctx)
  {
    WyrdEngine engine;
    Controls controls = defaultControls();

    engine.timeFilter.feedbackState = 0.42f;
    controls.touchSource = static_cast<int>(TouchSource::CV2);
    T_ASSERT_NEAR(ctx, engine.touchSourceValue(controls, -0.25f, 0.8f, 0.75f), 0.42f, 1e-6f);

    controls.touchSource = static_cast<int>(TouchSource::AGITATION);
    T_ASSERT_NEAR(ctx, engine.touchSourceValue(controls, -0.25f, 0.8f, 0.75f), 0.5f, 1e-6f);

    engine.last.subHarmonicsVolts = -3.f;
    controls.touchSource = static_cast<int>(TouchSource::SUB_HARMONICS);
    T_ASSERT_NEAR(ctx, engine.touchSourceValue(controls, -0.25f, 0.8f, 0.75f), -0.6f, 1e-6f);

    controls.touchSource = static_cast<int>(TouchSource::STRENGTH);
    T_ASSERT_NEAR(ctx, engine.touchSourceValue(controls, -0.25f, 0.8f, 0.75f), -0.25f, 1e-6f);
  }

  static void test_delay_wrap_and_core_recovery_edges(TestContext &ctx)
  {
    ReverbDelayLine reverbLine(8);
    reverbLine.writeIndex = 20;
    T_ASSERT(ctx, std::isfinite(reverbLine.read(1.f)));

    reverbLine.reset();
    reverbLine.writeIndex = 7;
    reverbLine.write(0.5f);
    T_ASSERT(ctx, reverbLine.writeIndex == 0);

    TimeFilterExperiment timeFilter;
    timeFilter.writeIndex = 20;
    T_ASSERT(ctx, std::isfinite(timeFilter.read(1.f, 8)));

    timeFilter.writeIndex = TimeFilterExperiment::kMaxDelaySamples;
    const TimeFilterResult result = timeFilter.process(0.f, 0.f, 0.1f, 0.f, 0.2f, 0.5f, 0.5f, 1000.f);
    T_ASSERT(ctx, std::isfinite(result.wet));
    T_ASSERT(ctx, timeFilter.writeIndex == 1);

    WyrdCoreProcessor core;
    core.context.phase = 2.f;
    const WyrdCoreSignals recovered = core.process(0.f, 0.4f, 0.3f, 0.f, 0.f, 0.f,
                                                   0.f, 0.f, 0.6f, 0.6f, 0.3f, 0.f,
                                                   48000.f);
    T_ASSERT(ctx, std::isfinite(recovered.core));
    T_ASSERT(ctx, core.contextHealthy());
  }

  static void test_listening_calibration_landmarks(TestContext &ctx)
  {
    const float sampleRate = 48000.f;
    WyrdCoreProcessor dark;
    WyrdCoreProcessor bright;
    std::vector<float> darkSamples;
    std::vector<float> brightSamples;
    darkSamples.reserve(4096);
    brightSamples.reserve(4096);

    for (int i = 0; i < 4096; ++i)
    {
      darkSamples.push_back(dark.process(0.f, 0.04f, 0.9f, 0.f, 0.f, 0.1f,
                                         0.1f, 0.1f, 0.58f, 0.8f, 0.45f, 0.1f,
                                         sampleRate).selectedTone);
      brightSamples.push_back(bright.process(0.f, 0.95f, 0.9f, 0.f, 0.f, 0.1f,
                                             0.1f, 0.1f, 0.95f, 0.8f, 0.45f, 0.1f,
                                             sampleRate).selectedTone);
    }

    const float darkCentroid = spectralCentroid(darkSamples, sampleRate);
    const float brightCentroid = spectralCentroid(brightSamples, sampleRate);
    T_ASSERT(ctx, brightCentroid > darkCentroid * 1.18f);

    WyrdEngine echoVerb;
    Controls echo = defaultControls();
    echo.activationConstant = 0.35f;
    echo.timeCoarse = 0.04f;
    echo.decay = 0.86f;
    echo.filter = 0.45f;
    echo.absorb = 0.88f;
    echo.blend = 1.f;
    echo.level = 1.f;
    Frame frame;
    frame.sampleTime = 1.f / sampleRate;

    float early = 0.f;
    float late = 0.f;
    for (int i = 0; i < 96000; ++i)
    {
      const Outputs out = echoVerb.process(frame, echo);
      if (i > 3000 && i < 12000)
      {
        early += std::fabs(out.cv2Volts);
      }
      if (i > 42000)
      {
        late += std::fabs(out.cv2Volts);
      }
      T_ASSERT(ctx, std::isfinite(out.modularVolts));
      T_ASSERT(ctx, std::fabs(out.cv2Volts) <= 5.001f);
    }

    T_ASSERT(ctx, early > 0.3f);
    T_ASSERT(ctx, late > 0.12f);
  }

  static void test_wyrd_ambient_reverb_dsp(TestContext &ctx)
  {
    WyrdAmbientReverb lowDecay;
    WyrdAmbientReverb highDecay;
    const float sampleRate = 48000.f;
    float lowTail = 0.f;
    float highTail = 0.f;
    float highPeak = 0.f;
    int activeSamples = 0;

    for (int i = 0; i < 96000; ++i)
    {
      const float impulse = i == 0 ? 1.f : 0.f;
      const WyrdReverbResult low = lowDecay.process(impulse, 0.72f, 0.18f, 0.82f, 0.55f, 0.2f, sampleRate);
      const WyrdReverbResult high = highDecay.process(impulse, 0.72f, 0.92f, 0.82f, 0.55f, 0.42f, sampleRate);

      T_ASSERT(ctx, std::isfinite(low.mono));
      T_ASSERT(ctx, std::isfinite(high.mono));
      T_ASSERT(ctx, std::fabs(low.mono) <= 1.001f);
      T_ASSERT(ctx, std::fabs(high.mono) <= 1.001f);

      if (i > 12000)
      {
        lowTail += std::fabs(low.mono);
        highTail += std::fabs(high.mono);
        if (std::fabs(high.mono) > 0.0002f)
        {
          ++activeSamples;
        }
      }
      highPeak = std::max(highPeak, std::fabs(high.mono));
    }

    T_ASSERT(ctx, highPeak > 0.01f);
    T_ASSERT(ctx, highTail > lowTail * 1.3f);
    T_ASSERT(ctx, activeSamples > 1800);
  }

  static void test_extreme_engine_stability(TestContext &ctx)
  {
    WyrdEngine engine;
    Controls controls;
    controls.strength = 1.f;
    controls.externalConstant = 1.f;
    controls.activationConstant = 1.f;
    controls.activationInterference = 1.f;
    controls.activationCvAmount = -1.f;
    controls.tonicCoarse = 1.f;
    controls.tonicFine = 0.0833333f;
    controls.tonicModAmount = 1.f;
    controls.tones = 1.f;
    controls.tonesCvAmount = -1.f;
    controls.timeCoarse = 1.f;
    controls.timeFine = 0.12f;
    controls.timeModAmount = 1.f;
    controls.timeCvAmount = -1.f;
    controls.decay = 1.f;
    controls.decayCvAmount = 1.f;
    controls.filter = 1.f;
    controls.filterCvAmount = -1.f;
    controls.absorb = 1.f;
    controls.blend = 1.f;
    controls.level = 1.f;
    controls.speed = 1.f;
    controls.angle = 1.f;
    controls.speedCvAmount = 1.f;
    controls.touchActivation = 1.f;
    controls.touchTime = 1.f;
    controls.touchFilter = 1.f;
    controls.touchAbsorb = 1.f;
    controls.touchTonic = 1.f;
    controls.touchDecay = 1.f;
    controls.touchSource = static_cast<int>(TouchSource::NOISE);
    controls.strengthCalibration = static_cast<int>(StrengthCalibration::MODULAR_HOT);

    Frame frame;
    frame.sampleTime = 1.f / 96000.f;
    frame.externalConnected = true;
    frame.externalVolts = 10.f;
    frame.beginEndConnected = false;
    frame.speedCvVolts = 5.f;
    frame.activationCvVolts = -5.f;
    frame.tonicModConnected = true;
    frame.tonicModVolts = 10.f;
    frame.tonesCvConnected = true;
    frame.tonesCvVolts = -10.f;
    frame.vOctVolts = 5.f;
    frame.timeModConnected = true;
    frame.timeModVolts = 10.f;
    frame.timeCvConnected = true;
    frame.timeCvVolts = -10.f;
    frame.timeUnityCvConnected = true;
    frame.timeUnityCvVolts = 10.f;
    frame.decayCvConnected = true;
    frame.decayCvVolts = 10.f;
    frame.blendCvConnected = true;
    frame.blendCvVolts = 5.f;
    frame.filterCvConnected = true;
    frame.filterCvVolts = -10.f;
    frame.absorbCvConnected = true;
    frame.absorbCvVolts = 10.f;

    for (int i = 0; i < 30000; ++i)
    {
      const Outputs out = engine.process(frame, controls);
      T_ASSERT(ctx, std::isfinite(out.strengthVolts));
      T_ASSERT(ctx, std::isfinite(out.cv1Volts));
      T_ASSERT(ctx, std::isfinite(out.cv2Volts));
      T_ASSERT(ctx, std::isfinite(out.modularVolts));
      T_ASSERT(ctx, out.cv1Volts >= -1e-6f && out.cv1Volts <= 10.001f);
      T_ASSERT(ctx, std::fabs(out.cv2Volts) <= 5.001f);
      T_ASSERT(ctx, std::fabs(out.modularVolts) <= 5.201f);
      T_ASSERT(ctx, std::fabs(out.lineVolts) <= 1.601f);
    }
  }

  int run_all_wyrd_tests()
  {
    TestContext ctx;

    std::printf("\n=== Wyrd DSP Unit Tests ===\n\n");

    test_strength_and_cv1_ranges(ctx);
    test_strength_calibration_modes(ctx);
    test_external_dummy_cable_pickup(ctx);
    test_vult_core_pitch_and_tone_palette(ctx);
    test_agitation_normals_and_range(ctx);
    test_time_filter_impulse_and_feedback(ctx);
    test_time_filter_v2_tail_density(ctx);
    test_raw_feedback_tonic_normal(ctx);
    test_engine_manual_ranges_and_normals(ctx);
    test_engine_blend_and_absorb_cv_behaviour(ctx);
    test_touch_routings_affect_core_destinations(ctx);
    test_touch_matrix_sources(ctx);
    test_touch_source_value_variants(ctx);
    test_delay_wrap_and_core_recovery_edges(ctx);
    test_listening_calibration_landmarks(ctx);
    test_wyrd_ambient_reverb_dsp(ctx);
    test_extreme_engine_stability(ctx);

    std::printf("\n");
    ctx.summary();
    std::printf("\n");
    return ctx.failed > 0 ? 1 : 0;
  }
} // namespace TestSuite

int main()
{
  return TestSuite::run_all_wyrd_tests();
}
