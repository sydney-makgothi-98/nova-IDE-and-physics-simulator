"""Generates every black-and-white diagram in the book as inline SVG strings."""
import os, html

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FONT = "'Times New Roman', Times, serif"
MONO = "'Courier New', Courier, monospace"


class F:
    def __init__(s, w, h):
        s.w, s.h, s.o = w, h, []

    def _t(s, x, y, t, fs=13, anchor="middle", bold=False, mono=False, italic=False, lh=None):
        lh = lh or fs * 1.22
        lines = str(t).split("\n")
        y0 = y - (len(lines) - 1) * lh / 2 + fs * 0.35
        fam = MONO if mono else FONT
        out = []
        for i, ln in enumerate(lines):
            out.append(
                f'<text x="{x}" y="{y0 + i * lh:.1f}" font-family="{fam}" font-size="{fs}" text-anchor="{anchor}" '
                f'{"font-weight=\"bold\"" if bold else ""} {"font-style=\"italic\"" if italic else ""} fill="#000">{html.escape(ln)}</text>')
        return "".join(out)

    def text(s, x, y, t, **k):
        s.o.append(s._t(x, y, t, **k))

    def box(s, x, y, w, h, t="", fs=13, fill="#fff", dash=False, bold=False, r=5, mono=False, sw=1.4, italic=False, anchor="middle"):
        d = ' stroke-dasharray="5,4"' if dash else ""
        s.o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="#000" stroke-width="{sw}"{d}/>')
        if t:
            tx = x + w / 2 if anchor == "middle" else x + 8
            s.o.append(s._t(tx, y + h / 2, t, fs=fs, bold=bold, mono=mono, italic=italic, anchor=anchor))

    def line(s, x1, y1, x2, y2, arrow=False, both=False, dash=False, sw=1.4):
        d = ' stroke-dasharray="5,4"' if dash else ""
        m = ' marker-end="url(#ar)"' if arrow else ""
        m2 = ' marker-start="url(#ar2)"' if both else ""
        s.o.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#000" stroke-width="{sw}"{d}{m}{m2}/>')

    def poly(s, pts, arrow=False, dash=False, sw=1.4, fill="none"):
        d = ' stroke-dasharray="5,4"' if dash else ""
        m = ' marker-end="url(#ar)"' if arrow else ""
        p = " ".join(f"{x},{y}" for x, y in pts)
        s.o.append(f'<polyline points="{p}" fill="{fill}" stroke="#000" stroke-width="{sw}"{d}{m}/>')

    def circle(s, x, y, r, t="", fill="#fff", fs=12, bold=False):
        s.o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="#000" stroke-width="1.4"/>')
        if t:
            s.o.append(s._t(x, y, t, fs=fs, bold=bold))

    def raw(s, x):
        s.o.append(x)

    def svg(s):
        defs = ('<defs>'
                '<marker id="ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="#000"/></marker>'
                '<marker id="ar2" viewBox="0 0 10 10" refX="1" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#000"/></marker>'
                '<pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="6" stroke="#000" stroke-width="1"/></pattern>'
                '<pattern id="dots" width="6" height="6" patternUnits="userSpaceOnUse"><circle cx="3" cy="3" r="1" fill="#000"/></pattern>'
                '</defs>')
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {s.w} {s.h}" width="100%" role="img">{defs}'
                f'<rect x="0" y="0" width="{s.w}" height="{s.h}" fill="#fff"/>{"".join(s.o)}</svg>')


G1, G2, G3 = "#f2f2f2", "#dcdcdc", "#bdbdbd"
FIGS = {}


def fig(name):
    def deco(fn):
        FIGS[name] = fn
        return fn
    return deco


def seq(f, actors, xs, top=30, bottom=None):
    bottom = bottom or f.h - 20
    for a, x in zip(actors, xs):
        f.box(x - 62, top - 22, 124, 30, a, fs=13, bold=True, fill=G2)
        f.line(x, top + 8, x, bottom, dash=True, sw=1)


# ---------------------------------------------------------------- PART I
@fig("ide-vs-editor")
def _():
    f = F(600, 330)
    layers = [("IDE", 0, G1), ("+ Intelligence (completion, errors, refactor)", 28, G1), ("+ Debugger (stop, step, inspect)", 56, G2),
              ("+ Build tools (compile, run, tests)", 84, G2), ("Text editor (type, save, search)", 112, G3)]
    for t, i, fl in layers:
        f.box(20 + i * 0.0, 10 + i, 560 - 0, 310 - i * 2 + 0, "", fill=fl, r=10) if False else None
    # concentric rounded rectangles
    sizes = [(10, 10, 580, 310), (50, 50, 500, 255), (90, 90, 420, 200), (130, 130, 340, 145)]
    fills = ["#fff", G1, G2, G3]
    for (x, y, w, h), fl in zip(sizes, fills):
        f.box(x, y, w, h, "", fill=fl, r=12)
    f.box(170, 170, 260, 65, "Text editor\ntype, save, search", fs=14, bold=True, fill="#fff")
    f.text(300, 150, "+ Build tools: compile and run", fs=13)
    f.text(300, 110, "+ Debugger: stop, step, look inside", fs=13)
    f.text(300, 68, "+ Intelligence: completion, error hints, rename", fs=13)
    f.text(300, 28, "A complete IDE  (all of the above, in one window)", fs=14, bold=True)
    return f


@fig("ide-anatomy")
def _():
    f = F(600, 400)
    f.box(10, 10, 580, 380, "", fill="#fff", r=8, sw=2)
    f.box(10, 10, 580, 36, "", fill=G2, r=8)
    f.text(300, 28, "1  Title bar and run toolbar", fs=13, bold=True)
    f.box(10, 46, 36, 320, "", fill=G2, r=0)
    f.text(28, 190, "2\nTool\nstripe", fs=11, bold=True)
    f.box(46, 46, 130, 320, "", fill=G1, r=0)
    f.text(111, 110, "3\nProject\ntree", fs=13, bold=True)
    for i, t in enumerate(["src", "  main.cpp", "  shapes.cpp", "include", "CMakeLists.txt"]):
        f.text(58, 150 + i * 20, t, fs=11, anchor="start", mono=True)
    f.box(176, 46, 414, 28, "", fill=G2, r=0)
    f.text(383, 60, "4  Editor tabs  (main.cpp  shapes.h)", fs=13, bold=True)
    f.box(176, 74, 414, 190, "", fill="#fff", r=0)
    f.text(383, 135, "5  The code editor", fs=15, bold=True)
    for i, t in enumerate(["int main() {", "    int x = 5;", "    return 0;", "}"]):
        f.text(200, 170 + i * 18, t, fs=12, anchor="start", mono=True)
    f.box(176, 264, 414, 102, "", fill=G1, r=0)
    f.text(383, 285, "6  Bottom panel:  Run | Debug | Problems | Terminal", fs=13, bold=True)
    f.text(200, 322, "Process finished with exit code 0", fs=11, anchor="start", mono=True)
    f.box(10, 366, 580, 24, "", fill=G2, r=0)
    f.text(300, 378, "7  Status bar  (line:column, compiler, language)", fs=13, bold=True)
    return f


@fig("compile-pipeline")
def _():
    f = F(600, 230)
    items = [("Source\ncode\nmain.cpp", 20), ("Preprocessor\n#include\n#define", 130), ("Compiler\nturns C++\ninto assembly", 240), ("Assembler\nmakes\nmachine code", 350), ("Linker\njoins pieces\n+ libraries", 460)]
    for t, x in items:
        f.box(x, 50, 105, 78, t, fs=12, fill=G1)
    for i in range(4):
        f.line(items[i][1] + 105 if False else items[i][1] + 105, 89, items[i + 1][1], 89, arrow=True)
    f.text(72, 150, "main.cpp", fs=11, mono=True)
    f.text(182, 150, "expanded text", fs=11, italic=True)
    f.text(292, 150, "main.s", fs=11, mono=True)
    f.text(402, 150, "main.o", fs=11, mono=True)
    f.text(512, 150, "hello.exe", fs=11, mono=True, bold=True)
    f.box(20, 180, 560, 36, "g++ main.cpp -o hello.exe   does ALL of these steps for you", fs=13, mono=False, fill="#fff", dash=True, italic=True)
    return f


@fig("interp-vs-comp")
def _():
    f = F(600, 330)
    f.text(300, 18, "Three ways of turning code into action", fs=14, bold=True)
    # compiler lane
    f.text(14, 58, "Compiler", fs=13, bold=True, anchor="start")
    f.box(110, 40, 100, 36, "Source", fill=G1); f.box(250, 40, 140, 36, "Translate ALL\nof it once", fill=G2); f.box(430, 40, 100, 36, "Program\nfile (.exe)", fill=G1)
    f.line(210, 58, 250, 58, arrow=True); f.line(390, 58, 430, 58, arrow=True)
    f.text(300, 94, "Run the file many times, fast.   Examples: C, C++, Rust, Go", fs=12, italic=True)
    # interpreter lane
    f.text(14, 168, "Interpreter", fs=13, bold=True, anchor="start")
    f.box(110, 150, 100, 36, "Source", fill=G1); f.box(250, 150, 140, 36, "Read one line,\nrun it, repeat", fill=G2); f.box(430, 150, 100, 36, "Result\nappears", fill=G1)
    f.line(210, 168, 250, 168, arrow=True); f.line(390, 168, 430, 168, arrow=True)
    f.poly([(320, 190), (320, 206), (360, 206), (360, 190)], arrow=True)
    f.text(300, 224, "No separate file. Starts instantly.   Examples: Python, Ruby, early JavaScript", fs=12, italic=True)
    # hybrid
    f.text(14, 278, "Hybrid (JIT)", fs=13, bold=True, anchor="start")
    f.box(110, 260, 100, 36, "Source", fill=G1); f.box(250, 260, 140, 36, "Interpret first,\ncompile hot parts", fill=G2); f.box(430, 260, 100, 36, "Fast\nresult", fill=G1)
    f.line(210, 278, 250, 278, arrow=True); f.line(390, 278, 430, 278, arrow=True)
    f.text(300, 316, "Best of both.   Examples: JavaScript in Chrome (V8), Java", fs=12, italic=True)
    return f


@fig("interp-stages")
def _():
    f = F(600, 300)
    items = [("Text\n\"1 + 2 * 3\"", 10), ("Lexer\ncuts into\ntokens", 130), ("Parser\nbuilds a\ntree", 250), ("Evaluator\nwalks the\ntree", 370), ("Answer\n7", 490)]
    for t, x in items:
        f.box(x, 20, 100, 62, t, fs=12, fill=G1)
    for i in range(4):
        f.line(items[i][1] + 100, 51, items[i + 1][1], 51, arrow=True)
    f.text(180, 110, "[1] [+] [2] [*] [3]", fs=12, mono=True)
    f.text(180, 128, "tokens", fs=11, italic=True)
    # tree
    f.circle(420, 180, 17, "+", fill=G2, bold=True)
    f.circle(370, 235, 15, "1"); f.circle(470, 235, 17, "*", fill=G2, bold=True)
    f.circle(440, 285, 13, "2", fs=11); f.circle(500, 285, 13, "3", fs=11)
    f.line(408, 192, 378, 223); f.line(432, 192, 462, 222); f.line(463, 250, 445, 273); f.line(477, 250, 495, 273)
    f.text(160, 205, "The tree remembers that\nmultiplication happens\nBEFORE addition.", fs=12, italic=True)
    f.text(160, 262, "2 * 3 = 6   then   1 + 6 = 7", fs=12, mono=True)
    return f


@fig("architecture")
def _():
    f = F(600, 430)
    f.box(20, 12, 560, 130, "", fill=G1, dash=True)
    f.text(300, 28, "RENDERER PROCESS  (the window you see: HTML + CSS + JavaScript)", fs=12, bold=True)
    for t, x in [("app.js\nUI + logic", 36), ("lsp.js\nclangd client", 156), ("debug.js\ngdb client", 276), ("terminal.js\nxterm.js", 396)]:
        f.box(x, 46, 108, 46, t, fs=12, fill="#fff")
    f.box(516, 46, 52, 46, "Monaco\neditor", fs=10, fill=G2)
    f.text(300, 122, "index.html  +  styles.css  +  icons.js", fs=11, mono=True)
    f.box(20, 160, 560, 44, "preload.js  —  the safe bridge   (window.nova.*)", fs=13, bold=True, fill=G2)
    f.line(300, 142, 300, 160, both=True)
    f.box(20, 222, 560, 60, "", fill=G1, dash=True)
    f.text(300, 238, "MAIN PROCESS  (main.js, runs on Node.js)", fs=12, bold=True)
    for t, x in [("File system", 36), ("Process runner", 146), ("PTY", 256), ("LSP pipe", 336), ("GDB pipe", 436)]:
        f.box(x, 250, 96 if x != 256 and x != 336 else 70, 24, t, fs=11, fill="#fff")
    f.line(300, 204, 300, 222, both=True)
    f.text(300, 306, "starts and talks to other programs through pipes", fs=11, italic=True)
    for i, (t, x) in enumerate([("g++ / cmake", 24), ("shell\n(PowerShell)", 140), ("clangd", 272), ("gdb", 384), ("Your\nprogram", 478)]):
        f.box(x, 336, 98 if i != 2 and i != 3 else 78, 50, t, fs=12, fill="#fff", bold=True)
        f.line(x + (49 if i not in (2, 3) else 39), 322, x + (49 if i not in (2, 3) else 39), 336, both=True)
    f.line(300, 282, 300, 322, dash=True)
    f.text(300, 410, "Child processes: separate programs that NOVA starts, feeds and listens to", fs=12, italic=True)
    return f


@fig("lsp-flow")
def _():
    f = F(600, 400)
    seq(f, ["NOVA (editor)", "clangd (server)"], [150, 450], top=34, bottom=385)
    steps = [("initialize  (here is my project folder)", 1), ("capabilities  (here is what I can do)", -1),
             ("didOpen  (I opened main.cpp, here is the text)", 1), ("publishDiagnostics  (error on line 14!)", -1),
             ("didChange  (user typed a letter)", 1), ("completion  (what can go after \"std::\" ?)", 1),
             ("list of 100 items", -1), ("hover  (what is this word?)", 1), ("documentation text", -1), ("definition  (where was it made?)", 1), ("file + line number", -1)]
    y = 80
    for t, d in steps:
        if d > 0:
            f.line(150, y, 450, y, arrow=True)
        else:
            f.line(450, y, 150, y, arrow=True, dash=True)
        f.text(300, y - 8, t, fs=11.5)
        y += 28
    return f


@fig("gdb-flow")
def _():
    f = F(600, 400)
    seq(f, ["NOVA (debug.js)", "gdb  (MI mode)"], [150, 450], top=34, bottom=385)
    steps = [("-break-insert  main.cpp:9", 1), ("^done,bkpt={number=\"1\"}", -1), ("-exec-run", 1), ("^running", -1),
             ("*stopped,reason=\"breakpoint-hit\"", -1), ("-stack-list-frames", 1), ("^done,stack=[ ... ]", -1),
             ("-stack-list-variables", 1), ("^done,variables=[ ... ]", -1), ("-exec-next", 1), ("*stopped,reason=\"end-stepping-range\"", -1)]
    y = 80
    for t, d in steps:
        if d > 0:
            f.line(150, y, 450, y, arrow=True)
        else:
            f.line(450, y, 150, y, arrow=True, dash=True)
        f.text(300, y - 8, t, fs=11, mono=True)
        y += 28
    return f


@fig("build-flow")
def _():
    f = F(600, 470)
    steps = ["You press Run  (Shift+F10)", "Save all changed files", "CMake project?", "cmake -S . -B build   (configure)", "cmake --build build   (compile + link)",
             "Find hello.exe in the build folder", "Start the program (spawn)", "Stream its output to the Run panel", "Parse errors → squiggles + Problems list"]
    y = 8
    for i, t in enumerate(steps):
        if i == 2:
            f.box(180, y, 240, 38, t, fs=13, fill=G2, bold=True)
            f.text(450, y + 19, "no → g++ file.cpp -o file.exe", fs=11, italic=True, anchor="start")
            f.line(420, y + 19, 440, y + 19, dash=True)
            f.text(212, y + 55, "yes", fs=11, italic=True, anchor="start")
        else:
            f.box(160, y, 280, 38, t, fs=13, fill=G1 if i % 2 == 0 else "#fff")
        if i < len(steps) - 1:
            f.line(300, y + 38, 300, y + 52, arrow=True)
        y += 52
    return f


@fig("loc-chart")
def _():
    files = ["main.js", "preload.js", "launch.js", "renderer/app.js", "renderer/lsp.js", "renderer/debug.js", "renderer/terminal.js", "renderer/icons.js", "renderer/index.html", "renderer/styles.css"]
    data = []
    for fn in files:
        try:
            n = sum(1 for _ in open(os.path.join(ROOT, fn), encoding="utf8"))
        except Exception:
            n = 0
        data.append((fn, n))
    mx = max(n for _, n in data)
    f = F(600, 60 + 30 * len(data))
    f.text(300, 18, "Lines of code in each NOVA file", fs=14, bold=True)
    for i, (fn, n) in enumerate(data):
        y = 40 + i * 30
        f.text(150, y + 10, fn, fs=12, anchor="end", mono=True)
        w = 380 * n / mx
        f.raw(f'<rect x="160" y="{y}" width="{w:.0f}" height="20" fill="url(#hatch)" stroke="#000" stroke-width="1.2"/>')
        f.text(160 + w + 6, y + 10, str(n), fs=12, anchor="start", bold=True)
    return f


@fig("lang-map")
def _():
    f = F(600, 330)
    rows = [("Skeleton", "HTML", "index.html", "What is on the screen: buttons, panels, boxes"), ("Skin", "CSS", "styles.css", "How it looks: colours, sizes, spacing, dark/light"),
            ("Brain", "JavaScript", "app.js  lsp.js  debug.js ...", "What happens when you click, type or press a key"),
            ("Engine room", "Node.js", "main.js  preload.js", "Talks to the computer: files, programs, windows"),
            ("The cargo", "C and C++", "your code", "The programs NOVA edits, builds and debugs"),
            ("The glue", "CMake, JSON", "CMakeLists.txt, messages", "Tells compilers what to build; carries messages")]
    f.box(10, 8, 580, 28, "", fill=G2, r=2)
    f.text(70, 22, "Body part", fs=13, bold=True); f.text(190, 22, "Language", fs=13, bold=True); f.text(340, 22, "Files", fs=13, bold=True); f.text(500, 22, "Job", fs=13, bold=True)
    for i, (a, b, c, d) in enumerate(rows):
        y = 36 + i * 48
        f.box(10, y, 580, 48, "", fill="#fff" if i % 2 else G1, r=0, sw=1)
        f.text(70, y + 24, a, fs=13, bold=True); f.text(190, y + 24, b, fs=13); f.text(285, y + 24, c, fs=11, anchor="start", mono=True)
        f.text(500, y + 24, "", fs=11)
    f.raw("")
    # job column text wrapped under rows is crowded; place as italic subtitle
    for i, (a, b, c, d) in enumerate(rows):
        f.text(300, 36 + i * 48 + 40, d, fs=11, italic=True)
    return f


@fig("folder-tree")
def _():
    f = F(600, 380)
    ents = [(0, "NOVA - C Interpreter/", True), (1, "package.json            project name, scripts, dependencies", False), (1, "main.js                 MAIN process: window, files, processes, PTY, clangd, gdb", False),
            (1, "preload.js              the safe bridge (window.nova)", False), (1, "launch.js               starts Electron without the Node-mode flag", False), (1, "Launch NOVA.bat         double-click launcher", False),
            (1, "renderer/", True), (2, "index.html             the skeleton of the window", False), (2, "styles.css             all colours and layout", False), (2, "icons.js               tiny SVG icon set", False),
            (2, "app.js                 UI, tabs, tree, build/run, search", False), (2, "lsp.js                 clangd (language server) client", False), (2, "debug.js               gdb (debugger) client", False),
            (2, "terminal.js            xterm.js terminal", False), (1, "node_modules/            downloaded libraries (Electron, Monaco, xterm...)", False), (1, "docs/                    this book", False)]
    for i, (lv, t, d) in enumerate(ents):
        y = 16 + i * 22
        f.text(16 + lv * 28, y, ("[+] " if d else "    ") + t, fs=11.5, anchor="start", mono=True, bold=d)
    return f


# ---------------------------------------------------------------- HTML / CSS
@fig("dom-tree")
def _():
    f = F(600, 290)
    f.circle(300, 30, 20, "html", fs=11, fill=G2)
    f.circle(180, 100, 20, "head", fs=11); f.circle(420, 100, 20, "body", fs=11, fill=G2)
    f.line(288, 46, 192, 86); f.line(312, 46, 408, 86)
    f.circle(130, 175, 22, "title", fs=11); f.circle(230, 175, 22, "link", fs=11)
    f.line(170, 116, 140, 156); f.line(190, 116, 220, 156)
    f.circle(350, 175, 20, "h1", fs=11); f.circle(430, 175, 20, "p", fs=11); f.circle(510, 175, 20, "ul", fs=11)
    f.line(412, 116, 355, 156); f.line(420, 120, 430, 155); f.line(432, 116, 505, 156)
    f.circle(480, 250, 17, "li", fs=11); f.circle(545, 250, 17, "li", fs=11)
    f.line(504, 194, 485, 233); f.line(515, 194, 540, 233)
    f.text(130, 215, "\"NOVA IDE\"", fs=11, mono=True); f.text(350, 215, "\"Hello\"", fs=11, mono=True)
    f.text(90, 262, "Every tag is a branch.\nText is a leaf.", fs=12, italic=True)
    return f


@fig("box-model")
def _():
    f = F(600, 300)
    f.box(150, 10, 300, 280, "margin  (space outside)", fs=12, fill="#fff", dash=True, anchor="middle", r=2)
    f.box(185, 48, 230, 210, "", fill=G1, r=2)
    f.text(300, 62, "border", fs=12, italic=True)
    f.box(205, 78, 190, 160, "", fill=G2, r=2)
    f.text(300, 92, "padding  (space inside)", fs=12)
    f.box(235, 108, 130, 100, "CONTENT\ntext, image", fs=13, bold=True, fill="#fff", r=2)
    f.text(60, 150, "width +\npadding +\nborder +\nmargin\n= space used", fs=12, italic=True)
    return f


@fig("flexbox")
def _():
    f = F(600, 250)
    f.text(150, 14, "display: flex; justify-content: space-between", fs=11.5, mono=True)
    f.box(20, 28, 560, 54, "", fill="#fff", dash=True, r=3)
    for x in (30, 260, 490):
        f.box(x, 38, 80, 34, "item", fill=G2)
    f.text(150, 112, "display: flex; justify-content: center; gap: 12px", fs=11.5, mono=True)
    f.box(20, 126, 560, 54, "", fill="#fff", dash=True, r=3)
    for x in (190, 282, 374):
        f.box(x, 136, 80, 34, "item", fill=G2)
    f.text(150, 206, "display: flex; flex-direction: column", fs=11.5, mono=True)
    f.box(20, 216, 120, 30, "", fill="#fff", dash=True, r=3, sw=1)
    f.text(300, 232, "(items stack top-to-bottom; the main axis turns 90 degrees)", fs=12, italic=True)
    return f


@fig("grid-layout")
def _():
    f = F(600, 300)
    f.box(10, 10, 580, 280, "", fill="#fff", dash=True, r=3)
    f.box(20, 20, 560, 36, "title bar  (row 1: 40px)", fill=G2)
    f.box(20, 62, 36, 190, "stripe", fill=G2, fs=11)
    f.box(62, 62, 130, 190, "sidebar", fill=G1)
    f.box(198, 62, 382, 190, "editor  (takes the rest: 1fr)", fill="#fff", bold=True)
    f.box(20, 258, 560, 24, "status bar  (row: 24px)", fill=G2, fs=12)
    return f


# ---------------------------------------------------------------- JS / Node / Electron
@fig("event-loop")
def _():
    f = F(600, 330)
    f.box(20, 20, 150, 120, "Call stack\n\n(runs one thing\nat a time)", fs=12, fill=G2)
    f.box(220, 20, 160, 54, "Web / Node APIs\ntimers, files, network", fs=12, fill=G1)
    f.box(220, 96, 160, 44, "Microtask queue\n(Promises)", fs=12, fill="#fff")
    f.box(430, 20, 150, 54, "Task queue\n(callbacks, events)", fs=12, fill="#fff")
    f.box(215, 190, 170, 56, "EVENT LOOP\n\"is the stack empty?\"", fs=13, bold=True, fill=G3)
    f.line(170, 50, 220, 47, arrow=True); f.text(195, 36, "start", fs=10.5, italic=True)
    f.line(380, 47, 430, 47, arrow=True); f.text(405, 36, "done", fs=10.5, italic=True)
    f.poly([(505, 74), (505, 218), (385, 218)], arrow=True)
    f.poly([(215, 218), (95, 218), (95, 140)], arrow=True)
    f.line(300, 140, 300, 190, arrow=True)
    f.text(300, 290, "Slow jobs (files, network) are handed away, so the window never freezes.", fs=12.5, italic=True)
    f.text(300, 310, "When they finish, their callback waits in a queue for its turn.", fs=12.5, italic=True)
    return f


@fig("promise-states")
def _():
    f = F(600, 190)
    f.box(20, 70, 140, 60, "pending\n(waiting...)", fs=13, fill=G1, bold=True)
    f.box(380, 20, 200, 52, "fulfilled\n(has a value)  → .then()", fs=12.5, fill=G2)
    f.box(380, 118, 200, 52, "rejected\n(has an error)  → .catch()", fs=12.5, fill=G2)
    f.line(160, 90, 380, 48, arrow=True); f.line(160, 112, 380, 142, arrow=True)
    f.text(270, 56, "resolve(value)", fs=11.5, mono=True); f.text(270, 150, "reject(error)", fs=11.5, mono=True)
    return f


@fig("scope-chain")
def _():
    f = F(600, 250)
    f.box(10, 10, 580, 230, "", fill=G1, r=6, dash=True)
    f.text(30, 26, "global scope", fs=12, anchor="start", bold=True)
    f.text(30, 48, "const S = {...}", fs=12, anchor="start", mono=True)
    f.box(40, 62, 520, 160, "", fill=G2, r=6)
    f.text(60, 78, "function makeCounter()", fs=12, anchor="start", bold=True, mono=True)
    f.text(60, 98, "let count = 0", fs=12, anchor="start", mono=True)
    f.box(80, 112, 440, 96, "", fill="#fff", r=6)
    f.text(100, 128, "return function () {", fs=12, anchor="start", mono=True)
    f.text(120, 148, "count = count + 1;   ← finds count in the outer box", fs=12, anchor="start", mono=True)
    f.text(100, 168, "}", fs=12, anchor="start", mono=True)
    f.text(100, 192, "An inner function can see every box around it. That is a closure.", fs=12, anchor="start", italic=True)
    return f


@fig("ipc-sequence")
def _():
    f = F(600, 330)
    seq(f, ["Renderer (app.js)", "preload.js", "Main (main.js)"], [90, 300, 510], top=34, bottom=318)
    steps = [(90, 300, "nova.read(path)", 80), (300, 510, "ipcRenderer.invoke('fs:read', path)", 108),
             (510, 510, "fs.readFile(...)  (disk)", 136), (510, 300, "{ text: '...' }", 164), (300, 90, "Promise resolves with the file text", 192)]
    for x1, x2, t, y in steps:
        if x1 == x2:
            f.box(x1 - 85, y - 14, 170, 26, t, fs=11, fill=G1)
        else:
            f.line(x1, y, x2, y, arrow=True, dash=(x1 > x2))
            f.text((x1 + x2) / 2, y - 9, t, fs=11, mono=True)
    f.text(300, 240, "Why this detour?  The window (renderer) runs web content, which could be unsafe.", fs=12.5, italic=True)
    f.text(300, 260, "It is not allowed to touch your disk directly. Only the main process can,", fs=12.5, italic=True)
    f.text(300, 280, "and preload.js decides exactly which doors are open.", fs=12.5, italic=True)
    return f


@fig("stream-flow")
def _():
    f = F(600, 170)
    f.box(20, 55, 110, 56, "g++ / your\nprogram", fs=12.5, fill=G2, bold=True)
    f.box(240, 20, 120, 40, "stdout  (pipe)", fs=12, fill=G1); f.box(240, 106, 120, 40, "stderr  (pipe)", fs=12, fill=G1)
    f.box(470, 55, 110, 56, "main.js\n'data' events", fs=12.5, fill=G2, bold=True)
    f.line(130, 70, 240, 42, arrow=True); f.line(130, 96, 240, 126, arrow=True)
    f.line(360, 40, 470, 70, arrow=True); f.line(360, 126, 470, 98, arrow=True)
    f.text(300, 8 + 150, "Output arrives in small chunks, not whole lines.", fs=12, italic=True)
    return f


# ---------------------------------------------------------------- C / C++
@fig("memory-layout")
def _():
    f = F(600, 340)
    blocks = [("Stack  (local variables, function calls)", G2, "grows ↓  automatic, fast"), ("free space", "#fff", ""), ("Heap  (malloc / new)", G1, "grows ↑  you manage it"),
              ("Globals and static data", G2, "live for the whole program"), ("Your program's code", G3, "read only")]
    hs = [78, 60, 70, 54, 54]
    y = 14
    for (t, fl, note), h in zip(blocks, hs):
        f.box(120, y, 330, h, t, fs=13, fill=fl, bold=True, dash=(t == "free space"))
        if note:
            f.text(465, y + h / 2, note, fs=11.5, anchor="start", italic=True)
        y += h
    f.text(58, 30, "high\naddress", fs=11, italic=True); f.text(58, 330, "low\naddress", fs=11, italic=True)
    f.line(60, 62, 60, 262, arrow=True, dash=True)
    return f


@fig("pointer")
def _():
    f = F(600, 190)
    f.text(300, 14, "int age = 25;      int *p = &age;", fs=13, mono=True)
    f.box(60, 50, 150, 56, "25", fs=18, bold=True, fill=G2)
    f.text(135, 128, "age", fs=13, bold=True); f.text(135, 148, "address 0x1000", fs=11, mono=True)
    f.box(390, 50, 150, 56, "0x1000", fs=15, bold=True, mono=True, fill="#fff")
    f.text(465, 128, "p  (a pointer)", fs=13, bold=True); f.text(465, 148, "address 0x2008", fs=11, mono=True)
    f.poly([(390, 78), (300, 78), (300, 40), (135, 40), (135, 50)], arrow=True)
    f.text(300, 170, "p does not hold 25. It holds WHERE 25 lives.   *p  means \"go there and look\".", fs=12, italic=True)
    return f


@fig("array-memory")
def _():
    f = F(600, 170)
    f.text(300, 14, "int a[4] = {10, 20, 30, 40};", fs=13, mono=True)
    for i, v in enumerate([10, 20, 30, 40]):
        f.box(70 + i * 120, 40, 120, 50, str(v), fs=17, bold=True, fill=G1 if i % 2 else G2)
        f.text(130 + i * 120, 108, f"a[{i}]", fs=12, mono=True)
        f.text(130 + i * 120, 128, f"0x{1000 + 4 * i:X}", fs=11, mono=True)
    f.text(300, 156, "Items sit side by side. a[2] means: start + 2 × 4 bytes.", fs=12, italic=True)
    return f


@fig("struct-layout")
def _():
    f = F(600, 200)
    f.text(300, 14, "struct Point { int x; int y; char tag; };", fs=13, mono=True)
    f.box(100, 40, 120, 50, "x  (4 bytes)", fs=12.5, fill=G2); f.box(220, 40, 120, 50, "y  (4 bytes)", fs=12.5, fill=G1)
    f.box(340, 40, 30, 50, "tag", fs=10.5, fill=G2); f.box(370, 40, 130, 50, "padding (3 bytes)", fs=11, fill="url(#hatch)")
    f.text(300, 120, "sizeof(Point) = 12, not 9.   The compiler adds padding so the next", fs=12, italic=True)
    f.text(300, 140, "item starts at an address the CPU likes. Order affects size!", fs=12, italic=True)
    return f


@fig("raii-timeline")
def _():
    f = F(600, 230)
    f.line(40, 70, 560, 70, arrow=True)
    f.text(300, 18, "{   // start of scope", fs=12, mono=True)
    pts = [(80, "File f(\"a.txt\");\nconstructor opens file"), (300, "use the file...\nanything can happen\n(even an error)"), (520, "}  end of scope\ndestructor closes file\nAUTOMATICALLY")]
    for x, t in pts:
        f.circle(x, 70, 7, fill="#000")
        f.box(x - 78, 110, 156, 72, t, fs=11.5, fill=G1 if x != 300 else "#fff")
        f.line(x, 77, x, 110)
    f.text(300, 210, "No matter how the block ends, cleanup runs. That is RAII.", fs=12.5, italic=True, bold=True)
    return f


@fig("vector-growth")
def _():
    f = F(600, 280)
    f.text(300, 16, "How std::vector grows (capacity doubles when full)", fs=14, bold=True)
    caps = [1, 2, 4, 8, 16, 32]
    f.line(60, 240, 560, 240); f.line(60, 240, 60, 36)
    for i, c in enumerate(caps):
        x = 90 + i * 78; h = c / 32 * 190
        f.raw(f'<rect x="{x}" y="{240 - h:.0f}" width="52" height="{h:.0f}" fill="url(#hatch)" stroke="#000" stroke-width="1.2"/>')
        f.text(x + 26, 256, f"{i + 1}", fs=12); f.text(x + 26, 232 - h, str(c), fs=12, bold=True)
    f.text(300, 274, "push_back call that triggered growth  (1st, 2nd, 3rd ...)", fs=11.5, italic=True)
    f.text(30, 140, "capacity", fs=11.5, italic=True)
    return f


@fig("vtable")
def _():
    f = F(600, 260)
    f.box(20, 20, 170, 56, "Shape  (base class)\nvirtual double area()", fs=12, fill=G2)
    f.box(20, 150, 100, 52, "Circle\narea() = πr²", fs=12, fill=G1); f.box(140, 150, 100, 52, "Square\narea() = s²", fs=12, fill=G1)
    f.line(70, 150, 90, 76, arrow=True); f.line(190, 150, 120, 76, arrow=True)
    f.box(340, 20, 240, 40, "Shape* p = new Circle(2);", fs=12, mono=True, fill="#fff")
    f.box(340, 90, 110, 54, "object\n[vptr][radius]", fs=11.5, fill=G1); f.box(480, 80, 100, 74, "Circle vtable\narea → Circle::area", fs=11, fill=G2)
    f.line(450, 117, 480, 117, arrow=True)
    f.text(460, 190, "p->area() looks in the table\nand calls the RIGHT version,\neven though p is a Shape*.", fs=12, italic=True)
    f.line(460, 60, 395, 90, arrow=True)
    return f


# ---------------------------------------------------------------- CMake / protocols
@fig("cmake-flow")
def _():
    f = F(600, 190)
    f.box(10, 50, 130, 70, "CMakeLists.txt\n(your recipe)", fs=12.5, fill=G1, bold=True)
    f.box(190, 50, 130, 70, "Configure +\nGenerate", fs=12.5, fill=G2, bold=True)
    f.box(370, 50, 100, 70, "Build files\n(Ninja /\nMakefile)", fs=12, fill=G1)
    f.box(510, 50, 80, 70, "hello\n.exe", fs=13, fill=G2, bold=True)
    f.line(140, 85, 190, 85, arrow=True); f.line(320, 85, 370, 85, arrow=True); f.line(470, 85, 510, 85, arrow=True)
    f.text(165, 142, "cmake -S . -B build", fs=10.5, mono=True); f.text(420, 142, "cmake --build build", fs=10.5, mono=True)
    f.text(300, 175, "CMake does not compile anything. It writes instructions for the tool that does.", fs=12, italic=True)
    return f


@fig("target-graph")
def _():
    f = F(600, 220)
    f.box(240, 10, 120, 44, "app  (exe)", fs=13, bold=True, fill=G2)
    f.box(90, 100, 120, 44, "shapes  (lib)", fs=13, fill=G1); f.box(390, 100, 120, 44, "utils  (lib)", fs=13, fill=G1)
    f.box(240, 170, 120, 40, "fmt  (3rd party)", fs=12.5, fill="#fff", dash=True)
    f.line(270, 54, 170, 100, arrow=True); f.line(330, 54, 430, 100, arrow=True); f.line(190, 144, 270, 170, arrow=True); f.line(410, 144, 330, 170, arrow=True)
    f.text(300, 90, "target_link_libraries", fs=10.5, mono=True)
    return f


@fig("jsonrpc-frame")
def _():
    f = F(600, 200)
    f.text(300, 14, "One LSP message on the wire", fs=13, bold=True)
    f.box(40, 34, 520, 38, "Content-Length: 52\\r\\n\\r\\n", fs=13, mono=True, fill=G2)
    f.box(40, 72, 520, 62, "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/hover\",\n \"params\":{...}}", fs=12.5, mono=True, fill="#fff")
    f.text(300, 158, "Header says how many bytes follow. Then the JSON body.", fs=12, italic=True)
    f.text(300, 178, "Without the length, the reader could not tell where one message ends.", fs=12, italic=True)
    return f


@fig("mi-anatomy")
def _():
    f = F(600, 210)
    f.text(300, 18, "Anatomy of a GDB/MI line", fs=13, bold=True)
    f.box(30, 40, 540, 40, "*stopped,reason=\"breakpoint-hit\",frame={func=\"main\",line=\"9\"}", fs=13, mono=True, fill=G1)
    for x, t in [(44, "first character:\n* = async event\n^ = reply\n~ = text"), (190, "class:\nstopped"), (330, "key=\"value\" pairs\n(comma separated)"), (480, "{ } tuple\n[ ] list")]:
        f.line(x if x != 190 else 105, 82, x, 118, arrow=True) if False else None
        f.text(x + (30 if x == 44 else 0), 130, t, fs=11.5, italic=True)
    return f


@fig("debug-states")
def _():
    f = F(600, 200)
    f.box(20, 70, 110, 50, "Not\ndebugging", fs=12.5, fill=G1); f.box(190, 70, 100, 50, "Running", fs=12.5, fill=G2)
    f.box(350, 70, 100, 50, "Paused", fs=12.5, fill=G2, bold=True); f.box(500, 70, 90, 50, "Ended", fs=12.5, fill=G1)
    f.line(130, 95, 190, 95, arrow=True); f.text(160, 82, "start", fs=10.5, italic=True)
    f.line(290, 85, 350, 85, arrow=True); f.text(320, 72, "hit bp", fs=10.5, italic=True)
    f.line(350, 108, 290, 108, arrow=True); f.text(320, 124, "step/resume", fs=10.5, italic=True)
    f.line(450, 95, 500, 95, arrow=True); f.text(475, 82, "exit", fs=10.5, italic=True)
    f.poly([(240, 120), (240, 160), (70, 160), (70, 120)], arrow=True, dash=True)
    f.text(155, 178, "program exits normally / Stop pressed", fs=11, italic=True)
    return f


@fig("progress-ladder")
def _():
    f = F(600, 330)
    steps = ["1  Install tools", "2  Folder + package.json", "3  Empty window (main.js)", "4  HTML skeleton + CSS", "5  Monaco editor + tabs", "6  File tree + save",
             "7  Build and run", "8  Problems + squiggles", "9  Search + palette", "10 Terminal (PTY)", "11 clangd (LSP)", "12 gdb debugger"]
    for i, t in enumerate(steps):
        x = 20 + (i % 2) * 290
        y = 14 + (i // 2) * 52
        f.box(x, y, 270, 40, t, fs=13, fill=G1 if (i // 2) % 2 == 0 else G2, anchor="start", bold=(i == 11))
    return f


@fig("tab-lifecycle")
def _():
    f = F(600, 190)
    for t, x in [("Click file\nin tree", 10), ("Read file\n(IPC)", 130), ("Create\nmodel", 250), ("Show in\neditor", 370), ("Edit →\ndirty ●", 490)]:
        f.box(x, 40, 100, 56, t, fs=12.5, fill=G1)
    for i in range(4):
        f.line(110 + i * 120, 68, 130 + i * 120, 68, arrow=True)
    f.box(370, 130, 100, 40, "Ctrl+S\nsave", fs=12, fill=G2); f.line(540, 96, 540, 112); f.poly([(540, 112), (540, 150), (470, 150)], arrow=True)
    f.text(250, 160, "dirty dot disappears", fs=11.5, italic=True)
    return f


def render_fig(name):
    return FIGS[name]().svg()


if __name__ == "__main__":
    for n in FIGS:
        s = render_fig(n)
        open(os.path.join(os.path.dirname(__file__), f"_fig_{n}.svg"), "w", encoding="utf8").write(s)
    print(len(FIGS), "figures OK")
