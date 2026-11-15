// Behaviour tests for ETS (spec §5.12).
#include "check.hpp"
#include "../src/dsp/EtsEngine.hpp"
#include <memory>
#include <vector>

using namespace ef;

static const float SR = 48000.f, DT = 1.f / 48000.f;

static float tFor(float seconds) { return std::log(seconds / ETS_T_MIN) / std::log(ETS_T_RATIO); }

struct ERig {
	std::unique_ptr<EtsEngine> e{new EtsEngine()};
	EtsParams P;
	EtsInputs I;
	double t = 0;
	void run(int n) { for (int k = 0; k < n; k++) { e->process(DT, P, I); t += DT; } }
};

TEST("ETS mult: spread centre, phase 0, ramp read gives eight copies of the input") {
	ERig r; r.I.inP = true; r.P.time = tFor(1.f);
	float worst = 0;
	for (int k = 0; k < (int)(SR * 3.f); k++) {
		r.I.in = 5.f * std::sin(2.f * PI * 0.5f * (float) r.t);
		r.run(1);
		if (r.t > 0.1)
			for (int i = 0; i < CH; i++) worst = std::max(worst, std::fabs(r.e->out[i] - r.I.in));
	}
	CHECK(worst < 0.1f);  // within 1% of 10 V
}

TEST("ETS phase 1: channel 8 is half the buffer behind channel 1") {
	ERig r; r.I.inP = true; r.P.time = tFor(1.f); r.P.phase = 1.f;
	r.I.in = 0.f; r.run((int)(SR * 1.2f));
	r.I.in = 5.f;
	double t0 = r.t, t1 = -1, t8 = -1;
	for (int k = 0; k < (int)(SR * 1.f); k++) {
		r.run(1);
		if (t1 < 0 && r.e->out[0] > 2.5f) t1 = r.t - t0;
		if (t8 < 0 && r.e->out[7] > 2.5f) t8 = r.t - t0;
	}
	CHECK(t1 >= 0 && t1 < 0.01);
	CHECK_NEAR(t8 - t1, 0.5, 0.01);
}

TEST("ETS spread +1: speeds x8 to /8 and LED colour bands") {
	CHECK_NEAR(EtsEngine::multFor(1.f, 0), 8.0, 1e-4);
	CHECK_NEAR(EtsEngine::multFor(1.f, 7), 0.125, 1e-5);
	CHECK_NEAR(EtsEngine::multFor(-1.f, 7), 8.0, 1e-4);
	CHECK_NEAR(EtsEngine::multFor(0.f, 3), 1.0, 1e-6);
	CHECK(EtsEngine::multFor(1.f, 3) < EtsEngine::multFor(1.f, 2));
	CHECK_NEAR(EtsEngine::multFor(ETS_SPREAD_MAX, 0), 24.0, 0.1);
}

TEST("ETS saw read reverses the material") {
	ERig r; r.I.inP = true; r.P.time = tFor(1.f); r.P.shape = RD_SAW;
	// write a rising ramp over exactly one buffer length
	for (int k = 0; k < (int) SR; k++) { r.I.in = 10.f * k / SR - 5.f; r.run(1); }
	r.P.hold = true;
	int falls = 0, rises = 0; float prev = r.e->out[0];
	for (int k = 0; k < (int)(SR * 0.5f); k++) {
		r.run(1);
		float v = r.e->out[0];
		if (v < prev - 1e-5f) falls++; else if (v > prev + 1e-5f) rises++;
		prev = v;
	}
	CHECK(falls > rises * 10);
}

TEST("ETS hold: outputs stop following the input and the loop repeats with period T") {
	ERig r; r.I.inP = true; r.P.time = tFor(0.5f);
	for (int k = 0; k < (int)(SR * 0.6f); k++) { r.I.in = 5.f * std::sin(2.f * PI * 3.f * (float) r.t); r.run(1); }
	r.P.hold = true; r.run(1);
	std::vector<float> a;
	for (int k = 0; k < (int)(SR * 0.5f); k++) { r.I.in = 9.f; r.run(1); a.push_back(r.e->out[0]); }
	float diff = 0, mx = 0;
	for (int k = 0; k < (int)(SR * 0.5f); k++) {
		r.I.in = -9.f; r.run(1);
		// skip the seam where the newest material meets the oldest
		if (k > 20 && k < (int)(SR * 0.5f) - 20) diff = std::max(diff, std::fabs(r.e->out[0] - a[k]));
		mx = std::max(mx, std::fabs(r.e->out[0]));
	}
	CHECK(diff < 0.05f);
	CHECK(mx < 5.5f);   // never shows the new ±9 V input
}

TEST("ETS reset realigns the heads") {
	ERig r; r.I.inP = true; r.P.time = tFor(1.f); r.P.spread = 0.7f;
	for (int k = 0; k < (int)(SR * 2.f); k++) { r.I.in = 5.f * std::sin(2.f * PI * 1.3f * (float) r.t); r.run(1); }
	r.I.resetButton = true; r.run(1); r.I.resetButton = false;
	float base = r.e->psi[0];
	for (int i = 1; i < CH; i++) CHECK_NEAR(r.e->psi[i], base, 1e-3);
}

TEST("ETS internal LFO: eight ramps of period T, channel 8 half a cycle behind") {
	ERig r; r.P.time = tFor(1.f); r.P.phase = 1.f; r.P.range = RANGE_0_10;
	r.I.resetButton = true; r.run(1); r.I.resetButton = false;
	float lo = 100, hi = -100;
	for (int k = 0; k < (int)(SR * 0.25f); k++) {
		r.run(1);
		for (int i = 0; i < CH; i++) { lo = std::min(lo, r.e->out[i]); hi = std::max(hi, r.e->out[i]); }
	}
	CHECK(lo >= 0.f); CHECK(hi <= 10.f);
	float d = r.e->out[0] - r.e->out[7];
	if (d < 0) d += 10.f;
	CHECK_NEAR(std::fabs(d), 5.0, 0.05);
}

TEST("ETS internal LFO: stairs is a square, step random holds one value per cycle") {
	ERig r; r.P.time = tFor(0.2f); r.P.shape = RD_STAIRS;
	bool onlyTwo = true;
	for (int k = 0; k < (int)(SR * 0.5f); k++) {
		r.run(1);
		float v = r.e->out[0];
		if (std::fabs(v) > 1e-4f && std::fabs(v - 10.f) > 1e-4f) onlyTwo = false;
	}
	CHECK(onlyTwo);
	ERig s; s.P.time = tFor(0.2f); s.P.shape = RD_STEPRND;
	int changes = 0; float prev = s.e->out[0];
	for (int k = 0; k < (int)(SR * 1.0f); k++) { s.run(1); if (s.e->out[0] != prev) changes++; prev = s.e->out[0]; }
	CHECK(changes >= 4 && changes <= 6);
}

TEST("ETS tempo: 2 Hz clock, time full CW gives T = 16 s and a green clock") {
	ERig r; r.I.tempoP = true; r.P.time = 1.f;
	int clocks = 0; bool prev = false;
	for (int k = 0; k < (int)(SR * 40.f); k++) {
		double ph = std::fmod(r.t, 0.5);
		r.I.tempo = ph < 0.01 ? 10.f : 0.f;
		r.run(1);
		if (r.t > 4.0 && r.e->clkOut && !prev) clocks++;
		prev = r.e->clkOut;
	}
	CHECK_NEAR(r.e->T, 16.0, 0.01);
	CHECK(r.e->tempoN == 32);
	CHECK(r.e->clockColour() == CLK_GREEN);
	CHECK(clocks >= 2 && clocks <= 3);
}

TEST("ETS channel index: points at the highest output") {
	ERig r; r.I.inP = true; r.P.time = tFor(1.f);
	r.I.in = 1.f; r.run((int)(SR * 1.2f));
	r.I.in = 3.f; r.run((int)(SR * 0.01f));
	// with spread 0 all are equal; push spread so channel 1 reads newest material
	r.P.spread = 0.f; r.P.phase = 1.f; r.run((int)(SR * 0.05f));
	CHECK_NEAR(r.e->indexOut, 0.5, 1e-4);   // channel 1 has the new, higher value
}

TEST("ETS time CV: 0 V to 5 V with the knob at 0 spans 50 ms to 30 s") {
	CHECK_NEAR(EtsEngine::timeFor(0.f), 0.05, 1e-6);
	CHECK_NEAR(EtsEngine::timeFor(1.f), 30.0, 1e-3);
	ERig r; r.P.time = 0.f; r.I.timeCvP = true; r.I.timeCv = 5.f; r.run(10);
	CHECK_NEAR(r.e->T, 30.0, 1e-3);
	r.I.timeCv = 0.f; r.run(10);
	CHECK_NEAR(r.e->T, 0.05, 1e-6);
}
