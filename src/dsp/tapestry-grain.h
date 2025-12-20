#pragma once

#include "tapestry-core.h"
#include "tapestry-buffer.h"
#include <array>

/*
 * Tapestry Grain Engine
 *
 * Multi-voice granular synthesis engine for the Tapestry.
 * Implements Gene-Size, Slide, Morph, and time stretch functionality.
 *
 * Features:
 * - Up to 4 overlapping grain voices
 * - Hann windowing for smooth transitions
 * - Clock-synced granulation (Gene Shift / Time Stretch)
 * - Pitch randomization and stereo panning for high Morph values
 */

namespace ShortwavDSP
{

class GrainEngine
{
public:
  static constexpr int kMaxVoices = TapestryConfig::kMaxGrainVoices;

  GrainEngine()
  {
    reset();
  }

  //--------------------------------------------------------------------------
  // Configuration
  //--------------------------------------------------------------------------

  void setSampleRate(float sampleRate) noexcept
  {
    sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    sampleRateRatio_ = TapestryConfig::kInternalSampleRate / sampleRate_;
  }

  void reset() noexcept
  {
    for (auto &voice : voices_)
    {
      voice.reset();
    }
    currentVoice_ = 0;
    grainPhase_ = 0.0f;
    lastClockTime_ = 0.0f;
    clockPeriodSamples_ = 0.0f;
    isClockSynced_ = false;
    timeStretchMode_ = false;
    totalSamplesProcessed_ = 0;
  }

  //--------------------------------------------------------------------------
  // Parameter Setters
  //--------------------------------------------------------------------------

  void setGeneSize(float geneSizeSamples) noexcept
  {
    geneSizeSamples_ = std::max(TapestryConfig::kMinGeneSamples, geneSizeSamples);
  }

  void setMorphState(const MorphState &state) noexcept
  {
    morphState_ = state;
  }

  void setSlide(float slide) noexcept
  {
    slide_ = TapestryUtil::clamp01(slide);
  }

  void setVariSpeed(const VariSpeedState &state) noexcept
  {
    variSpeedState_ = state;
  }

  //--------------------------------------------------------------------------
  // Clock Sync
  //--------------------------------------------------------------------------

  void onClockRising() noexcept
  {
    float currentTime = static_cast<float>(totalSamplesProcessed_);
    if (lastClockTime_ > 0.0f)
    {
      clockPeriodSamples_ = currentTime - lastClockTime_;
    }
    lastClockTime_ = currentTime;
    isClockSynced_ = true;

    // Determine mode based on Morph setting
    // Time Stretch: Morph > ~0.5 (2/1 overlap)
    // Gene Shift: Morph < ~0.5
    timeStretchMode_ = morphState_.overlap > 2.0f;

    if (!timeStretchMode_)
    {
      // Gene Shift: advance to next gene immediately on clock
      triggerNextGene();
    }
  }

  void setClockDisconnected() noexcept
  {
    isClockSynced_ = false;
    timeStretchMode_ = false;
  }

  bool isTimeStretchMode() const noexcept { return timeStretchMode_; }
  bool isClockSynced() const noexcept { return isClockSynced_; }

  //--------------------------------------------------------------------------
  // Main Processing
  //--------------------------------------------------------------------------

  // Process one sample frame
  // spliceStart/End: bounds of current splice in buffer
  // Returns true if end of gene/splice was reached
  bool process(const TapestryBuffer &buffer,
               size_t spliceStart, size_t spliceEnd,
               float &outL, float &outR,
               bool &endOfGene) noexcept
  {
    outL = outR = 0.0f;
    endOfGene = false;

    if (spliceEnd <= spliceStart)
      return false;

    size_t spliceLength = spliceEnd - spliceStart;
    float geneSamples = std::min(geneSizeSamples_, static_cast<float>(spliceLength));

    // Calculate slide offset within splice
    float slideOffset = slide_ * static_cast<float>(spliceLength - geneSamples);

    // Get playback speed (can be negative for reverse)
    float speed = variSpeedState_.speedRatio * sampleRateRatio_;
    
    // Update absolute position based on slide (even when stopped, for splice creation)
    updateAbsolutePosition(spliceStart, slideOffset, spliceLength);
    
    if (variSpeedState_.isStopped)
    {
      // When stopped, update position based on slide but output silence
      return false;
    }

    // Process each active voice
    int numVoices = morphState_.activeVoices;
    float voiceGain = 1.0f / std::sqrt(static_cast<float>(numVoices)); // Normalize

    for (int v = 0; v < numVoices && v < kMaxVoices; v++)
    {
      GrainVoice &voice = voices_[v];

      if (!voice.active)
        continue;

      // Calculate window amplitude
      float window = TapestryUtil::hannWindow(voice.phase);
      voice.amplitude = window;

      // Read from buffer at voice position
      float sampleL, sampleR;
      double readPos = static_cast<double>(spliceStart) + slideOffset + voice.position;

      // Wrap within splice bounds
      double relPos = readPos - static_cast<double>(spliceStart);
      while (relPos < 0.0)
        relPos += static_cast<double>(spliceLength);
      while (relPos >= static_cast<double>(spliceLength))
        relPos -= static_cast<double>(spliceLength);
      readPos = static_cast<double>(spliceStart) + relPos;

      buffer.readStereoInterpolatedBounded(readPos, spliceStart, spliceEnd, sampleL, sampleR);

      // Apply pitch modulation (for high Morph)
      float pitchMod = voice.pitchMod;

      // Apply window and gain
      sampleL *= window * voiceGain;
      sampleR *= window * voiceGain;

      // Apply panning (for high Morph)
      if (morphState_.enablePanning && numVoices > 2)
      {
        float pan = voice.pan;
        float panL = std::cos((pan + 1.0f) * 0.25f * 3.14159265359f);
        float panR = std::sin((pan + 1.0f) * 0.25f * 3.14159265359f);
        float mono = (sampleL + sampleR) * 0.5f;
        sampleL = mono * panL;
        sampleR = mono * panR;
      }

      outL += sampleL;
      outR += sampleR;

      // Advance voice position (speed can be negative for reverse playback)
      voice.position += speed * pitchMod;
      voice.phase += std::fabs(speed) / geneSamples;

      // Wrap voice.position to prevent unbounded growth
      // This keeps it synchronized with the actual read position
      while (voice.position < 0.0)
        voice.position += static_cast<double>(spliceLength);
      while (voice.position >= static_cast<double>(spliceLength))
        voice.position -= static_cast<double>(spliceLength);

      // Check if voice reached end of gene
      if (voice.phase >= 1.0f)
      {
        voice.active = false;
        endOfGene = true;
      }
    }

    // Trigger new grains based on Morph overlap
    updateGrainTriggers(geneSamples, speed);

    totalSamplesProcessed_++;
    return endOfGene;
  }

  //--------------------------------------------------------------------------
  // Gene/Playhead Management
  //--------------------------------------------------------------------------

  // Get current playhead position (relative to splice, for internal use)
  double getPlayheadPositionRelative() const noexcept
  {
    // Return position of most recent voice
    for (int i = 0; i < kMaxVoices; i++)
    {
      int idx = (currentVoice_ - i + kMaxVoices) % kMaxVoices;
      if (voices_[idx].active)
      {
        return voices_[idx].position;
      }
    }
    return grainStartPosition_;
  }

  // Get current playhead position as absolute buffer frame
  double getPlayheadPosition() const noexcept
  {
    return lastAbsolutePosition_;
  }

  // Set absolute position directly (for visual feedback when stopped)
  void setAbsolutePosition(double pos) noexcept
  {
    lastAbsolutePosition_ = pos;
    // Also reset the relative position so they stay in sync
    grainStartPosition_ = 0.0;
    for (auto& voice : voices_)
    {
      voice.position = 0.0;
    }
  }

  // Update the last known absolute position (call from process when we know splice bounds)
  void updateAbsolutePosition(size_t spliceStart, float slideOffset, size_t spliceLength) noexcept
  {
    double relPos = getPlayheadPositionRelative();
    double absPos = static_cast<double>(spliceStart) + slideOffset + relPos;
    
    // Wrap to splice bounds to prevent display issues
    double minPos = static_cast<double>(spliceStart);
    double maxPos = static_cast<double>(spliceStart + spliceLength);
    while (absPos < minPos)
      absPos += static_cast<double>(spliceLength);
    while (absPos >= maxPos)
      absPos -= static_cast<double>(spliceLength);
    
    lastAbsolutePosition_ = absPos;
  }

  // Retrigger playback from start (Play input)
  void retrigger(float slideOffset = 0.0f) noexcept
  {
    // Reset all voices
    for (auto &voice : voices_)
    {
      voice.reset();
    }
    currentVoice_ = 0;
    grainPhase_ = 0.0f;
    grainStartPosition_ = 0.0;

    // Start first voice
    triggerVoice(0, slideOffset);
  }

  // Check if any voice is active
  bool isActive() const noexcept
  {
    for (const auto &voice : voices_)
    {
      if (voice.active)
        return true;
    }
    return false;
  }

private:
  //--------------------------------------------------------------------------
  // Internal Methods
  //--------------------------------------------------------------------------

  void triggerNextGene() noexcept
  {
    // Advance grain start position by gene size
    grainStartPosition_ += geneSizeSamples_;
    grainPhase_ = 0.0f;

    // Trigger new voice
    triggerVoice(currentVoice_, 0.0f);
    currentVoice_ = (currentVoice_ + 1) % kMaxVoices;
  }

  void triggerVoice(int voiceIdx, float positionOffset) noexcept
  {
    if (voiceIdx < 0 || voiceIdx >= kMaxVoices)
      return;

    GrainVoice &voice = voices_[voiceIdx];
    voice.position = grainStartPosition_ + positionOffset;
    voice.phase = 0.0f;
    voice.amplitude = 0.0f;
    voice.active = true;

    // Set pitch randomization if enabled
    if (morphState_.enablePitchRand)
    {
      // Randomize pitch up to +1 octave
      float randPitch = rng_.nextRange(1.0f, 2.0f);
      voice.pitchMod = randPitch;
    }
    else
    {
      voice.pitchMod = 1.0f;
    }

    // Set panning if enabled
    if (morphState_.enablePanning)
    {
      voice.pan = rng_.nextBipolar();
    }
    else
    {
      voice.pan = 0.0f;
    }
  }

  void updateGrainTriggers(float geneSamples, float speed) noexcept
  {
    // Calculate grain trigger interval based on overlap
    float overlap = morphState_.overlap;

    if (overlap <= 0.0f)
    {
      // Gap mode: only one voice, with silence between genes
      if (!voices_[0].active)
      {
        // Add gap delay
        grainPhase_ += std::fabs(speed) / geneSamples;
        float gapFactor = 1.0f - overlap; // overlap < 0 means gap
        if (grainPhase_ >= gapFactor)
        {
          grainPhase_ = 0.0f;
          triggerNextGene();
        }
      }
      return;
    }

    // Calculate trigger interval for overlapping voices
    // overlap = 1: trigger at end of each grain (seamless)
    // overlap = 2: trigger at 50% of grain (2 voices)
    // overlap = 3: trigger at 33% of grain (3 voices)
    float triggerInterval = 1.0f / overlap;

    // Track phase for grain triggering
    grainPhase_ += std::fabs(speed) / geneSamples;

    if (grainPhase_ >= triggerInterval)
    {
      grainPhase_ -= triggerInterval;

      // Find next available voice
      int nextVoice = (currentVoice_ + 1) % kMaxVoices;
      for (int i = 0; i < kMaxVoices; i++)
      {
        int idx = (nextVoice + i) % kMaxVoices;
        if (!voices_[idx].active)
        {
          // Calculate position offset for this voice
          float offset = (1.0f - grainPhase_) * geneSamples;
          triggerVoice(idx, offset);
          currentVoice_ = idx;
          break;
        }
      }
    }
  }

  //--------------------------------------------------------------------------
  // State
  //--------------------------------------------------------------------------

  float sampleRate_ = 48000.0f;
  float sampleRateRatio_ = 1.0f;

  std::array<GrainVoice, kMaxVoices> voices_;
  int currentVoice_ = 0;

  float geneSizeSamples_ = 48000.0f; // Default 1 second
  float slide_ = 0.0f;
  MorphState morphState_;
  VariSpeedState variSpeedState_;

  double grainStartPosition_ = 0.0;
  float grainPhase_ = 0.0f;
  double lastAbsolutePosition_ = 0.0;  // Absolute buffer position for splice creation

  // Clock sync state
  float lastClockTime_ = 0.0f;
  float clockPeriodSamples_ = 0.0f;
  bool isClockSynced_ = false;
  bool timeStretchMode_ = false;

  size_t totalSamplesProcessed_ = 0;

  TapestryUtil::FastRandom rng_;
};

} // namespace ShortwavDSP
