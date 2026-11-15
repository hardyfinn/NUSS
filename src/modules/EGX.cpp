// EGX Eightfold Gesture Extractor (spec §6).
#include "../plugin.hpp"
#include "../dsp/EgxEngine.hpp"
#include "../widgets/EightfoldComponents.hpp"
#include "../generated/Layout.hpp"

struct EGX : Module {
	enum ParamId { SPAN_PARAM, SPACE_PARAM, SPAN_ATT_PARAM, PARAMS_LEN };
	enum InputId { SPACE_INPUT, SPAN_INPUT, CLOCK_INPUT, INPUTS_LEN };
	enum OutputId {
		OUT1_OUTPUT, OUT2_OUTPUT, OUT3_OUTPUT, OUT4_OUTPUT, OUT5_OUTPUT, OUT6_OUTPUT, OUT7_OUTPUT, OUT8_OUTPUT,
		ODD_OUTPUT, EVEN_OUTPUT, PULSE_OUTPUT, POLY_OUTPUT, INDEX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		INDEXMODE_LIGHT = 0,
		CH1_LIGHT = 3, CH2_LIGHT = 6, CH3_LIGHT = 9, CH4_LIGHT = 12,
		CH5_LIGHT = 15, CH6_LIGHT = 18, CH7_LIGHT = 21, CH8_LIGHT = 24,
		PULSE_LIGHT = 27,
		LIGHTS_LEN = 30
	};

	ef::EgxEngine engine;
	ef::EgxParams P;
	ef::EgxInputs I;
	float indexV = ef::INDEX_V_DEFAULT;
	bool gate5 = false;
	bool pulseOnLeave = false;
	dsp::PulseGenerator pulseLight;
	dsp::ClockDivider lightDiv;

	EGX() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SPAN_PARAM, -1.f, 1.f, 0.f, "Span offset", " V", 0.f, 5.f);
		configParam(SPACE_PARAM, 0.f, 1.f, 1.f, "Space (volts to sweep all 8 channels)", " V", ef::EGX_R_RATIO, ef::EGX_R_MIN);
		configParam(SPAN_ATT_PARAM, -1.f, 1.f, 1.f, "Span CV attenuverter", "%", 0.f, 100.f);
		configInput(SPACE_INPUT, "Space CV");
		configInput(SPAN_INPUT, "Span CV (the signal whose motion is watched)");
		configInput(CLOCK_INPUT, "Clock (channel changes only on rising edges)");
		for (int i = 0; i < 8; i++) configOutput(OUT1_OUTPUT + i, string::f("Channel %d gate", i + 1));
		configOutput(ODD_OUTPUT, "Odd channel gate");
		configOutput(EVEN_OUTPUT, "Even channel gate");
		configOutput(PULSE_OUTPUT, "Pulse on each channel change");
		configOutput(POLY_OUTPUT, "All 8 gates (polyphonic)");
		configOutput(INDEX_OUTPUT, "Channel Index");
		configLight(INDEXMODE_LIGHT, "Channel Index mode (Span full CCW, Atten full CW, Space full CW)");
		for (int i = 0; i < 8; i++) configLight(CH1_LIGHT + 3 * i, string::f("Channel %d", i + 1));
		configLight(PULSE_LIGHT, "Pulse");
		lightDiv.setDivision(32);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		indexV = ef::INDEX_V_DEFAULT; gate5 = false; pulseOnLeave = false;
		engine.reset();
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "indexV", json_real(indexV));
		json_object_set_new(r, "gate5", json_boolean(gate5));
		json_object_set_new(r, "pulseOnLeave", json_boolean(pulseOnLeave));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "indexV")) indexV = json_number_value(j) > 0.75 ? 1.f : 0.5f;
		if (json_t* j = json_object_get(r, "gate5")) gate5 = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "pulseOnLeave")) pulseOnLeave = json_boolean_value(j);
	}

	void process(const ProcessArgs& args) override {
		P.span = params[SPAN_PARAM].getValue();
		P.atten = params[SPAN_ATT_PARAM].getValue();
		P.space = params[SPACE_PARAM].getValue();
		P.indexV = indexV;
		P.gateV = gate5 ? 5.f : 10.f;
		P.pulseOnLeave = pulseOnLeave;
		I.spanCv = inputs[SPAN_INPUT].getVoltage();
		I.spanCvP = inputs[SPAN_INPUT].isConnected();
		I.spaceCv = inputs[SPACE_INPUT].getVoltage();
		I.clock = inputs[CLOCK_INPUT].getVoltage();
		I.clockP = inputs[CLOCK_INPUT].isConnected();

		int pulsesBefore = engine.pulses;
		engine.process(args.sampleTime, P, I);
		if (engine.pulses != pulsesBefore) pulseLight.trigger(0.05f);

		outputs[POLY_OUTPUT].setChannels(8);
		for (int i = 0; i < 8; i++) {
			float g = engine.gate(i, P);
			outputs[OUT1_OUTPUT + i].setVoltage(g);
			outputs[POLY_OUTPUT].setVoltage(g, i);
		}
		outputs[ODD_OUTPUT].setVoltage(engine.odd(P));
		outputs[EVEN_OUTPUT].setVoltage(engine.even(P));
		outputs[PULSE_OUTPUT].setVoltage(engine.pulseHigh ? 10.f : 0.f);
		outputs[INDEX_OUTPUT].setVoltage(engine.indexOut);

		bool pl = pulseLight.process(args.sampleTime);
		if (lightDiv.process()) {
			setRgb(this, INDEXMODE_LIGHT, col::white, engine.indexMode ? 1.f : 0.f);
			for (int i = 0; i < 8; i++) setRgb(this, CH1_LIGHT + 3 * i, col::white, engine.active == i + 1 ? 1.f : 0.f);
			setRgb(this, PULSE_LIGHT, col::magenta, pl ? 1.f : 0.f);
		}
	}
};

struct EGXWidget : ModuleWidget {
	EGXWidget(EGX* module) {
		efScaffold(this, module, "EGX");
		placeEGX(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		EGX* m = getModule<EGX>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(indexScaleItem(&m->indexV));
		menu->addChild(createIndexSubmenuItem("Gate level", {"10 V", "5 V"},
			[=]() { return m->gate5 ? 1 : 0; }, [=](int i) { m->gate5 = i == 1; }));
		menu->addChild(createBoolPtrMenuItem("Pulse when leaving the range", "", &m->pulseOnLeave));
	}
};

Model* modelEGX = createModel<EGX, EGXWidget>("EGX");
