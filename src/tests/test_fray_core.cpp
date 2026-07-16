#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>

#include "dsp/fray-core.h"

using namespace ShortwavDSP::Fray;

namespace {

std::size_t gAllocationCount = 0;

bool sameScene(const SceneState& left, const SceneState& right) {
    if (left.beats != right.beats || left.divisions != right.divisions ||
        left.loop != right.loop || left.seed != right.seed)
        return false;
    for (int lane = 0; lane < kLaneCount; ++lane) {
        if (left.lanes[lane].activeMask != right.lanes[lane].activeMask ||
            left.lanes[lane].startMask != right.lanes[lane].startMask)
            return false;
    }
    for (int effect = 0; effect < kEffectCount; ++effect) {
        if (left.randomWeights[effect] != right.randomWeights[effect])
            return false;
        const CommonSettings& a = left.effects[effect].common;
        const CommonSettings& b = right.effects[effect].common;
        if (a.filterType != b.filterType || a.cutoff != b.cutoff || a.q != b.q ||
            a.mix != b.mix || a.pan != b.pan || a.gain != b.gain)
            return false;
        for (int parameter = 0; parameter < kEffectParamCount; ++parameter) {
            if (left.effects[effect].values[parameter] !=
                right.effects[effect].values[parameter])
                return false;
        }
    }
    return true;
}

void clearEdges(TransportInput& input) {
    input.clockEdge = false;
    input.resetEdge = false;
}

TransportEvents processSamples(Transport& transport, TransportInput& input, int count) {
    TransportEvents last;
    for (int i = 0; i < count; ++i) {
        clearEdges(input);
        last = transport.process(input);
    }
    return last;
}

TransportEvents clockEdge(Transport& transport, TransportInput& input) {
    input.resetEdge = false;
    input.clockEdge = true;
    TransportEvents result = transport.process(input);
    input.clockEdge = false;
    return result;
}

void testConstantsAndDefaults() {
    static_assert(kLaneCount == 11, "Fray lane count is persisted");
    static_assert(kEffectCount == 10, "Fray effect count is persisted");
    static_assert(kMaxSteps == 64, "Fray mask width is persisted");
    static_assert(kSceneCount == 128, "Fray scene count is persisted");
    static_assert(kEffectParamCount == 12, "Fray effect parameter count is persisted");
    static_assert(MODULATOR == 0 && TAPE_STOP == 1 && RETRIGGER == 2 &&
                  REVERSER == 3 && STRETCHER == 4 && LOFI == 5 &&
                  DISTORTION == 6 && GATER == 7 && DELAY == 8 && SHUFFLER == 9,
                  "Effect IDs are serialized and must never be reordered");
    static_assert(DeterministicRng::version == 1u, "RNG version must be explicit");

    CommonSettings common;
    assert(common.filterType == 0);
    assert(common.cutoff >= 0.0f && common.cutoff <= 1.0f);
    assert(common.q >= 0.0f && common.q <= 1.0f);
    assert(common.mix == 1.0f);
    assert(common.pan == 0.0f);
    assert(common.gain == 1.0f);

    EffectSettings effect;
    for (int i = 0; i < kEffectParamCount; ++i)
        assert(effect.values[i] == 0.5f);

    SceneState scene;
    assert(scene.beats == 4);
    assert(scene.divisions == 4);
    assert(scene.loop);
    assert(scene.stepCount() == 16);
    for (int i = 0; i < kEffectCount; ++i)
        assert(scene.randomWeights[i] == 1.0f);
    assert(scene.effects[DISTORTION].values[7] == 0.0f);

    ProgramState program;
    assert(program.masterMix == 1.0f);
    assert(program.masterPan == 0.0f);
    assert(program.masterGain == 1.0f);
    for (int i = 0; i < kEffectCount; ++i)
        assert(program.effectOrder[i] == i);
}

void testLanePatterns() {
    LanePattern lane;
    lane.activeMask = UINT64_C(0x3);
    lane.startMask = UINT64_C(0x3); // Adjacent cells are two distinct blocks.
    assert(lane.isActive(0, 2));
    assert(lane.active(1, 2));
    assert(lane.blockStart(0, 2));
    assert(lane.blockStart(1, 2));
    assert(lane.blockEnd(0, 2, false));
    assert(lane.blockEnd(1, 2, false));
    assert(lane.blockEnd(1, 2, true));
    assert(!lane.isActive(-1, 2));
    assert(!lane.isActive(2, 2));

    lane.startMask = UINT64_C(0x1);
    assert(!lane.isEnd(0, 2, false));
    assert(lane.isEnd(1, 2, false));
    assert(lane.isEnd(1, 2, true)); // Wrapped cell zero begins a fresh block.

    lane.activeMask = (UINT64_C(1) << 63);
    lane.startMask = lane.activeMask;
    assert(lane.isActive(63, 64));
    assert(lane.isStart(63, 64));
    assert(lane.isEnd(63, 64, false));
    assert(!lane.isActive(64, 64));
    assert(LanePattern::validMask(64) == ~UINT64_C(0));

    lane.activeMask = ~UINT64_C(0);
    lane.startMask = ~UINT64_C(0);
    lane.sanitize(2);
    assert(lane.activeMask == UINT64_C(0x3));
    assert((lane.startMask & ~lane.activeMask) == 0);

    lane.activeMask = UINT64_C(0x6); // Cells 1-2, with a missing start bit.
    lane.startMask = 0;
    lane.sanitize(4);
    assert(lane.startMask == UINT64_C(0x2));
}

void testSanitizers() {
    CommonSettings common;
    common.filterType = 99;
    common.cutoff = std::numeric_limits<float>::quiet_NaN();
    common.q = -3.0f;
    common.mix = 4.0f;
    common.pan = -9.0f;
    common.gain = std::numeric_limits<float>::infinity();
    common.sanitize();
    assert(common.filterType == 4);
    assert(common.cutoff == 0.75f);
    assert(common.q == 0.0f);
    assert(common.mix == 1.0f);
    assert(common.pan == -1.0f);
    assert(common.gain == 1.0f);

    EffectSettings effect;
    effect.values[0] = -1.0f;
    effect.values[1] = 2.0f;
    effect.values[2] = std::numeric_limits<float>::quiet_NaN();
    effect.sanitize();
    assert(effect.values[0] == 0.0f);
    assert(effect.values[1] == 1.0f);
    assert(effect.values[2] == 0.5f);

    SceneState scene;
    scene.beats = -20;
    scene.divisions = 99;
    scene.randomWeights[0] = std::numeric_limits<float>::quiet_NaN();
    scene.randomWeights[1] = -1.0f;
    scene.randomWeights[2] = 4.0f;
    scene.lanes[0].activeMask = ~UINT64_C(0);
    scene.lanes[0].startMask = ~UINT64_C(0);
    scene.sanitize();
    assert(scene.beats == 1);
    assert(scene.divisions == 8);
    assert(scene.stepCount() == 8);
    assert(scene.randomWeights[0] == 0.0f);
    assert(scene.randomWeights[1] == 0.0f);
    assert(scene.randomWeights[2] == 1.0f);
    assert((scene.lanes[0].activeMask & ~UINT64_C(0xff)) == 0);

    ProgramState program;
    program.masterMix = -1.0f;
    program.masterPan = 8.0f;
    program.masterGain = std::numeric_limits<float>::quiet_NaN();
    program.triggerMode = 50;
    program.quantizeMode = -50;
    program.effectOrder.fill(0);
    program.sanitize();
    assert(program.masterMix == 0.0f);
    assert(program.masterPan == 1.0f);
    assert(program.masterGain == 1.0f);
    assert(program.triggerMode == 2);
    assert(program.quantizeMode == 0);
    for (int i = 0; i < kEffectCount; ++i)
        assert(program.effectOrder[i] == i);
}

void testRngAndWeightedSelection() {
    DeterministicRng rng(1u);
    assert(rng.nextU64() == UINT64_C(0x4ff5bb8dee914928));
    assert(rng.nextU64() == UINT64_C(0xf00568db34fbb666));
    assert(rng.nextU64() == UINT64_C(0x0e9fd07a18ca873a));
    assert(rng.nextU64() == UINT64_C(0x67f9681f781744de));
    assert(rng.nextU64() == UINT64_C(0x0bdbd4a1a166a8e8));

    DeterministicRng a(12345u);
    DeterministicRng b(12345u);
    for (int i = 0; i < 1000; ++i) {
        assert(a.nextU64() == b.nextU64());
        const float value = a.uniform();
        assert(value == b.uniform());
        assert(value >= 0.0f && value < 1.0f);
    }

    // Exercise the full signed range without overflowing the result addition.
    for (int i = 0; i < 1000; ++i) {
        const int value = a.uniformInt(
            std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        (void) value;
    }

    std::array<float, kEffectCount> weights;
    weights.fill(0.0f);
    assert(weightedCategorical(weights, a) == -1);
    weights[2] = -4.0f;
    weights[4] = std::numeric_limits<float>::quiet_NaN();
    assert(selectWeightedEffect(weights, a) == -1);
    weights[7] = 1.0f;
    for (int i = 0; i < 100; ++i)
        assert(weightedCategorical(weights, a) == 7);

    weights.fill(1.0f);
    bool selected[kEffectCount] = {};
    for (int i = 0; i < 1000; ++i) {
        const int result = weightedCategorical(weights, a);
        assert(result >= 0 && result < kEffectCount);
        selected[result] = true;
    }
    for (int i = 0; i < kEffectCount; ++i)
        assert(selected[i]);
}

void assertSceneMasksAreValid(const SceneState& scene) {
    const std::uint64_t valid = LanePattern::validMask(scene.stepCount());
    for (int lane = 0; lane < kLaneCount; ++lane) {
        assert((scene.lanes[lane].activeMask & ~valid) == 0);
        assert((scene.lanes[lane].startMask & ~scene.lanes[lane].activeMask) == 0);
    }
}

void testSceneGenerator() {
    SceneState first;
    SceneState second;
    randomizeScene(first, UINT32_C(0x12345678));
    randomizeScene(second, UINT32_C(0x12345678));
    assert(sameScene(first, second));
    assert(first.seed == UINT32_C(0x12345678));
    assert(first.beats >= 1 && first.beats <= 8);
    assert(first.divisions >= 2 && first.divisions <= 8);
    assertSceneMasksAreValid(first);

    SceneState patternOnly;
    for (int effect = 0; effect < kEffectCount; ++effect) {
        patternOnly.effects[effect].common.pan = -0.75f + 0.1f * effect;
        for (int parameter = 0; parameter < kEffectParamCount; ++parameter)
            patternOnly.effects[effect].values[parameter] = 0.01f * (effect + parameter);
    }
    const std::array<EffectSettings, kEffectCount> before = patternOnly.effects;
    SceneGenerator generator(99u);
    generator.randomizePattern(patternOnly);
    for (int effect = 0; effect < kEffectCount; ++effect) {
        assert(patternOnly.effects[effect].common.pan == before[effect].common.pan);
        for (int parameter = 0; parameter < kEffectParamCount; ++parameter)
            assert(patternOnly.effects[effect].values[parameter] ==
                   before[effect].values[parameter]);
    }
    assertSceneMasksAreValid(patternOnly);

    SceneState mutateA = first;
    SceneState mutateB = first;
    mutateScene(mutateA, 777u, 0.15f);
    mutateScene(mutateB, 777u, 0.15f);
    assert(sameScene(mutateA, mutateB));
    assert(!sameScene(first, mutateA));
    assert(mutateA.beats == first.beats);
    assert(mutateA.divisions == first.divisions);
    assertSceneMasksAreValid(mutateA);

    for (int effect = 0; effect < kEffectCount; ++effect) {
        assert(mutateA.randomWeights[effect] >= 0.0f && mutateA.randomWeights[effect] <= 1.0f);
        const CommonSettings& common = mutateA.effects[effect].common;
        assert(common.filterType >= 0 && common.filterType <= 4);
        assert(common.cutoff >= 0.0f && common.cutoff <= 1.0f);
        assert(common.q >= 0.0f && common.q <= 1.0f);
        assert(common.mix >= 0.0f && common.mix <= 1.0f);
        assert(common.pan >= -1.0f && common.pan <= 1.0f);
        assert(common.gain >= 0.0f && common.gain <= 2.0f);
        for (int parameter = 0; parameter < kEffectParamCount; ++parameter) {
            assert(mutateA.effects[effect].values[parameter] >= 0.0f);
            assert(mutateA.effects[effect].values[parameter] <= 1.0f);
        }
    }
}

void testInternalTransport() {
    Transport transport;
    TransportInput input;
    input.sampleRate = 1000.0;
    input.bpm = 120.0f;
    input.beats = 1;
    input.divisions = 4;
    input.loop = true;

    for (int i = 0; i < 124; ++i) {
        const TransportEvents event = transport.process(input);
        assert(!event.cellAdvanced);
    }
    TransportEvents event = transport.process(input);
    assert(event.cellAdvanced);
    assert(event.cell == 1);
    assert(!event.beat);

    processSamples(transport, input, 250);
    assert(transport.currentCell() == 3);
    event = processSamples(transport, input, 125);
    assert(event.cellAdvanced);
    assert(event.sceneEnded);
    assert(event.beat);
    assert(event.cell == 0);

    Transport paused;
    processSamples(paused, input, 100);
    input.run = false;
    processSamples(paused, input, 1000);
    assert(paused.currentCell() == 0);
    input.run = true;
    processSamples(paused, input, 24);
    assert(paused.currentCell() == 0);
    event = processSamples(paused, input, 1);
    assert(event.cellAdvanced && event.cell == 1);

    Transport oneShot;
    input = TransportInput();
    input.sampleRate = 1000.0;
    input.bpm = 120.0f;
    input.beats = 1;
    input.divisions = 2;
    input.loop = false;
    event = processSamples(oneShot, input, 250);
    assert(event.cellAdvanced && event.cell == 1);
    event = processSamples(oneShot, input, 250);
    assert(!event.cellAdvanced);
    assert(event.sceneEnded);
    assert(oneShot.hasEnded());
    processSamples(oneShot, input, 1000);
    assert(oneShot.currentCell() == 1);
    input.resetEdge = true;
    event = oneShot.process(input);
    assert(event.reset && event.beat && event.cell == 0);
    assert(!oneShot.hasEnded());

    Transport clamped;
    input = TransportInput();
    input.sampleRate = 1000.0;
    input.bpm = 100000.0f;
    input.beats = -5;
    input.divisions = 99;
    event = processSamples(clamped, input, 25);
    assert(event.cellAdvanced);
    assert(clamped.beats() == 1);
    assert(clamped.divisions() == 8);
    assert(clamped.effectiveBpm() == 300.0);
}

void testExternalStepTransport() {
    Transport transport;
    TransportInput input;
    input.sampleRate = 1000.0;
    input.beats = 4;
    input.divisions = 4;
    input.externalClockConnected = true;
    input.externalMode = EXTERNAL_STEP;

    TransportEvents event = clockEdge(transport, input);
    assert(event.clockAccepted);
    assert(event.cellAdvanced && event.cell == 1);
    assert(transport.lockState() == CLOCK_FIRST_PULSE);

    processSamples(transport, input, 99);
    event = clockEdge(transport, input);
    assert(event.clockAccepted);
    assert(event.cellAdvanced && event.cell == 2);
    assert(transport.lockState() == CLOCK_LOCKED);
    assert(std::fabs(transport.estimatedExternalPeriod() - 0.1) < 1e-9);
    assert(std::fabs(transport.effectiveBpm() - 150.0) < 1e-9);

    processSamples(transport, input, 499);
    assert(transport.lockState() == CLOCK_LOCKED);
    processSamples(transport, input, 1);
    assert(transport.isStale());
    event = clockEdge(transport, input);
    assert(event.cellAdvanced && event.cell == 3);
    assert(transport.lockState() == CLOCK_FIRST_PULSE);

    Transport paused;
    input.run = false;
    event = clockEdge(paused, input);
    assert(event.clockAccepted && !event.cellAdvanced);
    processSamples(paused, input, 99);
    event = clockEdge(paused, input);
    assert(event.clockAccepted && !event.cellAdvanced);
    assert(paused.lockState() == CLOCK_LOCKED);
    input.run = true;
    processSamples(paused, input, 50);
    assert(paused.currentCell() == 0);
    event = clockEdge(paused, input);
    assert(event.cellAdvanced && event.cell == 1);
}

void testResetSuppression() {
    Transport transport;
    TransportInput input;
    input.sampleRate = 48000.0;
    input.externalClockConnected = true;
    input.externalMode = EXTERNAL_STEP;
    input.resetEdge = true;
    input.clockEdge = true;
    TransportEvents event = transport.process(input);
    assert(event.reset);
    assert(!event.clockAccepted);
    assert(transport.currentCell() == 0);

    input.resetEdge = false;
    for (int i = 0; i < 47; ++i) {
        input.clockEdge = true;
        event = transport.process(input);
        assert(!event.clockAccepted);
    }
    input.clockEdge = true;
    event = transport.process(input);
    assert(event.clockAccepted);
    assert(event.cellAdvanced && event.cell == 1);
}

void testExternalBeatTransport() {
    Transport transport;
    TransportInput input;
    input.sampleRate = 1000.0;
    input.beats = 4;
    input.divisions = 4;
    input.externalClockConnected = true;
    input.externalMode = EXTERNAL_BEAT;

    TransportEvents event = clockEdge(transport, input);
    assert(event.clockAccepted);
    assert(!event.cellAdvanced);
    assert(event.beat);
    assert(event.cell == 0);
    assert(transport.lockState() == CLOCK_FIRST_PULSE);

    processSamples(transport, input, 399);
    assert(transport.currentCell() == 0); // First pulse does not predict cells.
    event = clockEdge(transport, input);
    assert(event.cellAdvanced && event.beat && event.cell == 4);
    assert(transport.lockState() == CLOCK_LOCKED);
    assert(std::fabs(transport.effectiveBpm() - 150.0) < 1e-9);

    processSamples(transport, input, 99);
    assert(transport.currentCell() == 4);
    event = processSamples(transport, input, 1);
    assert(event.cellAdvanced && !event.beat && event.cell == 5);
    processSamples(transport, input, 200);
    assert(transport.currentCell() == 7);
    processSamples(transport, input, 100);
    assert(transport.currentCell() == 7); // Hold; do not invent the next beat.
    event = clockEdge(transport, input);
    assert(event.cellAdvanced && event.beat && event.cell == 8);

    // The acquisition window must allow a 30 BPM beat clock to reach pulse two.
    Transport slow;
    TransportInput slowInput;
    slowInput.sampleRate = 1000.0;
    slowInput.beats = 4;
    slowInput.divisions = 4;
    slowInput.externalClockConnected = true;
    slowInput.externalMode = EXTERNAL_BEAT;
    clockEdge(slow, slowInput);
    processSamples(slow, slowInput, 1999);
    assert(slow.lockState() == CLOCK_FIRST_PULSE);
    event = clockEdge(slow, slowInput);
    assert(slow.lockState() == CLOCK_LOCKED);
    assert(std::fabs(slow.effectiveBpm() - 30.0) < 1e-9);

    // Hot-patching Beat clock after internal movement snaps forward to a beat.
    Transport hotPatch;
    input = TransportInput();
    input.sampleRate = 1000.0;
    input.bpm = 120.0f;
    input.beats = 4;
    input.divisions = 4;
    processSamples(hotPatch, input, 250);
    assert(hotPatch.currentCell() == 2);
    input.externalClockConnected = true;
    input.externalMode = EXTERNAL_BEAT;
    event = clockEdge(hotPatch, input);
    assert(event.cellAdvanced && event.beat && event.cell == 4);

    // Every supported division count schedules exactly divisions - 1 cells.
    for (int division = 2; division <= 8; ++division) {
        Transport divided;
        TransportInput dividedInput;
        dividedInput.sampleRate = 8000.0;
        dividedInput.beats = 8;
        dividedInput.divisions = division;
        dividedInput.externalClockConnected = true;
        dividedInput.externalMode = EXTERNAL_BEAT;
        clockEdge(divided, dividedInput);
        processSamples(divided, dividedInput, 799);
        clockEdge(divided, dividedInput); // One-second beat period; enter beat one.
        const int beatStart = division;
        assert(divided.currentCell() == beatStart);
        processSamples(divided, dividedInput, 8000);
        assert(divided.currentCell() == beatStart + division - 1);
    }
}

void testFiniteTransportAndNoAllocations() {
    Transport transport;
    TransportInput input;
    input.sampleRate = std::numeric_limits<double>::quiet_NaN();
    input.bpm = std::numeric_limits<float>::infinity();
    input.beats = std::numeric_limits<int>::min();
    input.divisions = std::numeric_limits<int>::max();
    TransportEvents event = transport.process(input);
    assert(event.cell >= 0 && event.cell < transport.stepCount());
    assert(std::isfinite(transport.effectiveBpm()));

    input = TransportInput();
    input.sampleRate = 192000.0;
    input.externalClockConnected = true;
    input.externalMode = EXTERNAL_BEAT;
    const std::size_t before = gAllocationCount;
    for (int i = 0; i < 100000; ++i) {
        input.clockEdge = (i % 48000) == 0;
        input.resetEdge = (i == 50000);
        transport.process(input);
    }
    assert(gAllocationCount == before);
}

} // namespace

void* operator new(std::size_t size) {
    ++gAllocationCount;
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    ++gAllocationCount;
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

int main() {
    testConstantsAndDefaults();
    testLanePatterns();
    testSanitizers();
    testRngAndWeightedSelection();
    testSceneGenerator();
    testInternalTransport();
    testExternalStepTransport();
    testResetSuppression();
    testExternalBeatTransport();
    testFiniteTransportAndNoAllocations();
    return 0;
}
