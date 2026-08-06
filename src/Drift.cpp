#include "Drift.hpp"

#include <algorithm>

namespace {

shortwav::drift::Controls readControls(Drift* module) {
  shortwav::drift::Controls controls;
  controls.offset = module->params[Drift::OFFSET_PARAM].getValue();
  controls.speed = module->params[Drift::SPEED_PARAM].getValue();
  controls.range = module->params[Drift::RANGE_PARAM].getValue();
  controls.mix = module->params[Drift::MIX_PARAM].getValue();
  return controls;
}

} // namespace

void Drift::process(const ProcessArgs &args)
{
  shortwav::drift::Frame frame;
  frame.sampleTime = args.sampleTime;
  frame.inputVolts = inputs[SIGNAL_INPUT].getVoltage();
  frame.inputConnected = inputs[SIGNAL_INPUT].isConnected();

  const shortwav::drift::Outputs out = engine.process(frame, readControls(this));

  outputs[CV_OUTPUT].setChannels(1);
  outputs[CV_OUTPUT].setVoltage(out.outputVolts);

  lights[OUTPUT_POS_LIGHT].setBrightnessSmooth(std::max(0.f, out.outputVolts) / 10.f, args.sampleTime);
  lights[OUTPUT_NEG_LIGHT].setBrightnessSmooth(std::max(0.f, -out.outputVolts) / 10.f, args.sampleTime);
  lights[ACTIVITY_LIGHT].setBrightnessSmooth(out.activity, args.sampleTime);
}

Model *modelDrift = createModel<Drift, DriftWidget>("Drift");
