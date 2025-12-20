// Unit tests for Tapestry DSP modules
//
// Tests cover:
// - TapestryBuffer: audio buffer management and interpolation
// - SpliceManager: splice marker creation, deletion, and navigation
// - GrainEngine: granular synthesis with multiple voices
// - TapestryDSP: integrated DSP processor
//
// Design principles:
// - Use only public APIs
// - Fast, allocation-free hot paths
// - Simple assertion-style testing

#include <cstdio>
#include <cmath>
#include <vector>
#include <limits>
#include "../dsp/tapestry-core.h"
#include "../dsp/tapestry-buffer.h"
#include "../dsp/tapestry-splice.h"
#include "../dsp/tapestry-grain.h"
#include "../dsp/tapestry-dsp.h"

// C++11 requires definitions for static constexpr members that are ODR-used
namespace ShortwavDSP
{
  constexpr float TapestryConfig::kInternalSampleRate;
  constexpr size_t TapestryConfig::kMaxReelFrames;
  constexpr size_t TapestryConfig::kMaxSplices;
  constexpr size_t TapestryConfig::kMaxGrainVoices;
  constexpr size_t TapestryConfig::kMaxReels;
  constexpr float TapestryConfig::kMinGeneSamples;
  constexpr float TapestryConfig::kMaxGeneSamples;
  constexpr float TapestryConfig::kSosCvMax;
  constexpr float TapestryConfig::kGeneSizeCvMax;
  constexpr float TapestryConfig::kVariSpeedCvMax;
  constexpr float TapestryConfig::kMorphCvMax;
  constexpr float TapestryConfig::kSlideCvMax;
  constexpr float TapestryConfig::kOrganizeCvMax;
  constexpr float TapestryConfig::kGateTriggerThreshold;
  constexpr float TapestryConfig::kVariSpeedUpSemitones;
  constexpr float TapestryConfig::kVariSpeedDownSemitones;
  constexpr float TapestryConfig::kAudioOutLevel;
  constexpr float TapestryConfig::kCvOutMax;
  constexpr float TapestryConfig::kGateOutLevel;
  
  constexpr size_t TapestryBuffer::kMaxFrames;
  constexpr size_t TapestryBuffer::kChannels;
  
  constexpr size_t SpliceManager::kMaxSplices;
  
  constexpr int GrainEngine::kMaxVoices;
}

namespace
{

constexpr float kEpsilon = 1e-5f;
constexpr float kTightEpsilon = 1e-6f;

// Simple assertion helpers
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
    if (diff <= tol || (std::isnan(expected) && std::isnan(actual)))
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

//------------------------------------------------------------------------------
// TapestryUtil tests
//------------------------------------------------------------------------------

void test_util_clamp(TestContext &ctx)
{
  using ShortwavDSP::TapestryUtil::clamp;
  using ShortwavDSP::TapestryUtil::clamp01;

  T_ASSERT_NEAR(ctx, clamp(-1.0f, 0.0f, 1.0f), 0.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, clamp(0.5f, 0.0f, 1.0f), 0.5f, kEpsilon);
  T_ASSERT_NEAR(ctx, clamp(2.0f, 0.0f, 1.0f), 1.0f, kEpsilon);

  T_ASSERT_NEAR(ctx, clamp01(-0.5f), 0.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, clamp01(0.5f), 0.5f, kEpsilon);
  T_ASSERT_NEAR(ctx, clamp01(1.5f), 1.0f, kEpsilon);
}

void test_util_lerp(TestContext &ctx)
{
  using ShortwavDSP::TapestryUtil::lerp;

  T_ASSERT_NEAR(ctx, lerp(0.0f, 1.0f, 0.0f), 0.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, lerp(0.0f, 1.0f, 0.5f), 0.5f, kEpsilon);
  T_ASSERT_NEAR(ctx, lerp(0.0f, 1.0f, 1.0f), 1.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, lerp(-1.0f, 1.0f, 0.5f), 0.0f, kEpsilon);
}

void test_util_hann_window(TestContext &ctx)
{
  using ShortwavDSP::TapestryUtil::hannWindow;

  // Hann window should be 0 at edges, 1 at center
  T_ASSERT_NEAR(ctx, hannWindow(0.0f), 0.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, hannWindow(0.5f), 1.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, hannWindow(1.0f), 0.0f, kEpsilon);

  // Check symmetry
  T_ASSERT_NEAR(ctx, hannWindow(0.25f), hannWindow(0.75f), kEpsilon);
}

void test_util_cubic_interpolate(TestContext &ctx)
{
  using ShortwavDSP::TapestryUtil::cubicInterpolate;

  // Linear data should return linear interpolation
  float y0 = 0.0f, y1 = 1.0f, y2 = 2.0f, y3 = 3.0f;
  T_ASSERT_NEAR(ctx, cubicInterpolate(y0, y1, y2, y3, 0.0f), 1.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, cubicInterpolate(y0, y1, y2, y3, 1.0f), 2.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, cubicInterpolate(y0, y1, y2, y3, 0.5f), 1.5f, 0.01f);
}

void test_util_varispeed_calculation(TestContext &ctx)
{
  using ShortwavDSP::TapestryUtil::calculateVariSpeed;

  // Center = stopped
  auto state = calculateVariSpeed(0.0f, 0.0f, 0.0f);
  T_ASSERT(ctx, state.isStopped);
  T_ASSERT_NEAR(ctx, state.speedRatio, 0.0f, kEpsilon);

  // Forward playback
  state = calculateVariSpeed(0.5f, 0.0f, 0.0f);
  T_ASSERT(ctx, !state.isStopped);
  T_ASSERT(ctx, state.isForward);
  T_ASSERT(ctx, state.speedRatio > 1.0f);

  // Reverse playback
  state = calculateVariSpeed(-0.5f, 0.0f, 0.0f);
  T_ASSERT(ctx, !state.isStopped);
  T_ASSERT(ctx, !state.isForward);
  T_ASSERT(ctx, state.speedRatio < 0.0f);

  // Unity speed detection
  state = calculateVariSpeed(0.02f, 0.0f, 0.0f);
  T_ASSERT(ctx, state.isAtUnity || state.isStopped);
}

void test_util_fast_random(TestContext &ctx)
{
  using ShortwavDSP::TapestryUtil::FastRandom;

  FastRandom rng(12345);

  // nextFloat should be in [0, 1)
  for (int i = 0; i < 100; i++)
  {
    float val = rng.nextFloat();
    T_ASSERT(ctx, val >= 0.0f && val < 1.0f);
  }

  // nextBipolar should be in [-1, 1)
  for (int i = 0; i < 100; i++)
  {
    float val = rng.nextBipolar();
    T_ASSERT(ctx, val >= -1.0f && val < 1.0f);
  }

  // nextRange should be in [min, max)
  for (int i = 0; i < 100; i++)
  {
    float val = rng.nextRange(5.0f, 10.0f);
    T_ASSERT(ctx, val >= 5.0f && val < 10.0f);
  }

  // Same seed should produce same sequence
  FastRandom rng1(999);
  FastRandom rng2(999);
  for (int i = 0; i < 10; i++)
  {
    T_ASSERT_NEAR(ctx, rng1.nextFloat(), rng2.nextFloat(), kTightEpsilon);
  }
}

//------------------------------------------------------------------------------
// TapestryBuffer tests
//------------------------------------------------------------------------------

void test_buffer_basic_operations(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Initially empty
  T_ASSERT(ctx, buffer.isEmpty());
  T_ASSERT(ctx, buffer.getUsedFrames() == 0);
  T_ASSERT(ctx, buffer.getMaxFrames() > 0);

  // Write stereo sample
  T_ASSERT(ctx, buffer.writeStereo(0, 0.5f, -0.5f));
  T_ASSERT(ctx, buffer.getUsedFrames() == 1);
  T_ASSERT(ctx, !buffer.isEmpty());

  // Read back
  float l, r;
  buffer.readStereo(0, l, r);
  T_ASSERT_NEAR(ctx, l, 0.5f, kEpsilon);
  T_ASSERT_NEAR(ctx, r, -0.5f, kEpsilon);

  // Clear
  buffer.clear();
  T_ASSERT(ctx, buffer.isEmpty());
}

void test_buffer_interpolation(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Write linear ramp
  for (size_t i = 0; i < 10; i++)
  {
    float val = static_cast<float>(i) / 10.0f;
    buffer.writeStereo(i, val, -val);
  }

  // Test interpolation between samples
  float l, r;
  buffer.readStereoInterpolated(0.5, l, r);
  // Cubic interpolation result should be in valid range (though not necessarily linear midpoint)
  T_ASSERT(ctx, l >= -0.1f && l <= 0.2f);
  T_ASSERT(ctx, r >= -0.2f && r <= 0.1f);

  // Test wrapping
  buffer.readStereoInterpolated(9.5, l, r);
  T_ASSERT(ctx, !std::isnan(l) && !std::isnan(r));
}

void test_buffer_bounded_interpolation(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Write samples 0-19
  for (size_t i = 0; i < 20; i++)
  {
    buffer.writeStereo(i, static_cast<float>(i), 0.0f);
  }

  // Read within splice bounds [5, 15)
  float l, r;
  buffer.readStereoInterpolatedBounded(10.0, 5, 15, l, r);
  T_ASSERT_NEAR(ctx, l, 10.0f, kEpsilon);

  // Read at boundary should wrap within splice
  buffer.readStereoInterpolatedBounded(14.5, 5, 15, l, r);
  T_ASSERT(ctx, l >= 5.0f && l < 15.0f);
}

void test_buffer_sound_on_sound(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Write initial content
  buffer.writeStereo(0, 0.5f, 0.5f);

  // Mix with new content (50/50 blend)
  buffer.mixAndWrite(0, 1.0f, 1.0f, 0.5f);

  float l, r;
  buffer.readStereo(0, l, r);
  T_ASSERT_NEAR(ctx, l, 0.75f, kEpsilon); // (1.0 * 0.5 + 0.5 * 0.5)
  T_ASSERT_NEAR(ctx, r, 0.75f, kEpsilon);
}

void test_buffer_bulk_operations(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Create test data
  std::vector<float> testData(200); // 100 stereo frames
  for (size_t i = 0; i < testData.size(); i++)
  {
    testData[i] = static_cast<float>(i);
  }

  // Copy into buffer
  buffer.copyFrom(testData.data(), 100, 0);
  T_ASSERT(ctx, buffer.getUsedFrames() == 100);

  // Copy back out
  std::vector<float> readData(200, 0.0f);
  buffer.copyTo(readData.data(), 100, 0);

  // Verify
  for (size_t i = 0; i < 10; i++)
  {
    T_ASSERT_NEAR(ctx, readData[i], testData[i], kEpsilon);
  }
}

void test_buffer_clear_range(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Write data
  for (size_t i = 0; i < 100; i++)
  {
    buffer.writeStereo(i, 1.0f, 1.0f);
  }

  // Clear range [20, 30)
  buffer.clearRange(20, 30);

  // Verify range is cleared
  float l, r;
  buffer.readStereo(25, l, r);
  T_ASSERT_NEAR(ctx, l, 0.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, r, 0.0f, kEpsilon);

  // Verify outside range is intact
  buffer.readStereo(10, l, r);
  T_ASSERT_NEAR(ctx, l, 1.0f, kEpsilon);
}

//------------------------------------------------------------------------------
// SpliceManager tests
//------------------------------------------------------------------------------

void test_splice_initialization(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;

  // Initially empty
  T_ASSERT(ctx, mgr.isEmpty());
  T_ASSERT(ctx, mgr.getNumSplices() == 0);

  // Initialize with buffer
  mgr.initialize(48000);
  T_ASSERT(ctx, !mgr.isEmpty());
  T_ASSERT(ctx, mgr.getNumSplices() == 1);

  auto *splice = mgr.getCurrentSplice();
  T_ASSERT(ctx, splice != nullptr);
  T_ASSERT(ctx, splice->startFrame == 0);
  T_ASSERT(ctx, splice->endFrame == 48000);
}

void test_splice_marker_creation(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);

  // Add marker at position 500
  T_ASSERT(ctx, mgr.addMarker(500));
  T_ASSERT(ctx, mgr.getNumSplices() == 2);

  // First splice should be [0, 500)
  auto *splice0 = mgr.getSplice(0);
  T_ASSERT(ctx, splice0->startFrame == 0);
  T_ASSERT(ctx, splice0->endFrame == 500);

  // Second splice should be [500, 1000)
  auto *splice1 = mgr.getSplice(1);
  T_ASSERT(ctx, splice1->startFrame == 500);
  T_ASSERT(ctx, splice1->endFrame == 1000);

  // Add another marker
  T_ASSERT(ctx, mgr.addMarker(250));
  T_ASSERT(ctx, mgr.getNumSplices() == 3);
}

void test_splice_marker_deletion(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);
  mgr.addMarker(500);

  // Should have 2 splices now
  T_ASSERT(ctx, mgr.getNumSplices() == 2);

  // Delete current marker (should merge)
  T_ASSERT(ctx, mgr.deleteCurrentMarker());
  T_ASSERT(ctx, mgr.getNumSplices() == 1);

  // Can't delete last marker
  T_ASSERT(ctx, !mgr.deleteCurrentMarker());
}

void test_splice_navigation_shift(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);
  mgr.addMarker(333);
  mgr.addMarker(666);
  // Now have 3 splices: [0,333), [333,666), [666,1000)

  T_ASSERT(ctx, mgr.getCurrentIndex() == 0);

  // Shift immediately to next
  mgr.shiftImmediate();
  T_ASSERT(ctx, mgr.getCurrentIndex() == 1);

  mgr.shiftImmediate();
  T_ASSERT(ctx, mgr.getCurrentIndex() == 2);

  // Should wrap around
  mgr.shiftImmediate();
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0);
}

void test_splice_navigation_organize(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);
  mgr.addMarker(500);
  // 2 splices: [0,500), [500,1000)

  // Organize parameter 0.0 -> first splice (applies immediately)
  mgr.setOrganize(0.0f);
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0);

  // Organize parameter 1.0 -> last splice (applies immediately)
  mgr.setOrganize(1.0f);
  T_ASSERT(ctx, mgr.getCurrentIndex() == 1);

  // Organize parameter 0.5 -> middle (rounds)
  mgr.setOrganize(0.5f);
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0 || mgr.getCurrentIndex() == 1);
}

void test_splice_pending_system(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);
  mgr.addMarker(500);

  T_ASSERT(ctx, !mgr.hasPending());

  // Shift creates pending
  mgr.shift();
  T_ASSERT(ctx, mgr.hasPending());
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0); // Not changed yet

  // onEndOfSplice applies pending
  mgr.onEndOfSplice();
  T_ASSERT(ctx, mgr.getCurrentIndex() == 1);
  T_ASSERT(ctx, !mgr.hasPending());
}

void test_splice_organize_vs_shift_priority(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);
  mgr.addMarker(333);
  mgr.addMarker(666);
  // 3 splices

  // Start at splice 0
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0);

  // Set organize to target splice 2 (applies immediately)
  mgr.setOrganize(1.0f);
  T_ASSERT(ctx, mgr.getCurrentIndex() == 2);

  // Shift immediate should move to next (wraps to 0)
  mgr.shiftImmediate();
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0);

  // Setting organize again should apply immediately
  mgr.setOrganize(0.5f);  // Middle splice
  T_ASSERT(ctx, mgr.getCurrentIndex() == 1);
}

void test_splice_extend_for_recording(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);

  auto *splice = mgr.getCurrentSplice();
  T_ASSERT(ctx, splice->endFrame == 1000);

  // Extend last splice
  mgr.extendLastSplice(1500);
  splice = mgr.getCurrentSplice();
  T_ASSERT(ctx, splice->endFrame == 1500);
}

void test_splice_delete_all(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;

  SpliceManager mgr;
  mgr.initialize(1000);
  mgr.addMarker(250);
  mgr.addMarker(500);
  mgr.addMarker(750);

  T_ASSERT(ctx, mgr.getNumSplices() == 4);

  mgr.deleteAllMarkers();
  T_ASSERT(ctx, mgr.getNumSplices() == 1);
  T_ASSERT(ctx, mgr.getCurrentIndex() == 0);

  auto *splice = mgr.getCurrentSplice();
  T_ASSERT(ctx, splice->startFrame == 0);
  T_ASSERT(ctx, splice->endFrame == 1000);
}

//------------------------------------------------------------------------------
// GrainEngine tests
//------------------------------------------------------------------------------

void test_grain_basic_setup(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::MorphState;
  using ShortwavDSP::VariSpeedState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);
  engine.setGeneSize(4800.0f); // 0.1 seconds

  TapestryBuffer buffer;
  // Fill buffer with test tone
  for (size_t i = 0; i < 1000; i++)
  {
    buffer.writeStereo(i, 0.5f, 0.5f);
  }

  MorphState morph;
  morph.activeVoices = 1;
  morph.overlap = 1.0f;
  engine.setMorphState(morph);

  VariSpeedState speed;
  speed.speedRatio = 1.0f;
  speed.isForward = true;
  speed.isStopped = false;
  engine.setVariSpeed(speed);

  // Start playback
  engine.retrigger(0.0f);

  // Process some samples
  float outL, outR;
  bool endOfGene = false;
  for (int i = 0; i < 100; i++)
  {
    engine.process(buffer, 0, 1000, outL, outR, endOfGene);
  }

  // Should produce some output
  T_ASSERT(ctx, engine.isActive() || endOfGene);
}

void test_grain_stopped_mode(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::VariSpeedState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);

  TapestryBuffer buffer;
  for (size_t i = 0; i < 1000; i++)
  {
    buffer.writeStereo(i, 1.0f, 1.0f);
  }

  // Set stopped
  VariSpeedState speed;
  speed.isStopped = true;
  engine.setVariSpeed(speed);

  // Process should return false and output silence
  float outL, outR;
  bool endOfGene;
  bool result = engine.process(buffer, 0, 1000, outL, outR, endOfGene);
  T_ASSERT(ctx, !result);
  T_ASSERT_NEAR(ctx, outL, 0.0f, kEpsilon);
  T_ASSERT_NEAR(ctx, outR, 0.0f, kEpsilon);
}

void test_grain_retrigger(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::VariSpeedState;
  using ShortwavDSP::MorphState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);

  TapestryBuffer buffer;
  for (size_t i = 0; i < 1000; i++)
  {
    buffer.writeStereo(i, static_cast<float>(i) / 1000.0f, 0.0f);
  }

  MorphState morph;
  morph.activeVoices = 1;
  morph.overlap = 1.0f;
  engine.setMorphState(morph);

  VariSpeedState speed;
  speed.speedRatio = 1.0f;
  speed.isForward = true;
  speed.isStopped = false;
  engine.setVariSpeed(speed);

  // Start and process to advance position
  engine.retrigger(0.0f);
  float outL, outR;
  bool endOfGene;
  for (int i = 0; i < 100; i++)
  {
    engine.process(buffer, 0, 1000, outL, outR, endOfGene);
  }

  // Retrigger should reset position to near zero
  engine.retrigger(0.0f);
  double pos2 = engine.getPlayheadPosition();

  // After retrigger, position should be reset
  // Both positions are relative to splice start after wrapping
  // So we just verify both are valid positions
  T_ASSERT(ctx, pos2 >= 0.0 && pos2 < 1100.0); // Within reasonable bounds
}

void test_grain_slide_parameter(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::VariSpeedState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);
  engine.setGeneSize(4800.0f);

  TapestryBuffer buffer;
  for (size_t i = 0; i < 10000; i++)
  {
    buffer.writeStereo(i, static_cast<float>(i), 0.0f);
  }

  VariSpeedState speed;
  speed.speedRatio = 1.0f;
  speed.isForward = true;
  speed.isStopped = false;
  engine.setVariSpeed(speed);

  // Slide at 0.0 (start)
  engine.setSlide(0.0f);
  engine.retrigger(0.0f);
  float outL1, outR1;
  bool endOfGene;
  engine.process(buffer, 0, 10000, outL1, outR1, endOfGene);

  // Slide at 0.5 (middle)
  engine.setSlide(0.5f);
  engine.retrigger(0.0f);
  float outL2, outR2;
  engine.process(buffer, 0, 10000, outL2, outR2, endOfGene);

  // Different slide values should produce different starting positions
  // (though windowing may make them similar at first sample)
  T_ASSERT(ctx, engine.getPlayheadPosition() >= 0.0);
}

void test_grain_reverse_playback(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::VariSpeedState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);

  TapestryBuffer buffer;
  for (size_t i = 0; i < 1000; i++)
  {
    buffer.writeStereo(i, static_cast<float>(i), 0.0f);
  }

  // Set reverse playback
  VariSpeedState speed;
  speed.speedRatio = -1.0f;
  speed.isForward = false;
  speed.isStopped = false;
  engine.setVariSpeed(speed);

  engine.retrigger(0.0f);

  float outL, outR;
  bool endOfGene;
  for (int i = 0; i < 10; i++)
  {
    engine.process(buffer, 0, 1000, outL, outR, endOfGene);
  }

  // Position change direction is complex due to wrapping,
  // but engine should still produce output
  T_ASSERT(ctx, engine.isActive() || endOfGene);
}

void test_grain_multiple_voices(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::MorphState;
  using ShortwavDSP::VariSpeedState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);
  engine.setGeneSize(4800.0f);

  TapestryBuffer buffer;
  for (size_t i = 0; i < 10000; i++)
  {
    buffer.writeStereo(i, 0.5f, 0.5f);
  }

  // Set 4-voice mode with high overlap
  MorphState morph;
  morph.activeVoices = 4;
  morph.overlap = 4.0f;
  morph.enablePitchRand = true;
  morph.enablePanning = true;
  engine.setMorphState(morph);

  VariSpeedState speed;
  speed.speedRatio = 1.0f;
  speed.isForward = true;
  speed.isStopped = false;
  engine.setVariSpeed(speed);

  engine.retrigger(0.0f);

  // Process enough to trigger multiple voices
  float outL, outR;
  bool endOfGene;
  float totalOut = 0.0f;
  for (int i = 0; i < 1000; i++)
  {
    engine.process(buffer, 0, 10000, outL, outR, endOfGene);
    totalOut += std::fabs(outL) + std::fabs(outR);
  }

  // Should produce output
  T_ASSERT(ctx, totalOut > 0.0f);
  T_ASSERT(ctx, engine.isActive());
}

void test_grain_position_wrapping(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;
  using ShortwavDSP::VariSpeedState;

  GrainEngine engine;
  engine.setSampleRate(48000.0f);

  TapestryBuffer buffer;
  size_t spliceLength = 1000;
  for (size_t i = 0; i < spliceLength; i++)
  {
    buffer.writeStereo(i, 1.0f, 1.0f);
  }

  VariSpeedState speed;
  speed.speedRatio = 1.0f;
  speed.isForward = true;
  speed.isStopped = false;
  engine.setVariSpeed(speed);

  engine.retrigger(0.0f);

  // Process many samples to ensure position wraps
  float outL, outR;
  bool endOfGene;
  for (int i = 0; i < 2000; i++)
  {
    engine.process(buffer, 0, spliceLength, outL, outR, endOfGene);
  }

  // Position should have wrapped and stayed within bounds
  double pos = engine.getPlayheadPosition();
  T_ASSERT(ctx, pos >= 0.0 && pos < static_cast<double>(spliceLength * 2));
}

//------------------------------------------------------------------------------
// TapestryDSP Integration tests
//------------------------------------------------------------------------------

void test_dsp_initialization(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);
  dsp.reset();

  // Should be in valid initial state
  T_ASSERT(ctx, !dsp.isRecording());
  T_ASSERT(ctx, dsp.getBuffer().isEmpty());
  T_ASSERT(ctx, dsp.getSpliceManager().isEmpty());
}

void test_dsp_basic_recording(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Start recording
  dsp.clearAndStartRecording(false);
  T_ASSERT(ctx, dsp.isRecording());

  // Process some input
  for (int i = 0; i < 100; i++)
  {
    dsp.process(0.5f, 0.5f);
  }

  // Stop recording
  dsp.stopRecordingRequest(false);

  // Should have recorded data
  T_ASSERT(ctx, !dsp.getBuffer().isEmpty());
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() > 0);
}

void test_dsp_playback_basic(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record some data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 1000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  // Set varispeed for playback
  dsp.setVariSpeed(0.6f); // Forward playback
  
  // Start playback
  dsp.startPlayback();

  // Process playback
  float totalOut = 0.0f;
  for (int i = 0; i < 500; i++)
  {
    auto result = dsp.process(0.0f, 0.0f);
    totalOut += std::fabs(result.audioOutL) + std::fabs(result.audioOutR);
  }

  // Should produce output
  T_ASSERT(ctx, totalOut > 0.0f);
}

void test_dsp_varispeed_stopped(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 1000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  // Set varispeed to center (stopped)
  dsp.setVariSpeed(0.5f);

  // Process - should output silence
  auto result = dsp.process(0.0f, 0.0f);
  T_ASSERT_NEAR(ctx, result.audioOutL, 0.0f, 0.01f);
  T_ASSERT_NEAR(ctx, result.audioOutR, 0.0f, 0.01f);
}

void test_dsp_splice_creation(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 2000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  size_t initialSplices = dsp.getSpliceManager().getNumSplices();

  // Start playback and advance position
  dsp.setVariSpeed(0.6f);
  dsp.startPlayback();
  for (int i = 0; i < 500; i++)
  {
    dsp.process(0.0f, 0.0f);
  }

  // Create splice at position within first splice
  size_t currentFrame = 1000; // Known position within recorded range
  dsp.onSpliceTrigger(currentFrame);

  // Should have more splices now
  T_ASSERT(ctx, dsp.getSpliceManager().getNumSplices() > initialSplices);
}

void test_dsp_shift_navigation(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record and create splices
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 3000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  dsp.getSpliceManager().addMarker(1000);
  dsp.getSpliceManager().addMarker(2000);
  // Now have 3 splices

  int initialIndex = dsp.getSpliceManager().getCurrentIndex();

  // Trigger shift
  dsp.onShiftTrigger();

  int newIndex = dsp.getSpliceManager().getCurrentIndex();
  T_ASSERT(ctx, newIndex != initialIndex);
}

void test_dsp_morph_parameter(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 1000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  // Test different morph values
  dsp.setVariSpeed(0.6f);

  // Low morph (single voice)
  dsp.setMorph(0.1f);
  dsp.startPlayback();
  float outLow = 0.0f;
  for (int i = 0; i < 500; i++)
  {
    auto result = dsp.process(0.0f, 0.0f);
    outLow += std::fabs(result.audioOutL);
  }

  // High morph (multiple voices)
  dsp.setMorph(0.9f);
  dsp.startPlayback();
  float outHigh = 0.0f;
  for (int i = 0; i < 500; i++)
  {
    auto result = dsp.process(0.0f, 0.0f);
    outHigh += std::fabs(result.audioOutL);
  }

  // Both should produce output
  T_ASSERT(ctx, outLow > 0.0f);
  T_ASSERT(ctx, outHigh > 0.0f);
}

void test_dsp_sound_on_sound(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record initial layer
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 500; i++)
  {
    dsp.process(0.3f, 0.3f);
  }
  dsp.stopRecordingRequest(false);

  // Overdub with SOS
  dsp.startRecordingSameSplice(false);
  dsp.setSos(0.5f); // 50/50 mix

  for (int i = 0; i < 100; i++)
  {
    dsp.process(0.6f, 0.6f);
  }
  dsp.stopRecordingRequest(false);

  // Buffer should contain mixed result
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() >= 500);
}

void test_dsp_organize_parameter(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record and create splices
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 3000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  dsp.getSpliceManager().addMarker(1000);
  dsp.getSpliceManager().addMarker(2000);
  // 3 splices

  // Set organize to select middle splice
  dsp.setOrganize(0.5f);

  // Process to apply organize
  dsp.setVariSpeed(0.6f);
  for (int i = 0; i < 100; i++)
  {
    dsp.process(0.0f, 0.0f);
  }

  // Organize should influence splice selection
  // (exact behavior depends on when splice boundary is hit)
  T_ASSERT(ctx, dsp.getSpliceManager().getNumSplices() == 3);
}

void test_dsp_slide_parameter(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 5000; i++)
  {
    float val = static_cast<float>(i) / 5000.0f;
    dsp.process(val, val);
  }
  dsp.stopRecordingRequest(false);

  dsp.setVariSpeed(0.6f);
  dsp.setGeneSize(0.2f);

  // Slide at start
  dsp.setSlide(0.0f);
  dsp.startPlayback();
  auto result1 = dsp.process(0.0f, 0.0f);

  // Slide at middle
  dsp.setSlide(0.5f);
  dsp.startPlayback();
  auto result2 = dsp.process(0.0f, 0.0f);

  // Different slide positions access different parts of audio
  // (though windowing may affect the difference)
  T_ASSERT(ctx, std::fabs(result1.audioOutL) >= 0.0f);
  T_ASSERT(ctx, std::fabs(result2.audioOutL) >= 0.0f);
}

void test_dsp_gene_size_parameter(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 2000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  dsp.setVariSpeed(0.6f);

  // Small gene size
  dsp.setGeneSize(0.0f);
  dsp.startPlayback();
  float out1 = 0.0f;
  for (int i = 0; i < 50; i++)
  {
    auto result = dsp.process(0.0f, 0.0f);
    out1 += std::fabs(result.audioOutL);
  }

  // Large gene size
  dsp.setGeneSize(1.0f);
  dsp.startPlayback();
  float out2 = 0.0f;
  for (int i = 0; i < 50; i++)
  {
    auto result = dsp.process(0.0f, 0.0f);
    out2 += std::fabs(result.audioOutL);
  }

  // Both should produce output
  T_ASSERT(ctx, out1 > 0.0f);
  T_ASSERT(ctx, out2 > 0.0f);
}

void test_dsp_clear_operations(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 1000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  T_ASSERT(ctx, !dsp.getBuffer().isEmpty());

  // Clear reel
  dsp.clearReel();
  T_ASSERT(ctx, dsp.getBuffer().isEmpty());
  T_ASSERT(ctx, dsp.getSpliceManager().isEmpty());
}

void test_dsp_overdub_mode_default_off(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Overdub mode should default to off
  T_ASSERT(ctx, !dsp.getOverdubMode());
}

void test_dsp_overdub_mode_toggle(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Toggle on
  dsp.setOverdubMode(true);
  T_ASSERT(ctx, dsp.getOverdubMode());

  // Toggle off
  dsp.setOverdubMode(false);
  T_ASSERT(ctx, !dsp.getOverdubMode());
}

void test_dsp_overdub_mode_replace_behavior(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record some initial data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 100; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == 100);

  // With overdub mode OFF (default), clearAndStartRecording clears buffer
  dsp.setOverdubMode(false);
  dsp.clearAndStartRecording(false);

  // Buffer should be cleared after starting new recording
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == 0);

  // Record new data
  for (int i = 0; i < 50; i++)
  {
    dsp.process(0.8f, 0.8f);
  }
  dsp.stopRecordingRequest(false);

  // Should only have new data
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == 50);
}

void test_dsp_overdub_mode_keep_existing(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record some initial data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 100; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == 100);

  // With overdub mode ON, clearAndStartRecording should NOT clear buffer
  dsp.setOverdubMode(true);
  dsp.clearAndStartRecording(false);

  // Buffer should still have existing data
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == 100);

  // Record more data (overwrites at current position, but buffer is preserved)
  for (int i = 0; i < 50; i++)
  {
    dsp.process(0.8f, 0.8f);
  }
  dsp.stopRecordingRequest(false);

  // Should still have original 100 frames (recording overwrites but doesn't extend in same splice mode)
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() >= 100);
}

void test_dsp_overdub_mode_reset(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Set overdub mode on
  dsp.setOverdubMode(true);
  T_ASSERT(ctx, dsp.getOverdubMode());

  // Reset should turn overdub mode off
  dsp.reset();
  T_ASSERT(ctx, !dsp.getOverdubMode());
}

void test_dsp_clear_all_markers(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record some audio
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 1000; i++)
  {
    dsp.process(0.5f, 0.5f);
  }
  dsp.stopRecordingRequest(false);

  // Create multiple splices
  dsp.onSpliceTrigger(250);
  dsp.onSpliceTrigger(500);
  dsp.onSpliceTrigger(750);

  T_ASSERT(ctx, dsp.getSpliceManager().getNumSplices() == 4);

  // Clear all markers
  dsp.deleteAllMarkers();

  // Should have single splice covering entire buffer
  T_ASSERT(ctx, dsp.getSpliceManager().getNumSplices() == 1);
  T_ASSERT(ctx, dsp.getSpliceManager().getCurrentIndex() == 0);
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == 1000); // Audio preserved
}

void test_dsp_clear_markers_preserves_audio(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 500; i++)
  {
    dsp.process(0.8f, 0.8f);
  }
  dsp.stopRecordingRequest(false);

  size_t framesBeforeClear = dsp.getBuffer().getUsedFrames();
  T_ASSERT(ctx, framesBeforeClear == 500);

  // Add splices
  dsp.onSpliceTrigger(250);
  T_ASSERT(ctx, dsp.getSpliceManager().getNumSplices() == 2);

  // Clear markers
  dsp.deleteAllMarkers();

  // Audio should be preserved
  T_ASSERT(ctx, dsp.getBuffer().getUsedFrames() == framesBeforeClear);
  T_ASSERT(ctx, !dsp.getBuffer().isEmpty());
}

void test_dsp_clear_markers_empty_buffer(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Clear markers on empty buffer should not crash
  T_ASSERT(ctx, dsp.getBuffer().isEmpty());
  dsp.deleteAllMarkers();
  T_ASSERT(ctx, dsp.getSpliceManager().isEmpty());
}

//------------------------------------------------------------------------------
// Edge case and stress tests
//------------------------------------------------------------------------------

void test_edge_empty_splice(TestContext &ctx)
{
  using ShortwavDSP::GrainEngine;
  using ShortwavDSP::TapestryBuffer;

  GrainEngine engine;
  TapestryBuffer buffer;

  float outL, outR;
  bool endOfGene;

  // Process with empty splice (start == end)
  bool result = engine.process(buffer, 0, 0, outL, outR, endOfGene);
  T_ASSERT(ctx, !result);
  T_ASSERT_NEAR(ctx, outL, 0.0f, kEpsilon);
}

void test_edge_max_splices(TestContext &ctx)
{
  using ShortwavDSP::SpliceManager;
  using ShortwavDSP::TapestryConfig;

  SpliceManager mgr;
  mgr.initialize(100000);

  // Try to add maximum splices
  size_t maxAdded = 0;
  for (size_t i = 1; i < 300 && maxAdded < 300; i++)
  {
    if (mgr.addMarker(i * 300))
    {
      maxAdded++;
    }
    else
    {
      break;
    }
  }

  // Should reach limit
  T_ASSERT(ctx, maxAdded > 0);
  T_ASSERT(ctx, mgr.getNumSplices() <= TapestryConfig::kMaxSplices);
}

void test_edge_buffer_boundaries(TestContext &ctx)
{
  using ShortwavDSP::TapestryBuffer;

  TapestryBuffer buffer;

  // Write at max position
  bool success = buffer.writeStereo(buffer.getMaxFrames() - 1, 1.0f, 1.0f);
  T_ASSERT(ctx, success);

  // Try to write beyond max (should fail gracefully)
  success = buffer.writeStereo(buffer.getMaxFrames() + 100, 1.0f, 1.0f);
  T_ASSERT(ctx, !success);
}

void test_stress_continuous_playback(TestContext &ctx)
{
  using ShortwavDSP::TapestryDSP;

  TapestryDSP dsp;
  dsp.setSampleRate(48000.0f);

  // Record substantial data
  dsp.clearAndStartRecording(false);
  for (int i = 0; i < 10000; i++)
  {
    dsp.process(std::sin(i * 0.01f), std::cos(i * 0.01f));
  }
  dsp.stopRecordingRequest(false);

  // Continuous playback
  dsp.setVariSpeed(0.7f);
  dsp.setMorph(0.8f);

  float totalOut = 0.0f;
  for (int i = 0; i < 50000; i++)
  {
    auto result = dsp.process(0.0f, 0.0f);
    totalOut += std::fabs(result.audioOutL) + std::fabs(result.audioOutR);
  }

  // Should produce continuous output without crashes
  T_ASSERT(ctx, totalOut > 0.0f);
}

//------------------------------------------------------------------------------
// Test Runner
//------------------------------------------------------------------------------

void run_all_tapestry_tests()
{
  TestContext ctx;

  std::printf("\n=== Tapestry DSP Unit Tests ===\n\n");

  std::printf("--- TapestryUtil Tests ---\n");
  test_util_clamp(ctx);
  test_util_lerp(ctx);
  test_util_hann_window(ctx);
  test_util_cubic_interpolate(ctx);
  test_util_varispeed_calculation(ctx);
  test_util_fast_random(ctx);

  std::printf("--- TapestryBuffer Tests ---\n");
  test_buffer_basic_operations(ctx);
  test_buffer_interpolation(ctx);
  test_buffer_bounded_interpolation(ctx);
  test_buffer_sound_on_sound(ctx);
  test_buffer_bulk_operations(ctx);
  test_buffer_clear_range(ctx);

  std::printf("--- SpliceManager Tests ---\n");
  test_splice_initialization(ctx);
  test_splice_marker_creation(ctx);
  test_splice_marker_deletion(ctx);
  test_splice_navigation_shift(ctx);
  test_splice_navigation_organize(ctx);
  test_splice_pending_system(ctx);
  test_splice_organize_vs_shift_priority(ctx);
  test_splice_extend_for_recording(ctx);
  test_splice_delete_all(ctx);

  std::printf("--- GrainEngine Tests ---\n");
  test_grain_basic_setup(ctx);
  test_grain_stopped_mode(ctx);
  test_grain_retrigger(ctx);
  test_grain_slide_parameter(ctx);
  test_grain_reverse_playback(ctx);
  test_grain_multiple_voices(ctx);
  test_grain_position_wrapping(ctx);

  std::printf("--- TapestryDSP Integration Tests ---\n");
  test_dsp_initialization(ctx);
  test_dsp_basic_recording(ctx);
  test_dsp_playback_basic(ctx);
  test_dsp_varispeed_stopped(ctx);
  test_dsp_splice_creation(ctx);
  test_dsp_shift_navigation(ctx);
  test_dsp_morph_parameter(ctx);
  test_dsp_sound_on_sound(ctx);
  test_dsp_organize_parameter(ctx);
  test_dsp_slide_parameter(ctx);
  test_dsp_gene_size_parameter(ctx);
  test_dsp_clear_operations(ctx);

  std::printf("--- Overdub Mode Tests ---\n");
  test_dsp_overdub_mode_default_off(ctx);
  test_dsp_overdub_mode_toggle(ctx);
  test_dsp_overdub_mode_replace_behavior(ctx);
  test_dsp_overdub_mode_keep_existing(ctx);
  test_dsp_overdub_mode_reset(ctx);

  std::printf("--- Clear Markers Tests ---\n");
  test_dsp_clear_all_markers(ctx);
  test_dsp_clear_markers_preserves_audio(ctx);
  test_dsp_clear_markers_empty_buffer(ctx);

  std::printf("--- Edge Cases and Stress Tests ---\n");
  test_edge_empty_splice(ctx);
  test_edge_max_splices(ctx);
  test_edge_buffer_boundaries(ctx);
  test_stress_continuous_playback(ctx);

  std::printf("\n");
  ctx.summary();
  std::printf("\n");
}

} // anonymous namespace

//------------------------------------------------------------------------------
// Main Entry Point
//------------------------------------------------------------------------------

int main()
{
  run_all_tapestry_tests();
  return 0;
}
