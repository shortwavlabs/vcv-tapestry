#pragma once

#include "tapestry-core.h"
#include <algorithm>
#include <cstring>
#include <vector>

/*
 * Tapestry Audio Buffer
 *
 * Pre-allocated stereo audio buffer for reel storage.
 * Supports up to 2.9 minutes of 48kHz stereo audio.
 *
 * Features:
 * - Pre-allocated maximum size for real-time safety
 * - Interleaved stereo storage [L0, R0, L1, R1, ...]
 * - Cubic interpolation for high-quality playback
 * - Lock-free read/write operations
 */

namespace ShortwavDSP
{

class TapestryBuffer
{
public:
  static constexpr size_t kMaxFrames = TapestryConfig::kMaxReelFrames;
  static constexpr size_t kChannels = 2;

  TapestryBuffer()
  {
    // Pre-allocate maximum size
    data_.resize(kMaxFrames * kChannels, 0.0f);
    clear();
  }

  //--------------------------------------------------------------------------
  // Buffer Management
  //--------------------------------------------------------------------------

  void clear() noexcept
  {
    std::fill(data_.begin(), data_.end(), 0.0f);
    usedFrames_ = 0;
  }

  void clearRange(size_t startFrame, size_t endFrame) noexcept
  {
    startFrame = std::min(startFrame, kMaxFrames);
    endFrame = std::min(endFrame, kMaxFrames);
    if (startFrame < endFrame)
    {
      std::fill(data_.begin() + startFrame * kChannels,
                data_.begin() + endFrame * kChannels, 0.0f);
    }
  }

  size_t getUsedFrames() const noexcept { return usedFrames_; }
  size_t getMaxFrames() const noexcept { return kMaxFrames; }
  bool isEmpty() const noexcept { return usedFrames_ == 0; }
  bool isFull() const noexcept { return usedFrames_ >= kMaxFrames; }

  float getDurationSeconds(float sampleRate = 48000.0f) const noexcept
  {
    return static_cast<float>(usedFrames_) / sampleRate;
  }

  //--------------------------------------------------------------------------
  // Sample Access (Non-interpolated)
  //--------------------------------------------------------------------------

  // Write stereo sample at frame index
  bool writeStereo(size_t frame, float left, float right) noexcept
  {
    if (frame >= kMaxFrames)
      return false;

    data_[frame * kChannels] = left;
    data_[frame * kChannels + 1] = right;

    // Track used frames
    if (frame >= usedFrames_)
    {
      usedFrames_ = frame + 1;
    }
    return true;
  }

  // Read stereo sample at frame index (no interpolation)
  void readStereo(size_t frame, float &outL, float &outR) const noexcept
  {
    if (frame >= usedFrames_ || usedFrames_ == 0)
    {
      outL = outR = 0.0f;
      return;
    }
    frame = std::min(frame, usedFrames_ - 1);
    outL = data_[frame * kChannels];
    outR = data_[frame * kChannels + 1];
  }

  // Direct pointer access (for bulk operations)
  float *data() noexcept { return data_.data(); }
  const float *data() const noexcept { return data_.data(); }

  //--------------------------------------------------------------------------
  // Interpolated Read (Cubic)
  //--------------------------------------------------------------------------

  void readStereoInterpolated(double position, float &outL, float &outR) const noexcept
  {
    if (usedFrames_ == 0)
    {
      outL = outR = 0.0f;
      return;
    }

    // Handle negative positions (wrap or clamp)
    while (position < 0.0)
    {
      position += static_cast<double>(usedFrames_);
    }

    // Wrap position within buffer
    while (position >= static_cast<double>(usedFrames_))
    {
      position -= static_cast<double>(usedFrames_);
    }

    size_t idx = static_cast<size_t>(position);
    float frac = static_cast<float>(position - static_cast<double>(idx));

    // Get 4 sample frames for cubic interpolation
    // Handle wrapping at buffer boundaries
    size_t maxIdx = usedFrames_ - 1;
    size_t i0 = (idx > 0) ? idx - 1 : maxIdx;
    size_t i1 = idx;
    size_t i2 = (idx < maxIdx) ? idx + 1 : 0;
    size_t i3 = (i2 < maxIdx) ? i2 + 1 : 0;

    // Left channel
    outL = TapestryUtil::cubicInterpolate(
        data_[i0 * kChannels],
        data_[i1 * kChannels],
        data_[i2 * kChannels],
        data_[i3 * kChannels],
        frac);

    // Right channel
    outR = TapestryUtil::cubicInterpolate(
        data_[i0 * kChannels + 1],
        data_[i1 * kChannels + 1],
        data_[i2 * kChannels + 1],
        data_[i3 * kChannels + 1],
        frac);
  }

  //--------------------------------------------------------------------------
  // Interpolated Read within Splice Bounds
  //--------------------------------------------------------------------------

  void readStereoInterpolatedBounded(double position, size_t startFrame, size_t endFrame,
                                     float &outL, float &outR) const noexcept
  {
    if (usedFrames_ == 0 || startFrame >= endFrame)
    {
      outL = outR = 0.0f;
      return;
    }

    endFrame = std::min(endFrame, usedFrames_);
    size_t length = endFrame - startFrame;

    // Wrap position within splice bounds
    double relPos = position - static_cast<double>(startFrame);
    while (relPos < 0.0)
    {
      relPos += static_cast<double>(length);
    }
    while (relPos >= static_cast<double>(length))
    {
      relPos -= static_cast<double>(length);
    }

    double absPos = static_cast<double>(startFrame) + relPos;
    size_t idx = static_cast<size_t>(absPos);
    float frac = static_cast<float>(absPos - static_cast<double>(idx));

    // Get 4 sample frames with wrapping within splice
    auto wrapInSplice = [startFrame, endFrame, length](size_t i) -> size_t {
      if (i < startFrame)
        return endFrame - (startFrame - i) % length;
      if (i >= endFrame)
        return startFrame + (i - startFrame) % length;
      return i;
    };

    size_t i0 = wrapInSplice((idx > 0) ? idx - 1 : endFrame - 1);
    size_t i1 = wrapInSplice(idx);
    size_t i2 = wrapInSplice(idx + 1);
    size_t i3 = wrapInSplice(idx + 2);

    // Left channel
    outL = TapestryUtil::cubicInterpolate(
        data_[i0 * kChannels],
        data_[i1 * kChannels],
        data_[i2 * kChannels],
        data_[i3 * kChannels],
        frac);

    // Right channel
    outR = TapestryUtil::cubicInterpolate(
        data_[i0 * kChannels + 1],
        data_[i1 * kChannels + 1],
        data_[i2 * kChannels + 1],
        data_[i3 * kChannels + 1],
        frac);
  }

  //--------------------------------------------------------------------------
  // Sound-On-Sound Mix and Write
  //--------------------------------------------------------------------------

  // Mix live input with existing buffer content and write back
  void mixAndWrite(size_t frame, float liveL, float liveR,
                   float sosAmount) noexcept
  {
    if (frame >= kMaxFrames)
      return;

    // sosAmount: 0 = live only, 1 = loop only
    float loopL = data_[frame * kChannels];
    float loopR = data_[frame * kChannels + 1];

    float mixL = liveL * (1.0f - sosAmount) + loopL * sosAmount;
    float mixR = liveR * (1.0f - sosAmount) + loopR * sosAmount;

    data_[frame * kChannels] = mixL;
    data_[frame * kChannels + 1] = mixR;

    if (frame >= usedFrames_)
    {
      usedFrames_ = frame + 1;
    }
  }

  //--------------------------------------------------------------------------
  // Bulk Operations
  //--------------------------------------------------------------------------

  // Copy from external buffer
  void copyFrom(const float *src, size_t numFrames, size_t destOffset = 0) noexcept
  {
    size_t framesToCopy = std::min(numFrames, kMaxFrames - destOffset);
    if (framesToCopy > 0 && src != nullptr)
    {
      std::memcpy(data_.data() + destOffset * kChannels,
                  src, framesToCopy * kChannels * sizeof(float));
      usedFrames_ = std::max(usedFrames_, destOffset + framesToCopy);
    }
  }

  // Copy to external buffer
  void copyTo(float *dest, size_t numFrames, size_t srcOffset = 0) const noexcept
  {
    size_t framesToCopy = std::min(numFrames, usedFrames_ - srcOffset);
    if (framesToCopy > 0 && dest != nullptr && srcOffset < usedFrames_)
    {
      std::memcpy(dest, data_.data() + srcOffset * kChannels,
                  framesToCopy * kChannels * sizeof(float));
    }
  }

  // Set used frames (for loading external data)
  void setUsedFrames(size_t frames) noexcept
  {
    usedFrames_ = std::min(frames, kMaxFrames);
  }

private:
  std::vector<float> data_;
  size_t usedFrames_ = 0;
};

} // namespace ShortwavDSP
