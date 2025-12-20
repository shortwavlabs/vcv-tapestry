#pragma once

#include "tapestry-core.h"
#include <algorithm>
#include <vector>

/*
 * Tapestry Splice Manager
 *
 * Manages splice markers within a reel buffer.
 * Supports up to 300 splices per reel.
 *
 * Features:
 * - Splice marker creation and deletion
 * - Organize parameter mapping to splice selection
 * - Pending splice system (change at end of current)
 * - Shift button/gate increment
 */

namespace ShortwavDSP
{

class SpliceManager
{
public:
  static constexpr size_t kMaxSplices = TapestryConfig::kMaxSplices;

  SpliceManager() = default;

  //--------------------------------------------------------------------------
  // Initialization
  //--------------------------------------------------------------------------

  // Initialize with a single splice covering the entire buffer
  void initialize(size_t totalFrames) noexcept
  {
    splices_.clear();
    if (totalFrames > 0)
    {
      splices_.push_back({0, totalFrames});
    }
    currentIndex_ = 0;
    pendingIndex_ = -1;
    organizeTarget_ = -1;
  }

  // Clear all splices
  void clear() noexcept
  {
    splices_.clear();
    currentIndex_ = 0;
    pendingIndex_ = -1;
    organizeTarget_ = -1;
  }

  //--------------------------------------------------------------------------
  // Splice Access
  //--------------------------------------------------------------------------

  size_t getNumSplices() const noexcept { return splices_.size(); }
  bool isEmpty() const noexcept { return splices_.empty(); }
  bool isFull() const noexcept { return splices_.size() >= kMaxSplices; }

  int getCurrentIndex() const noexcept { return currentIndex_; }
  int getPendingIndex() const noexcept { return pendingIndex_; }
  bool hasPending() const noexcept { return pendingIndex_ >= 0; }

  // Get current splice
  const SpliceMarker *getCurrentSplice() const noexcept
  {
    if (splices_.empty() || currentIndex_ < 0 ||
        currentIndex_ >= static_cast<int>(splices_.size()))
    {
      return nullptr;
    }
    return &splices_[currentIndex_];
  }

  // Get splice by index
  const SpliceMarker *getSplice(int index) const noexcept
  {
    if (index < 0 || index >= static_cast<int>(splices_.size()))
    {
      return nullptr;
    }
    return &splices_[index];
  }

  // Get all splices (for UI display)
  const std::vector<SpliceMarker> &getAllSplices() const noexcept
  {
    return splices_;
  }

  //--------------------------------------------------------------------------
  // Marker Creation
  //--------------------------------------------------------------------------

  // Add a splice marker at the given frame position
  // Returns true if marker was added
  bool addMarker(size_t framePosition) noexcept
  {
    if (splices_.size() >= kMaxSplices)
      return false;
    if (splices_.empty())
      return false;

    // Find which splice contains this position
    int spliceIdx = -1;
    for (size_t i = 0; i < splices_.size(); i++)
    {
      if (framePosition > splices_[i].startFrame &&
          framePosition < splices_[i].endFrame)
      {
        spliceIdx = static_cast<int>(i);
        break;
      }
    }

    if (spliceIdx < 0)
      return false;

    // Split the splice at this position
    SpliceMarker &current = splices_[spliceIdx];
    size_t oldEnd = current.endFrame;
    current.endFrame = framePosition;

    // Insert new splice after current
    SpliceMarker newSplice = {framePosition, oldEnd};
    splices_.insert(splices_.begin() + spliceIdx + 1, newSplice);

    return true;
  }

  // Add marker at current playback position
  bool addMarkerAtPosition(size_t playbackFrame) noexcept
  {
    return addMarker(playbackFrame);
  }

  //--------------------------------------------------------------------------
  // Marker Deletion
  //--------------------------------------------------------------------------

  // Delete current splice marker (merge with next)
  // Returns true if marker was deleted
  bool deleteCurrentMarker() noexcept
  {
    if (splices_.size() <= 1)
      return false;

    // Merge current with next splice
    int nextIdx = (currentIndex_ + 1) % static_cast<int>(splices_.size());

    if (nextIdx == 0)
    {
      // Current is last splice, merge with first (wrap)
      splices_[currentIndex_].endFrame = splices_[0].endFrame;
      splices_.erase(splices_.begin());
      if (currentIndex_ > 0)
        currentIndex_--;
    }
    else
    {
      // Extend current to include next
      splices_[currentIndex_].endFrame = splices_[nextIdx].endFrame;
      splices_.erase(splices_.begin() + nextIdx);
    }

    // Clamp current index
    if (currentIndex_ >= static_cast<int>(splices_.size()))
    {
      currentIndex_ = static_cast<int>(splices_.size()) - 1;
    }

    pendingIndex_ = -1;
    return true;
  }

  // Delete all splice markers (single splice covering entire reel)
  void deleteAllMarkers() noexcept
  {
    if (splices_.empty())
      return;

    size_t totalEnd = splices_.back().endFrame;
    splices_.clear();
    splices_.push_back({0, totalEnd});
    currentIndex_ = 0;
    pendingIndex_ = -1;
  }

  // Delete current splice and its audio content
  // Returns the range that was deleted for buffer clearing
  bool deleteCurrentSpliceAudio(size_t &deletedStart, size_t &deletedEnd) noexcept
  {
    if (splices_.empty())
      return false;

    deletedStart = splices_[currentIndex_].startFrame;
    deletedEnd = splices_[currentIndex_].endFrame;

    if (splices_.size() == 1)
    {
      // Only one splice - clear it but keep the marker
      return true;
    }

    // Remove the splice
    splices_.erase(splices_.begin() + currentIndex_);

    // Adjust remaining splice positions
    size_t deletedLength = deletedEnd - deletedStart;
    for (size_t i = currentIndex_; i < splices_.size(); i++)
    {
      splices_[i].startFrame -= deletedLength;
      splices_[i].endFrame -= deletedLength;
    }

    // Clamp current index
    if (currentIndex_ >= static_cast<int>(splices_.size()))
    {
      currentIndex_ = static_cast<int>(splices_.size()) - 1;
    }

    pendingIndex_ = -1;
    return true;
  }

  //--------------------------------------------------------------------------
  // Navigation
  //--------------------------------------------------------------------------

  // Set target splice from Organize parameter (0-1)
  // Applies immediately only when the knob actually moves
  void setOrganize(float param) noexcept
  {
    if (splices_.empty())
    {
      organizeTarget_ = -1;
      lastOrganizeParam_ = param;
      return;
    }

    param = TapestryUtil::clamp01(param);
    
    // Only apply if the organize parameter actually changed significantly
    // This prevents overriding shift when the knob is just sitting at a position
    const float kThreshold = 0.01f;  // ~1% change required
    if (std::fabs(param - lastOrganizeParam_) > kThreshold)
    {
      int index = static_cast<int>(param * (splices_.size() - 1) + 0.5f);
      index = std::min(index, static_cast<int>(splices_.size()) - 1);

      organizeTarget_ = index;
      lastOrganizeParam_ = param;
      
      // Apply immediately when organize knob is actually moved
      if (organizeTarget_ != currentIndex_)
      {
        currentIndex_ = organizeTarget_;
        pendingIndex_ = -1;  // Clear any shift pending when user actively moves organize
      }
    }
  }

  // Apply organize target as pending (called at end of splice if no manual pending)
  void applyOrganizeIfNoManualPending() noexcept
  {
    // Only apply organize if there's no manual pending (from shift)
    if (pendingIndex_ < 0 && organizeTarget_ >= 0 && organizeTarget_ != currentIndex_)
    {
      pendingIndex_ = organizeTarget_;
    }
  }

  // Increment to next splice immediately (Shift button - immediate mode)
  void shiftImmediate() noexcept
  {
    if (splices_.empty())
      return;

    int nextIndex = (currentIndex_ + 1) % static_cast<int>(splices_.size());
    currentIndex_ = nextIndex;
    pendingIndex_ = -1;  // Clear any pending
    organizeTarget_ = currentIndex_;  // Sync organize target to prevent override
    
    // Do NOT update lastOrganizeParam_ here - we want it to stay at the actual knob position
    // so that setOrganize() won't apply unless the user actually moves the organize knob
  }

  // Increment to next splice (pending mode - waits for end of gene)
  void shift() noexcept
  {
    if (splices_.empty())
      return;

    int nextIndex = (currentIndex_ + 1) % static_cast<int>(splices_.size());
    if (nextIndex != currentIndex_)
    {
      pendingIndex_ = nextIndex;
    }
  }

  // Called at end of splice/gene - apply pending change
  // Returns true if splice changed
  bool onEndOfSplice() noexcept
  {
    // First, check if organize wants to change splice (only if no manual pending)
    applyOrganizeIfNoManualPending();
    
    if (pendingIndex_ >= 0 && pendingIndex_ < static_cast<int>(splices_.size()))
    {
      currentIndex_ = pendingIndex_;
      pendingIndex_ = -1;
      return true;
    }
    return false;
  }

  // Force immediate splice change (for Organize override)
  void setCurrentIndex(int index) noexcept
  {
    if (index >= 0 && index < static_cast<int>(splices_.size()))
    {
      currentIndex_ = index;
      pendingIndex_ = -1;
    }
  }

  //--------------------------------------------------------------------------
  // Recording Support
  //--------------------------------------------------------------------------

  // Extend the last splice (for recording into new splice)
  void extendLastSplice(size_t newEndFrame) noexcept
  {
    if (!splices_.empty())
    {
      splices_.back().endFrame = newEndFrame;
    }
  }

  // Add a new empty splice at the end
  bool addNewSpliceAtEnd(size_t startFrame, size_t endFrame) noexcept
  {
    if (splices_.size() >= kMaxSplices)
      return false;
    if (startFrame >= endFrame)
      return false;

    splices_.push_back({startFrame, endFrame});
    return true;
  }

  // Get the end position of the last splice
  size_t getReelEndFrame() const noexcept
  {
    if (splices_.empty())
      return 0;
    return splices_.back().endFrame;
  }

  //--------------------------------------------------------------------------
  // Serialization Support
  //--------------------------------------------------------------------------

  // Get marker positions for WAV file export
  std::vector<size_t> getMarkerPositions() const
  {
    std::vector<size_t> positions;
    for (const auto &splice : splices_)
    {
      positions.push_back(splice.startFrame);
    }
    return positions;
  }

  // Set markers from WAV file import
  void setFromMarkerPositions(const std::vector<size_t> &positions, size_t totalFrames)
  {
    splices_.clear();
    if (positions.empty() || totalFrames == 0)
    {
      if (totalFrames > 0)
      {
        splices_.push_back({0, totalFrames});
      }
      currentIndex_ = 0;
      pendingIndex_ = -1;
      return;
    }

    // Sort positions
    std::vector<size_t> sorted = positions;
    std::sort(sorted.begin(), sorted.end());

    // Remove duplicates and out-of-range
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    sorted.erase(std::remove_if(sorted.begin(), sorted.end(),
                                [totalFrames](size_t p)
                                { return p >= totalFrames; }),
                 sorted.end());

    // Ensure starts at 0
    if (sorted.empty() || sorted[0] != 0)
    {
      sorted.insert(sorted.begin(), 0);
    }

    // Create splices from marker positions
    for (size_t i = 0; i < sorted.size() && i < kMaxSplices; i++)
    {
      size_t start = sorted[i];
      size_t end = (i + 1 < sorted.size()) ? sorted[i + 1] : totalFrames;
      if (end > start)
      {
        splices_.push_back({start, end});
      }
    }

    currentIndex_ = 0;
    pendingIndex_ = -1;
    organizeTarget_ = -1;
  }

private:
  std::vector<SpliceMarker> splices_;
  int currentIndex_ = 0;
  int pendingIndex_ = -1;    // -1 = no pending change (from shift or organize)
  int organizeTarget_ = -1;  // Target splice from organize knob
  float lastOrganizeParam_ = -1.0f;  // Last organize parameter value (to detect actual movement)
};

} // namespace ShortwavDSP
