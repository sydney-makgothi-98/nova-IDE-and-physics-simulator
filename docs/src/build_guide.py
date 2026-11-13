#!/usr/bin/env python
"""Builds the Vibration Lab engine guide (PDF) from Markdown chapters in docs/src/guide/.

Re-uses the book machinery in build_book.py (callouts, figures, TOC, Electron printToPDF) and adds:
  @@src <file> | <start regex> | <end regex or +N>     -> a numbered, REAL source listing taken from the code
so the listings in the guide can never drift away from the program.
"""
import os, re, sys, glob, html, subprocess, json
import pymupdf as fitz
import build_book as bb
import guide_figures

HERE = bb.HERE
DOCS = bb.DOCS
ROOT = bb.ROOT
OUT = bb.OUT
LAB = os.path.join(ROOT, "examples", "vibration-lab")
TITLE = "Inside the Vibration Lab"
SUB = "Building a Lagrangian physics engine, a design interface and a video recorder"
FOOT = "Inside the Vibration Lab — Team NOVA"
CHAPTER_DIR = "guide"
OUT_NAME = "Inside the Vibration Lab - Team NOVA.pdf"

bb.render_fig = guide_figures.render_fig       # the book's figure() now draws OUR diagrams
bb.TITLE = TITLE
bb.CALL.update({"eq": "EQUATION", "mapx": "MATHS TO CODE"})

EXTRA_CSS = r"""
.eq { text-align: center; font-style: italic; font-size: 12.5pt; margin: .55em 1em .75em 1em; padding: 4px 0; border-top: .6px solid #888; border-bottom: .6px solid #888; page-break-inside: avoid; }
.eq .tag { float: right; font-style: normal; font-size: 10pt; }
pre.code.src { font-size: 7.9pt; line-height: 1.24; }
.co-mapx { border: 1.4px solid #000; border-left: 7px solid #000; background: #f7f7f7; }
td code, li code, p code { font-size: 9.6pt; }
.legend td { font-size: 10pt; }
"""


# ------------------------------------------------------------------ real source listings
_cache = {}


def src_lines(name):
    if name not in _cache:
        _cache[name] = open(os.path.join(LAB, name), encoding="utf-8").read().split("\n")
    return _cache[name]


def expand_src(text):
    out = []
    for ln in text.split("\n"):
        if not ln.startswith("@@src "):
            out.append(ln)
            continue
        name, start, end = [p.strip() for p in ln[6:].split("|")]
        L = src_lines(name)
        a = next((i for i, l in enumerate(L) if re.search(start, l)), None)
        if a is None:
            raise SystemExit(f"@@src: start pattern not found in {name}: {start}")
        if end.startswith("+"):
            b = min(len(L) - 1, a + int(end[1:]) - 1)
        else:
            b = next((i for i in range(a, len(L)) if re.search(end, L[i])), None)
            if b is None:
                raise SystemExit(f"@@src: end pattern not found in {name}: {end}")
        body = "\n".join(f"{i + 1:4d}  {L[i]}" for i in range(a, b + 1))
        out.append(f"```src\n{name}   (lines {a + 1} to {b + 1})\n{body}\n```")
    return "\n".join(out)


class GuideBook(bb.Book):
    def code_block(self, lang, flags, code):
        if lang == "src":
            first, _, rest = code.partition("\n")
            return (f'<div class="codewrap breakable"><div class="code-t">{html.escape(first)}</div>'
                    f'<pre class="code src">{html.escape(rest.rstrip())}</pre></div>')
        return super().code_block(lang, flags, code)

    def convert(self, text, fname):
        return super().convert(expand_src(text), fname)


# ------------------------------------------------------------------ cover pages
def cover_html():
    logo = ('<svg viewBox="0 0 220 120" width="330" height="180"><rect x="2" y="2" width="216" height="116" rx="12" fill="#fff" stroke="#000" stroke-width="4"/>'
            '<line x1="14" y1="30" x2="14" y2="92" stroke="#000" stroke-width="6"/>'
            '<polyline points="14,61 30,61 36,45 48,77 60,45 72,77 84,45 90,61 104,61" fill="none" stroke="#000" stroke-width="4"/>'
            '<rect x="104" y="38" width="46" height="46" fill="#bbb" stroke="#000" stroke-width="4"/>'
            '<polyline points="150,61 164,61 170,45 182,77 194,45 200,61 206,61" fill="none" stroke="#000" stroke-width="4"/>'
            '<line x1="206" y1="30" x2="206" y2="92" stroke="#000" stroke-width="6"/></svg>')
    return f"""
<div style="height:236mm;display:flex;flex-direction:column;justify-content:space-between;text-align:center;">
  <div style="border-top:6px solid #000;border-bottom:2px solid #000;padding:10px 0;font-size:11pt;letter-spacing:.4em;text-transform:uppercase;">An Engineering Guide</div>
  <div>
    <div style="display:flex;justify-content:center;margin-bottom:26px">{logo}</div>
    <div style="font-size:15pt;letter-spacing:.3em;text-transform:uppercase;margin-bottom:10px">Inside the</div>
    <div style="font-size:50pt;font-weight:bold;line-height:1.05;letter-spacing:.02em">VIBRATION<br>LAB</div>
    <div style="border-top:2px solid #000;width:60%;margin:28px auto 22px auto"></div>
    <div style="font-size:14.5pt;font-style:italic;line-height:1.45;padding:0 30px">How a Lagrangian physics engine, a click-and-draw design interface and a direct-to-video recorder were built — every equation derived, every function explained, every line accounted for.</div>
  </div>
  <div>
    <div style="font-size:22pt;letter-spacing:.2em;text-transform:uppercase;font-weight:bold">by Team NOVA</div>
    <div style="margin-top:10px;font-size:11pt;font-style:italic">First edition · 2026</div>
    <div style="border-top:2px solid #000;border-bottom:6px solid #000;margin-top:18px;padding:6px 0;font-size:9pt;letter-spacing:.08em;text-transform:uppercase">Lagrangian mechanics · RK4 · C++17 · Win32 · GDI+ · Media Foundation · H.264</div>
  </div>
</div>"""


def copyright_html():
    return """<div style="page-break-before:always;height:232mm;display:flex;flex-direction:column;justify-content:flex-end;text-align:left;font-size:11pt;line-height:1.5">
<p><b>Inside the Vibration Lab</b><br>by Team NOVA</p>
<p>First edition, 2026.</p>
<p>This guide describes the Vibration Lab program that lives in the folder <i>examples/vibration-lab</i> of the NOVA IDE project. It is a companion to <i>How to Build the NOVA C-IDE</i>.</p>
<p>Every source listing in this guide is copied automatically from the real files at the moment the PDF is built, so the listings always match the program. Every number printed as a test result was produced by running the program's own self-test. All screenshots are real, taken from the program itself and converted to black and white for printing.</p>
<p>Names such as Windows, Media Foundation, H.264, GDI+ and MPEG-4 belong to their respective owners.</p>
<p>Set in Times New Roman, 12 point, with 1.5 line spacing.</p></div>"""


def electron_pdf(html_path, pdf_path, footer):
    env = dict(os.environ)
    env.pop("ELECTRON_RUN_AS_NODE", None)
    r = subprocess.run([bb.ELECTRON, os.path.join(HERE, "render_pdf.js"), html_path, pdf_path, "1" if footer else "0", FOOT], env=env,
                       capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=900)
    if not os.path.exists(pdf_path):
        raise RuntimeError("PDF render failed: " + r.stderr[-800:])


def main():
    chapters = sorted(glob.glob(os.path.join(HERE, CHAPTER_DIR, "*.md")))
    b = GuideBook()
    parts = [b.convert(open(f, encoding="utf8").read(), os.path.basename(f)) for f in chapters]
    body_path = os.path.join(OUT, "guide_body.html")
    open(body_path, "w", encoding="utf8").write(bb.page("\n".join(parts), EXTRA_CSS))
    st = bb.stats
    print(f"markdown converted: {len(chapters)} files, {st['words']:,} words, {st['figs']} diagrams, {st['imgs']} screenshots")

    body_pdf = os.path.join(OUT, "guide_body.pdf")
    electron_pdf(body_path, body_pdf, True)
    pages, npages = bb.find_pages(body_pdf, b.toc)

    front_path = os.path.join(OUT, "guide_front.html")
    open(front_path, "w", encoding="utf8").write(bb.page(cover_html() + copyright_html() + '<div style="page-break-before:always"></div>' + bb.toc_html(b.toc, pages), EXTRA_CSS))
    front_pdf = os.path.join(OUT, "guide_front.pdf")
    electron_pdf(front_path, front_pdf, False)

    final = fitz.open()
    front = fitz.open(front_pdf)
    body = fitz.open(body_pdf)
    final.insert_pdf(front)
    off = len(front)
    final.insert_pdf(body)
    bm, prev = [[1, "Cover", 1]], 1
    for (lvl, label, aid, needle), pg in zip(b.toc, pages):
        lv = min({0: 1, 1: 2, 2: 3}[lvl], prev + 1)
        bm.append([lv, re.sub(r"\s+", " ", label), pg + off])
        prev = lv
    final.set_toc(bm)
    final.set_metadata({"title": TITLE + ": " + SUB, "author": "Team NOVA", "subject": SUB,
                        "keywords": "Lagrangian mechanics, spring mass damper, RK4, C++, Win32, GDI+, Media Foundation, H.264"})
    out_pdf = os.path.join(DOCS, OUT_NAME)
    final.save(out_pdf, garbage=4, deflate=True)
    print(f"DONE: {len(final)} pages ({len(front)} front + {npages} body) -> {out_pdf}  [{os.path.getsize(out_pdf) / 1e6:.1f} MB]")


if __name__ == "__main__":
    main()
