# Make Noise NUSS in VCV Rack: feasibility

Researched 2026-11-15. NUSS is Make Noise's **New Universal Skiff System**, unveiled at Buchla & Friends in January 2026 ($2,399, 104 hp skiff). This note says which of its modules are documented well enough to emulate, how faithfully, and which are worth doing. Nothing is designed or built yet.

## Lineup (from makenoisemusic.com)

| Module | HP | Price | What it is | Analog/digital |
|---|---|---|---|---|
| MultiWAVE | 26 | $529 | 8 channels x dual wavetable oscillator (Osc A + Osc B), 8 banks x 8 tables, quantizer, glide, built-in MultiMod-style modulator, MIDI/MPE | Digital |
| PoliMATHS | 20 | $459 | 8-channel function generator: MATHS-style Rise/Fall/Curve per channel plus a per-channel oscillation | Digital (not stated, behaviour implies it) |
| MultiMod | 10 | $349 | One CV in, 8 time-shifted / speed-scaled copies out (written to a buffer, read back) | Digital (DSP) |
| QXG (x2) | 12 | $239 | 4-channel stereo low pass gate and mixer, vactrol-free, slow/fast response, Xpread panning | 100% analog |
| QPAS | 18 | $379 | Quad-peak stereo filter: four 12 dB SVF cores, Radiate stereo spread, LP/BP/HP/"Smile Pass" outs | 100% analog |
| XOH | 6 | $110 | 2-channel stereo mixer, line out, headphone out with soft limiting | Analog |
| USB-C input / MIDI Inlet | 2 | $35 | Passive USB-C MIDI/MPE into MultiWAVE | Passive |

The modules talk through **Span modes** (Channel Index, Round, Parallel, Shift Register / Binary Counter) that decide which of the 8 channels an Activate pulse fires, a shared **Channel Index** CV (0.5 V per channel in PoliMATHS; MultiMod's manual says 1 V, then its changelog says it conforms to PoliMATHS), and rear **headers** (MultiWAVE audio and PoliMATHS control both pre-patched into the two QXGs). Header pinouts are not published, but in Rack the obvious replacement is a 8-channel polyphonic cable, which maps onto the system very naturally.

## What is documented

All six have official PDF manuals at makenoise-manuals.com with numbered, control-by-control panel lists. None includes a block diagram or schematic, and none publishes the DSP or circuit internals.

| Module | Manual depth | Gaps |
|---|---|---|
| MultiWAVE | Long (~37k chars): every control, Page 2 functions, activation modes, MIDI map, wavetable list, custom table loading, changelog | The 512 factory wavetables themselves (we'd make our own), exact modulator curves, Channel Index voltage |
| PoliMATHS | Long (~39k chars): every control, cycle modes, Span modes, submix, Channel Index, changelogs | Parallel clock-division table (an image), exact envelope/curve law and time ranges |
| MultiMod | Medium (~23k chars): every jack with voltage ranges, read shapes, clock colour code, patch corner | Buffer length/resolution, exact Spread law beyond the stated x8 to /8 |
| QXG | Short (~7k chars) but complete panel list | No response times, no filter curve; "vactrol-like" response is undefined |
| QPAS | Medium (~16k chars): theory section on FREQ, Radiate, Q, Smile Pass, !!¡¡ inputs, patch ideas | No circuit; Radiate offset amounts and negative-resonance mechanism not quantified |
| XOH | Short (~5k chars) | Nothing important missing; it's a mixer |

Plenty of Make Noise and dealer videos exist (Perfect Circuit reviews of PoliMATHS and MultiMod, launch demos), which help pin down feel and timing by ear.

## Per-module verdict

**MultiWAVE: feasible, behavioural.** Every control and mode is documented, and it's digital, so a behavioural model is close to the real thing in kind. The work is large (it's the biggest module) and the factory wavetables can't be copied; we'd ship our own tables and support loading WAV tables, which the hardware also does. Rack's polyphony makes the 8 outputs a single poly cable plus 8 mono jacks. MIDI/MPE is unnecessary, as Rack's MIDI-CV handles it.

**PoliMATHS: feasible, behavioural, high value.** Fully described control by control. Envelope curve law and time ranges would be estimated from the original MATHS (well understood) and videos. The Span/activation engine is shared with MultiWAVE, so building both shares code.

**MultiMod: feasible, very close.** It's a DSP buffer-and-read-back design with documented voltage ranges, shapes and clock behaviour. This is the easiest to get near-identical in behaviour, and the most unusual idea in the system.

**QXG: feasible, approximate.** The panel is simple and fully listed, but "vactrol-free LPG with a vactrol-like mode" has no published curves, so the tone would be our own LPG model (combined VCA + lowpass on one CTRL, slow/fast decay) tuned by ear. Xpread panning is straightforward.

**QPAS: feasible, approximate in tone.** Behaviour is well explained (four SVFs, Radiate spread, Smile Pass, negative resonance, !!¡¡ ping inputs). Structure can be matched; the analog character can only be approximated without a schematic. Still very worthwhile because it sounds distinctive and nothing in VCV does exactly this.

**XOH: not worth it.** It's a stereo mixer and headphone amp; Rack's audio module and any mixer already do the job.

**USB/MIDI Inlet: not needed.** VCV's own MIDI-CV (with MPE) covers it.

## Already in VCV Rack

- No official Make Noise plugin exists for VCV Rack (unlike Instruō or Befaco, who did official ports). Make Noise has not licensed any.
- MATHS-style function generators: Befaco Rampage (official port) and several free modules covering the same ground; nothing has PoliMATHS's 8-channel Span activation.
- Low pass gates: several (e.g. Bogaudio LPG and LLPG). None is a 4-channel stereo LPG mixer with spread panning.
- Wavetable oscillators: VCV WT VCO, Bogaudio, Surge XT. None is 8-channel with Span activation and dissemination.
- Nothing like MultiMod or QPAS that I could find.

## Recommendation

Worth doing: **MultiMod, PoliMATHS, QPAS, QXG, MultiWAVE**, roughly in that order of value-for-effort. Skip XOH and the MIDI Inlet. A sensible first round is MultiMod + PoliMATHS + QXG (the shared activation engine, the time-smearing idea, and a stereo LPG mixer give a complete mini-system with any oscillator), with QPAS and MultiWAVE as a second round. The faithfulness level throughout would be "behavioural from the manuals", as with the Tudor modules, not circuit-accurate as attempted for Serge.

## Trademark note

Make Noise is a living company selling these modules now. Module names (MultiWAVE, PoliMATHS, MATHS, QPAS, QXG, NUSS) and the panel art are theirs. For a personal plugin the risk is low, but the clean route (and the one needed if it ever went to the VCV Library) is our own names, our own panel design and our own wavetables, describing the modules as "inspired by" rather than clones, and not using the Make Noise name or logo on panels. Copying the manuals' text into our manual should also be avoided; paraphrase.

## Sources

- Make Noise NUSS page: https://www.makenoisemusic.com/systems/new-universal-skiff-system/
- Manuals: https://www.makenoise-manuals.com/ (MultiWAVE, PoliMATHS, MultiMod, QXG, QPAS, XOH, MIDI Inlet PDFs)
- Synth Anatomy NUSS launch: https://synthanatomy.com/2026/01/make-noise-new-universal-skiff-system-a-multi-channel-modular-synthesizer.html
- Gearnews firmware updates (Jul 2026): https://www.gearnews.com/make-noise-nuss-new-universal-skiff-system/
- Synth Anatomy QXG: https://synthanatomy.com/2025/10/make-noise-qxg-a-unique-four-channel-vactrol-free-stereo-low-pass-gate-and-mixer-module.html
- Synth Anatomy MultiWAVE: https://synthanatomy.com/2025/11/make-noise-multiwave-8-channel-wavetable-oscillator.html
- VCV community MATHS clone thread: https://community.vcvrack.com/t/a-clone-of-maths/21539
