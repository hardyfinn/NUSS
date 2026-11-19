#!/usr/bin/env python3
"""Generate the Eightfold panels (res/*.svg) and the widget placement header
(src/generated/Layout.hpp) from one table, so the drawing and the controls always match.

Every control gets a printed label, and the script fails if anything overlaps,
sits too close, or leaves the panel.

    python3 scripts/make_panels.py [--font /path/to/DejaVuSans-Bold.ttf]

Needs fontTools (pip install fonttools). Text is converted to paths, because Rack
does not render SVG text. Controls are Rack's own component library widgets.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import haer_logo

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
if "--font" in sys.argv:
    FONT = sys.argv[sys.argv.index("--font") + 1]

# ---------------------------------------------------------------- look ------
GROUND = "#2b2d31"
BAND = "#16171a"
INK = "#ececec"
DIM = "#9a9da3"
OUTBOX = "#45484e"
H = 128.5
HP = 5.08
TOP_BAND = 11.5
BOTTOM_BAND = 121.8
LABEL_CAP = 2.0
SMALL_CAP = 1.6
TITLE_CAP = 3.0
CONDENSE = 0.86

# Rack component sizes (mm, half-extent used for layout checks)
SIZES = {
    "knob": ("RoundBlackKnob", 5.2),
    "bknob": ("RoundBigBlackKnob", 7.7),
    "sknob": ("RoundSmallBlackKnob", 4.1),
    "trim": ("Trimpot", 3.2),
}
JACK_R = 4.2
BTN_R = 4.8
LED_R = 1.6
SLED_R = 1.1

# ---------------------------------------------------------------- text ------
_font = TTFont(FONT)
_glyphs = _font.getGlyphSet()
_cmap = _font.getBestCmap()
_cap = getattr(_font["OS/2"], "sCapHeight", 0) or 0.729 * _font["head"].unitsPerEm
_hmtx = _font["hmtx"]


def text_width(s, cap):
    scale = cap / _cap
    w = 0
    for ch in s:
        g = _cmap.get(ord(ch))
        if g is None:
            raise SystemExit(f"font has no glyph for {ch!r}")
        w += _hmtx[g][0]
    return w * scale * CONDENSE


def text_path(s, x, y, cap, anchor="middle"):
    scale = cap / _cap
    w = text_width(s, cap)
    if anchor == "middle":
        x -= w / 2
    elif anchor == "end":
        x -= w
    d = []
    pen_x = x
    for ch in s:
        g = _cmap[ord(ch)]
        sp = SVGPathPen(_glyphs)
        tp = TransformPen(sp, (scale * CONDENSE, 0, 0, -scale, pen_x, y))
        _glyphs[g].draw(tp)
        d.append(sp.getCommands())
        pen_x += _hmtx[g][0] * scale * CONDENSE
    return " ".join(p for p in d if p)


class Box:
    def __init__(self, x0, y0, x1, y1, what):
        self.x0, self.y0, self.x1, self.y1, self.what = x0, y0, x1, y1, what

    def overlaps(self, o, pad=0.25):
        return not (self.x1 + pad <= o.x0 or o.x1 + pad <= self.x0 or self.y1 + pad <= o.y0 or o.y1 + pad <= self.y0)


class Panel:
    def __init__(self, slug, title, hp, accent, logo=False):
        self.slug, self.title, self.hp, self.accent = slug, title, hp, accent
        self.w = hp * HP
        self.items = []
        self.texts = []    # free text: (text, x, baseline, cap, colour)
        self.lines = []    # decorative lines: (x1, y1, x2, y2, colour, width)
        self.logo = logo   # Haer maker logo on the bottom band

    def text(self, t, x, y, cap, colour=None):
        self.texts.append((t, x, y, cap, colour or DIM))

    def add(self, kind, pid, label, x, y, **ex):
        self.items.append(dict(kind=kind, pid=pid, label=label, x=x, y=y, cap=ex.pop("cap", LABEL_CAP), **ex))

    def knob(self, pid, label, x, y, kind="knob"):
        self.add(kind, pid, label, x, y)

    def trim(self, pid, label, x, y):
        self.add("trim", pid, label, x, y, cap=1.8)

    def jack(self, pid, label, x, y, cap=LABEL_CAP):
        assert pid.endswith("_INPUT") or pid.endswith("_OUTPUT"), pid
        self.add("jack", pid, label, x, y, cap=cap)

    def button(self, pid, label, x, y, light, latch=False, cap=1.8):
        self.add("button", pid, label, x, y, light=light, latch=latch, cap=cap)

    def led(self, pid, label, x, y, small=False):
        self.add("led", pid, label, x, y, small=small, cap=SMALL_CAP)

    # ------------------------------------------------ geometry --
    def half(self, it):
        k = it["kind"]
        if k in SIZES:
            return SIZES[k][1]
        if k == "jack":
            return JACK_R
        if k == "button":
            return BTN_R
        if k == "led":
            return SLED_R if it["small"] else LED_R
        raise ValueError(k)

    def comp_box(self, it):
        r = self.half(it) + (0.2 if it["kind"] == "led" else 0.0)
        return Box(it["x"] - r, it["y"] - r, it["x"] + r, it["y"] + r, it["pid"])

    def label_lines(self, it):
        if not it["label"]:
            return []
        lines = it["label"].split("\n")
        cap = it["cap"]
        x, y = it["x"], it["y"]
        r = self.half(it)
        out = []
        if it["kind"] in ("knob", "sknob", "bknob"):
            base = y - r - 1.2
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (cap + 0.7), cap, "middle"))
        else:
            base = y + r + 1.0 + cap
            for i, ln in enumerate(lines):
                out.append((ln, x, base + i * (cap + 0.7), cap, "middle"))
        return out

    def all_labels(self):
        return [(t, it) for it in self.items for t in self.label_lines(it)]

    def boxes(self):
        b = [self.comp_box(it) for it in self.items]
        for (t, x, base, cap, anchor), it in self.all_labels():
            w = text_width(t, cap)
            b.append(Box(x - w / 2, base - cap, x + w / 2, base, f"label '{t}' of {it['pid']}"))
        for (t, x, base, cap, _) in self.texts:
            w = text_width(t, cap)
            b.append(Box(x - w / 2, base - cap, x + w / 2, base, f"text '{t}'"))
        return b

    def check(self):
        errs = []
        bx = self.boxes()
        n = len(self.items)   # the first n boxes are components (round), the rest labels
        for i, a in enumerate(bx):
            if a.x0 < 1.6 or a.x1 > self.w - 1.6 or a.y0 < TOP_BAND + 1.0 or a.y1 > BOTTOM_BAND - 1.0:
                errs.append(f"{a.what} leaves the panel area")
            for j in range(i + 1, len(bx)):
                c = bx[j]
                if i < n and j < n:
                    p, q = self.items[i], self.items[j]
                    d = math.hypot(p["x"] - q["x"], p["y"] - q["y"])
                    if d < self.half(p) + self.half(q) + 0.4:
                        errs.append(f"{a.what} overlaps {c.what}")
                elif a.overlaps(c):
                    errs.append(f"{a.what} overlaps {c.what}")
        for it in self.items:
            if not it["label"] and it["kind"] != "led":
                errs.append(f"{it['pid']} has no label")
        return errs

    def outbox(self, it):
        """Lighter rounded box behind an output jack and its label."""
        r = JACK_R + 0.9
        lines = self.label_lines(it)
        bottom = max(base for (_, _, base, _, _) in lines) + 0.9 if lines else it["y"] + r
        half_w = max([r] + [text_width(t, c) / 2 + 0.8 for (t, _, _, c, _) in lines])
        return (it["x"] - half_w, it["y"] - r, it["x"] + half_w, bottom)

    # ------------------------------------------------------------ output --
    def svg(self):
        w = self.w
        o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w:.2f}mm" height="{H}mm" viewBox="0 0 {w:.2f} {H}">']
        o.append(f'<rect x="0" y="0" width="{w:.2f}" height="{H}" fill="{GROUND}"/>')
        o.append(f'<rect x="0" y="0" width="{w:.2f}" height="{TOP_BAND}" fill="{BAND}"/>')
        o.append(f'<rect x="0" y="{BOTTOM_BAND}" width="{w:.2f}" height="{H - BOTTOM_BAND:.2f}" fill="{BAND}"/>')
        o.append(f'<rect x="0" y="{TOP_BAND - 0.6:.2f}" width="{w:.2f}" height="0.6" fill="{self.accent}"/>')
        g = []
        for it in self.items:
            if it["kind"] == "jack" and it["pid"].endswith("_OUTPUT"):
                x0, y0, x1, y1 = self.outbox(it)
                g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="1.2" fill="{OUTBOX}"/>')
        for it in self.items:
            if it["kind"] in ("knob", "sknob", "bknob"):
                x, y = it["x"], it["y"]
                r = self.half(it) + 0.9
                for t in range(11):
                    a = math.radians(-135 + 27 * t - 90)
                    g.append(f'<line x1="{x + r * math.cos(a):.2f}" y1="{y + r * math.sin(a):.2f}" x2="{x + (r + 0.8) * math.cos(a):.2f}" y2="{y + (r + 0.8) * math.sin(a):.2f}" stroke="{DIM}" stroke-width="0.3"/>')
        ink, acc = [], []
        cap = TITLE_CAP
        while text_width(self.title, cap) > w - 22:
            cap -= 0.1
        acc.append(text_path(self.title, w / 2, TOP_BAND / 2 + cap / 2, cap))
        bx = w / 2 if not self.logo else w * 0.36
        acc.append(text_path("EIGHTFOLD", bx, BOTTOM_BAND + (H - BOTTOM_BAND) / 2 + SMALL_CAP / 2, SMALL_CAP))
        for (x1, y1, x2, y2, c, sw) in self.lines:
            g.append(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{c}" stroke-width="{sw}" stroke-linecap="round"/>')
        extra = {}
        for (t, x, base, cap_, c) in self.texts:
            extra.setdefault(c, []).append(text_path(t, x, base, cap_))
        for (t, x, base, cap_, anchor), it in self.all_labels():
            ink.append(text_path(t, x, base, cap_, anchor))
        o.append("".join(g))
        o.append(f'<path fill="{INK}" d="{" ".join(ink)}"/>')
        o.append(f'<path fill="{self.accent}" d="{" ".join(acc)}"/>')
        for c, ds in extra.items():
            o.append(f'<path fill="{c}" d="{" ".join(ds)}"/>')
        if self.logo:
            o.append(haer_logo.logo_group(w, BAND, x=w * 0.72, y=BOTTOM_BAND + (H - BOTTOM_BAND) / 2, cap=3.0))
        o.append("</svg>")
        return "\n".join(o)

    def cpp(self):
        L = [f"template <class M, class W>\ninline void place{self.slug}(W* w, M* m) {{"]
        for it in self.items:
            v = f"mm2px(Vec({it['x']:.3f}f, {it['y']:.3f}f))"
            pid, kind = it["pid"], it["kind"]
            if kind in SIZES:
                L.append(f"\tw->addParam(createParamCentered<{SIZES[kind][0]}>({v}, m, M::{pid}));")
            elif kind == "jack":
                fn, add = ("createInputCentered", "addInput") if pid.endswith("_INPUT") else ("createOutputCentered", "addOutput")
                L.append(f"\tw->{add}({fn}<PJ301MPort>({v}, m, M::{pid}));")
            elif kind == "led":
                size = "SmallLight" if it["small"] else "MediumLight"
                L.append(f"\tw->addChild(createLightCentered<{size}<RedGreenBlueLight>>({v}, m, M::{pid}));")
            elif kind == "button":
                cls = "VCVLightBezelLatch<RedGreenBlueLight>" if it["latch"] else "VCVLightBezel<RedGreenBlueLight>"
                L.append(f"\tw->addParam(createLightParamCentered<{cls}>({v}, m, M::{pid}, M::{it['light']}));")
        L.append("}\n")
        return "\n".join(L)

    def labels(self):
        return [(it["pid"], (it["label"] or "").replace("\n", " ")) for it in self.items]


P = []

# ---------------------------------------------------------------- EFG -------
p = Panel("EFG", "EFG · FUNCTION GENERATOR", 22, "#e8913a")
cols = [10.0 + 18.35 * i for i in range(6)]
for x, (n, lab) in zip(cols, [("SPAN", "SPAN"), ("RISE", "RISE"), ("FALL", "FALL"),
                              ("RATE", "RATE"), ("OSC", "OSC"), ("STRENGTH", "STRENGTH")]):
    p.knob(f"{n}_PARAM", lab, x, 22)
    p.trim(f"{n}_ATT_PARAM", "ATTEN", x, 34)
    p.jack(f"{n}_INPUT", f"{lab} CV", x, 45, cap=1.8)
for x, (n, lab) in zip(cols[:3], [("CURVE", "CURVE"), ("SHAPE", "SHAPE"), ("SPREAD", "SPREAD")]):
    p.knob(f"{n}_PARAM", lab, x, 64)
    p.jack(f"{n}_INPUT", f"{lab} CV", x, 76, cap=1.8)
p.button("MODE_PARAM", "MODE", cols[3], 61, "MODE_LIGHT")
p.button("CYCLE_PARAM", "CYCLE", cols[4], 61, "CYCLE_LIGHT", latch=True)
p.button("CYCLETYPE_PARAM", "CYCLE\nTYPE", cols[5], 61, "CYCLETYPE_LIGHT")
p.button("BIPOLAR_PARAM", "BIPOLAR", cols[3], 76, "BIPOLAR_LIGHT", latch=True)
p.button("SUBMIX_PARAM", "SUBMIX", cols[4], 76, "SUBMIX_LIGHT", latch=True)
for x, (n, lab) in zip(cols[:4], [("ACTIVATE", "ACTIVATE"), ("ACCUM", "ACCUM"), ("RESET", "RESET"), ("CYCLEIN", "CYCLE IN")]):
    p.jack(f"{n}_INPUT", lab, x, 93)
oxs = [7.0 + 10.85 * i for i in range(10)]
for i in range(8):
    p.led(f"CH{i + 1}_LIGHT", None, oxs[i], 104.5)
    p.jack(f"OUT{i + 1}_OUTPUT", str(i + 1), oxs[i], 113)
p.jack("POLY_OUTPUT", "POLY", oxs[8], 113)
p.jack("INDEX_OUTPUT", "INDEX", oxs[9], 113)
P.append(p)

# ---------------------------------------------------------------- ETS -------
p = Panel("ETS", "ETS · TIME SMEAR", 12, "#36b5a8")
p.knob("TIME_PARAM", "TIME", 15.0, 22)
p.knob("PHASE_PARAM", "PHASE", 46.0, 22)
p.jack("TIME_INPUT", "TIME CV", 15.0, 35, cap=1.8)
p.jack("PHASE_INPUT", "PHASE CV", 46.0, 35, cap=1.8)
p.knob("SPREAD_PARAM", "SPREAD", 12.0, 55)
p.trim("SPREAD_ATT_PARAM", "ATTEN", 30.5, 55)
p.jack("SPREAD_INPUT", "SPREAD CV", 48.0, 55, cap=1.8)
bx = [8.0, 21.0, 34.0, 47.0]
p.button("HOLD_PARAM", "HOLD", bx[0], 71, "HOLD_LIGHT", latch=True)
p.button("RESET_PARAM", "RESET", bx[1], 71, "RESET_LIGHT")
p.button("READ_PARAM", "READ", bx[2], 71, "READ_LIGHT")
p.button("RANGE_PARAM", "RANGE", bx[3], 71, "RANGE_LIGHT")
p.led("CLOCK_LIGHT", "CLK", 56.0, 71)
jx = [7.0 + 11.7 * i for i in range(5)]
for x, (pid, lab) in zip(jx, [("IN_INPUT", "IN"), ("HOLD_INPUT", "HOLD IN"), ("RESET_INPUT", "RESET IN"),
                             ("TEMPO_INPUT", "TEMPO"), ("CLK_OUTPUT", "CLK OUT")]):
    p.jack(pid, lab, x, 86, cap=1.8)
names = [("OUT1_OUTPUT", "1"), ("OUT2_OUTPUT", "2"), ("OUT3_OUTPUT", "3"), ("OUT4_OUTPUT", "4"), ("OUT5_OUTPUT", "5"),
         ("OUT6_OUTPUT", "6"), ("OUT7_OUTPUT", "7"), ("OUT8_OUTPUT", "8"), ("POLY_OUTPUT", "POLY"), ("INDEX_OUTPUT", "INDEX")]
for k, (pid, lab) in enumerate(names):
    x, y = jx[k % 5], 101 if k < 5 else 113
    p.jack(pid, lab, x, y, cap=1.8)
    if k < 8:
        p.led(f"CH{k + 1}_LIGHT", None, x - 4.0, y + 4.6, small=True)
P.append(p)

# ---------------------------------------------------------------- EGX -------
p = Panel("EGX", "EGX · GESTURE", 10, "#a27ce0")
p.knob("SPAN_PARAM", "SPAN", 13.0, 22)
p.knob("SPACE_PARAM", "SPACE", 38.0, 22)
p.trim("SPAN_ATT_PARAM", "ATTEN", 13.0, 35)
p.jack("SPACE_INPUT", "SPACE CV", 38.0, 36, cap=1.8)
p.led("INDEXMODE_LIGHT", "INDEX", 25.4, 33)
p.jack("SPAN_INPUT", "SPAN CV", 13.0, 49, cap=1.8)
p.jack("CLOCK_INPUT", "CLOCK", 38.0, 49, cap=1.8)
gx = [7.0 + 12.25 * i for i in range(4)]
rows = [64, 78, 92, 106]
outs = [(f"OUT{i + 1}_OUTPUT", str(i + 1)) for i in range(8)] + \
       [("ODD_OUTPUT", "ODD"), ("EVEN_OUTPUT", "EVEN"), ("PULSE_OUTPUT", "PULSE"), ("POLY_OUTPUT", "POLY"), ("INDEX_OUTPUT", "INDEX")]
for k, (pid, lab) in enumerate(outs):
    x, y = gx[k % 4], rows[k // 4]
    p.jack(pid, lab, x, y, cap=1.8)
    if k < 8:
        p.led(f"CH{k + 1}_LIGHT", None, x - 4.0, y + 4.6, small=True)
    if pid == "PULSE_OUTPUT":
        p.led("PULSE_LIGHT", None, x + 4.2, y - 4.8, small=True)
P.append(p)

# ---------------------------------------------------------- JumbleSale -----
# After Make Noise's Jumbler. Laid out top to bottom in the order the signal flows:
# inputs, then the two routing controls, then outputs, with every channel's input
# directly above its output so ROTATE's shift reads as a diagonal.
CTRL_CAP, JACK_CAP = 3.2, 2.9        # project label standard
p = Panel("JumbleSale", "JUMBLESALE", 20, "#e05a8a", logo=True)
W = p.w
cx = [7.6 + 10.0 * i for i in range(8)]    # channel columns
px = 93.0                                  # POLY column
def header(t, y):
    tw = text_width(t, JACK_CAP) / 2 + 2.5
    p.text(t, W / 2, y, JACK_CAP, INK)
    p.lines.append((4.0, y - JACK_CAP / 2, W / 2 - tw, y - JACK_CAP / 2, DIM, 0.35))
    p.lines.append((W / 2 + tw, y - JACK_CAP / 2, W - 4.0, y - JACK_CAP / 2, DIM, 0.35))


header("INPUTS", 17.0)
for i, x in enumerate(cx):
    p.jack(f"IN{i + 1}_INPUT", str(i + 1), x, 24.5, cap=JACK_CAP)
p.jack("POLY_INPUT", "POLY", px, 24.5, cap=JACK_CAP)
p.lines.append((86.0, 20.0, 86.0, 32.5, OUTBOX, 0.4))

kx = [24.0, 77.6]
for x, (n, lab) in zip(kx, [("ROTATE", "ROTATE"), ("RADIATE", "RADIATE")]):
    p.add("bknob", f"{n}_PARAM", lab, x, 53.5, cap=CTRL_CAP)
    p.add("trim", f"{n}_ATT_PARAM", "ATTEN", x - 9.0, 72.0, cap=CTRL_CAP)
    p.jack(f"{n}_INPUT", "CV", x + 9.0, 72.0, cap=JACK_CAP)
p.button("WRAP_PARAM", "WRAP", W / 2, 53.5, "WRAP_LIGHT", latch=True, cap=CTRL_CAP)

header("OUTPUTS", 89.5)
for i, x in enumerate(cx):
    p.led(f"CH{i + 1}_LIGHT", None, x, 95.5)
    p.jack(f"OUT{i + 1}_OUTPUT", str(i + 1), x, 104.0, cap=JACK_CAP)
p.jack("POLY_OUTPUT", "POLY", px, 104.0, cap=JACK_CAP)
P.append(p)


def main():
    errs = []
    for p in P:
        for e in p.check():
            errs.append(f"{p.slug}: {e}")
    if errs:
        print("\n".join(errs))
        raise SystemExit(f"{len(errs)} layout problems")
    os.makedirs(os.path.join(ROOT, "res"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "src", "generated"), exist_ok=True)
    for p in P:
        with open(os.path.join(ROOT, "res", f"{p.slug}.svg"), "w") as f:
            f.write(p.svg())
    with open(os.path.join(ROOT, "src", "generated", "Layout.hpp"), "w") as f:
        f.write("// Generated by scripts/make_panels.py. Do not edit.\n#pragma once\n#include <rack.hpp>\n\n")
        for p in P:
            f.write(p.cpp())
    with open(os.path.join(ROOT, "src", "generated", "labels.txt"), "w") as f:
        for p in P:
            for pid, lab in p.labels():
                f.write(f"{p.slug} {pid} {lab}\n")
    print(f"wrote {len(P)} panels")


if __name__ == "__main__":
    main()
