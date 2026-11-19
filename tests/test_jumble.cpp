// Behaviour tests for JumbleSale.
#include "check.hpp"
#include "../src/dsp/JumbleEngine.hpp"
#include <random>

using namespace ef;

static void one(JumbleEngine& e, JumbleParams& P, int input, float v = 1.f) {
	JumbleInputs I;
	I.in[input] = v;
	e.process(P, I);
}

TEST("JumbleSale noon: each input straight to its own output") {
	JumbleEngine e; JumbleParams P;
	for (int i = 0; i < JS_CH; i++) {
		one(e, P, i, 3.f);
		for (int k = 0; k < JS_CH; k++) CHECK_NEAR(e.out[k], k == i ? 3.f : 0.f, 1e-6);
	}
}

TEST("JumbleSale rotate one step: input 1 to output 2, input 8 falls off the end") {
	JumbleEngine e; JumbleParams P;
	P.rotate = 1.f / 8.f;
	one(e, P, 0); CHECK_NEAR(e.out[1], 1.f, 1e-6); CHECK_NEAR(e.out[0], 0.f, 1e-6);
	one(e, P, 7); for (int k = 0; k < JS_CH; k++) CHECK_NEAR(e.out[k], 0.f, 1e-6);
	P.wrap = true;
	one(e, P, 7); CHECK_NEAR(e.out[0], 1.f, 1e-6);
}

TEST("JumbleSale half step crossfades between two outputs") {
	JumbleEngine e; JumbleParams P;
	P.rotate = 0.5f / 8.f;
	one(e, P, 2); CHECK_NEAR(e.out[2], 0.5f, 1e-6); CHECK_NEAR(e.out[3], 0.5f, 1e-6);
}

TEST("JumbleSale full CW/CCW without wrap mutes everything; with wrap it comes back round") {
	JumbleEngine e; JumbleParams P;
	for (float r : {1.f, -1.f}) {
		P.rotate = r; P.wrap = false;
		JumbleInputs I; for (int i = 0; i < JS_CH; i++) I.in[i] = 1.f;
		e.process(P, I);
		for (int k = 0; k < JS_CH; k++) CHECK_NEAR(e.out[k], 0.f, 1e-6);
		P.wrap = true;
		e.process(P, I);
		for (int k = 0; k < JS_CH; k++) CHECK_NEAR(e.out[k], 1.f, 1e-6);
	}
}

TEST("JumbleSale rotate CV: +5 V at full attenuverter = full CW") {
	JumbleEngine e; JumbleParams P; JumbleInputs I;
	P.rotateAtt = 1.f; I.rotateCv = 5.f / 8.f * 3.f;   // three steps
	I.in[0] = 1.f;
	e.process(P, I);
	CHECK_NEAR(e.out[3], 1.f, 1e-5);
	P.rotateAtt = -1.f; P.wrap = true;
	e.process(P, I);
	CHECK_NEAR(e.out[5], 1.f, 1e-5);   // three steps the other way, wrapped: 1 -> 6
}

TEST("JumbleSale radiate: neighbours fade in, 1 and 8 are neighbours, full reaches all") {
	JumbleEngine e; JumbleParams P;
	P.radiate = 0.125f;   // spread 0.5
	one(e, P, 0);
	CHECK_NEAR(e.out[0], 1.f, 1e-6); CHECK_NEAR(e.out[1], 0.5f, 1e-6); CHECK_NEAR(e.out[7], 0.5f, 1e-6);
	CHECK_NEAR(e.out[2], 0.f, 1e-6);
	P.radiate = 1.f;
	one(e, P, 3);
	for (int k = 0; k < JS_CH; k++) CHECK_NEAR(e.out[k], 1.f, 1e-6);
}

TEST("JumbleSale rotate is applied before radiate") {
	JumbleEngine e; JumbleParams P;
	P.rotate = 2.f / 8.f; P.radiate = 0.25f;   // input 1 -> output 3, spread 1
	one(e, P, 0);
	CHECK_NEAR(e.out[2], 1.f, 1e-6); CHECK_NEAR(e.out[1], 1.f, 1e-6); CHECK_NEAR(e.out[3], 1.f, 1e-6);
	CHECK_NEAR(e.out[4], 0.f, 1e-6); CHECK_NEAR(e.out[0], 0.f, 1e-6);
	CHECK_NEAR(e.input1At(2), 1.f, 1e-6);
}

TEST("JumbleSale average option keeps a full-radiate mix at input level; outputs clip at 12 V") {
	JumbleEngine e; JumbleParams P; JumbleInputs I;
	for (int i = 0; i < JS_CH; i++) I.in[i] = 4.f;
	P.radiate = 1.f;
	e.process(P, I);
	CHECK_NEAR(e.out[0], 12.f, 1e-6);
	P.average = true;
	e.process(P, I);
	for (int k = 0; k < JS_CH; k++) CHECK_NEAR(e.out[k], 4.f, 1e-5);
}

TEST("JumbleSale stability: random CV at audio rate") {
	std::mt19937 rng(9);
	std::uniform_real_distribution<float> U(-10.f, 10.f), K(-1.f, 1.f), Z(0.f, 1.f);
	JumbleEngine e; JumbleParams P; JumbleInputs I;
	bool ok = true;
	for (int n = 0; n < 48000 * 5; n++) {
		if (n % 4800 == 0) { P.rotate = K(rng); P.rotateAtt = K(rng); P.radiate = Z(rng); P.radiateAtt = K(rng); P.wrap = Z(rng) > 0.5f; P.average = Z(rng) > 0.5f; }
		for (int i = 0; i < JS_CH; i++) I.in[i] = U(rng);
		I.rotateCv = U(rng); I.radiateCv = U(rng);
		e.process(P, I);
		for (int k = 0; k < JS_CH; k++) if (!std::isfinite(e.out[k]) || std::fabs(e.out[k]) > 12.f) ok = false;
		if (!ok) break;
	}
	CHECK(ok);
}
