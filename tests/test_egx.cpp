// Behaviour tests for EGX (spec §6.8).
#include "check.hpp"
#include "../src/dsp/EgxEngine.hpp"
#include "../src/dsp/EfgEngine.hpp"
#include <vector>

using namespace ef;

static const float SR = 48000.f, DT = 1.f / 48000.f;

struct GRig {
	EgxEngine e;
	EgxParams P;
	EgxInputs I;
	std::vector<int> seq;   // committed channel after each pulse
	void run(int n) {
		for (int k = 0; k < n; k++) {
			int before = e.pulses;
			e.process(DT, P, I);
			if (e.pulses != before) seq.push_back(e.active);
		}
	}
	void at(float v, int n = 10) { I.spanCv = v; I.spanCvP = true; run(n); }
};

TEST("EGX bands with SPACE full: channel, odd/even, out of range") {
	GRig r;
	r.at(0.6f); CHECK(r.e.active == 1); CHECK(r.e.odd(r.P) == 10.f); CHECK(r.e.even(r.P) == 0.f);
	r.at(1.9f); CHECK(r.e.active == 2); CHECK(r.e.even(r.P) == 10.f); CHECK(r.e.gate(1, r.P) == 10.f); CHECK(r.e.gate(0, r.P) == 0.f);
	r.at(9.9f); CHECK(r.e.active == 8);
	r.at(10.1f); CHECK(r.e.active == 0); CHECK(r.e.odd(r.P) == 0.f); CHECK(r.e.even(r.P) == 0.f);
	r.at(5.f); r.at(-0.1f); CHECK(r.e.active == 0);
}

TEST("EGX ramp 0..10 V over 1 s: eight pulses in order, index steps 0.5..4 V") {
	GRig r;
	std::vector<float> idx;
	for (int k = 0; k < (int) SR; k++) {
		r.at(10.f * k / SR, 1);
		if (idx.empty() || std::fabs(idx.back() - r.e.indexOut) > 1e-4f) idx.push_back(r.e.indexOut);
	}
	CHECK(r.seq.size() == 8);
	for (size_t i = 0; i < r.seq.size(); i++) CHECK(r.seq[i] == (int) i + 1);
	CHECK_NEAR(idx.back(), 4.0, 1e-4);
}

TEST("EGX space 0.5: all eight bands within about 0.71 V") {
	GRig r; r.P.space = 0.5f;
	CHECK_NEAR(EgxEngine::rangeFor(0.5f), 0.707, 0.002);
	double t8 = -1;
	for (int k = 0; k < (int) SR; k++) {
		r.at(10.f * k / SR, 1);
		if (t8 < 0 && r.e.active == 8) t8 = (double) k / SR;
	}
	CHECK(t8 > 0 && t8 < 0.075);
	CHECK(r.e.active == 0);
}

TEST("EGX hysteresis: a noisy signal on a band edge gives no extra pulses") {
	GRig r;
	r.at(2.5f - 0.2f, 100);
	int before = r.e.pulses;
	for (int k = 0; k < 10000; k++) r.at(2.5f + ((k % 2) ? 0.001f : -0.001f), 1);
	CHECK(r.e.pulses - before <= 1);
}

TEST("EGX clock: gates change only on clock edges") {
	GRig r; r.I.clockP = true;
	int changes = 0, prev = 0, badChanges = 0; float prevClk = 0.f;
	double t = 0;
	for (int k = 0; k < (int) SR; k++) {
		t += DT;
		float clk = std::fmod(t, 0.25) < 0.005 ? 10.f : 0.f;
		r.I.clock = clk;
		bool edge = clk > 5.f && prevClk < 5.f;
		prevClk = clk;
		r.at(5.f + 4.9f * std::sin(2.f * PI * 7.f * (float) t), 1);
		if (r.e.active != prev) { changes++; if (!edge) badChanges++; }
		prev = r.e.active;
	}
	CHECK(changes >= 1);
	CHECK(badChanges == 0);
}

TEST("EGX attenuverter -1 steps downward as the input rises") {
	GRig r; r.P.atten = -1.f; r.P.span = 2.f;  // offset +10 V
	for (int k = 0; k < (int) SR; k++) r.at(10.f * k / SR, 1);
	CHECK(r.seq.size() >= 7);
	for (size_t i = 1; i < r.seq.size(); i++) CHECK(r.seq[i] < r.seq[i - 1]);
}

TEST("EGX index mode: 0.5/1.0/1.5 V give channels 1/2/3; a flam gives three pulses ending on 1") {
	GRig r; r.P.span = -1.f; r.P.atten = 1.f; r.P.space = 1.f;
	r.at(0.f);
	CHECK(r.e.indexMode);
	r.at(0.5f); r.at(1.0f); r.at(1.5f);
	std::vector<int> want = {1, 2, 3};
	CHECK(r.seq == want);
	r.at(0.f, 100);
	r.seq.clear();
	int n = (int)(SR * 0.005f);
	r.at(1.5f, n); r.at(1.0f, n); r.at(0.5f, n);
	std::vector<int> flam = {3, 2, 1};
	CHECK(r.seq == flam);
	CHECK(r.e.active == 1);
}

TEST("EGX follows EFG activations through the channel index") {
	EfgEngine f; EfgParams FP; EfgInputs FI;
	FP.mode = SPAN_ROUND; FI.activateP = true; FP.rise = 0.6f; FP.fall = 0.8f;
	GRig r; r.P.span = -1.f; r.P.atten = 1.f; r.P.space = 1.f;
	std::vector<int> fired;
	for (int g = 0; g < 4; g++) {
		for (int k = 0; k < 2400; k++) {
			FI.activate = k < 48 ? 10.f : 0.f;
			f.process(DT, FP, FI);
			for (int i = 0; i < CH; i++) if (f.firedMask & (1u << i)) fired.push_back(i + 1);
			r.at(f.indexOut, 1);
		}
	}
	CHECK(fired.size() == 4);
	CHECK(r.seq == fired);
}
