#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

/*
 * Tapestry Core Types and Configuration
 *
 * Core data structures and configuration constants for the Tapestry
 * VCV Rack module implementation. Based on the Make Noise Tapestry
 * hardware specifications.
 *
 * Features:
 * - Reels: Audio buffers up to 2.9 minutes @ 48kHz stereo
 * - Splices: Up to 300 markers per reel
 * - Genes: Granular particles with overlap control
 * - Vari-Speed: Bipolar speed/direction control
 * - Sound On Sound: Crossfade recording
 * - Time Stretch: Clock-synced granular playback
 *
 * Design principles:
 * - Real-time safe (no allocations in audio path after init)
 * - Header-only core types for easy inclusion
 * - Clear separation of concerns between components
 */

namespace ShortwavDSP
{

//------------------------------------------------------------------------------
// Configuration Constants
//------------------------------------------------------------------------------

struct TapestryConfig
{
  // Audio specifications (matching hardware)
  static constexpr float kInternalSampleRate = 48000.0f;
  static constexpr size_t kMaxReelFrames = 8352000;  // ~2.9 minutes stereo @ 48kHz
  static constexpr size_t kMaxSplices = 300;
  static constexpr size_t kMaxGrainVoices = 4;
  static constexpr size_t kMaxReels = 32;

  // Gene size limits (in samples at 48kHz)
  static constexpr float kMinGeneSamples = 48.0f;     // ~1ms minimum
  static constexpr float kMaxGeneSamples = 8352000.0f; // Full reel

  // CV voltage ranges
  static constexpr float kSosCvMax = 8.0f;            // 0-8V unipolar
  static constexpr float kGeneSizeCvMax = 8.0f;       // ±8V bipolar
  static constexpr float kVariSpeedCvMax = 4.0f;      // ±4V bipolar
  static constexpr float kMorphCvMax = 5.0f;          // 0-5V unipolar
  static constexpr float kSlideCvMax = 8.0f;          // 0-8V unipolar
  static constexpr float kOrganizeCvMax = 5.0f;       // 0-5V unipolar
  static constexpr float kGateTriggerThreshold = 2.5f; // Gate threshold

  // Vari-speed range (semitones)
  static constexpr float kVariSpeedUpSemitones = 12.0f;   // +1 octave
  static constexpr float kVariSpeedDownSemitones = 26.0f; // ~-2.2 octaves

  // Output voltage levels
  static constexpr float kAudioOutLevel = 5.0f;       // ±5V audio
  static constexpr float kCvOutMax = 8.0f;            // 0-8V envelope
  static constexpr float kGateOutLevel = 10.0f;       // 0-10V gates
};

//------------------------------------------------------------------------------
// Splice Marker
//------------------------------------------------------------------------------

struct SpliceMarker
{
  size_t startFrame;
  size_t endFrame;

  SpliceMarker() : startFrame(0), endFrame(0) {}
  SpliceMarker(size_t start, size_t end) : startFrame(start), endFrame(end) {}

  size_t length() const noexcept
  {
    return (endFrame > startFrame) ? (endFrame - startFrame) : 0;
  }

  bool isValid() const noexcept
  {
    return endFrame > startFrame;
  }
};

//------------------------------------------------------------------------------
// Grain Voice State
//------------------------------------------------------------------------------

struct GrainVoice
{
  double position = 0.0;       // Fractional sample position in buffer
  float phase = 0.0f;          // Envelope phase (0-1)
  float amplitude = 1.0f;      // Current envelope amplitude
  float pan = 0.0f;            // Stereo pan (-1 to +1)
  float pitchMod = 1.0f;       // Pitch randomization multiplier
  bool active = false;         // Voice is currently playing

  void reset() noexcept
  {
    position = 0.0;
    phase = 0.0f;
    amplitude = 1.0f;
    pan = 0.0f;
    pitchMod = 1.0f;
    active = false;
  }
};

//------------------------------------------------------------------------------
// Morph State (Gene Overlap Configuration)
//------------------------------------------------------------------------------

struct MorphState
{
  float overlap = 1.0f;        // Overlap ratio (0.0 to 4.0)
  int activeVoices = 1;        // Number of active grain voices (1-4)
  bool hasGaps = false;        // True when gaps between genes
  bool enablePanning = false;  // Enable stereo panning
  bool enablePitchRand = false; // Enable pitch randomization

  // LED indicator state
  bool isSeamless() const noexcept
  {
    return !hasGaps && overlap >= 0.95f && overlap <= 1.05f;
  }
};

//------------------------------------------------------------------------------
// Vari-Speed State
//------------------------------------------------------------------------------

struct VariSpeedState
{
  float speedRatio = 1.0f;     // Playback speed multiplier (can be negative)
  bool isForward = true;       // Direction of playback
  bool isStopped = false;      // True when speed is zero
  bool isAtUnity = false;      // True when at 1x speed
  int octaveShift = 0;         // Octave indicator (-2 to +1)

  // LED color indicators
  enum class LedColor
  {
    Red,       // Stopped
    Amber,     // Other speeds
    Green,     // Unity (1x)
    BabyBlue,  // Octave up
    Peach      // Octave down
  };

  LedColor getLedColor() const noexcept
  {
    if (isStopped) return LedColor::Red;
    if (isAtUnity) return LedColor::Green;
    if (octaveShift >= 1) return LedColor::BabyBlue;
    if (octaveShift <= -1) return LedColor::Peach;
    return LedColor::Amber;
  }
};

//------------------------------------------------------------------------------
// Playback State
//------------------------------------------------------------------------------

struct PlaybackState
{
  double playheadPosition = 0.0;  // Current fractional sample position
  int currentSplice = 0;          // Active splice index
  int pendingSplice = -1;         // Next splice (set by Organize, -1 = none)
  bool isPlaying = true;          // Playback active
  bool playGateHigh = true;       // Play input state (normalized HIGH)
};

//------------------------------------------------------------------------------
// Recording State
//------------------------------------------------------------------------------

struct RecordState
{
  enum class Mode
  {
    Idle,           // Not recording
    SameSplice,     // Time Lag Accumulation (TLA)
    NewSplice       // Recording into new splice
  };

  Mode mode = Mode::Idle;
  size_t recordPosition = 0;        // Current write position
  size_t recordStartFrame = 0;      // Start of current recording
  bool waitingForClock = false;     // Waiting for clock sync to start/stop
  bool isInitialRecording = false;  // True when recording into freshly created splice (extend, don't loop)
};

//------------------------------------------------------------------------------
// Module Operating Mode
//------------------------------------------------------------------------------

enum class ModuleMode
{
  Normal,           // Standard playback/record
  ReelSelect,       // Selecting reel from storage
  SdBusy            // Writing to storage (flash Shift LED)
};

//------------------------------------------------------------------------------
// Reel Color Cycle (for LED indicators)
//------------------------------------------------------------------------------

struct ReelColors
{
  // 8-color cycle matching hardware
  static constexpr int kNumColors = 8;

  struct RGB
  {
    uint8_t r, g, b;
  };

  static RGB getColor(int reelIndex) noexcept
  {
    static const RGB colors[kNumColors] = {
        {0, 0, 255},     // Blue
        {0, 255, 0},     // Green
        {128, 255, 0},   // Light green
        {255, 255, 0},   // Yellow
        {255, 128, 0},   // Orange
        {255, 0, 0},     // Red
        {255, 0, 128},   // Pink
        {255, 255, 255}  // White
    };
    return colors[reelIndex % kNumColors];
  }

  static void getRGBNormalized(int reelIndex, float &r, float &g, float &b) noexcept
  {
    RGB c = getColor(reelIndex);
    r = c.r / 255.0f;
    g = c.g / 255.0f;
    b = c.b / 255.0f;
  }
};

//------------------------------------------------------------------------------
// Utility Functions
//------------------------------------------------------------------------------

namespace TapestryUtil
{

// Clamp value to range
inline float clamp(float x, float min, float max) noexcept
{
  return std::max(min, std::min(max, x));
}

// Clamp to [0, 1]
inline float clamp01(float x) noexcept
{
  return clamp(x, 0.0f, 1.0f);
}

// Linear interpolation
inline float lerp(float a, float b, float t) noexcept
{
  return a + t * (b - a);
}

// Hann window function for grain envelopes
inline float hannWindow(float phase) noexcept
{
  // phase: 0.0 to 1.0 through grain
  return 0.5f * (1.0f - std::cos(2.0f * 3.14159265359f * phase));
}

// Cubic interpolation (Hermite spline)
// Returns interpolated value at position t (0..1) between y1 and y2
inline float cubicInterpolate(float y0, float y1, float y2, float y3, float t) noexcept
{
  const float t2 = t * t;
  const float t3 = t2 * t;

  const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
  const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
  const float a2 = -0.5f * y0 + 0.5f * y2;
  const float a3 = y1;

  return a0 * t3 + a1 * t2 + a2 * t + a3;
}

// Calculate gene size in samples from parameter (0-1)
// Exponential mapping: 0 = full splice, 1 = minimum gene
inline float calculateGeneSizeSamples(float param, float spliceLengthSamples) noexcept
{
  const float minGene = TapestryConfig::kMinGeneSamples;
  const float maxGene = std::max(minGene, spliceLengthSamples);

  // Invert: 0 = small (param=1), 1 = full (param=0)
  float normalized = 1.0f - clamp01(param);

  // Exponential curve for musical response
  const float exponent = 4.0f;
  return minGene + std::pow(normalized, exponent) * (maxGene - minGene);
}

// Calculate morph state from parameter (0-1)
inline MorphState calculateMorphState(float morphParam) noexcept
{
  MorphState state;
  morphParam = clamp01(morphParam);

  if (morphParam < 0.15f)
  {
    // Gap mode (pointillist)
    state.overlap = -0.5f + morphParam * 3.33f; // -0.5 to 0.0
    state.activeVoices = 1;
    state.hasGaps = true;
  }
  else if (morphParam < 0.35f)
  {
    // Transition to seamless
    state.overlap = (morphParam - 0.15f) * 5.0f; // 0.0 to 1.0
    state.activeVoices = 1;
    state.hasGaps = state.overlap < 0.5f;
  }
  else if (morphParam < 0.50f)
  {
    // Seamless to 2x overlap
    state.overlap = 1.0f + (morphParam - 0.35f) * 6.67f; // 1.0 to 2.0
    state.activeVoices = 2;
    state.hasGaps = false;
  }
  else if (morphParam < 0.70f)
  {
    // 2x overlap with panning
    state.overlap = 2.0f + (morphParam - 0.50f) * 5.0f; // 2.0 to 3.0
    state.activeVoices = 3;
    state.enablePanning = true;
  }
  else
  {
    // 3x to 4x overlap with pitch randomization
    state.overlap = 3.0f + (morphParam - 0.70f) * 3.33f; // 3.0 to 4.0
    state.activeVoices = 4;
    state.enablePanning = true;
    state.enablePitchRand = true;
  }

  return state;
}

// Calculate vari-speed state from parameter and CV
inline VariSpeedState calculateVariSpeed(float param, float cvInput, float cvAtten) noexcept
{
  VariSpeedState state;

  // Combine param (-1 to +1) with CV
  float combined = param + (cvInput / TapestryConfig::kVariSpeedCvMax) * cvAtten;
  combined = clamp(combined, -1.0f, 1.0f);

  // Dead zone around center for clean stop
  const float deadZone = 0.02f;
  if (std::fabs(combined) < deadZone)
  {
    state.speedRatio = 0.0f;
    state.isForward = true;
    state.isStopped = true;
    state.isAtUnity = false;
    state.octaveShift = 0;
    return state;
  }

  state.isStopped = false;
  state.isForward = combined > 0.0f;
  float absParam = std::fabs(combined);

  // Asymmetric range: more range slowing down than speeding up
  float semitones;
  if (state.isForward)
  {
    semitones = absParam * TapestryConfig::kVariSpeedUpSemitones;
  }
  else
  {
    semitones = absParam * TapestryConfig::kVariSpeedDownSemitones;
  }

  state.speedRatio = std::pow(2.0f, semitones / 12.0f);
  if (!state.isForward)
  {
    state.speedRatio = -state.speedRatio;
  }

  // Check for unity speed (±0.5 semitone tolerance)
  state.isAtUnity = semitones < 0.5f;
  state.octaveShift = static_cast<int>(std::round(semitones / 12.0f));

  return state;
}

// Simple deterministic LCG random number generator
class FastRandom
{
public:
  FastRandom(uint32_t seed = 0x1234567u) : state_(seed != 0 ? seed : 0x1234567u) {}

  void seed(uint32_t s) noexcept { state_ = s != 0 ? s : 0x1234567u; }

  // Returns value in [0, 1)
  float nextFloat() noexcept
  {
    state_ = state_ * 1664525u + 1013904223u;
    return static_cast<float>((state_ >> 8) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
  }

  // Returns value in [-1, 1)
  float nextBipolar() noexcept
  {
    return nextFloat() * 2.0f - 1.0f;
  }

  // Returns value in [min, max)
  float nextRange(float min, float max) noexcept
  {
    return min + nextFloat() * (max - min);
  }

private:
  uint32_t state_;
};

} // namespace TapestryUtil

} // namespace ShortwavDSP
