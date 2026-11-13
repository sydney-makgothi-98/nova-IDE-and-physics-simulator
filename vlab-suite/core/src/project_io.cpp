#include "vlab/project_io.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace vlab {

// ====================================================================== text format (tab-separated, one record per line)
static std::string clean(std::string s) {
    for (char& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

std::string modelToText(const Model& m) {
    std::ostringstream o;
    o << std::setprecision(10);
    o << "vlab\t2\n";
    o << "name\t" << clean(m.name) << "\n";
    o << "notes\t" << clean(m.notes) << "\n";
    const Environment& e = m.env;
    o << "env\t" << e.gravity << "\t" << e.realistic << "\t" << e.collisions << "\t" << e.friction << "\t" << e.inelastic << "\t"
      << e.rolling << "\t" << e.damping << "\t" << e.airDrag << "\t" << e.drag1 << "\t" << e.drag2 << "\t" << (int)e.muCombine << "\t"
      << (int)e.eCombine << "\t" << e.restThreshold << "\t" << e.solverIterations << "\t" << e.dt << "\n";
    o << "run\t" << m.run.duration << "\t" << m.run.sampleHz << "\n";
    for (const Body& b : m.bodies) {
        o << "body\t" << clean(b.name) << "\t" << (int)b.kind << "\t" << (int)b.shape.preset << "\t" << b.shape.radius << "\t"
          << b.shape.w << "\t" << b.shape.h << "\t" << b.shape.sides << "\t" << clean(b.mat.name) << "\t" << b.mat.density << "\t"
          << b.mat.muS << "\t" << b.mat.muK << "\t" << b.mat.restitution << "\t" << b.mat.rollRes << "\t" << b.massFromDensity << "\t"
          << b.mass << "\t" << b.pos.x << "\t" << b.pos.y << "\t" << b.angle << "\t" << b.vel.x << "\t" << b.vel.y << "\t" << b.omega
          << "\t" << b.lockX << "\t" << b.lockY << "\t" << b.lockRot << "\t" << b.driveF << "\t" << b.driveAngleDeg << "\t" << b.driveW
          << "\t" << b.color << "\t" << b.collide << "\n";
    }
    for (const Link& l : m.links) {
        o << "link\t" << (int)l.kind << "\t" << l.a << "\t" << l.b << "\t" << l.la.x << "\t" << l.la.y << "\t" << l.lb.x << "\t" << l.lb.y
          << "\t" << l.k << "\t" << l.k3 << "\t" << l.L0 << "\t" << l.autoRest << "\t" << l.c << "\t" << l.collideConnected << "\t" << l.refAngle << "\t" << l.jointC << "\t" << l.motorTorque
          << "\t" << l.motorAmp << "\t" << l.motorW << "\n";
    }
    return o.str();
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> v;
    std::string cur;
    for (char c : s) {
        if (c == sep) { v.push_back(cur); cur.clear(); }
        else cur += c;
    }
    v.push_back(cur);
    return v;
}
static double num(const std::vector<std::string>& f, size_t i, double def = 0) {
    if (i >= f.size() || f[i].empty()) return def;
    try { return std::stod(f[i]); } catch (...) { return def; }
}

bool modelFromText(const std::string& text, Model& out, std::string* error) {
    Model m;
    std::istringstream in(text);
    std::string line;
    bool header = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> f = split(line, '\t');
        const std::string& k = f[0];
        if (k == "vlab") header = true;
        else if (k == "name" && f.size() > 1) m.name = f[1];
        else if (k == "notes" && f.size() > 1) m.notes = f[1];
        else if (k == "env") {
            Environment& e = m.env;
            e.gravity = num(f, 1, 9.81); e.realistic = num(f, 2) != 0; e.collisions = num(f, 3) != 0; e.friction = num(f, 4) != 0;
            e.inelastic = num(f, 5) != 0; e.rolling = num(f, 6) != 0; e.damping = num(f, 7) != 0; e.airDrag = num(f, 8) != 0;
            e.drag1 = num(f, 9); e.drag2 = num(f, 10); e.muCombine = (Combine)(int)num(f, 11); e.eCombine = (Combine)(int)num(f, 12, 3);
            e.restThreshold = num(f, 13, 0.25); e.solverIterations = (int)num(f, 14, 14); e.dt = num(f, 15, 0.00025);
            if (e.dt < 1e-5) e.dt = 1e-5;
            if (e.dt > 0.005) e.dt = 0.005;
        } else if (k == "run") {
            m.run.duration = num(f, 1, 10);
            m.run.sampleHz = num(f, 2, 100);
        } else if (k == "body" && f.size() >= 28) {
            Body b;
            b.name = f[1];
            b.kind = (BodyKind)(int)num(f, 2);
            b.shape.preset = (ShapePreset)(int)num(f, 3);
            b.shape.radius = num(f, 4, 0.5); b.shape.w = num(f, 5, 1); b.shape.h = num(f, 6, 1); b.shape.sides = (int)num(f, 7, 5);
            b.mat.name = f[8]; b.mat.density = num(f, 9, 1000); b.mat.muS = num(f, 10); b.mat.muK = num(f, 11);
            b.mat.restitution = num(f, 12); b.mat.rollRes = num(f, 13);
            b.massFromDensity = num(f, 14) != 0; b.mass = num(f, 15, 1);
            b.pos = Vec2(num(f, 16), num(f, 17)); b.angle = num(f, 18); b.vel = Vec2(num(f, 19), num(f, 20)); b.omega = num(f, 21);
            b.lockX = num(f, 22) != 0; b.lockY = num(f, 23) != 0; b.lockRot = num(f, 24) != 0;
            b.driveF = num(f, 25); b.driveAngleDeg = num(f, 26); b.driveW = num(f, 27);
            if (f.size() > 28) b.color = (unsigned)std::stoull(f[28]);
            if (f.size() > 29) b.collide = num(f, 29, 1) != 0;
            m.bodies.push_back(b);
        } else if (k == "link" && f.size() >= 14) {
            Link l;
            l.kind = (LinkKind)(int)num(f, 1);
            l.a = (int)num(f, 2, -1); l.b = (int)num(f, 3, -1);
            l.la = Vec2(num(f, 4), num(f, 5)); l.lb = Vec2(num(f, 6), num(f, 7));
            l.k = num(f, 8, 50); l.k3 = num(f, 9); l.L0 = num(f, 10, 1); l.autoRest = num(f, 11) != 0; l.c = num(f, 12, 1);
            l.collideConnected = num(f, 13) != 0;
            l.refAngle = num(f, 14); l.jointC = num(f, 15); l.motorTorque = num(f, 16); l.motorAmp = num(f, 17); l.motorW = num(f, 18);
            m.links.push_back(l);
        }
    }
    if (!header) {
        if (error) *error = "This is not a Vibration Lab Suite file (missing 'vlab' header).";
        return false;
    }
    for (int i = (int)m.links.size() - 1; i >= 0; --i) {          // drop links that point at missing bodies
        const Link& l = m.links[i];
        if (l.a < 0 || l.b < 0 || l.a >= (int)m.bodies.size() || l.b >= (int)m.bodies.size() || l.a == l.b) m.links.erase(m.links.begin() + i);
    }
    for (Body& b : m.bodies) {                                     // designs from v1.0 drew floors and walls as thin slabs: turn them into lines
        if (b.kind != BodyKind::Static || b.shape.preset != ShapePreset::Box) continue;
        Rot r(b.angle);
        double w = b.shape.w, h = b.shape.h;
        if (w >= h) b.pos += r.apply(Vec2(0, -h / 2));              // the line sits on the slab's top face
        else { b.angle += kPi / 2; b.shape.w = h; }                 // an upright slab becomes a vertical line along its centre
        if (w >= h) b.shape.w = w;
        b.shape.preset = ShapePreset::Line;
        b.color = 0xFFC8CEDA;
    }
    m.updateMass();
    out = m;
    return true;
}

bool saveModelFile(const std::string& path, const Model& m) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << modelToText(m);
    return (bool)f;
}

bool loadModelFile(const std::string& path, Model& out, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "Could not open the file.";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return modelFromText(ss.str(), out, error);
}

// ====================================================================== example gallery
namespace {
Material mat(const char* name) {
    const Material* m = findMaterial(name);
    return m ? *m : Material();
}
int add(Model& m, Body b, const char* name) {
    b.name = name;
    return m.addBody(b);
}
Vec2 onSlope(const Body& floor, double blockHalfH, double along) {      // a point just above a (rotated) floor line
    Rot r(floor.angle);
    Vec2 up = r.apply(Vec2(0, -1)), tang = r.apply(Vec2(1, 0));
    return floor.pos + up * blockHalfH + tang * along;
}
void spring(Model& m, int a, int b, double k, double L0 = -1) {
    int i = m.addLink(LinkKind::Spring, a, b, m.bodies[a].pos, m.bodies[b].pos);
    if (i >= 0) { m.links[i].k = k; if (L0 > 0) { m.links[i].L0 = L0; m.links[i].autoRest = false; } }
}
void damper(Model& m, int a, int b, double c) {
    int i = m.addLink(LinkKind::Damper, a, b, m.bodies[a].pos, m.bodies[b].pos);
    if (i >= 0) m.links[i].c = c;
}
void rail(Body& b) { b.lockY = true; b.lockRot = true; b.collide = false; }   // vibration masses do not hit each other

Example oneDof() {
    Example e{"Mass-spring-damper (1 degree of freedom)",
              "The classic vibration problem. The spring starts stretched by 1 m and is released. Compare the measured frequency "
              "(Analysis tab) with sqrt(k/m)/(2 pi) = 0.62 Hz, and watch the damped decay (damping ratio about 0.10).", {}};
    Model& m = e.model;
    m.name = "Mass-spring-damper";
    m.env.setRealistic();
    m.env.gravity = 0;
    int a = m.addBody(makeAnchor({-3, 0}));
    Body b = makeBox(0.8, 0.8, mat("Steel"), {0, 0});
    b.mass = 2.0;
    rail(b);
    int mb = add(m, b, "Mass");
    spring(m, a, mb, 30, 2.0);
    damper(m, a, mb, 1.5);
    m.run.duration = 12;
    return e;
}
Example twoDof() {
    Example e{"Two coupled masses",
              "Two masses between two walls. Ask for Modal analysis: two natural frequencies and two mode shapes (moving together, moving apart). "
              "Run it and watch the energy pass back and forth.", {}};
    Model& m = e.model;
    m.name = "Two coupled masses";
    m.env.setRealistic();
    m.env.gravity = 0;
    int g1 = m.addBody(makeAnchor({-4, 0})), g2 = m.addBody(makeAnchor({4, 0}));
    Body b1 = makeBox(0.7, 0.7, mat("Steel"), {-1.25, 0}), b2 = makeBox(0.7, 0.7, mat("Aluminium"), {1.25, 0});
    b1.mass = b2.mass = 1.0;
    rail(b1); rail(b2);
    b1.vel = {3.0, 0};
    int a = add(m, b1, "Mass A"), b = add(m, b2, "Mass B");
    spring(m, g1, a, 40); damper(m, g1, a, 0.6);
    spring(m, a, b, 25);  damper(m, a, b, 0.2);
    spring(m, b, g2, 40); damper(m, b, g2, 0.6);
    m.run.duration = 12;
    return e;
}
Example pendulum() {
    Example e{"Spring pendulum",
              "A mass on a spring that can swing and bounce. The motion never repeats exactly. Turn on 'Ideal' and the ledger error stays near zero.", {}};
    Model& m = e.model;
    m.name = "Spring pendulum";
    m.env.setRealistic();
    int a = m.addBody(makeAnchor({0, -1.8}));
    Body b = makeCircle(0.25, mat("Steel"), {1.6, 0.6});
    b.mass = 1.0;
    b.lockRot = true;
    int mb = add(m, b, "Bob");
    spring(m, a, mb, 60, 2.0);
    damper(m, a, mb, 0.25);
    m.run.duration = 15;
    return e;
}
Example rodPendulum() {
    Example e{"Pendulum on a rod",
              "A rigid rod pinned to the ceiling. Switch to Ideal and the swing never fades; in Realistic it fades only through air drag. "
              "Small-swing period T = 2 pi sqrt(L/g) = 2.84 s for L = 2 m.", {}};
    Model& m = e.model;
    m.name = "Pendulum on a rod";
    m.env.setRealistic();
    m.env.airDrag = true;
    m.env.drag1 = 0.05;
    int a = m.addBody(makeAnchor({0, -2}));
    Body b = makeCircle(0.3, mat("Steel"), {std::sin(0.4) * 2.0, -2 + std::cos(0.4) * 2.0});
    b.mass = 1.5;
    int mb = add(m, b, "Bob");
    int j = m.addLink(LinkKind::Rod, a, mb, m.bodies[a].pos, m.bodies[mb].pos);
    if (j >= 0) m.links[j].L0 = 2.0;
    m.run.duration = 12;
    return e;
}
Example quarterCar() {
    Example e{"Quarter-car suspension",
              "A wheel hits a bump (initial upward speed) and the body rides it out. Change the damper strength live and watch the ride. "
              "The body sags under gravity first (static deflection = m g / k).", {}};
    Model& m = e.model;
    m.name = "Quarter-car suspension";
    m.env.setRealistic();
    int road = m.addBody(makeAnchor({0, 2.2}));
    Body w = makeBox(0.9, 0.35, mat("Rubber"), {0, 1.0}), c = makeBox(1.6, 0.5, mat("Steel"), {0, -1.0});
    w.mass = 1.0; c.mass = 4.0;
    w.lockX = c.lockX = true; w.lockRot = c.lockRot = true;
    w.collide = c.collide = false;
    w.vel = {0, -2.0};
    int wi = add(m, w, "Wheel"), ci = add(m, c, "Car body");
    spring(m, road, wi, 220);
    spring(m, wi, ci, 70);
    damper(m, wi, ci, 5.0);
    m.run.duration = 8;
    return e;
}
Example bounce() {
    Example e{"Bouncing ball: ideal vs realistic",
              "Drop height 2 m. Realistic: each bounce reaches e^2 of the previous height and the ball settles. Ideal (press the Ideal button): "
              "it bounces to the same height forever. Open the Energy tab to see where the energy goes.", {}};
    Model& m = e.model;
    m.name = "Bouncing ball";
    m.env.setRealistic();
    Body fl = makeFloor(10, 0, mat("Concrete"), {0, 2.0});
    add(m, fl, "Floor");
    Body b = makeCircle(0.25, mat("Rubber"), {0, 2.0 - 0.25 - 2.0});
    b.mass = 0.5;
    add(m, b, "Ball");
    m.run.duration = 8;
    return e;
}
Example incline() {
    Example e{"Friction on an incline: four materials",
              "Rubber, steel, wood and ice blocks on the same 28 degree wooden ramp. A block slides only if tan(angle) = 0.53 exceeds its static "
              "friction. Ice and wood slide; rubber and steel stay put. Switch to Ideal (frictionless) and all four slide at g sin(angle).", {}};
    Model& m = e.model;
    m.name = "Friction on an incline";
    m.env.setRealistic();
    Body ramp = makeFloor(10, 28, mat("Wood"), {0, 1.2});
    ramp.pos += Rot(ramp.angle).apply(Vec2(0, -0.15));
    int r = add(m, ramp, "Ramp");
    const char* mats[4] = {"Rubber", "Steel", "Wood", "Ice"};
    for (int i = 0; i < 4; ++i) {
        Body b = makeBox(0.6, 0.6, mat(mats[i]), {});
        b.mass = 1.0;
        b.angle = m.bodies[r].angle;
        b.pos = onSlope(m.bodies[r], 0.3, -3.6 + i * 1.15);
        add(m, b, mats[i]);
    }
    Body stop = makeFloor(3.0, 90, mat("Concrete"), {5.0, 4.5});
    add(m, stop, "Wall");
    m.run.duration = 6;
    return e;
}
Example billiards() {
    Example e{"Collision: two balls",
              "A moving ball hits a resting ball on a frictionless table. With restitution 1 the cue ball stops and the target leaves with the same "
              "speed (momentum and energy are both conserved). Lower the restitution and see the energy ledger's 'impact' loss.", {}};
    Model& m = e.model;
    m.name = "Collision of two balls";
    m.env.setRealistic();
    m.env.gravity = 9.81;
    Material ideal = mat("Ideal (frictionless, elastic)");
    add(m, makeFloor(18, 0, ideal, {0, 0.0}), "Table");
    add(m, makeFloor(2.8, 90, ideal, {-9.0, -1.4}), "Left wall");
    add(m, makeFloor(2.8, 90, ideal, {9.0, -1.4}), "Right wall");
    Body a = makeCircle(0.3, ideal, {-3.0, -0.3}), b = makeCircle(0.3, ideal, {1.0, -0.3});
    a.mass = b.mass = 1.0;
    a.vel = {4.0, 0};
    add(m, a, "Cue ball");
    add(m, b, "Target ball");
    m.run.duration = 6;
    return e;
}
Example rolling() {
    Example e{"Ball rolling down a ramp",
              "A rubber ball rolls without slipping down a 20 degree ramp. Ideal theory: a = (2/3) g sin(angle) for a solid disc. "
              "Compare with a block that slides.", {}};
    Model& m = e.model;
    m.name = "Rolling ball";
    m.env.setRealistic();
    Body ramp = makeFloor(10, 20, mat("Concrete"), {0, 1.0});
    ramp.pos += Rot(ramp.angle).apply(Vec2(0, -0.15));
    int r = add(m, ramp, "Ramp");
    Body ball = makeCircle(0.3, mat("Steel"), {});
    ball.mass = 1.0;
    ball.pos = onSlope(m.bodies[r], 0.3, -3.5);
    add(m, ball, "Ball");
    add(m, makeFloor(3.0, 90, mat("Concrete"), {5.0, 3.0}), "Wall");
    m.run.duration = 6;
    return e;
}
Example tmd() {
    Example e{"Tuned mass damper",
              "A main mass is pushed at its resonance. A small absorber mass tuned to the same frequency cuts the vibration dramatically. "
              "Delete the absorber (or detune it) and compare the Position plots. Try Frequency response in the Analysis tab.", {}};
    Model& m = e.model;
    m.name = "Tuned mass damper";
    m.env.setRealistic();
    m.env.gravity = 0;
    int g = m.addBody(makeAnchor({-4, 0}));
    Body p = makeBox(1.0, 0.8, mat("Steel"), {0, 0}), s = makeBox(0.5, 0.5, mat("Aluminium"), {2.0, 0});
    p.mass = 10.0; s.mass = 1.0;
    rail(p); rail(s);
    p.driveF = 5.0; p.driveW = std::sqrt(400.0 / 10.0);
    int pi = add(m, p, "Main mass"), si = add(m, s, "Absorber");
    spring(m, g, pi, 400);
    damper(m, g, pi, 1.0);
    spring(m, pi, si, 40);
    damper(m, pi, si, 0.8);
    m.run.duration = 40;
    m.run.sampleHz = 50;
    return e;
}
Example stackOfBoxes() {
    Example e{"Impact of a block on a spring platform",
              "A block drops onto a platform held up by a spring and damper. Contacts, springs and friction all at work; watch the energy ledger.", {}};
    Model& m = e.model;
    m.name = "Block on a spring platform";
    m.env.setRealistic();
    int g = m.addBody(makeAnchor({0, 2.5}));
    Body plat = makeBox(2.0, 0.2, mat("Wood"), {0, 1.2});
    plat.mass = 3.0;
    plat.lockX = true; plat.lockRot = true;
    int pi = add(m, plat, "Platform");
    spring(m, g, pi, 300);
    damper(m, g, pi, 6.0);
    Body blk = makeBox(0.6, 0.6, mat("Steel"), {0.1, -1.2});
    blk.mass = 2.0;
    add(m, blk, "Block");
    m.run.duration = 8;
    return e;
}
// ---- structures: beams, supports, mechanisms
Example beamFalls() {
    Example e{"A beam with no support falls",
              "A wooden beam released above the floor line. Nothing holds it, so it drops, lands and settles. Compare with the next example, "
              "where a pin holds one end.", {}};
    Model& m = e.model;
    m.name = "Unsupported beam";
    m.env.setRealistic();
    add(m, makeFloor(10, 0, mat("Concrete"), {0, 2.0}), "Floor");
    Body b = makeBeam(3.0, 0.15, mat("Wood"), {0, 0.2});
    b.angle = 0.25;
    add(m, b, "Beam");
    m.run.duration = 5;
    return e;
}
Example pinnedBeam() {
    Example e{"Beam hinged at one end",
              "The same beam with a pin support at its left end: it swings down like a door. A uniform beam hinged at one end has period "
              "T = 2 pi sqrt(2L / 3g) for small swings. The pin's reaction force is in the Properties panel while it runs.", {}};
    Model& m = e.model;
    m.name = "Hinged beam";
    m.env.setRealistic();
    Body b = makeBeam(3.0, 0.15, mat("Wood"), {1.5 - 3.0, 0.0});
    b.angle = 0;
    b.massFromDensity = false; b.mass = 5.0;
    int bi = add(m, b, "Beam");
    m.addSupport(LinkKind::Pin, bi, {-3.0, 0.0}, 0);
    m.run.duration = 8;
    return e;
}
Example simplySupported() {
    Example e{"Simply supported beam with a load",
              "A beam resting on a pin at the left end and a roller at the right end, carrying a block at mid-span. Each support carries half of the "
              "total weight: select a support while running to read its reaction.", {}};
    Model& m = e.model;
    m.name = "Simply supported beam";
    m.env.setRealistic();
    Body b = makeBeam(4.0, 0.2, mat("Steel"), {0, 0});
    b.massFromDensity = false; b.mass = 20.0;
    int bi = add(m, b, "Beam");
    m.addSupport(LinkKind::Pin, bi, {-2.0, 0.0}, 0);
    m.addSupport(LinkKind::Roller, bi, {2.0, 0.0}, 0);
    Body blk = makeBox(0.5, 0.5, mat("Concrete"), {0, -0.1 - 0.25});
    blk.massFromDensity = false; blk.mass = 30.0;
    add(m, blk, "Load");
    m.run.duration = 6;
    return e;
}
Example cantilever() {
    Example e{"Cantilever with a tip load",
              "A beam welded (fixed support) to a wall at its left end, with a heavy block hanging at the free end. A fixed support provides a force AND a "
              "moment; the moment is the weight times its distance from the wall.", {}};
    Model& m = e.model;
    m.name = "Cantilever";
    m.env.setRealistic();
    Body b = makeBeam(3.0, 0.15, mat("Steel"), {1.5, 0});
    b.massFromDensity = false; b.mass = 12.0;
    int bi = add(m, b, "Beam");
    m.addSupport(LinkKind::Weld, bi, {0.0, 0.0}, 0);
    Body blk = makeBox(0.4, 0.4, mat("Concrete"), {2.8, -0.075 - 0.2});
    blk.massFromDensity = false; blk.mass = 25.0;
    int ki = add(m, blk, "Tip load");
    m.addLink(LinkKind::Weld, bi, ki, {2.8, -0.075}, {2.8, -0.075});
    m.run.duration = 5;
    return e;
}
Example springAssembly() {
    Example e{"Spring-mass assembly",
              "Three masses joined by springs and dampers, hung between two walls. Pull one with the mouse while running and watch the whole "
              "network answer. Try Modal analysis to see its natural frequencies.", {}};
    Model& m = e.model;
    m.name = "Spring-mass assembly";
    m.env.setRealistic();
    int g1 = m.addBody(makeAnchor({-3.0, -1.0})), g2 = m.addBody(makeAnchor({3.0, -1.0}));
    Body a = makeCircle(0.3, mat("Steel"), {-1.2, 0.5}), b = makeCircle(0.3, mat("Steel"), {1.2, 0.5}), c = makeCircle(0.35, mat("Aluminium"), {0, 1.8});
    a.mass = 2; b.mass = 2; c.mass = 3;
    a.lockRot = b.lockRot = c.lockRot = true;
    a.collide = b.collide = c.collide = false;
    int ia = add(m, a, "Mass A"), ib = add(m, b, "Mass B"), ic = add(m, c, "Mass C");
    spring(m, g1, ia, 80);  damper(m, g1, ia, 1.0);
    spring(m, g2, ib, 80);  damper(m, g2, ib, 1.0);
    spring(m, ia, ib, 50);  damper(m, ia, ib, 0.5);
    spring(m, ia, ic, 60);  damper(m, ia, ic, 0.6);
    spring(m, ib, ic, 60);  damper(m, ib, ic, 0.6);
    m.run.duration = 15;
    return e;
}
Example gantryCrane() {
    Example e{"Gantry crane carrying a load",
              "A bridge beam welded to two posts, a trolley hanging from it on a roller, and a load on a rigid rope. The trolley is pushed back and forth "
              "slowly; the load sways. Plot the load's x and the trolley's x to see the sway lag behind the push. Read the post reactions too.", {}};
    Model& m = e.model;
    m.name = "Gantry crane";
    m.env.setRealistic();
    add(m, makeFloor(14, 0, mat("Concrete"), {0, 3.0}), "Ground");
    Body p1 = makeBox(0.25, 4.0, mat("Steel"), {-3.0, 1.0}), p2 = makeBox(0.25, 4.0, mat("Steel"), {3.0, 1.0});
    p1.massFromDensity = p2.massFromDensity = false;
    p1.mass = p2.mass = 60;
    p1.collide = p2.collide = false;
    int i1 = add(m, p1, "Post L"), i2 = add(m, p2, "Post R");
    m.addSupport(LinkKind::Weld, i1, {-3.0, 3.0}, 0);
    m.addSupport(LinkKind::Weld, i2, {3.0, 3.0}, 0);
    Body br = makeBeam(6.25, 0.3, mat("Steel"), {0, -1.15});
    br.massFromDensity = false; br.mass = 80;
    int ib = add(m, br, "Bridge");
    m.addLink(LinkKind::Weld, i1, ib, {-3.0, -1.0}, {-3.0, -1.0});
    m.addLink(LinkKind::Weld, i2, ib, {3.0, -1.0}, {3.0, -1.0});
    Body tr = makeBox(0.6, 0.4, mat("Aluminium"), {0, -0.8});
    tr.massFromDensity = false; tr.mass = 15;
    tr.lockRot = true; tr.collide = false;
    tr.driveF = 60; tr.driveW = 1.3; tr.driveAngleDeg = 0;
    int it = add(m, tr, "Trolley");
    m.addLink(LinkKind::Roller, ib, it, {0, -1.0}, {0, -1.0});
    Body ld = makeBox(0.5, 0.5, mat("Concrete"), {0, 0.9});
    ld.massFromDensity = false; ld.mass = 40;
    ld.collide = false;
    int il = add(m, ld, "Load");
    int j = m.addLink(LinkKind::Rod, it, il, {0, -0.8}, {0, 0.9});
    (void)j;
    m.run.duration = 20;
    return e;
}
Example roboticArm() {
    Example e{"Robotic arm (two joints)",
              "A two-link arm on a pedestal. Each joint has a motor torque (constant to hold the weight, plus a slow sinusoid to move) and rotary damping. "
              "The payload is welded to the wrist. Plot the joint angles and read the shoulder reaction.", {}};
    Model& m = e.model;
    m.name = "Robotic arm";
    m.env.setRealistic();
    add(m, makeFloor(6, 0, mat("Concrete"), {0, 3.2}), "Ground");
    Body l1 = makeBeam(1.4, 0.14, mat("Aluminium"), {0.7, 0});
    l1.massFromDensity = false; l1.mass = 6;
    int a1 = add(m, l1, "Upper arm");
    Body l2 = makeBeam(1.1, 0.12, mat("Aluminium"), {1.4 + 0.55, 0});
    l2.massFromDensity = false; l2.mass = 3;
    int a2 = add(m, l2, "Forearm");
    Body pl = makeBox(0.3, 0.3, mat("Steel"), {2.5 + 0.15, 0});
    pl.massFromDensity = false; pl.mass = 4;
    int a3 = add(m, pl, "Payload");
    int sh = m.addSupport(LinkKind::Pin, a1, {0.0, 0.0}, 0);
    int el = m.addLink(LinkKind::Pin, a1, a2, {1.4, 0.0}, {1.4, 0.0});
    m.addLink(LinkKind::Weld, a2, a3, {2.5, 0.0}, {2.5, 0.0});
    if (sh >= 0) { m.links[sh].motorTorque = -200; m.links[sh].motorAmp = 45; m.links[sh].motorW = 1.0; m.links[sh].jointC = 25; }
    if (el >= 0) { m.links[el].motorTorque = -65; m.links[el].motorAmp = 18; m.links[el].motorW = 1.7; m.links[el].jointC = 10; m.links[el].collideConnected = false; }
    m.run.duration = 15;
    return e;
}
}  // namespace

const std::vector<Example>& exampleGallery() {
    static std::vector<Example> g = [] {
        std::vector<Example> v;
        v.push_back(oneDof());          v.back().category = 0;
        v.push_back(twoDof());          v.back().category = 1;
        v.push_back(tmd());             v.back().category = 1;
        v.push_back(pendulum());        v.back().category = 0;
        v.push_back(rodPendulum());     v.back().category = 0;
        v.push_back(quarterCar());      v.back().category = 1;
        v.push_back(bounce());          v.back().category = 0;
        v.push_back(incline());         v.back().category = 1;
        v.push_back(billiards());       v.back().category = 0;
        v.push_back(rolling());         v.back().category = 0;
        v.push_back(stackOfBoxes());    v.back().category = 1;
        v.push_back(beamFalls());       v.back().category = 0;
        v.push_back(pinnedBeam());      v.back().category = 0;
        v.push_back(simplySupported()); v.back().category = 1;
        v.push_back(cantilever());      v.back().category = 1;
        v.push_back(springAssembly());  v.back().category = 1;
        v.push_back(gantryCrane());     v.back().category = 2;
        v.push_back(roboticArm());      v.back().category = 2;
        for (Example& e : v) e.model.updateMass();
        return v;
    }();
    return g;
}

}  // namespace vlab
