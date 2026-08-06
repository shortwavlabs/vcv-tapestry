#pragma once

#include <algorithm>
#include <cmath>

#include "generated/drift_core.h"

namespace shortwav {
namespace drift {

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline float safeFinite(float v, float fallback = 0.f) {
  return std::isfinite(v) ? v : fallback;
}

struct Controls {
  float offset = 0.5f;
  float speed = 0.38f;
  float range = 0.72f;
  float mix = 0.f;
};

struct Frame {
  float sampleTime = 1.f / 48000.f;
  float inputVolts = 0.f;
  bool inputConnected = false;
};

struct Outputs {
  float outputVolts = 0.f;
  float randomVolts = 0.f;
  float mixedVolts = 0.f;
  float activity = 0.f;
};

struct DriftCoreProcessor {
  Drift_core_process_type context;

  DriftCoreProcessor() {
    reset();
  }

  void reset() {
    Drift_core_process_init(context);
  }

  Outputs process(const Frame& frame, const Controls& controls) {
    const float sampleTime = clampf(safeFinite(frame.sampleTime, 1.f / 48000.f),
                                    1.f / 384000.f, 1.f / 1000.f);
    const float sampleRate = 1.f / sampleTime;
    const float inputNorm = clampf(safeFinite(frame.inputVolts) / 5.f, -1.4f, 1.4f);

    Drift_core_process(context,
                       clampf(safeFinite(controls.speed, 0.38f), 0.f, 1.f),
                       clampf(safeFinite(controls.range, 0.72f), 0.f, 1.f),
                       clampf(safeFinite(controls.offset, 0.5f), 0.f, 1.f),
                       inputNorm,
                       clampf(safeFinite(controls.mix), -1.f, 1.f),
                       frame.inputConnected ? 1.f : 0.f,
                       sampleRate);

    const float randomNorm = Drift_core_process_ret_0(context);
    const float mixedNorm = Drift_core_process_ret_1(context);
    const float outputNorm = Drift_core_process_ret_2(context);
    const float activity = Drift_core_process_ret_3(context);
    if (!std::isfinite(outputNorm) || !std::isfinite(randomNorm)
        || !std::isfinite(mixedNorm) || !std::isfinite(activity)) {
      reset();
      return {};
    }

    Outputs out;
    out.randomVolts = clampf(safeFinite(randomNorm) * 5.f, -6.25f, 6.25f);
    out.mixedVolts = clampf(safeFinite(mixedNorm) * 5.f, -7.f, 7.f);
    out.outputVolts = clampf(safeFinite(outputNorm) * 5.f, -10.f, 10.f);
    out.activity = clampf(safeFinite(activity), 0.f, 1.f);
    return out;
  }
};

struct Engine {
  DriftCoreProcessor core;

  void reset() {
    core.reset();
  }

  Outputs process(const Frame& frame, const Controls& controls) {
    return core.process(frame, controls);
  }
};

} // namespace drift
} // namespace shortwav
