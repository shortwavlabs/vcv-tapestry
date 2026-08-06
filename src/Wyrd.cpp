#include "Wyrd.hpp"

namespace {

float safeVoltage(float v, float lo = -12.f, float hi = 12.f) {
  if (!std::isfinite(v)) {
    return 0.f;
  }
  return clamp(v, lo, hi);
}

shortwav::wyrd::Controls readControls(Wyrd* module) {
  shortwav::wyrd::Controls controls;
  controls.strength = module->params[Wyrd::STRENGTH_PARAM].getValue();
  controls.externalConstant = module->params[Wyrd::EXTERNAL_CONSTANT_PARAM].getValue();
  controls.activationConstant = module->params[Wyrd::ACTIVATION_CONSTANT_PARAM].getValue();
  controls.activationInterference = module->params[Wyrd::ACTIVATION_INTERFERENCE_PARAM].getValue();
  controls.activationCvAmount = module->params[Wyrd::ACTIVATION_CV_AMOUNT_PARAM].getValue();
  controls.tonicCoarse = module->params[Wyrd::TONIC_COARSE_PARAM].getValue();
  controls.tonicFine = module->params[Wyrd::TONIC_FINE_PARAM].getValue();
  controls.tonicModAmount = module->params[Wyrd::TONIC_MOD_AMOUNT_PARAM].getValue();
  controls.tones = module->params[Wyrd::TONES_PARAM].getValue();
  controls.tonesCvAmount = module->params[Wyrd::TONES_CV_AMOUNT_PARAM].getValue();
  controls.timeCoarse = module->params[Wyrd::TIME_COARSE_PARAM].getValue();
  controls.timeFine = module->params[Wyrd::TIME_FINE_PARAM].getValue();
  controls.timeModAmount = module->params[Wyrd::TIME_MOD_AMOUNT_PARAM].getValue();
  controls.timeCvAmount = module->params[Wyrd::TIME_CV_AMOUNT_PARAM].getValue();
  controls.decay = module->params[Wyrd::DECAY_PARAM].getValue();
  controls.decayCvAmount = module->params[Wyrd::DECAY_CV_AMOUNT_PARAM].getValue();
  controls.filter = module->params[Wyrd::FILTER_PARAM].getValue();
  controls.filterCvAmount = module->params[Wyrd::FILTER_CV_AMOUNT_PARAM].getValue();
  controls.absorb = module->params[Wyrd::ABSORB_PARAM].getValue();
  controls.blend = module->params[Wyrd::BLEND_PARAM].getValue();
  controls.level = module->params[Wyrd::LEVEL_PARAM].getValue();
  controls.speed = module->params[Wyrd::SPEED_PARAM].getValue();
  controls.angle = module->params[Wyrd::ANGLE_PARAM].getValue();
  controls.speedCvAmount = module->params[Wyrd::SPEED_CV_AMOUNT_PARAM].getValue();
  controls.touchActivation = module->params[Wyrd::TOUCH_ACTIVATE_PARAM].getValue();
  controls.touchTime = module->params[Wyrd::TOUCH_TIME_PARAM].getValue();
  controls.touchFilter = module->params[Wyrd::TOUCH_FILTER_PARAM].getValue();
  controls.touchAbsorb = module->params[Wyrd::TOUCH_ABSORB_PARAM].getValue();
  controls.touchTonic = module->params[Wyrd::TOUCH_TONIC_PARAM].getValue();
  controls.touchDecay = module->params[Wyrd::TOUCH_DECAY_PARAM].getValue();
  controls.touchSource = clamp(module->touchSource, 0, shortwav::wyrd::kNumTouchSources - 1);
  controls.strengthCalibration = clamp(module->strengthCalibration, 0, shortwav::wyrd::kNumStrengthCalibrations - 1);
  return controls;
}

} // namespace

void Wyrd::process(const ProcessArgs &args)
{
  shortwav::wyrd::Frame frame;
  frame.sampleTime = args.sampleTime;

  frame.externalVolts = inputs[EXTERNAL_INPUT].getVoltage();
  frame.beginEndVolts = inputs[BEGIN_END_INPUT].getVoltage();
  frame.speedCvVolts = inputs[SPEED_CV_INPUT].getVoltage();
  frame.activationCvVolts = inputs[ACTIVATION_CV_INPUT].getVoltage();
  frame.tonicModVolts = inputs[TONIC_MOD_INPUT].getVoltage();
  frame.tonesCvVolts = inputs[TONES_CV_INPUT].getVoltage();
  frame.vOctVolts = inputs[V_OCT_INPUT].getVoltage();
  frame.timeModVolts = inputs[TIME_MOD_INPUT].getVoltage();
  frame.timeCvVolts = inputs[TIME_CV_INPUT].getVoltage();
  frame.timeUnityCvVolts = inputs[TIME_UNITY_CV_INPUT].getVoltage();
  frame.decayCvVolts = inputs[DECAY_CV_INPUT].getVoltage();
  frame.blendCvVolts = inputs[BLEND_CV_INPUT].getVoltage();
  frame.filterCvVolts = inputs[FILTER_CV_INPUT].getVoltage();
  frame.absorbCvVolts = inputs[ABSORB_CV_INPUT].getVoltage();

  frame.externalConnected = inputs[EXTERNAL_INPUT].isConnected();
  frame.beginEndConnected = inputs[BEGIN_END_INPUT].isConnected();
  frame.tonicModConnected = inputs[TONIC_MOD_INPUT].isConnected();
  frame.tonesCvConnected = inputs[TONES_CV_INPUT].isConnected();
  frame.timeModConnected = inputs[TIME_MOD_INPUT].isConnected();
  frame.timeCvConnected = inputs[TIME_CV_INPUT].isConnected();
  frame.timeUnityCvConnected = inputs[TIME_UNITY_CV_INPUT].isConnected();
  frame.decayCvConnected = inputs[DECAY_CV_INPUT].isConnected();
  frame.blendCvConnected = inputs[BLEND_CV_INPUT].isConnected();
  frame.filterCvConnected = inputs[FILTER_CV_INPUT].isConnected();
  frame.absorbCvConnected = inputs[ABSORB_CV_INPUT].isConnected();

  const shortwav::wyrd::Outputs out = engine.process(frame, readControls(this));

  outputs[STRENGTH_OUTPUT].setVoltage(safeVoltage(out.strengthVolts, -10.5f, 10.5f));
  outputs[CV1_OUTPUT].setVoltage(safeVoltage(out.cv1Volts, 0.f, 10.f));
  outputs[CV2_OUTPUT].setVoltage(safeVoltage(out.cv2Volts, -5.f, 5.f));
  outputs[AGITATION_OUTPUT].setVoltage(safeVoltage(out.agitationVolts, 0.f, 6.f));
  outputs[TONE_CORE_OUTPUT].setVoltage(safeVoltage(out.toneCoreVolts, -5.2f, 5.2f));
  outputs[SUB_HARMONICS_OUTPUT].setVoltage(safeVoltage(out.subHarmonicsVolts, -5.2f, 5.2f));
  outputs[MODULAR_OUTPUT].setVoltage(safeVoltage(out.modularVolts, -5.2f, 5.2f));
  outputs[LINE_OUTPUT].setVoltage(safeVoltage(out.lineVolts, -1.6f, 1.6f));

  auto* expanderMessage = static_cast<shortwav::wyrd::WyrdReverbExpanderMessage*>(rightExpander.producerMessage);
  if (expanderMessage) {
    expanderMessage->modularVolts = safeVoltage(out.modularVolts, -5.2f, 5.2f);
    expanderMessage->cv2Volts = safeVoltage(out.cv2Volts, -5.f, 5.f);
    expanderMessage->active = rightExpander.module && rightExpander.module->model == modelWyrdexpander;
    rightExpander.requestMessageFlip();
  }

  lights[STRENGTH_LIGHT].setBrightnessSmooth(std::fabs(out.strengthVolts) / 10.f, args.sampleTime);
  lights[CV1_LIGHT].setBrightnessSmooth(out.cv1Volts / 10.f, args.sampleTime);
  lights[CV2_POS_LIGHT].setBrightnessSmooth(std::max(0.f, out.cv2Volts) / 5.f, args.sampleTime);
  lights[CV2_NEG_LIGHT].setBrightnessSmooth(std::max(0.f, -out.cv2Volts) / 5.f, args.sampleTime);
  lights[AGITATION_LIGHT].setBrightnessSmooth(out.agitationVolts / 6.f, args.sampleTime);
  lights[ACTIVATION_POS_LIGHT].setBrightnessSmooth(std::max(0.f, out.activation), args.sampleTime);
  lights[ACTIVATION_NEG_LIGHT].setBrightnessSmooth(std::max(0.f, -out.activation), args.sampleTime);
  lights[RESULT_LIGHT].setBrightnessSmooth(std::fabs(out.modularVolts) / 5.f, args.sampleTime);
}

void Wyrd::onReset()
{
  engine.reset();
}

json_t* Wyrd::dataToJson()
{
  json_t* rootJ = json_object();
  json_object_set_new(rootJ, "touchSource", json_integer(touchSource));
  json_object_set_new(rootJ, "strengthCalibration", json_integer(strengthCalibration));
  return rootJ;
}

void Wyrd::dataFromJson(json_t* rootJ)
{
  json_t* touchSourceJ = json_object_get(rootJ, "touchSource");
  if (touchSourceJ) {
    touchSource = clamp(static_cast<int>(json_integer_value(touchSourceJ)),
                        0, shortwav::wyrd::kNumTouchSources - 1);
  }

  json_t* strengthCalibrationJ = json_object_get(rootJ, "strengthCalibration");
  if (strengthCalibrationJ) {
    strengthCalibration = clamp(static_cast<int>(json_integer_value(strengthCalibrationJ)),
                                0, shortwav::wyrd::kNumStrengthCalibrations - 1);
  }
}

Model *modelWyrd = createModel<Wyrd, WyrdWidget>("Wyrd");
