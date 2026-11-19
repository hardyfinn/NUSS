// JumbleSale: 8 in, 8 out routing network after Make Noise's Jumbler.
#include "../plugin.hpp"
#include "../dsp/JumbleEngine.hpp"
#include "../widgets/EightfoldComponents.hpp"
#include "../generated/Layout.hpp"

struct JumbleSale : Module {
	enum ParamId { ROTATE_PARAM, ROTATE_ATT_PARAM, RADIATE_PARAM, RADIATE_ATT_PARAM, WRAP_PARAM, PARAMS_LEN };
	enum InputId {
		IN1_INPUT, IN2_INPUT, IN3_INPUT, IN4_INPUT, IN5_INPUT, IN6_INPUT, IN7_INPUT, IN8_INPUT,
		POLY_INPUT, ROTATE_INPUT, RADIATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT1_OUTPUT, OUT2_OUTPUT, OUT3_OUTPUT, OUT4_OUTPUT, OUT5_OUTPUT, OUT6_OUTPUT, OUT7_OUTPUT, OUT8_OUTPUT,
		POLY_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		WRAP_LIGHT = 0,
		CH1_LIGHT = 3, CH2_LIGHT = 6, CH3_LIGHT = 9, CH4_LIGHT = 12,
		CH5_LIGHT = 15, CH6_LIGHT = 18, CH7_LIGHT = 21, CH8_LIGHT = 24,
		LIGHTS_LEN = 27
	};

	ef::JumbleEngine engine;
	ef::JumbleParams P;
	ef::JumbleInputs I;
	bool average = false;
	dsp::ClockDivider lightDiv;
	float level[ef::JS_CH] = {};

	JumbleSale() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(ROTATE_PARAM, -1.f, 1.f, 0.f, "Rotate (steps; noon = straight through)", "", 0.f, 8.f);
		configParam(ROTATE_ATT_PARAM, -1.f, 1.f, 0.f, "Rotate CV attenuverter", "%", 0.f, 100.f);
		configParam(RADIATE_PARAM, 0.f, 1.f, 0.f, "Radiate (blend into neighbouring outputs)", "%", 0.f, 100.f);
		configParam(RADIATE_ATT_PARAM, -1.f, 1.f, 0.f, "Radiate CV attenuverter", "%", 0.f, 100.f);
		configSwitch(WRAP_PARAM, 0.f, 1.f, 0.f, "Wrap", {"Off: signals fall silent past the ends", "On: signals come back round"});
		for (int i = 0; i < 8; i++) configInput(IN1_INPUT + i, string::f("Input %d", i + 1));
		configInput(POLY_INPUT, "Poly (feeds any input jack left empty)");
		configInput(ROTATE_INPUT, "Rotate CV (+/-5 V = full range at full attenuverter)");
		configInput(RADIATE_INPUT, "Radiate CV (+/-5 V = full range at full attenuverter)");
		for (int i = 0; i < 8; i++) configOutput(OUT1_OUTPUT + i, string::f("Output %d", i + 1));
		configOutput(POLY_OUTPUT, "All 8 outputs (polyphonic)");
		configLight(WRAP_LIGHT, "Wrap");
		for (int i = 0; i < 8; i++) configLight(CH1_LIGHT + 3 * i, string::f("Output %d (blue: input 1 is here; green/red: +/- signal)", i + 1));
		lightDiv.setDivision(32);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		average = false;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "average", json_boolean(average));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "average")) average = json_boolean_value(j);
	}

	void process(const ProcessArgs& args) override {
		P.rotate = params[ROTATE_PARAM].getValue();
		P.rotateAtt = params[ROTATE_ATT_PARAM].getValue();
		P.radiate = params[RADIATE_PARAM].getValue();
		P.radiateAtt = params[RADIATE_ATT_PARAM].getValue();
		P.wrap = params[WRAP_PARAM].getValue() > 0.5f;
		P.average = average;
		int polyCh = inputs[POLY_INPUT].getChannels();
		for (int i = 0; i < 8; i++) {
			if (inputs[IN1_INPUT + i].isConnected()) I.in[i] = inputs[IN1_INPUT + i].getVoltage();
			else I.in[i] = i < polyCh ? inputs[POLY_INPUT].getVoltage(i) : 0.f;
		}
		I.rotateCv = inputs[ROTATE_INPUT].getVoltage();
		I.radiateCv = inputs[RADIATE_INPUT].getVoltage();

		engine.process(P, I);

		outputs[POLY_OUTPUT].setChannels(8);
		for (int k = 0; k < 8; k++) {
			outputs[OUT1_OUTPUT + k].setVoltage(engine.out[k]);
			outputs[POLY_OUTPUT].setVoltage(engine.out[k], k);
			// Peak follower for the LED, so audio shows as well as CV.
			float a = std::fabs(engine.out[k]) > std::fabs(level[k]) ? 1.f : args.sampleTime * 20.f;
			level[k] += (engine.out[k] - level[k]) * a;
		}

		if (lightDiv.process()) {
			lights[WRAP_LIGHT + 0].setBrightness(P.wrap ? 0.88f : 0.f);
			lights[WRAP_LIGHT + 1].setBrightness(P.wrap ? 0.35f : 0.f);
			lights[WRAP_LIGHT + 2].setBrightness(P.wrap ? 0.54f : 0.f);
			for (int k = 0; k < 8; k++) {
				float b = engine.input1At(k);
				float pos = ef::clampf(level[k] / 5.f, 0.f, 1.f);
				float neg = ef::clampf(-level[k] / 5.f, 0.f, 1.f);
				int id = CH1_LIGHT + 3 * k;
				if (b > 0.01f) {
					lights[id + 0].setBrightness(0.f);
					lights[id + 1].setBrightness(0.25f * b);
					lights[id + 2].setBrightness(b);
				} else {
					lights[id + 0].setBrightness(neg);
					lights[id + 1].setBrightness(pos);
					lights[id + 2].setBrightness(0.f);
				}
			}
		}
	}
};

struct JumbleSaleWidget : ModuleWidget {
	JumbleSaleWidget(JumbleSale* module) {
		efScaffold(this, module, "JumbleSale");
		placeJumbleSale(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		JumbleSale* m = getModule<JumbleSale>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Radiate keeps level (average instead of sum)", "", &m->average));
	}
};

Model* modelJumbleSale = createModel<JumbleSale, JumbleSaleWidget>("JumbleSale");
