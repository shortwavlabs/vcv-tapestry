#pragma once

#include "tapestry-core.h"
#include "tapestry-buffer.h"
#include "tapestry-splice.h"
#include "tapestry-grain.h"
#include <cmath>

/*
 * Tapestry DSP Processor
 *
 * Main DSP processing engine that combines all Tapestry components:
 * - Audio buffer management
 * - Splice management
 * - Granular synthesis
 * - Recording with Sound-On-Sound
 * - Envelope follower for CV output
 *
 * This is the top-level DSP class used by the VCV Rack module.
 */

namespace ShortwavDSP
{

class TapestryDSP
{
public:
  TapestryDSP()
  {
    setSampleRate(48000.0f);
    reset();
  }

  //--------------------------------------------------------------------------
  // Configuration
  //--------------------------------------------------------------------------

  void setSampleRate(float sampleRate) noexcept
  {
    sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    grainEngine_.setSampleRate(sampleRate_);

    // Envelope follower coefficients
    float attackMs = 1.0f;
    float releaseMs = 100.0f;
    envAttackCoeff_ = 1.0f - std::exp(-1.0f / (sampleRate_ * attackMs * 0.001f));
    envReleaseCoeff_ = 1.0f - std::exp(-1.0f / (sampleRate_ * releaseMs * 0.001f));

    // Auto-level attack/release
    autoLevelAttack_ = 1.0f - std::exp(-1.0f / (sampleRate_ * 0.1f));
    autoLevelRelease_ = 1.0f - std::exp(-1.0f / (sampleRate_ * 0.5f));
  }

  void reset() noexcept
  {
    buffer_.clear();
    spliceManager_.clear();
    grainEngine_.reset();

    playbackState_ = PlaybackState();
    recordState_ = RecordState();
    moduleMode_ = ModuleMode::Normal;

    envelopeValue_ = 0.0f;
    autoLevelGain_ = 1.0f;
    autoLevelPeak_ = 0.0f;

    // Parameters
    sosParam_ = 1.0f;
    geneSizeParam_ = 0.0f;
    morphParam_ = 0.3f;
    slideParam_ = 0.0f;
    organizeParam_ = 0.0f;
    variSpeedParam_ = 0.5f;
    overdubMode_ = false;  // Reset to default (replace mode)
  }

  //--------------------------------------------------------------------------
  // Parameter Setters
  //--------------------------------------------------------------------------

  void setSos(float sos) noexcept { sosParam_ = TapestryUtil::clamp01(sos); }
  void setGeneSize(float size) noexcept { geneSizeParam_ = TapestryUtil::clamp01(size); }
  void setMorph(float morph) noexcept { morphParam_ = TapestryUtil::clamp01(morph); }
  void setSlide(float slide) noexcept { slideParam_ = TapestryUtil::clamp01(slide); }
  void setOrganize(float organize) noexcept
  {
    organizeParam_ = TapestryUtil::clamp01(organize);
    spliceManager_.setOrganize(organizeParam_);
  }

  // Vari-speed: 0 = full reverse, 0.5 = stopped, 1 = full forward
  void setVariSpeed(float speed) noexcept
  {
    variSpeedParam_ = TapestryUtil::clamp01(speed);
  }

  // Overdub mode: 0 = replace (clear buffer on record), 1 = overdub (keep existing)
  void setOverdubMode(bool overdub) noexcept
  {
    overdubMode_ = overdub;
  }

  bool getOverdubMode() const noexcept
  {
    return overdubMode_;
  }

  // CV inputs with attenuverters
  void setGeneSizeCv(float cv, float atten) noexcept
  {
    geneSizeCv_ = cv;
    geneSizeCvAtten_ = TapestryUtil::clamp(atten, -1.0f, 1.0f);
  }

  void setVariSpeedCv(float cv, float atten) noexcept
  {
    variSpeedCv_ = cv;
    variSpeedCvAtten_ = TapestryUtil::clamp(atten, -1.0f, 1.0f);
  }

  void setSlideCv(float cv, float atten) noexcept
  {
    slideCv_ = cv;
    slideCvAtten_ = TapestryUtil::clamp(atten, -1.0f, 1.0f);
  }

  void setSosCv(float cv) noexcept { sosCv_ = cv; }
  void setMorphCv(float cv) noexcept { morphCv_ = cv; }
  void setOrganizeCv(float cv) noexcept { organizeCv_ = cv; }

  //--------------------------------------------------------------------------
  // Gate/Trigger Inputs
  //--------------------------------------------------------------------------

  void onPlayGate(bool high) noexcept
  {
    bool wasLow = !playbackState_.playGateHigh;
    playbackState_.playGateHigh = high;

    if (wasLow && high)
    {
      // Rising edge: retrigger from start of splice
      grainEngine_.retrigger(slideParam_);
      playbackState_.isPlaying = true;
    }
    else if (!high)
    {
      // Gate low: stop at end of current gene/splice
      // (actual stop happens in process when gene ends)
    }
  }

  void onClockRising() noexcept
  {
    grainEngine_.onClockRising();

    // If recording and waiting for clock sync
    if (recordState_.waitingForClock)
    {
      if (recordState_.mode == RecordState::Mode::Idle)
      {
        // Start recording on clock
        startRecording(pendingRecordMode_, pendingRecordPosition_);
        pendingRecordPosition_ = 0;
      }
      else
      {
        // Stop recording on clock
        stopRecording();
      }
      recordState_.waitingForClock = false;
    }
  }

  void onClockDisconnected() noexcept
  {
    grainEngine_.setClockDisconnected();
  }

  void onShiftTrigger() noexcept
  {
    if (moduleMode_ == ModuleMode::Normal)
    {
      // Shift immediately to next splice
      // Playhead continues from current position - does not reset
      spliceManager_.shiftImmediate();

      // Retrigger the grain engine at new splice position
      // This also works when stopped - it sets up the position for when playback resumes
      grainEngine_.retrigger(slideParam_);
      
      // Reset playhead to start of new splice for visual feedback
      const SpliceMarker* newSplice = spliceManager_.getCurrentSplice();
      if (newSplice)
      {
        grainEngine_.setAbsolutePosition(static_cast<double>(newSplice->startFrame));
      }
    }
  }

  void onSpliceTrigger(size_t currentFrame) noexcept
  {
    if (moduleMode_ == ModuleMode::Normal && !isRecording())
    {
      spliceManager_.addMarkerAtPosition(currentFrame);
    }
  }

  //--------------------------------------------------------------------------
  // Recording Control
  //--------------------------------------------------------------------------

  bool isRecording() const noexcept
  {
    return recordState_.mode != RecordState::Mode::Idle;
  }

  RecordState::Mode getRecordMode() const noexcept
  {
    return recordState_.mode;
  }

  // Clear buffer and start fresh recording (replaces existing content)
  // If overdub mode is enabled, this skips the clear and just starts recording
  // currentPosition: where to start recording in overdub mode (typically current playhead)
  void clearAndStartRecording(bool clockSync = false, size_t currentPosition = 0) noexcept
  {
    if (!overdubMode_)
    {      // Replace mode: Clear existing buffer and splices
      buffer_.clear();
      spliceManager_.clear();
      currentPosition = 0; // Always start from 0 in replace mode
    }
    // If overdub mode is on and buffer has content, start recording at current position
    
    // Start recording
    if (clockSync)
    {
      pendingRecordMode_ = RecordState::Mode::SameSplice;
      pendingRecordPosition_ = currentPosition;
      recordState_.waitingForClock = true;
    }
    else
    {
      startRecording(RecordState::Mode::SameSplice, currentPosition);
    }
  }

  void startRecordingSameSplice(bool clockSync = false) noexcept
  {
    if (clockSync)
    {
      pendingRecordMode_ = RecordState::Mode::SameSplice;
      recordState_.waitingForClock = true;
    }
    else
    {
      startRecording(RecordState::Mode::SameSplice);
    }
  }

  void startRecordingNewSplice(bool clockSync = false) noexcept
  {
    if (clockSync)
    {
      pendingRecordMode_ = RecordState::Mode::NewSplice;
      recordState_.waitingForClock = true;
    }
    else
    {
      startRecording(RecordState::Mode::NewSplice);
    }
  }

  void stopRecordingRequest(bool clockSync = false) noexcept
  {
    if (clockSync && isRecording())
    {
      recordState_.waitingForClock = true;
    }
    else
    {
      stopRecording();
    }
  }

  bool isWaitingForClock() const noexcept
  {
    return recordState_.waitingForClock;
  }

  //--------------------------------------------------------------------------
  // Auto-Leveling
  //--------------------------------------------------------------------------

  void startAutoLevel() noexcept
  {
    isAutoLeveling_ = true;
    autoLevelPeak_ = 0.0f;
  }

  void stopAutoLevel() noexcept
  {
    isAutoLeveling_ = false;
    // Calculate gain to normalize to ~0.8 (10Vpp headroom)
    if (autoLevelPeak_ > 0.001f)
    {
      autoLevelGain_ = 0.8f / autoLevelPeak_;
      autoLevelGain_ = TapestryUtil::clamp(autoLevelGain_, 0.1f, 10.0f);
    }
  }

  bool isAutoLeveling() const noexcept { return isAutoLeveling_; }
  float getAutoLevelGain() const noexcept { return autoLevelGain_; }

  //--------------------------------------------------------------------------
  // Splice Management
  //--------------------------------------------------------------------------

  void deleteCurrentMarker() noexcept
  {
    if (!isRecording())
    {
      spliceManager_.deleteCurrentMarker();
    }
  }

  void deleteAllMarkers() noexcept
  {
    if (!isRecording())
    {
      spliceManager_.deleteAllMarkers();
    }
  }

  void deleteCurrentSpliceAudio() noexcept
  {
    if (!isRecording())
    {
      size_t start, end;
      if (spliceManager_.deleteCurrentSpliceAudio(start, end))
      {
        buffer_.clearRange(start, end);
        // TODO: Shift remaining audio if splice was deleted (not just cleared)
      }
    }
  }

  void clearReel() noexcept
  {
    stopRecording();
    buffer_.clear();
    spliceManager_.clear();
    grainEngine_.reset();
    playbackState_ = PlaybackState();
  }

  //--------------------------------------------------------------------------
  // Main Processing
  //--------------------------------------------------------------------------

  struct ProcessResult
  {
    float audioOutL = 0.0f;
    float audioOutR = 0.0f;
    float cvOut = 0.0f;
    bool endOfSpliceGene = false;
  };

  ProcessResult process(float audioInL, float audioInR) noexcept
  {
    ProcessResult result;

    // Apply auto-level gain to input
    if (isAutoLeveling_)
    {
      float peak = std::max(std::fabs(audioInL), std::fabs(audioInR));
      if (peak > autoLevelPeak_)
      {
        autoLevelPeak_ += autoLevelAttack_ * (peak - autoLevelPeak_);
      }
      else
      {
        autoLevelPeak_ += autoLevelRelease_ * (peak - autoLevelPeak_);
      }
    }

    audioInL *= autoLevelGain_;
    audioInR *= autoLevelGain_;

    // Calculate effective parameters with CV modulation
    float effectiveSos = sosParam_;
    if (sosCv_ != 0.0f)
    {
      effectiveSos += sosCv_ / TapestryConfig::kSosCvMax;
      effectiveSos = TapestryUtil::clamp01(effectiveSos);
    }

    float effectiveGeneSize = geneSizeParam_;
    effectiveGeneSize += (geneSizeCv_ / TapestryConfig::kGeneSizeCvMax) * geneSizeCvAtten_;
    effectiveGeneSize = TapestryUtil::clamp01(effectiveGeneSize);

    float effectiveMorph = morphParam_;
    if (morphCv_ != 0.0f)
    {
      effectiveMorph += morphCv_ / TapestryConfig::kMorphCvMax;
      effectiveMorph = TapestryUtil::clamp01(effectiveMorph);
    }

    float effectiveSlide = slideParam_;
    effectiveSlide += (slideCv_ / TapestryConfig::kSlideCvMax) * slideCvAtten_;
    effectiveSlide = TapestryUtil::clamp01(effectiveSlide);

    float effectiveOrganize = organizeParam_;
    if (organizeCv_ != 0.0f)
    {
      effectiveOrganize += organizeCv_ / TapestryConfig::kOrganizeCvMax;
      effectiveOrganize = TapestryUtil::clamp01(effectiveOrganize);
      spliceManager_.setOrganize(effectiveOrganize);
    }

    // Vari-speed: convert 0-1 to -1 to +1
    float variSpeedBipolar = (variSpeedParam_ - 0.5f) * 2.0f;
    variSpeedState_ = TapestryUtil::calculateVariSpeed(
        variSpeedBipolar, variSpeedCv_, variSpeedCvAtten_);

    // Update morph state
    morphState_ = TapestryUtil::calculateMorphState(effectiveMorph);

    // Get current splice bounds
    const SpliceMarker *currentSplice = spliceManager_.getCurrentSplice();
    size_t spliceStart = 0;
    size_t spliceEnd = buffer_.getUsedFrames();

    if (currentSplice && currentSplice->isValid())
    {
      spliceStart = currentSplice->startFrame;
      spliceEnd = std::min(currentSplice->endFrame, buffer_.getUsedFrames());
    }

    // Calculate gene size in samples
    float spliceLengthSamples = static_cast<float>(spliceEnd - spliceStart);
    float geneSizeSamples = TapestryUtil::calculateGeneSizeSamples(
        effectiveGeneSize, spliceLengthSamples);

    // Update grain engine parameters
    grainEngine_.setGeneSize(geneSizeSamples);
    grainEngine_.setMorphState(morphState_);
    grainEngine_.setSlide(effectiveSlide);
    grainEngine_.setVariSpeed(variSpeedState_);

    // Process playback
    float playbackL = 0.0f;
    float playbackR = 0.0f;
    bool endOfGene = false;

    if (playbackState_.isPlaying && !buffer_.isEmpty())
    {
      grainEngine_.process(buffer_, spliceStart, spliceEnd,
                           playbackL, playbackR, endOfGene);

      // Check for end of splice
      if (endOfGene)
      {
        result.endOfSpliceGene = true;

        // Apply pending splice change
        if (spliceManager_.onEndOfSplice())
        {
          // Splice changed - retrigger if gate is high
          if (playbackState_.playGateHigh)
          {
            grainEngine_.retrigger(effectiveSlide);
          }
        }

        // Stop if gate is low
        if (!playbackState_.playGateHigh)
        {
          playbackState_.isPlaying = false;
        }
      }
    }

    // Process recording
    if (recordState_.mode != RecordState::Mode::Idle)
    {
      processRecording(audioInL, audioInR, playbackL, playbackR, effectiveSos);
    }

    // Mix output based on S.O.S. setting
    // During playback: S.O.S. controls dry/wet
    // Full CCW (0): live input only
    // Full CW (1): loop playback only
    result.audioOutL = audioInL * (1.0f - effectiveSos) + playbackL * effectiveSos;
    result.audioOutR = audioInR * (1.0f - effectiveSos) + playbackR * effectiveSos;

    // Envelope follower for CV output
    float outputPeak = std::max(std::fabs(result.audioOutL), std::fabs(result.audioOutR));
    if (outputPeak > envelopeValue_)
    {
      envelopeValue_ += envAttackCoeff_ * (outputPeak - envelopeValue_);
    }
    else
    {
      envelopeValue_ += envReleaseCoeff_ * (outputPeak - envelopeValue_);
    }
    result.cvOut = envelopeValue_ * TapestryConfig::kCvOutMax;

    return result;
  }

  //--------------------------------------------------------------------------
  // State Accessors
  //--------------------------------------------------------------------------

  const TapestryBuffer &getBuffer() const noexcept { return buffer_; }
  TapestryBuffer &getBuffer() noexcept { return buffer_; }

  const SpliceManager &getSpliceManager() const noexcept { return spliceManager_; }
  SpliceManager &getSpliceManager() noexcept { return spliceManager_; }

  const GrainEngine &getGrainEngine() const noexcept { return grainEngine_; }

  const PlaybackState &getPlaybackState() const noexcept { return playbackState_; }
  const RecordState &getRecordState() const noexcept { return recordState_; }
  const VariSpeedState &getVariSpeedState() const noexcept { return variSpeedState_; }
  const MorphState &getMorphState() const noexcept { return morphState_; }

  ModuleMode getModuleMode() const noexcept { return moduleMode_; }
  void setModuleMode(ModuleMode mode) noexcept { moduleMode_ = mode; }

  float getEnvelopeValue() const noexcept { return envelopeValue_; }

  //--------------------------------------------------------------------------
  // Playback Control
  //--------------------------------------------------------------------------

  void startPlayback() noexcept
  {
    if (!buffer_.isEmpty())
    {
      playbackState_.isPlaying = true;
      if (!grainEngine_.isActive())
      {
        grainEngine_.retrigger(slideParam_);
      }
    }
  }

  void stopPlayback() noexcept
  {
    playbackState_.isPlaying = false;
  }

  //--------------------------------------------------------------------------
  // Reel Management
  //--------------------------------------------------------------------------

  // Initialize buffer with external data (for loading files)
  void loadReel(const float *data, size_t numFrames,
                const std::vector<size_t> &markers = {}) noexcept
  {
    clearReel();

    size_t framesToLoad = std::min(numFrames, TapestryBuffer::kMaxFrames);
    buffer_.copyFrom(data, framesToLoad);
    buffer_.setUsedFrames(framesToLoad);

    if (markers.empty())
    {
      spliceManager_.initialize(framesToLoad);
    }
    else
    {
      spliceManager_.setFromMarkerPositions(markers, framesToLoad);
    }

    playbackState_.isPlaying = true;
    grainEngine_.retrigger(0.0f);
  }

  // Get data for saving
  size_t getReelData(float *dest, size_t maxFrames) const noexcept
  {
    size_t framesToCopy = std::min(buffer_.getUsedFrames(), maxFrames);
    buffer_.copyTo(dest, framesToCopy);
    return framesToCopy;
  }

  std::vector<size_t> getMarkerPositions() const
  {
    return spliceManager_.getMarkerPositions();
  }

private:
  //--------------------------------------------------------------------------
  // Recording Implementation
  //--------------------------------------------------------------------------

  void startRecording(RecordState::Mode mode) noexcept
  {
    startRecording(mode, 0);
  }

  void startRecording(RecordState::Mode mode, size_t overdubPosition) noexcept
  {
    // Track if this is a new recording into a freshly created splice
    recordState_.isInitialRecording = false;
    
    if (mode == RecordState::Mode::SameSplice)
    {
      // TLA: record into current splice, or start fresh if empty
      const SpliceMarker *splice = spliceManager_.getCurrentSplice();
      if (splice && splice->isValid() && buffer_.getUsedFrames() > 0)
      {
        // Overdub mode: start at specified position (or splice start if position is 0)
        if (overdubPosition > 0 && overdubMode_)
        {
          // Start recording from current playhead position
          recordState_.recordPosition = overdubPosition;
          recordState_.recordStartFrame = splice->startFrame;
        }
        else
        {
          // Normal mode: loop within existing splice bounds from start
          recordState_.recordPosition = splice->startFrame;
          recordState_.recordStartFrame = splice->startFrame;
        }
      }
      else
      {
        // No valid splice or empty buffer - create one and start recording from beginning
        // This is "initial recording" mode - extend the splice as we record
        recordState_.recordPosition = 0;
        recordState_.recordStartFrame = 0;
        recordState_.isInitialRecording = true;
        
        // Create an initial splice that will be extended as we record
        spliceManager_.clear();
        spliceManager_.addNewSpliceAtEnd(0, 1); // Start with minimal splice
      }
    }
    else if (mode == RecordState::Mode::NewSplice)
    {
      // Record into new splice at end of reel - always "initial recording" mode
      recordState_.isInitialRecording = true;
      
      size_t reelEnd = buffer_.getUsedFrames();
      recordState_.recordPosition = reelEnd;
      recordState_.recordStartFrame = reelEnd;

      // Create new splice marker at the end
      if (reelEnd > 0)
      {
        spliceManager_.addNewSpliceAtEnd(reelEnd, reelEnd + 1);
      }
      else
      {
        // First recording - create initial splice
        spliceManager_.clear();
        spliceManager_.addNewSpliceAtEnd(0, 1);
      }
      
      // Set current index to the new splice
      if (spliceManager_.getNumSplices() > 0)
      {
        spliceManager_.setCurrentIndex(static_cast<int>(spliceManager_.getNumSplices()) - 1);
      }
    }

    recordState_.mode = mode;
    recordState_.waitingForClock = false;
  }

  void stopRecording() noexcept
  {
    // Finalize splice when stopping an initial recording (extending mode)
    if (recordState_.isInitialRecording)
    {
      spliceManager_.extendLastSplice(recordState_.recordPosition);
    }

    recordState_.mode = RecordState::Mode::Idle;
    recordState_.waitingForClock = false;
    recordState_.isInitialRecording = false;
  }

  void processRecording(float liveL, float liveR,
                        float playbackL, float playbackR,
                        float sosAmount) noexcept
  {
    if (recordState_.mode == RecordState::Mode::Idle)
      return;

    // Check max reel length
    if (recordState_.recordPosition >= TapestryBuffer::kMaxFrames)
    {
      stopRecording();
      return;
    }

    if (recordState_.mode == RecordState::Mode::SameSplice)
    {
      // Check if we need to loop or extend based on recording mode
      if (recordState_.isInitialRecording)
      {
        // Initial recording mode: write live input directly (ignore SOS)
        // SOS only makes sense when there's existing content to blend with
        buffer_.writeStereo(recordState_.recordPosition, liveL, liveR);
        recordState_.recordPosition++;
        
        // Extend the splice as we record
        spliceManager_.extendLastSplice(recordState_.recordPosition);
      }
      else
      {
        // Overdub mode: Sound-on-Sound recording into current splice
        if (overdubMode_)
        {
          // True overdub: ADD new audio to existing (ignore SOS parameter)
          float existingL, existingR;
          buffer_.readStereo(recordState_.recordPosition, existingL, existingR);
          buffer_.writeStereo(recordState_.recordPosition, 
                            existingL + liveL, 
                            existingR + liveR);
        }
        else
        {
          // Traditional SOS: blend based on sosAmount parameter
          // sosAmount controls wet/dry of what we're recording:
          // 0 = record live input over existing (replace)
          // 1 = keep existing loop content (no new recording)
          // 0.5 = blend 50/50
          buffer_.mixAndWrite(recordState_.recordPosition, liveL, liveR, sosAmount);
        }
        recordState_.recordPosition++;
        
        // Loop within current splice bounds
        const SpliceMarker *splice = spliceManager_.getCurrentSplice();
        if (splice && splice->isValid() && recordState_.recordPosition >= splice->endFrame)
        {
          // Loop back to start of splice
          recordState_.recordPosition = splice->startFrame;
        }
      }
    }
    else if (recordState_.mode == RecordState::Mode::NewSplice)
    {
      // New splice: Write live input
      // This is always "initial recording" mode - extending the splice
      buffer_.writeStereo(recordState_.recordPosition, liveL, liveR);
      recordState_.recordPosition++;

      // Update splice end
      spliceManager_.extendLastSplice(recordState_.recordPosition);
    }
  }

  //--------------------------------------------------------------------------
  // State
  //--------------------------------------------------------------------------

  float sampleRate_ = 48000.0f;

  TapestryBuffer buffer_;
  SpliceManager spliceManager_;
  GrainEngine grainEngine_;

  PlaybackState playbackState_;
  RecordState recordState_;
  RecordState::Mode pendingRecordMode_ = RecordState::Mode::Idle;
  size_t pendingRecordPosition_ = 0;
  ModuleMode moduleMode_ = ModuleMode::Normal;

  VariSpeedState variSpeedState_;
  MorphState morphState_;

  // Parameters
  float sosParam_ = 1.0f;
  float geneSizeParam_ = 0.0f;
  float morphParam_ = 0.3f;
  float slideParam_ = 0.0f;
  float organizeParam_ = 0.0f;
  float variSpeedParam_ = 0.5f;

  // CV inputs
  float sosCv_ = 0.0f;
  float geneSizeCv_ = 0.0f;
  float geneSizeCvAtten_ = 0.0f;
  float morphCv_ = 0.0f;
  float slideCv_ = 0.0f;
  float slideCvAtten_ = 0.0f;
  float organizeCv_ = 0.0f;
  float variSpeedCv_ = 0.0f;
  float variSpeedCvAtten_ = 0.0f;

  // Envelope follower
  float envelopeValue_ = 0.0f;
  float envAttackCoeff_ = 0.0f;
  float envReleaseCoeff_ = 0.0f;

  // Auto-leveling
  bool isAutoLeveling_ = false;
  float autoLevelGain_ = 1.0f;
  float autoLevelPeak_ = 0.0f;
  float autoLevelAttack_ = 0.0f;
  float autoLevelRelease_ = 0.0f;

  // Overdub mode
  bool overdubMode_ = false;  // Default OFF: replace existing content
};

} // namespace ShortwavDSP
