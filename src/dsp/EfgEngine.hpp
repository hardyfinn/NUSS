// EFG engine: eight rise/fall functions with enveloped oscillations, fired through
// four span modes (spec §4). Plain C++17, no Rack includes.
#pragma once
#include "Common.hpp"
#include "ChannelIndex.hpp"

namespace ef {

enum SpanMode { SPAN_INDEX = 0, SPAN_ROUND, SPAN_PARALLEL, SPAN_BINARY, SPAN_MODES };
enum CycleType { CYCLE_ALL = 0, CYCLE_FOLLOW };
enum Stage { ST_IDLE = 0, ST_RISE, ST_FALL };

struct EfgParams {
	float span = 0.f, spanAtt = 0.f;
	float curve = 0.5f;
	float rise = 0.25f, riseAtt = 0.f;
	float fall = 0.6f, fallAtt = 0.f;
	float shape = 0.f;
	float strength = 1.f, strengthAtt = 0.f;
	float rate = 0.4f, rateAtt = 0.f;
	float osc = 0.f, oscAtt = 0.f;
	float spread = 0.f;
	int mode = SPAN_INDEX;
	bool cycle = false;
	int cycleType = CYCLE_ALL;
	bool bipolar = false;
	bool submix = false;
	float indexV = INDEX_V_DEFAULT;
};

struct EfgInputs {
	float activate = 0.f, accum = 0.f, reset = 0.f, cycleIn = 0.f;
	bool activateP = false, accumP = false;
	float spanCv = 0.f; bool spanCvP = false;
	float curveCv = 0.f, shapeCv = 0.f;
	float spreadCv = 0.f; bool spreadCvP = false;
	float riseCv = 0.f, fallCv = 0.f, rateCv = 0.f, oscCv = 0.f, strengthCv = 0.f;
	bool riseP = false, fallP = false, rateP = false, oscP = false, strengthP = false;
	bool outP[CH] = {};
};

// Curve law (spec §4.5). c in 0..1: 0 log (fast start), 0.5 linear, 1 exp (slow start).
struct CurveLaw {
	float k = 1.f;
	bool expo = false;
	void set(float c) {
		c = clampf(c, 0.f, 1.f);
		k = 1.f + EFG_CURVE_K * std::fabs(2.f * c - 1.f);
		expo = c > 0.5f;
	}
	float timeMult(float c) const { return 1.f + EFG_CURVE_TIME * std::fabs(2.f * clampf(c, 0.f, 1.f) - 1.f); }
	float S(float u) const {
		u = clampf(u, 0.f, 1.f);
		if (k == 1.f) return u;
		return expo ? fastPow(u, k) : 1.f - fastPow(1.f - u, k);
	}
	float Sinv(float y) const {
		y = clampf(y, 0.f, 1.f);
		return expo ? std::pow(y, 1.f / k) : 1.f - std::pow(1.f - y, 1.f / k);
	}
};

// Rise/Fall time for an effective parameter value p (spec §4.5, §12).
inline float efgTime(float p) {
	p = clampf(p, EFG_T_PMIN, EFG_T_PMAX);
	return EFG_T_MIN * std::exp2(p * EFG_T_OCT);
}

// Oscillation wave 0..1 from phasor t and shape h (spec §4.6), without the noise region.
inline float efgWave(float t, float h) {
	float tp = (clampf(h, -1.f, 1.f) + 1.f) * 0.5f;
	if (tp <= 0.f) return 1.f - t;
	if (tp >= 1.f) return t;
	return t < tp ? t / tp : (1.f - t) / (1.f - tp);
}

struct EfgChannel {
	int st = ST_IDLE;
	float p = 0.f;       // progress through the current stage
	float y = 0.f;       // function level 0..1 after Curve
	float theta = 0.f;   // oscillation phasor
	float noise = 0.f, noiseTheta = 0.f;
	float dRise = 0.f, dFall = 0.f, dOsc = 0.f, dStr = 0.f, dRate = 0.f;  // disseminated values
	float out = 0.f;     // channel voltage before submix
	bool queued = false; // follow-the-leader activation waiting for this channel's Fall to end
	bool follow = false; // follow-the-leader activation arriving this sample
	// coefficients, refreshed every few samples and at activation
	float incR = 0.f, incF = 0.f, freq = 1.f, h = 0.f, o = 0.f, S = 1.f;
	bool fresh = true;
};

struct EfgEngine {
	EfgChannel ch[CH];
	ChannelIndexOut idxOut;
	ChannelIndexIn idxIn;
	Schmitt actS, accS, rstS, cycS;
	Rng rng{12345};
	CurveLaw curve;
	int prevSel = 0;
	int roundNext = 0;
	int parCnt[CH] = {};
	uint32_t binCount = 0;
	uint8_t heldMask = 0;
	int coefTick = 0;
	// results
	float out[CH] = {};
	float indexOut = 0.f;
	int cursor = -1;          // channel under the span cursor, -1 none
	bool indexReceive = false;
	float weight[CH] = {};
	uint8_t firedMask = 0;    // channels activated this sample (for tests and lights)

	void reset() {
		for (auto& c : ch) c = EfgChannel();
		idxOut.reset(); idxIn.reset();
		actS.reset(); accS.reset(); rstS.reset(); cycS.reset();
		prevSel = 0; roundNext = 0; binCount = 0; heldMask = 0;
		for (int& c : parCnt) c = 0;
		for (float& o : out) o = 0.f;
		indexOut = 0.f;
	}

	// SPAN in INDEX/BINARY: channel 1..8, 0 = none (spec §4.3).
	static int indexSel(float s) {
		float a = std::fabs(s);
		if (a < EFG_SPAN_DEAD) return 0;
		float band = (1.f - EFG_SPAN_DEAD) / 8.f;
		int n = 1 + (int) std::floor((std::min(a, 1.f) - EFG_SPAN_DEAD) / band);
		n = std::min(std::max(n, 1), 8);
		return s > 0.f ? n : 9 - n;
	}
	static int roundStep(float s) {
		float a = std::fabs(s);
		if (a < EFG_SPAN_DEAD) return 1;
		float band = (1.f - EFG_SPAN_DEAD) / 7.f;
		int k = 1 + (int) std::floor((std::min(a, 1.f) - EFG_SPAN_DEAD) / band);
		return std::min(std::max(k, 1), 7);
	}
	static int parDiv(float s, int i) {
		float a = std::min(std::fabs(s), 1.5f);
		float pos = s >= 0.f ? i / 7.f : (7 - i) / 7.f;
		int d = 1 + (int) std::lround(a * EFG_PAR_SCALE * pos);
		return std::min(std::max(d, 1), EFG_PAR_CAP);
	}

	void activate(int i, const EfgParams& P, const EfgInputs& I) {
		EfgChannel& c = ch[i];
		if (I.riseP) c.dRise = P.riseAtt * I.riseCv / EFG_CV_FULL;
		if (I.fallP) c.dFall = P.fallAtt * I.fallCv / EFG_CV_FULL;
		if (I.oscP) c.dOsc = P.oscAtt * I.oscCv / EFG_CV_FULL;
		if (I.strengthP) c.dStr = P.strengthAtt * I.strengthCv / EFG_CV_FULL;
		if (I.rateP) c.dRate = P.rateAtt * I.rateCv;  // octaves: 1 V/oct at full attenuverter
		c.p = c.st == ST_IDLE ? 0.f : curve.Sinv(c.y);  // re-trigger from the current level
		c.st = ST_RISE;
		c.theta = 0.f;
		c.queued = false;
		c.fresh = true;
		firedMask |= (uint8_t)(1u << i);
		idxOut.push(i + 1);
	}

	void process(float dt, const EfgParams& P, const EfgInputs& I) {
		firedMask = 0;
		idxOut.vPer = P.indexV;
		curve.set(clampf(P.curve + I.curveCv * EFG_CURVE_CV, 0.f, 1.f));
		float cEff = clampf(P.curve + I.curveCv * EFG_CURVE_CV, 0.f, 1.f);
		float tMult = curve.timeMult(cEff);

		bool actEdge = actS.process(I.activate);
		bool accEdge = accS.process(I.accum);
		bool rstEdge = rstS.process(I.reset);
		cycS.process(I.cycleIn);

		// ---- span: which channels this sample activates (spec §4.3) ----
		float s = P.span + (I.spanCvP ? P.spanAtt * I.spanCv / EFG_CV_FULL : 0.f);
		s = clampf(s, -1.5f, 1.5f);
		int sel = indexSel(s);
		indexReceive = P.mode == SPAN_INDEX && I.spanCvP && std::fabs(P.span) < EFG_RECV_KNOB && P.spanAtt > EFG_RECV_ATT;
		uint8_t fire = 0;
		switch (P.mode) {
			case SPAN_INDEX: {
				if (indexReceive) {
					int f = idxIn.process(I.spanCv, P.indexV);
					if (f) fire |= (uint8_t)(1u << (f - 1));
					if (rstEdge && idxIn.last > 0) fire |= (uint8_t)(1u << (idxIn.last - 1));
					cursor = idxIn.last - 1;
				} else {
					idxIn.last = 0;
					if (sel > 0) {
						if (!I.activateP) { if (sel != prevSel) fire |= (uint8_t)(1u << (sel - 1)); }
						else if (actEdge) fire |= (uint8_t)(1u << (sel - 1));
						if (rstEdge) fire |= (uint8_t)(1u << (sel - 1));
					}
					cursor = sel - 1;
				}
			} break;
			case SPAN_ROUND: {
				if (rstEdge) roundNext = 0;
				if (actEdge) {
					fire |= (uint8_t)(1u << roundNext);
					int k = roundStep(s);
					int dir = s < -EFG_SPAN_DEAD ? -1 : 1;
					roundNext = ((roundNext + dir * k) % CH + CH) % CH;
				}
				cursor = roundNext;
			} break;
			case SPAN_PARALLEL: {
				if (rstEdge) for (int& c : parCnt) c = 0;
				if (actEdge) {
					for (int i = 0; i < CH; i++) {
						int d = parDiv(s, i);
						parCnt[i] %= d;
						if (parCnt[i] == 0) fire |= (uint8_t)(1u << i);
						parCnt[i] = (parCnt[i] + 1) % d;
					}
				}
				cursor = -1;
			} break;
			case SPAN_BINARY: {
				int b = (sel > 0 ? sel : 1) - 1;
				if (rstEdge) binCount = 0;
				if (actEdge) {
					binCount = (binCount + 1) & 255u;
					uint32_t mirror = 255u - binCount;
					for (int i = 0; i < CH; i++) {
						bool on;
						if (i == b) on = true;
						else if (i > b) on = (binCount >> (i - b - 1)) & 1u;
						else on = (mirror >> (b - i - 1)) & 1u;
						if (on) fire |= (uint8_t)(1u << i);
					}
				}
				cursor = b;
			} break;
		}
		prevSel = sel;

		// ---- cycling (spec §4.7a) ----
		bool cyc = P.cycle || cycS.high;
		if (cyc && P.cycleType == CYCLE_ALL) {
			for (int i = 0; i < CH; i++)
				if (ch[i].st == ST_IDLE && !(heldMask & (1u << i))) fire |= (uint8_t)(1u << i);
		}
		for (int i = 0; i < CH; i++) {
			EfgChannel& c = ch[i];
			if (!c.follow) continue;
			c.follow = false;
			if (c.st == ST_IDLE) fire |= (uint8_t)(1u << i);
			else if (c.st == ST_FALL) c.queued = true;
			// in Rise: ignored
		}

		// ---- accumulate (spec §4.4) ----
		if (I.accumP) {
			heldMask |= fire;
			fire = 0;
			if (accEdge) { fire = heldMask; heldMask = 0; }
		} else if (heldMask) {
			fire |= heldMask;
			heldMask = 0;
		}
		for (int i = 0; i < CH; i++)
			if (fire & (1u << i)) activate(i, P, I);

		// ---- spread weights (spec §4.10) ----
		float sp = I.spreadCvP ? P.spread * I.spreadCv / EFG_CV_FULL : P.spread;
		sp = clampf(sp, -1.5f, 1.5f);
		for (int i = 0; i < CH; i++)
			weight[i] = sp >= 0.f ? sp * i / 7.f : -sp * (7 - i) / 7.f;

		// ---- run the channels ----
		bool upd = ++coefTick >= 8;
		if (upd) coefTick = 0;
		for (int i = 0; i < CH; i++) {
			EfgChannel& c = ch[i];
			if (upd || c.fresh) {
				float w = weight[i];
				float riseP = P.rise + (I.riseP ? c.dRise : P.riseAtt * w);
				float fallP = P.fall + (I.fallP ? c.dFall : P.fallAtt * w);
				c.incR = dt / (efgTime(riseP) * tMult);
				c.incF = dt / (efgTime(fallP) * tMult);
				c.h = clampf(P.shape + w + I.shapeCv * EFG_SHAPE_CV, -1.5f, 1.5f);
				float oct = P.rate * (P.bipolar ? EFG_RATE_BI_OCT : EFG_RATE_UNI_OCT) + (I.rateP ? c.dRate : 0.f);
				c.freq = std::min((P.bipolar ? EFG_RATE_BI_MIN : EFG_RATE_UNI_MIN) * std::exp2(clampf(oct, -8.f, 14.f)), 0.25f / dt);
				c.o = clampf(P.osc + (I.oscP ? c.dOsc : P.oscAtt * w), 0.f, 1.f);
				c.S = clampf(P.strength + (I.strengthP ? c.dStr : P.strengthAtt * w * 2.f), -2.f, 2.f);
				c.fresh = false;
			}
			bool eoc = false;
			if (c.st == ST_RISE) {
				c.p += c.incR;
				if (c.p >= 1.f) { c.st = ST_FALL; c.p = 0.f; c.y = 1.f; }
				else c.y = curve.S(c.p);
			} else if (c.st == ST_FALL) {
				c.p += c.incF;
				if (c.p >= 1.f) { c.st = ST_IDLE; c.p = 0.f; c.y = 0.f; eoc = true; }
				else c.y = 1.f - curve.S(c.p);
			}

			// oscillation (spec §4.6, §4.7); an idle channel has y = 0, so skip it
			float y = 0.f;
			if (c.y > 0.f) {
				c.theta = fracf(c.theta + c.freq * dt);
				c.noiseTheta += c.freq * EFG_NOISE_MULT * dt;
				if (c.noiseTheta >= 1.f) { c.noiseTheta = fracf(c.noiseTheta); c.noise = rng.uni(); }
				float wave = efgWave(c.theta, c.h);
				float nAmt = clampf((std::fabs(c.h) - 1.f) / 0.5f, 0.f, 1.f);
				if (nAmt > 0.f) wave = lerpf(wave, c.noise, nAmt);
				if (P.bipolar) wave = 2.f * wave - 1.f;
				y = c.y * ((1.f - c.o) + c.o * wave);
			}

			// strength (spec §4.8)
			float S = c.S;
			float v;
			if (P.submix || std::fabs(S) <= 1.f) v = EFG_OUT_V * S * y;
			else if (S > 1.f) { float e = std::min(S - 1.f, 1.f); v = EFG_OUT_V * (e + (1.f - e) * y); }
			else { float e = std::min(-S - 1.f, 1.f); v = EFG_OUT_V * (e - (1.f - e) * y); }
			if (!std::isfinite(v)) { c = EfgChannel(); v = 0.f; }
			c.out = v;

			if (eoc) {
				idxOut.release(i + 1);
				if (cyc && P.cycleType == CYCLE_FOLLOW) ch[(i + 1) % CH].follow = true;
				if (c.queued) { c.queued = false; c.follow = true; }
			}
		}

		// ---- outputs and submix (spec §4.9) ----
		if (P.submix) {
			float acc = 0.f;
			for (int i = 0; i < CH; i++) {
				acc += ch[i].out;
				if (I.outP[i]) { out[i] = P.bipolar ? 10.f * std::tanh(acc / 10.f) : fold10(acc); acc = 0.f; }
				else out[i] = clampf(ch[i].out, -10.f, 10.f);
			}
		} else {
			for (int i = 0; i < CH; i++) out[i] = clampf(ch[i].out, -10.f, 10.f);
		}
		indexOut = idxOut.process(dt);
	}

	static float fold10(float v) {
		for (int n = 0; n < 32 && (v > 10.f || v < -10.f); n++) v = v > 10.f ? 20.f - v : -20.f - v;
		return clampf(v, -10.f, 10.f);
	}
	bool held(int i) const { return heldMask & (1u << i); }
};

} // namespace ef
