// render.hpp -- everything that gets PAINTED. Reads the state in app.hpp, never changes it.
// Included by live.cpp after app.hpp.
#pragma once
#include <algorithm>
#include <cstdarg>

using namespace Gdiplus;

// ------------------------------------------------------------------ fonts, colours, tiny helpers
static FontFamily* gMono = nullptr;
static Font *fSmall, *fNorm, *fBig;

static Color C(int r, int g, int b, int a = 255) { return Color((BYTE)a, (BYTE)r, (BYTE)g, (BYTE)b); }
static const Color MASS_COL[8] = {Color(255, 53, 116, 240), Color(255, 232, 88, 84), Color(255, 46, 190, 110), Color(255, 235, 170, 20),
                                  Color(255, 150, 100, 250), Color(255, 20, 180, 210), Color(255, 220, 60, 150), Color(255, 160, 160, 170)};
static Color massColor(int nodeIndex) {          // colour by order among the masses, so it matches plots and blocks
    int k = 0;
    for (int i = 0; i < nodeIndex; ++i) k += (A.model.nodes[i].kind == lag::NodeKind::Mass);
    return MASS_COL[k % 8];
}
static int massOrdinal(int nodeIndex) {
    int k = 0;
    for (int i = 0; i < nodeIndex; ++i) k += (A.model.nodes[i].kind == lag::NodeKind::Mass);
    return k;
}

static void text(Graphics& g, const char* s, REAL x, REAL y, Font* f, Color c, StringAlignment al = StringAlignmentNear) {
    WCHAR w[256];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 256);
    SolidBrush b(c);
    StringFormat sf;
    sf.SetAlignment(al);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(w, -1, f, PointF(x, y), &sf, &b);
}
static void textf(Graphics& g, REAL x, REAL y, Font* f, Color c, const char* fmtStr, ...) {
    char buf[200];
    va_list ap; va_start(ap, fmtStr);
    std::vsnprintf(buf, sizeof buf, fmtStr, ap);
    va_end(ap);
    text(g, buf, x, y, f, c);
}

// A zig-zag spring drawn along the x axis from x1 to x2 at height y (we rotate the canvas to aim it).
static void coil(Graphics& g, Pen& pen, double x1, double x2, double y, int coils, double amp) {
    std::vector<PointF> p;
    double len = x2 - x1, lead = std::fmin(10.0, len * 0.15), span = len - 2 * lead;
    p.push_back(PointF((REAL)x1, (REAL)y));
    p.push_back(PointF((REAL)(x1 + lead), (REAL)y));
    for (int i = 0; i < coils * 2; ++i)
        p.push_back(PointF((REAL)(x1 + lead + span * (i + 0.5) / (coils * 2)), (REAL)(y + (i % 2 ? amp : -amp))));
    p.push_back(PointF((REAL)(x2 - lead), (REAL)y));
    p.push_back(PointF((REAL)x2, (REAL)y));
    g.DrawLines(&pen, p.data(), (INT)p.size());
}
// A dashpot (cylinder + piston) drawn along the x axis.
static void dashpot(Graphics& g, Pen& pen, double x1, double x2, double y) {
    double len = x2 - x1, cl = std::fmax(14.0, len * 0.5), c0 = x1 + 8, c1 = std::fmin(c0 + cl, x2 - 8);
    double p = clampd(x2 - 14, c0 + 6, c1 - 2);
    g.DrawLine(&pen, (REAL)x1, (REAL)y, (REAL)c0, (REAL)y);
    PointF cyl[4] = {PointF((REAL)c1, (REAL)(y - 8)), PointF((REAL)c0, (REAL)(y - 8)), PointF((REAL)c0, (REAL)(y + 8)), PointF((REAL)c1, (REAL)(y + 8))};
    g.DrawLines(&pen, cyl, 4);
    g.DrawLine(&pen, (REAL)x2, (REAL)y, (REAL)p, (REAL)y);
    g.DrawLine(&pen, (REAL)p, (REAL)(y - 6), (REAL)p, (REAL)(y + 6));
}
static void panelFrame(Graphics& g, RectF r, const char* title) {
    SolidBrush bg(C(20, 23, 29));
    Pen pen(C(52, 58, 70), 1.0f);
    g.FillRectangle(&bg, r);
    g.DrawRectangle(&pen, r.X, r.Y, r.Width, r.Height);
    if (title && *title) text(g, title, r.X, r.Y - 18, fSmall, C(150, 158, 175));
}

// ------------------------------------------------------------------ the drawing area
static void drawNodeBody(Graphics& g, int i) {
    const lag::Node& n = A.model.nodes[i];
    double wx, wy; nodeWorld(i, wx, wy);
    float x = (float)toScreenX(wx), y = (float)toScreenY(wy);
    bool selected = (A.selKind == SEL_NODE && A.sel == i);
    if (n.kind == lag::NodeKind::Ground) {                       // a fixed anchor: pin + hatched base
        Pen base(C(220, 224, 232), 2.5f), hatch(C(110, 116, 130), 1.5f);
        g.DrawLine(&base, x - 16, y + 8, x + 16, y + 8);
        for (int k = -16; k < 16; k += 6) g.DrawLine(&hatch, x + k + 6, y + 8, x + k, y + 16);
        SolidBrush pin(selected ? C(90, 205, 255) : C(235, 238, 246));
        g.FillEllipse(&pin, x - 6, y - 6, 12.0f, 12.0f);
        Pen ring(C(10, 12, 16), 2.0f);
        g.DrawEllipse(&ring, x - 6, y - 6, 12.0f, 12.0f);
        return;
    }
    float R = (float)nodeRadius(i);
    int rail = (int)std::lround(n.rail);
    if (rail != lag::FREE && A.mode == MODE_EDIT) {              // faint rail guide
        Pen rp(C(70, 76, 92), 1.0f);
        rp.SetDashStyle(DashStyleDash);
        if (rail == lag::RAIL_X) g.DrawLine(&rp, x - 90, y, x + 90, y); else g.DrawLine(&rp, x, y - 90, x, y + 90);
    }
    if (A.mode != MODE_EDIT) {                                   // a ghost where it started
        Pen gp(C(60, 66, 80), 1.0f);
        gp.SetDashStyle(DashStyleDot);
        g.DrawRectangle(&gp, (REAL)toScreenX(n.x) - R, (REAL)toScreenY(n.y) - R, 2 * R, 2 * R);
    }
    RectF r(x - R, y - R, 2 * R, 2 * R);
    Color base = massColor(i);
    LinearGradientBrush gb(r, Color(255, (BYTE)std::min(255, base.GetR() + 55), (BYTE)std::min(255, base.GetG() + 55), (BYTE)std::min(255, base.GetB() + 55)), base, LinearGradientModeVertical);
    g.FillRectangle(&gb, r);
    Pen outline(selected ? C(255, 255, 255) : C(10, 12, 16), selected ? 3.0f : 2.0f);
    g.DrawRectangle(&outline, r);
    char b[40];
    std::snprintf(b, sizeof b, "m%d", massOrdinal(i));
    text(g, b, x, y - 15, fNorm, C(255, 255, 255), StringAlignmentCenter);
    std::snprintf(b, sizeof b, "%.3gkg", n.m);
    text(g, b, x, y + 1, fSmall, C(255, 255, 255), StringAlignmentCenter);
}

static void drawLink(Graphics& g, int li, double forceRef) {
    const lag::Link& L = A.model.links[li];
    double ax, ay, bx, by; nodeWorld(L.a, ax, ay); nodeWorld(L.b, bx, by);
    float pax = (float)toScreenX(ax), pay = (float)toScreenY(ay), pbx = (float)toScreenX(bx), pby = (float)toScreenY(by);
    double dx = pbx - pax, dy = pby - pay, len = std::hypot(dx, dy);
    if (len < 4) return;
    double ux = dx / len, uy = dy / len;
    // links that join the same two nodes are fanned out sideways so they do not overlap
    int same = 0, before = 0;
    for (int j = 0; j < (int)A.model.links.size(); ++j) {
        const lag::Link& o = A.model.links[j];
        if ((o.a == L.a && o.b == L.b) || (o.a == L.b && o.b == L.a)) { if (j < li) ++before; ++same; }
    }
    double off = (before - (same - 1) / 2.0) * 16.0;
    double ra = nodeRadius(L.a), rb = nodeRadius(L.b);
    bool sel = (A.selKind == SEL_LINK && A.sel == li);

    Color col = (L.kind == lag::LinkKind::Spring) ? C(235, 238, 246) : C(150, 158, 175);
    if (A.mode != MODE_EDIT && L.kind == lag::LinkKind::Spring) {          // colour the spring by how hard it pulls (warm) or pushes (cool)
        lag::LinkInfo in = lag::linkInfo(A.model, A.st.q, A.st.v, li);
        double f = clampd(in.force / forceRef, -1, 1);
        col = f >= 0 ? C((int)(235 + 20 * f), (int)(238 - 110 * f), (int)(246 - 190 * f)) : C((int)(235 + 150 * f), (int)(238 + 20 * f), 246);
    }
    if (sel) col = C(90, 205, 255);
    Pen pen(col, sel ? 2.8f : 2.2f);

    GraphicsState state = g.Save();
    g.TranslateTransform((REAL)(pax - uy * off), (REAL)(pay + ux * off));      // move to the (offset) start
    g.RotateTransform((REAL)(std::atan2(dy, dx) * 180.0 / lag::PI));            // point the x axis along the link
    double s0 = std::fmin(ra, len * 0.4), s1 = len - std::fmin(rb, len * 0.4);
    if (s1 - s0 < 12) {
        g.DrawLine(&pen, (REAL)s0, 0.0f, (REAL)s1, 0.0f);
    } else if (L.kind == lag::LinkKind::Spring) {
        coil(g, pen, s0, s1, 0, (int)clampd(std::lround((s1 - s0) / 16.0), 3, 22), 8.0);
    } else {
        dashpot(g, pen, s0, s1, 0);
    }
    g.Restore(state);
}

static void drawScene(Graphics& g) {
    RectF sc = sceneRect();
    g.SetClip(sc);
    // grid: a line every metre, brighter through the origin
    Pen grid(C(26, 30, 38), 1.0f), axis(C(44, 50, 62), 1.0f);
    double x0 = toWorldX(0), x1 = toWorldX(sc.Width), y0 = toWorldY(0), y1 = toWorldY(sc.Height);
    for (int k = (int)std::floor(x0); k <= (int)std::ceil(x1); ++k) g.DrawLine(k == 0 ? &axis : &grid, (REAL)toScreenX(k), 0.0f, (REAL)toScreenX(k), sc.Height);
    for (int k = (int)std::floor(y0); k <= (int)std::ceil(y1); ++k) g.DrawLine(k == 0 ? &axis : &grid, 0.0f, (REAL)toScreenY(k), sc.Width, (REAL)toScreenY(k));

    if (A.model.nodes.empty() && A.mode == MODE_EDIT) {
        text(g, "Empty canvas.  Pick  Ground  or  Mass  on the right, then click here.", sc.Width / 2, sc.Height / 2 - 20, fNorm, C(150, 158, 175), StringAlignmentCenter);
        text(g, "Pick  Spring  or  Damper, then click two things to join them.  Press  Run.", sc.Width / 2, sc.Height / 2 + 4, fSmall, C(110, 118, 135), StringAlignmentCenter);
    }
    // reference for spring colours: the biggest spring force seen recently (never below 5 N)
    double forceRef = 5;
    if (!A.log.empty())
        for (size_t li = 0; li < A.model.links.size(); ++li)
            if (A.model.links[li].kind == lag::LinkKind::Spring && 3 * li + 1 < A.log.back().lk.size())
                forceRef = std::fmax(forceRef, std::fabs(A.log.back().lk[3 * li + 1]) * 1.5);
    for (int li = 0; li < (int)A.model.links.size(); ++li) drawLink(g, li, forceRef);
    for (int i = 0; i < (int)A.model.nodes.size(); ++i) drawNodeBody(g, i);

    // tool previews
    bool over = inScene(A.mx, A.my);
    if (over && A.mode == MODE_EDIT && (A.tool == TOOL_MASS || A.tool == TOOL_GROUND) && hitNode(A.mx, A.my) < 0) {
        float x = (float)toScreenX(snap(toWorldX(A.mx))), y = (float)toScreenY(snap(toWorldY(A.my)));
        Pen gp(C(90, 205, 255, 160), 1.5f);
        gp.SetDashStyle(DashStyleDash);
        if (A.tool == TOOL_MASS) g.DrawRectangle(&gp, x - 20, y - 20, 40.0f, 40.0f); else g.DrawEllipse(&gp, x - 8, y - 8, 16.0f, 16.0f);
    }
    if (A.linkStart >= 0 && A.linkStart < (int)A.model.nodes.size()) {
        double ax, ay; nodeWorld(A.linkStart, ax, ay);
        Pen rb(C(90, 205, 255), 1.8f);
        rb.SetDashStyle(DashStyleDot);
        g.DrawLine(&rb, (REAL)toScreenX(ax), (REAL)toScreenY(ay), (REAL)A.mx, (REAL)A.my);
        text(g, A.tool == TOOL_SPRING ? "click the second end of the spring" : "click the second end of the damper", (REAL)A.mx + 12, (REAL)A.my - 18, fSmall, C(90, 205, 255));
    }
    if (A.mode == MODE_RUN && A.dragNode >= 0 && A.model.nodes[A.dragNode].kind == lag::NodeKind::Mass) {   // the mouse spring
        double x, y; nodeWorld(A.dragNode, x, y);
        Pen mp(C(255, 110, 100), 1.6f);
        mp.SetDashStyle(DashStyleDot);
        g.DrawLine(&mp, (REAL)toScreenX(x), (REAL)toScreenY(y), (REAL)A.mx, (REAL)A.my);
        SolidBrush hb(C(255, 110, 100));
        g.FillEllipse(&hb, (REAL)A.mx - 6, (REAL)A.my - 6, 12.0f, 12.0f);
    }
    g.ResetClip();
    Pen divider(C(52, 58, 70), 1.0f);
    g.DrawLine(&divider, 0.0f, sc.Height, sc.Width, sc.Height);
}

// ------------------------------------------------------------------ telemetry readout (top-left of the drawing area)
static void drawHud(Graphics& g) {
    const lag::Model& M = A.model;
    RectF hud(12, 10, 392, 170);
    SolidBrush hudBg(C(8, 10, 14, 215));
    Pen hudPen(C(210, 214, 224), 1.2f);
    g.FillRectangle(&hudBg, hud);
    g.DrawRectangle(&hudPen, hud);
    const char* mode = A.mode == MODE_EDIT ? "DESIGN" : (A.mode == MODE_RUN ? "RUNNING" : "PAUSED");
    Color mc = A.mode == MODE_EDIT ? C(255, 214, 120) : (A.mode == MODE_RUN ? C(110, 230, 150) : C(255, 120, 110));
    text(g, "VIBRATION LAB  //  TELEMETRY", 22, 15, fNorm, C(240, 242, 248));
    text(g, mode, 380, 15, fNorm, mc, StringAlignmentFar);
    float y = 38;
    const Color dim = C(190, 196, 210), hi = C(120, 215, 255);
    textf(g, 22, y, fSmall, dim, "t = %8.3f s     speed %.3gx     FPS %3.0f", A.mode == MODE_EDIT ? 0.0 : A.st.t, A.speed, A.fps); y += 16;
    if (A.mode != MODE_EDIT) {
        textf(g, 22, y, fSmall, dim, "T = %8.3f J   V = %8.3f J", A.meter.T, A.meter.V); y += 16;
        textf(g, 22, y, fSmall, hi,  "E = %8.3f J   dissipated %.3f J", A.meter.E, A.meter.dissipated); y += 16;
        textf(g, 22, y, fSmall, dim, "energy-law error %.1e J   masses %d, links %d", std::fabs(A.meter.residual()), M.massCount(), (int)M.links.size()); y += 20;
    } else {
        textf(g, 22, y, fSmall, dim, "%d masses, %d springs/dampers, gravity %.3g", M.massCount(), (int)M.links.size(), M.gravity); y += 20;
    }
    if (A.selKind == SEL_NODE && A.sel >= 0 && A.sel < (int)M.nodes.size() && M.nodes[A.sel].kind == lag::NodeKind::Mass) {
        int i = A.sel;
        textf(g, 22, y, fNorm, massColor(i), "selected m%d  (%.3g kg)", massOrdinal(i), M.nodes[i].m); y += 18;
        if (A.mode != MODE_EDIT && 2 * i + 1 < (int)A.st.q.size()) {
            double vx = A.st.v[2 * i], vy = A.st.v[2 * i + 1];
            std::vector<double> a;
            lag::accel(M, A.st.t, A.st.q, A.st.v, A.ext, a);
            textf(g, 22, y, fSmall, dim, "x %+7.3f  y %+7.3f m   |v| %6.3f m/s", A.st.q[2 * i] - M.nodes[i].x, A.st.q[2 * i + 1] - M.nodes[i].y, std::hypot(vx, vy)); y += 16;
            textf(g, 22, y, fSmall, dim, "|a| %6.3f m/s2     dominant freq %s", std::hypot(a[2 * i], a[2 * i + 1]),
                  A.domFreq > 0 ? (std::to_string(A.domFreq).substr(0, 5) + " Hz").c_str() : "--");
        } else {
            textf(g, 22, y, fSmall, dim, "press Run to see x, v, a and frequency");
        }
    } else if (A.selKind == SEL_NODE && A.sel >= 0 && A.sel < (int)M.nodes.size()) {
        textf(g, 22, y, fNorm, C(220, 224, 232), "selected ground anchor"); y += 18;
        textf(g, 22, y, fSmall, dim, "at (%.2f, %.2f) m  - never moves", M.nodes[A.sel].x, M.nodes[A.sel].y);
    } else if (A.selKind == SEL_LINK && A.sel >= 0 && A.sel < (int)M.links.size()) {
        const lag::Link& L = M.links[A.sel];
        textf(g, 22, y, fNorm, C(90, 205, 255), "selected %s", L.kind == lag::LinkKind::Spring ? "spring" : "damper"); y += 18;
        if (A.mode != MODE_EDIT) {
            lag::LinkInfo in = lag::linkInfo(M, A.st.q, A.st.v, A.sel);
            if (L.kind == lag::LinkKind::Spring) {
                textf(g, 22, y, fSmall, dim, "length %.3f m   stretch %+.3f m", in.len, in.stretch); y += 16;
                textf(g, 22, y, fSmall, dim, "force %+8.3f N  (+ = pulling)", in.force);
            } else {
                textf(g, 22, y, fSmall, dim, "length %.3f m   rate %+.3f m/s", in.len, in.rate); y += 16;
                textf(g, 22, y, fSmall, dim, "force %+8.3f N   power lost %.3f W", in.force, L.c * in.rate * in.rate);
            }
        } else {
            textf(g, 22, y, fSmall, dim, "drawn length %.3f m", M.distance(L.a, L.b));
        }
    } else {
        text(g, "click something to see its numbers", 22, y, fSmall, C(120, 128, 145));
    }
    // recording badge, top-right of the drawing area
    RectF sc = sceneRect();
    if (A.recording) {
        SolidBrush red(C(235, 60, 60));
        g.FillEllipse(&red, sc.Width - 140, 16.0f, 14.0f, 14.0f);
        textf(g, sc.Width - 120, 14, fNorm, C(255, 130, 120), "REC %02d:%02d", (A.recFrames / 60) / 60, (A.recFrames / 60) % 60);
    }
    text(g, "wheel = zoom   right-drag = pan   Space = run   Esc = stop", 14, sc.Height - 20, fSmall, C(110, 118, 135));
    if (A.toastUntil > nowSeconds()) text(g, A.toastText.c_str(), sc.Width / 2, sc.Height - 48, fNorm, C(110, 230, 150), StringAlignmentCenter);
}

// ------------------------------------------------------------------ plots
struct Series { std::string name; Color col; std::vector<double> v; };

// Draw several curves against time (the last PLOT_SECONDS seconds of the log).
static void drawTimePlot(Graphics& g, RectF r, const std::vector<Series>& series, const std::vector<double>& times, int modeIndex, bool symmetric) {
    g.SetClip(r);
    double lo = 1e300, hi = -1e300;
    for (const Series& s : series) for (double v : s.v) { lo = std::fmin(lo, v); hi = std::fmax(hi, v); }
    if (lo > hi) { lo = -1; hi = 1; }
    if (symmetric) { double m = std::fmax(std::fabs(lo), std::fabs(hi)); lo = -m; hi = m; }
    if (hi - lo < 1e-6) { hi += 0.5; lo -= 0.5; }
    double pad = (hi - lo) * 0.1;
    lo -= pad; hi += pad;
    // smooth the vertical range so the picture does not jitter
    double& sm = A.plotScale[modeIndex];
    if (sm <= 1.0001 && sm >= 0.9999) sm = hi - lo;
    sm += ((hi - lo) - sm) * 0.2;
    double mid = (hi + lo) / 2;
    lo = mid - sm / 2; hi = mid + sm / 2;
    double t1 = times.empty() ? 1.0 : times.back(), t0 = t1 - PLOT_SECONDS;
    Pen gridPen(C(34, 39, 48), 1.0f);
    for (int k = 0; k <= 4; ++k) {
        double v = lo + (hi - lo) * k / 4;
        float yy = (float)(r.Y + r.Height * (1 - (v - lo) / (hi - lo)));
        g.DrawLine(&gridPen, r.X, yy, r.X + r.Width, yy);
        textf(g, r.X + 4, yy - 14, fSmall, C(120, 128, 145), "%.3g", v);
    }
    if (lo < 0 && hi > 0) {
        Pen zero(C(70, 76, 92), 1.0f);
        float yy = (float)(r.Y + r.Height * (1 - (0 - lo) / (hi - lo)));
        g.DrawLine(&zero, r.X, yy, r.X + r.Width, yy);
    }
    for (const Series& s : series) {
        std::vector<PointF> p;
        for (size_t i = 0; i < s.v.size() && i < times.size(); ++i)
            p.push_back(PointF((REAL)(r.X + (times[i] - t0) / PLOT_SECONDS * r.Width), (REAL)(r.Y + r.Height * (1 - (s.v[i] - lo) / (hi - lo)))));
        Pen lp(s.col, 1.8f);
        if (p.size() > 1) g.DrawLines(&lp, p.data(), (INT)p.size());
    }
    float lx = r.X + 60;
    for (const Series& s : series) {                    // legend
        SolidBrush b(s.col);
        g.FillRectangle(&b, lx, r.Y + 7, 10.0f, 10.0f);
        text(g, s.name.c_str(), lx + 14, r.Y + 3, fSmall, C(190, 196, 210));
        lx += 24 + 8.0f * (float)s.name.size();
    }
    g.ResetClip();
}

static void drawPlots(Graphics& g) {
    const lag::Model& M = A.model;
    RectF tr = timeRect(), pr = phaseRect();
    panelFrame(g, tr, "");      // the tab buttons above the plot already say what it shows
    text(g, "last 10 s", tr.X + tr.Width - 8, tr.Y + tr.Height - 18, fSmall, C(110, 118, 135), StringAlignmentFar);

    // collect the frames of the last PLOT_SECONDS
    std::vector<const Frame*> fr;
    if (!A.log.empty()) {
        double tEnd = A.log.back().t;
        for (auto it = A.log.rbegin(); it != A.log.rend() && it->t >= tEnd - PLOT_SECONDS; ++it) fr.push_back(&*it);
        std::reverse(fr.begin(), fr.end());
    }
    std::vector<double> times;
    for (const Frame* f : fr) times.push_back(f->t);
    std::vector<Series> series;
    if (A.plotMode == 0 || A.plotMode == 1) {
        for (int i = 0; i < (int)M.nodes.size(); ++i) {
            if (M.nodes[i].kind != lag::NodeKind::Mass) continue;
            int rail = (int)std::lround(M.nodes[i].rail);
            Color c = massColor(i);
            for (int comp = 0; comp < 2; ++comp) {
                if ((comp == 0 && rail == lag::RAIL_Y) || (comp == 1 && rail == lag::RAIL_X)) continue;   // that axis cannot move
                Series s;
                char nm[24];
                std::snprintf(nm, sizeof nm, "m%d.%c", massOrdinal(i), comp == 0 ? 'x' : 'y');
                s.name = nm;
                s.col = comp == 0 ? c : Color(255, (BYTE)(c.GetR() * 0.6), (BYTE)(c.GetG() * 0.6), (BYTE)(c.GetB() * 0.6));
                for (const Frame* f : fr) {
                    size_t b = 4 * (size_t)i;
                    if (b + 3 >= f->nd.size()) { s.v.push_back(0); continue; }
                    s.v.push_back(A.plotMode == 0 ? (f->nd[b + comp] - (comp == 0 ? M.nodes[i].x : M.nodes[i].y)) : f->nd[b + 2 + comp]);
                }
                series.push_back(s);
            }
        }
        drawTimePlot(g, tr, series, times, A.plotMode, true);
    } else if (A.plotMode == 2) {
        Series T{"T kinetic", C(232, 88, 84), {}}, V{"V potential", C(53, 116, 240), {}}, E{"E = T+V", C(235, 238, 246), {}}, D{"dissipated", C(235, 170, 20), {}};
        for (const Frame* f : fr) { T.v.push_back(f->T); V.v.push_back(f->V); E.v.push_back(f->T + f->V); D.v.push_back(f->dissipated); }
        series = {T, V, E, D};
        drawTimePlot(g, tr, series, times, 2, false);
    } else {
        if (A.selKind == SEL_LINK && A.sel >= 0 && A.sel < (int)M.links.size()) {
            size_t li = (size_t)A.sel;
            Series F{M.links[li].kind == lag::LinkKind::Spring ? "spring force N" : "damper force N", C(90, 205, 255), {}};
            Series S{M.links[li].kind == lag::LinkKind::Spring ? "stretch m" : "rate m/s", C(235, 170, 20), {}};
            for (const Frame* f : fr) {
                if (3 * li + 2 >= f->lk.size()) { F.v.push_back(0); S.v.push_back(0); continue; }
                F.v.push_back(f->lk[3 * li + 1]); S.v.push_back(f->lk[3 * li]);
            }
            series = {F, S};
            drawTimePlot(g, tr, series, times, 3, true);
        } else {
            drawTimePlot(g, tr, series, times, 3, true);
            text(g, "select a spring or damper (Select tool) to plot its force", tr.X + tr.Width / 2, tr.Y + tr.Height / 2 - 8, fSmall, C(150, 158, 175), StringAlignmentCenter);
        }
    }

    // phase portrait of the selected mass (or the first mass)
    int pm = -1;
    if (A.selKind == SEL_NODE && A.sel >= 0 && A.sel < (int)M.nodes.size() && M.nodes[A.sel].kind == lag::NodeKind::Mass) pm = A.sel;
    else for (int i = 0; i < (int)M.nodes.size(); ++i) if (M.nodes[i].kind == lag::NodeKind::Mass) { pm = i; break; }
    char title[96];
    if (pm >= 0) std::snprintf(title, sizeof title, "PHASE PORTRAIT of m%d  (velocity vs position)", massOrdinal(pm));
    else std::snprintf(title, sizeof title, "PHASE PORTRAIT");
    panelFrame(g, pr, title);
    if (pm >= 0 && A.log.size() > 2) {
        int comp = ((int)std::lround(M.nodes[pm].rail) == lag::RAIL_Y) ? 1 : 0;
        size_t n = std::min<size_t>(A.log.size(), 700), start = A.log.size() - n;
        double xm = 0.3, vm = 0.3;
        for (size_t i = start; i < A.log.size(); ++i) {
            const Frame& f = A.log[i];
            size_t b = 4 * (size_t)pm;
            if (b + 3 >= f.nd.size()) continue;
            xm = std::fmax(xm, std::fabs(f.nd[b + comp] - (comp == 0 ? M.nodes[pm].x : M.nodes[pm].y)));
            vm = std::fmax(vm, std::fabs(f.nd[b + 2 + comp]));
        }
        xm *= 1.15; vm *= 1.15;
        g.SetClip(pr);
        Pen axis(C(52, 58, 70), 1.0f);
        g.DrawLine(&axis, pr.X, pr.Y + pr.Height / 2, pr.X + pr.Width, pr.Y + pr.Height / 2);
        g.DrawLine(&axis, pr.X + pr.Width / 2, pr.Y, pr.X + pr.Width / 2, pr.Y + pr.Height);
        Color mc = massColor(pm);
        PointF prev; bool havePrev = false;
        for (size_t i = start; i < A.log.size(); ++i) {
            const Frame& f = A.log[i];
            size_t b = 4 * (size_t)pm;
            if (b + 3 >= f.nd.size()) continue;
            PointF p((REAL)(pr.X + pr.Width / 2 + (f.nd[b + comp] - (comp == 0 ? M.nodes[pm].x : M.nodes[pm].y)) / xm * pr.Width / 2),
                     (REAL)(pr.Y + pr.Height / 2 - f.nd[b + 2 + comp] / vm * pr.Height / 2));
            if (havePrev) {
                Pen tp(Color((BYTE)(30 + 225 * (i - start) / n), mc.GetR(), mc.GetG(), mc.GetB()), 1.8f);
                g.DrawLine(&tp, prev, p);
            }
            prev = p; havePrev = true;
        }
        if (havePrev) { SolidBrush dot(C(255, 255, 255)); g.FillEllipse(&dot, prev.X - 4, prev.Y - 4, 8.0f, 8.0f); }
        g.ResetClip();
    }
}

// ------------------------------------------------------------------ right-hand panel
static void drawPanel(Graphics& g) {
    REAL px = (REAL)(W - PANEL_W);
    SolidBrush pbg(C(22, 25, 31));
    g.FillRectangle(&pbg, px, 0.0f, (REAL)PANEL_W, (REAL)H);
    Pen divider(C(52, 58, 70), 1.0f);
    g.DrawLine(&divider, px, 0.0f, px, (REAL)H);
    text(g, "DESIGNER", px + 16, 14, fBig, C(235, 238, 246));
    text(g, "TOOLS", px + 16, 40, fSmall, C(120, 128, 145));
    text(g, "SIMULATION", px + 16, 126, fSmall, C(120, 128, 145));
    text(g, "DESIGN FILE", px + 16, 210, fSmall, C(120, 128, 145));
    text(g, "EXAMPLES", px + 16, 260, fSmall, C(120, 128, 145));

    for (const Btn& b : layoutButtons()) {
        bool hover = A.mx >= b.r.X && A.mx <= b.r.X + b.r.Width && A.my >= b.r.Y && A.my <= b.r.Y + b.r.Height;
        bool rec = (b.id == ACT_RECORD && A.recording);
        Color fill = rec ? C(170, 50, 50) : (b.active ? C(53, 116, 240) : (hover ? C(52, 58, 72) : C(38, 43, 54)));
        SolidBrush bf(fill);
        Pen bp(b.active || rec ? C(120, 170, 255) : C(60, 66, 80), 1.0f);
        g.FillRectangle(&bf, b.r);
        g.DrawRectangle(&bp, b.r);
        text(g, b.label.c_str(), b.r.X + b.r.Width / 2, b.r.Y + b.r.Height / 2 - 8, fSmall, C(240, 242, 248), StringAlignmentCenter);
    }

    // selection title + sliders
    text(g, "PROPERTIES", px + 16, PROPS_Y - 4, fSmall, C(120, 128, 145));
    char s[120];
    const lag::Model& M = A.model;
    if (A.selKind == SEL_NODE && A.sel >= 0 && A.sel < (int)M.nodes.size())
        std::snprintf(s, sizeof s, M.nodes[A.sel].kind == lag::NodeKind::Mass ? "mass m%d" : "ground anchor", massOrdinal(A.sel));
    else if (A.selKind == SEL_LINK && A.sel >= 0 && A.sel < (int)M.links.size())
        std::snprintf(s, sizeof s, "%s", M.links[A.sel].kind == lag::LinkKind::Spring ? "spring" : "damper");
    else std::snprintf(s, sizeof s, "nothing selected (world settings)");
    text(g, s, px + 16, PROPS_Y + 14, fNorm, C(215, 220, 232));
    std::vector<Prop> props = buildProps();
    for (int i = 0; i < (int)props.size(); ++i) {
        const Prop& p = props[i];
        RectF tk = propTrack(i);
        double val = *p.p, frac = clampd((val - p.lo) / (p.hi - p.lo), 0, 1);
        text(g, p.name.c_str(), tk.X, tk.Y - 22, fSmall, C(200, 206, 220));
        if (p.labels) std::snprintf(s, sizeof s, "%s", p.labels[(int)clampd(std::lround(val), 0, 2)]);
        else std::snprintf(s, sizeof s, "%.3g %s", val, p.unit.c_str());
        text(g, s, tk.X + tk.Width, tk.Y - 22, fSmall, C(120, 215, 255), StringAlignmentFar);
        SolidBrush track(C(48, 54, 66)), fill(C(60, 140, 240)), knob(C(245, 247, 252));
        g.FillRectangle(&track, tk);
        g.FillRectangle(&fill, tk.X, tk.Y, (REAL)(tk.Width * frac), tk.Height);
        g.FillEllipse(&knob, (REAL)(tk.X + tk.Width * frac - 7), tk.Y - 3, 14.0f, 14.0f);
    }
    if (A.mode != MODE_EDIT) text(g, "sliders edit the physics LIVE", px + 16, (REAL)(H - 40), fSmall, C(110, 230, 150));
    else text(g, "start v and push apply at Run", px + 16, (REAL)(H - 40), fSmall, C(120, 128, 145));
}

// ------------------------------------------------------------------ the whole picture
static void render(Graphics& g, int w, int h) {
    W = w; H = h;
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    SolidBrush bg(C(12, 14, 18));
    g.FillRectangle(&bg, 0, 0, w, h);
    drawScene(g);
    drawPlots(g);
    drawHud(g);
    drawPanel(g);       // also paints every button, including the plot tabs (they come from the same list)
}
