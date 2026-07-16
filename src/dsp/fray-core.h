#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace ShortwavDSP {
namespace Fray {

static const int kLaneCount = 11;
static const int kEffectCount = 10;
static const int kMaxSteps = 64;
static const int kSceneCount = 128;
static const int kEffectParamCount = 12;
static const std::uint32_t kRngVersion = 1u;

enum EffectId {
    MODULATOR = 0,
    TAPE_STOP,
    RETRIGGER,
    REVERSER,
    STRETCHER,
    LOFI,
    DISTORTION,
    GATER,
    DELAY,
    SHUFFLER
};

namespace detail {

template <typename T>
inline T clamp(T value, T minimum, T maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

inline float finiteOr(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

inline double finiteOr(double value, double fallback) {
    return std::isfinite(value) ? value : fallback;
}

inline int clampStepCount(int stepCount) {
    return clamp(stepCount, 2, kMaxSteps);
}

inline std::uint64_t maskForStepCount(int stepCount) {
    const int count = clampStepCount(stepCount);
    return count == 64 ? ~std::uint64_t(0)
                       : ((std::uint64_t(1) << count) - std::uint64_t(1));
}

inline std::uint64_t rotateLeft(std::uint64_t value, int amount) {
    return (value << amount) | (value >> (64 - amount));
}

inline std::uint64_t splitMix64(std::uint64_t& value) {
    std::uint64_t result = (value += UINT64_C(0x9e3779b97f4a7c15));
    result = (result ^ (result >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    result = (result ^ (result >> 27)) * UINT64_C(0x94d049bb133111eb);
    return result ^ (result >> 31);
}

} // namespace detail

struct LanePattern {
    std::uint64_t activeMask;
    std::uint64_t startMask;

    LanePattern() : activeMask(0), startMask(0) {}

    static std::uint64_t validMask(int stepCount) {
        return detail::maskForStepCount(stepCount);
    }

    void clear() {
        activeMask = 0;
        startMask = 0;
    }

    void sanitize(int stepCount) {
        const int count = detail::clampStepCount(stepCount);
        const std::uint64_t valid = validMask(count);
        activeMask &= valid;
        startMask &= activeMask;

        if (activeMask == 0)
            return;

        // Preserve explicit adjacent block starts while guaranteeing that every
        // otherwise-disconnected run has a start marker.
        std::uint64_t preceding = (activeMask << 1) & valid;
        std::uint64_t requiredStarts = activeMask & ~preceding;
        startMask |= requiredStarts;
        startMask &= activeMask;
    }

    bool isActive(int step, int stepCount) const {
        const int count = detail::clampStepCount(stepCount);
        if (step < 0 || step >= count)
            return false;
        return (activeMask & (std::uint64_t(1) << step)) != 0;
    }

    bool isStart(int step, int stepCount) const {
        const int count = detail::clampStepCount(stepCount);
        if (step < 0 || step >= count)
            return false;
        const std::uint64_t bit = std::uint64_t(1) << step;
        return (activeMask & startMask & bit) != 0;
    }

    bool isEnd(int step, int stepCount, bool loop) const {
        const int count = detail::clampStepCount(stepCount);
        if (!isActive(step, count))
            return false;

        const int next = step + 1;
        if (next >= count) {
            if (!loop)
                return true;
            return !isActive(0, count) || isStart(0, count);
        }
        return !isActive(next, count) || isStart(next, count);
    }

    bool active(int step, int stepCount) const {
        return isActive(step, stepCount);
    }

    bool blockStart(int step, int stepCount) const {
        return isStart(step, stepCount);
    }

    bool blockEnd(int step, int stepCount, bool loop) const {
        return isEnd(step, stepCount, loop);
    }
};

struct CommonSettings {
    int filterType;
    float cutoff;
    float q;
    float mix;
    float pan;
    float gain;

    CommonSettings()
        : filterType(0), cutoff(0.75f), q(0.25f), mix(1.0f), pan(0.0f), gain(1.0f) {}

    void sanitize() {
        filterType = detail::clamp(filterType, 0, 4);
        cutoff = detail::clamp(detail::finiteOr(cutoff, 0.75f), 0.0f, 1.0f);
        q = detail::clamp(detail::finiteOr(q, 0.25f), 0.0f, 1.0f);
        mix = detail::clamp(detail::finiteOr(mix, 1.0f), 0.0f, 1.0f);
        pan = detail::clamp(detail::finiteOr(pan, 0.0f), -1.0f, 1.0f);
        gain = detail::clamp(detail::finiteOr(gain, 1.0f), 0.0f, 2.0f);
    }
};

struct EffectSettings {
    CommonSettings common;
    std::array<float, kEffectParamCount> values;

    EffectSettings() : common(), values() {
        values.fill(0.5f);
    }

    void sanitize() {
        common.sanitize();
        for (int i = 0; i < kEffectParamCount; ++i)
            values[i] = detail::clamp(detail::finiteOr(values[i], 0.5f), 0.0f, 1.0f);
    }
};

struct SceneState {
    int beats;
    int divisions;
    bool loop;
    std::uint32_t seed;
    std::array<LanePattern, kLaneCount> lanes;
    std::array<float, kEffectCount> randomWeights;
    std::array<EffectSettings, kEffectCount> effects;

    SceneState()
        : beats(4), divisions(4), loop(true), seed(1u), lanes(), randomWeights(), effects() {
        randomWeights.fill(1.0f);
        // Raw distortion is the zero-added-latency default. Users can opt into
        // Rack's 2x FIR oversampling with the Distortion Quality switch.
        effects[DISTORTION].values[7] = 0.0f;
    }

    int stepCount() const {
        const int safeBeats = detail::clamp(beats, 1, 8);
        const int safeDivisions = detail::clamp(divisions, 2, 8);
        return safeBeats * safeDivisions;
    }

    void sanitize() {
        beats = detail::clamp(beats, 1, 8);
        divisions = detail::clamp(divisions, 2, 8);
        const int steps = beats * divisions;
        for (int i = 0; i < kLaneCount; ++i)
            lanes[i].sanitize(steps);
        for (int i = 0; i < kEffectCount; ++i) {
            randomWeights[i] = detail::clamp(
                detail::finiteOr(randomWeights[i], 0.0f), 0.0f, 1.0f);
            effects[i].sanitize();
        }
    }
};

struct ProgramState {
    std::array<SceneState, kSceneCount> scenes;
    std::array<int, kEffectCount> effectOrder;
    float masterMix;
    float masterPan;
    float masterGain;
    int triggerMode;
    int quantizeMode;

    ProgramState()
        : scenes(), effectOrder(), masterMix(1.0f), masterPan(0.0f), masterGain(1.0f),
          triggerMode(0), quantizeMode(0) {
        for (int i = 0; i < kEffectCount; ++i)
            effectOrder[i] = i;
    }

    void sanitize() {
        masterMix = detail::clamp(detail::finiteOr(masterMix, 1.0f), 0.0f, 1.0f);
        masterPan = detail::clamp(detail::finiteOr(masterPan, 0.0f), -1.0f, 1.0f);
        masterGain = detail::clamp(detail::finiteOr(masterGain, 1.0f), 0.0f, 2.0f);
        triggerMode = detail::clamp(triggerMode, 0, 2);
        quantizeMode = detail::clamp(quantizeMode, 0, 8);

        bool used[kEffectCount] = {};
        for (int i = 0; i < kEffectCount; ++i) {
            int candidate = effectOrder[i];
            if (candidate < 0 || candidate >= kEffectCount || used[candidate]) {
                candidate = 0;
                while (candidate < kEffectCount && used[candidate])
                    ++candidate;
            }
            effectOrder[i] = candidate;
            used[candidate] = true;
        }
        for (int i = 0; i < kSceneCount; ++i)
            scenes[i].sanitize();
    }
};

class DeterministicRng {
public:
    static const std::uint32_t version = kRngVersion;

    explicit DeterministicRng(std::uint64_t seedValue = 1u) {
        reseed(seedValue);
    }

    void reseed(std::uint64_t seedValue) {
        std::uint64_t mixer = seedValue;
        state0_ = detail::splitMix64(mixer);
        state1_ = detail::splitMix64(mixer);
        if ((state0_ | state1_) == 0)
            state1_ = UINT64_C(0x9e3779b97f4a7c15);
    }

    std::uint64_t nextU64() {
        const std::uint64_t result = state0_ + state1_;
        const std::uint64_t nextState1 = state1_ ^ state0_;
        state0_ = detail::rotateLeft(state0_, 55) ^ nextState1 ^ (nextState1 << 14);
        state1_ = detail::rotateLeft(nextState1, 36);
        return result;
    }

    std::uint32_t nextU32() {
        return static_cast<std::uint32_t>(nextU64() >> 32);
    }

    float uniform() {
        // The upper 24 bits map exactly to the mantissa precision of float.
        return static_cast<float>(nextU64() >> 40) * (1.0f / 16777216.0f);
    }

    float bipolar() {
        return uniform() * 2.0f - 1.0f;
    }

    bool chance(float probability) {
        probability = detail::clamp(detail::finiteOr(probability, 0.0f), 0.0f, 1.0f);
        return uniform() < probability;
    }

    int uniformInt(int minimum, int maximum) {
        if (maximum < minimum) {
            const int temporary = minimum;
            minimum = maximum;
            maximum = temporary;
        }
        const std::uint64_t span = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(maximum) - static_cast<std::int64_t>(minimum)) + 1u;
        if (span <= 1u)
            return minimum;
        const std::int64_t result = static_cast<std::int64_t>(minimum)
            + static_cast<std::int64_t>(nextU64() % span);
        return static_cast<int>(result);
    }

private:
    std::uint64_t state0_;
    std::uint64_t state1_;
};

inline int weightedCategorical(const std::array<float, kEffectCount>& weights,
                               DeterministicRng& rng) {
    double total = 0.0;
    for (int i = 0; i < kEffectCount; ++i) {
        if (std::isfinite(weights[i]) && weights[i] > 0.0f)
            total += static_cast<double>(weights[i]);
    }
    if (!(total > 0.0) || !std::isfinite(total))
        return -1;

    const double target = static_cast<double>(rng.uniform()) * total;
    double cumulative = 0.0;
    int lastEligible = -1;
    for (int i = 0; i < kEffectCount; ++i) {
        if (!std::isfinite(weights[i]) || weights[i] <= 0.0f)
            continue;
        lastEligible = i;
        cumulative += static_cast<double>(weights[i]);
        if (target < cumulative)
            return i;
    }
    return lastEligible;
}

inline int selectWeightedEffect(const std::array<float, kEffectCount>& weights,
                                DeterministicRng& rng) {
    return weightedCategorical(weights, rng);
}

class SceneGenerator {
public:
    explicit SceneGenerator(std::uint32_t seedValue)
        : seed_(seedValue), rng_(seedValue) {}

    void reseed(std::uint32_t seedValue) {
        seed_ = seedValue;
        rng_.reseed(seedValue);
    }

    void randomize(SceneState& scene, bool randomizeEffects = true) {
        // Rewinding here makes a Scene Rand operation a pure function of its seed.
        rng_.reseed(seed_);
        scene.seed = seed_;
        scene.beats = rng_.uniformInt(1, 8);
        scene.divisions = rng_.uniformInt(2, 8);
        scene.loop = rng_.chance(0.8f);

        for (int i = 0; i < kEffectCount; ++i) {
            scene.randomWeights[i] = rng_.chance(0.18f) ? 0.0f : (0.1f + 0.9f * rng_.uniform());
        }
        randomizeLanes(scene);

        if (randomizeEffects) {
            for (int i = 0; i < kEffectCount; ++i)
                randomizeEffect(scene.effects[i]);
        }
        scene.sanitize();
    }

    // This intentionally leaves every effect setting untouched. It is the
    // Rack adapter's entry point after Module::onRandomize() has randomized the
    // visible ParamQuantities and copied them into SceneState.
    void randomizePattern(SceneState& scene) {
        randomize(scene, false);
    }

    void mutate(SceneState& scene, float amount = 0.15f, bool mutateEffects = true) {
        amount = detail::clamp(detail::finiteOr(amount, 0.15f), 0.0f, 1.0f);
        rng_.reseed(seed_);
        scene.seed = seed_;
        scene.sanitize();
        const int steps = scene.stepCount();

        const int patternEdits = 1 + static_cast<int>(amount * 8.0f);
        for (int edit = 0; edit < patternEdits; ++edit) {
            const int lane = rng_.uniformInt(0, kLaneCount - 1);
            const int step = rng_.uniformInt(0, steps - 1);
            const std::uint64_t bit = std::uint64_t(1) << step;
            if (scene.lanes[lane].activeMask & bit) {
                scene.lanes[lane].activeMask &= ~bit;
                scene.lanes[lane].startMask &= ~bit;
            }
            else {
                scene.lanes[lane].activeMask |= bit;
                scene.lanes[lane].startMask |= bit;
            }
            rebuildStarts(scene.lanes[lane], steps, scene.loop);
        }

        const int weightEdits = 1 + static_cast<int>(amount * 3.0f);
        for (int edit = 0; edit < weightEdits; ++edit) {
            const int index = rng_.uniformInt(0, kEffectCount - 1);
            scene.randomWeights[index] = detail::clamp(
                scene.randomWeights[index] + rng_.bipolar() * (0.2f * amount), 0.0f, 1.0f);
        }

        if (mutateEffects) {
            const float parameterDelta = 0.18f * amount;
            const int effectEdits = 1 + static_cast<int>(amount * 8.0f);
            for (int edit = 0; edit < effectEdits; ++edit) {
                EffectSettings& effect = scene.effects[rng_.uniformInt(0, kEffectCount - 1)];
                const int parameter = rng_.uniformInt(0, kEffectParamCount + 4);
                if (parameter < kEffectParamCount) {
                    effect.values[parameter] = detail::clamp(
                        effect.values[parameter] + rng_.bipolar() * parameterDelta, 0.0f, 1.0f);
                }
                else {
                    mutateCommon(effect.common, parameter - kEffectParamCount, parameterDelta);
                }
            }
        }
        scene.sanitize();
    }

private:
    static void rebuildStarts(LanePattern& lane, int stepCount, bool loop) {
        const std::uint64_t valid = LanePattern::validMask(stepCount);
        lane.activeMask &= valid;
        if (lane.activeMask == 0) {
            lane.startMask = 0;
            return;
        }

        std::uint64_t preceding = (lane.activeMask << 1) & valid;
        if (loop && (lane.activeMask & (std::uint64_t(1) << (stepCount - 1))))
            preceding |= std::uint64_t(1);
        lane.startMask = lane.activeMask & ~preceding;
        if (lane.startMask == 0) {
            // A full looping lane is one block beginning at cell zero.
            lane.startMask = std::uint64_t(1);
        }
    }

    void randomizeLanes(SceneState& scene) {
        const int steps = scene.stepCount();
        for (int laneIndex = 0; laneIndex < kLaneCount; ++laneIndex) {
            LanePattern& lane = scene.lanes[laneIndex];
            lane.clear();
            const float weight = laneIndex == 0
                ? 0.5f
                : scene.randomWeights[laneIndex - 1];
            const float startProbability = 0.035f + 0.14f * weight;

            int step = 0;
            while (step < steps) {
                if (!rng_.chance(startProbability)) {
                    ++step;
                    continue;
                }
                const int maximumLength = detail::clamp(1 + steps / 8, 1, 8);
                const int length = rng_.uniformInt(1, maximumLength);
                lane.startMask |= std::uint64_t(1) << step;
                for (int offset = 0; offset < length && step + offset < steps; ++offset)
                    lane.activeMask |= std::uint64_t(1) << (step + offset);
                step += length + 1; // Leave a gap so generated blocks remain distinct.
            }
            lane.sanitize(steps);
        }
    }

    void randomizeEffect(EffectSettings& effect) {
        effect.common.filterType = rng_.uniformInt(0, 4);
        effect.common.cutoff = rng_.uniform();
        effect.common.q = rng_.uniform();
        effect.common.mix = 0.35f + 0.65f * rng_.uniform();
        effect.common.pan = rng_.bipolar();
        effect.common.gain = 0.5f + 1.0f * rng_.uniform();
        for (int i = 0; i < kEffectParamCount; ++i)
            effect.values[i] = rng_.uniform();
    }

    void mutateCommon(CommonSettings& common, int parameter, float delta) {
        switch (parameter) {
        case 0:
            common.cutoff = detail::clamp(common.cutoff + rng_.bipolar() * delta, 0.0f, 1.0f);
            break;
        case 1:
            common.q = detail::clamp(common.q + rng_.bipolar() * delta, 0.0f, 1.0f);
            break;
        case 2:
            common.mix = detail::clamp(common.mix + rng_.bipolar() * delta, 0.0f, 1.0f);
            break;
        case 3:
            common.pan = detail::clamp(common.pan + rng_.bipolar() * delta, -1.0f, 1.0f);
            break;
        default:
            common.gain = detail::clamp(common.gain + rng_.bipolar() * (2.0f * delta), 0.0f, 2.0f);
            break;
        }
    }

    std::uint32_t seed_;
    DeterministicRng rng_;
};

inline void randomizeScene(SceneState& scene, std::uint32_t seed,
                           bool randomizeEffects = true) {
    SceneGenerator generator(seed);
    generator.randomize(scene, randomizeEffects);
}

inline void mutateScene(SceneState& scene, std::uint32_t seed, float amount = 0.15f,
                        bool mutateEffects = true) {
    SceneGenerator generator(seed);
    generator.mutate(scene, amount, mutateEffects);
}

enum ExternalClockMode {
    EXTERNAL_STEP = 0,
    EXTERNAL_BEAT = 1
};

enum ClockLockState {
    CLOCK_UNLOCKED = 0,
    CLOCK_FIRST_PULSE,
    CLOCK_LOCKED,
    CLOCK_STALE
};

struct TransportInput {
    double sampleRate;
    float bpm;
    int beats;
    int divisions;
    bool loop;
    bool run;
    bool resetEdge;
    bool externalClockConnected;
    bool clockEdge;
    ExternalClockMode externalMode;

    TransportInput()
        : sampleRate(48000.0), bpm(120.0f), beats(4), divisions(4), loop(true),
          run(true), resetEdge(false), externalClockConnected(false), clockEdge(false),
          externalMode(EXTERNAL_STEP) {}
};

struct TransportEvents {
    bool cellAdvanced;
    bool sceneEnded;
    bool beat;
    bool reset;
    bool clockAccepted;
    bool lockChanged;
    int previousCell;
    int cell;

    TransportEvents()
        : cellAdvanced(false), sceneEnded(false), beat(false), reset(false),
          clockAccepted(false), lockChanged(false), previousCell(0), cell(0) {}
};

class Transport {
public:
    Transport()
        : sampleRate_(48000.0), bpm_(120.0), beats_(4), divisions_(4), loop_(true),
          currentCell_(0), ended_(false), runningLastFrame_(true),
          externalConnected_(false), externalMode_(EXTERNAL_STEP),
          lockState_(CLOCK_UNLOCKED), secondsSinceEdge_(0.0), estimatedEdgePeriod_(0.0),
          internalCellPhase_(0.0), externalBeatPhase_(0.0), externalSubdivision_(0),
          resetGuardRemaining_(0.0), awaitingEdgeAfterRun_(false),
          resetBeatConfirmation_(true), effectiveBpm_(120.0) {}

    void reset() {
        currentCell_ = 0;
        ended_ = false;
        internalCellPhase_ = 0.0;
        externalBeatPhase_ = 0.0;
        externalSubdivision_ = 0;
        secondsSinceEdge_ = 0.0;
        estimatedEdgePeriod_ = 0.0;
        lockState_ = CLOCK_UNLOCKED;
        resetGuardRemaining_ = 0.001;
        resetBeatConfirmation_ = true;
        awaitingEdgeAfterRun_ = externalConnected_;
    }

    TransportEvents process(const TransportInput& input) {
        TransportEvents events;
        events.previousCell = currentCell_;

        configure(input, events);
        const double sampleTime = 1.0 / sampleRate_;

        if (input.resetEdge) {
            reset();
            events.reset = true;
            events.beat = true;
            events.lockChanged = true;
        }

        const bool run = input.run;
        if (!runningLastFrame_ && run && externalConnected_)
            awaitingEdgeAfterRun_ = true;

        if (externalConnected_)
            processExternal(input.clockEdge, run, sampleTime, events);
        else
            processInternal(run, sampleTime, events);

        if (resetGuardRemaining_ > 0.0) {
            resetGuardRemaining_ = detail::clamp(resetGuardRemaining_ - sampleTime, 0.0, 0.001);
            if (resetGuardRemaining_ < 1e-12)
                resetGuardRemaining_ = 0.0;
        }

        runningLastFrame_ = run;
        events.cell = currentCell_;
        return events;
    }

    int currentCell() const { return currentCell_; }
    int stepCount() const { return beats_ * divisions_; }
    int beats() const { return beats_; }
    int divisions() const { return divisions_; }
    bool hasEnded() const { return ended_; }
    bool isStale() const { return lockState_ == CLOCK_STALE; }
    ClockLockState lockState() const { return lockState_; }
    double effectiveBpm() const { return effectiveBpm_; }
    double estimatedExternalPeriod() const { return estimatedEdgePeriod_; }
    static std::uint32_t rngVersion() { return kRngVersion; }

private:
    void configure(const TransportInput& input, TransportEvents& events) {
        const double candidateRate = detail::finiteOr(input.sampleRate, sampleRate_);
        sampleRate_ = detail::clamp(candidateRate, 1.0, 768000.0);
        bpm_ = detail::clamp(
            static_cast<double>(detail::finiteOr(input.bpm, static_cast<float>(bpm_))),
            30.0, 300.0);

        const int newBeats = detail::clamp(input.beats, 1, 8);
        const int newDivisions = detail::clamp(input.divisions, 2, 8);
        if (newBeats != beats_ || newDivisions != divisions_) {
            beats_ = newBeats;
            divisions_ = newDivisions;
            const int steps = stepCount();
            currentCell_ = detail::clamp(currentCell_, 0, steps - 1);
            externalSubdivision_ = currentCell_ % divisions_;
            internalCellPhase_ = 0.0;
            externalBeatPhase_ = 0.0;
        }
        loop_ = input.loop;

        const ExternalClockMode newMode = input.externalMode == EXTERNAL_BEAT
            ? EXTERNAL_BEAT : EXTERNAL_STEP;
        if (input.externalClockConnected != externalConnected_ || newMode != externalMode_) {
            externalConnected_ = input.externalClockConnected;
            externalMode_ = newMode;
            lockState_ = CLOCK_UNLOCKED;
            secondsSinceEdge_ = 0.0;
            estimatedEdgePeriod_ = 0.0;
            externalBeatPhase_ = 0.0;
            externalSubdivision_ = 0;
            internalCellPhase_ = 0.0;
            awaitingEdgeAfterRun_ = externalConnected_ && !input.run;
            events.lockChanged = true;
        }

        if (!externalConnected_)
            effectiveBpm_ = bpm_;
    }

    void processInternal(bool run, double sampleTime, TransportEvents& events) {
        if (!run || ended_)
            return;
        const double cellPeriod = 60.0 / (bpm_ * static_cast<double>(divisions_));
        internalCellPhase_ += sampleTime;

        // Under valid Fray rates this runs once. The fixed bound also makes a
        // pathological sample-rate input safe without turning process() unbounded.
        int advances = 0;
        while (internalCellPhase_ + 1e-12 >= cellPeriod && advances < kMaxSteps) {
            internalCellPhase_ -= cellPeriod;
            advanceOneCell(events);
            ++advances;
            if (ended_)
                break;
        }
    }

    void processExternal(bool clockEdge, bool run, double sampleTime,
                         TransportEvents& events) {
        secondsSinceEdge_ += sampleTime;

        // With only one edge there is no measured period yet. Five seconds
        // accommodates a 30 BPM Beat clock (2 s period) plus the same 2.5x
        // grace used after lock, without guessing from the internal tempo.
        if (lockState_ == CLOCK_FIRST_PULSE && secondsSinceEdge_ >= 5.0) {
            lockState_ = CLOCK_STALE;
            events.lockChanged = true;
        }
        else if (lockState_ == CLOCK_LOCKED) {
            const double timeout = estimatedEdgePeriod_ > 0.0
                ? detail::clamp(estimatedEdgePeriod_ * 2.5, 0.5, 60.0)
                : 0.5;
            if (secondsSinceEdge_ >= timeout) {
                lockState_ = CLOCK_STALE;
                events.lockChanged = true;
            }
        }

        bool acceptedThisFrame = false;
        if (clockEdge && resetGuardRemaining_ <= 0.0) {
            acceptedThisFrame = true;
            acceptExternalEdge(run, events);
        }

        if (!acceptedThisFrame && run && !awaitingEdgeAfterRun_ && !ended_ &&
            externalMode_ == EXTERNAL_BEAT && lockState_ == CLOCK_LOCKED) {
            externalBeatPhase_ += sampleTime;
            int guard = 0;
            while (externalSubdivision_ < divisions_ - 1 && guard < 8) {
                const double threshold = estimatedEdgePeriod_ *
                    static_cast<double>(externalSubdivision_ + 1) /
                    static_cast<double>(divisions_);
                if (externalBeatPhase_ + 1e-12 < threshold)
                    break;
                ++externalSubdivision_;
                advanceOneCell(events);
                ++guard;
                if (ended_)
                    break;
            }
        }
    }

    void acceptExternalEdge(bool run, TransportEvents& events) {
        events.clockAccepted = true;
        const ClockLockState previousLock = lockState_;
        const double measuredPeriod = secondsSinceEdge_;

        if (previousLock == CLOCK_UNLOCKED || previousLock == CLOCK_STALE) {
            lockState_ = CLOCK_FIRST_PULSE;
            estimatedEdgePeriod_ = 0.0;
            events.lockChanged = true;
        }
        else {
            updatePeriodEstimate(measuredPeriod);
            if (lockState_ != CLOCK_LOCKED) {
                lockState_ = CLOCK_LOCKED;
                events.lockChanged = true;
            }
        }

        secondsSinceEdge_ = 0.0;
        externalBeatPhase_ = 0.0;
        externalSubdivision_ = 0;

        if (!run)
            return;

        awaitingEdgeAfterRun_ = false;
        if (externalMode_ == EXTERNAL_STEP) {
            advanceOneCell(events);
            resetBeatConfirmation_ = false;
            return;
        }

        if (resetBeatConfirmation_ && currentCell_ == 0) {
            events.beat = true;
            resetBeatConfirmation_ = false;
            return;
        }

        advanceToNextBeat(events);
        resetBeatConfirmation_ = false;
    }

    void updatePeriodEstimate(double measuredPeriod) {
        if (!std::isfinite(measuredPeriod) || measuredPeriod <= 0.0)
            return;
        measuredPeriod = detail::clamp(measuredPeriod, 1.0 / 768000.0, 60.0);
        if (!(estimatedEdgePeriod_ > 0.0)) {
            estimatedEdgePeriod_ = measuredPeriod;
        }
        else {
            const double relativeError = std::fabs(measuredPeriod - estimatedEdgePeriod_) /
                                         estimatedEdgePeriod_;
            const double alpha = relativeError < 0.02 ? 0.15
                               : (relativeError < 0.20 ? 0.50 : 1.0);
            estimatedEdgePeriod_ += alpha * (measuredPeriod - estimatedEdgePeriod_);
        }

        if (externalMode_ == EXTERNAL_BEAT)
            effectiveBpm_ = 60.0 / estimatedEdgePeriod_;
        else
            effectiveBpm_ = 60.0 /
                (estimatedEdgePeriod_ * static_cast<double>(divisions_));
        if (!std::isfinite(effectiveBpm_))
            effectiveBpm_ = bpm_;
    }

    void advanceOneCell(TransportEvents& events) {
        if (ended_)
            return;
        const int steps = stepCount();
        if (currentCell_ + 1 >= steps) {
            events.sceneEnded = true;
            if (!loop_) {
                ended_ = true;
                return;
            }
            currentCell_ = 0;
        }
        else {
            ++currentCell_;
        }
        events.cellAdvanced = true;
        if ((currentCell_ % divisions_) == 0)
            events.beat = true;
        resetBeatConfirmation_ = false;
    }

    void advanceToNextBeat(TransportEvents& events) {
        if (ended_)
            return;
        const int steps = stepCount();
        const int target = ((currentCell_ / divisions_) + 1) * divisions_;
        if (target >= steps) {
            events.sceneEnded = true;
            if (!loop_) {
                ended_ = true;
                return;
            }
            currentCell_ = 0;
        }
        else {
            currentCell_ = target;
        }
        events.cellAdvanced = true;
        events.beat = true;
    }

    double sampleRate_;
    double bpm_;
    int beats_;
    int divisions_;
    bool loop_;
    int currentCell_;
    bool ended_;
    bool runningLastFrame_;
    bool externalConnected_;
    ExternalClockMode externalMode_;
    ClockLockState lockState_;
    double secondsSinceEdge_;
    double estimatedEdgePeriod_;
    double internalCellPhase_;
    double externalBeatPhase_;
    int externalSubdivision_;
    double resetGuardRemaining_;
    bool awaitingEdgeAfterRun_;
    bool resetBeatConfirmation_;
    double effectiveBpm_;
};

} // namespace Fray
} // namespace ShortwavDSP
