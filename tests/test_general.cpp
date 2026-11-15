// General tests (spec §9): stability under random input, sample-rate independence, CPU.
#include "check.hpp"
#include "../src/dsp/EfgEngine.hpp"
#include "../src/dsp/EtsEngine.hpp"
#include "../src/dsp/EgxEngine.hpp"
#include <random>
#include <memory>
#include <chrono>

using namespace ef;

TEST("Stability: random CV at audio rate on every EFG input") {
	std::mt19937 rng(3);
	std::uniform_real_distribution<float> U(-10.f, 10.f), K(-1.f, 1.f), Z(0.f, 1.f);
	for (float sr : {44100.f, 96000.f}) {
		EfgEngine e; EfgParams P; EfgInputs I;
		I.activateP = I.accumP = I.spanCvP = I.spreadCvP = I.riseP = I.fallP = I.rateP = I.oscP = I.strengthP = true;
		for (int i = 0; i < CH; i++) I.outP[i] = i % 3 == 0;
		bool ok = true;
		int n = (int)(sr * 20.f);
		for (int k = 0; k < n; k++) {
			if (k % 4800 == 0) {
				P.span = K(rng); P.spanAtt = K(rng); P.curve = Z(rng); P.rise = Z(rng); P.fall = Z(rng); P.shape = K(rng);
				P.strength = K(rng); P.rate = Z(rng); P.osc = Z(rng); P.spread = K(rng);
				P.riseAtt = K(rng); P.fallAtt = K(rng); P.oscAtt = K(rng); P.strengthAtt = K(rng); P.rateAtt = K(rng);
				P.mode = (int)(Z(rng) * 3.99f); P.cycle = Z(rng) > 0.5f; P.cycleType = Z(rng) > 0.5f; P.bipolar = Z(rng) > 0.5f; P.submix = Z(rng) > 0.5f;
			}
			I.activate = U(rng); I.accum = U(rng); I.reset = U(rng); I.cycleIn = U(rng);
			I.spanCv = U(rng); I.curveCv = U(rng); I.shapeCv = U(rng); I.spreadCv = U(rng);
			I.riseCv = U(rng); I.fallCv = U(rng); I.rateCv = U(rng); I.oscCv = U(rng); I.strengthCv = U(rng);
			e.process(1.f / sr, P, I);
			for (int i = 0; i < CH; i++) if (!std::isfinite(e.out[i]) || std::fabs(e.out[i]) > 10.f) ok = false;
			if (!std::isfinite(e.indexOut)) ok = false;
			if (!ok) break;
		}
		CHECK(ok);
	}
}

TEST("Stability: random CV at audio rate on every ETS input") {
	std::mt19937 rng(5);
	std::uniform_real_distribution<float> U(-10.f, 10.f), K(-1.f, 1.f), Z(0.f, 1.f);
	auto e = std::make_unique<EtsEngine>();
	EtsParams P; EtsInputs I;
	I.inP = I.holdInP = I.tempoP = I.timeCvP = I.phaseCvP = true;
	bool ok = true;
	for (int k = 0; k < 48000 * 20; k++) {
		if (k % 4800 == 0) {
			P.time = Z(rng); P.phase = Z(rng); P.spread = K(rng); P.spreadAtt = K(rng);
			P.shape = (int)(Z(rng) * 6.99f); P.range = (int)(Z(rng) * 2.99f); P.hold = Z(rng) > 0.8f;
			I.inP = Z(rng) > 0.3f; I.tempoP = Z(rng) > 0.5f;
		}
		I.in = U(rng); I.holdIn = U(rng); I.resetIn = U(rng); I.tempo = U(rng);
		I.timeCv = U(rng); I.phaseCv = U(rng); I.spreadCv = U(rng);
		e->process(1.f / 48000.f, P, I);
		for (int i = 0; i < CH; i++) if (!std::isfinite(e->out[i]) || std::fabs(e->out[i]) > 10.f) ok = false;
		if (!ok) break;
	}
	CHECK(ok);
}

TEST("Stability: random CV on every EGX input") {
	std::mt19937 rng(9);
	std::uniform_real_distribution<float> U(-10.f, 10.f), K(-1.f, 1.f), Z(0.f, 1.f);
	EgxEngine e; EgxParams P; EgxInputs I; I.spanCvP = true;
	bool ok = true;
	for (int k = 0; k < 48000 * 10; k++) {
		if (k % 4800 == 0) { P.span = K(rng); P.atten = K(rng); P.space = Z(rng); I.clockP = Z(rng) > 0.5f; }
		I.spanCv = U(rng); I.spaceCv = U(rng); I.clock = U(rng);
		e.process(1.f / 48000.f, P, I);
		if (e.active < 0 || e.active > 8 || !std::isfinite(e.indexOut)) ok = false;
	}
	CHECK(ok);
}

TEST("Sample rate: EFG rise time and ETS buffer time hold at 44.1 and 96 kHz") {
	for (float sr : {44100.f, 96000.f}) {
		EfgEngine e; EfgParams P; EfgInputs I; I.activateP = true; P.span = 0.1f;
		float p = std::log2(0.1f / EFG_T_MIN) / EFG_T_OCT;  // 100 ms rise
		P.rise = p; P.fall = p;
		I.activate = 10.f;
		int n = 0;
		do { e.process(1.f / sr, P, I); n++; } while (e.ch[0].st == ST_RISE && n < (int) sr);
		CHECK_NEAR(n / sr, 0.1, 0.001);
	}
	for (float sr : {44100.f, 96000.f}) {
		auto e = std::make_unique<EtsEngine>(); EtsParams P; EtsInputs I;
		P.time = std::log(2.f / ETS_T_MIN) / std::log(ETS_T_RATIO);  // 2 s
		int clocks = 0; bool prev = false;
		for (int k = 0; k < (int)(sr * 11.f); k++) { e->process(1.f / sr, P, I); if (e->clkOut && !prev) clocks++; prev = e->clkOut; }
		CHECK(clocks == 5);
	}
}

// Shared CI runners and cloud VMs are slow and noisy, so this only catches gross regressions
// (2% of a core). The spec's budget (0.5%) is for Rack's own meter on the Mac.
TEST("CPU: each engine well under budget on the host") {
	auto bench = [](auto&& fn) {
		auto t0 = std::chrono::steady_clock::now();
		fn();
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	};
	const int n = 48000 * 5;
	EfgEngine f; EfgParams FP; EfgInputs FI; FP.cycle = true; FP.osc = 0.5f; FP.spread = 0.5f;
	double tf = bench([&] { for (int k = 0; k < n; k++) f.process(1.f / 48000.f, FP, FI); });
	auto s = std::make_unique<EtsEngine>(); EtsParams SP; EtsInputs SI; SI.inP = true; SP.spread = 0.6f; SP.phase = 0.5f;
	double ts = bench([&] { for (int k = 0; k < n; k++) { SI.in = (k % 100) * 0.05f; s->process(1.f / 48000.f, SP, SI); } });
	EgxEngine g; EgxParams GP; EgxInputs GI; GI.spanCvP = true;
	double tg = bench([&] { for (int k = 0; k < n; k++) { GI.spanCv = (k % 1000) * 0.01f; g.process(1.f / 48000.f, GP, GI); } });
	INFO("  EFG %.2f%%  ETS %.2f%%  EGX %.2f%% of one core\n", tf / 5.0 * 100, ts / 5.0 * 100, tg / 5.0 * 100);
	CHECK(tf / 5.0 < 0.02);
	CHECK(ts / 5.0 < 0.02);
	CHECK(tg / 5.0 < 0.005);
}
