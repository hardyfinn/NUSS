// Every tunable number in one place. Values marked [est] are estimates: the Make Noise
// manuals don't give them. See docs/BUILD-INSTRUCTIONS.md §12 and docs/DECISIONS.md.
#pragma once

namespace ef {

constexpr int CH = 8;

// ---- shared -----------------------------------------------------------------
constexpr float GATE_HI = 2.f;            // rising edge above 2 V (MultiMod manual)
constexpr float GATE_LO = 1.f;            // Schmitt release [est]
constexpr float INDEX_V_DEFAULT = 0.5f;   // Channel Index volts per channel (PoliMATHS manual)
constexpr float INDEX_FLAM_S = 0.005f;    // flam step, manual: roughly 5 ms
constexpr float INDEX_GAP_S = 0.001f;     // gap before re-sending the same index [est]
constexpr int INDEX_QUEUE = 32;

// ---- EFG (function generator) ---------------------------------------------------
constexpr float EFG_OUT_V = 9.f;          // Strength range ±9 V (manual)
constexpr float EFG_T_MIN = 0.0005f;      // Rise/Fall at p = 0: 0.5 ms [est]
constexpr float EFG_T_OCT = 16.f;         // octaves of time over the knob: 0.5 ms .. 33 s [est]
constexpr float EFG_T_PMIN = -0.2f;       // CV reach below the knob [est]
constexpr float EFG_T_PMAX = 1.35f;       // CV reach above the knob: ~20 min, MATHS's quoted maximum [est]
constexpr float EFG_CURVE_K = 4.f;        // curve exponent k = 1 + 4|2c-1| [est]
constexpr float EFG_CURVE_TIME = 0.5f;    // Curve lengthens time by up to 50% at the ends [est]
constexpr float EFG_CURVE_CV = 0.1f;      // per volt [est]
constexpr float EFG_SHAPE_CV = 0.2f;      // per volt [est]
constexpr float EFG_CV_FULL = 5.f;        // 5 V = full knob range [est]
constexpr float EFG_RATE_UNI_MIN = 0.25f; // 4 s per cycle at minimum (firmware 1.7.3)
constexpr float EFG_RATE_UNI_OCT = 7.3f;  // 0.25 .. ~40 Hz [est]
constexpr float EFG_RATE_BI_MIN = 2.f;    // bipolar raises the rate: 2 Hz .. 2 kHz [est]
constexpr float EFG_RATE_BI_OCT = 10.f;
constexpr float EFG_NOISE_MULT = 16.f;    // noise S&H at 16x the oscillation rate [est]
constexpr float EFG_SPAN_DEAD = 1.f / 17.f;
constexpr float EFG_PAR_SCALE = 15.f;     // Parallel: full CW gives /16 on the far channel [est]
constexpr int EFG_PAR_CAP = 64;
constexpr float EFG_RECV_KNOB = 0.02f;    // index receive: SPAN within this of centre
constexpr float EFG_RECV_ATT = 0.98f;     // ... and SPAN CV attenuverter at least this

// ---- ETS (time smear) ---------------------------------------------------------
constexpr int ETS_N = 16384;              // buffer slots [est]
constexpr float ETS_T_MIN = 0.05f;        // 50 ms = "about 20 Hz" (manual)
constexpr float ETS_T_RATIO = 600.f;      // .. 30 s at full CW (manual)
constexpr float ETS_T_FLOOR = 0.005f;
constexpr float ETS_T_CEIL = 300.f;
constexpr float ETS_SPREAD_OCT = 3.f;     // x8 / ÷8 at the knob ends (manual)
constexpr float ETS_SPREAD_MAX = 1.528f;  // log2(24)/3: x24 / ÷24 via CV (manual)
constexpr int ETS_STAIRS = 8;             // [est]
constexpr float ETS_INDEX_HYST_S = 0.002f;// [est]
constexpr int ETS_TEMPO_MAXN = 32;        // /1 .. /32 (manual)
constexpr float ETS_TRIG_S = 0.005f;
constexpr float ETS_FLASH_S = 0.05f;

// ---- EGX (gesture extractor) ----------------------------------------------------
constexpr float EGX_R_MIN = 0.05f;        // SPACE at 0: all 8 bands in 50 mV [est]
constexpr float EGX_R_RATIO = 200.f;      // SPACE at 1: 10 V [est]
constexpr float EGX_R_FLOOR = 0.005f;
constexpr float EGX_R_CEIL = 20.f;
constexpr float EGX_HYST = 0.02f;         // fraction of a band [est]
constexpr float EGX_PULSE_S = 0.001f;     // [est]
constexpr float EGX_SPACE_CV = 0.1f;      // per volt [est]
constexpr float EGX_REF = 0.98f;          // reference knob positions for index mode

} // namespace ef
