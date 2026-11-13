"""Black-and-white diagrams for 'Inside the Vibration Lab'. Plots are computed from the real equations."""
import math
from figures import F, G1, G2, G3

FIGS = {}


def fig(name):
    def deco(fn):
        FIGS[name] = fn
        return fn
    return deco


def render_fig(name):
    return FIGS[name]().svg()


def axes(f, x0, y0, w, h, xr, yr, xlabel="", ylabel="", xt=5, yt=4, fmt="{:.1f}"):
    """Draw plot axes; return mapping functions."""
    f.box(x0, y0, w, h, "", fill="#fff", r=0, sw=1)
    mx = lambda v: x0 + (v - xr[0]) / (xr[1] - xr[0]) * w
    my = lambda v: y0 + h - (v - yr[0]) / (yr[1] - yr[0]) * h
    for i in range(xt + 1):
        v = xr[0] + (xr[1] - xr[0]) * i / xt
        f.line(mx(v), y0 + h, mx(v), y0 + h + 4, sw=1)
        f.text(mx(v), y0 + h + 15, fmt.format(v), fs=10)
    for i in range(yt + 1):
        v = yr[0] + (yr[1] - yr[0]) * i / yt
        f.line(x0 - 4, my(v), x0, my(v), sw=1)
        f.text(x0 - 7, my(v), fmt.format(v), fs=10, anchor="end")
        if 0 < i < yt:
            f.line(x0, my(v), x0 + w, my(v), dash=True, sw=0.4)
    if xlabel:
        f.text(x0 + w / 2, y0 + h + 31, xlabel, fs=11, italic=True)
    if ylabel:
        f.raw(f'<text x="{x0 - 38}" y="{y0 + h / 2}" font-size="11" font-style="italic" text-anchor="middle" transform="rotate(-90 {x0 - 38} {y0 + h / 2})" font-family="Times New Roman">{ylabel}</text>')
    return mx, my


def curve(f, mx, my, ts, vs, dash=False, sw=1.8):
    f.poly([(round(mx(t), 1), round(my(v), 1)) for t, v in zip(ts, vs)], dash=dash, sw=sw)


def mtext(f, x, ytop, t, fs=10.5, lh=15, **k):
    """multi-line text whose FIRST line sits at ytop"""
    n = len(t.split(chr(10)))
    f.text(x, ytop + (n - 1) * lh / 2, t, fs=fs, lh=lh, **k)


def spring_path(x1, x2, y, n=7, amp=9):
    pts = [(x1, y)]
    lead = 10
    span = x2 - x1 - 2 * lead
    pts.append((x1 + lead, y))
    for i in range(n * 2):
        pts.append((x1 + lead + span * (i + 0.5) / (n * 2), y + (amp if i % 2 else -amp)))
    pts += [(x2 - lead, y), (x2, y)]
    return pts


def wall(f, x, y, h=60, side="L"):
    f.line(x, y - h / 2, x, y + h / 2, sw=3)
    d = -9 if side == "L" else 9
    for k in range(int(h / 9)):
        yy = y - h / 2 + k * 9
        f.line(x, yy + 9, x + d, yy, sw=1)


# ------------------------------------------------------------------ chapter 1
@fig("gl-product")
def _():
    f = F(620, 300)
    items = [("You\nclick + drag", 10, G1), ("Designer\napp.hpp", 112, G2), ("Model\n(your design)", 214, G1), ("Engine\nengine.hpp", 316, G2), ("State + Meter\n+ Telemetry log", 418, G1)]
    for t, x, fl in items:
        f.box(x, 40, 92, 58, t, fs=11.5, fill=fl, bold=(fl == G2))
    for i in range(4):
        f.line(102 + i * 102, 69, 112 + i * 102 - 10 + 10, 69, arrow=True)
    f.box(214, 150, 92, 50, "Renderer\nrender.hpp", fs=11.5, fill=G2, bold=True)
    f.box(316, 150, 92, 50, "Screen\n(your eyes)", fs=11.5, fill=G1)
    f.box(418, 150, 92, 50, "Recorder\nencoder.hpp", fs=11.5, fill=G2, bold=True)
    f.box(520, 150, 90, 50, "movie.mp4", fs=11.5, fill="#fff", mono=True)
    f.poly([(464, 98), (464, 125), (260, 125), (260, 150)], arrow=True)
    f.line(306, 175, 316, 175, arrow=True)
    f.poly([(362, 200), (362, 222), (464, 222), (464, 200)], arrow=True)
    f.line(510, 175, 520, 175, arrow=True)
    f.text(362, 238, "same picture goes to the screen and to the encoder", fs=11, italic=True)
    f.box(10, 150, 180, 50, "design.vlab (save / load)\ntelemetry.csv (export)", fs=11, fill="#fff", dash=True)
    f.text(310, 280, "Everything is C++; the Windows API draws and records.", fs=12, italic=True)
    return f


@fig("gl-files")
def _():
    f = F(620, 300)
    f.box(240, 10, 140, 40, "live.cpp\nwindow, loop, recording", fs=11, fill=G2, bold=True)
    f.box(40, 110, 130, 40, "app.hpp\nstate + clicks", fs=11.5, fill=G1)
    f.box(240, 110, 140, 40, "render.hpp\nall drawing", fs=11.5, fill=G1)
    f.box(450, 110, 140, 40, "encoder.hpp\nH.264 video", fs=11.5, fill=G1)
    f.box(40, 210, 130, 40, "model_io.hpp\nsave/load/presets", fs=11, fill=G1)
    f.box(240, 210, 140, 40, "engine.hpp\nLagrangian physics", fs=11.5, fill=G2, bold=True)
    for x in (105, 310, 520):
        f.line(310 if x != 105 else 280, 50, x, 110, arrow=True)
    f.line(105, 150, 105, 210, arrow=True)
    f.poly([(170, 130), (210, 130), (210, 230), (240, 230)], arrow=True)
    f.line(170, 230, 240, 230, arrow=True)
    f.line(310, 150, 310, 210, arrow=True)
    f.poly([(240, 130), (200, 130)], arrow=False)
    f.text(460, 215, "An arrow means\n\"includes / uses\"", fs=11.5, italic=True)
    f.text(310, 285, "Windows libraries: user32, gdi32, gdiplus, mfplat, mfreadwrite", fs=11, italic=True)
    return f


@fig("gl-quartercar")
def _():
    f = F(420, 300)
    wall(f, 210, 262, 0, "L")
    f.line(110, 262, 310, 262, sw=3)
    for k in range(10):
        f.line(110 + k * 20 + 20, 262, 110 + k * 20, 276, sw=1)
    f.poly([(210, 262), (210, 240)] + [(210 + (8 if i % 2 else -8), 240 - 6 - i * 8) for i in range(10)] + [(210, 150), (210, 140)], sw=1.6)
    f.box(170, 105, 80, 35, "wheel m₀ = 1 kg", fs=11, fill=G2)
    f.text(300, 215, "tyre spring\nk = 220 N/m", fs=11, anchor="start")
    f.poly([(190, 105), (190, 90)] + [(190 + (7 if i % 2 else -7), 90 - 4 - i * 5) for i in range(8)] + [(190, 48), (190, 40)], sw=1.6)
    f.line(232, 105, 232, 66); f.box(222, 66, 20, 24, "", fill="#fff"); f.line(232, 66, 232, 40)
    f.box(150, 5, 120, 35, "car body m₁ = 4 kg", fs=11, fill=G3)
    f.text(30, 90, "suspension\nspring k = 70\ndamper c = 5", fs=11, anchor="start")
    f.text(30, 200, "both masses are on\nY rails: they can\nonly go up and down", fs=11, anchor="start", italic=True)
    return f


# ------------------------------------------------------------------ chapter 2 : Lagrange
@fig("gl-coords")
def _():
    f = F(620, 250)
    f.text(155, 20, "A: two masses on a rail", fs=13, bold=True)
    wall(f, 20, 100, 60, "L"); wall(f, 290, 100, 60, "R")
    f.box(95, 80, 36, 40, "1", fs=12, fill=G2); f.box(185, 80, 36, 40, "2", fs=12, fill=G2)
    f.poly(spring_path(20, 95, 100, 5, 7), sw=1.4); f.poly(spring_path(131, 185, 100, 4, 7)); f.poly(spring_path(221, 290, 100, 5, 7))
    f.line(113, 150, 140, 150, arrow=True); f.text(150, 150, "x₁", fs=12, italic=True)
    f.line(203, 175, 230, 175, arrow=True); f.text(240, 175, "x₂", fs=12, italic=True)
    f.text(155, 215, "q = ( x₁ , x₂ )     2 numbers = 2 degrees of freedom", fs=12)
    f.line(320, 10, 320, 240, dash=True, sw=0.8)
    f.text(470, 20, "B: one mass free to move in the plane", fs=13, bold=True)
    f.line(350, 200, 590, 200, arrow=True); f.line(350, 200, 350, 40, arrow=True)
    f.text(600, 210, "x", fs=12, italic=True); f.text(340, 38, "y", fs=12, italic=True)
    f.box(470, 90, 34, 34, "m", fs=12, fill=G2)
    f.line(350, 200, 487, 107, dash=True, sw=0.8)
    f.line(487, 107, 487, 200, dash=True); f.line(350, 200, 487, 200, dash=True, sw=0.8)
    f.text(487, 215, "x", fs=11, italic=True); f.text(520, 150, "y", fs=11, italic=True)
    f.text(470, 237, "q = ( x , y )     2 degrees of freedom", fs=12)
    return f


@fig("gl-recipe")
def _():
    f = F(620, 150)
    steps = ["1. Choose\ncoordinates q", "2. Write T, V, D\nand outside forces Q", "3. Differentiate\n(chain rule)", "4. Euler–Lagrange\nequations", "5. Solve on a\ncomputer (RK4)"]
    for i, t in enumerate(steps):
        f.box(8 + i * 122, 40, 108, 66, t, fs=11.5, fill=G1 if i % 2 == 0 else G2)
        if i < 4:
            f.line(116 + i * 122, 73, 130 + i * 122, 73, arrow=True)
    f.text(310, 130, "The whole method in five steps. Newton's way would need a force diagram for every part.", fs=11.5, italic=True)
    return f


@fig("gl-variation")
def _():
    f = F(560, 284)
    mx, my = axes(f, 60, 20, 460, 180, (0, 1), (-1, 1.2), "time t", "position q", xt=4, yt=4)
    ts = [i / 60 for i in range(61)]
    true = [math.sin(math.pi * t) * 0.9 for t in ts]
    curve(f, mx, my, ts, true, sw=2.6)
    for a in (0.45, -0.4, 0.8):
        curve(f, mx, my, ts, [v + a * math.sin(math.pi * t) * (1 if a > 0.5 else 0.8) * 0.6 * (math.sin(2 * math.pi * t) + 0.6 * math.sin(math.pi * t)) for v, t in zip(true, ts)], dash=True, sw=1.2)
    f.circle(mx(0), my(0), 4, fill="#000"); f.circle(mx(1), my(0), 4, fill="#000")
    f.text(mx(0.5), my(0.9) - 12, "the real path (makes the action stationary)", fs=11.5, bold=True)
    f.text(mx(0.77), my(-0.55), "other paths with\nthe same two ends", fs=11, italic=True)
    f.text(280, 252, "Hamilton's idea: of all paths between two fixed end-points,", fs=11, italic=True)
    f.text(280, 270, "nature picks the one where the action S = ∫ L dt stops changing.", fs=11, italic=True)
    return f


@fig("gl-energy-flow")
def _():
    f = F(620, 230)
    f.box(20, 90, 120, 56, "Outside forces\n(drive, mouse)\nwork in  P", fs=11.5, fill=G1)
    f.box(250, 30, 130, 56, "T  kinetic\nenergy of motion", fs=11.5, fill=G2)
    f.box(250, 140, 130, 56, "V  potential\nenergy stored in springs\nand height", fs=11, fill=G2)
    f.line(300, 86, 300, 140, both=True); f.line(330, 86, 330, 140, both=True)
    f.text(398, 113, "swap back\nand forth", fs=11, italic=True, anchor="start")
    f.box(490, 90, 120, 56, "Dampers turn\nit into heat\n2D", fs=11.5, fill=G1)
    f.line(140, 118, 250, 70, arrow=True); f.line(140, 118, 250, 165, arrow=True)
    f.line(380, 70, 490, 118, arrow=True); f.line(380, 165, 490, 118, arrow=True)
    f.text(310, 218, "dE/dt  =  P(in)  −  2D          where E = T + V", fs=13, bold=True)
    return f


@fig("gl-damping")
def _():
    f = F(600, 280)
    mx, my = axes(f, 60, 20, 510, 200, (0, 10), (-0.6, 1.1), "time t  (seconds)", "x(t) / x(0)", xt=5, yt=4)
    wn = 2.0
    ts = [i * 0.05 for i in range(201)]

    def resp(z):
        out = []
        for t in ts:
            if z < 1:
                wd = wn * math.sqrt(1 - z * z)
                out.append(math.exp(-z * wn * t) * (math.cos(wd * t) + z * wn / wd * math.sin(wd * t)))
            elif z == 1:
                out.append((1 + wn * t) * math.exp(-wn * t))
            else:
                w2 = wn * math.sqrt(z * z - 1)
                out.append(math.exp(-z * wn * t) * (math.cosh(w2 * t) + z * wn / w2 * math.sinh(w2 * t)))
        return out
    curve(f, mx, my, ts, resp(0.1), sw=1.8)
    curve(f, mx, my, ts, resp(0.3), dash=True, sw=1.4)
    curve(f, mx, my, ts, resp(1.0), sw=2.8)
    curve(f, mx, my, ts, resp(2.0), dash=True, sw=2.2)
    f.box(330, 28, 232, 78, "", fill="#fff", r=3, sw=1)
    f.line(340, 44, 372, 44, sw=1.8); f.text(380, 44, "ζ = 0.1  rings for ages", fs=11, anchor="start")
    f.line(340, 62, 372, 62, dash=True, sw=1.4); f.text(380, 62, "ζ = 0.3  rings, fades", fs=11, anchor="start")
    f.line(340, 80, 372, 80, sw=2.8); f.text(380, 80, "ζ = 1  critical: fastest, no overshoot", fs=11, anchor="start", bold=True)
    f.line(340, 98, 372, 98, dash=True, sw=2.2); f.text(380, 98, "ζ = 2  overdamped: slow creep", fs=11, anchor="start")
    f.text(300, 270, "All four start at x = 1 and speed 0, with ωn = 2 rad/s. Only the damping ratio ζ changes.", fs=11, italic=True)
    return f


# ------------------------------------------------------------------ chapter 3 : geometry
@fig("gl-spring-geom")
def _():
    f = F(620, 270)
    ax, ay, bx, by = 150, 195, 470, 85
    f.circle(ax, ay, 9, "a", fill=G2, fs=12, bold=True); f.circle(bx, by, 9, "b", fill=G2, fs=12, bold=True)
    f.line(ax, ay, bx, by, sw=2.4)
    f.text(ax - 6, ay + 24, "(xₐ , yₐ)", fs=12, anchor="end"); f.text(bx - 14, by - 22, "(x_b , y_b)", fs=12, anchor="end")
    f.poly([(ax, ay), (bx, ay), (bx, by)], dash=True, sw=1)
    f.text((ax + bx) / 2, ay + 16, "dx = x_b − xₐ", fs=12); f.text(bx + 8, (ay + by) / 2, "dy = y_b − yₐ", fs=12, anchor="start")
    f.text(330, 128, "length  = √(dx² + dy²)", fs=12.5, bold=True, anchor="end")
    f.line(ax, ay, ax + 90, ay - 30, arrow=True, sw=2.4); f.text(ax + 100, ay - 40, "u = (dx , dy) / length   (unit vector a→b)", fs=12, anchor="start", italic=True)
    f.line(ax, ay, ax - 55, ay + 18, arrow=True, sw=2.2); f.text(14, ay + 50, "pull on a  = + f·u", fs=11.5, anchor="start")
    f.line(bx, by, bx + 55, by - 18, arrow=True, sw=2.2); f.text(bx + 20, by - 40, "pull on b  = − f·u", fs=11.5, anchor="start")
    f.text(310, 252, "stretch  s = length − L₀      spring force  f = k·s + k₃·s³      (f > 0 means: pulling the ends together)", fs=11.5, italic=True)
    return f


@fig("gl-accel-flow")
def _():
    f = F(620, 360)
    f.box(10, 10, 600, 38, "INPUT:  time t,  positions q,  velocities v,  outside forces ext", fs=12.5, fill=G1, bold=True)
    f.box(30, 80, 560, 108, "", fill="#fff", dash=True)
    f.text(310, 94, "LOOP 1: for every spring and damper", fs=12, bold=True)
    f.box(50, 110, 150, 60, "linkInfo()\nlength, u, stretch,\nrate, force f", fs=11, fill=G2)
    f.box(235, 110, 160, 60, "F[a] += f·u\nF[b] −= f·u", fs=12, fill=G2, mono=True)
    f.box(430, 110, 140, 60, "springs: f = ks + k₃s³\ndampers: f = c·rate", fs=11, fill=G1)
    f.line(200, 140, 235, 140, arrow=True); f.line(395, 140, 430, 140, dash=True)
    f.box(30, 210, 560, 106, "", fill="#fff", dash=True)
    f.text(310, 224, "LOOP 2: for every mass", fs=12, bold=True)
    f.box(50, 240, 120, 60, "start with F\nfrom loop 1", fs=11.5, fill=G1)
    f.box(185, 240, 135, 60, "+ gravity  m·g\n+ drive  F·cos(ωt)\n+ mouse force", fs=11, fill=G2)
    f.box(335, 240, 110, 60, "a = F / m", fs=13, fill=G2, mono=True)
    f.box(460, 240, 110, 60, "rail?  zero the\nblocked part", fs=11, fill=G1)
    for x in (170, 320, 445):
        f.line(x, 270, x + 15, 270, arrow=True)
    f.line(310, 48, 310, 80, arrow=True); f.line(310, 188, 310, 210, arrow=True)
    f.box(10, 330, 600, 26, "OUTPUT:  accelerations a   (one pair per node; ground nodes get 0)", fs=12.5, fill=G1, bold=True)
    f.line(310, 316, 310, 330, arrow=True)
    return f


# ------------------------------------------------------------------ chapter 4 : numerics
@fig("gl-euler-rk4")
def _():
    f = F(600, 300)
    mx, my = axes(f, 60, 20, 510, 220, (0, 12), (-3, 3), "time t", "x(t)", xt=6, yt=4)
    h = 0.3
    n = int(12 / h)
    ts = [i * h for i in range(n + 1)]
    # Euler
    x, v = 1.0, 0.0
    eu = [x]
    for _i in range(n):
        x, v = x + h * v, v - h * x
        eu.append(x)
    # RK4
    x, v = 1.0, 0.0
    rk = [x]
    for _i in range(n):
        k1x, k1v = v, -x
        k2x, k2v = v + 0.5 * h * k1v, -(x + 0.5 * h * k1x)
        k3x, k3v = v + 0.5 * h * k2v, -(x + 0.5 * h * k2x)
        k4x, k4v = v + h * k3v, -(x + h * k3x)
        x += h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x)
        v += h / 6 * (k1v + 2 * k2v + 2 * k3v + k4v)
        rk.append(x)
    fine = [i * 0.05 for i in range(241)]
    curve(f, mx, my, fine, [math.cos(t) for t in fine], sw=1.0)
    curve(f, mx, my, ts, eu, dash=True, sw=2.2)
    curve(f, mx, my, ts, rk, sw=2.8)
    f.text(mx(0.4), my(2.5), "dashed: simple Euler  (energy grows, the answer explodes)", fs=11.5, anchor="start", bold=True)
    f.text(mx(0.4), my(-2.5), "thick: RK4,  lying right on the true cos(t) (thin)", fs=11.5, anchor="start", bold=True)
    f.text(300, 292, "A frictionless spring, x'' = −x, step size h = 0.3 s (huge on purpose). Same step, very different quality.", fs=11, italic=True)
    return f


@fig("gl-rk4-stages")
def _():
    f = F(600, 270)
    mx, my = axes(f, 50, 15, 330, 190, (0, 1), (0, 1), "time inside one step  (0 to h)", "state", xt=2, yt=2, fmt="{:.1f}")
    pts = [(0.0, 0.20), (0.5, 0.40), (0.5, 0.50), (1.0, 0.62)]
    slopes = [0.9, 0.7, 0.6, 0.45]
    for i, ((t, y), s_) in enumerate(zip(pts, slopes)):
        tx, ty = mx(t), my(y)
        dx = 38
        f.line(tx - dx * (0 if i == 0 else 1), ty + dx * s_ * 0.5 * (0 if i == 0 else 1), tx + dx * (1 if i < 3 else 0.2), ty - dx * s_ * 0.5 * (1 if i < 3 else 0.2), arrow=True, sw=2)
        f.circle(tx, ty, 3, fill="#000")
        f.text(tx + 8, ty + (24 if i in (0, 2) else -22), f"k{i + 1}", fs=12, bold=True, italic=True)
    f.line(mx(0), my(0.2), mx(1), my(0.62), dash=True, sw=2.6)
    items = ["k₁  slope at the start", "k₂  slope at the middle, using k₁", "k₃  slope at the middle, using k₂", "k₄  slope at the end, using k₃", "", "step = h · (k₁ + 2k₂ + 2k₃ + k₄) / 6"]
    for i, t in enumerate(items):
        f.text(405, 40 + i * 24, t, fs=11.5, anchor="start", bold=(i == 5))
    f.text(405, 190, "(dashed line = the final averaged step)", fs=11, anchor="start", italic=True)
    f.text(300, 258, "Four peeks at the slope, one weighted guess: error shrinks with the 4th power of the step size.", fs=11, italic=True)
    return f


@fig("gl-accumulator")
def _():
    f = F(620, 230)
    f.text(10, 20, "real time (screen frames — uneven)", fs=12, anchor="start", bold=True)
    f.line(10, 50, 610, 50, sw=1)
    for x in (30, 78, 130, 181, 226, 281, 335, 380, 440, 485, 543, 590):
        f.line(x, 38, x, 62, sw=2)
    f.text(250, 112, "physics steps (always exactly 1 ms)", fs=12, anchor="start", bold=True)
    f.line(10, 140, 610, 140, sw=1)
    for i in range(0, 60):
        f.line(10 + i * 10, 133, 10 + i * 10, 147, sw=1)
    f.poly([(78, 62), (78, 125)], arrow=True, dash=True)
    f.poly([(181, 62), (181, 125)], arrow=True, dash=True)
    f.text(310, 190, "simAcc  +=  dt·speed        while  simAcc ≥ 1 ms :  step, simAcc −= 1 ms", fs=12.5, bold=True, mono=False)
    f.text(310, 212, "The leftover (less than 1 ms) stays in the accumulator for next frame — no time is ever lost.", fs=11, italic=True)
    return f


@fig("gl-classes")
def _():
    f = F(620, 330)
    f.box(10, 10, 190, 150, "", fill=G1)
    f.text(105, 26, "Node", fs=13, bold=True)
    mtext(f, 18, 52, "kind  (Mass / Ground)\nx, y         design position\nm            mass\nvx0, vy0   start velocity\nrail         free / X / Y\ndriveF, driveAngle, driveW", anchor="start", mono=True)
    f.box(220, 10, 190, 150, "", fill=G1)
    f.text(315, 26, "Link", fs=13, bold=True)
    mtext(f, 228, 52, "kind  (Spring / Damper)\na, b         node indices\nk, k3, L0   spring data\nautoRest    follow drawing?\nc            damper data", anchor="start", mono=True)
    f.box(430, 10, 180, 150, "", fill=G1)
    f.text(520, 26, "State", fs=13, bold=True)
    mtext(f, 438, 52, "q[2i], q[2i+1]\n  = x_i, y_i\nv[2i], v[2i+1]\n  = vx_i, vy_i\nt   time", anchor="start", mono=True)
    f.box(10, 190, 400, 120, "", fill=G2)
    f.text(210, 206, "Model", fs=13, bold=True)
    mtext(f, 20, 232, "vector<Node> nodes      all masses and grounds\nvector<Link> links      all springs and dampers\ndouble gravity\naddMass addGround addLink removeNode removeLink\nrefreshAutoRest  massCount  distance", anchor="start", mono=True)
    f.box(430, 190, 180, 120, "", fill=G1)
    f.text(520, 206, "Meter", fs=13, bold=True)
    mtext(f, 438, 232, "T V E\ndissipated\nworkIn   E0\nresidual()", anchor="start", mono=True)
    f.line(105, 160, 105, 190, arrow=True); f.line(315, 160, 315, 190, arrow=True)
    return f


@fig("gl-dft")
def _():
    f = F(620, 260)
    mx, my = axes(f, 50, 15, 240, 160, (0, 6), (-1.2, 1.2), "time (s)", "signal", xt=3, yt=2)
    ts = [i / 60 for i in range(361)]
    sig = [math.sin(2 * math.pi * 2.5 * t) * math.exp(-0.3 * t) for t in ts]
    curve(f, mx, my, ts, sig, sw=1.4)
    # spectrum
    N = len(sig)
    mean = sum(sig) / N
    fr = [0.05 + i * 0.05 for i in range(120)]
    mags = []
    for fq in fr:
        re = im = 0.0
        for i, s in enumerate(sig):
            w = 0.5 - 0.5 * math.cos(2 * math.pi * i / (N - 1))
            a = 2 * math.pi * fq * i / 60.0
            re += (s - mean) * w * math.cos(a)
            im -= (s - mean) * w * math.sin(a)
        mags.append(math.hypot(re, im))
    mm = max(mags)
    mx2, my2 = axes(f, 360, 15, 240, 160, (0, 6), (0, 1.05), "frequency (Hz)", "strength", xt=3, yt=2)
    curve(f, mx2, my2, fr, [m / mm for m in mags], sw=1.6)
    pk = fr[mags.index(mm)]
    f.line(mx2(pk) + 12, my2(1.0) + 8, mx2(pk) + 2, my2(1.0) + 2, arrow=True)
    f.text(mx2(pk) + 14, my2(1.0) + 14, f"peak = {pk:.2f} Hz", fs=11.5, anchor="start", bold=True)
    f.text(310, 245, "The frequency finder tries many frequencies and keeps the one that matches the wiggle best. True answer: 2.5 Hz.", fs=11, italic=True)
    return f


# ------------------------------------------------------------------ interface chapters
@fig("gl-msgloop")
def _():
    f = F(620, 340)
    f.box(220, 8, 180, 34, "main(): create window", fs=12, fill=G2, bold=True)
    f.box(220, 66, 180, 40, "PeekMessage: any mouse,\nkey, resize messages?", fs=11.5, fill=G1)
    f.box(430, 66, 170, 40, "WndProc → onMouseDown,\nonKey, onWheel ...", fs=11, fill=G2)
    f.box(220, 130, 180, 34, "dt = now − last", fs=12, fill=G1, mono=True)
    f.box(220, 186, 180, 40, "advanceSim(dt)\n(or exactly 1/60 s if recording)", fs=11, fill=G2)
    f.box(220, 250, 180, 34, "present(): draw + show", fs=12, fill=G2)
    f.box(430, 250, 170, 34, "captureFrame() → .mp4", fs=11.5, fill=G1)
    f.box(220, 304, 180, 28, "Sleep, then repeat", fs=12, fill=G1)
    for y in (42, 106, 164, 226, 284):
        f.line(310, y, 310, y + 24 if y != 284 else y + 20, arrow=True)
    f.line(400, 86, 430, 86, arrow=True)
    f.poly([(515, 106), (515, 118), (310, 118), (310, 130)], arrow=False, dash=True)
    f.line(400, 267, 430, 267, arrow=True)
    f.poly([(220, 318), (60, 318), (60, 86), (220, 86)], arrow=True, dash=True)
    f.text(30, 200, "forever\nuntil the\nwindow\ncloses", fs=11, italic=True)
    return f


@fig("gl-layers")
def _():
    f = F(560, 300)
    layers = ["7  Right panel: buttons, sliders", "6  Telemetry box (HUD) + toast + REC badge", "5  Plots and phase portrait", "4  Tool previews, mouse spring", "3  Masses and ground anchors", "2  Springs and dampers", "1  Grid + floor + background colour"]
    for i, t in enumerate(layers):
        y = 10 + (6 - i) * 38
        f.box(30 + (6 - i) * 6, y, 500 - (6 - i) * 12, 32, t, fs=12, fill=[G1, G2, G1, G2, G1, G2, G3][i], anchor="start")
    f.text(280, 290, "Painter's algorithm: draw the back first, the front last (bottom of the stack is drawn first).", fs=11, italic=True)
    return f


@fig("gl-layout")
def _():
    f = F(620, 330)
    f.box(10, 10, 600, 310, "", fill="#fff", sw=2)
    f.box(10, 10, 440, 175, "", fill=G1)
    f.text(230, 80, "SCENE  (sceneRect)\nwidth = W − 300\nheight = H × 0.56", fs=13, bold=True)
    f.box(20, 20, 150, 46, "HUD box", fs=11.5, fill="#fff")
    f.box(10, 185, 255, 135, "TIME PLOT  (timeRect)\nwith 4 tabs above it", fs=12, fill=G2)
    f.box(265, 185, 185, 135, "PHASE PLOT\n(phaseRect)", fs=12, fill=G1)
    f.box(450, 10, 160, 310, "", fill=G2)
    f.text(530, 40, "PANEL\nwidth = 300 px", fs=13, bold=True)
    for i, t in enumerate(["tools", "simulation", "design file", "examples", "properties\n(sliders)"]):
        f.box(460, 70 + i * 48, 140, 38, t, fs=11, fill="#fff")
    f.text(230, 177, "", fs=10)
    return f


@fig("gl-view")
def _():
    f = F(620, 270)
    f.text(155, 18, "world (metres, y down)", fs=12.5, bold=True)
    f.line(20, 40, 280, 40, arrow=True); f.line(20, 40, 20, 240, arrow=True)
    f.text(285, 52, "x", fs=12, italic=True); f.text(10, 245, "y", fs=12, italic=True)
    f.circle(150, 130, 5, fill="#000"); f.text(158, 148, "(1.5 , 1.0) m", fs=11.5, anchor="start")
    f.line(20, 40, 150, 130, dash=True, sw=1)
    f.line(310, 15, 310, 255, dash=True, sw=0.8)
    f.text(465, 18, "screen (pixels, y down)", fs=12.5, bold=True)
    f.box(340, 40, 260, 200, "", fill="#fff")
    f.circle(430, 120, 4, "", fill="#000"); f.text(436, 108, "(ox , oy) = world origin", fs=11, anchor="start")
    f.circle(530, 190, 5, fill="#000"); f.text(530, 208, "sx = ox + 1.5·scale\nsy = oy + 1.0·scale", fs=11)
    f.line(430, 120, 530, 190, dash=True, sw=1)
    f.text(465, 258, "scale = pixels per metre (80 normally)", fs=11.5, italic=True)
    f.line(285, 130, 345, 130, arrow=True)
    return f


@fig("gl-modes")
def _():
    f = F(620, 230)
    f.box(30, 80, 140, 60, "DESIGN\n(edit the layout)", fs=13, fill=G2, bold=True)
    f.box(240, 20, 140, 60, "RUN\n(physics moves)", fs=13, fill=G2, bold=True)
    f.box(450, 80, 140, 60, "PAUSE\n(frozen, look around)", fs=12, fill=G2, bold=True)
    f.line(170, 95, 240, 55, arrow=True); f.text(180, 62, "Run / Space", fs=11, anchor="start")
    f.line(380, 50, 450, 90, arrow=True); f.text(430, 50, "Pause / Space", fs=11, anchor="start")
    f.line(450, 120, 380, 78, arrow=True, dash=True)
    f.poly([(310, 80), (310, 175), (100, 175), (100, 140)], arrow=True); f.text(205, 192, "Stop / Esc (from RUN)", fs=11)
    f.poly([(520, 140), (520, 205), (110, 205), (110, 140)], arrow=True, dash=True); f.text(300, 222, "Stop (from PAUSE)", fs=11)
    return f


@fig("gl-hit")
def _():
    f = F(620, 250)
    f.text(150, 18, "clicking a mass or anchor", fs=12.5, bold=True)
    f.circle(150, 120, 45, "", fill=G1); f.circle(150, 120, 3, fill="#000")
    f.circle(215, 95, 3, fill="#000"); f.line(150, 120, 215, 95, dash=True)
    f.text(190, 120, "d", fs=12, italic=True); f.text(150, 188, "hit  if  d ≤ radius + 4 pixels", fs=11.5)
    f.line(310, 15, 310, 235, dash=True, sw=0.8)
    f.text(465, 18, "clicking a spring or damper", fs=12.5, bold=True)
    f.circle(350, 170, 4, "", fill="#000"); f.circle(590, 80, 4, fill="#000"); f.line(350, 170, 590, 80, sw=2.4)
    f.circle(460, 70, 3, fill="#000"); f.text(468, 62, "click P", fs=11.5, anchor="start")
    f.line(460, 70, 500, 119, dash=True); f.circle(500, 119, 3, fill="#000")
    f.text(505, 92, "d", fs=12, italic=True)
    f.text(470, 205, "project P on the segment, clamp to its ends,\nhit if the distance d < 9 pixels", fs=11.5)
    return f


@fig("gl-mousespring")
def _():
    f = F(560, 200)
    wall(f, 25, 100, 60, "L")
    f.poly(spring_path(25, 120, 100, 5, 8))
    f.box(120, 80, 40, 40, "m", fs=13, fill=G2)
    f.line(160, 100, 380, 100, dash=True, sw=1.6)
    f.circle(380, 100, 7, fill="#000")
    f.text(270, 80, "mouse spring (invisible force)", fs=11.5, italic=True)
    f.text(270, 130, "F = m·( 700·(target − q) − 45·v )", fs=12.5, bold=True)
    f.text(280, 170, "Stiff enough to follow the cursor, damped enough not to ring: it feels like grabbing the mass.", fs=11, italic=True)
    return f


# ------------------------------------------------------------------ recorder
@fig("gl-recpipe")
def _():
    f = F(620, 250)
    steps = [("Canvas\n(32-bit pixels,\ntop row first)", G1), ("Crop to\nscene + plots,\nflip rows", G2), ("Media buffer\n+ sample with\ntime stamp", G1), ("Sink Writer", G2), ("H.264 encoder\n(built into\nWindows)", G2), ("MP4 file", G1)]
    for i, (t, fl) in enumerate(steps):
        x = 8 + i * 102
        f.box(x, 50, 92, 74, t, fs=10.5, fill=fl, bold=(fl == G2))
        if i < 5:
            f.line(x + 92, 87, x + 102, 87, arrow=True)
    f.text(310, 160, "addFrame()  →  MFCreateMemoryBuffer / MFCreateSample / SetSampleTime  →  WriteSample()  →  Finalize()", fs=11.5, mono=False)
    f.text(310, 190, "30 or 60 times per second, one picture at a time. Finalize() writes the index at the end.", fs=11.5, italic=True)
    f.box(60, 210, 500, 30, "Test result: a 6 s recording = 360 frames, ≈ 5 MB, decoded again and found correct.", fs=11.5, fill="#fff", dash=True)
    return f


@fig("gl-rowflip")
def _():
    f = F(560, 220)
    f.text(130, 16, "How the canvas stores rows", fs=12.5, bold=True)
    for i in range(5):
        f.box(40, 30 + i * 30, 180, 26, f"row {i}" + ("   (top of the picture)" if i == 0 else ""), fs=11, fill=G1 if i % 2 else G2, anchor="start")
    f.line(130, 184, 130, 184)
    f.text(130, 200, "top row first", fs=11.5, italic=True)
    f.line(250, 100, 310, 100, arrow=True); f.text(280, 86, "flip", fs=12, bold=True)
    f.text(450, 16, "What Media Foundation wants", fs=12.5, bold=True)
    for i in range(5):
        f.box(370, 30 + i * 30, 170, 26, f"row {4 - i}" + ("   (bottom of the picture)" if i == 0 else ""), fs=11, fill=G1 if i % 2 else G2, anchor="start")
    f.text(455, 200, "bottom row first", fs=11.5, italic=True)
    return f


@fig("gl-recclock")
def _():
    f = F(620, 240)
    f.text(10, 18, "Live (no recording): physics advances by the REAL time between frames", fs=12, anchor="start", bold=True)
    f.line(10, 50, 610, 50, sw=1)
    for x in (20, 90, 150, 235, 300, 390, 450, 560):
        f.line(x, 40, x, 60, sw=2)
    f.text(310, 76, "uneven gaps (the computer was busy now and then) — fine for watching", fs=11, italic=True)
    f.text(10, 120, "Recording: every frame advances EXACTLY 1/60 s of simulated time", fs=12, anchor="start", bold=True)
    f.line(10, 150, 610, 150, sw=1)
    for i in range(0, 60):
        f.line(10 + i * 10, 142 if i % 5 else 138, 10 + i * 10, 158 if i % 5 else 162, sw=1)
    f.text(310, 186, "so the video plays at the right speed, however slowly the computer drew the frames", fs=11, italic=True)
    f.box(60, 200, 500, 30, "video time = frame number × (1/60 s)   — the time stamp we give each frame", fs=12, fill=G1)
    return f


@fig("gl-mp4")
def _():
    f = F(560, 200)
    parts = [("ftyp\nfile type", 20, 80, G1), ("mdat  —  the compressed pictures, frame after frame", 100, 300, G2), ("moov\nindex: where each frame is,\nhow long, how big", 400, 140, G1)]
    for t, x, w, fl in parts:
        f.box(x, 50, w, 80, t, fs=11.5, fill=fl)
    f.text(280, 20, "An .mp4 file is a row of 'boxes'", fs=13, bold=True)
    f.text(280, 160, "If the program dies before Finalize(), the moov box is never written and players cannot open the file.", fs=11.5, italic=True)
    f.text(470, 148, "written last", fs=11, italic=True)
    return f


@fig("gl-linkdraw")
def _():
    f = F(620, 250)
    f.text(155, 16, "1. Draw a spring flat along x", fs=12.5, bold=True)
    f.poly(spring_path(30, 270, 80, 8, 12), sw=1.8); f.circle(30, 80, 4, fill="#000"); f.circle(270, 80, 4, fill="#000")
    f.text(155, 118, "coil(x1 = 0 ... x2 = length, y = 0)", fs=11, mono=False)
    f.line(310, 10, 310, 240, dash=True, sw=0.8)
    f.text(465, 16, "2. Move + rotate the whole canvas", fs=12.5, bold=True)
    f.line(350, 215, 560, 80, sw=1, dash=True)
    ang = -math.atan2(135, 210)
    pts = []
    for (px, py) in spring_path(0, 250, 0, 8, 12):
        x = 350 + px * math.cos(ang) - py * math.sin(ang)
        y = 215 + px * math.sin(ang) + py * math.cos(ang)
        pts.append((round(x, 1), round(y, 1)))
    f.poly(pts, sw=1.8)
    f.circle(350, 215, 4, fill="#000")
    f.text(380, 232, "TranslateTransform(a)", fs=11, anchor="start"); f.text(500, 150, "RotateTransform(angle)", fs=11, anchor="start")
    f.text(310, 242, "", fs=10)
    return f


@fig("gl-tests")
def _():
    f = F(620, 270)
    tests = ["1 one mass vs textbook formula", "2 energy law (1 mass)", "3 driven 2-mass + mouse force", "4 swinging spring keeps energy", "5 Y-rail never drifts sideways", "6 frequency finder", "7 save/load round trip", "8 H.264 encoder", "9 design by clicking, then run"]
    for i, t in enumerate(tests):
        x = 10 + (i % 3) * 205
        y = 12 + (i // 3) * 70
        f.box(x, y, 195, 56, t, fs=11.5, fill=G1 if (i // 3) % 2 == 0 else G2)
    f.text(310, 235, "vibration_live --selftest  runs all nine without opening a window", fs=12, mono=False, bold=True)
    f.text(310, 255, "(it exits with a non-zero code if any check fails, so it can be used by scripts)", fs=11, italic=True)
    return f
