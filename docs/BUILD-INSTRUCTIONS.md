# Build instructions: EIGHTFOLD, two 8-channel modulation modules for VCV Rack

**Audience:** an Opus model implementing this plugin from scratch.
**Owner:** Hardy. Fluent in VCV Rack, does not read C++. Build the whole plugin in one go, test it yourself, hand over the finished result. No milestone check-ins.

**What it is:** a VCV Rack 2 plugin, brand and slug `Eightfold`, with three modules whose behaviour follows three Make Noise New Universal Skiff System (N.U.S.S.) modules as described in their public manuals:

| Our module | Inspired by | One line |
|---|---|---|
| **EFG** Eightfold Function Generator | PoliMATHS | Eight MATHS-style rise/fall functions, each with an enveloped oscillation, fired by one gate through four "span" activation modes |
| **ETS** Eightfold Time Smear | MultiMod | One CV in, eight copies out, each read from a shared buffer at its own speed and phase |
| **EGX** Eightfold Gesture Extractor | GTE (Gestural Time Extractor) | Turns the movement of a CV, or a Channel Index signal, into eight channel gates, odd/even gates and a change-pulse stream |

Feasibility notes and sources: `docs/RESEARCH.md` (copy of `/mnt/project-files/nuss-vcv/01-feasibility.md`).

**Decisions used here (Hardy asked on 2026-11-15 for EFG and ETS to be designed for Opus, then for EGX to be added before the build; the defaults below were chosen by Fable and can be overridden):**

| Question | Answer used here |
|---|---|
| Faithfulness | Behavioural, from the official manuals. There are no schematics or published DSP. Where the manuals leave a number out, this spec gives an estimate, marked **[est]**, kept in one constants file and logged in the accuracy ledger (§12) |
| Names | Plugin `Eightfold`, modules `EFG`, `ETS` and `EGX`. "Make Noise", "MATHS", "PoliMATHS", "MultiMod", "GTE", "Gestural Time Extractor", "N.U.S.S." never appear in plugin, module, panel or tag names, and no Make Noise art or logo is used. The manual may say "inspired by the Make Noise PoliMATHS" once per module |
| Hardware press-and-hold combos | Replaced by dedicated, labelled buttons (and right-click menu items). Rack users can't discover hold combos and the panels have room |
| Polyphony | Every module has eight mono channel outputs **and** one 8-channel POLY output. Inputs are mono (channel 0) unless stated |
| Channel Index voltage | 0.5 V per channel on all three modules (the PoliMATHS figure). The MultiMod manual prints 1 V per channel but its changelog says it follows PoliMATHS; a right-click option offers 1 V per channel |
| Look | Text labels on every control and jack, no symbols. Per-channel RGB LEDs mirror the hardware's activity windows |
| Repo | `hardyfinn/NUSS` (created by Hardy 2026-11-15), work on `main`. The repo name is fine for a personal repo; the plugin, module and panel names stay Eightfold/EFG/ETS |
| Builds | Apple silicon Mac only (Hardy's standing preference). Keep the other platforms' CI jobs out unless trivial to leave in |

---

## 0. Ground rules

1. **Behaviour first.** Each section below describes behaviour from the manual. Implement that behaviour exactly; where a number is marked [est], use it as given and log it.
2. **All tunable constants in one place:** `src/dsp/Constants.hpp`, one named `constexpr` per [est] value, each with a comment pointing at the section and saying "estimate".
3. **DSP separate from Rack.** Engines live in `src/dsp/` as plain C++17 with no Rack includes, unit-tested on the host. `src/modules/*.cpp` are thin wrappers.
4. **No allocation, locks, file I/O or logging in `process()`.** ETS's buffer is allocated once in the constructor.
5. **Every param has a tooltip and unit, every port a name.** The tooltip name is the printed label spelled out.
6. **Stability.** Clamp every state variable; end each `process()` with an `isfinite` guard that resets bad state to 0. Outputs stay within ±10 V (EFG soft-clips submix overs as in §4.9).
7. **Ambiguity:** pick the reading closest to the manual, write it in `docs/DECISIONS.md`, list it in the hand-over. The manual extracts in §4 and §5 note the known inconsistencies; follow the resolution given here.
8. **CPU:** EFG and ETS each under 0.5% of one core at 48 kHz, EGX under 0.1%.
9. **Persistence:** every button state (modes, cycle type, bipolar, submix, read shape, LFO range, hold) saves to the patch via `dataToJson`/`dataFromJson`. The hardware also saves these at power-off.
10. Licence GPL-3.0-or-later.

---

## 1. Setup

- **VCV Rack 2.x**, newest Rack SDK. Read https://vcvrack.com/manual/PluginDevelopmentTutorial, https://vcvrack.com/manual/Panel, https://vcvrack.com/manual/DSP and https://vcvrack.com/manual/Polyphony first.
- `make`, `make dist`, `make install` with `RACK_DIR` set.
- Unit tests with **doctest** in `tests/`; `make test` builds `tests/*.cpp` with `src/dsp/*.cpp` as a host executable and runs it.
- `.github/workflows/build.yml`: reuse the working workflow from `hardyfinn/silverbox` or `hardyfinn/tudor` (both build Mac arm64 green). Build macOS arm64, run `make test` on the runner, upload the `.vcvplugin` artifact on every push, attach it to a release on tags `v*`. The Rack SDK download is blocked from the cloud container, so Rack builds happen only on Actions; `make test` runs locally.

```
plugin.json  Makefile  README.md  LICENSE
docs/  BUILD-INSTRUCTIONS.md (this file)  RESEARCH.md  DECISIONS.md  MANUAL.md  Eightfold-Manual.pdf
res/   EFG.svg  ETS.svg  EGX.svg  components/
scripts/ make_panels.py  make_manual_pdf.sh
src/
  plugin.hpp  plugin.cpp
  dsp/  Constants.hpp  Span.hpp  FunctionChannel.hpp  EfgEngine.hpp
        TimeBuffer.hpp  ReadHead.hpp  EtsEngine.hpp  ChannelIndex.hpp  EgxEngine.hpp  (+ .cpp as needed)
  modules/  EFG.cpp  ETS.cpp  EGX.cpp
  widgets/  EightfoldComponents.hpp  ChannelLeds.hpp
tests/
```

`plugin.json`: slug `Eightfold`, name `Eightfold`, brand `Eightfold`, version `2.0.0`.

| Slug | Name shown in browser | HP | Tags |
|---|---|---|---|
| `EFG` | `EFG Eightfold Function Generator` | 22 | Function generator, Envelope generator, LFO, Polyphonic |
| `ETS` | `ETS Eightfold Time Smear` | 12 | Delay, LFO, Utility, Polyphonic |
| `EGX` | `EGX Eightfold Gesture Extractor` | 8 | Clock modulator, Logic, Utility, Polyphonic |

Descriptions should contain the words "8 channel", "function", "envelope" (EFG) "time shift", "phase", "copies" (ETS), and "gesture", "gates", "channel index", "comparator" (EGX) so search finds them. Rack only accepts its own tag list; use the closest valid tags.

---

## 2. Signal conventions

| Signal | Level |
|---|---|
| Gates and triggers in | Rising edge at **> 2 V** (Schmitt: high above 2 V, low below 1 V). Matches MultiMod's stated threshold; the PoliMATHS manual gives none |
| Trigger outs | ETS CLK OUT 10 V, 5 ms; EGX PULSE 10 V, 1 ms (§6.5) |
| Gate outs (EGX channel, ODD, EVEN) | 0 V / 10 V |
| EFG channel outputs | ±9 V per channel (Strength range), submix may go higher, then folded/clipped (§4.9) |
| ETS channel outputs | ±10 V (input is DC-coupled ±10 V; copies keep the level) |
| Channel Index | 0.5 V × channel number (Ch1 = 0.5 V … Ch8 = 4 V), 0 V = none. Option: 1 V per channel |
| CV inputs | ±10 V accepted. Per-parameter scaling is given with each control |
| Pitch (EFG RATE in 1 V/oct use) | 1 V/oct |

**Knob conventions.** "Attenuverter" = bipolar −1…+1, centre 0. "Combo pot" = a knob that is an offset when its CV jack is empty and an attenuator (0…1, or −1…+1 where stated) for that CV when the jack is patched, as on the hardware.

---

## 3. Panels

- Dark grey panel, white text, one accent colour per module (EFG warm orange, ETS teal, EGX violet) for section headings only. Generic look, nothing resembling Make Noise panel art.
- Every control and jack has a printed text label. Section headings group controls (ACTIVATION, FUNCTION, OSCILLATION, SPREAD, OUTPUTS on EFG; INPUT, TIME, PHASE, SPREAD, READ, OUTPUTS on ETS).
- Each channel has a column with its RGB LED ("activity window") above its output jack, numbered 1 to 8. The 8 outputs sit in a row at the bottom, POLY OUT and INDEX OUT next to them.
- Buttons are latching or momentary as stated, with an RGB LED inside or next to them showing the state colour from the tables.
- Generate panels with `scripts/make_panels.py` (SVG, text converted to paths), as in the Silverbox repo.

---

## 4. EFG, Eightfold Function Generator (22 HP)

### 4.1 Overview

Eight identical channels. Each channel, when **activated**, runs a Rise then Fall function (like MATHS, with variable Curve). An oscillation of selectable shape is mixed in and enveloped by the function. The result is scaled by Strength and sent to that channel's output. Which channel(s) an incoming gate activates is decided by **Span** and the **Span mode**. Five per-channel parameters (Rise, Fall, Osc, Strength, Shape) can be **spread** across the channels from one knob, or each one sampled per channel from a CV at activation (**dissemination**).

### 4.2 Controls and jacks

| Label | Kind | Function |
|---|---|---|
| ACTIVATE | gate in | Rising edge activates channel(s) per Span mode (§4.3) |
| ACCUM | gate in | While patched, activations are held and released together on the next ACCUM edge (§4.4) |
| RESET | gate in | Mode-dependent reset (§4.3) |
| SPAN | knob, −1…+1, centre 0 | Mode-dependent (§4.3) |
| SPAN CV | in | Adds to SPAN through SPAN CV attenuverter; 5 V × attenuverter = full knob range [est] |
| SPAN CV (atten) | attenuverter | |
| MODE | button, momentary | Cycles Span mode: INDEX → ROUND → PARALLEL → BINARY → INDEX. LED: white, yellow, cyan, magenta |
| CYCLE | button, latching | Cycling on/off. LED shows cycle type colour when on, off when off |
| CYCLE TYPE | button, momentary | Toggles ALL (purple) / FOLLOW (pink). (Hardware: hold CYCLE 1 s) |
| CYCLE IN | gate in | High = cycling on, while the CYCLE button is off. Ignored when the button is on |
| BIPOLAR | button, latching | Oscillation bias: off = unipolar, orange = bipolar. (Hardware: hold MODE 1 s) |
| SUBMIX | button, latching | Submix outputs on/off (§4.9). LED green when on. (Hardware: hold CYCLE+MODE) |
| CURVE | knob 0…1 (default 0.5) | Log (0) → linear (0.5) → exp (1). Global, not spreadable |
| CURVE CV | in | Live, adds 0.1 per volt [est] |
| RISE | knob 0…1 (default 0.3) | Rise time (§4.5). Spreadable |
| RISE (atten) | attenuverter | Spread depth, or dissemination depth when RISE CV patched |
| RISE CV | in | Dissemination source |
| FALL / FALL (atten) / FALL CV | as RISE | Fall time. Default 0.5 |
| SHAPE | knob −1…+1 (default 0) | Oscillation wave: −1 falling saw, 0 triangle, +1 rising ramp (§4.6). Spreadable (no attenuverter: Spread uses a fixed depth of 1 [est]) |
| SHAPE CV | in | Live, 0.2 per volt [est]; values past ±1 go into the noise region (§4.6) |
| STRENGTH | knob −1…+1 (default +1) | Output scaling, −9 V … 0 … +9 V (§4.8). Spreadable |
| STRENGTH (atten) / STRENGTH CV | as RISE | |
| RATE | knob 0…1 (default 0.4) | Oscillation rate (§4.7) |
| RATE (atten) / RATE CV | | Disseminated. With atten at +1, RATE CV tracks 1 V/oct |
| OSC | knob 0…1 (default 0) | Oscillation amount (§4.7). Spreadable |
| OSC (atten) / OSC CV | as RISE | |
| SPREAD | combo knob −1…+1 (default 0) | Global spread amount and direction (§4.10) |
| SPREAD CV | in | When patched, SPREAD knob attenuates it (bipolar); 5 V × 1 = full spread [est] |
| OUT 1…8 | out | Channel outputs |
| POLY OUT | out, 8 ch | Same as OUT 1…8 |
| INDEX OUT | out | Channel Index (§4.11) |
| LEDs 1…8 | RGB | §4.12 |

Spreadable parameters per the manual: Rise, Fall, Osc, Shape, Strength. Disseminated: Rise, Fall, Rate, Osc, Strength. Global and live (no spread, no dissemination): Span, Curve, Shape CV, Spread. The manual contradicts itself on Rate's spreadability; **Rate is disseminated but not spread** (follow the Spread section's list), logged in DECISIONS.

### 4.3 Span modes

Quantise SPAN (knob + CV, clamped to −1.5…+1.5) as each mode needs. Channels are 1…8 in this text, 0…7 in code.

**INDEX (white).** SPAN selects one channel: centre band (|span| < 1/17) selects none; right of centre selects 1…8 in equal bands from centre to full CW; left of centre selects 8…1 from centre to full CCW (so full CCW = channel 1, as on the hardware where left-of-noon runs 8 down to 1).
- If ACTIVATE is **unpatched**, the selected channel activates whenever the selection changes to it (so sweeping SPAN plays channels).
- If ACTIVATE is **patched**, the selected channel activates on each ACTIVATE edge. RESET re-triggers the selected channel.
- **Index receive:** when the SPAN knob is at centre and SPAN CV atten is at +1 (within 0.02), SPAN CV is read as a Channel Index voltage instead: channel = round(V / 0.5) (or / 1 V with the option), 0 = none. Each change to a non-zero index activates that channel immediately (also each new index in a FLAM burst, §4.11). LED on MODE shows bright white in this state. This is how ETS or another EFG drives this one.

**ROUND (yellow).** Step size k = 1…7 from |span| in 7 equal bands beyond the centre band (centre band = step 1). Each ACTIVATE edge activates the next channel: pointer += k (right of centre) or −= k (left of centre), modulo 8. RESET sets the pointer so the next edge activates channel 1. LED colour of the cursor: yellow for right, lime for left.

**PARALLEL (cyan).** Every channel counts ACTIVATE edges and activates on its own division. Divisions [est, the manual's table is an image we couldn't read]: let a = |span| (0…1.5) and pos_i = (i−1)/7 for right of centre, (8−i)/7 for left. d_i = 1 + round(a × 15 × pos_i), capped at 64 (so full CW gives Ch1 /1 … Ch8 /16; CV past the knob reaches /23; centre gives all /1). Left of centre mirrors (Ch1 largest). Channel i activates when its counter reaches 0 mod d_i. RESET zeroes all counters so every channel fires on the next edge.

**BINARY (magenta).** An 8-bit counter c increments on each ACTIVATE edge. Let s = selected channel from SPAN as in INDEX (none → 1). Channel s fires every edge. The other channels take successive bits: channel at distance j from s (j = 1…7, going right and wrapping) fires on edges where bit (j−1) of c is 1. **Our reading** of the manual's "second counter running in the opposite direction": channels to the left of s use a second counter c' = 255 − c, so the channel adjacent to s on either side flips most often. Log this. RESET clears both counters.

All modes: activation of a channel that is already running restarts its function from its current level (re-trigger from current value, like MATHS), not from 0.

### 4.4 Accumulate

When ACCUM is patched, every activation produced by §4.3 (or by cycling) sets a per-channel "held" flag instead of firing (LED orange). On an ACCUM rising edge, all held channels fire together and the flags clear. Unpatched: activations fire immediately.

### 4.5 The function (per channel)

State: phase ∈ {idle, rise, fall}, level x ∈ 0…1.
- **Rise** goes 0→1 (or from the current x on re-trigger) over time Tr; **Fall** goes 1→0 over Tf, then idle.
- Times [est]: T = 0.5 ms × 2^(p × 25.25) for effective parameter p ∈ 0…1 (0.5 ms … ~20 min is MATHS's quoted range; the knob alone, p ≤ 1, gives 0.5 ms…~20 min; clamp p to −0.2…1.0). With the default, Rise 0.3 ≈ 96 ms, Fall 0.5 ≈ 3 s. Choose and document the actual law in DECISIONS if this feels wrong in testing; MATHS's taper is strongly expo.
- **Curve** c ∈ 0…1 shapes both segments: progress u ∈ 0…1, value y = shape(u): c = 0.5 linear; c → 0 logarithmic (fast start: y = 1 − (1−u)^k); c → 1 exponential (slow start: y = u^k), with k = 1 + 4 × |2c − 1| [est]. The manual notes Curve also changes overall time; apply a time multiplier 1 + 0.5 × |2c − 1| [est].
- **End of fall** = an EOC event used by cycling (§4.7a).

### 4.6 Oscillation shape

A phasor θ ∈ 0…1 per channel. Shape h (−1…+1, after spread):
- h = −1: falling saw 1 − θ. h = 0: triangle. h = +1: rising ramp θ. In between, a variable-slope triangle with peak at θp = (h + 1)/2 (θp = 0 is falling saw, 1 is ramp). Output w ∈ 0…1.
- |h| > 1 (via CV): crossfade toward sample-and-hold noise refreshed at 16× the oscillation rate [est], fully noise at |h| = 1.5. ("Turning CV past the knob gives other waveforms including noise.")
- Unipolar: w as is (0…1). Bipolar: w_b = 2w − 1.

### 4.7 Oscillation amount, rate and how it combines

- **Rate** [est]: unipolar f = 0.25 Hz × 2^(r × 7.3) (0.25…40 Hz on the knob; 4 s per cycle at minimum, matching firmware 1.7.3). Bipolar raises the range: f = 2 Hz × 2^(r × 10) (2 Hz…2 kHz) for vibrato/FM/audio use. RATE CV × atten adds to r in octaves/10 (i.e. 1 V = +1 octave when atten = +1). CV may push below the knob minimum.
- The phasor θ resets to 0 at each activation (so enveloped bursts are repeatable) [est].
- **Combination**, with function level x (0…1, after Curve), Osc amount o (0…1), Strength S (−1…+1):
  - Unipolar: y = x × ((1 − o) + o × w). At o = 0 pure function; at o = 1 the oscillation's full amplitude is enveloped by x (tremolo/AM shape).
  - Bipolar: y = x × ((1 − o) + o × w_b). At o = 1 the output swings ± around 0 V inside the envelope.
  - Output V = 9 V × S × y (before submix and firmware-1.7.3 offset, §4.8).
- Oscillation polarity and amount apply before Strength, per the manual.

### 4.7a Cycling

- **ALL (purple):** while cycling is on, any idle channel activates immediately; each channel re-activates at its own EOC. Turning cycling on starts all eight. (Like MATHS's EOC patched to its own trigger, ×8.)
- **FOLLOW (pink):** when channel i reaches EOC, channel i+1 activates (8 → 1). A chain has to be started by an activation. If the target channel is still in its Rise when the activation arrives, the activation is ignored; if it is in its Fall, the activation is queued until its Fall ends. Several chains may run at once.
- Cycling activations go through ACCUM if it is patched.

### 4.8 Strength and the DC offset behaviour

- Normal range S ∈ −1…+1 (knob, plus spread or dissemination). S = 0 means the channel outputs 0 V.
- With SUBMIX **off**, values of S beyond ±1 (only reachable via spread, CV or dissemination) behave as in firmware 1.7.3: for S > 1, add a positive DC offset of 9 V × (S − 1) and shrink the function by the same proportion, reaching pure DC (+9 V) at S = 2; for S < −1, the output becomes DC offset minus the envelope ("voltage mirror"), fading to pure positive DC at S = −2 [est on the exact curves; log]. Practical use: with S beyond the knob and no activations, each output is a steady voltage.
- With SUBMIX on, S beyond ±1 just keeps scaling.

### 4.9 Submix

With SUBMIX on, each **patched** output carries the sum of its own channel plus every **unpatched** channel to its left back to the previous patched output. (E.g. only OUT 4 and OUT 8 patched: OUT 4 = ch1+2+3+4, OUT 8 = ch5+6+7+8.) POLY OUT always carries the eight unmixed channels. Overs: in unipolar oscillation mode, fold anything above 10 V back down (wavefold at 10 V, applied repeatedly); in bipolar, soft-clip with tanh to ±10 V.

### 4.10 Spread and dissemination

- Spread s = SPREAD knob (or knob × SPREAD CV / 5 V when patched), clamped −1.5…+1.5.
- Channel weight: for s ≥ 0, w_i = s × (i − 1)/7 (rightmost gets most); for s < 0, w_i = |s| × (8 − i)/7 (leftmost gets most). At s = 0 every weight is 0, so attenuverters do nothing (as the manual says).
- For each spreadable parameter P with its CV jack **unpatched**: P_i = knob_P + atten_P × w_i × range_P, where range_P = 1 for Rise, Fall, Osc and 2 for Strength; for Shape, Shape_i = knob + w_i × 1 (fixed depth).
- **Dissemination:** if P's CV jack is patched, P is removed from Spread. At each activation of channel i, sample CV, store D_i = atten_P × CV / 5 V (for Rate: atten × CV / 1 V octaves), and hold it in that channel until its next activation. P_i = knob_P + D_i. This lets one LFO or sequence give each channel its own value at its moment of activation.
- Global live parameters (Curve, Shape CV, Span, Spread) apply to all channels at once, immediately.

### 4.11 Channel Index out (shared code with ETS: `dsp/ChannelIndex.hpp`)

- Holds 0.5 V × (most recently activated channel) while that channel's function runs; returns to 0 V when that channel goes idle.
- Re-activating the channel already indicated drops the output to 0 V for one 1 ms gap [est], then back, so a receiver sees a new index.
- If several channels activate in the same sample (Parallel, Binary, Accumulate, Cycle All), send them as a **flam**: each index in turn for 5 ms, separated by the 1 ms gap, lowest channel first, the last one staying high.
- Option in right-click menu: "Channel Index: 0.5 V per channel (default) / 1 V per channel". Same option on ETS.

### 4.12 LEDs

Per channel: green brightness = positive output, red = negative; orange = held by ACCUM; white outline/cursor (set the LED white at 30% when not otherwise lit) = channel selected by SPAN in INDEX and BINARY; in ROUND the cursor shows the next channel. With SPREAD ≠ 0 and the channel idle, show blue at brightness w_i. MODE LED shows Span mode colour (bright white for Index receive). CYCLE LED purple/pink. BIPOLAR orange. SUBMIX green.

### 4.13 Right-click menu

Span mode, Cycle type, Bipolar, Submix (duplicates of the buttons), Channel Index scale, and "Reset all channels".

### 4.14 Behaviour tests (doctest, on `EfgEngine`)

1. INDEX, ACTIVATE patched, SPAN on channel 3: each gate fires only channel 3; INDEX OUT = 1.5 V while it runs, 0 V after.
2. INDEX, ACTIVATE unpatched: sweeping SPAN from centre to full CW fires 1…8 in order once each.
3. ROUND with step 1: 10 gates fire 1,2,…,8,1,2. RESET after gate 3 makes gate 4 fire channel 1. Step 3 right: 1,4,7,2,5,8,3,6.
4. PARALLEL full CW: over 16 gates channel 1 fires 16 times, channel 8 once (/16), RESET realigns all.
5. BINARY with s = 1: over 8 gates, channel 1 fires 8 times, channel 2 four times, channel 3 four times in runs of two, channel 4 in a run of four.
6. ACCUM: gates go to orange flags, nothing fires until ACCUM edge, then all flagged channels fire together and INDEX OUT flams (5 ms steps).
7. Curve 0.5, Rise 1 s: level at 0.5 s is 0.5 ±0.01. Curve 1: level at 0.5 s < 0.3. Curve 0: > 0.7.
8. Cycle ALL: all eight start and each re-fires at its own EOC; FOLLOW: activation of 3 then runs 3→4→5…→2→3.
9. Osc 0, Strength +1, Rise→peak: output peaks at 9.0 V ±0.05. Strength −1: −9 V. Strength 0: 0 V.
10. Osc 1 unipolar: output stays in 0…9 V and its envelope follows the function; bipolar: output has both signs.
11. RATE atten +1, CV +1 V: oscillation frequency doubles (±0.5%).
12. Spread +1, Rise atten +1: Rise_8 > Rise_1, Rise_1 = knob. Spread −1 reverses. Spread 0: atten has no effect.
13. Dissemination: Rise CV patched to a ramp; channels activated at different times keep different Rise times, which don't change until reactivated.
14. Submix with only OUT 4 and OUT 8 patched: OUT 4 = sum of channels 1–4.
15. Strength beyond +1 with submix off: output gains a DC offset, pure +9 V at S = 2.
16. Index receive: SPAN centre, atten +1, CV stepping 0.5, 1.0, 1.5 V fires channels 1, 2, 3.

---

## 5. ETS, Eightfold Time Smear (12 HP)

### 5.1 Overview

The input is written continuously into a circular buffer whose length in time is set by TIME. Eight read heads read the buffer, each at its own speed (SPREAD) and with its own phase offset (PHASE), in the direction pattern chosen by READ shape. With nothing patched to IN, an internal LFO is used instead and the eight outputs are related LFOs. Optionally clocked by TEMPO, which snaps Time, Phase and Spread to musical ratios.

### 5.2 Controls and jacks

| Label | Kind | Function |
|---|---|---|
| IN | in | Signal, DC-coupled, ±10 V. Unpatched → internal LFO (§5.7) |
| HOLD | button, latching + LED | Stop writing, loop the current buffer (§5.6) |
| HOLD IN | gate in | Rising edge toggles Hold [the manual says "momentary gate engages Hold"; **our reading:** gate high = held, low = released, unless the button is latched on]. Log |
| RESET | button, momentary | §5.6 |
| RESET IN | gate in | Same as button |
| TEMPO | clock in | §5.8 |
| CLK OUT | trigger out | §5.9 |
| TIME | combo knob 0…1 (default 0.4) | Buffer length (§5.3) |
| TIME CV | in | When patched: 0…5 V covers the knob range, ±10 V accepted, knob acts as offset added to the CV ("unity CV plus a panel offset") |
| PHASE | combo knob 0…1 (default 0) | Phase spacing (§5.5). When PHASE CV patched, the knob attenuates it |
| PHASE CV | in | 0…5 V = knob range; up to 10 V keeps increasing; negative reverses order |
| SPREAD | knob −1…+1 (default 0) | Speed spread (§5.4) |
| SPREAD CV + (atten) | in + attenuverter | ±5 V × atten = knob range; beyond extends to ×24 / ÷24 |
| READ | button, momentary | Cycles read shape (§5.5a). LED colour per shape |
| RANGE | button, momentary | Cycles LFO range 0…10 V (green), ±5 V (purple), 0…5 V (blue). Only affects the internal LFO. (Hardware: hold Reset + press Shape) |
| OUT 1…8 | out | Copies |
| POLY OUT | out, 8 ch | Same |
| INDEX OUT | out | Channel Index (§5.10) |
| LEDs 1…8 | RGB | Speed colour (§5.4) with brightness from output level |
| CLOCK LED | RGB | §5.9 |

### 5.3 Buffer and Time

- One circular buffer of N = 16384 floats [est], allocated once.
- **Buffer length in time:** T = 0.05 s × 600^t (t = 0 → 50 ms, i.e. the manual's "about 20 Hz"; t = 1 → 30 s). t = knob, or knob + CV/5 V when patched; t clamped −1…+1.5 → T from ~0.08 ms clamp to 5 ms floor, and up to ~735 s, clamp T to 5 ms…300 s.
- **Write head:** phasor φw advances by dt / T per sample. Write position = φw × N. When the write position passes several slots in one sample (short T), fill each with linearly interpolated input; when it passes less than one (long T), write the input averaged since the last slot (box filter) [est]. Writing pauses while Hold is on.
- Short T is "more responsive"; long T gives Phase and Spread more depth and delays changes reaching the outputs, as in the manual.

### 5.4 Spread (speed per channel)

- s = SPREAD knob + atten × SPREAD CV / 5 V, clamped to ±log2(24)/3 ≈ ±1.528.
- Channel i (0…7) speed multiplier m_i = 2^(3 × s × (3.5 − i)/3.5). Full CW (s = 1): Ch1 ×8, Ch8 ÷8, with Ch4/Ch5 changing least (×1.35 / ÷1.35). Full CCW mirrors. Centre: all ×1. Beyond the knob via CV: up to ×24 / ÷24 at the ends.
- **Speed LED colours:** aqua = within ±5% of ×1; green = faster up to ×8; blue = slower down to ÷8; hot pink = faster than ×8; yellow = slower than ÷8.
- With TEMPO patched, m_i snaps to the nearest integer (≥1) or 1/integer (§5.8).

### 5.5 Read heads and Phase

- Each channel has a read phasor ψ_i ∈ 0…1 advancing by m_i × dt / T per sample.
- Phase offset: p = PHASE value (0…1 knob range; up to 2 via CV; negative via CV reverses). Channel offset o_i = p × 0.5 × (i/7) for p ≥ 0 (Ch1 offset 0, Ch8 offset 0.5 = 180°), and for p < 0 o_i = |p| × 0.5 × ((7 − i)/7) (reversed order). At p = 2 (10 V) all channels meet again at full offset (o = 1 ≡ 0 for Ch8… acceptable approximation; log).
- Read address a_i = (g(frac(ψ_i − o_i)) − ε) × N, wrapped, where g is the read shape (§5.5a) and ε = 2/N keeps the head just behind the write head. Read with linear interpolation between slots.
- **Result:** with Spread centre, Phase 0 and the Ramp shape, ψ_i tracks φw and every output equals the input (a precision mult). With Phase raised, each channel is a delayed copy, up to half the buffer. With Spread off centre, each head drifts past the write head, producing discontinuities where it crosses from old to new material, as the manual describes.
- Changing SPREAD away from centre and back leaves phase offsets between heads; RESET realigns.

### 5.5a Read shapes (READ button, cycle order as listed; default RAMP)

Let u = frac(ψ_i − o_i).

| LED | Name | g(u) / behaviour |
|---|---|---|
| Red | RAMP | g = u (forward). Accurate copies |
| Green | SAW | g = 1 − u (backward). Reversed copies |
| Blue | TRIANGLE | g = 2u for u < 0.5, 2 − 2u after (ping-pong). Discontinuities at turns |
| Purple | SINE | g = 0.5 − 0.5 cos(2πu) (ping-pong slowing at the ends; "wow and flutter") |
| Pink | STAIRS | g = floor(u × 8)/8 + small hold: reads the copy in 8 held steps [est step count] |
| Orange | STEP RANDOM | At each wrap of u (per channel), pick a random address and output the buffer value there, held until next wrap ("separate tape heads at random places") |
| Yellow | RAMPLETS | At each wrap of u, jump to a random address and play forward from there at speed m_i until the next wrap |

Per-channel random generators, seeded differently. Shapes persist in the patch.

### 5.6 Hold and Reset

- **Hold on:** writing stops; the buffer content loops. The current write position becomes the reset point. Time, Phase and Spread still act on the held loop (Time stretches/compresses it).
- **Reset** (button or RESET IN edge): set every ψ_i = φw (or, under Hold, the captured hold position). With Spread at centre the heads stay aligned after reset; otherwise they drift again. In LFO mode, the LFO restarts its cycle. CLOCK LED flashes blue.

### 5.7 Internal LFO (IN unpatched)

- No buffer. Output_i = range(L(frac(ψ_i − o_i))) where ψ_i advances at m_i / T, so one LFO cycle = T and each channel is the same LFO at its own speed and phase.
- L by READ shape: RAMP rising ramp, SAW falling saw, TRIANGLE triangle, SINE sine, STAIRS square (50% duty), STEP RANDOM a new random value each cycle (stepped), RAMPLETS smoothly interpolated random (cosine interpolation between per-cycle random targets).
- RANGE: 0…10 V (default, green), ±5 V (purple), 0…5 V (blue).
- Hold in LFO mode acts as a track-and-hold: outputs freeze at their current values.

### 5.8 Tempo

When TEMPO is patched (rising edge > 2 V), measure the period P between edges (average of last 2, ignore jitter < 1%).
- **Time** snaps to T = P × n, n = 1…32 from the TIME value in 32 equal steps (CCW = ×1, CW = ×32). Write phasor φw re-syncs to the clock: on each Tempo edge where the cycle count is a multiple of n, set φw = 0.
- **Spread** multipliers snap to integers or reciprocals (§5.4).
- **Phase** offsets snap to multiples of 1/16 of the buffer.
- **INDEX OUT** updates only on Tempo edges.
- **Hold** loop length snaps to the same grid.
- If no edge arrives for 4 × P, fall back to free-running with the last T.

### 5.9 CLK OUT and CLOCK LED

- CLK OUT emits a 5 ms 10 V trigger each time φw wraps (one per buffer length T).
- CLOCK LED flashes on each trigger. Colour without Tempo: by T: red < 0.2 s, orange < 1.5 s, yellow < 8 s, green ≤ 30 s, purple when T is shorter than the knob range allows (CV pushed), blue when longer [band edges est]. With Tempo: green when n is a power of 2, red when a multiple of 3, gold otherwise. Blue flash on reset.

### 5.10 Channel Index out

Every sample (or on Tempo edges when patched), find the channel with the highest output value and send its index on INDEX OUT, using the shared `ChannelIndex` code (0.5 V per channel default, 1 V option). Change only when a different channel becomes highest, with 2 ms hysteresis [est] to avoid chatter with audio inputs. Patch this into EFG's SPAN CV (EFG in INDEX receive) to make EFG fire whichever ETS copy is currently on top.

### 5.11 Right-click menu

Read shape, LFO range, Channel Index scale, "Clear buffer".

### 5.12 Behaviour tests (doctest, on `EtsEngine`)

1. Spread centre, Phase 0, RAMP, IN = slow sine: all 8 outputs equal the input within 1% (mult behaviour).
2. Phase 1, Spread 0, T = 1 s, IN = step: channel 8 shows the step about 0.5 s after channel 1.
3. Spread +1: channel 1's head moves 8× faster than the write head; channel 8's 1/8 ×. LED colours: Ch1 green, Ch8 blue, all aqua at centre.
4. SAW: output of a rising ramp input reads as falling within each buffer cycle.
5. HOLD: after hold, changing IN does not change outputs; the held material repeats with period T.
6. RESET with Spread ≠ 0: after reset, outputs re-align with each other at the instant of reset.
7. IN unpatched, RAMP, Spread 0, Phase 1, range 0–10 V: eight ramps of period T, Ch8 half a cycle behind Ch1, all within 0…10 V.
8. STAIRS LFO = square; STEP RANDOM LFO = one new value per cycle per channel.
9. TEMPO at 2 Hz, TIME full CW: T = 16 s (32 × 0.5 s); CLK OUT fires every 16 s; CLOCK LED colour green (32 is a power of 2).
10. Channel Index: with eight distinct DC-ish outputs, INDEX OUT = 0.5 V × index of the highest.
11. Time CV at 0 V vs 5 V with knob at 0 spans 50 ms → 30 s.

---

## 6. EGX, Eightfold Gesture Extractor (8 HP)

### 6.1 Overview

Inspired by the Make Noise GTE (Gestural Time Extractor), an analog N.U.S.S. companion. A CV at SPAN CV is split into eight voltage bands, one per channel. Whichever band the voltage is in, that channel's gate is high. As the CV moves, the gates change, so the **motion** of any signal (an LFO, an envelope, a hand on a knob, an ETS copy) becomes rhythm. SPACE sets how many volts it takes to cross all eight bands, so it sets how busy the gates are. A clock input can make changes happen only on clock edges. With the knobs at reference settings, EGX translates a Channel Index signal from EFG, ETS or another source into eight gates.

The GTE manual is short and gives almost no numbers (no input range, band edges, gate level, pulse width or LED behaviour). Everything numeric here is [est] and goes in the accuracy ledger (§12). The hardware is analog; this is a behavioural model.

### 6.2 Controls and jacks

| Label | Kind | Function |
|---|---|---|
| SPAN | knob −1…+1 (default 0) | Offset added to the input (±5 V): moves which band the signal sits in. Higher voltage = higher channel |
| SPAN CV | in | The signal being watched |
| SPAN (atten) | attenuverter −1…+1 (default +1) | Sensitivity and polarity of SPAN CV. Centre = CV ignored, CCW = inverted |
| SPACE | knob 0…1 (default 1) | Voltage needed to sweep all eight channels (§6.3). Low = tiny movements cross many bands |
| SPACE CV | in | Adds to SPACE, 0.1 per volt [est] |
| CLOCK | gate in | When patched, the active channel only updates on rising edges (§6.4) |
| OUT 1…8 | gate out | One high at a time, or none |
| POLY OUT | out, 8 ch | The eight gates as one poly cable |
| PULSE | trigger out | One trigger every time the active channel changes (the "pulse stream": denser when the input moves faster) |
| ODD | gate out | High while the active channel is 1, 3, 5 or 7 |
| EVEN | gate out | High while the active channel is 2, 4, 6 or 8 |
| INDEX OUT | out | Channel Index of the active channel (0.5 V per channel), so EGX can drive EFG directly. **Our addition**; the GTE has no index out. Log it |
| LEDs 1…8 | per channel | White on the active channel. An LED by PULSE flashes on each trigger. (The GTE manual lists a channel LED but doesn't describe it) |

### 6.3 Band mapping

- Input voltage v = SPAN × 5 V + atten × SPAN CV.
- Total range R (volts covered by the eight bands) = 0.05 V × 200^space [est]: SPACE full CW → 10 V (1.25 V per band); SPACE at 0 → 50 mV total. SPACE CV can push R lower, to a 5 mV floor, or higher, to 20 V.
- Bands run from 0 V up to R: channel n (1…8) is active when (n−1) × R/8 ≤ v < n × R/8. Below 0 V or at/above R, **no channel is active** (all gates low, ODD and EVEN low), matching the manual's "outside the range set by Space, none is high".
- **Hysteresis:** a change needs v to pass a band edge by 2% of the band width [est], so a slow or noisy signal doesn't chatter at an edge.
- Jumps across several bands in one sample go straight to the new channel and fire one PULSE, not one per skipped band [est; log].

### 6.4 Clock

- CLOCK unpatched: updates every sample.
- CLOCK patched: compute the band continuously but only commit the active channel (gates, ODD/EVEN, PULSE, INDEX OUT) on a CLOCK rising edge (> 2 V). If the band at the edge is the same channel as before, no PULSE.

### 6.5 Outputs

- Channel gates, ODD, EVEN: 10 V high, 0 V low (menu option 5 V).
- PULSE: 1 ms 10 V trigger [est] on each committed change, including entering from "none". Leaving to "none" fires no pulse by default (menu option).
- INDEX OUT: shared `ChannelIndex` code; 0 V when no channel is active.

### 6.6 Channel Index translation

The manual's reference settings are SPAN fully CCW, SPAN atten fully CW, SPACE fully CW. Our band maths wouldn't read a 0.5 V-per-channel signal correctly at those settings, so EGX detects them explicitly: when SPAN ≤ −0.98, atten ≥ 0.98 and SPACE ≥ 0.98, it switches to **index mode** (the LED by SPACE lit bright white, like EFG's MODE LED in index receive):
- channel = round(SPAN CV / 0.5 V) (or / 1 V with the 1 V option); 0 or out of range = none.
- Each new non-zero index becomes the active channel at once (or at the next CLOCK edge if patched) and fires PULSE. Flam bursts (several 5 ms indices in a row, §4.11) play as a quick flam across the gates, as the GTE manual describes.
- Moving any of the three knobs off the reference returns to band mode.
Log that this is our reading: the GTE manual gives only the knob positions.

### 6.7 Right-click menu

Channel Index scale (0.5 V / 1 V), gate level (10 V / 5 V), "Pulse on leaving range" (default off).

### 6.8 Behaviour tests (doctest, on `EgxEngine`)

1. SPACE 1, atten +1, SPAN 0: input 0.6 V → OUT 1 only, ODD high, EVEN low; 1.9 V → OUT 2 and EVEN; 9.9 V → OUT 8; 10.1 V and −0.1 V → nothing high, ODD and EVEN low.
2. A ramp 0 → 10 V over 1 s: eight PULSE triggers, channels 1…8 in order, INDEX OUT steps 0.5 → 4 V.
3. Same ramp with SPACE 0.5 (R ≈ 0.71 V): channel 8 reached within the first ~0.07 s, then nothing high.
4. Hysteresis: a signal sitting on a band edge with 1 mV noise gives no extra pulses.
5. CLOCK at 4 Hz with a fast LFO in: gates change only on clock edges.
6. Atten −1: a rising input steps the channels downward.
7. Index mode: CV 0.5, 1.0, 1.5 V → channels 1, 2, 3, one PULSE each. A flam (1.5, 1.0, 0.5 V, 5 ms each) → three pulses, ending on channel 1.
8. EFG engine INDEX OUT → EGX engine in index mode: EGX gates follow EFG activations.

### 6.9 Patch ideas for the manual

- **Gesture rhythms:** a slow random or an ETS copy into SPAN CV, SPACE low, gates to drums. Faster or bigger motion = busier rhythm.
- **Motion clock:** PULSE as a clock that speeds up when the input moves faster.
- **Odd/even call and response:** ODD and EVEN to two voices.
- **EGX → EFG:** INDEX OUT into EFG SPAN CV (index receive), so an LFO's motion picks which EFG envelope fires; or POLY OUT straight into a poly envelope or LPG.
- **ETS → EGX:** ETS INDEX OUT into EGX in index mode: eight gates showing which ETS copy is highest.

## 7. EFG, ETS and EGX together

The hardware links modules with Channel Index voltages and pre-wired headers. In Rack:
- **Channel Index:** ETS INDEX OUT → EFG SPAN CV with EFG in INDEX, SPAN centre, SPAN atten full CW. EFG INDEX OUT → another EFG the same way. Any INDEX OUT → EGX in index mode (§6.6) gives eight gates; EGX INDEX OUT → EFG lets the motion of any CV choose EFG channels.
- **Headers:** use POLY OUT cables. EFG POLY OUT → an 8-voice poly VCA/LPG's CV input, oscillator POLY → its audio input, gives the QXG-style "eight voices, eight envelopes" layout with no extra modules from us.
- **Spread an LFO:** ETS (IN unpatched, Phase raised) → EFG SPREAD CV, or ETS OUT n → EFG RISE CV to disseminate a different rise time each activation.

---

## 8. Leaning on existing modules (document in the manual)

These two modules intentionally have no audio voice or LPG. For the patch recipes use Rack modules Hardy already has or that are free in the Library:
- **Oscillators:** VCV VCO / WT VCO (polyphonic, 8 voices).
- **Low pass gate / VCA:** Bogaudio LLPG or LPG (poly), or VCV VCA + VCF, as the QXG stand-in.
- **Mixer / stereo spread:** VCV Mix, Bogaudio Mix8, or Bogaudio Pan + VCV Merge/Split for an 8-voice stereo spread.
- **Quantizer:** VCV Quantizer (poly) for the Shift Register recipe.
Say in the manual which recipes need which, and why (the NUSS gets these jobs from MultiWAVE and QXG, which we haven't built).

---

## 9. General tests

- Plugin loads, all three modules appear by searching "EFG", "ETS", "EGX", "Eightfold", "function", "time shift", "gesture".
- All params have tooltips; all ports named; POLY OUT reports 8 channels.
- Save/load restores every button state (modes, cycle, bipolar, submix, read shape, range, hold).
- Sample rate change (44.1 → 96 kHz) keeps times and rates correct within 1%.
- No NaN/inf after 10 minutes of random CV on every input at audio rate (fuzz test on the engines).
- CPU under the §0 budget, measured with Rack's meter on the Mac build.

---

## 10. Manual (`docs/MANUAL.md` and `docs/Eightfold-Manual.pdf`)

Same structure as the Silverbox and Tudor manuals: what each module is (one line saying it's inspired by the Make Noise PoliMATHS / MultiMod / GTE, a living maker's current product, and that this is an independent behavioural re-imagining with its own names), a labelled panel drawing, every control in plain English, the LED colour tables, the Channel Index link, and **patch recipes** (each with the modules needed and the settings):

1. **Eight-voice plucks from one gate:** EFG ROUND, step 1 → POLY OUT to LPG CV, poly VCO → LPG audio, Spread raising Fall from left to right.
2. **Polyrhythmic envelopes:** EFG PARALLEL, clock into ACTIVATE, SPAN full CW, outputs to eight different destinations.
3. **Binary bursts:** EFG BINARY, Osc at 0.6, Bipolar off, for tremolo-shaped hits that thin out by channel.
4. **Follow the leader drone swell:** EFG FOLLOW cycling, long Rise/Fall, Strength past full via CV for offsets.
5. **Audio-rate EFG:** Bipolar on, Osc full, RATE CV from a keyboard (1 V/oct), Rise short, Fall medium: eight enveloped oscillators.
6. **Dissemination sequencing:** a slow random into STRENGTH CV, ROUND mode: each channel's level is set at its own hit.
7. **ETS shift register:** (manual's Patch Corner) Spread centre, Phase from 0 up, sequencer CV into IN, copy of its clock into TEMPO, outputs → quantizer → eight VCOs for canons.
8. **ETS snap focus:** tempo to TEMPO, Spread off centre, a /8 of the tempo into RESET: copies drift and snap back.
9. **ETS noise spectrum:** noise into IN, short Time, outputs to a mixer for gritty lo-fi drums.
10. **ETS weighted random gates:** a PWM square into IN, STEP RANDOM read.
11. **ETS clock divider/multiplier:** IN unpatched, STAIRS shape, Spread to taste: eight related clocks.
12. **The pair:** ETS INDEX OUT → EFG SPAN CV (Index receive): the loudest ETS copy fires its EFG channel; ETS LFOs into EFG SPREAD CV.
13. **Gesture rhythms (EGX):** an ETS copy into EGX SPAN CV, SPACE low, gates to drums; PULSE as a motion-speed clock; ODD/EVEN to two voices.
14. **All three:** ETS LFO → EGX → INDEX OUT → EFG: one slow LFO's motion picks which of eight envelopes fires.

Build the PDF with `scripts/make_manual_pdf.sh` (pandoc or a Python HTML-to-PDF path, as in the Silverbox repo). Copy the finished PDF to `/mnt/project-files/nuss-vcv/Eightfold-Manual.pdf` if that folder is reachable.

---

## 11. Order of work

1. Repo skeleton, plugin.json, CI (Mac arm64), `make test` harness.
2. `ChannelIndex`, `Span`, `FunctionChannel`, `EfgEngine` + tests §4.14.
3. `TimeBuffer`, `ReadHead`, `EtsEngine` + tests §5.12.
3a. `EgxEngine` + tests §6.8.
4. Panels and module wrappers, LEDs, menus, persistence.
5. General tests §9, CI green, tag `v2.0.0`.
6. Manual and PDF, README with install steps.
7. Hand-over: what was built, every [est] value used, every ambiguity decided, CI run link, release link.

---

## 12. Accuracy ledger (values not in the manuals, all [est])

| Item | Value used | Basis |
|---|---|---|
| EFG Rise/Fall time law | 0.5 ms × 2^(25.25p) | Original MATHS range; PoliMATHS manual gives no numbers |
| Curve exponent and time multiplier | k = 1 + 4|2c−1|, ×(1 + 0.5|2c−1|) | Manual says log/lin/exp and that Curve affects time |
| Rate ranges | Uni 0.25–40 Hz, bipolar 2 Hz–2 kHz | Manual: slowest ~4 s/cycle (fw 1.7.3); bipolar "raises the rate" for audio use |
| Shape noise region | Noise fully at |h| = 1.5, S&H at 16× rate | Manual says noise appears past the knob |
| Parallel division table | 1 + round(a×15×pos), cap 64 | Manual table is an image we could not read |
| Binary second counter | Mirrored counter to the left of Span channel | Manual wording ambiguous |
| Strength > ±1 offset curves | Linear crossfade to DC by S = ±2 | Firmware 1.7.3 note, no numbers |
| SPAN / SPREAD / CURVE / SHAPE CV scaling | 5 V = full knob | Hardware convention |
| Channel Index flam gap | 5 ms triggers, 1 ms gaps | Manual gives "roughly 5 ms" |
| ETS buffer size | 16384 slots | Not published |
| ETS Time law | 50 ms × 600^t | Manual: ~20 Hz to 30 s |
| ETS Spread law | 2^(3s(3.5−i)/3.5) | Manual: ×8/÷8 ends, Ch4/5 least, ×24 via CV |
| ETS stairs steps | 8 | Not published |
| ETS Index hysteresis | 2 ms | Avoids chatter; fw 0.7.5 "improved with audio input" |
| Clock LED colour bands | 0.2 s / 1.5 s / 8 s | Manual gives colours only |
| HOLD IN behaviour | Gate-high = held | Manual says momentary gate engages hold |
| EGX range law | R = 0.05 V × 200^space (50 mV…10 V) | GTE manual: Space sets the volts for all 8 channels, minimum "infinitesimally small"; no numbers |
| EGX band origin | Bands from 0 V up to R | Not published |
| EGX hysteresis | 2% of band width | Not published |
| EGX gate level / PULSE width | 10 V / 1 ms | Not published (a third-party listing says 5 V gates; menu offers 5 V) |
| EGX index mode detection | Explicit at the reference knob positions | Manual gives only the knob positions |
| EGX INDEX OUT | Added | Not on the hardware; lets EGX drive EFG |

Each value lives in `src/dsp/Constants.hpp`. If testing by ear suggests a better value, change it there and note it in DECISIONS.
