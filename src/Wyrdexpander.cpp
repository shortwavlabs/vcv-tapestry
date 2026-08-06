#include "Wyrdexpander.hpp"

#include <cmath>

namespace {

float safeVoltage(float v, float lo = -12.f, float hi = 12.f) {
  if (!std::isfinite(v)) {
    return 0.f;
  }
  return clamp(v, lo, hi);
}

} // namespace

void Wyrdexpander::process(const ProcessArgs &args)
{
  bool linked = false;
  float modularVolts = 0.f;
  float cv2Volts = 0.f;

  if (leftExpander.module && leftExpander.module->model == modelWyrd) {
    const auto* message = static_cast<const shortwav::wyrd::WyrdReverbExpanderMessage*>(
      leftExpander.module->rightExpander.consumerMessage);
    if (message && message->active) {
      linked = true;
      modularVolts = message->modularVolts;
      cv2Volts = message->cv2Volts;
    }
  }

  if (!linked) {
    reverb.reset();
    outputs[REVERB_OUTPUT].setVoltage(0.f);
    lights[LINK_LIGHT].setBrightnessSmooth(0.f, args.sampleTime);
    lights[REVERB_LIGHT].setBrightnessSmooth(0.f, args.sampleTime);
    return;
  }

  const float sampleRate = args.sampleTime > 0.f ? 1.f / args.sampleTime : 48000.f;
  const float cv2Motion = clamp(std::fabs(cv2Volts) / 5.f, 0.f, 1.f);
  const float size = params[SIZE_PARAM].getValue();
  const float decay = clamp(params[DECAY_PARAM].getValue() + 0.06f * cv2Motion, 0.f, 1.f);
  const float diffusion = params[DIFFUSION_PARAM].getValue();
  const float tone = clamp(params[TONE_PARAM].getValue() - 0.08f * cv2Motion, 0.f, 1.f);
  const float modulation = clamp(params[MOD_PARAM].getValue() + 0.12f * cv2Motion, 0.f, 1.f);
  const float level = params[LEVEL_PARAM].getValue();

  const shortwav::wyrd::WyrdReverbResult wet = reverb.process(modularVolts / 5.f, size, decay,
                                                              diffusion, tone, modulation, sampleRate);
  const float outVolts = safeVoltage(shortwav::wyrd::softClip(wet.mono * (0.25f + 1.45f * level)) * 5.f,
                                     -5.5f, 5.5f);
  outputs[REVERB_OUTPUT].setVoltage(outVolts);
  lights[LINK_LIGHT].setBrightnessSmooth(1.f, args.sampleTime);
  lights[REVERB_LIGHT].setBrightnessSmooth(std::fabs(outVolts) / 5.f, args.sampleTime);

}

Model *modelWyrdexpander = createModel<Wyrdexpander, WyrdexpanderWidget>("Wyrdexpander");
