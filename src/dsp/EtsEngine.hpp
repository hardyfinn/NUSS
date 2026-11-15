// ETS engine: one input written into a circular buffer, eight read heads at their own
// speed and phase (spec §5). Plain C++17, no Rack includes.
#pragma once
#include <vector>
#include "Common.hpp"
#include "ChannelIndex.hpp"

namespace ef {

enum ReadShape { RD_RAMP = 0, RD_SAW, RD_TRI, RD_SINE, RD_STAIRS, RD_STEPRND, RD_RAMPLETS, RD_SHAPES };
enum LfoRange { RANGE_0_10 = 0, RANGE_PM5, RANGE_0_5, RANGE_COUNT };
enum ClockColour { CLK_RED = 0, CLK_ORANGE, CLK_YELLOW, CLK_GREEN, CLK_PURPLE, CLK_BLUE, CLK_GOLD };

struct EtsParams {
	float time = 0.4f;
	float phase = 0.f;
	float spread = 0.f, spreadAtt = 0.f;
	int shape = RD_RAMP;
	int range = RANGE_0_10;
	bool hold = false;
	float indexV = INDEX_V_DEFAULT;
};

struct EtsInputs {
	float in = 0.f; bool inP = false;
	float holdIn = 0.f; bool holdInP = false;
	float resetIn = 0.f;
	bool resetButton = false;  // edge, from the panel button
	float tempo = 0.f; bool tempoP = false;
	float timeCv = 0.f; bool timeCvP = false;
	float phaseCv = 0.f; bool phaseCvP = false;
	float spreadCv = 0.f;
};

struct EtsEngine {
	std::vector<float> buf;
	float wpos = 0.f;          // write position in slots
	float acc = 0.f; int accN = 0; float prevIn = 0.f;
	float psi[CH] = {};        // read phasors
	float prevU[CH] = {};
	float rndAddr[CH] = {};    // STEP RANDOM / RAMPLETS jump targets
	float rndHeld[CH] = {};    // STEP RANDOM held value
	float lfoA[CH] = {}, lfoB[CH] = {};  // random LFO targets
	Rng rng[CH];
	bool holding = false;
	float holdPos = 0.f;
	Schmitt holdS, rstS, tempoS;
	// tempo
	float sinceEdge = 0.f, period = 0.f, lastInterval = 0.f;
	bool tempoActive = false;
	int edgeCount = 0;
	// clock
	float clkPhase = 0.f;
	Pulse clk, flash, resetFlash;
	// index
	ChannelIndexOut idxOut;
	int shown = -1, cand = -1;
	float candT = 0.f;
	// results
	float out[CH] = {};
	float mult[CH] = {};
	float indexOut = 0.f;
	bool clkOut = false;
	float T = 1.f;
	int multTick = 0; float lastS = 1e9f; bool lastTempo = false;
	int tempoN = 1;

	EtsEngine() : buf(ETS_N, 0.f) {
		for (int i = 0; i < CH; i++) { rng[i].seed_(101 + 7919 * i); mult[i] = 1.f; }
	}

	void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
	void reset() {
		clear();
		wpos = 0.f; acc = 0.f; accN = 0; prevIn = 0.f;
		for (int i = 0; i < CH; i++) { psi[i] = 0.f; prevU[i] = 0.f; out[i] = 0.f; }
		holding = false; tempoActive = false; edgeCount = 0; sinceEdge = 0.f; period = 0.f;
		clkPhase = 0.f; idxOut.reset(); shown = cand = -1;
	}

	static float timeFor(float t) {
		return clampf(ETS_T_MIN * std::pow(ETS_T_RATIO, t), ETS_T_FLOOR, ETS_T_CEIL);
	}
	static float multFor(float s, int i) {
		return std::exp2(ETS_SPREAD_OCT * s * (3.5f - i) / 3.5f);
	}
	static float snapMult(float m) {
		if (m >= 1.f) return std::max(1.f, std::round(m));
		return 1.f / std::max(1.f, std::round(1.f / m));
	}
	static float shapeG(int shape, float u) {
		switch (shape) {
			case RD_SAW: return 1.f - u;
			case RD_TRI: return u < 0.5f ? 2.f * u : 2.f - 2.f * u;
			case RD_SINE: return 0.5f - 0.5f * std::cos(2.f * PI * u);
			case RD_STAIRS: return std::floor(u * ETS_STAIRS) / ETS_STAIRS;
			default: return u;
		}
	}
	float readAt(float a) const {
		if (a < 0.f || a >= (float) ETS_N) a -= ETS_N * std::floor(a / ETS_N);
		int i0 = (int) a;
		if (i0 >= ETS_N) i0 = 0;
		int i1 = i0 + 1 == ETS_N ? 0 : i0 + 1;
		float t = a - i0;
		return buf[i0] + (buf[i1] - buf[i0]) * t;
	}
	int clockColour() const {
		if (tempoActive) {
			int n = tempoN;
			if ((n & (n - 1)) == 0) return CLK_GREEN;
			if (n % 3 == 0) return CLK_RED;
			return CLK_GOLD;
		}
		if (T < ETS_T_MIN * 0.999f) return CLK_PURPLE;
		if (T > ETS_T_MIN * ETS_T_RATIO * 1.001f) return CLK_BLUE;
		if (T < 0.2f) return CLK_RED;
		if (T < 1.5f) return CLK_ORANGE;
		if (T < 8.f) return CLK_YELLOW;
		return CLK_GREEN;
	}

	void doReset(bool lfo) {
		float base = holding ? holdPos : wpos / ETS_N;
		for (int i = 0; i < CH; i++) psi[i] = lfo ? 0.f : base;
		clkPhase = 0.f;
		resetFlash.fire(ETS_FLASH_S);
	}

	void process(float dt, const EtsParams& P, const EtsInputs& I) {
		idxOut.vPer = P.indexV;
		bool lfo = !I.inP;

		// ---- hold (spec §5.6) ----
		holdS.process(I.holdIn);
		bool h = P.hold || (I.holdInP && holdS.high);
		if (h && !holding) holdPos = wpos / ETS_N;
		holding = h;

		// ---- tempo (spec §5.8) ----
		sinceEdge += dt;
		bool tEdge = I.tempoP && tempoS.process(I.tempo);
		if (tEdge) {
			if (sinceEdge > 1e-4f) {
				float iv = sinceEdge;
				period = lastInterval > 0.f ? 0.5f * (iv + lastInterval) : iv;
				lastInterval = iv;
				if (edgeCount > 0) tempoActive = true;
			}
			sinceEdge = 0.f;
			edgeCount++;
		}
		if (!I.tempoP || (tempoActive && sinceEdge > 4.f * period)) { tempoActive = false; lastInterval = 0.f; if (!I.tempoP) edgeCount = 0; }

		// ---- time ----
		float t = P.time + (I.timeCvP ? I.timeCv / 5.f : 0.f);
		t = clampf(t, -1.f, 1.5f);
		if (tempoActive) {
			tempoN = 1 + (int) std::floor(clampf(t, 0.f, 1.f) * (ETS_TEMPO_MAXN - 0.001f));
			T = period * tempoN;
		} else {
			T = timeFor(t);
		}
		T = clampf(T, ETS_T_FLOOR, ETS_T_CEIL * 4.f);

		// ---- reset ----
		bool rEdge = rstS.process(I.resetIn) || I.resetButton;
		if (rEdge) doReset(lfo);

		// ---- tempo resync: every n edges the write head returns to 0, heads keep their relation ----
		if (tEdge && tempoActive && edgeCount % tempoN == 0 && !holding) {
			float delta = -wpos / ETS_N;
			if (delta < -0.5f) delta += 1.f;
			wpos = 0.f;
			for (int i = 0; i < CH; i++) psi[i] += delta;
			clkPhase = 0.f;
			clk.fire(ETS_TRIG_S);
			flash.fire(ETS_FLASH_S);
		}

		// ---- write (spec §5.3) ----
		float x = clampf(I.in, -10.f, 10.f);
		float step = ETS_N * dt / T;
		if (!lfo && !holding) {
			acc += x; accN++;
			float a = wpos, b = wpos + step;
			long k0 = (long) std::floor(a) + 1, k1 = (long) std::floor(b);
			if (k1 >= k0) {
				long n = std::min<long>(k1 - k0 + 1, ETS_N);
				if (n == 1) buf[((k1 % ETS_N) + ETS_N) % ETS_N] = acc / accN;
				else {
					for (long k = k1 - n + 1; k <= k1; k++) {
						float fr = (k - a) / (b - a);
						buf[((k % ETS_N) + ETS_N) % ETS_N] = lerpf(prevIn, x, fr);
					}
				}
				acc = 0.f; accN = 0;
			}
			wpos = b;
			if (wpos >= ETS_N) wpos -= ETS_N * std::floor(wpos / ETS_N);
		}
		prevIn = x;

		// ---- clock out (spec §5.9) ----
		clkPhase += dt / T;
		if (clkPhase >= 1.f) {
			clkPhase -= std::floor(clkPhase);
			if (!tempoActive) { clk.fire(ETS_TRIG_S); flash.fire(ETS_FLASH_S); }
		}
		clkOut = clk.process(dt);
		flash.process(dt);
		resetFlash.process(dt);

		// ---- spread and phase (spec §5.4, §5.5) ----
		float s = clampf(P.spread + P.spreadAtt * I.spreadCv / 5.f, -ETS_SPREAD_MAX, ETS_SPREAD_MAX);
		float p = I.phaseCvP ? P.phase * I.phaseCv / 5.f : P.phase;
		p = clampf(p, -2.f, 2.f);

		// ---- read (spec §5.5, §5.5a, §5.7) ----
		bool freeze = lfo && holding;
		if (++multTick >= 16 || s != lastS || tempoActive != lastTempo) {
			multTick = 0; lastS = s; lastTempo = tempoActive;
			for (int i = 0; i < CH; i++) mult[i] = tempoActive ? snapMult(multFor(s, i)) : multFor(s, i);
		}
		for (int i = 0; i < CH; i++) {
			float m = mult[i];
			float o = p >= 0.f ? p * 0.5f * i / 7.f : -p * 0.5f * (7 - i) / 7.f;
			if (tempoActive) o = std::round(o * 16.f) / 16.f;
			if (freeze) continue;
			psi[i] += m * dt / T;
			if (psi[i] >= 1.f || psi[i] < 0.f) psi[i] -= std::floor(psi[i]);
			float u = fracf(psi[i] - o);
			bool wrapped = u < prevU[i] - 0.5f || u > prevU[i] + 0.5f;
			prevU[i] = u;
			float v;
			if (lfo) {
				float L;
				switch (P.shape) {
					case RD_SAW: L = 1.f - u; break;
					case RD_TRI: L = u < 0.5f ? 2.f * u : 2.f - 2.f * u; break;
					case RD_SINE: L = 0.5f - 0.5f * std::cos(2.f * PI * u); break;
					case RD_STAIRS: L = u < 0.5f ? 1.f : 0.f; break;
					case RD_STEPRND:
						if (wrapped) lfoA[i] = rng[i].uni();
						L = lfoA[i]; break;
					case RD_RAMPLETS:
						if (wrapped) { lfoA[i] = lfoB[i]; lfoB[i] = rng[i].uni(); }
						L = lerpf(lfoA[i], lfoB[i], 0.5f - 0.5f * std::cos(PI * u)); break;
					default: L = u; break;
				}
				v = P.range == RANGE_PM5 ? 10.f * L - 5.f : P.range == RANGE_0_5 ? 5.f * L : 10.f * L;
			} else {
				float g;
				if (P.shape == RD_STEPRND) {
					if (wrapped) { rndAddr[i] = rng[i].uni(); rndHeld[i] = readAt(rndAddr[i] * ETS_N); }
					v = rndHeld[i];
					out[i] = v;
					continue;
				} else if (P.shape == RD_RAMPLETS) {
					if (wrapped) rndAddr[i] = rng[i].uni();
					g = fracf(rndAddr[i] + u);
				} else {
					g = shapeG(P.shape, u);
				}
				v = readAt(g * ETS_N - 2.f);
			}
			if (!std::isfinite(v)) { v = 0.f; psi[i] = 0.f; }
			out[i] = clampf(v, -10.f, 10.f);
		}

		// ---- channel index: highest output (spec §5.10) ----
		int best = 0;
		for (int i = 1; i < CH; i++) if (out[i] > out[best]) best = i;
		if (tempoActive) {
			if (tEdge && best != shown) { shown = best; idxOut.push(best + 1); }
		} else if (best != shown) {
			if (best != cand) { cand = best; candT = 0.f; }
			candT += dt;
			if (candT >= ETS_INDEX_HYST_S || shown < 0) { shown = best; idxOut.push(best + 1); }
		} else {
			cand = best; candT = 0.f;
		}
		indexOut = idxOut.process(dt);
	}
};

} // namespace ef
