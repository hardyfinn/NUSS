// Behaviour tests for EFG (spec §4.14).
#include "check.hpp"
#include "../src/dsp/EfgEngine.hpp"
#include <vector>

using namespace ef;

static const float SR = 48000.f, DT = 1.f / 48000.f;

// Run n samples, return how many times each channel fired.
struct Rig {
	EfgEngine e;
	EfgParams P;
	EfgInputs I;
	int fires[CH] = {};
	std::vector<int> order;
	void run(int n) {
		for (int k = 0; k < n; k++) {
			e.process(DT, P, I);
			for (int i = 0; i < CH; i++) if (e.firedMask & (1u << i)) { fires[i]++; order.push_back(i + 1); }
		}
	}
	void gate(float& jack, int high = 48, int low = 48) { jack = 10.f; run(high); jack = 0.f; run(low); }
	void clearCounts() { for (int& f : fires) f = 0; order.clear(); }
};

static float spanFor(int ch) {  // SPAN value at the centre of channel ch's band (right of centre)
	float band = (1.f - EFG_SPAN_DEAD) / 8.f;
	return EFG_SPAN_DEAD + band * (ch - 0.5f);
}

TEST("EFG index: gate fires only the selected channel; index out 1.5 V while it runs") {
	Rig r; r.I.activateP = true; r.P.span = spanFor(3);
	r.P.rise = 0.3f; r.P.fall = 0.3f;
	r.run(10);
	r.I.activate = 10.f; r.run(10);
	CHECK(r.fires[2] == 1);
	int others = 0; for (int i = 0; i < CH; i++) if (i != 2) others += r.fires[i];
	CHECK(others == 0);
	CHECK_NEAR(r.e.indexOut, 1.5, 1e-4);
	r.I.activate = 0.f; r.run((int)(SR * 1.0f));
	CHECK_NEAR(r.e.indexOut, 0.0, 1e-4);
	r.gate(r.I.activate);
	r.gate(r.I.activate);
	CHECK(r.fires[2] == 3);
}

TEST("EFG index, activate unpatched: sweeping SPAN fires 1..8 once each") {
	Rig r;
	for (int i = 0; i <= 200; i++) { r.P.span = i / 200.f; r.run(20); }
	CHECK(r.order.size() == 8);
	for (size_t i = 0; i < r.order.size(); i++) CHECK(r.order[i] == (int) i + 1);
	// full CCW selects channel 1
	CHECK(EfgEngine::indexSel(-1.f) == 1);
	CHECK(EfgEngine::indexSel(-0.07f) == 8);
	CHECK(EfgEngine::indexSel(0.f) == 0);
}

TEST("EFG round: step 1 and step 3, reset returns to channel 1") {
	Rig r; r.P.mode = SPAN_ROUND; r.I.activateP = true;
	for (int k = 0; k < 10; k++) r.gate(r.I.activate);
	std::vector<int> want = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2};
	CHECK(r.order == want);
	Rig q; q.P.mode = SPAN_ROUND; q.I.activateP = true;
	for (int k = 0; k < 3; k++) q.gate(q.I.activate);
	q.gate(q.I.reset);
	q.gate(q.I.activate);
	CHECK(q.order.back() == 1);
	Rig s; s.P.mode = SPAN_ROUND; s.I.activateP = true;
	float band = (1.f - EFG_SPAN_DEAD) / 7.f;
	s.P.span = EFG_SPAN_DEAD + band * 2.5f;  // step 3
	CHECK(EfgEngine::roundStep(s.P.span) == 3);
	for (int k = 0; k < 8; k++) s.gate(s.I.activate);
	std::vector<int> want3 = {1, 4, 7, 2, 5, 8, 3, 6};
	CHECK(s.order == want3);
}

TEST("EFG parallel full CW: channel 1 every gate, channel 8 every 16th; reset realigns") {
	Rig r; r.P.mode = SPAN_PARALLEL; r.I.activateP = true; r.P.span = 1.f;
	r.P.rise = 0.f; r.P.fall = 0.f;
	for (int k = 0; k < 16; k++) r.gate(r.I.activate);
	CHECK(r.fires[0] == 16);
	CHECK(r.fires[7] == 1);
	CHECK(EfgEngine::parDiv(1.f, 7) == 16);
	r.gate(r.I.activate); r.gate(r.I.activate);
	r.clearCounts();
	r.gate(r.I.reset);
	r.gate(r.I.activate);
	for (int i = 0; i < CH; i++) CHECK(r.fires[i] == 1);
}

TEST("EFG binary: channel 1 every gate, 2 every other, 3 in runs of two, 4 in a run of four") {
	Rig r; r.P.mode = SPAN_BINARY; r.I.activateP = true; r.P.rise = 0.f; r.P.fall = 0.f;
	std::vector<std::vector<int>> per;
	for (int k = 0; k < 8; k++) {
		r.clearCounts();
		r.gate(r.I.activate);
		std::vector<int> f(CH); for (int i = 0; i < CH; i++) f[i] = r.fires[i];
		per.push_back(f);
	}
	int c1 = 0, c2 = 0, c3 = 0, c4 = 0;
	for (auto& f : per) { c1 += f[0]; c2 += f[1]; c3 += f[2]; c4 += f[3]; }
	CHECK(c1 == 8); CHECK(c2 == 4); CHECK(c3 == 4); CHECK(c4 == 4);
	// channel 3 pattern: 0 1 1 0 0 1 1 0 ; channel 4: 0 0 0 1 1 1 1 0
	int p3[8] = {0, 1, 1, 0, 0, 1, 1, 0}, p4[8] = {0, 0, 0, 1, 1, 1, 1, 0};
	for (int k = 0; k < 8; k++) { CHECK(per[k][2] == p3[k]); CHECK(per[k][3] == p4[k]); }
}

TEST("EFG accumulate: gates are held, then all fire together and the index flams") {
	Rig r; r.P.mode = SPAN_ROUND; r.I.activateP = true; r.I.accumP = true;
	r.P.rise = 0.5f; r.P.fall = 0.5f;
	for (int k = 0; k < 3; k++) r.gate(r.I.activate);
	CHECK(r.order.empty());
	CHECK(r.e.held(0) && r.e.held(1) && r.e.held(2));
	r.I.accum = 10.f; r.run(1);
	CHECK(r.order.size() == 3);
	// flam: 0.5 V for ~5 ms, then 1.0, then 1.5 stays
	std::vector<float> seen;
	for (int k = 0; k < (int)(SR * 0.02f); k++) {
		r.run(1);
		if (seen.empty() || std::fabs(seen.back() - r.e.indexOut) > 1e-4f) seen.push_back(r.e.indexOut);
	}
	CHECK(seen.size() >= 3);
	CHECK_NEAR(seen.back(), 1.5, 1e-4);
}

TEST("EFG curve: linear midpoint 0.5, exp below 0.3, log above 0.7") {
	for (float c : {0.5f, 1.f, 0.f}) {
		CurveLaw L; L.set(c);
		float y = L.S(0.5f);
		if (c == 0.5f) CHECK_NEAR(y, 0.5, 0.01);
		if (c == 1.f) CHECK(y < 0.3f);
		if (c == 0.f) CHECK(y > 0.7f);
		CHECK_NEAR(L.Sinv(L.S(0.3f)), 0.3, 3e-3);
	}
	// a 1 s rise with linear curve is half way at 0.5 s
	Rig r; r.I.activateP = true; r.P.span = spanFor(1);
	float p = std::log2(1.f / EFG_T_MIN) / EFG_T_OCT;
	r.P.rise = p; r.P.fall = p;
	CHECK_NEAR(efgTime(p), 1.0, 1e-3);
	r.I.activate = 10.f; r.run(1);
	r.run((int)(SR * 0.5f) - 1);
	CHECK_NEAR(r.e.ch[0].y, 0.5, 0.01);
}

TEST("EFG cycle all and follow the leader") {
	Rig r; r.P.cycle = true; r.P.rise = 0.2f; r.P.fall = 0.2f;
	r.run(1);
	for (int i = 0; i < CH; i++) CHECK(r.fires[i] == 1);
	r.run((int)(SR * 0.1f));
	for (int i = 0; i < CH; i++) CHECK(r.fires[i] > 2);

	Rig f; f.P.cycle = true; f.P.cycleType = CYCLE_FOLLOW; f.P.rise = 0.15f; f.P.fall = 0.15f;
	f.I.activateP = true; f.P.span = spanFor(3);
	f.run(100);
	CHECK(f.order.empty());  // a chain has to be started
	f.gate(f.I.activate, 1, 1);
	f.run((int)(SR * 0.2f));
	CHECK(f.order.size() >= 9);
	std::vector<int> want = {3, 4, 5, 6, 7, 8, 1, 2, 3};
	for (int k = 0; k < 9 && k < (int) f.order.size(); k++) CHECK(f.order[k] == want[k]);
}

TEST("EFG strength and peak voltage") {
	for (float S : {1.f, -1.f, 0.f}) {
		Rig r; r.I.activateP = true; r.P.span = spanFor(2); r.P.strength = S; r.P.rise = 0.3f; r.P.fall = 0.6f;
		r.I.activate = 10.f;
		float mx = -100, mn = 100;
		for (int k = 0; k < (int)(SR * 0.05f); k++) { r.run(1); mx = std::max(mx, r.e.out[1]); mn = std::min(mn, r.e.out[1]); }
		if (S == 1.f) CHECK_NEAR(mx, 9.0, 0.05);
		if (S == -1.f) CHECK_NEAR(mn, -9.0, 0.05);
		if (S == 0.f) { CHECK_NEAR(mx, 0.0, 1e-6); CHECK_NEAR(mn, 0.0, 1e-6); }
	}
}

TEST("EFG oscillation: unipolar stays inside the envelope, bipolar swings both ways") {
	for (bool bi : {false, true}) {
		Rig r; r.I.activateP = true; r.P.span = spanFor(1); r.P.osc = 1.f; r.P.bipolar = bi;
		r.P.rate = 0.6f; r.P.rise = 0.5f; r.P.fall = 0.7f;
		r.I.activate = 10.f;
		float mx = -100, mn = 100; bool inside = true;
		for (int k = 0; k < (int)(SR * 0.5f); k++) {
			r.run(1);
			float v = r.e.out[0], env = 9.f * r.e.ch[0].y;
			mx = std::max(mx, v); mn = std::min(mn, v);
			if (std::fabs(v) > env + 1e-3f) inside = false;
		}
		CHECK(inside);
		if (!bi) CHECK(mn >= -1e-4f);
		else { CHECK(mn < -1.f); CHECK(mx > 1.f); }
	}
}

TEST("EFG rate CV: 1 V at full attenuverter doubles the oscillation") {
	auto measure = [](float cv) {
		Rig r; r.I.activateP = true; r.P.span = spanFor(1); r.P.rate = 0.5f; r.P.rise = 1.f; r.P.fall = 1.f;
		r.P.rateAtt = 1.f; r.I.rateP = true; r.I.rateCv = cv;
		r.I.activate = 10.f; r.run(1);
		float th0 = r.e.ch[0].theta; int wraps = 0; float prev = th0;
		int n = (int)(SR * 10.f);
		for (int k = 0; k < n; k++) { r.run(1); if (r.e.ch[0].theta < prev) wraps++; prev = r.e.ch[0].theta; }
		return wraps;
	};
	int a = measure(0.f), b = measure(1.f);
	CHECK(a > 5);
	CHECK_NEAR((double) b / a, 2.0, 0.05);
}

TEST("EFG spread: weights the attenuverter by channel; zero spread does nothing") {
	Rig r; r.P.spread = 1.f; r.P.riseAtt = 1.f;
	r.run(1);
	CHECK_NEAR(r.e.weight[0], 0.0, 1e-6);
	CHECK_NEAR(r.e.weight[7], 1.0, 1e-6);
	r.P.spread = -1.f; r.run(1);
	CHECK_NEAR(r.e.weight[0], 1.0, 1e-6);
	CHECK_NEAR(r.e.weight[7], 0.0, 1e-6);
	r.P.spread = 0.f; r.run(1);
	for (int i = 0; i < CH; i++) CHECK_NEAR(r.e.weight[i], 0.0, 1e-6);
	// with spread +1 channel 8 takes longer to rise than channel 1
	Rig q; q.P.spread = 1.f; q.P.riseAtt = 0.3f; q.P.rise = 0.4f; q.P.mode = SPAN_PARALLEL; q.I.activateP = true;
	q.I.activate = 10.f; q.run(1);
	q.run((int)(SR * 0.01f));
	CHECK(q.e.ch[0].y > q.e.ch[7].y);
}

TEST("EFG dissemination: each channel keeps the CV it saw at activation") {
	Rig r; r.P.mode = SPAN_ROUND; r.I.activateP = true; r.I.riseP = true; r.P.riseAtt = 1.f; r.P.rise = 0.3f;
	r.I.riseCv = 0.f; r.gate(r.I.activate);
	r.I.riseCv = 2.5f; r.gate(r.I.activate);
	r.I.riseCv = -1.f; r.run(10);
	CHECK_NEAR(r.e.ch[0].dRise, 0.0, 1e-6);
	CHECK_NEAR(r.e.ch[1].dRise, 0.5, 1e-6);
}

TEST("EFG submix: OUT 4 carries channels 1-4 when only OUT 4 and OUT 8 are patched") {
	Rig r; r.P.submix = true; r.I.outP[3] = true; r.I.outP[7] = true;
	r.P.mode = SPAN_PARALLEL; r.I.activateP = true; r.P.rise = 0.4f; r.P.fall = 0.6f; r.P.strength = 0.5f;
	r.I.activate = 10.f; r.run((int)(SR * 0.005f));
	float sum = 0; for (int i = 0; i < 4; i++) sum += r.e.ch[i].out;
	CHECK_NEAR(r.e.out[3], EfgEngine::fold10(sum), 1e-4);
	CHECK(sum > 1.f);
}

TEST("EFG strength past full with submix off adds DC: pure +9 V at S = 2") {
	Rig r; r.P.strengthAtt = 1.f; r.I.strengthP = true; r.I.strengthCv = 5.f;  // S = 1 + 1 = 2
	r.I.activateP = true; r.P.span = spanFor(1);
	r.I.activate = 10.f; r.run((int)(SR * 0.01f));
	CHECK_NEAR(r.e.out[0], 9.0, 1e-3);
}

TEST("EFG index receive: 0.5, 1.0, 1.5 V fire channels 1, 2, 3") {
	Rig r; r.P.span = 0.f; r.P.spanAtt = 1.f; r.I.spanCvP = true;
	for (float v : {0.5f, 1.0f, 1.5f}) { r.I.spanCv = v; r.run(10); }
	CHECK(r.e.indexReceive);
	std::vector<int> want = {1, 2, 3};
	CHECK(r.order == want);
}
