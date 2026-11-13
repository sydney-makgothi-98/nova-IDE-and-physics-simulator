#!/usr/bin/env python
"""Builds the Vibration Lab Suite USER GUIDE (PDF) from docs/src/suite/*.md using the same pipeline as the engine guide."""
import build_guide as g
import build_book as bb

g.TITLE = "Vibration Lab Suite: User Guide"
g.SUB = "Design a mechanical system, run it for as long as you like, take the data to Excel"
g.FOOT = "Vibration Lab Suite 1.0: User Guide"
g.CHAPTER_DIR = "suite"
g.OUT_NAME = "Vibration Lab Suite - User Guide.pdf"
bb.TITLE = g.TITLE


def cover():
    logo = ('<svg viewBox="0 0 220 120" width="330" height="180"><rect x="2" y="2" width="216" height="116" rx="12" fill="#fff" stroke="#000" stroke-width="4"/>'
            '<line x1="14" y1="30" x2="14" y2="92" stroke="#000" stroke-width="6"/>'
            '<polyline points="14,61 30,61 36,45 48,77 60,45 72,77 84,45 90,61 104,61" fill="none" stroke="#000" stroke-width="4"/>'
            '<rect x="104" y="38" width="46" height="46" fill="#bbb" stroke="#000" stroke-width="4"/>'
            '<polyline points="150,61 164,61 170,45 182,77 194,45 200,61 206,61" fill="none" stroke="#000" stroke-width="4"/>'
            '<line x1="206" y1="30" x2="206" y2="92" stroke="#000" stroke-width="6"/></svg>')
    return f"""
<div style="height:236mm;display:flex;flex-direction:column;justify-content:space-between;text-align:center;">
  <div style="border-top:6px solid #000;border-bottom:2px solid #000;padding:10px 0;font-size:11pt;letter-spacing:.4em;text-transform:uppercase;">User Guide</div>
  <div>
    <div style="display:flex;justify-content:center;margin-bottom:26px">{logo}</div>
    <div style="font-size:50pt;font-weight:bold;line-height:1.05;letter-spacing:.02em">VIBRATION<br>LAB SUITE</div>
    <div style="border-top:2px solid #000;width:60%;margin:28px auto 22px auto"></div>
    <div style="font-size:14.5pt;font-style:italic;line-height:1.45;padding:0 30px">Design a mechanical system, choose ideal or realistic physics, run it for as long as you like, and take every number to Excel with the charts already drawn.</div>
  </div>
  <div>
    <div style="font-size:22pt;letter-spacing:.2em;text-transform:uppercase;font-weight:bold">by Team NOVA</div>
    <div style="margin-top:10px;font-size:11pt;font-style:italic">Version 1.0 · 2026</div>
    <div style="border-top:2px solid #000;border-bottom:6px solid #000;margin-top:18px;padding:6px 0;font-size:9pt;letter-spacing:.08em;text-transform:uppercase">Springs · dampers · friction · collisions · energy ledger · Excel export · video</div>
  </div>
</div>"""


def copyright():
    return """<div style="page-break-before:always;height:232mm;display:flex;flex-direction:column;justify-content:flex-end;text-align:left;font-size:11pt;line-height:1.5">
<p><b>Vibration Lab Suite: User Guide</b><br>by Team NOVA</p>
<p>Version 1.0, 2026.</p>
<p>All screenshots were taken from the real program. Charts shown from Excel were exported by Microsoft Excel from workbooks that the program created. Every number quoted from the validation suite was produced by running it.</p>
<p>Excel and Windows are trademarks of Microsoft. Unreal Engine is a trademark of Epic Games. Other names belong to their owners.</p>
<p>Set in Times New Roman, 12 point, with 1.5 line spacing.</p></div>"""


g.cover_html = cover
g.copyright_html = copyright
g.main()
