// ETS Eightfold Time Smear (spec §5).
#include "../plugin.hpp"
#include "../dsp/EtsEngine.hpp"
#include "../widgets/EightfoldComponents.hpp"
#include "../generated/Layout.hpp"

struct ETS : Module {
	enum ParamId {
		TIME_PARAM, PHASE_PARAM, SPREAD_PARAM, SPREAD_ATT_PARAM,
		HOLD_PARAM, RESET_PARAM, READ_PARAM, RANGE_PARAM,
		PARAMS_LEN
	};
	enum InputId { TIME_INPUT, PHASE_INPUT, SPREAD_INPUT, IN_INPUT, HOLD_INPUT, RESET_INPUT, TEMPO_INPUT, INPUTS_LEN };
	enum OutputId {
		CLK_OUTPUT,
		OUT1_OUTPUT, OUT2_OUTPUT, OUT3_OUTPUT, OUT4_OUTPUT, OUT5_OUTPUT, OUT6_OUTPUT, OUT7_OUTPUT, OUT8_OUTPUT,
		POLY_OUTPUT, INDEX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		HOLD_LIGHT = 0, RESET_LIGHT = 3, READ_LIGHT = 6, RANGE_LIGHT = 9, CLOCK_LIGHT = 12,
		CH1_LIGHT = 15, CH2_LIGHT = 18, CH3_LIGHT = 21, CH4_LIGHT = 24,
		CH5_LIGHT = 27, CH6_LIGHT = 30, CH7_LIGHT = 33, CH8_LIGHT = 36,
		LIGHTS_LEN = 39
	};

	ef::EtsEngine engine;
	ef::EtsParams P;
	ef::EtsInputs I;
	int shape = ef::RD_RAMP;
	int range = ef::RANGE_0_10;
	float indexV = ef::INDEX_V_DEFAULT;
	dsp::BooleanTrigger readBtn, rangeBtn, resetBtn;
	dsp::ClockDivider lightDiv;
	bool clearRequest = false;

	ETS() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TIME_PARAM, 0.f, 1.f, 0.4f, "Time (buffer length)", " s", ef::ETS_T_RATIO, ef::ETS_T_MIN);
		configParam(PHASE_PARAM, 0.f, 1.f, 0.f, "Phase (attenuates Phase CV when patched)", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread (speed per channel)", "%", 0.f, 100.f);
		configParam(SPREAD_ATT_PARAM, -1.f, 1.f, 0.f, "Spread CV attenuverter", "%", 0.f, 100.f);
		configSwitch(HOLD_PARAM, 0.f, 1.f, 0.f, "Hold", {"Off", "On"});
		configButton(RESET_PARAM, "Reset (realign all channels)");
		configButton(READ_PARAM, "Read shape: Ramp, Saw, Triangle, Sine, Stairs, Step random, Ramplets");
		configButton(RANGE_PARAM, "Internal LFO range: 0..10 V, ±5 V, 0..5 V");
		configInput(TIME_INPUT, "Time CV (0..5 V covers the knob range, added to the knob)");
		configInput(PHASE_INPUT, "Phase CV (0..5 V covers the knob range)");
		configInput(SPREAD_INPUT, "Spread CV");
		configInput(IN_INPUT, "Signal (unpatched: internal LFO)");
		configInput(HOLD_INPUT, "Hold gate (high = held)");
		configInput(RESET_INPUT, "Reset");
		configInput(TEMPO_INPUT, "Tempo clock");
		configOutput(CLK_OUTPUT, "Clock (one pulse per buffer length)");
		for (int i = 0; i < 8; i++) configOutput(OUT1_OUTPUT + i, string::f("Copy %d", i + 1));
		configOutput(POLY_OUTPUT, "All 8 copies (polyphonic)");
		configOutput(INDEX_OUTPUT, "Channel Index (highest copy)");
		configLight(HOLD_LIGHT, "Hold");
		configLight(RESET_LIGHT, "Reset");
		configLight(READ_LIGHT, "Read shape");
		configLight(RANGE_LIGHT, "LFO range");
		configLight(CLOCK_LIGHT, "Clock");
		for (int i = 0; i < 8; i++) configLight(CH1_LIGHT + 3 * i, string::f("Copy %d speed", i + 1));
		lightDiv.setDivision(32);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		shape = ef::RD_RAMP; range = ef::RANGE_0_10; indexV = ef::INDEX_V_DEFAULT;
		engine.reset();
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "shape", json_integer(shape));
		json_object_set_new(r, "range", json_integer(range));
		json_object_set_new(r, "indexV", json_real(indexV));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "shape")) shape = clamp((int) json_integer_value(j), 0, ef::RD_SHAPES - 1);
		if (json_t* j = json_object_get(r, "range")) range = clamp((int) json_integer_value(j), 0, ef::RANGE_COUNT - 1);
		if (json_t* j = json_object_get(r, "indexV")) indexV = json_number_value(j) > 0.75 ? 1.f : 0.5f;
	}

	void process(const ProcessArgs& args) override {
		if (readBtn.process(params[READ_PARAM].getValue() > 0.5f)) shape = (shape + 1) % ef::RD_SHAPES;
		if (rangeBtn.process(params[RANGE_PARAM].getValue() > 0.5f)) range = (range + 1) % ef::RANGE_COUNT;
		if (clearRequest) { engine.clear(); clearRequest = false; }

		P.time = params[TIME_PARAM].getValue();
		P.phase = params[PHASE_PARAM].getValue();
		P.spread = params[SPREAD_PARAM].getValue();
		P.spreadAtt = params[SPREAD_ATT_PARAM].getValue();
		P.shape = shape;
		P.range = range;
		P.hold = params[HOLD_PARAM].getValue() > 0.5f;
		P.indexV = indexV;

		I.in = inputs[IN_INPUT].getVoltage();
		I.inP = inputs[IN_INPUT].isConnected();
		I.holdIn = inputs[HOLD_INPUT].getVoltage();
		I.holdInP = inputs[HOLD_INPUT].isConnected();
		I.resetIn = inputs[RESET_INPUT].getVoltage();
		I.resetButton = resetBtn.process(params[RESET_PARAM].getValue() > 0.5f);
		I.tempo = inputs[TEMPO_INPUT].getVoltage();
		I.tempoP = inputs[TEMPO_INPUT].isConnected();
		I.timeCv = inputs[TIME_INPUT].getVoltage();
		I.timeCvP = inputs[TIME_INPUT].isConnected();
		I.phaseCv = inputs[PHASE_INPUT].getVoltage();
		I.phaseCvP = inputs[PHASE_INPUT].isConnected();
		I.spreadCv = inputs[SPREAD_INPUT].getVoltage();

		engine.process(args.sampleTime, P, I);

		outputs[POLY_OUTPUT].setChannels(8);
		for (int i = 0; i < 8; i++) {
			outputs[OUT1_OUTPUT + i].setVoltage(engine.out[i]);
			outputs[POLY_OUTPUT].setVoltage(engine.out[i], i);
		}
		outputs[CLK_OUTPUT].setVoltage(engine.clkOut ? 10.f : 0.f);
		outputs[INDEX_OUTPUT].setVoltage(engine.indexOut);

		if (lightDiv.process()) updateLights();
	}

	void updateLights() {
		static const Rgb shapeCol[7] = {col::red, col::green, col::blue, col::purple, col::pink, col::orange, col::yellow};
		static const Rgb rangeCol[3] = {col::green, col::purple, col::blue};
		static const Rgb clkCol[7] = {col::red, col::orange, col::yellow, col::green, col::purple, col::blue, col::gold};
		setRgb(this, HOLD_LIGHT, col::white, engine.holding ? 1.f : 0.f);
		setRgb(this, RESET_LIGHT, col::blue, engine.resetFlash.left > 0.f ? 1.f : 0.f);
		setRgb(this, READ_LIGHT, shapeCol[shape]);
		setRgb(this, RANGE_LIGHT, rangeCol[range], inputs[IN_INPUT].isConnected() ? 0.25f : 1.f);
		if (engine.resetFlash.left > 0.f) setRgb(this, CLOCK_LIGHT, col::blue);
		else setRgb(this, CLOCK_LIGHT, clkCol[engine.clockColour()], engine.flash.left > 0.f ? 1.f : 0.15f);
		for (int i = 0; i < 8; i++) {
			float m = engine.mult[i];
			Rgb c = (m > 0.95f && m < 1.05f) ? col::aqua : m > 8.01f ? col::hotpink : m < 0.1249f ? col::yellow : m > 1.f ? col::green : col::blue;
			setRgb(this, CH1_LIGHT + 3 * i, c, 0.2f + 0.8f * std::min(1.f, std::fabs(engine.out[i]) / 10.f));
		}
	}
};

struct ETSWidget : ModuleWidget {
	ETSWidget(ETS* module) {
		efScaffold(this, module, "ETS");
		placeETS(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		ETS* m = getModule<ETS>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Read shape",
			{"Ramp (forward)", "Saw (backward)", "Triangle (ping-pong)", "Sine (wow and flutter)", "Stairs", "Step random", "Ramplets"}, &m->shape));
		menu->addChild(createIndexPtrSubmenuItem("Internal LFO range", {"0 to 10 V", "±5 V", "0 to 5 V"}, &m->range));
		menu->addChild(indexScaleItem(&m->indexV));
		menu->addChild(createMenuItem("Clear buffer", "", [=]() { m->clearRequest = true; }));
	}
};

Model* modelETS = createModel<ETS, ETSWidget>("ETS");
