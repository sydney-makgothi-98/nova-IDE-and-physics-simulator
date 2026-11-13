// app.hpp -- the brain of the interface: what state the program keeps, and what every click / key does.
// (No drawing happens here -- that is render.hpp -- and no Windows plumbing -- that is live.cpp.)
//
// Included by live.cpp AFTER <windows.h> and <gdiplus.h>.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "engine.hpp"
#include "model_io.hpp"

#ifndef LAB_DIR
#define LAB_DIR "."
#endif

// ------------------------------------------------------------------ small vocabulary
static const int    PANEL_W      = 300;     // width of the right-hand panel (pixels)
static const double SCENE_FRAC   = 0.56;    // the drawing area takes this fraction of the window height
static const double PLOT_SECONDS = 10.0;    // the time plot shows the most recent 10 s
static const double SIM_STEP     = 0.001;   // physics step: 1 millisecond
static const size_t LOG_LIMIT    = 36000;   // keep at most 10 minutes of telemetry at 60 samples/s

enum Tool    { TOOL_SELECT, TOOL_MASS, TOOL_GROUND, TOOL_SPRING, TOOL_DAMPER };
enum SelKind { SEL_NONE, SEL_NODE, SEL_LINK };
enum Mode    { MODE_EDIT, MODE_RUN, MODE_PAUSE };

// One row of telemetry: everything we measured at one instant.
struct Frame {
    double t = 0, T = 0, V = 0, dissipated = 0, workIn = 0;
    std::vector<float> nd;   // per node:  x, y, vx, vy
    std::vector<float> lk;   // per link:  stretch (spring) or rate (damper), force, length
};

struct App {
    lag::Model model;
    lag::State st;
    lag::Meter meter;
    Mode mode = MODE_EDIT;
    double simAcc = 0, speed = 1.0;

    Tool tool = TOOL_SELECT;
    SelKind selKind = SEL_NONE;
    int sel = -1;                 // index of the selected node or link
    int linkStart = -1;           // first node clicked while drawing a spring / damper

    double ox = 0, oy = 0, scale = 80.0;      // view: screen = (ox, oy) + world * scale
    double mx = 0, my = 0;                    // mouse position in pixels
    bool panning = false; double panX = 0, panY = 0;
    int dragNode = -1;                        // node being dragged with the mouse
    int dragSlider = -1;                      // slider being dragged
    std::vector<double> ext;                  // live forces on every node (the mouse spring)

    std::deque<Frame> log;
    int plotMode = 0;                         // 0 position, 1 velocity, 2 energy, 3 link force
    double plotScale[4] = {1, 1, 1, 1};
    double domFreq = 0, domFreqAt = 0;

    double fps = 60;
    int stepsLastFrame = 0;
    std::string toastText; double toastUntil = 0;

    // recording (filled in by live.cpp)
    bool recording = false;
    int recFrames = 0;
    std::string recPath;
} A;

static int W = 1320, H = 800;                 // current client size, updated on resize

// ------------------------------------------------------------------ time, maths, toast
inline double nowSeconds() {
    static LARGE_INTEGER f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x; }();
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline void toast(const std::string& s) { A.toastText = s; A.toastUntil = nowSeconds() + 3.5; }

// ------------------------------------------------------------------ screen layout
inline Gdiplus::RectF sceneRect() { return Gdiplus::RectF(0, 0, (Gdiplus::REAL)(W - PANEL_W), (Gdiplus::REAL)(H * SCENE_FRAC)); }
inline Gdiplus::RectF timeRect() {
    double w = W - PANEL_W, top = H * SCENE_FRAC + 44;
    return Gdiplus::RectF(18, (Gdiplus::REAL)top, (Gdiplus::REAL)(w * 0.58 - 26), (Gdiplus::REAL)(H - top - 16));
}
inline Gdiplus::RectF phaseRect() {
    double w = W - PANEL_W, top = H * SCENE_FRAC + 44;
    return Gdiplus::RectF((Gdiplus::REAL)(w * 0.58 + 14), (Gdiplus::REAL)top, (Gdiplus::REAL)(w * 0.42 - 32), (Gdiplus::REAL)(H - top - 16));
}
inline bool inScene(double x, double y) { Gdiplus::RectF r = sceneRect(); return x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height; }

inline void resetView() { Gdiplus::RectF r = sceneRect(); A.ox = r.Width / 2; A.oy = r.Height / 2; A.scale = 80; }

// world (metres) <-> screen (pixels)
inline double toScreenX(double x) { return A.ox + x * A.scale; }
inline double toScreenY(double y) { return A.oy + y * A.scale; }
inline double toWorldX(double sx) { return (sx - A.ox) / A.scale; }
inline double toWorldY(double sy) { return (sy - A.oy) / A.scale; }
inline double snap(double v) { return std::round(v / 0.25) * 0.25; }     // grid of 0.25 m

// where a node is drawn: its live position while a simulation exists, its design position otherwise
inline void nodeWorld(int i, double& x, double& y) {
    if (A.mode != MODE_EDIT && 2 * i + 1 < (int)A.st.q.size()) { x = A.st.q[2 * i]; y = A.st.q[2 * i + 1]; }
    else { x = A.model.nodes[i].x; y = A.model.nodes[i].y; }
}
inline double nodeRadius(int i) {         // on-screen size (pixels), also the click target size
    const lag::Node& n = A.model.nodes[i];
    return n.kind == lag::NodeKind::Ground ? 12.0 : 13.0 + 7.0 * std::cbrt(n.m);
}

// ------------------------------------------------------------------ telemetry log
inline void logFrame() {
    const lag::Model& M = A.model;
    Frame f;
    f.t = A.st.t; f.T = A.meter.T; f.V = A.meter.V; f.dissipated = A.meter.dissipated; f.workIn = A.meter.workIn;
    f.nd.resize(4 * M.nodes.size());
    for (size_t i = 0; i < M.nodes.size(); ++i) {
        f.nd[4 * i]     = (float)A.st.q[2 * i];     f.nd[4 * i + 1] = (float)A.st.q[2 * i + 1];
        f.nd[4 * i + 2] = (float)A.st.v[2 * i];     f.nd[4 * i + 3] = (float)A.st.v[2 * i + 1];
    }
    f.lk.resize(3 * M.links.size());
    for (size_t li = 0; li < M.links.size(); ++li) {
        lag::LinkInfo in = lag::linkInfo(M, A.st.q, A.st.v, (int)li);
        f.lk[3 * li]     = (float)(M.links[li].kind == lag::LinkKind::Spring ? in.stretch : in.rate);
        f.lk[3 * li + 1] = (float)in.force;
        f.lk[3 * li + 2] = (float)in.len;
    }
    A.log.push_back(std::move(f));
    while (A.log.size() > LOG_LIMIT) A.log.pop_front();
}

// ------------------------------------------------------------------ simulation control
inline void startRun() {
    if (A.model.massCount() == 0) { toast("Add at least one mass first."); return; }
    A.model.refreshAutoRest();
    A.st = lag::initialState(A.model);
    A.meter.reset(A.model, A.st);
    A.ext.assign(A.st.q.size(), 0.0);
    A.log.clear();
    A.simAcc = 0; A.domFreq = 0;
    A.mode = MODE_RUN;
    logFrame();
}
inline void stopRun() {
    A.mode = MODE_EDIT;
    A.dragNode = -1;
    std::fill(A.ext.begin(), A.ext.end(), 0.0);
}
inline void toggleRun() {
    if (A.mode == MODE_EDIT) startRun();
    else A.mode = (A.mode == MODE_RUN) ? MODE_PAUSE : MODE_RUN;
}

// The "mouse spring": while you drag a mass in run mode, a critically-damped spring pulls it to the cursor.
inline void applyMouseForce() {
    std::fill(A.ext.begin(), A.ext.end(), 0.0);
    int i = A.dragNode;
    if (A.mode != MODE_RUN || i < 0 || i >= (int)A.model.nodes.size() || A.model.nodes[i].kind != lag::NodeKind::Mass) return;
    double tx = toWorldX(A.mx), ty = toWorldY(A.my);
    double dx = tx - A.st.q[2 * i], dy = ty - A.st.q[2 * i + 1], d = std::hypot(dx, dy);
    if (d > 3.0) { dx *= 3.0 / d; dy *= 3.0 / d; }                       // never pull from further than 3 m away
    double m = A.model.nodes[i].m;
    A.ext[2 * i]     = m * (700.0 * dx - 45.0 * A.st.v[2 * i]);
    A.ext[2 * i + 1] = m * (700.0 * dy - 45.0 * A.st.v[2 * i + 1]);
}

// Move the physics forward by `dt` seconds of "wall clock" (times the speed knob), then log one telemetry row.
inline void advanceSim(double dt) {
    if (A.mode != MODE_RUN) return;
    A.simAcc += dt * A.speed;
    int steps = 0;
    while (A.simAcc >= SIM_STEP && steps < 800) {
        applyMouseForce();
        lag::advance(A.model, A.st, A.ext, SIM_STEP, A.meter);
        A.simAcc -= SIM_STEP; ++steps;
    }
    if (steps == 800) A.simAcc = 0;                                      // fell behind: drop the backlog
    A.stepsLastFrame = steps;
    for (double v : A.st.q)
        if (!std::isfinite(v) || std::fabs(v) > 1e4) { stopRun(); toast("Numbers blew up (too stiff or too light). Try a heavier mass or a softer spring."); return; }
    logFrame();
}

// ------------------------------------------------------------------ hit testing
inline int hitNode(double sx, double sy) {
    for (int i = (int)A.model.nodes.size() - 1; i >= 0; --i) {           // topmost (last drawn) first
        double x, y; nodeWorld(i, x, y);
        double dx = sx - toScreenX(x), dy = sy - toScreenY(y), r = nodeRadius(i) + 4;
        if (dx * dx + dy * dy <= r * r) return i;
    }
    return -1;
}
inline double distToSegment(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
    double t = L2 > 1e-9 ? clampd(((px - ax) * dx + (py - ay) * dy) / L2, 0, 1) : 0;
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}
inline int hitLink(double sx, double sy) {
    int best = -1; double bd = 9.0;
    for (size_t li = 0; li < A.model.links.size(); ++li) {
        const lag::Link& L = A.model.links[li];
        double ax, ay, bx, by; nodeWorld(L.a, ax, ay); nodeWorld(L.b, bx, by);
        double d = distToSegment(sx, sy, toScreenX(ax), toScreenY(ay), toScreenX(bx), toScreenY(by));
        if (d < bd) { bd = d; best = (int)li; }
    }
    return best;
}

// ------------------------------------------------------------------ editing
inline void clearSelection() { A.selKind = SEL_NONE; A.sel = -1; }
inline bool editingAllowed() {
    if (A.mode == MODE_EDIT) return true;
    toast("Press Stop to edit the layout. (You can still drag masses and move sliders.)");
    return false;
}
inline void deleteSelection() {
    if (!editingAllowed()) return;
    if (A.selKind == SEL_NODE) A.model.removeNode(A.sel);
    else if (A.selKind == SEL_LINK) A.model.removeLink(A.sel);
    clearSelection();
    A.linkStart = -1;
}
inline void setTool(Tool t) { A.tool = t; A.linkStart = -1; }

inline void loadPreset(int n) {
    if (A.mode != MODE_EDIT) stopRun();
    A.model = lag::presetByNumber(n);
    clearSelection(); A.linkStart = -1; A.log.clear();
    A.plotMode = 0;
    toast("Loaded example " + std::to_string(n));
}
inline std::string designPath() { return std::string(LAB_DIR) + "/design.vlab"; }
inline void saveDesign() {
    toast(lag::saveModel(designPath(), A.model) ? "Design saved to design.vlab" : "Could not save design.vlab");
}
inline void loadDesign() {
    lag::Model M;
    if (!lag::loadModel(designPath(), M)) { toast("No design.vlab found (press Save first)"); return; }
    if (A.mode != MODE_EDIT) stopRun();
    A.model = M; clearSelection(); A.log.clear();
    toast("Design loaded");
}
inline void clearDesign() {
    if (!editingAllowed()) return;
    A.model = lag::Model();
    clearSelection(); A.linkStart = -1; A.log.clear();
    toast("Empty canvas: pick the Mass and Ground tools to start designing");
}

// ------------------------------------------------------------------ the property sliders on the right panel
struct Prop {
    std::string name;
    double* p;                 // points straight into the model, so moving a slider edits the physics
    double lo, hi;
    std::string unit;
    double step = 0;           // > 0: snap to multiples (used for the rail choice)
    const char* const* labels = nullptr;   // text for each snapped value
};
static const char* const RAIL_LABELS[3] = {"free (2-D)", "X rail", "Y rail"};

inline std::vector<Prop> buildProps() {
    std::vector<Prop> v;
    lag::Model& M = A.model;
    if (A.selKind == SEL_NODE && A.sel >= 0 && A.sel < (int)M.nodes.size()) {
        lag::Node& n = M.nodes[A.sel];
        if (n.kind == lag::NodeKind::Mass) {
            v.push_back({"mass m", &n.m, 0.1, 20, "kg"});
            v.push_back({"motion", &n.rail, 0, 2, "", 1, RAIL_LABELS});
            v.push_back({"start vx", &n.vx0, -10, 10, "m/s"});
            v.push_back({"start vy", &n.vy0, -10, 10, "m/s"});
            v.push_back({"push force F", &n.driveF, 0, 60, "N"});
            v.push_back({"push angle", &n.driveAngle, 0, 360, "deg"});
            v.push_back({"push freq", &n.driveW, 0, 30, "rad/s"});
        }
    } else if (A.selKind == SEL_LINK && A.sel >= 0 && A.sel < (int)M.links.size()) {
        lag::Link& l = M.links[A.sel];
        if (l.kind == lag::LinkKind::Spring) {
            v.push_back({"stiffness k", &l.k, 0, 500, "N/m"});
            v.push_back({"rest length", &l.L0, 0.1, 8, "m"});
            v.push_back({"cubic k3", &l.k3, 0, 500, "N/m3"});
        } else {
            v.push_back({"damping c", &l.c, 0, 30, "N s/m"});
        }
    }
    v.push_back({"gravity g", &M.gravity, 0, 20, "m/s2"});
    return v;
}
static const int PROPS_Y = 322;     // top of the property section of the panel
inline Gdiplus::RectF propTrack(int i) { return Gdiplus::RectF((Gdiplus::REAL)(W - PANEL_W + 24), (Gdiplus::REAL)(PROPS_Y + 58 + i * 46), (Gdiplus::REAL)(PANEL_W - 48), 8); }

inline void setPropFromX(int i, double x) {
    std::vector<Prop> props = buildProps();
    if (i < 0 || i >= (int)props.size()) return;
    Prop& p = props[i];
    Gdiplus::RectF tk = propTrack(i);
    double frac = clampd((x - tk.X) / tk.Width, 0, 1), val = p.lo + frac * (p.hi - p.lo);
    if (p.step > 0) val = std::round(val / p.step) * p.step;
    *p.p = val;
    if (A.selKind == SEL_LINK && p.p == &A.model.links[A.sel].L0) A.model.links[A.sel].autoRest = false;   // hand-set: stop following the drawing
    if (A.mode != MODE_EDIT) A.meter.rebase(A.model, A.st);                    // a live edit changes energy for non-physical reasons
}

// ------------------------------------------------------------------ the buttons
enum Action {
    ACT_TOOL_SELECT, ACT_TOOL_MASS, ACT_TOOL_GROUND, ACT_TOOL_SPRING, ACT_TOOL_DAMPER, ACT_DELETE,
    ACT_RUN, ACT_STOP, ACT_RECORD, ACT_CSV, ACT_SAVE, ACT_LOAD, ACT_CLEAR,
    ACT_PRESET1, ACT_PRESET2, ACT_PRESET3, ACT_PRESET4,
    ACT_TAB0, ACT_TAB1, ACT_TAB2, ACT_TAB3
};
struct Btn { Gdiplus::RectF r; std::string label; Action id; bool active; };

inline std::vector<Btn> layoutButtons() {
    using Gdiplus::RectF;
    std::vector<Btn> b;
    const float px = (float)(W - PANEL_W);
    auto add = [&](float x, float y, float w, float h, const char* label, Action id, bool active = false) {
        b.push_back({RectF(px + x, y, w, h), label, id, active});
    };
    add(16, 56, 84, 30, "Select",  ACT_TOOL_SELECT, A.tool == TOOL_SELECT);
    add(108, 56, 84, 30, "Mass",   ACT_TOOL_MASS,   A.tool == TOOL_MASS);
    add(200, 56, 84, 30, "Ground", ACT_TOOL_GROUND, A.tool == TOOL_GROUND);
    add(16, 92, 84, 30, "Spring",  ACT_TOOL_SPRING, A.tool == TOOL_SPRING);
    add(108, 92, 84, 30, "Damper", ACT_TOOL_DAMPER, A.tool == TOOL_DAMPER);
    add(200, 92, 84, 30, "Delete", ACT_DELETE);
    add(16, 142, 128, 30, A.mode == MODE_RUN ? "Pause" : (A.mode == MODE_PAUSE ? "Resume" : "Run"), ACT_RUN, A.mode == MODE_RUN);
    add(156, 142, 128, 30, "Stop",  ACT_STOP);
    add(16, 178, 128, 30, A.recording ? "Stop REC" : "Record", ACT_RECORD, A.recording);
    add(156, 178, 128, 30, "Export CSV", ACT_CSV);
    add(16, 226, 84, 28, "Save",   ACT_SAVE);
    add(108, 226, 84, 28, "Load",  ACT_LOAD);
    add(200, 226, 84, 28, "Clear", ACT_CLEAR);
    for (int i = 0; i < 4; ++i) add(16 + i * 70.0f, 276, 62, 28, std::to_string(i + 1).c_str(), (Action)(ACT_PRESET1 + i));
    // plot tabs sit just above the time plot
    const char* tabs[4] = {"Position", "Velocity", "Energy", "Link force"};
    Gdiplus::RectF tr = timeRect();
    for (int i = 0; i < 4; ++i)
        b.push_back({RectF(tr.X + i * 92.0f, tr.Y - 30, 86, 24), tabs[i], (Action)(ACT_TAB0 + i), A.plotMode == i});
    return b;
}

void toggleRecording();     // defined in live.cpp (needs the pixel buffer)
void exportCsv();

inline void doAction(Action id) {
    switch (id) {
        case ACT_TOOL_SELECT: setTool(TOOL_SELECT); break;
        case ACT_TOOL_MASS:   setTool(TOOL_MASS); break;
        case ACT_TOOL_GROUND: setTool(TOOL_GROUND); break;
        case ACT_TOOL_SPRING: setTool(TOOL_SPRING); break;
        case ACT_TOOL_DAMPER: setTool(TOOL_DAMPER); break;
        case ACT_DELETE: deleteSelection(); break;
        case ACT_RUN:  toggleRun(); break;
        case ACT_STOP: stopRun(); break;
        case ACT_RECORD: toggleRecording(); break;
        case ACT_CSV: exportCsv(); break;
        case ACT_SAVE: saveDesign(); break;
        case ACT_LOAD: loadDesign(); break;
        case ACT_CLEAR: clearDesign(); break;
        case ACT_PRESET1: case ACT_PRESET2: case ACT_PRESET3: case ACT_PRESET4: loadPreset(id - ACT_PRESET1 + 1); break;
        case ACT_TAB0: case ACT_TAB1: case ACT_TAB2: case ACT_TAB3: A.plotMode = id - ACT_TAB0; break;
    }
}

// ------------------------------------------------------------------ mouse
inline void onMouseDown(double x, double y) {
    A.mx = x; A.my = y;
    for (const Btn& b : layoutButtons())                                  // 1) buttons
        if (x >= b.r.X && x <= b.r.X + b.r.Width && y >= b.r.Y && y <= b.r.Y + b.r.Height) { doAction(b.id); return; }
    int n = (int)buildProps().size();                                      // 2) sliders
    for (int i = 0; i < n; ++i) {
        Gdiplus::RectF tk = propTrack(i);
        if (x >= tk.X - 10 && x <= tk.X + tk.Width + 10 && y >= tk.Y - 16 && y <= tk.Y + 22) { A.dragSlider = i; setPropFromX(i, x); return; }
    }
    if (!inScene(x, y)) return;                                            // 3) the drawing area
    int hn = hitNode(x, y);
    switch (A.tool) {
        case TOOL_SELECT:
            if (hn >= 0) { A.selKind = SEL_NODE; A.sel = hn; A.dragNode = hn; }
            else {
                int hl = hitLink(x, y);
                if (hl >= 0) { A.selKind = SEL_LINK; A.sel = hl; } else clearSelection();
            }
            break;
        case TOOL_MASS: case TOOL_GROUND:
            if (!editingAllowed()) break;
            if (hn < 0) {
                double wx = snap(toWorldX(x)), wy = snap(toWorldY(y));
                int id = (A.tool == TOOL_MASS) ? A.model.addMass(wx, wy) : A.model.addGround(wx, wy);
                if (id >= 0) { A.selKind = SEL_NODE; A.sel = id; } else toast("Limit reached: 24 nodes");
            }
            break;
        case TOOL_SPRING: case TOOL_DAMPER:
            if (!editingAllowed()) break;
            if (hn < 0) { A.linkStart = -1; break; }
            if (A.linkStart < 0) { A.linkStart = hn; break; }
            {
                int li = A.model.addLink(A.tool == TOOL_SPRING ? lag::LinkKind::Spring : lag::LinkKind::Damper, A.linkStart, hn);
                if (li >= 0) { A.selKind = SEL_LINK; A.sel = li; }
                else toast("Cannot connect those two (same node, two grounds, or already connected)");
                A.linkStart = -1;
            }
            break;
    }
}
inline void onMouseMove(double x, double y) {
    A.mx = x; A.my = y;
    if (A.panning) { A.ox += x - A.panX; A.oy += y - A.panY; A.panX = x; A.panY = y; return; }
    if (A.dragSlider >= 0) { setPropFromX(A.dragSlider, x); return; }
    if (A.dragNode >= 0 && A.mode == MODE_EDIT && A.tool == TOOL_SELECT) {          // move a node (edit mode only)
        lag::Node& nd = A.model.nodes[A.dragNode];
        nd.x = clampd(snap(toWorldX(x)), -40, 40);
        nd.y = clampd(snap(toWorldY(y)), -40, 40);
        A.model.refreshAutoRest();
    }
}
inline void onMouseUp() { A.dragNode = -1; A.dragSlider = -1; }
inline void onWheel(double x, double y, int notches) {
    if (!inScene(x, y)) return;
    double before = toWorldX(x), beforeY = toWorldY(y);
    A.scale = clampd(A.scale * std::pow(1.12, notches), 25, 260);
    A.ox = x - before * A.scale;                                           // keep the point under the cursor fixed
    A.oy = y - beforeY * A.scale;
}
