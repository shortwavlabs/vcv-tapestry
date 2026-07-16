#include "Fray.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <thread>

using namespace ShortwavDSP::Fray;

namespace {

static const float kPanelWidth = 630.f;

static const char* const kEffectNames[kEffectCount] = {
	"MODULATOR", "TAPE STOP", "RETRIGGER", "REVERSER", "STRETCHER",
	"LOFI", "DISTORTION", "GATER", "DELAY", "SHUFFLER"
};

static const char* const kLaneNames[kLaneCount] = {
	"RANDOM", "MODULATOR", "TAPE STOP", "RETRIGGER", "REVERSER", "STRETCHER",
	"LOFI", "DISTORTION", "GATER", "DELAY", "SHUFFLER"
};

static const char* const kUniqueParamNames[kEffectCount][8] = {
	{"FREQ", "DEPTH", "OSC", "SPREAD", "FM", "ATTACK", "RELEASE", "PHASE"},
	{"SLOW", "SPEED UP", "MODE", "TIME", "CURVE", "HOLD", "SMOOTH", "TIMING"},
	{"INITIAL", "FINAL", "TRANS", "DECAY", "TIME", "PITCH", "SMOOTH", "TIMING"},
	{"TIME", "POINT A", "POINT B", "SMOOTH", "CAPTURE", "RATE", "BLEND", "TIMING"},
	{"SPEED", "GRAIN", "JITTER", "SMOOTH", "GM", "ATTACK", "RELEASE", "VOICES"},
	{"MODE", "BITS", "RATE", "FM", "ATTACK", "RELEASE", "BIAS", "JITTER"},
	{"MODE", "DRIVE", "TONE", "WET", "DRY", "BIAS", "SHAPE", "QUALITY"},
	{"STEP", "SMOOTH", "STEPS", "DEPTH", "PHASE", "ACCENT", "SWING", "TIMING"},
	{"TIME", "SLEW", "SPREAD", "SEND", "FEEDBACK", "RETURN", "DUCK", "TIMING"},
	{"MIN", "MAX", "RANGE", "SHUFFLE", "REPEAT", "REVERSE", "SMOOTH", "TIMING"}
};

static const char* const kCommonParamNames[6] = {
	"FILTER", "CUTOFF", "Q", "MIX", "PAN", "GAIN"
};

static uint64_t identityEffectOrder() {
	uint64_t packed = 0;
	for (int position = 0; position < kEffectCount; ++position)
		packed |= static_cast<uint64_t>(position) << (position * 4);
	return packed;
}

static int unpackEffectOrder(uint64_t packed, int position) {
	position = clamp(position, 0, kEffectCount - 1);
	return clamp(static_cast<int>((packed >> (position * 4)) & UINT64_C(0xf)), 0, kEffectCount - 1);
}

static uint64_t packEffectOrder(const std::array<int, kEffectCount>& order) {
	uint64_t packed = 0;
	for (int position = 0; position < kEffectCount; ++position)
		packed |= static_cast<uint64_t>(clamp(order[position], 0, kEffectCount - 1)) << (position * 4);
	return packed;
}

static uint64_t swappedEffectOrder(uint64_t packed, int position, int selectedEffect) {
	std::array<int, kEffectCount> order;
	for (int p = 0; p < kEffectCount; ++p)
		order[p] = unpackEffectOrder(packed, p);
	position = clamp(position, 0, kEffectCount - 1);
	selectedEffect = clamp(selectedEffect, 0, kEffectCount - 1);
	int otherPosition = position;
	for (int p = 0; p < kEffectCount; ++p) {
		if (order[p] == selectedEffect) {
			otherPosition = p;
			break;
		}
	}
	std::swap(order[position], order[otherPosition]);
	return packEffectOrder(order);
}

class AuthoredStateGuard {
public:
	explicit AuthoredStateGuard(std::atomic_flag& flag) : flag_(flag) {
		while (flag_.test_and_set(std::memory_order_acquire))
			std::this_thread::yield();
	}
	~AuthoredStateGuard() {
		flag_.clear(std::memory_order_release);
	}

private:
	std::atomic_flag& flag_;
};

class AudioStateGuard {
public:
	explicit AudioStateGuard(std::atomic_flag& flag)
	: flag_(flag), locked_(!flag_.test_and_set(std::memory_order_acquire)) {}
	~AudioStateGuard() {
		if (locked_)
			flag_.clear(std::memory_order_release);
	}
	bool locked() const { return locked_; }

private:
	std::atomic_flag& flag_;
	bool locked_;
};

class AtomicBoolScope {
public:
	explicit AtomicBoolScope(std::atomic<bool>& value) : value_(value) {
		value_.store(true, std::memory_order_release);
	}
	~AtomicBoolScope() {
		value_.store(false, std::memory_order_release);
	}

private:
	std::atomic<bool>& value_;
};

static NVGcolor effectColor(int effect, int alpha = 255) {
	static const unsigned char colors[kEffectCount][3] = {
		{242, 137, 72}, {235, 91, 93}, {229, 184, 73}, {169, 111, 206}, {93, 171, 196},
		{128, 192, 103}, {226, 93, 139}, {91, 194, 166}, {85, 139, 211}, {184, 125, 76}
	};
	effect = clamp(effect, 0, kEffectCount - 1);
	return nvgRGBA(colors[effect][0], colors[effect][1], colors[effect][2], alpha);
}

static void setFont(const Widget::DrawArgs& args, float size, int align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE) {
	static std::weak_ptr<Font> cachedFont;
	std::shared_ptr<Font> font = cachedFont.lock();
	if (!font && APP && APP->window) {
		font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		cachedFont = font;
	}
	if (font) {
		nvgFontFaceId(args.vg, font->handle);
	}
	nvgFontSize(args.vg, size);
	nvgTextAlign(args.vg, align);
}

static float finiteOrZero(float value) {
	return std::isfinite(value) ? value : 0.f;
}

struct FrayGridAction : history::ModuleAction {
	int scene = 0;
	int lane = 0;
	uint64_t oldActive = 0;
	uint64_t oldStart = 0;
	uint64_t newActive = 0;
	uint64_t newStart = 0;

	FrayGridAction() {
		name = "edit Fray pattern";
	}

	void apply(uint64_t expectedActive, uint64_t expectedStart, uint64_t active, uint64_t start) {
		if (!APP || !APP->engine)
			return;
		Module* base = APP->engine->getModule(moduleId);
		Fray* module = dynamic_cast<Fray*>(base);
		if (module)
			module->enqueueConditionalLaneState(
				scene, lane, expectedActive, expectedStart, active, start);
	}

	void undo() override {
		apply(newActive, newStart, oldActive, oldStart);
	}

	void redo() override {
		apply(oldActive, oldStart, newActive, newStart);
	}
};

struct FrayPatternCanvas : Widget {
	Fray* module = NULL;

	void draw(const DrawArgs& args) override {
		const float labelWidth = 72.f;
		const float rowHeight = box.size.y / static_cast<float>(kLaneCount);
		const int steps = module ? clamp(module->uiStepCount.load(), 2, kMaxSteps) : 16;
		const float gridWidth = box.size.x - labelWidth;
		const float cellWidth = gridWidth / static_cast<float>(steps);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGBA(5, 7, 9, 238));
		nvgFill(args.vg);

		setFont(args, 7.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		for (int lane = 0; lane < kLaneCount; ++lane) {
			const float y = lane * rowHeight;
			if (lane & 1) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0.f, y, box.size.x, rowHeight);
				nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 7));
				nvgFill(args.vg);
			}

			nvgFillColor(args.vg, lane == 0 ? nvgRGBA(222, 222, 222, 210) : effectColor(lane - 1, 225));
			nvgText(args.vg, 5.f, y + rowHeight * 0.52f, kLaneNames[lane], NULL);

			const uint64_t active = module ? module->uiActiveMasks[lane].load() : 0;
			const uint64_t starts = module ? module->uiStartMasks[lane].load() : 0;
			for (int cell = 0; cell < steps; ++cell) {
				const float x = labelWidth + cell * cellWidth;
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x, y, cellWidth, rowHeight);
				nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, cell % 4 == 0 ? 26 : 12));
				nvgStrokeWidth(args.vg, 0.5f);
				nvgStroke(args.vg);
				if (active & (uint64_t(1) << cell)) {
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, x + 0.7f, y + 1.1f, std::max(0.8f, cellWidth - 1.4f), rowHeight - 2.2f, 1.f);
					nvgFillColor(args.vg, lane == 0 ? nvgRGBA(225, 225, 225, 155) : effectColor(lane - 1, 178));
					nvgFill(args.vg);
				}
				if (starts & (uint64_t(1) << cell)) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x + 0.5f, y + 1.f, std::min(1.5f, cellWidth), rowHeight - 2.f);
					nvgFillColor(args.vg, nvgRGBA(255, 245, 219, 235));
					nvgFill(args.vg);
				}
			}
		}
	}
};

struct FrayGridWidget : OpaqueWidget {
	Fray* module = NULL;
	FramebufferWidget* framebuffer = NULL;
	FrayPatternCanvas* canvas = NULL;
	int renderedRevision = -1;
	int lastLane = -1;
	int lastCell = -1;
	int gestureLane = -1;
	int gestureScene = -1;
	int gestureMinCell = -1;
	int gestureMaxCell = -1;
	uint64_t gestureOldActive = 0;
	uint64_t gestureOldStart = 0;
	uint64_t gestureNewActive = 0;
	uint64_t gestureNewStart = 0;
	uint32_t gestureEpoch = 0;
	bool gestureChanged = false;
	bool paintValue = true;

	FrayGridWidget() {
		framebuffer = new FramebufferWidget;
		framebuffer->box.pos = Vec(0.f, 0.f);
		canvas = new FrayPatternCanvas;
		framebuffer->addChild(canvas);
		addChild(framebuffer);
	}

	void setModule(Fray* m) {
		module = m;
		canvas->module = m;
	}

	void setSize(Vec size) {
		box.size = size;
		framebuffer->box.size = size;
		canvas->box.size = size;
		framebuffer->setDirty();
	}

	void step() override {
		if (module) {
			const int revision = module->uiRevision.load();
			if (revision != renderedRevision) {
				renderedRevision = revision;
				framebuffer->setDirty();
			}
		}
		OpaqueWidget::step();
	}

	void draw(const DrawArgs& args) override {
		OpaqueWidget::draw(args);
		const int steps = module ? clamp(module->uiStepCount.load(), 2, kMaxSteps) : 16;
		const int cell = module ? clamp(module->uiCurrentCell.load(), 0, steps - 1) : 0;
		const float labelWidth = 72.f;
		const float cellWidth = (box.size.x - labelWidth) / static_cast<float>(steps);
		const float x = labelWidth + cell * cellWidth;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, x, 0.f, std::max(1.f, cellWidth), box.size.y);
		nvgFillColor(args.vg, nvgRGBA(255, 244, 218, 34));
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, x, 0.f, 1.f, box.size.y);
		nvgFillColor(args.vg, nvgRGBA(255, 244, 218, 210));
		nvgFill(args.vg);
	}

	bool cellAt(Vec pos, int& lane, int& cell) const {
		if (!module || pos.x < 72.f || pos.x >= box.size.x || pos.y < 0.f || pos.y >= box.size.y)
			return false;
		lane = clamp(static_cast<int>(pos.y / (box.size.y / static_cast<float>(kLaneCount))), 0, kLaneCount - 1);
		const int steps = clamp(module->uiStepCount.load(), 2, kMaxSteps);
		cell = clamp(static_cast<int>((pos.x - 72.f) / ((box.size.x - 72.f) / steps)), 0, steps - 1);
		return true;
	}

	void selectLane(int lane) {
		if (module && lane > 0 && APP && APP->engine)
			APP->engine->setParamValue(module, Fray::SELECT_EFFECT_PARAM, static_cast<float>(lane - 1));
	}

	void commitLaneEdit(int lane, uint64_t oldActive, uint64_t oldStart, uint64_t active, uint64_t starts) {
		if (!module || lane < 0 || lane >= kLaneCount)
			return;
		starts &= active;
		if (active == oldActive && starts == oldStart)
			return;
		const int scene = gestureScene >= 0 ? gestureScene : module->uiActiveScene.load();
		if (!module->enqueueLaneStateAtEpoch(scene, lane, active, starts, gestureEpoch))
			return;
		if (lane == gestureLane) {
			gestureNewActive = active;
			gestureNewStart = starts;
			gestureChanged = gestureNewActive != gestureOldActive || gestureNewStart != gestureOldStart;
		}
	}

	void beginGesture(int lane, int cell) {
		gestureLane = lane;
		gestureScene = module ? module->uiActiveScene.load() : -1;
		gestureEpoch = module ? module->gridEpoch.load(std::memory_order_acquire) : 0;
		gestureMinCell = gestureMaxCell = cell;
		gestureOldActive = module ? module->uiActiveMasks[lane].load() : 0;
		gestureOldStart = module ? module->uiStartMasks[lane].load() : 0;
		gestureNewActive = gestureOldActive;
		gestureNewStart = gestureOldStart;
		gestureChanged = false;
	}

	void finishGesture() {
		if (gestureChanged && module
			&& module->gridEpoch.load(std::memory_order_acquire) == gestureEpoch
			&& APP && APP->history) {
			FrayGridAction* action = new FrayGridAction;
			action->moduleId = module->id;
			action->scene = gestureScene;
			action->lane = gestureLane;
			action->oldActive = gestureOldActive;
			action->oldStart = gestureOldStart;
			action->newActive = gestureNewActive;
			action->newStart = gestureNewStart;
			APP->history->push(action);
		}
		gestureLane = -1;
		gestureScene = -1;
		gestureEpoch = 0;
		gestureMinCell = -1;
		gestureMaxCell = -1;
		gestureChanged = false;
	}

	void editCell(int lane, int cell, bool value, bool continuing) {
		if (!module || lane < 0 || lane >= kLaneCount || cell < 0 || cell >= kMaxSteps)
			return;
		const uint64_t bit = uint64_t(1) << cell;
		uint64_t oldActive = module->uiActiveMasks[lane].load();
		uint64_t oldStart = module->uiStartMasks[lane].load();
		uint64_t active = oldActive;
		uint64_t starts = oldStart;
		if (value) {
			active |= bit;
			if (!continuing)
				starts |= bit;
			else
				starts &= ~bit;
		}
		else {
			active &= ~bit;
			starts &= ~bit;
			const int steps = clamp(module->uiStepCount.load(), 2, kMaxSteps);
				if (cell + 1 < steps && (active & (uint64_t(1) << (cell + 1))))
					starts |= uint64_t(1) << (cell + 1);
		}
		commitLaneEdit(lane, oldActive, oldStart, active, starts);
	}

	static uint64_t cellRangeMask(int first, int last) {
		first = clamp(first, 0, kMaxSteps - 1);
		last = clamp(last, first, kMaxSteps - 1);
		const uint64_t throughLast = last == 63
			? ~UINT64_C(0) : (UINT64_C(1) << (last + 1)) - UINT64_C(1);
		const uint64_t beforeFirst = first == 0
			? UINT64_C(0) : (UINT64_C(1) << first) - UINT64_C(1);
		return throughLast & ~beforeFirst;
	}

	void editDragSegment(int lane, int cell) {
		if (!module || lane < 0 || lane >= kLaneCount || cell < 0 || cell >= kMaxSteps)
			return;
		if (lane != gestureLane) {
			return;
		}

		const uint64_t oldActive = module->uiActiveMasks[lane].load();
		const uint64_t oldStart = module->uiStartMasks[lane].load();
		uint64_t active = oldActive;
		uint64_t starts = oldStart;
		const int steps = clamp(module->uiStepCount.load(), 2, kMaxSteps);
		if (paintValue) {
			gestureMinCell = std::min(gestureMinCell, cell);
			gestureMaxCell = std::max(gestureMaxCell, cell);
			const uint64_t range = cellRangeMask(gestureMinCell, gestureMaxCell);
			active |= range;
			starts &= ~range;
			starts |= UINT64_C(1) << gestureMinCell;
		}
		else {
			const int first = std::min(lastCell, cell);
			const int last = std::max(lastCell, cell);
			const uint64_t range = cellRangeMask(first, last);
			active &= ~range;
			starts &= active;
			const uint64_t visible = LanePattern::validMask(steps);
			const uint64_t requiredStarts = (active & visible) & ~((active << 1) & visible);
			starts |= requiredStarts;
		}
		commitLaneEdit(lane, oldActive, oldStart, active, starts);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS && module) {
			int lane = -1;
			int cell = -1;
			if (e.pos.x < 72.f && e.pos.y >= 0.f && e.pos.y < box.size.y) {
				lane = clamp(static_cast<int>(e.pos.y / (box.size.y / static_cast<float>(kLaneCount))), 0, kLaneCount - 1);
				selectLane(lane);
			}
				else if (cellAt(e.pos, lane, cell)) {
					selectLane(lane);
					paintValue = (module->uiActiveMasks[lane].load() & (uint64_t(1) << cell)) == 0;
					lastLane = lane;
					lastCell = cell;
					beginGesture(lane, cell);
					editCell(lane, cell, paintValue, false);
			}
			e.consume(this);
			return;
		}
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_RELEASE && gestureLane >= 0) {
			finishGesture();
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}

	void onDragHover(const DragHoverEvent& e) override {
		if (e.origin == this && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			int lane = -1;
				int cell = -1;
				if (cellAt(e.pos, lane, cell) && lane == gestureLane && (lane != lastLane || cell != lastCell)) {
					editDragSegment(lane, cell);
					selectLane(lane);
				lastLane = lane;
				lastCell = cell;
			}
			e.consume(this);
			return;
		}
		OpaqueWidget::onDragHover(e);
	}

	void onDragEnd(const DragEndEvent& e) override {
		finishGesture();
		lastLane = -1;
		lastCell = -1;
		OpaqueWidget::onDragEnd(e);
	}
};

struct FrayStatusWidget : Widget {
	Fray* module = NULL;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 4.f);
		nvgFillColor(args.vg, nvgRGBA(7, 9, 12, 218));
		nvgFill(args.vg);

		const float textX = 60.f;
		const int effect = module ? clamp(static_cast<int>(std::round(module->params[Fray::SELECT_EFFECT_PARAM].getValue())), 0, kEffectCount - 1) : 0;
		setFont(args, 13.f);
		nvgFillColor(args.vg, effectColor(effect));
		nvgText(args.vg, textX, 20.f, kEffectNames[effect], NULL);

		char text[64];
		const int scene = module ? module->uiActiveScene.load() : 0;
		const int cell = module ? module->uiCurrentCell.load() : 0;
		const int steps = module ? module->uiStepCount.load() : 16;
		const float bpm = module ? module->uiBpmTimes10.load() * 0.1f : 120.f;
		setFont(args, 10.f);
		nvgFillColor(args.vg, nvgRGBA(235, 232, 222, 220));
		std::snprintf(text, sizeof(text), "SCENE %03d", scene + 1);
		nvgText(args.vg, textX, 52.f, text, NULL);
		std::snprintf(text, sizeof(text), "CELL %02d / %02d", cell + 1, steps);
		nvgText(args.vg, textX, 72.f, text, NULL);
		std::snprintf(text, sizeof(text), "%5.1f BPM", bpm);
		nvgText(args.vg, textX, 92.f, text, NULL);

		setFont(args, 7.f);
		nvgFillColor(args.vg, nvgRGBA(190, 191, 187, 165));
		nvgText(args.vg, textX, 119.f, "CLICK/DRAG GRID", NULL);
	}
};

struct FrayLabelsWidget : Widget {
	Fray* module = NULL;

	void label(const DrawArgs& args, float x, float y, const char* text, float size = 6.5f, bool darkSurface = false) {
		setFont(args, size);
		const NVGcolor lightText = nvgRGBA(230, 226, 214, 220);
		const NVGcolor panelText = nvgRGBA(44, 46, 47, 225);
		nvgFillColor(args.vg, darkSurface ? lightText : panelText);
		nvgText(args.vg, x, y, text, NULL);
	}

	void draw(const DrawArgs& args) override {
		setFont(args, 18.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGBA(242, 137, 72, 245));
		nvgText(args.vg, 36.f, 15.f, "FRAY", NULL);

		const char* header[] = {"TEMPO", "EXT", "BEATS", "DIV", "SCENE", "LOOP", "RUN", "RESET", "RAND", "MUT", "MIX", "PAN", "GAIN"};
		const float hx[] = {72.f, 108.f, 145.f, 181.f, 220.f, 258.f, 292.f, 326.f, 360.f, 394.f, 520.f, 558.f, 596.f};
		for (int i = 0; i < 13; ++i)
			label(args, hx[i], 48.f, header[i], 5.8f, true);

		const int effect = module ? clamp(static_cast<int>(std::round(module->params[Fray::SELECT_EFFECT_PARAM].getValue())), 0, kEffectCount - 1) : 0;
		const float px[7] = {50.f, 138.f, 226.f, 314.f, 402.f, 490.f, 578.f};
		for (int i = 0; i < 7; ++i) {
			const char* top = i < 7 ? kUniqueParamNames[effect][i] : "";
			label(args, px[i], 269.f, top, 6.f);
			const char* bottom = i == 0 ? kUniqueParamNames[effect][7] : kCommonParamNames[i - 1];
			label(args, px[i], 300.f, bottom, 6.f);
		}

		const char* topPorts[11] = {"IN L", "IN R", "CLOCK", "TEMPO", "RESET", "RUN", "SCENE", "S GATE", "NEXT", "PREV", "RAND"};
		const char* bottomPorts[11] = {"MUT", "MIX CV", "MOD A", "MOD B", "MOD C", "MOD D", "OUT L", "OUT R", "STEP", "EOC", "ACTIVE"};
		for (int i = 0; i < 11; ++i) {
			const float x = 30.f + i * 57.f;
			label(args, x, 313.f, topPorts[i], 5.5f, true);
			label(args, x, 344.f, bottomPorts[i], 5.5f, true);
		}

		label(args, 482.f, 211.f, "A", 6.f);
		label(args, 522.f, 211.f, "B", 6.f);
		label(args, 562.f, 211.f, "C", 6.f);
		label(args, 602.f, 211.f, "D", 6.f);
	}
};

struct FrayNativeActionButton : VCVButton {
	ModuleWidget* owner = NULL;
	bool randomize = false;

	void onButton(const ButtonEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && owner) {
			if (e.action == GLFW_PRESS) {
				if (randomize)
					owner->randomizeAction();
				else
					owner->resetAction();
			}
			e.consume(this);
			return;
		}
		VCVButton::onButton(e);
	}
};

} // namespace

Fray::Fray() {
	config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

	configParam(TEMPO_PARAM, -2.f, std::log2(300.f / 120.f), 0.f, "Tempo", " BPM", 2.f, 120.f);
	configSwitch(EXT_CLOCK_MODE_PARAM, 0.f, 1.f, 1.f, "External clock mode", {"Step", "Beat"});
	configSwitch(BEATS_PARAM, 1.f, 8.f, 4.f, "Scene beats", {"1", "2", "3", "4", "5", "6", "7", "8"});
	configSwitch(DIVISIONS_PARAM, 2.f, 8.f, 4.f, "Divisions per beat", {"2", "3", "4", "5", "6", "7", "8"});
	configParam(SCENE_PARAM, 0.f, 127.f, 0.f, "Scene");
	getParamQuantity(SCENE_PARAM)->snapEnabled = true;
	getParamQuantity(SCENE_PARAM)->smoothEnabled = false;
	configSwitch(LOOP_PARAM, 0.f, 1.f, 1.f, "Loop", {"One shot", "Loop"});
	configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Paused", "Running"});
	configButton(RESET_PARAM, "Reset transport");
	configButton(RANDOMIZE_PARAM, "Randomize scene");
	configButton(MUTATE_PARAM, "Mutate scene");
	configSwitch(SELECT_EFFECT_PARAM, 0.f, 9.f, 0.f, "Selected effect", {
		"Modulator", "Tape Stop", "Retrigger", "Reverser", "Stretcher",
		"Lofi", "Distortion", "Gater", "Delay", "Shuffler"
	});
	configParam(MASTER_MIX_PARAM, 0.f, 1.f, 1.f, "Master mix", "%", 0.f, 100.f);
	configParam(MASTER_PAN_PARAM, -1.f, 1.f, 0.f, "Master pan", "%", 0.f, 100.f);
	configParam(MASTER_GAIN_PARAM, 0.f, 2.f, 1.f, "Master gain", "%", 0.f, 100.f);
	configParam(MACRO_A_PARAM, -1.f, 1.f, 1.f, "Macro A amount", "%", 0.f, 100.f);
	configParam(MACRO_B_PARAM, -1.f, 1.f, 1.f, "Macro B amount", "%", 0.f, 100.f);
	configParam(MACRO_C_PARAM, -1.f, 1.f, 1.f, "Macro C amount", "%", 0.f, 100.f);
	configParam(MACRO_D_PARAM, -1.f, 1.f, 1.f, "Macro D amount", "%", 0.f, 100.f);
	configureEffectParams();

	configInput(AUDIO_L_INPUT, "Left audio");
	configInput(AUDIO_R_INPUT, "Right audio");
	configInput(CLOCK_INPUT, "Clock");
	configInput(TEMPO_CV_INPUT, "Tempo (1 V/oct)");
	configInput(RESET_INPUT, "Reset");
	configInput(RUN_INPUT, "Run gate");
	configInput(SCENE_INPUT, "Scene CV");
	configInput(SCENE_GATE_INPUT, "Scene gate/trigger");
	configInput(NEXT_INPUT, "Next scene");
	configInput(PREV_INPUT, "Previous scene");
	configInput(RANDOMIZE_INPUT, "Randomize scene");
	configInput(MUTATE_INPUT, "Mutate scene");
	configInput(MIX_CV_INPUT, "Master mix CV");
	configInput(MOD_A_INPUT, "Macro A CV");
	configInput(MOD_B_INPUT, "Macro B CV");
	configInput(MOD_C_INPUT, "Macro C CV");
	configInput(MOD_D_INPUT, "Macro D CV");

	configOutput(AUDIO_L_OUTPUT, "Left audio");
	configOutput(AUDIO_R_OUTPUT, "Right audio");
	configOutput(STEP_OUTPUT, "Step trigger");
	configOutput(EOC_OUTPUT, "End of scene trigger");
	configOutput(ACTIVITY_OUTPUT, "Effect activity");
	configBypass(AUDIO_L_INPUT, AUDIO_L_OUTPUT);
	configBypass(AUDIO_R_INPUT, AUDIO_R_OUTPUT);

	for (int param = 0; param < EFFECT_PARAM_BASE; ++param) {
		if (paramQuantities[param])
			paramQuantities[param]->randomizeEnabled = false;
	}

	controlDivider.setDivision(32);
	chainTransition.setRiseFall(1.f / 0.003f, 1.f / 0.003f);
	chainTransition.out = 1.f;
	uiCurrentCell.store(0);
	uiStepCount.store(16);
	uiActiveScene.store(0);
	uiBpmTimes10.store(1200);
	uiRevision.store(0);
	sceneCvMode.store(0);
	resetOnRun.store(false);
	gridEpoch.store(1);
	suppressDeferredCapture.store(false);
	for (int lane = 0; lane < kLaneCount; ++lane) {
		uiActiveMasks[lane].store(0);
		uiStartMasks[lane].store(0);
	}
	effectActive.fill(false);
	effectStart.fill(false);
	effectEnd.fill(false);
	for (int i = 0; i < 4; ++i)
		macroTargets[i].store(-1 - i);
	uiEffectOrderPacked.store(identityEffectOrder());
}

void Fray::configureEffectParams() {
	for (int effect = 0; effect < kEffectCount; ++effect) {
		for (int slot = 0; slot < 8; ++slot) {
			if (effect == DISTORTION && slot == 7) {
				configSwitch(effectParamId(effect, slot), 0.f, 1.f, 0.f,
					"Distortion quality", {"Raw (zero added latency)", "2x oversampled"});
			}
			else {
				configParam(effectParamId(effect, slot), 0.f, 1.f, 0.5f,
					std::string(kEffectNames[effect]) + " " + kUniqueParamNames[effect][slot], "%", 0.f, 100.f);
			}
		}
		configSwitch(effectParamId(effect, 8), 0.f, 4.f, 0.f,
			std::string(kEffectNames[effect]) + " filter", {"Off", "Low-pass", "High-pass", "Band-pass", "Notch"});
		configParam(effectParamId(effect, 9), 0.f, 1.f, 0.72f,
			std::string(kEffectNames[effect]) + " filter cutoff", "%", 0.f, 100.f);
		configParam(effectParamId(effect, 10), 0.f, 1.f, 0.2f,
			std::string(kEffectNames[effect]) + " filter Q", "%", 0.f, 100.f);
		configParam(effectParamId(effect, 11), 0.f, 1.f, 1.f,
			std::string(kEffectNames[effect]) + " mix", "%", 0.f, 100.f);
		configParam(effectParamId(effect, 12), -1.f, 1.f, 0.f,
			std::string(kEffectNames[effect]) + " pan", "%", 0.f, 100.f);
		configParam(effectParamId(effect, 13), 0.f, 2.f, 1.f,
			std::string(kEffectNames[effect]) + " gain", "%", 0.f, 100.f);
	}
}

namespace {

static float jsonNumber(json_t* object, const char* key, float fallback) {
	json_t* value = object ? json_object_get(object, key) : NULL;
	return value && json_is_number(value) ? static_cast<float>(json_number_value(value)) : fallback;
}

static int jsonIntegerValue(json_t* value, int fallback) {
	if (!value || !json_is_integer(value))
		return fallback;
	const json_int_t raw = json_integer_value(value);
	if (raw > static_cast<json_int_t>(std::numeric_limits<int>::max()))
		return std::numeric_limits<int>::max();
	if (raw < static_cast<json_int_t>(std::numeric_limits<int>::min()))
		return std::numeric_limits<int>::min();
	return static_cast<int>(raw);
}

static int jsonInteger(json_t* object, const char* key, int fallback) {
	return jsonIntegerValue(object ? json_object_get(object, key) : NULL, fallback);
}

static bool jsonBoolean(json_t* object, const char* key, bool fallback) {
	json_t* value = object ? json_object_get(object, key) : NULL;
	return value && json_is_boolean(value) ? json_is_true(value) : fallback;
}

static uint64_t jsonMask(json_t* array, size_t lowIndex, size_t highIndex) {
	if (!array || !json_is_array(array))
		return 0;
	json_t* lowJ = json_array_get(array, lowIndex);
	json_t* highJ = json_array_get(array, highIndex);
	const uint64_t low = lowJ && json_is_integer(lowJ)
		? static_cast<uint64_t>(json_integer_value(lowJ)) & UINT64_C(0xffffffff) : 0;
	const uint64_t high = highJ && json_is_integer(highJ)
		? static_cast<uint64_t>(json_integer_value(highJ)) & UINT64_C(0xffffffff) : 0;
	return low | (high << 32);
}

static json_t* maskPairJson(uint64_t active, uint64_t starts) {
	json_t* array = json_array();
	json_array_append_new(array, json_integer(static_cast<json_int_t>(active & UINT64_C(0xffffffff))));
	json_array_append_new(array, json_integer(static_cast<json_int_t>((active >> 32) & UINT64_C(0xffffffff))));
	json_array_append_new(array, json_integer(static_cast<json_int_t>(starts & UINT64_C(0xffffffff))));
	json_array_append_new(array, json_integer(static_cast<json_int_t>((starts >> 32) & UINT64_C(0xffffffff))));
	return array;
}

static void sanitizeSceneKeepingHiddenCells(SceneState& scene) {
	const std::array<LanePattern, kLaneCount> authoredLanes = scene.lanes;
	scene.sanitize();
	const uint64_t visibleMask = LanePattern::validMask(scene.stepCount());
	for (int lane = 0; lane < kLaneCount; ++lane) {
		const uint64_t hiddenActive = authoredLanes[lane].activeMask & ~visibleMask;
		const uint64_t hiddenStarts = authoredLanes[lane].startMask & hiddenActive;
		scene.lanes[lane].activeMask |= hiddenActive;
		scene.lanes[lane].startMask |= hiddenStarts;
		scene.lanes[lane].startMask &= scene.lanes[lane].activeMask;
	}
}

static void sanitizeProgramKeepingHiddenCells(ProgramState& program) {
	program.masterMix = clamp(std::isfinite(program.masterMix) ? program.masterMix : 1.f, 0.f, 1.f);
	program.masterPan = clamp(std::isfinite(program.masterPan) ? program.masterPan : 0.f, -1.f, 1.f);
	program.masterGain = clamp(std::isfinite(program.masterGain) ? program.masterGain : 1.f, 0.f, 2.f);
	program.triggerMode = clamp(program.triggerMode, 0, 2);
	program.quantizeMode = clamp(program.quantizeMode, 0, 8);
	bool used[kEffectCount] = {};
	for (int position = 0; position < kEffectCount; ++position) {
		int effect = program.effectOrder[position];
		if (effect < 0 || effect >= kEffectCount || used[effect]) {
			effect = 0;
			while (effect < kEffectCount && used[effect])
				++effect;
		}
		program.effectOrder[position] = effect;
		used[effect] = true;
	}
	for (int scene = 0; scene < kSceneCount; ++scene)
		sanitizeSceneKeepingHiddenCells(program.scenes[scene]);
}

} // namespace

bool Fray::enqueueLaneState(int scene, int lane, uint64_t activeMask, uint64_t startMask) {
	return enqueueLaneStateAtEpoch(
		scene, lane, activeMask, startMask, gridEpoch.load(std::memory_order_acquire));
}

bool Fray::enqueueLaneStateAtEpoch(
	int scene, int lane, uint64_t activeMask, uint64_t startMask, uint32_t expectedEpoch) {
	if (expectedEpoch != gridEpoch.load(std::memory_order_acquire))
		return false;
	if (scene < 0 || scene >= kSceneCount || lane < 0 || lane >= kLaneCount || gridCommands.full())
		return false;
	GridCommand command;
	command.scene = scene;
	command.lane = lane;
	command.activeMask = activeMask;
	command.startMask = startMask & activeMask;
	command.expectedActiveMask = 0;
	command.expectedStartMask = 0;
	command.epoch = expectedEpoch;
	command.conditional = false;
	gridCommands.push(command);
	if (scene == uiActiveScene.load()) {
		uiActiveMasks[lane].store(command.activeMask);
		uiStartMasks[lane].store(command.startMask);
		uiRevision.fetch_add(1);
	}
	return true;
}

bool Fray::enqueueConditionalLaneState(
	int scene, int lane,
	uint64_t expectedActiveMask, uint64_t expectedStartMask,
	uint64_t activeMask, uint64_t startMask) {
	if (scene < 0 || scene >= kSceneCount || lane < 0 || lane >= kLaneCount || gridCommands.full())
		return false;
	GridCommand command;
	command.scene = scene;
	command.lane = lane;
	command.activeMask = activeMask;
	command.startMask = startMask & activeMask;
	command.expectedActiveMask = expectedActiveMask;
	command.expectedStartMask = expectedStartMask & expectedActiveMask;
	command.epoch = gridEpoch.load(std::memory_order_acquire);
	command.conditional = true;
	gridCommands.push(command);
	return true;
}

void Fray::applyGridCommands(int limit) {
	int guard = 0;
	bool activeChanged = false;
	bool staleUiChanged = false;
	limit = clamp(limit, 1, 1024);
	while (!gridCommands.empty() && guard++ < limit) {
		const GridCommand command = gridCommands.shift();
		if (command.epoch != gridEpoch.load(std::memory_order_acquire)) {
			staleUiChanged = staleUiChanged || command.scene == activeScene;
			continue;
		}
		if (command.scene < 0 || command.scene >= kSceneCount || command.lane < 0 || command.lane >= kLaneCount)
			continue;
		LanePattern& lane = program.scenes[command.scene].lanes[command.lane];
		if (command.conditional
			&& (lane.activeMask != command.expectedActiveMask
				|| lane.startMask != command.expectedStartMask))
			continue;
		lane.activeMask = command.activeMask;
		lane.startMask = command.startMask & command.activeMask;
		const int steps = program.scenes[command.scene].stepCount();
		const uint64_t visibleMask = LanePattern::validMask(steps);
		const uint64_t hiddenActive = lane.activeMask & ~visibleMask;
		const uint64_t hiddenStarts = lane.startMask & hiddenActive;
		lane.sanitize(steps);
		lane.activeMask |= hiddenActive;
		lane.startMask |= hiddenStarts;
		activeChanged = activeChanged || command.scene == activeScene;
	}
	if (activeChanged) {
		++stateRevision;
		publishUiState();
		updateEffectGates(false);
	}
	else if (staleUiChanged) {
		publishUiState();
	}
}

void Fray::syncParamsToScene() {
	activeScene = clamp(activeScene, 0, kSceneCount - 1);
	SceneState& scene = program.scenes[activeScene];
	scene.beats = clamp(static_cast<int>(std::round(params[BEATS_PARAM].getValue())), 1, 8);
	scene.divisions = clamp(static_cast<int>(std::round(params[DIVISIONS_PARAM].getValue())), 2, 8);
	scene.loop = params[LOOP_PARAM].getValue() >= 0.5f;

	for (int effect = 0; effect < kEffectCount; ++effect) {
		EffectSettings& settings = scene.effects[effect];
		for (int slot = 0; slot < 8; ++slot)
			settings.values[slot] = clamp(params[effectParamId(effect, slot)].getValue(), 0.f, 1.f);
		settings.common.filterType = clamp(static_cast<int>(std::round(params[effectParamId(effect, 8)].getValue())), 0, 4);
		settings.common.cutoff = clamp(params[effectParamId(effect, 9)].getValue(), 0.f, 1.f);
		settings.common.q = clamp(params[effectParamId(effect, 10)].getValue(), 0.f, 1.f);
		settings.common.mix = clamp(params[effectParamId(effect, 11)].getValue(), 0.f, 1.f);
		settings.common.pan = clamp(params[effectParamId(effect, 12)].getValue(), -1.f, 1.f);
		settings.common.gain = clamp(params[effectParamId(effect, 13)].getValue(), 0.f, 2.f);
		settings.sanitize();
	}
	program.masterMix = clamp(params[MASTER_MIX_PARAM].getValue(), 0.f, 1.f);
	program.masterPan = clamp(params[MASTER_PAN_PARAM].getValue(), -1.f, 1.f);
	program.masterGain = clamp(params[MASTER_GAIN_PARAM].getValue(), 0.f, 2.f);
	sanitizeSceneKeepingHiddenCells(scene);
}

void Fray::loadSceneToParams() {
	activeScene = clamp(activeScene, 0, kSceneCount - 1);
	SceneState& scene = program.scenes[activeScene];
	sanitizeSceneKeepingHiddenCells(scene);
	params[SCENE_PARAM].setValue(static_cast<float>(activeScene));
	params[BEATS_PARAM].setValue(static_cast<float>(scene.beats));
	params[DIVISIONS_PARAM].setValue(static_cast<float>(scene.divisions));
	params[LOOP_PARAM].setValue(scene.loop ? 1.f : 0.f);
	for (int effect = 0; effect < kEffectCount; ++effect) {
		const EffectSettings& settings = scene.effects[effect];
		for (int slot = 0; slot < 8; ++slot)
			params[effectParamId(effect, slot)].setValue(settings.values[slot]);
		params[effectParamId(effect, 8)].setValue(static_cast<float>(settings.common.filterType));
		params[effectParamId(effect, 9)].setValue(settings.common.cutoff);
		params[effectParamId(effect, 10)].setValue(settings.common.q);
		params[effectParamId(effect, 11)].setValue(settings.common.mix);
		params[effectParamId(effect, 12)].setValue(settings.common.pan);
		params[effectParamId(effect, 13)].setValue(settings.common.gain);
	}
	params[MASTER_MIX_PARAM].setValue(program.masterMix);
	params[MASTER_PAN_PARAM].setValue(program.masterPan);
	params[MASTER_GAIN_PARAM].setValue(program.masterGain);
}

void Fray::publishUiState() {
	const SceneState& scene = program.scenes[clamp(activeScene, 0, kSceneCount - 1)];
	bool patternChanged = false;
	for (int lane = 0; lane < kLaneCount; ++lane) {
		patternChanged = uiActiveMasks[lane].exchange(scene.lanes[lane].activeMask) != scene.lanes[lane].activeMask
			|| patternChanged;
		patternChanged = uiStartMasks[lane].exchange(scene.lanes[lane].startMask) != scene.lanes[lane].startMask
			|| patternChanged;
	}
	uiCurrentCell.store(clamp(transport.currentCell(), 0, std::max(1, scene.stepCount()) - 1));
	patternChanged = uiStepCount.exchange(scene.stepCount()) != scene.stepCount() || patternChanged;
	patternChanged = uiActiveScene.exchange(activeScene) != activeScene || patternChanged;
	uiBpmTimes10.store(static_cast<int>(std::round(clamp(static_cast<float>(transport.effectiveBpm()), 0.f, 999.f) * 10.f)));
	if (patternChanged)
		uiRevision.fetch_add(1);
}

void Fray::resetRuntime(bool clearEffectMemory) {
	transport.reset();
	if (clearEffectMemory) {
		effects.reset();
		chainTransition.reset();
		chainTransition.out = 1.f;
		chainTransitionFrom = StereoFrame();
		lastChainOutput = StereoFrame();
		chainOutputInitialized = false;
		clockTrigger.reset();
		resetTrigger.reset();
		sceneGateTrigger.reset();
		nextTrigger.reset();
		prevTrigger.reset();
		randomizeTrigger.reset();
		mutateTrigger.reset();
		deferredResetEdge = false;
		deferredClockEdge = false;
		deferredRandomizeEdge = false;
		deferredMutateEdge = false;
		deferredNextEdge = false;
		deferredPrevEdge = false;
		deferredSceneGateEdge = false;
	}
	stepPulse.reset();
	eocPulse.reset();
	randomizePulse.reset();
	effectActive.fill(false);
	effectStart.fill(false);
	effectEnd.fill(false);
	previousCell = -1;
	randomOverlayEffect = -1;
	randomEventOrdinal = 0;
	transportEventOrdinal = 0;
	previousRun = true;
	updateEffectGates(true);
	publishUiState();
}

void Fray::selectScene(int scene, bool resetTransport) {
	scene = clamp(scene, 0, kSceneCount - 1);
	if (scene == activeScene) {
		params[SCENE_PARAM].setValue(static_cast<float>(scene));
		return;
	}
	syncParamsToScene();
	activeScene = scene;
	loadSceneToParams();
	randomOverlayEffect = -1;
	randomEventOrdinal = 0;
	if (resetTransport)
		resetRuntime(false);
	else {
		previousCell = -1;
		updateEffectGates(true);
		publishUiState();
	}
}

void Fray::randomizeCurrentScene(bool mutateOnly) {
	syncParamsToScene();
	// Commands authored before this state-changing action must not be replayed
	// over the new pattern on a later audio frame.
	gridEpoch.fetch_add(1, std::memory_order_acq_rel);
	SceneState& scene = program.scenes[activeScene];
	uint32_t seed = random::u32();
	if (seed == 0)
		seed = 1;
	SceneGenerator generator(seed);
	if (mutateOnly) {
		const std::array<LanePattern, kLaneCount> authoredLanes = scene.lanes;
		generator.mutate(scene, 0.18f, true);
		const uint64_t visibleMask = LanePattern::validMask(scene.stepCount());
		for (int lane = 0; lane < kLaneCount; ++lane) {
			const uint64_t hiddenActive = authoredLanes[lane].activeMask & ~visibleMask;
			scene.lanes[lane].activeMask |= hiddenActive;
			scene.lanes[lane].startMask |= authoredLanes[lane].startMask & hiddenActive;
		}
	}
	else
		generator.randomize(scene, true);
	loadSceneToParams();
	++stateRevision;
	resetRuntime(false);
	randomizePulse.trigger(0.08f);
}

void Fray::onReset(const ResetEvent& e) {
	AtomicBoolScope runtimeReset(suppressDeferredCapture);
	AuthoredStateGuard stateGuard(authoredStateLock);
	Module::onReset(e);
	program = ProgramState();
	activeScene = 0;
	sceneCvMode.store(0);
	resetOnRun.store(false);
	gridEpoch.fetch_add(1, std::memory_order_acq_rel);
	for (int i = 0; i < 4; ++i)
		macroTargets[i].store(-1 - i);
	uiEffectOrderPacked.store(identityEffectOrder(), std::memory_order_release);
	loadSceneToParams();
	gridCommands.clear();
	++stateRevision;
	resetRuntime();
}

void Fray::onRandomize(const RandomizeEvent& e) {
	AtomicBoolScope runtimeReset(suppressDeferredCapture);
	AuthoredStateGuard stateGuard(authoredStateLock);
	applyGridCommands(1024);
	Module::onRandomize(e);
	// Rack has now randomized every eligible visible scene parameter. Copy those
	// values first, then generate only the non-Param pattern state exactly once.
	syncParamsToScene();
	gridEpoch.fetch_add(1, std::memory_order_acq_rel);
	SceneState& scene = program.scenes[activeScene];
	uint32_t seed = random::u32();
	if (seed == 0)
		seed = 1;
	SceneGenerator generator(seed);
	generator.randomizePattern(scene);
	loadSceneToParams();
	++stateRevision;
	resetRuntime(false);
	randomizePulse.trigger(0.08f);
}

void Fray::onSampleRateChange(const SampleRateChangeEvent& e) {
	AuthoredStateGuard stateGuard(authoredStateLock);
	Module::onSampleRateChange(e);
	effects.prepare(e.sampleRate);
	chainOutputInitialized = false;
}

void Fray::updateSceneSelection(bool resetEdge) {
	(void) resetEdge;
	int requested = activeScene;
	const bool nextInputEdge = nextTrigger.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 2.f);
	const bool prevInputEdge = prevTrigger.process(inputs[PREV_INPUT].getVoltage(), 0.1f, 2.f);
	const bool next = deferredNextEdge || nextInputEdge;
	const bool previous = deferredPrevEdge || prevInputEdge;
	deferredNextEdge = false;
	deferredPrevEdge = false;

	if (next)
		requested = (activeScene + 1) % kSceneCount;
	else if (previous)
		requested = (activeScene + kSceneCount - 1) % kSceneCount;
	else if (inputs[SCENE_INPUT].isConnected()) {
		const float volts = inputs[SCENE_INPUT].getVoltage();
		if (sceneCvMode.load() == 0)
			requested = clamp(static_cast<int>(std::round(volts * 12.f)), 0, kSceneCount - 1);
		else
			requested = clamp(static_cast<int>(std::round(rescale(volts, 0.f, 10.f, 0.f, 127.f))), 0, kSceneCount - 1);
		if (inputs[SCENE_GATE_INPUT].isConnected()) {
			const bool sceneGateEdge = sceneGateTrigger.process(inputs[SCENE_GATE_INPUT].getVoltage(), 0.1f, 2.f);
			const bool gate = deferredSceneGateEdge || sceneGateEdge;
			deferredSceneGateEdge = false;
			if (!gate)
				requested = activeScene;
		}
	}
	else {
		requested = clamp(static_cast<int>(std::round(params[SCENE_PARAM].getValue())), 0, kSceneCount - 1);
		// Keep the gate detector current even when it is not selecting a CV scene.
		sceneGateTrigger.process(inputs[SCENE_GATE_INPUT].getVoltage(), 0.1f, 2.f);
		deferredSceneGateEdge = false;
	}

	if (requested != activeScene)
		selectScene(requested, false);
}

void Fray::updateEffectGates(bool forceStarts) {
	SceneState& scene = program.scenes[clamp(activeScene, 0, kSceneCount - 1)];
	const int steps = scene.stepCount();
	const int cell = clamp(transport.currentCell(), 0, steps - 1);
	const LanePattern& randomLane = scene.lanes[0];
	const bool randomActive = randomLane.isActive(cell, steps);
	const bool randomStart = randomLane.isStart(cell, steps) || (forceStarts && randomActive);
	if (!randomActive) {
		randomOverlayEffect = -1;
	}
	else if (randomStart) {
		const uint64_t salt = (static_cast<uint64_t>(activeScene) << 48)
			^ (static_cast<uint64_t>(cell) << 32)
			^ static_cast<uint64_t>(randomEventOrdinal++);
		DeterministicRng rng((static_cast<uint64_t>(scene.seed) << 1) ^ salt ^ UINT64_C(0x9e3779b97f4a7c15));
		randomOverlayEffect = weightedCategorical(scene.randomWeights, rng);
	}

	for (int effect = 0; effect < kEffectCount; ++effect) {
		const bool authored = scene.lanes[effect + 1].isActive(cell, steps);
		const bool active = authored || (randomActive && randomOverlayEffect == effect);
		const bool explicitStart = scene.lanes[effect + 1].isStart(cell, steps)
			|| (randomStart && randomOverlayEffect == effect);
		const bool start = active && (forceStarts || explicitStart || !effectActive[effect]);
		const bool end = effectActive[effect] && (!active || start);
		effectEnd[effect] = end;
		effectStart[effect] = start;
		effectActive[effect] = active;
	}
	previousCell = cell;
}

EffectSettings Fray::modulatedSettings(int effect) {
	effect = clamp(effect, 0, kEffectCount - 1);
	EffectSettings settings = program.scenes[clamp(activeScene, 0, kSceneCount - 1)].effects[effect];
	const int selected = clamp(static_cast<int>(std::round(params[SELECT_EFFECT_PARAM].getValue())), 0, kEffectCount - 1);
	for (int macro = 0; macro < 4; ++macro) {
		int targetEffect = -1;
		int targetSlot = -1;
		const int target = macroTargets[macro].load();
		if (target < 0) {
			targetEffect = selected;
			targetSlot = clamp(-target - 1, 0, 7);
		}
		else {
			targetEffect = target / kEffectParamCount;
			targetSlot = target % kEffectParamCount;
		}
		if (targetEffect != effect || targetSlot < 0 || targetSlot >= kEffectParamCount)
			continue;
		const int inputId = MOD_A_INPUT + macro;
		const int paramId = MACRO_A_PARAM + macro;
		const float modulation = inputs[inputId].getVoltage() * 0.2f * params[paramId].getValue();
		settings.values[targetSlot] = clamp(settings.values[targetSlot] + modulation, 0.f, 1.f);
	}
	settings.sanitize();
	return settings;
}

StereoFrame Fray::processEffectChain(const StereoFrame& input, const ProcessArgs& args, bool cellAdvanced) {
	StereoFrame current = input;
	const SceneState& scene = program.scenes[activeScene];
	for (int position = 0; position < kEffectCount; ++position) {
		const int effect = clamp(program.effectOrder[position], 0, kEffectCount - 1);
		const EffectSettings settings = modulatedSettings(effect);
		EffectContext context;
		context.sampleRate = args.sampleRate;
		context.sampleTime = args.sampleTime;
		context.bpm = clamp(static_cast<float>(transport.effectiveBpm()), 30.f, 300.f);
		context.active = effectActive[effect];
		context.blockStart = effectStart[effect];
		context.blockEnd = effectEnd[effect];
		context.sceneSeed = scene.seed;
		context.eventOrdinal = transportEventOrdinal;

		current = effects.process(static_cast<EffectId>(effect), current, settings, context);
		current.left = clamp(finiteOrZero(current.left), -8.f, 8.f);
		current.right = clamp(finiteOrZero(current.right), -8.f, 8.f);
	}
	if (!chainOutputInitialized) {
		lastChainOutput = current;
		chainTransitionFrom = current;
		chainTransition.out = 1.f;
		chainOutputInitialized = true;
		return current;
	}
	if (cellAdvanced) {
		chainTransitionFrom = lastChainOutput;
		chainTransition.reset();
	}
	const float transition = clamp(chainTransition.process(args.sampleTime, 1.f), 0.f, 1.f);
	if (transition < 1.f) {
		current.left = rack::math::crossfade(chainTransitionFrom.left, current.left, transition);
		current.right = rack::math::crossfade(chainTransitionFrom.right, current.right, transition);
	}
	lastChainOutput = current;
	return current;
}

void Fray::latchDeferredEdges() {
	const bool panelReset = resetButtonTrigger.process(params[RESET_PARAM].getValue() > 0.5f);
	const bool inputReset = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f);
	const bool inputClock = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f);
	const bool panelRandomize = randomizeButtonTrigger.process(params[RANDOMIZE_PARAM].getValue() > 0.5f);
	const bool inputRandomize = randomizeTrigger.process(inputs[RANDOMIZE_INPUT].getVoltage(), 0.1f, 2.f);
	const bool panelMutate = mutateButtonTrigger.process(params[MUTATE_PARAM].getValue() > 0.5f);
	const bool inputMutate = mutateTrigger.process(inputs[MUTATE_INPUT].getVoltage(), 0.1f, 2.f);
	const bool inputNext = nextTrigger.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 2.f);
	const bool inputPrev = prevTrigger.process(inputs[PREV_INPUT].getVoltage(), 0.1f, 2.f);
	const bool inputSceneGate = sceneGateTrigger.process(inputs[SCENE_GATE_INPUT].getVoltage(), 0.1f, 2.f);
	deferredResetEdge = deferredResetEdge || panelReset || inputReset;
	deferredClockEdge = deferredClockEdge || inputClock;
	deferredRandomizeEdge = deferredRandomizeEdge || panelRandomize || inputRandomize;
	deferredMutateEdge = deferredMutateEdge || panelMutate || inputMutate;
	deferredNextEdge = deferredNextEdge || inputNext;
	deferredPrevEdge = deferredPrevEdge || inputPrev;
	deferredSceneGateEdge = deferredSceneGateEdge || inputSceneGate;
	const bool run = params[RUN_PARAM].getValue() >= 0.5f
		&& inputs[RUN_INPUT].getNormalVoltage(10.f) >= 1.f;
	if (!previousRun && run && resetOnRun.load())
		deferredResetEdge = true;
}

void Fray::process(const ProcessArgs& args) {
	AudioStateGuard stateGuard(authoredStateLock);
	if (!stateGuard.locked()) {
		if (!suppressDeferredCapture.load(std::memory_order_acquire))
			latchDeferredEdges();
		return;
	}
	applyGridCommands();

	const bool panelReset = resetButtonTrigger.process(params[RESET_PARAM].getValue() > 0.5f);
	const bool inputReset = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f);
	const bool inputClock = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f);
	const bool panelRandomize = randomizeButtonTrigger.process(params[RANDOMIZE_PARAM].getValue() > 0.5f);
	const bool inputRandomize = randomizeTrigger.process(inputs[RANDOMIZE_INPUT].getVoltage(), 0.1f, 2.f);
	const bool panelMutate = mutateButtonTrigger.process(params[MUTATE_PARAM].getValue() > 0.5f);
	const bool inputMutate = mutateTrigger.process(inputs[MUTATE_INPUT].getVoltage(), 0.1f, 2.f);
	bool resetEdge = deferredResetEdge || panelReset || inputReset;
	const bool clockEdge = deferredClockEdge || inputClock;
	const bool randomizeEdge = deferredRandomizeEdge || panelRandomize || inputRandomize;
	const bool mutateEdge = deferredMutateEdge || panelMutate || inputMutate;
	deferredResetEdge = false;
	deferredClockEdge = false;
	deferredRandomizeEdge = false;
	deferredMutateEdge = false;

	const bool run = params[RUN_PARAM].getValue() >= 0.5f
		&& inputs[RUN_INPUT].getNormalVoltage(10.f) >= 1.f;
	if (!previousRun && run && resetOnRun.load())
		resetEdge = true;
	previousRun = run;

	if (randomizeEdge)
		randomizeCurrentScene(false);
	else if (mutateEdge)
		randomizeCurrentScene(true);

	updateSceneSelection(resetEdge);
	const float tempo = clamp(120.f * std::pow(2.f,
		params[TEMPO_PARAM].getValue() + inputs[TEMPO_CV_INPUT].getVoltage()), 30.f, 300.f);
	TransportInput transportInput;
	transportInput.sampleRate = args.sampleRate;
	transportInput.bpm = tempo;
	transportInput.beats = clamp(static_cast<int>(std::round(params[BEATS_PARAM].getValue())), 1, 8);
	transportInput.divisions = clamp(static_cast<int>(std::round(params[DIVISIONS_PARAM].getValue())), 2, 8);
	transportInput.loop = params[LOOP_PARAM].getValue() >= 0.5f;
	transportInput.run = run;
	transportInput.resetEdge = resetEdge;
	transportInput.externalClockConnected = inputs[CLOCK_INPUT].isConnected();
	transportInput.clockEdge = clockEdge;
	transportInput.externalMode = params[EXT_CLOCK_MODE_PARAM].getValue() >= 0.5f
		? EXTERNAL_BEAT : EXTERNAL_STEP;
	const TransportEvents events = transport.process(transportInput);
	if (events.reset) {
		randomOverlayEffect = -1;
		randomEventOrdinal = 0;
		transportEventOrdinal = 0;
	}
	bool chainBoundary = events.cellAdvanced || events.reset || previousCell != events.cell;
	if (chainBoundary) {
		if (!events.reset)
			++transportEventOrdinal;
		updateEffectGates(events.reset || previousCell < 0);
		if (events.cellAdvanced || events.reset)
			stepPulse.trigger(0.001f);
	}
	if (events.sceneEnded) {
		eocPulse.trigger(0.001f);
		if (transport.hasEnded()) {
			for (int effect = 0; effect < kEffectCount; ++effect) {
				effectEnd[effect] = effectEnd[effect] || effectActive[effect];
				effectStart[effect] = false;
				effectActive[effect] = false;
			}
			randomOverlayEffect = -1;
		}
	}

	const bool controlTick = controlDivider.process();
	if (controlTick) {
		syncParamsToScene();
		const uint64_t packedOrder = uiEffectOrderPacked.load(std::memory_order_acquire);
		for (int position = 0; position < kEffectCount; ++position) {
			const int effect = unpackEffectOrder(packedOrder, position);
			chainBoundary = chainBoundary || program.effectOrder[position] != effect;
			program.effectOrder[position] = effect;
		}
		publishUiState();
	}
	else {
		uiCurrentCell.store(events.cell);
	}

	const float inputLeftVoltage = finiteOrZero(inputs[AUDIO_L_INPUT].getVoltage());
	const float inputRightVoltage = finiteOrZero(inputs[AUDIO_R_INPUT].getNormalVoltage(inputLeftVoltage));
	const StereoFrame dry(inputLeftVoltage * 0.2f, inputRightVoltage * 0.2f);
	const StereoFrame effected = processEffectChain(dry, args, chainBoundary);
	const float masterMix = clamp(params[MASTER_MIX_PARAM].getValue()
		+ inputs[MIX_CV_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
	StereoFrame output(
		rack::math::crossfade(dry.left, effected.left, masterMix),
		rack::math::crossfade(dry.right, effected.right, masterMix));
	const float pan = clamp(params[MASTER_PAN_PARAM].getValue(), -1.f, 1.f);
	if (pan > 0.f)
		output.left *= 1.f - pan;
	else if (pan < 0.f)
		output.right *= 1.f + pan;
	const float gain = clamp(params[MASTER_GAIN_PARAM].getValue(), 0.f, 2.f);
	output.left = finiteOrZero(output.left * gain);
	output.right = finiteOrZero(output.right * gain);

	outputs[AUDIO_L_OUTPUT].setVoltage(clamp(output.left * 5.f, -10.f, 10.f));
	outputs[AUDIO_R_OUTPUT].setVoltage(clamp(output.right * 5.f, -10.f, 10.f));
	outputs[AUDIO_L_OUTPUT].setChannels(1);
	outputs[AUDIO_R_OUTPUT].setChannels(1);
	const bool stepHigh = stepPulse.process(args.sampleTime);
	const bool eocHigh = eocPulse.process(args.sampleTime);
	const bool randomizeHigh = randomizePulse.process(args.sampleTime);
	outputs[STEP_OUTPUT].setVoltage(stepHigh ? 10.f : 0.f);
	outputs[EOC_OUTPUT].setVoltage(eocHigh ? 10.f : 0.f);
	outputs[STEP_OUTPUT].setChannels(1);
	outputs[EOC_OUTPUT].setChannels(1);
	outputs[ACTIVITY_OUTPUT].setChannels(kEffectCount);
	for (int effect = 0; effect < kEffectCount; ++effect) {
		outputs[ACTIVITY_OUTPUT].setVoltage(effectActive[effect] ? 10.f : 0.f, effect);
		lights[EFFECT_LIGHT + effect].setSmoothBrightness(effectActive[effect] ? 1.f : 0.f, args.sampleTime);
	}
	lights[RUN_LIGHT].setSmoothBrightness(run ? 1.f : 0.f, args.sampleTime);
	lights[CLOCK_LIGHT].setSmoothBrightness((events.clockAccepted || stepHigh) ? 1.f : 0.f, args.sampleTime);
	lights[RANDOMIZE_LIGHT].setSmoothBrightness(randomizeHigh ? 1.f : 0.f, args.sampleTime);

	for (int effect = 0; effect < kEffectCount; ++effect) {
		effectStart[effect] = false;
		effectEnd[effect] = false;
	}
}

void Fray::processBypass(const ProcessArgs& args) {
	(void) args;
	chainOutputInitialized = false;
	const float left = finiteOrZero(inputs[AUDIO_L_INPUT].getVoltage());
	const float right = finiteOrZero(inputs[AUDIO_R_INPUT].getNormalVoltage(left));
	outputs[AUDIO_L_OUTPUT].setVoltage(left);
	outputs[AUDIO_R_OUTPUT].setVoltage(right);
	outputs[AUDIO_L_OUTPUT].setChannels(1);
	outputs[AUDIO_R_OUTPUT].setChannels(1);
	outputs[STEP_OUTPUT].setVoltage(0.f);
	outputs[EOC_OUTPUT].setVoltage(0.f);
	outputs[STEP_OUTPUT].setChannels(1);
	outputs[EOC_OUTPUT].setChannels(1);
	outputs[ACTIVITY_OUTPUT].setChannels(kEffectCount);
	for (int effect = 0; effect < kEffectCount; ++effect)
		outputs[ACTIVITY_OUTPUT].setVoltage(0.f, effect);
}

json_t* Fray::dataToJson() {
	ProgramState snapshot;
	int snapshotActiveScene = 0;
	int snapshotSceneCvMode = 0;
	bool snapshotResetOnRun = false;
	std::array<int, 4> snapshotMacros;
	{
		// Hold the audio exclusion only for the bounded state copy. Jansson's
		// comparatively expensive 128-scene tree construction happens afterward.
		AuthoredStateGuard stateGuard(authoredStateLock);
		applyGridCommands(1024);
		syncParamsToScene();
		const uint64_t packedOrder = uiEffectOrderPacked.load(std::memory_order_acquire);
		for (int position = 0; position < kEffectCount; ++position)
			program.effectOrder[position] = unpackEffectOrder(packedOrder, position);
		snapshot = program;
		snapshotActiveScene = activeScene;
		snapshotSceneCvMode = sceneCvMode.load();
		snapshotResetOnRun = resetOnRun.load();
		for (int macro = 0; macro < 4; ++macro)
			snapshotMacros[macro] = macroTargets[macro].load();
	}
	json_t* root = json_object();
	json_object_set_new(root, "schema", json_integer(1));
	json_object_set_new(root, "rngVersion", json_integer(DeterministicRng::version));
	json_object_set_new(root, "activeScene", json_integer(snapshotActiveScene));
	json_object_set_new(root, "sceneCvMode", json_integer(snapshotSceneCvMode));
	json_object_set_new(root, "resetOnRun", json_boolean(snapshotResetOnRun));

	json_t* master = json_object();
	json_object_set_new(master, "mix", json_real(snapshot.masterMix));
	json_object_set_new(master, "pan", json_real(snapshot.masterPan));
	json_object_set_new(master, "gain", json_real(snapshot.masterGain));
	json_object_set_new(master, "triggerMode", json_integer(snapshot.triggerMode));
	json_object_set_new(master, "quantizeMode", json_integer(snapshot.quantizeMode));
	json_object_set_new(root, "master", master);

	json_t* order = json_array();
	for (int position = 0; position < kEffectCount; ++position)
		json_array_append_new(order, json_integer(snapshot.effectOrder[position]));
	json_object_set_new(root, "effectOrder", order);
	json_t* macros = json_array();
	for (int macro = 0; macro < 4; ++macro)
		json_array_append_new(macros, json_integer(snapshotMacros[macro]));
	json_object_set_new(root, "macroTargets", macros);

	json_t* scenes = json_array();
	for (int sceneIndex = 0; sceneIndex < kSceneCount; ++sceneIndex) {
		const SceneState& scene = snapshot.scenes[sceneIndex];
		json_t* sceneJ = json_object();
		json_object_set_new(sceneJ, "beats", json_integer(scene.beats));
		json_object_set_new(sceneJ, "divisions", json_integer(scene.divisions));
		json_object_set_new(sceneJ, "loop", json_boolean(scene.loop));
		json_object_set_new(sceneJ, "seed", json_integer(static_cast<json_int_t>(scene.seed)));

		json_t* lanes = json_array();
		for (int lane = 0; lane < kLaneCount; ++lane)
			json_array_append_new(lanes, maskPairJson(scene.lanes[lane].activeMask, scene.lanes[lane].startMask));
		json_object_set_new(sceneJ, "lanes", lanes);
		json_t* weights = json_array();
		for (int effect = 0; effect < kEffectCount; ++effect)
			json_array_append_new(weights, json_real(scene.randomWeights[effect]));
		json_object_set_new(sceneJ, "weights", weights);

		json_t* effectArray = json_array();
		for (int effect = 0; effect < kEffectCount; ++effect) {
			const EffectSettings& settings = scene.effects[effect];
			json_t* effectJ = json_object();
			json_t* values = json_array();
			for (int slot = 0; slot < kEffectParamCount; ++slot)
				json_array_append_new(values, json_real(settings.values[slot]));
			json_object_set_new(effectJ, "values", values);
			json_t* common = json_array();
			json_array_append_new(common, json_integer(settings.common.filterType));
			json_array_append_new(common, json_real(settings.common.cutoff));
			json_array_append_new(common, json_real(settings.common.q));
			json_array_append_new(common, json_real(settings.common.mix));
			json_array_append_new(common, json_real(settings.common.pan));
			json_array_append_new(common, json_real(settings.common.gain));
			json_object_set_new(effectJ, "common", common);
			json_array_append_new(effectArray, effectJ);
		}
		json_object_set_new(sceneJ, "effects", effectArray);
		json_array_append_new(scenes, sceneJ);
	}
	json_object_set_new(root, "scenes", scenes);
	return root;
}

void Fray::dataFromJson(json_t* root) {
	AtomicBoolScope runtimeReset(suppressDeferredCapture);
	AuthoredStateGuard stateGuard(authoredStateLock);
	ProgramState restored;
	if (!root || !json_is_object(root)) {
		program = restored;
		activeScene = 0;
		sceneCvMode.store(0);
		resetOnRun.store(false);
		for (int macro = 0; macro < 4; ++macro)
			macroTargets[macro].store(-1 - macro);
		uiEffectOrderPacked.store(identityEffectOrder(), std::memory_order_release);
		gridEpoch.fetch_add(1, std::memory_order_acq_rel);
		gridCommands.clear();
		loadSceneToParams();
		++stateRevision;
		resetRuntime();
		return;
	}

	json_t* master = json_object_get(root, "master");
	restored.masterMix = jsonNumber(master, "mix", restored.masterMix);
	restored.masterPan = jsonNumber(master, "pan", restored.masterPan);
	restored.masterGain = jsonNumber(master, "gain", restored.masterGain);
	restored.triggerMode = jsonInteger(master, "triggerMode", restored.triggerMode);
	restored.quantizeMode = jsonInteger(master, "quantizeMode", restored.quantizeMode);
	json_t* order = json_object_get(root, "effectOrder");
	if (order && json_is_array(order)) {
		for (int position = 0; position < kEffectCount; ++position) {
			json_t* value = json_array_get(order, position);
			if (value && json_is_integer(value))
				restored.effectOrder[position] = jsonIntegerValue(value, restored.effectOrder[position]);
		}
	}

	json_t* scenes = json_object_get(root, "scenes");
	if (scenes && json_is_array(scenes)) {
		const int count = std::min<int>(kSceneCount, static_cast<int>(json_array_size(scenes)));
		for (int sceneIndex = 0; sceneIndex < count; ++sceneIndex) {
			json_t* sceneJ = json_array_get(scenes, sceneIndex);
			if (!sceneJ || !json_is_object(sceneJ))
				continue;
			SceneState& scene = restored.scenes[sceneIndex];
			scene.beats = jsonInteger(sceneJ, "beats", scene.beats);
			scene.divisions = jsonInteger(sceneJ, "divisions", scene.divisions);
			scene.loop = jsonBoolean(sceneJ, "loop", scene.loop);
			json_t* seed = json_object_get(sceneJ, "seed");
			if (seed && json_is_integer(seed))
				scene.seed = static_cast<uint32_t>(json_integer_value(seed));

			json_t* lanes = json_object_get(sceneJ, "lanes");
			if (lanes && json_is_array(lanes)) {
				for (int lane = 0; lane < kLaneCount; ++lane) {
					json_t* laneJ = json_array_get(lanes, lane);
					scene.lanes[lane].activeMask = jsonMask(laneJ, 0, 1);
					scene.lanes[lane].startMask = jsonMask(laneJ, 2, 3);
				}
			}
			json_t* weights = json_object_get(sceneJ, "weights");
			if (weights && json_is_array(weights)) {
				for (int effect = 0; effect < kEffectCount; ++effect) {
					json_t* value = json_array_get(weights, effect);
					if (value && json_is_number(value))
						scene.randomWeights[effect] = static_cast<float>(json_number_value(value));
				}
			}

			json_t* effectsJ = json_object_get(sceneJ, "effects");
			if (effectsJ && json_is_array(effectsJ)) {
				for (int effect = 0; effect < kEffectCount; ++effect) {
					json_t* effectJ = json_array_get(effectsJ, effect);
					if (!effectJ || !json_is_object(effectJ))
						continue;
					EffectSettings& settings = scene.effects[effect];
					json_t* values = json_object_get(effectJ, "values");
					if (values && json_is_array(values)) {
						for (int slot = 0; slot < kEffectParamCount; ++slot) {
							json_t* value = json_array_get(values, slot);
							if (value && json_is_number(value))
								settings.values[slot] = static_cast<float>(json_number_value(value));
						}
					}
					json_t* common = json_object_get(effectJ, "common");
					if (common && json_is_array(common)) {
						json_t* value = json_array_get(common, 0);
						if (value && json_is_integer(value)) settings.common.filterType = jsonIntegerValue(value, settings.common.filterType);
						value = json_array_get(common, 1);
						if (value && json_is_number(value)) settings.common.cutoff = static_cast<float>(json_number_value(value));
						value = json_array_get(common, 2);
						if (value && json_is_number(value)) settings.common.q = static_cast<float>(json_number_value(value));
						value = json_array_get(common, 3);
						if (value && json_is_number(value)) settings.common.mix = static_cast<float>(json_number_value(value));
						value = json_array_get(common, 4);
						if (value && json_is_number(value)) settings.common.pan = static_cast<float>(json_number_value(value));
						value = json_array_get(common, 5);
						if (value && json_is_number(value)) settings.common.gain = static_cast<float>(json_number_value(value));
					}
				}
			}
		}
	}
	sanitizeProgramKeepingHiddenCells(restored);
	program = restored;
	activeScene = clamp(jsonInteger(root, "activeScene", 0), 0, kSceneCount - 1);
	sceneCvMode.store(clamp(jsonInteger(root, "sceneCvMode", 0), 0, 1));
	resetOnRun.store(jsonBoolean(root, "resetOnRun", false));
	for (int macro = 0; macro < 4; ++macro)
		macroTargets[macro].store(-1 - macro);
	json_t* macros = json_object_get(root, "macroTargets");
	if (macros && json_is_array(macros)) {
		for (int macro = 0; macro < 4; ++macro) {
			json_t* value = json_array_get(macros, macro);
			if (!value || !json_is_integer(value))
				continue;
			const int target = jsonIntegerValue(value, -1 - macro);
			macroTargets[macro].store(target < 0
				? clamp(target, -8, -1)
				: clamp(target, 0, kEffectCount * kEffectParamCount - 1));
		}
	}
	uiEffectOrderPacked.store(packEffectOrder(program.effectOrder), std::memory_order_release);
	gridEpoch.fetch_add(1, std::memory_order_acq_rel);
	gridCommands.clear();
	loadSceneToParams();
	++stateRevision;
	resetRuntime();
}

FrayWidget::FrayWidget(Fray* module) {
	setModule(module);
	setPanel(createPanel(asset::plugin(pluginInstance, "res/FRAY.svg")));

	addChild(createWidget<ThemedScrew>(Vec(15.f, 0.f)));
	addChild(createWidget<ThemedScrew>(Vec(kPanelWidth - 30.f, 0.f)));
	addChild(createWidget<ThemedScrew>(Vec(15.f, RACK_GRID_HEIGHT - 15.f)));
	addChild(createWidget<ThemedScrew>(Vec(kPanelWidth - 30.f, RACK_GRID_HEIGHT - 15.f)));

	FrayLabelsWidget* labels = new FrayLabelsWidget;
	labels->module = module;
	labels->box.size = Vec(kPanelWidth, RACK_GRID_HEIGHT);
	addChild(labels);

	FrayGridWidget* grid = new FrayGridWidget;
	grid->box.pos = Vec(18.f, 62.f);
	grid->setSize(Vec(428.f, 162.f));
	grid->setModule(module);
	addChild(grid);

	FrayStatusWidget* status = new FrayStatusWidget;
	status->module = module;
	status->box.pos = Vec(462.f, 62.f);
	status->box.size = Vec(150.f, 126.f);
	addChild(status);

	addParam(createParamCentered<RoundSmallBlackKnob>(Vec(72.f, 30.f), module, Fray::TEMPO_PARAM));
	addParam(createParamCentered<CKSS>(Vec(108.f, 30.f), module, Fray::EXT_CLOCK_MODE_PARAM));
	addParam(createParamCentered<RoundBlackSnapKnob>(Vec(145.f, 30.f), module, Fray::BEATS_PARAM));
	addParam(createParamCentered<RoundBlackSnapKnob>(Vec(181.f, 30.f), module, Fray::DIVISIONS_PARAM));
	addParam(createParamCentered<RoundBlackSnapKnob>(Vec(220.f, 30.f), module, Fray::SCENE_PARAM));
	addParam(createParamCentered<CKSS>(Vec(258.f, 30.f), module, Fray::LOOP_PARAM));
	addParam(createParamCentered<VCVLatch>(Vec(292.f, 30.f), module, Fray::RUN_PARAM));
	addParam(createParamCentered<VCVButton>(Vec(326.f, 30.f), module, Fray::RESET_PARAM));
	FrayNativeActionButton* randomButton = createParamCentered<FrayNativeActionButton>(
		Vec(360.f, 30.f), module, Fray::RANDOMIZE_PARAM);
	randomButton->owner = this;
	randomButton->randomize = true;
	addParam(randomButton);
	addParam(createParamCentered<VCVButton>(Vec(394.f, 30.f), module, Fray::MUTATE_PARAM));
	addParam(createParamCentered<RoundSmallBlackKnob>(Vec(520.f, 30.f), module, Fray::MASTER_MIX_PARAM));
	addParam(createParamCentered<RoundSmallBlackKnob>(Vec(558.f, 30.f), module, Fray::MASTER_PAN_PARAM));
	addParam(createParamCentered<RoundSmallBlackKnob>(Vec(596.f, 30.f), module, Fray::MASTER_GAIN_PARAM));
	addParam(createParamCentered<RoundBlackSnapKnob>(Vec(590.f, 151.f), module, Fray::SELECT_EFFECT_PARAM));

	addParam(createParamCentered<Trimpot>(Vec(482.f, 198.f), module, Fray::MACRO_A_PARAM));
	addParam(createParamCentered<Trimpot>(Vec(522.f, 198.f), module, Fray::MACRO_B_PARAM));
	addParam(createParamCentered<Trimpot>(Vec(562.f, 198.f), module, Fray::MACRO_C_PARAM));
	addParam(createParamCentered<Trimpot>(Vec(602.f, 198.f), module, Fray::MACRO_D_PARAM));

	const float parameterX[7] = {50.f, 138.f, 226.f, 314.f, 402.f, 490.f, 578.f};
	for (int effect = 0; effect < kEffectCount; ++effect) {
		for (int slot = 0; slot < Fray::EFFECT_PARAM_COUNT; ++slot) {
			const Vec position = slot < 7
				? Vec(parameterX[slot], 251.f)
				: Vec(parameterX[slot - 7], 283.f);
			ParamWidget* widget = NULL;
			if (slot == 8)
				widget = createParamCentered<RoundBlackSnapKnob>(position, module, Fray::effectParamId(effect, slot));
			else
				widget = createParamCentered<RoundSmallBlackKnob>(position, module, Fray::effectParamId(effect, slot));
			widget->visible = effect == 0;
			effectWidgets[effect].push_back(widget);
			addParam(widget);
		}
	}

	const int topInputs[11] = {
		Fray::AUDIO_L_INPUT, Fray::AUDIO_R_INPUT, Fray::CLOCK_INPUT, Fray::TEMPO_CV_INPUT,
		Fray::RESET_INPUT, Fray::RUN_INPUT, Fray::SCENE_INPUT, Fray::SCENE_GATE_INPUT,
		Fray::NEXT_INPUT, Fray::PREV_INPUT, Fray::RANDOMIZE_INPUT
	};
	for (int column = 0; column < 11; ++column) {
		const float x = 30.f + 57.f * column;
		addInput(createInputCentered<PJ301MPort>(Vec(x, 327.f), module, topInputs[column]));
	}
	const int bottomInputs[6] = {
		Fray::MUTATE_INPUT, Fray::MIX_CV_INPUT, Fray::MOD_A_INPUT,
		Fray::MOD_B_INPUT, Fray::MOD_C_INPUT, Fray::MOD_D_INPUT
	};
	for (int column = 0; column < 6; ++column) {
		const float x = 30.f + 57.f * column;
		addInput(createInputCentered<PJ301MPort>(Vec(x, 357.f), module, bottomInputs[column]));
	}
	const int bottomOutputs[5] = {
		Fray::AUDIO_L_OUTPUT, Fray::AUDIO_R_OUTPUT, Fray::STEP_OUTPUT,
		Fray::EOC_OUTPUT, Fray::ACTIVITY_OUTPUT
	};
	for (int output = 0; output < 5; ++output) {
		const float x = 30.f + 57.f * (output + 6);
		addOutput(createOutputCentered<PJ301MPort>(Vec(x, 357.f), module, bottomOutputs[output]));
	}

	addChild(createLightCentered<TinyLight<GreenLight>>(Vec(292.f, 15.f), module, Fray::RUN_LIGHT));
	addChild(createLightCentered<TinyLight<BlueLight>>(Vec(108.f, 15.f), module, Fray::CLOCK_LIGHT));
	addChild(createLightCentered<TinyLight<YellowLight>>(Vec(360.f, 15.f), module, Fray::RANDOMIZE_LIGHT));
	for (int effect = 0; effect < kEffectCount; ++effect)
		addChild(createLightCentered<TinyLight<YellowLight>>(
			Vec(472.f + effect * 14.f, 220.f), module, Fray::EFFECT_LIGHT + effect));
}

void FrayWidget::step() {
	int selected = 0;
	Fray* fray = dynamic_cast<Fray*>(module);
	if (fray)
		selected = clamp(static_cast<int>(std::round(fray->params[Fray::SELECT_EFFECT_PARAM].getValue())), 0, kEffectCount - 1);
	if (selected != visibleEffect) {
		for (int effect = 0; effect < kEffectCount; ++effect) {
			for (size_t widget = 0; widget < effectWidgets[effect].size(); ++widget)
				effectWidgets[effect][widget]->visible = effect == selected;
		}
		visibleEffect = selected;
	}
	ModuleWidget::step();
}

void FrayWidget::appendContextMenu(Menu* menu) {
	Fray* fray = dynamic_cast<Fray*>(module);
	if (!fray)
		return;
	menu->addChild(new MenuSeparator);
	menu->addChild(createMenuLabel("Fray"));
	menu->addChild(createIndexSubmenuItem("Scene CV mapping", {"1 V/oct", "0–10 V"},
		[=]() { return static_cast<size_t>(clamp(fray->sceneCvMode.load(), 0, 1)); },
		[=](size_t mode) { fray->sceneCvMode.store(clamp(static_cast<int>(mode), 0, 1)); }));
	menu->addChild(createBoolMenuItem("Reset transport when Run rises", "",
		[=]() { return fray->resetOnRun.load(); },
		[=](bool value) { fray->resetOnRun.store(value); }));

	menu->addChild(new MenuSeparator);
	menu->addChild(createMenuLabel("Macro destinations"));
	std::vector<std::string> macroLabels;
	for (int slot = 0; slot < 4; ++slot)
		macroLabels.push_back(string::f("Selected effect — control %d", slot + 1));
	for (int effect = 0; effect < kEffectCount; ++effect) {
		for (int slot = 0; slot < 8; ++slot)
			macroLabels.push_back(std::string(kEffectNames[effect]) + " — " + kUniqueParamNames[effect][slot]);
	}
	for (int macro = 0; macro < 4; ++macro) {
		const std::string label = string::f("Macro %c", 'A' + macro);
		menu->addChild(createIndexSubmenuItem(label, macroLabels,
			[=]() {
				const int target = fray->macroTargets[macro].load();
				if (target < 0)
					return static_cast<size_t>(clamp(-target - 1, 0, 3));
				const int effect = clamp(target / kEffectParamCount, 0, kEffectCount - 1);
				const int slot = clamp(target % kEffectParamCount, 0, 7);
				return static_cast<size_t>(4 + effect * 8 + slot);
			},
			[=](size_t index) {
				if (index < 4)
					fray->macroTargets[macro].store(-1 - static_cast<int>(index));
				else {
					const int flattened = clamp(static_cast<int>(index) - 4, 0, kEffectCount * 8 - 1);
					fray->macroTargets[macro].store((flattened / 8) * kEffectParamCount + flattened % 8);
				}
			}));
	}

	menu->addChild(new MenuSeparator);
	menu->addChild(createMenuLabel("Serial effect order"));
	std::vector<std::string> effectLabels;
	for (int effect = 0; effect < kEffectCount; ++effect)
		effectLabels.push_back(kEffectNames[effect]);
	for (int position = 0; position < kEffectCount; ++position) {
		menu->addChild(createIndexSubmenuItem(string::f("Position %d", position + 1), effectLabels,
			[=]() {
				return static_cast<size_t>(unpackEffectOrder(
					fray->uiEffectOrderPacked.load(std::memory_order_acquire), position));
			},
			[=](size_t selection) {
				const int selectedEffect = clamp(static_cast<int>(selection), 0, kEffectCount - 1);
				uint64_t current = fray->uiEffectOrderPacked.load(std::memory_order_acquire);
				uint64_t updated = 0;
				do {
					updated = swappedEffectOrder(current, position, selectedEffect);
				} while (!fray->uiEffectOrderPacked.compare_exchange_weak(
					current, updated, std::memory_order_release, std::memory_order_acquire));
			}));
	}
}

Model* modelFray = createModel<Fray, FrayWidget>("Fray");
