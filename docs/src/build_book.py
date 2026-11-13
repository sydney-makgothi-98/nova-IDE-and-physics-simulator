#!/usr/bin/env python
"""Builds 'How to Build the NOVA C-IDE' (PDF) from Markdown chapters.

Pipeline: markdown -> HTML (with diagrams) -> run code samples for real output -> Electron printToPDF
          -> find heading pages -> build table of contents -> merge front matter + body -> bookmarks.
Usage:  python build_book.py            (full build)
        python build_book.py --nocheck  (skip running code samples)
"""
import os, re, sys, glob, html, subprocess, tempfile, shutil, json, hashlib
import markdown
import pymupdf as fitz  # PyMuPDF
from figures import render_fig, FIGS

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.abspath(os.path.join(HERE, ".."))
ROOT = os.path.abspath(os.path.join(DOCS, ".."))
OUT = os.path.join(DOCS, "out")
os.makedirs(OUT, exist_ok=True)
ELECTRON = os.path.join(ROOT, "node_modules", "electron", "dist", "electron.exe")
TITLE = "How to Build the NOVA C-IDE"
NOCHECK = "--nocheck" in sys.argv
CACHE_FILE = os.path.join(OUT, "run_cache.json")
CACHE = json.load(open(CACHE_FILE)) if os.path.exists(CACHE_FILE) else {}

CALL = {"why": "WHY?", "whatif": "WHAT IF IT WERE DIFFERENT?", "note": "NOTE", "tip": "TIP", "warn": "WATCH OUT", "try": "TRY IT",
        "def": "KEY WORD", "assign": "MINI ASSIGNMENT", "big": "BIG ASSIGNMENT", "quiz": "QUIZ", "answers": "ANSWERS", "check": "CHECKPOINT",
        "story": "IN PLAIN WORDS", "hint": "HINTS"}

stats = {"code_ok": 0, "code_fail": [], "figs": 0, "words": 0, "imgs": 0}


# ------------------------------------------------------------------ code execution
def run_code(lang, code):
    """Compile/run a snippet. Returns (ok, output)."""
    key = hashlib.sha1((lang + code).encode()).hexdigest()
    if key in CACHE:
        return tuple(CACHE[key])
    tmp = tempfile.mkdtemp(prefix="novabook_")
    try:
        if lang in ("c", "cpp"):
            src = os.path.join(tmp, "t." + ("c" if lang == "c" else "cpp"))
            open(src, "w", encoding="utf8").write(code)
            exe = os.path.join(tmp, "t.exe")
            cc = ["gcc", "-std=c17"] if lang == "c" else ["g++", "-std=c++20"]
            r = subprocess.run(cc + ["-Wall", "-o", exe, src], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
            if r.returncode != 0:
                return (False, r.stderr[-1500:])
            r = subprocess.run([exe], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=10, cwd=tmp, input="")
            res = (True, (r.stdout + r.stderr).replace("\r\n", "\n"))
        elif lang == "js":
            src = os.path.join(tmp, "t.js")
            open(src, "w", encoding="utf8").write(code)
            r = subprocess.run(["node", src], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=15, cwd=tmp)
            res = (r.returncode == 0, (r.stdout + r.stderr).replace("\r\n", "\n"))
        else:
            res = (True, "")
        CACHE[key] = list(res)
        return res
    except Exception as e:
        return (False, str(e))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check_only(lang, code):
    """Compile (no run)."""
    key = hashlib.sha1(("chk" + lang + code).encode()).hexdigest()
    if key in CACHE:
        return tuple(CACHE[key])
    tmp = tempfile.mkdtemp(prefix="novabook_")
    try:
        src = os.path.join(tmp, "t." + ("c" if lang == "c" else "cpp"))
        open(src, "w", encoding="utf8").write(code)
        cc = ["gcc", "-std=c17"] if lang == "c" else ["g++", "-std=c++20"]
        r = subprocess.run(cc + ["-fsyntax-only", src], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        res = (r.returncode == 0, r.stderr[-1500:])
        CACHE[key] = list(res)
        return res
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ------------------------------------------------------------------ markdown processing
class Book:
    def __init__(self):
        self.chap = 0
        self.sec = 0
        self.sub = 0
        self.fig = 0
        self.toc = []  # (level, label, anchor)
        self.appendix = 0
        self.body = []
        self.flag_unnum = False
        self.app_letter = None

    # --- helpers
    def code_block(self, lang, flags, code):
        code = code.rstrip("\n")
        shown = html.escape(code)
        out = ""
        if flags and not NOCHECK and lang in ("c", "cpp", "js"):
            if "run" in flags:
                ok, o = run_code(lang, code)
                if ok:
                    stats["code_ok"] += 1
                    o = o.rstrip("\n")
                    out = f'<div class="out"><div class="out-t">OUTPUT</div><pre>{html.escape(o) if o else "(no output)"}</pre></div>'
                else:
                    stats["code_fail"].append((self.chap, code[:80].replace("\n", " "), o[:600]))
            elif "check" in flags:
                ok, o = check_only(lang, code)
                if ok:
                    stats["code_ok"] += 1
                else:
                    stats["code_fail"].append((self.chap, code[:80].replace("\n", " "), o[:600]))
        label = {"c": "C", "cpp": "C++", "js": "JavaScript", "html": "HTML", "css": "CSS", "cmake": "CMake", "json": "JSON", "bash": "Shell", "text": ""}.get(lang, lang)
        cap = f'<div class="code-t">{label}</div>' if label else ""
        cls = "codewrap breakable" if len(code.splitlines()) > 16 else "codewrap"
        return f'<div class="{cls}">{cap}<pre class="code">{shown}</pre></div>{out}'

    def figure(self, kind, name, caption):
        self.fig += 1
        n = f"{self.chap if self.chap else 0}.{self.fig}"
        if kind == "fig":
            stats["figs"] += 1
            body = f'<div class="svgwrap">{render_fig(name)}</div>'
        else:
            stats["imgs"] += 1
            p = os.path.join(DOCS, "img", name).replace("\\", "/")
            body = f'<div class="imgwrap"><img src="file:///{p}"/></div>'
        return f'<figure>{body}<figcaption><b>Figure {n}.</b> {html.escape(caption)}</figcaption></figure>'

    def callout(self, kind, title, inner_html):
        lab = CALL.get(kind, kind.upper())
        sub = f' <span class="co-sub">{html.escape(title)}</span>' if title else ""
        return f'<div class="co co-{kind}"><div class="co-t">{lab}{sub}</div>{inner_html}</div>'

    def heading(self, level, text):
        if level == 1:
            self.sec = 0
            self.fig = 0
            m = re.match(r"^(Appendix [A-Z])\s*[:.-]?\s*(.*)$", text)
            aid = f"c{len(self.toc)}"
            if m:
                label, title = m.group(1), m.group(2)
                self.app_letter = label[-1]
                self.toc.append((1, f"{label}.  {title}", aid, f"{label} {title}"))
                return f'<h1 class="chapter" id="{aid}"><span class="cn">{label}</span><span class="ct">{html.escape(title)}</span></h1>'
            if self.flag_unnum:
                self.flag_unnum = False
                self.toc.append((1, text, aid, text))
                return f'<h1 class="chapter" id="{aid}"><span class="cn">&nbsp;</span><span class="ct">{html.escape(text)}</span></h1>'
            self.chap += 1
            self.toc.append((1, f"Chapter {self.chap}.  {text}", aid, f"CHAPTER {self.chap} {text}"))
            return f'<h1 class="chapter" id="{aid}"><span class="cn">Chapter {self.chap}</span><span class="ct">{html.escape(text)}</span></h1>'
        if level == 2:
            self.sec += 1
            self.sub = 0
            aid = f"s{len(self.toc)}"
            lab = f"{self.app_letter}.{self.sec}" if self.app_letter else (f"{self.chap}.{self.sec}" if self.chap else "")
            full = f"{lab}  {text}" if lab else text
            self.toc.append((2, full, aid, full))
            return f'<h2 id="{aid}">{html.escape(full)}</h2>'
        self.sub += 1
        return f'<h3>{html.escape(text)}</h3>'

    def convert(self, text, fname):
        # 1) fenced code
        codes = []

        def grab_code(m):
            info = (m.group(1) or "").split()
            lang = info[0] if info else ""
            flags = info[1:]
            codes.append((lang, flags, m.group(2)))
            return f"\n\n@@CODE{len(codes) - 1}@@\n\n"
        text = re.sub(r"^```([^\n]*)\n(.*?)^```[ \t]*$", grab_code, text, flags=re.S | re.M)
        # 2) callouts
        cos = []

        def grab_co(m):
            cos.append((m.group(1), (m.group(2) or "").strip(), m.group(3)))
            return f"\n\n@@CO{len(cos) - 1}@@\n\n"
        text = re.sub(r"^:::[ \t]*(\w+)[ \t]*([^\n]*)\n(.*?)^:::[ \t]*$", grab_co, text, flags=re.S | re.M)
        # 3) line directives
        out_lines = []
        for ln in text.split("\n"):
            s = ln.strip()
            if s.startswith("@@part "):
                t = s[7:]
                aid = f"p{len(self.toc)}"
                self.toc.append((0, t, aid, t.upper()))
                out_lines.append(f'\n\n<div class="part" id="{aid}"><div class="part-inner">{html.escape(t)}</div></div>\n\n')
            elif s == "@@unnumbered":
                self.flag_unnum = True
            elif s == "@@pagebreak":
                out_lines.append('\n\n<div class="pb"></div>\n\n')
            elif re.match(r"^\{\{(fig|img):", s):
                m = re.match(r"^\{\{(fig|img):([^|}]+)\|([^}]*)\}\}$", s)
                out_lines.append(f"\n\n{self.figure(m.group(1), m.group(2).strip(), m.group(3).strip())}\n\n")
            elif re.match(r"^#{1,3} ", ln):
                lv = len(ln.split(" ")[0])
                out_lines.append(f"\n\n{self.heading(lv, ln[lv + 1:].strip())}\n\n")
            else:
                out_lines.append(ln)
        text = "\n".join(out_lines)
        stats["words"] += len(re.findall(r"\w+", re.sub(r"@@\w+", "", text)))
        md = markdown.Markdown(extensions=["tables", "sane_lists", "attr_list"])

        def mdconv(t):
            md.reset()
            return md.convert(t)

        body = mdconv(text)

        def restore_code(t):
            return re.sub(r"(?:<p>)?@@CODE(\d+)@@(?:</p>)?", lambda m: self.code_block(*codes[int(m.group(1))]), t)

        def restore_co(m):
            kind, title, inner = cos[int(m.group(1))]
            inner_html = restore_code(mdconv(inner))
            stats["words"] += len(re.findall(r"\w+", inner))
            if kind == "quiz" or kind == "answers":
                pass
            return self.callout(kind, title, inner_html)
        body = restore_code(body)
        body = re.sub(r"(?:<p>)?@@CO(\d+)@@(?:</p>)?", restore_co, body)
        return body


CSS = r"""
* { box-sizing: border-box; }
html { background: #fff; }
body { font-family: 'Times New Roman', Times, serif; font-size: 12pt; line-height: 1.5; color: #000; margin: 0; text-align: justify; hyphens: auto; }
p { margin: 0 0 0.55em 0; orphans: 3; widows: 3; }
h1.chapter { page-break-before: always; break-before: page; margin: 0 0 1.2em 0; padding: 0 0 .35em 0; border-bottom: 3px double #000; text-align: left; line-height: 1.15; }
h1.chapter .cn { display: block; font-size: 15pt; font-weight: normal; letter-spacing: .25em; text-transform: uppercase; margin-top: 1.2em; margin-bottom: .2em; }
h1.chapter .ct { display: block; font-size: 28pt; font-weight: bold; }
h2 { font-size: 17pt; margin: 1.4em 0 .4em 0; line-height: 1.2; page-break-after: avoid; break-after: avoid; text-align: left; border-bottom: 1px solid #000; padding-bottom: 2px; }
h3 { font-size: 13.5pt; font-style: italic; margin: 1.1em 0 .3em 0; page-break-after: avoid; break-after: avoid; text-align: left; }
ul, ol { margin: 0 0 .6em 0; padding-left: 1.6em; } li { margin-bottom: .15em; }
code { font-family: 'Courier New', Courier, monospace; font-size: 10.5pt; border: .5px solid #888; padding: 0 2px; background: #f2f2f2; }
.codewrap { margin: .5em 0 .8em 0; page-break-inside: avoid; break-inside: avoid; text-align: left; border: 1.2px solid #000; }
.codewrap.breakable { page-break-inside: auto; break-inside: auto; }
.code-t { font-size: 8.5pt; letter-spacing: .12em; text-transform: uppercase; font-weight: bold; background: #000; color: #fff; padding: 0 8px; line-height: 1.5; }
pre.code { margin: 0; padding: 6px 10px; font-family: 'Courier New', Courier, monospace; font-size: 9.2pt; line-height: 1.28; white-space: pre-wrap; word-break: break-word; background: #f5f5f5; }
pre.code code, .out pre code { border: 0; padding: 0; background: none; font-size: inherit; }
.out { margin: -.55em 0 .9em 0; border: 1.2px solid #000; border-top: 0; page-break-inside: avoid; break-inside: avoid; text-align: left; }
.out-t { font-size: 8pt; letter-spacing: .12em; font-weight: bold; padding: 0 8px; border-bottom: 1px dashed #000; background: #e4e4e4; line-height: 1.5; }
.out pre { margin: 0; padding: 5px 10px; font-family: 'Courier New', Courier, monospace; font-size: 9.2pt; line-height: 1.25; white-space: pre-wrap; background: #fff; }
figure { margin: 1em 0 1.1em 0; page-break-inside: avoid; break-inside: avoid; text-align: center; }
.svgwrap, .imgwrap { border: 1px solid #000; padding: 6px; background: #fff; }
.imgwrap img { width: 100%; display: block; }
figcaption { font-size: 10.5pt; line-height: 1.3; margin-top: .35em; font-style: italic; text-align: center; }
figcaption b { font-style: normal; }
.co { margin: .8em 0 1em 0; padding: 6px 12px 4px 12px; border: 1.2px solid #000; text-align: left; page-break-inside: avoid; break-inside: avoid; font-size: 11.5pt; line-height: 1.42; }
.co p:last-child, .co ul:last-child, .co ol:last-child { margin-bottom: .3em; }
.co-t { font-size: 9pt; font-weight: bold; letter-spacing: .14em; margin-bottom: .2em; }
.co-sub { letter-spacing: 0; font-size: 11.5pt; margin-left: .5em; font-style: italic; text-transform: none; }
.co-why { border-left: 7px solid #000; background: #f7f7f7; }
.co-whatif { border-style: dashed; border-width: 1.6px; border-left: 7px double #000; }
.co-note, .co-def, .co-story { background: #fff; border-left: 4px solid #000; }
.co-tip { background: #efefef; border-left: 4px solid #000; border-style: solid; }
.co-warn { border: 2.4px solid #000; background: #fff; }
.co-try { border: 1.2px solid #000; border-left: 7px solid #000; background: #fafafa; }
.co-check { background: #efefef; border: 1.2px solid #000; border-left: 7px double #000; }
.co-hint { border-style: dotted; }
.co-assign { border: 1.6px solid #000; border-radius: 0; background: #f2f2f2; border-top: 6px solid #000; }
.co-big { border: 3px double #000; border-top: 8px solid #000; background: #f2f2f2; page-break-inside: auto; break-inside: auto; }
.co-quiz { border: 1.6px solid #000; background: #fff; border-top: 6px double #000; page-break-inside: auto; break-inside: auto; }
.co-answers { border: 1.6px dashed #000; background: #fafafa; page-break-inside: auto; break-inside: auto; }
.co-big .co-t, .co-assign .co-t, .co-quiz .co-t { font-size: 10pt; }
table { border-collapse: collapse; width: 100%; margin: .6em 0 1em 0; font-size: 10.5pt; line-height: 1.3; text-align: left; page-break-inside: auto; }
th { background: #000; color: #fff; padding: 4px 7px; border: 1px solid #000; font-size: 10pt; }
td { padding: 4px 7px; border: 1px solid #000; vertical-align: top; }
tr { page-break-inside: avoid; break-inside: avoid; }
tr:nth-child(even) td { background: #f2f2f2; }
.part { page-break-before: always; break-before: page; page-break-after: always; break-after: page; height: 235mm; display: flex; align-items: center; justify-content: center; text-align: center; }
.part-inner { font-size: 30pt; font-weight: bold; line-height: 1.3; border-top: 4px solid #000; border-bottom: 4px solid #000; padding: 40px 30px; letter-spacing: .02em; }
.pb { page-break-after: always; break-after: page; }
hr { border: 0; border-top: 1px solid #000; margin: 1em 0; }
blockquote { margin: .6em 1.5em; padding-left: 1em; border-left: 3px solid #000; font-style: italic; }
.toc h1 { font-size: 26pt; border-bottom: 3px double #000; margin: 0 0 .5em 0; text-align: left; }
.toc .row { display: flex; align-items: baseline; text-align: left; line-height: 1.38; }
.toc .row .dots { flex: 1; border-bottom: 1px dotted #000; margin: 0 6px; transform: translateY(-3px); }
.toc .l0 { font-weight: bold; font-size: 12.5pt; margin-top: .7em; text-transform: uppercase; letter-spacing: .05em; }
.toc .l1 { font-size: 11.5pt; font-weight: bold; margin-top: .25em; }
.toc .l2 { font-size: 10pt; padding-left: 1.8em; }
"""


def page(body, extra_css=""):
    return f'<!doctype html><html><head><meta charset="utf-8"><title>{TITLE}</title><style>{CSS}{extra_css}</style></head><body>{body}</body></html>'


def cover_html():
    logo = ('<svg viewBox="0 0 120 120" width="150" height="150"><rect x="4" y="4" width="112" height="112" rx="26" fill="#fff" stroke="#000" stroke-width="5"/>'
            '<path d="M34 86V34l52 52V34" fill="none" stroke="#000" stroke-width="12" stroke-linecap="round" stroke-linejoin="round"/></svg>')
    return f"""
<div style="height:236mm;display:flex;flex-direction:column;justify-content:space-between;text-align:center;">
  <div style="border-top:6px solid #000;border-bottom:2px solid #000;padding:10px 0;font-size:11pt;letter-spacing:.4em;text-transform:uppercase;">A Complete Guide</div>
  <div>
    <div style="display:flex;justify-content:center;margin-bottom:26px">{logo}</div>
    <div style="font-size:15pt;letter-spacing:.3em;text-transform:uppercase;margin-bottom:10px">How to Build the</div>
    <div style="font-size:52pt;font-weight:bold;line-height:1.05;letter-spacing:.02em">NOVA<br>C-IDE</div>
    <div style="border-top:2px solid #000;width:60%;margin:28px auto 22px auto"></div>
    <div style="font-size:15pt;font-style:italic;line-height:1.45;padding:0 30px">From a blank folder to a working C and C++ programming environment —<br>editor, build tools, debugger, language server and terminal —<br>explained in plain English, one step at a time.</div>
  </div>
  <div>
    <div style="font-size:22pt;letter-spacing:.2em;text-transform:uppercase;font-weight:bold">by Team NOVA</div>
    <div style="margin-top:10px;font-size:11pt;font-style:italic">First edition · 2026</div>
    <div style="border-top:2px solid #000;border-bottom:6px solid #000;margin-top:18px;padding:6px 0;font-size:9pt;letter-spacing:.08em;text-transform:uppercase">HTML · CSS · JavaScript · Node.js · Electron · C · C++ · CMake · JSON</div>
  </div>
</div>"""


def copyright_html():
    return f"""<div style="page-break-before:always;height:232mm;display:flex;flex-direction:column;justify-content:flex-end;text-align:left;font-size:11pt;line-height:1.5">
<p><b>How to Build the NOVA C-IDE</b><br>by Team NOVA</p>
<p>First edition, 2026.</p>
<p>NOVA IDE is an in-house tool. This book describes how it was built so that anyone on the team can understand it, rebuild it, change it and grow it.</p>
<p>All screenshots in this book were taken from the real NOVA IDE running on Windows 11 and were converted to black and white for printing. Every code sample marked with an <b>OUTPUT</b> box was compiled and run while this book was being made, so the output you see is the real output.</p>
<p>Trademarks: CLion and JetBrains are trademarks of JetBrains s.r.o. NOVA was designed from public documentation and its own ideas. It contains no JetBrains code. Visual Studio Code, Monaco, Electron, xterm.js, clangd, LLVM, GDB, CMake and other names belong to their respective owners.</p>
<p>Set in Times New Roman, 12 point, with 1.5 line spacing.</p></div>"""


# ------------------------------------------------------------------ rendering
def electron_pdf(html_path, pdf_path, footer):
    env = dict(os.environ)
    env.pop("ELECTRON_RUN_AS_NODE", None)
    r = subprocess.run([ELECTRON, os.path.join(HERE, "render_pdf.js"), html_path, pdf_path, "1" if footer else "0"], env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=600)
    if not os.path.exists(pdf_path):
        raise RuntimeError("PDF render failed: " + r.stderr[-800:])


def find_pages(pdf_path, toc):
    """Return list of printed page numbers (1-based within body) for each toc entry."""
    d = fitz.open(pdf_path)
    texts = [re.sub(r"\s+", " ", p.get_text()) for p in d]
    res, cur = [], 0
    for lvl, label, aid, needle in toc:
        n = re.sub(r"\s+", " ", needle).strip()
        found = None
        for i in range(cur, len(texts)):
            t = texts[i]
            if re.sub(r"\s", "", n.lower()) in re.sub(r"\s", "", t.lower()):
                found = i; break
        if found is None:
            found = cur
        cur = found
        res.append(found + 1)
    return res, len(d)


def toc_html(toc, pages):
    rows = []
    for (lvl, label, aid, needle), pg in zip(toc, pages):
        rows.append(f'<div class="row l{lvl}"><span>{html.escape(label)}</span><span class="dots"></span><span>{pg}</span></div>')
    return '<div class="toc"><h1>Contents</h1>' + "".join(rows) + "</div>"


def main():
    chapters = sorted(glob.glob(os.path.join(HERE, "chapters", "*.md")))
    b = Book()
    parts = []
    for f in chapters:
        parts.append(b.convert(open(f, encoding="utf8").read(), os.path.basename(f)))
    body_html = page("\n".join(parts))
    body_path = os.path.join(OUT, "body.html")
    open(body_path, "w", encoding="utf8").write(body_html)
    print(f"markdown converted: {len(chapters)} files, {stats['words']:,} words, {stats['figs']} diagrams, {stats['imgs']} screenshots, {stats['code_ok']} code samples verified")
    for ch, snip, err in stats["code_fail"]:
        print(f"!! CODE FAILED (chapter {ch}): {snip}\n   {err}")
    json.dump(CACHE, open(CACHE_FILE, "w"))

    body_pdf = os.path.join(OUT, "body.pdf")
    electron_pdf(body_path, body_pdf, True)
    toc = b.toc
    pages, npages = find_pages(body_pdf, toc)

    # TOC (twice: page count must be stable)
    toc_path = os.path.join(OUT, "front.html")
    front_html = page(cover_html() + copyright_html() + '<div style="page-break-before:always"></div>' + toc_html(toc, pages))
    open(toc_path, "w", encoding="utf8").write(front_html)
    front_pdf = os.path.join(OUT, "front.pdf")
    electron_pdf(toc_path, front_pdf, False)

    final = fitz.open()
    front = fitz.open(front_pdf)
    body = fitz.open(body_pdf)
    final.insert_pdf(front)
    off = len(front)
    final.insert_pdf(body)
    bm, prev = [[1, "Cover", 1]], 1
    for (lvl, label, aid, needle), pg in zip(toc, pages):
        lv = min({0: 1, 1: 2, 2: 3}[lvl], prev + 1)
        bm.append([lv, re.sub(r"\s+", " ", label), pg + off])
        prev = lv
    final.set_toc(bm)
    final.set_metadata({"title": TITLE, "author": "Team NOVA", "subject": "Building a C and C++ IDE from scratch", "keywords": "IDE, C, C++, JavaScript, Electron, CMake, gdb, clangd"})
    out_pdf = os.path.join(DOCS, "How to Build the NOVA C-IDE - Team NOVA.pdf")
    final.save(out_pdf, garbage=4, deflate=True)
    print(f"DONE: {len(final)} pages ({len(front)} front + {npages} body) -> {out_pdf}  [{os.path.getsize(out_pdf) / 1e6:.1f} MB]")


if __name__ == "__main__":
    main()
