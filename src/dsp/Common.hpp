// Small helpers shared by the engines. Plain C++17, no Rack includes.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "Constants.hpp"

namespace ef {

constexpr float PI = 3.14159265358979f;
inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
inline float fracf(float x) { return x - std::floor(x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Fast approximations (about 1e-4 relative), used for the curve law in the audio loop.
inline float fastLog2(float x) {  // after P. Mineiro's fastapprox
	union { float f; uint32_t i; } vx{x};
	union { uint32_t i; float f; } mx{(vx.i & 0x007FFFFFu) | 0x3F000000u};
	float y = (float) vx.i * 1.1920928955078125e-7f;
	return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}
inline float fastExp2(float x) {
	x = std::max(-126.f, std::min(126.f, x));
	float fi = std::floor(x), f = x - fi;
	float p = 1.f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * 0.00961813f)));
	union { float f; uint32_t i; } v{p};
	v.i += (uint32_t)((int) fi) << 23;
	return v.f;
}
inline float fastPow(float u, float k) {
	if (u <= 0.f) return 0.f;
	return fastExp2(k * fastLog2(u));
}

// Gate input with a Schmitt trigger. process() returns true on a rising edge.
struct Schmitt {
	bool high = false;
	bool process(float v) {
		bool prev = high;
		high = high ? (v > GATE_LO) : (v > GATE_HI);
		return high && !prev;
	}
	void reset() { high = false; }
};

// Small fast PRNG (xorshift32).
struct Rng {
	uint32_t s = 0x9E3779B9u;
	explicit Rng(uint32_t seed = 1) { seed_(seed); }
	void seed_(uint32_t seed) { s = seed ? seed * 2654435761u : 0x9E3779B9u; if (!s) s = 1; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }  // 0..1
};

// Trigger generator: high for a set time after fire().
struct Pulse {
	float left = 0.f;
	void fire(float t) { left = t; }
	bool process(float dt) { bool on = left > 0.f; if (on) left -= dt; return on; }
	void reset() { left = 0.f; }
};

} // namespace ef
