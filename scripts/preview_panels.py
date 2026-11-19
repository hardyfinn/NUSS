#!/usr/bin/env python3
"""Render docs/images/panels.png: the three panels with stand-in drawings of the Rack
components, for the manual and for checking the layout. Needs Node with Playwright."""
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_panels as mp

def overlay(p):
    out = []
    for it in p.items:
        x, y, k = it["x"], it["y"], it["kind"]
        r = p.half(it)
        if k in ("knob", "sknob", "bknob"):
            out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#111" stroke="#555" stroke-width="0.3"/><line x1="{x}" y1="{y}" x2="{x}" y2="{y - r + 0.8}" stroke="#fff" stroke-width="0.6"/>')
        elif k == "trim":
            out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#d8d8d8" stroke="#777" stroke-width="0.3"/><line x1="{x}" y1="{y}" x2="{x}" y2="{y - r + 0.5}" stroke="#333" stroke-width="0.5"/>')
        elif k == "jack":
            out.append(f'<circle cx="{x}" cy="{y}" r="3.9" fill="#bbb"/><circle cx="{x}" cy="{y}" r="2.6" fill="#555"/><circle cx="{x}" cy="{y}" r="1.7" fill="#000"/>')
        elif k == "button":
            out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#222" stroke="#666" stroke-width="0.4"/><circle cx="{x}" cy="{y}" r="{r - 1.3}" fill="#3a3a3a"/>')
        elif k == "led":
            out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#5b2" stroke="#222" stroke-width="0.2"/>')
    return "".join(out)

html = ['<html><body style="margin:0;background:#888;display:flex;gap:12px;padding:12px;align-items:flex-start">']
for p in mp.P:
    svg = p.svg().replace("</svg>", overlay(p) + "</svg>")
    svg = svg.replace(f'width="{p.w:.2f}mm" height="{mp.H}mm"', f'width="{p.w * 4:.0f}" height="{mp.H * 4:.0f}"')
    html.append(svg)
html.append("</body></html>")
d = tempfile.mkdtemp()
open(os.path.join(d, "p.html"), "w").write("".join(html))
out = os.path.join(mp.ROOT, "docs", "images", "panels.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
js = """const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:%d,height:%d},deviceScaleFactor:2});
await p.goto('file://%s');await p.screenshot({path:'%s'});await b.close();})();""" % (
    int(sum(p.w * 4 for p in mp.P) + 12 * 4), int(mp.H * 4 + 24), os.path.join(d, "p.html"), out)
open(os.path.join(d, "s.js"), "w").write(js)
subprocess.run(["node", os.path.join(d, "s.js")], check=True)
print("wrote", out)
