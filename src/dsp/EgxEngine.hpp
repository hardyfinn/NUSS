// EGX engine: turns the movement of a CV, or a Channel Index signal, into eight channel
// gates, odd/even gates and a change pulse (spec §6). Plain C++17, no Rack includes.
#pragma once
#include "Common.hpp"
#include "ChannelIndex.hpp"

namespace ef {

struct EgxParams {
	float span = 0.f, atten = 1.f, space = 1.f;
	float indexV = INDEX_V_DEFAULT;
	float gateV = 10.f;
	bool pulseOnLeave = false;
};

struct EgxInputs {
	float spanCv = 0.f; bool spanCvP = false;
	float spaceCv = 0.f;
	float clock = 0.f; bool clockP = false;
};

struct EgxEngine {
	int band = 0;      // band the input is in now (1..8, 0 none)
	int active = 0;    // committed channel (1..8, 0 none)
	Schmitt clkS;
	ChannelIndexIn idxIn;
	ChannelIndexOut idxOut;
	Pulse pulse;
	bool indexMode = false;
	bool pendingNew = false;  // a fresh index arrived (counts even when it's the same channel)
	bool pulseHigh = false;
	float indexOut = 0.f;
	float R = 10.f;
	int pulses = 0;    // count, for tests

	void reset() { band = active = 0; clkS.reset(); idxIn.reset(); idxOut.reset(); pulse.reset(); pulses = 0; pendingNew = false; }

	static float rangeFor(float space) {
		return clampf(EGX_R_MIN * std::pow(EGX_R_RATIO, space), EGX_R_FLOOR, EGX_R_CEIL);
	}

	void process(float dt, const EgxParams& P, const EgxInputs& I) {
		idxOut.vPer = P.indexV;
		indexMode = P.span <= -EGX_REF && P.atten >= EGX_REF && P.space >= EGX_REF;
		bool forceNew = false;
		if (indexMode) {
			int f = idxIn.process(I.spanCvP ? I.spanCv : 0.f, P.indexV);
			if (f) { band = f; forceNew = true; }
			else if (idxIn.last == 0) band = 0;
		} else {
			idxIn.last = 0;
			float v = P.span * 5.f + P.atten * (I.spanCvP ? I.spanCv : 0.f);
			R = rangeFor(P.space + I.spaceCv * EGX_SPACE_CV);
			float bw = R / CH;
			float hy = EGX_HYST * bw;
			int raw = (v < 0.f || v >= R) ? 0 : std::min(CH, 1 + (int) std::floor(v / bw));
			if (raw != band) {
				if (band == 0) {
					if (v >= hy && v < R - hy) band = raw;
				} else {
					float lo = (band - 1) * bw, hi = band * bw;
					if (v < lo - hy || v >= hi + hy) band = raw;
				}
			}
		}

		if (forceNew) pendingNew = true;
		bool commit = !I.clockP || clkS.process(I.clock);
		if (!I.clockP) clkS.reset();
		if (commit) {
			if (band != active || (pendingNew && band > 0)) {
				int prev = active;
				active = band;
				if (active > 0) {
					pulse.fire(EGX_PULSE_S); pulses++;
					idxOut.push(active);
				} else {
					idxOut.release(prev);
					if (P.pulseOnLeave) { pulse.fire(EGX_PULSE_S); pulses++; }
				}
			}
			pendingNew = false;
		}
		pulseHigh = pulse.process(dt);
		indexOut = idxOut.process(dt);
	}

	float gate(int i, const EgxParams& P) const { return active == i + 1 ? P.gateV : 0.f; }
	float odd(const EgxParams& P) const { return (active > 0 && (active & 1)) ? P.gateV : 0.f; }
	float even(const EgxParams& P) const { return (active > 0 && !(active & 1)) ? P.gateV : 0.f; }
};

} // namespace ef
