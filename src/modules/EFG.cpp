// EFG Eightfold Function Generator (spec §4).
#include "../plugin.hpp"
#include "../dsp/EfgEngine.hpp"
#include "../widgets/EightfoldComponents.hpp"
#include "../generated/Layout.hpp"

struct EFG : Module {
	enum ParamId {
		SPAN_PARAM, SPAN_ATT_PARAM, RISE_PARAM, RISE_ATT_PARAM, FALL_PARAM, FALL_ATT_PARAM,
		RATE_PARAM, RATE_ATT_PARAM, OSC_PARAM, OSC_ATT_PARAM, STRENGTH_PARAM, STRENGTH_ATT_PARAM,
		CURVE_PARAM, SHAPE_PARAM, SPREAD_PARAM,
		MODE_PARAM, CYCLE_PARAM, CYCLETYPE_PARAM, BIPOLAR_PARAM, SUBMIX_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SPAN_INPUT, RISE_INPUT, FALL_INPUT, RATE_INPUT, OSC_INPUT, STRENGTH_INPUT,
		CURVE_INPUT, SHAPE_INPUT, SPREAD_INPUT,
		ACTIVATE_INPUT, ACCUM_INPUT, RESET_INPUT, CYCLEIN_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT1_OUTPUT, OUT2_OUTPUT, OUT3_OUTPUT, OUT4_OUTPUT, OUT5_OUTPUT, OUT6_OUTPUT, OUT7_OUTPUT, OUT8_OUTPUT,
		POLY_OUTPUT, INDEX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		MODE_LIGHT = 0, CYCLE_LIGHT = 3, CYCLETYPE_LIGHT = 6, BIPOLAR_LIGHT = 9, SUBMIX_LIGHT = 12,
		CH1_LIGHT = 15, CH2_LIGHT = 18, CH3_LIGHT = 21, CH4_LIGHT = 24,
		CH5_LIGHT = 27, CH6_LIGHT = 30, CH7_LIGHT = 33, CH8_LIGHT = 36,
		LIGHTS_LEN = 39
	};

	ef::EfgEngine engine;
	ef::EfgParams P;
	ef::EfgInputs I;
	int mode = ef::SPAN_INDEX;
	int cycleType = ef::CYCLE_ALL;
	float indexV = ef::INDEX_V_DEFAULT;
	dsp::BooleanTrigger modeBtn, typeBtn;
	dsp::ClockDivider lightDiv;

	EFG() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SPAN_PARAM, -1.f, 1.f, 0.f, "Span (channel select / step / division, by mode)");
		configParam(SPAN_ATT_PARAM, -1.f, 1.f, 0.f, "Span CV attenuverter (full CW with Span centred = Channel Index receive)", "%", 0.f, 100.f);
		configParam(RISE_PARAM, 0.f, 1.f, 0.25f, "Rise time", " s", 65536.f, ef::EFG_T_MIN);
		configParam(RISE_ATT_PARAM, -1.f, 1.f, 0.f, "Rise attenuverter (Spread depth, or CV depth when Rise CV is patched)", "%", 0.f, 100.f);
		configParam(FALL_PARAM, 0.f, 1.f, 0.6f, "Fall time", " s", 65536.f, ef::EFG_T_MIN);
		configParam(FALL_ATT_PARAM, -1.f, 1.f, 0.f, "Fall attenuverter (Spread depth, or CV depth when Fall CV is patched)", "%", 0.f, 100.f);
		configParam(RATE_PARAM, 0.f, 1.f, 0.4f, "Oscillation rate", "%", 0.f, 100.f);
		configParam(RATE_ATT_PARAM, -1.f, 1.f, 0.f, "Rate CV attenuverter (full CW = 1 V/oct)", "%", 0.f, 100.f);
		configParam(OSC_PARAM, 0.f, 1.f, 0.f, "Oscillation amount", "%", 0.f, 100.f);
		configParam(OSC_ATT_PARAM, -1.f, 1.f, 0.f, "Osc attenuverter (Spread depth, or CV depth when Osc CV is patched)", "%", 0.f, 100.f);
		configParam(STRENGTH_PARAM, -1.f, 1.f, 1.f, "Strength", " V", 0.f, ef::EFG_OUT_V);
		configParam(STRENGTH_ATT_PARAM, -1.f, 1.f, 0.f, "Strength attenuverter (Spread depth, or CV depth when Strength CV is patched)", "%", 0.f, 100.f);
		configParam(CURVE_PARAM, 0.f, 1.f, 0.5f, "Curve (log, linear at centre, exp)", "%", 0.f, 100.f);
		configParam(SHAPE_PARAM, -1.f, 1.f, 0.f, "Oscillation shape (falling saw, triangle at centre, rising ramp)");
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread (attenuates Spread CV when patched)", "%", 0.f, 100.f);
		configButton(MODE_PARAM, "Span mode: Index, Round, Parallel, Binary");
		configSwitch(CYCLE_PARAM, 0.f, 1.f, 0.f, "Cycle", {"Off", "On"});
		configButton(CYCLETYPE_PARAM, "Cycle type: All / Follow the leader");
		configSwitch(BIPOLAR_PARAM, 0.f, 1.f, 0.f, "Oscillation", {"Unipolar", "Bipolar"});
		configSwitch(SUBMIX_PARAM, 0.f, 1.f, 0.f, "Submix", {"Off", "On"});
		configInput(SPAN_INPUT, "Span CV / Channel Index in");
		configInput(RISE_INPUT, "Rise CV (sampled per channel at activation)");
		configInput(FALL_INPUT, "Fall CV (sampled per channel at activation)");
		configInput(RATE_INPUT, "Rate CV (sampled per channel at activation)");
		configInput(OSC_INPUT, "Osc CV (sampled per channel at activation)");
		configInput(STRENGTH_INPUT, "Strength CV (sampled per channel at activation)");
		configInput(CURVE_INPUT, "Curve CV");
		configInput(SHAPE_INPUT, "Shape CV");
		configInput(SPREAD_INPUT, "Spread CV");
		configInput(ACTIVATE_INPUT, "Activate");
		configInput(ACCUM_INPUT, "Accumulate");
		configInput(RESET_INPUT, "Reset");
		configInput(CYCLEIN_INPUT, "Cycle gate");
		for (int i = 0; i < 8; i++) configOutput(OUT1_OUTPUT + i, string::f("Channel %d", i + 1));
		configOutput(POLY_OUTPUT, "All 8 channels (polyphonic)");
		configOutput(INDEX_OUTPUT, "Channel Index");
		configLight(MODE_LIGHT, "Span mode");
		configLight(CYCLE_LIGHT, "Cycle");
		configLight(CYCLETYPE_LIGHT, "Cycle type");
		configLight(BIPOLAR_LIGHT, "Bipolar");
		configLight(SUBMIX_LIGHT, "Submix");
		for (int i = 0; i < 8; i++) configLight(CH1_LIGHT + 3 * i, string::f("Channel %d", i + 1));
		lightDiv.setDivision(32);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		mode = ef::SPAN_INDEX; cycleType = ef::CYCLE_ALL; indexV = ef::INDEX_V_DEFAULT;
		engine.reset();
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "mode", json_integer(mode));
		json_object_set_new(r, "cycleType", json_integer(cycleType));
		json_object_set_new(r, "indexV", json_real(indexV));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "mode")) mode = clamp((int) json_integer_value(j), 0, ef::SPAN_MODES - 1);
		if (json_t* j = json_object_get(r, "cycleType")) cycleType = clamp((int) json_integer_value(j), 0, 1);
		if (json_t* j = json_object_get(r, "indexV")) indexV = json_number_value(j) > 0.75 ? 1.f : 0.5f;
	}

	void process(const ProcessArgs& args) override {
		if (modeBtn.process(params[MODE_PARAM].getValue() > 0.5f)) mode = (mode + 1) % ef::SPAN_MODES;
		if (typeBtn.process(params[CYCLETYPE_PARAM].getValue() > 0.5f)) cycleType ^= 1;

		P.span = params[SPAN_PARAM].getValue();
		P.spanAtt = params[SPAN_ATT_PARAM].getValue();
		P.curve = params[CURVE_PARAM].getValue();
		P.rise = params[RISE_PARAM].getValue();
		P.riseAtt = params[RISE_ATT_PARAM].getValue();
		P.fall = params[FALL_PARAM].getValue();
		P.fallAtt = params[FALL_ATT_PARAM].getValue();
		P.shape = params[SHAPE_PARAM].getValue();
		P.strength = params[STRENGTH_PARAM].getValue();
		P.strengthAtt = params[STRENGTH_ATT_PARAM].getValue();
		P.rate = params[RATE_PARAM].getValue();
		P.rateAtt = params[RATE_ATT_PARAM].getValue();
		P.osc = params[OSC_PARAM].getValue();
		P.oscAtt = params[OSC_ATT_PARAM].getValue();
		P.spread = params[SPREAD_PARAM].getValue();
		P.mode = mode;
		P.cycle = params[CYCLE_PARAM].getValue() > 0.5f;
		P.cycleType = cycleType;
		P.bipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;
		P.submix = params[SUBMIX_PARAM].getValue() > 0.5f;
		P.indexV = indexV;

		I.activate = inputs[ACTIVATE_INPUT].getVoltage();
		I.activateP = inputs[ACTIVATE_INPUT].isConnected();
		I.accum = inputs[ACCUM_INPUT].getVoltage();
		I.accumP = inputs[ACCUM_INPUT].isConnected();
		I.reset = inputs[RESET_INPUT].getVoltage();
		I.cycleIn = inputs[CYCLEIN_INPUT].getVoltage();
		I.spanCv = inputs[SPAN_INPUT].getVoltage();
		I.spanCvP = inputs[SPAN_INPUT].isConnected();
		I.curveCv = inputs[CURVE_INPUT].getVoltage();
		I.shapeCv = inputs[SHAPE_INPUT].getVoltage();
		I.spreadCv = inputs[SPREAD_INPUT].getVoltage();
		I.spreadCvP = inputs[SPREAD_INPUT].isConnected();
		I.riseCv = inputs[RISE_INPUT].getVoltage();     I.riseP = inputs[RISE_INPUT].isConnected();
		I.fallCv = inputs[FALL_INPUT].getVoltage();     I.fallP = inputs[FALL_INPUT].isConnected();
		I.rateCv = inputs[RATE_INPUT].getVoltage();     I.rateP = inputs[RATE_INPUT].isConnected();
		I.oscCv = inputs[OSC_INPUT].getVoltage();       I.oscP = inputs[OSC_INPUT].isConnected();
		I.strengthCv = inputs[STRENGTH_INPUT].getVoltage(); I.strengthP = inputs[STRENGTH_INPUT].isConnected();
		for (int i = 0; i < 8; i++) I.outP[i] = outputs[OUT1_OUTPUT + i].isConnected();

		engine.process(args.sampleTime, P, I);

		outputs[POLY_OUTPUT].setChannels(8);
		for (int i = 0; i < 8; i++) {
			outputs[OUT1_OUTPUT + i].setVoltage(engine.out[i]);
			outputs[POLY_OUTPUT].setVoltage(clamp(engine.ch[i].out, -10.f, 10.f), i);
		}
		outputs[INDEX_OUTPUT].setVoltage(engine.indexOut);

		if (lightDiv.process()) updateLights();
	}

	void updateLights() {
		static const Rgb modeCol[4] = {col::white, col::yellow, col::cyan, col::magenta};
		setRgb(this, MODE_LIGHT, modeCol[mode], mode == ef::SPAN_INDEX && !engine.indexReceive ? 0.55f : 1.f);
		Rgb ct = cycleType == ef::CYCLE_ALL ? col::purple : col::pink;
		setRgb(this, CYCLE_LIGHT, ct, P.cycle ? 1.f : 0.f);
		setRgb(this, CYCLETYPE_LIGHT, ct, 0.6f);
		setRgb(this, BIPOLAR_LIGHT, col::orange, P.bipolar ? 1.f : 0.f);
		setRgb(this, SUBMIX_LIGHT, col::green, P.submix ? 1.f : 0.f);
		for (int i = 0; i < 8; i++) {
			int id = CH1_LIGHT + 3 * i;
			float v = engine.ch[i].out;
			if (engine.held(i)) setRgb(this, id, col::orange);
			else if (std::fabs(v) > 0.05f) setRgb(this, id, v > 0.f ? col::green : col::red, std::min(1.f, std::fabs(v) / 9.f + 0.1f));
			else if (engine.cursor == i) setRgb(this, id, col::white, 0.3f);
			else if (engine.weight[i] > 0.f) setRgb(this, id, col::blue, std::min(1.f, engine.weight[i]) * 0.6f);
			else setRgb(this, id, col::off);
		}
	}
};

struct EFGWidget : ModuleWidget {
	EFGWidget(EFG* module) {
		efScaffold(this, module, "EFG");
		placeEFG(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		EFG* m = getModule<EFG>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Span mode", {"Index", "Round", "Parallel", "Binary"}, &m->mode));
		menu->addChild(createIndexPtrSubmenuItem("Cycle type", {"All", "Follow the leader"}, &m->cycleType));
		menu->addChild(createBoolMenuItem("Bipolar oscillation", "",
			[=]() { return m->params[EFG::BIPOLAR_PARAM].getValue() > 0.5f; },
			[=](bool b) { m->params[EFG::BIPOLAR_PARAM].setValue(b); }));
		menu->addChild(createBoolMenuItem("Submix", "",
			[=]() { return m->params[EFG::SUBMIX_PARAM].getValue() > 0.5f; },
			[=](bool b) { m->params[EFG::SUBMIX_PARAM].setValue(b); }));
		menu->addChild(indexScaleItem(&m->indexV));
		menu->addChild(createMenuItem("Reset all channels", "", [=]() { m->engine.reset(); }));
	}
};

Model* modelEFG = createModel<EFG, EFGWidget>("EFG");
