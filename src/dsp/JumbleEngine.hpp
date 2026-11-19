// JumbleSale engine: an 8 in, 8 out VCA routing network after Make Noise's Jumbler.
// ROTATE shifts which input lands on which output; RADIATE bleeds each output into its
// neighbours. Plain C++17, no Rack includes.
#pragma once
#include "Common.hpp"

namespace ef {

constexpr int JS_CH = 8;

struct JumbleParams {
	float rotate = 0.f;      // knob, -1..1 (noon = straight through)
	float rotateAtt = 0.f;   // attenuverter, -1..1
	float radiate = 0.f;     // knob, 0..1
	float radiateAtt = 0.f;  // attenuverter, -1..1
	bool wrap = false;       // off: signals pushed past either end fall silent (as Jumbler)
	bool average = false;    // RADIATE divides by the number of outputs blended, so level holds
};

struct JumbleInputs {
	float in[JS_CH] = {};
	float rotateCv = 0.f;    // +/-5 V through the attenuverter sweeps the full range
	float radiateCv = 0.f;
};

struct JumbleEngine {
	float out[JS_CH] = {};
	float rot[JS_CH][JS_CH] = {};  // rot[i][j]: gain of input i at position j after ROTATE
	float kern[JS_CH] = {};        // RADIATE gain by circular distance 0..4
	float shift = 0.f;             // positions moved by ROTATE (-8..8 is the full range)
	float spread = 0.f;            // RADIATE amount 0..4

	// Circular distance between two positions on a ring of 8.
	static float ringDist(float a, float b) {
		float d = std::fabs(a - b);
		d = d - JS_CH * std::floor(d / JS_CH);
		return std::min(d, JS_CH - d);
	}

	void update(const JumbleParams& P, const JumbleInputs& I) {
		float r = P.rotate + P.rotateAtt * I.rotateCv / 5.f;
		shift = clampf(r, -2.f, 2.f) * JS_CH;
		spread = clampf(P.radiate + P.radiateAtt * I.radiateCv / 5.f, 0.f, 1.f) * 4.f;
		for (int i = 0; i < JS_CH; i++) {
			float pos = i + shift;
			for (int j = 0; j < JS_CH; j++) {
				float d = P.wrap ? ringDist((float) j, pos) : std::fabs(j - pos);
				rot[i][j] = std::max(0.f, 1.f - d);
			}
		}
		// Neighbour gain: 1 at distance 0; spread 1 brings the next outputs in fully,
		// spread 4 (full) reaches every output. Outputs 1 and 8 count as neighbours.
		for (int d = 0; d < JS_CH; d++) kern[d] = clampf(spread - d + 1.f, 0.f, 1.f);
	}

	static constexpr float RAIL = 12.f;   // outputs clip at the Eurorack rails, like the hardware

	void process(const JumbleParams& P, const JumbleInputs& I) {
		update(P, I);
		float g = 1.f;
		if (P.average) g = 1.f / (kern[0] + 2.f * (kern[1] + kern[2] + kern[3]) + kern[4]);
		float y[JS_CH] = {};
		for (int i = 0; i < JS_CH; i++) {
			if (I.in[i] == 0.f) continue;
			for (int j = 0; j < JS_CH; j++) y[j] += I.in[i] * rot[i][j];
		}
		for (int k = 0; k < JS_CH; k++) {
			float s = 0.f;
			for (int j = 0; j < JS_CH; j++) {
				int d = std::abs(j - k);
				d = std::min(d, JS_CH - d);
				s += y[j] * kern[d];
			}
			out[k] = clampf(s * g, -RAIL, RAIL);
		}
	}

	// How much of input 1 lands on output k after ROTATE (drives the blue LED).
	float input1At(int k) const { return rot[0][k]; }
};

} // namespace ef
