#include <algorithm>
#include <cmath>
#include <cstdio>

#include "dsp/drif-dsp.h"

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

  using namespace shortwav::drift;

  static Controls centeredControls()
  {
    Controls controls;
    controls.offset = 0.5f;
    controls.speed = 0.5f;
    controls.range = 0.72f;
    controls.mix = 0.f;
    return controls;
  }

  static void test_offset_and_range_shape_voltage(TestContext &ctx)
  {
    Frame frame;
    Controls positive = centeredControls();
    positive.range = 0.f;
    positive.offset = 1.f;
    Engine positiveEngine;
    const Outputs positiveOut = positiveEngine.process(frame, positive);
    T_ASSERT_NEAR(ctx, positiveOut.outputVolts, 5.f, 0.001f);

    Controls negative = centeredControls();
    negative.range = 0.f;
    negative.offset = 0.f;
    Engine negativeEngine;
    const Outputs negativeOut = negativeEngine.process(frame, negative);
    T_ASSERT_NEAR(ctx, negativeOut.outputVolts, -5.f, 0.001f);
  }

  static void test_signal_b_attenuverter(TestContext &ctx)
  {
    Frame frame;
    frame.inputConnected = true;
    frame.inputVolts = 5.f;

    Controls positive = centeredControls();
    positive.range = 1.f;
    positive.mix = 1.f;
    Engine positiveEngine;
    const Outputs positiveOut = positiveEngine.process(frame, positive);
    T_ASSERT_NEAR(ctx, positiveOut.outputVolts, 5.f, 0.001f);

    Controls inverted = centeredControls();
    inverted.range = 1.f;
    inverted.mix = -1.f;
    Engine invertedEngine;
    const Outputs invertedOut = invertedEngine.process(frame, inverted);
    T_ASSERT_NEAR(ctx, invertedOut.outputVolts, -5.f, 0.001f);
  }

  static void test_random_lfo_moves_and_stays_bounded(TestContext &ctx)
  {
    Engine engine;
    Frame frame;
    Controls controls = centeredControls();
    controls.speed = 1.f;
    controls.range = 1.f;

    float minOut = 100.f;
    float maxOut = -100.f;
    for (int i = 0; i < 96000; ++i)
    {
      const Outputs out = engine.process(frame, controls);
      minOut = std::min(minOut, out.outputVolts);
      maxOut = std::max(maxOut, out.outputVolts);
      T_ASSERT(ctx, std::isfinite(out.outputVolts));
      T_ASSERT(ctx, out.outputVolts >= -10.001f && out.outputVolts <= 10.001f);
    }

    T_ASSERT(ctx, maxOut - minOut > 3.f);
  }

  static void test_activity_reports_interference(TestContext &ctx)
  {
    Engine engine;
    Frame frame;
    Controls controls = centeredControls();
    controls.speed = 1.f;
    controls.range = 1.f;

    float maxActivity = 0.f;
    for (int i = 0; i < 192000; ++i)
    {
      const Outputs out = engine.process(frame, controls);
      maxActivity = std::max(maxActivity, out.activity);
    }

    T_ASSERT(ctx, maxActivity > 0.03f);
  }

  static void test_extreme_control_stability(TestContext &ctx)
  {
    Engine engine;
    Frame frame;
    Controls controls = centeredControls();

    for (int i = 0; i < 60000; ++i)
    {
      const float t = static_cast<float>(i) / 48000.f;
      controls.offset = 0.5f + 0.5f * std::sin(t * 1.7f);
      controls.speed = 0.5f + 0.5f * std::sin(t * 2.3f + 0.2f);
      controls.range = 0.5f + 0.5f * std::sin(t * 3.1f + 0.7f);
      controls.mix = std::sin(t * 4.9f);
      frame.inputConnected = true;
      frame.inputVolts = 10.f * std::sin(t * 8.0f);

      const Outputs out = engine.process(frame, controls);
      T_ASSERT(ctx, std::isfinite(out.outputVolts));
      T_ASSERT(ctx, std::isfinite(out.randomVolts));
      T_ASSERT(ctx, std::isfinite(out.mixedVolts));
      T_ASSERT(ctx, out.outputVolts >= -10.001f && out.outputVolts <= 10.001f);
    }
  }
}

int main()
{
  using namespace TestSuite;
  std::printf("\n=== Drift DSP Unit Tests ===\n\n");
  TestContext ctx;

  test_offset_and_range_shape_voltage(ctx);
  test_signal_b_attenuverter(ctx);
  test_random_lfo_moves_and_stays_bounded(ctx);
  test_activity_reports_interference(ctx);
  test_extreme_control_stability(ctx);

  ctx.summary();
  return ctx.failed == 0 ? 0 : 1;
}
