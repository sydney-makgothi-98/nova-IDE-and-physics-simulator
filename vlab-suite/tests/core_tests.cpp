// core_tests.cpp -- the validation suite. Every physics claim the product makes is checked here against
// an analytic answer or a conservation law, with the measured number printed.
//   vlab_tests            runs everything, exit code 0 = all passed
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "vlab/analysis.hpp"
#include "vlab/model.hpp"
#include "vlab/project_io.hpp"
#include "vlab/telemetry.hpp"
#include "vlab/world.hpp"

using namespace vlab;

static int gFail = 0, gTotal = 0;
static void check(const char* name, bool ok, const char* fmt, ...) {
    char buf[300];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::printf("  [%s] %-46s %s\n", ok ? "PASS" : "FAIL", name, buf);
    ++gTotal;
    if (!ok) ++gFail;
}
static void run(World& w, double seconds) {
    long long n = (long long)(seconds / w.model().env.dt + 0.5);
    for (long long i = 0; i < n; ++i) w.step();
}
static const Material& M(const char* n) { return *findMaterial(n); }
static Body withMass(Body b, double m) { b.mass = m; b.massFromDensity = false; return b; }

// ------------------------------------------------------------------ shapes and mass properties
static void testShapes() {
    std::printf("Shapes and mass properties\n");
    Body box = makeBox(2.0, 1.0, M("Steel"), {});
    check("box inertia I/m = (w^2+h^2)/12", std::fabs(box.shape.inertiaPerMass - (4.0 + 1.0) / 12.0) < 1e-12, "%.6f vs %.6f", box.shape.inertiaPerMass, 5.0 / 12.0);
    Body circ = makeCircle(0.5, M("Steel"), {});
    check("circle area and I/m = r^2/2", std::fabs(circ.shape.area - kPi * 0.25) < 1e-12 && std::fabs(circ.shape.inertiaPerMass - 0.125) < 1e-12, "area %.6f", circ.shape.area);
    Body hex = makeRegular(6, 1.0, M("Steel"), {});
    check("regular hexagon area = 3 sqrt(3)/2 r^2", std::fabs(hex.shape.area - 1.5 * std::sqrt(3.0)) < 1e-9, "%.6f vs %.6f", hex.shape.area, 1.5 * std::sqrt(3.0));
    Body wedge = makeWedge(3.0, 2.0, M("Steel"), {});
    check("wedge area = w h / 2, centred on its centroid", std::fabs(wedge.shape.area - 3.0) < 1e-9, "area %.6f", wedge.shape.area);
    Model m;
    m.addBody(withMass(makeBox(1, 1, M("Steel"), {}), 4.0));
    check("mass override, inertia = m (w^2+h^2)/12", std::fabs(m.bodies[0].inertia - 4.0 * 2.0 / 12.0) < 1e-12, "%.6f", m.bodies[0].inertia);
}

// ------------------------------------------------------------------ smooth Lagrangian systems (RK4 path)
static void testSmooth() {
    std::printf("Smooth systems (springs, dampers, rails)\n");
    {
        Model m = exampleGallery()[0].model;                      // 1-DOF: m=2, k=30, c=1.5, stretched 1 m
        World w(m);
        check("1-DOF uses the RK4 path", !w.usesConstraintSolver(), "constraint solver off");
        const double mm = 2, k = 30, c = 1.5, wn = std::sqrt(k / mm), z = c / (2 * std::sqrt(k * mm)), wd = wn * std::sqrt(1 - z * z);
        double worst = 0;
        long long n = (long long)(10.0 / m.env.dt);
        for (long long i = 0; i < n; ++i) {
            w.step();
            double t = w.time(), u = std::exp(-z * wn * t) * (std::cos(wd * t) + z * wn / wd * std::sin(wd * t));
            worst = std::fmax(worst, std::fabs((-1.0 + u) - w.state()[1].pos.x));
        }
        check("1-DOF matches the exact solution", worst < 1e-6, "worst gap %.2e m over 10 s", worst);
        check("1-DOF energy ledger closes", std::fabs(w.ledger().numerical()) < 1e-6, "numerical error %.2e J", w.ledger().numerical());
    }
    {
        Model m = exampleGallery()[1].model;                      // two masses driven
        m.bodies[2].driveF = 6;
        m.bodies[2].driveW = 5;
        World w(m);
        double worst = 0;
        long long n = (long long)(20.0 / m.env.dt);
        for (long long i = 0; i < n; ++i) { w.step(); if (i % 100 == 0) worst = std::fmax(worst, std::fabs(w.ledger().numerical())); }
        check("driven 2-DOF energy ledger closes", worst < 2e-4, "worst error %.2e J over 20 s (E to %.1f J)", worst, w.ledger().E);
    }
    {
        Model m = exampleGallery()[3].model;                      // spring pendulum, dampers removed
        for (int i = (int)m.links.size() - 1; i >= 0; --i) if (m.links[i].kind == LinkKind::Damper) m.removeLink(i);
        World w(m);
        double worst = 0, e0 = w.ledger().E;
        long long n = (long long)(30.0 / m.env.dt);
        for (long long i = 0; i < n; ++i) { w.step(); if (i % 100 == 0) worst = std::fmax(worst, std::fabs(w.ledger().E - e0)); }
        check("frictionless spring pendulum conserves energy", worst < 1e-5, "E wandered %.2e J of %.3f J", worst, e0);
    }
    {
        Model m = exampleGallery()[5].model;                      // quarter car, rails
        World w(m);
        run(w, 3.0);
        double drift = std::fabs(w.state()[1].pos.x) + std::fabs(w.state()[2].pos.x);
        check("rails: locked axis never moves", drift == 0.0, "sideways drift %.1e m", drift);
    }
    {
        Model m = exampleGallery()[5].model;                      // static sag = m g / k
        m.bodies[1].vel = {};
        m.bodies[1].pos.y = 1.0;
        World w(m);
        run(w, 40.0);                                             // let the damper remove the ring
        double kTyre = 220, kSusp = 70, mw = 1.0, mc = 4.0, g = 9.81;
        double sagTyre = (mw + mc) * g / kTyre, sagSusp = mc * g / kSusp;
        double tyre = w.state()[1].pos.y - 1.0, susp = (w.state()[2].pos.y - w.state()[1].pos.y) - (-1.0 - 1.0);
        check("suspension settles to static sag m g / k", std::fabs(tyre - sagTyre) < 1e-3 && std::fabs(susp - sagSusp) < 1e-3, "tyre %.4f (%.4f)  susp %.4f (%.4f)", tyre, sagTyre, susp, sagSusp);
    }
}

// ------------------------------------------------------------------ rod pendulum
static void testPendulum() {
    std::printf("Rod pendulum\n");
    Model m = exampleGallery()[4].model;
    m.env.setIdeal();
    m.env.airDrag = false;
    // small swing: 0.1 rad
    m.bodies[1].pos = {std::sin(0.1) * 2.0, -2 + std::cos(0.1) * 2.0};
    World w(m);
    check("rod pendulum uses the constraint solver", w.usesConstraintSolver(), "yes");
    double e0 = w.ledger().E, worst = 0;
    std::vector<double> cross;
    double prev = w.state()[1].pos.x;
    long long n = (long long)(20.0 / m.env.dt);
    for (long long i = 0; i < n; ++i) {
        w.step();
        double x = w.state()[1].pos.x;
        if (prev < 0 && x >= 0) cross.push_back(w.time());
        prev = x;
        if (i % 200 == 0) worst = std::fmax(worst, std::fabs(w.ledger().E - e0));
    }
    double period = cross.size() >= 3 ? (cross.back() - cross.front()) / (double)(cross.size() - 1) : 0;
    double T = 2 * kPi * std::sqrt(2.0 / 9.81) * (1 + 0.1 * 0.1 / 16);
    check("pendulum period 2 pi sqrt(L/g)", std::fabs(period - T) / T < 0.01, "measured %.4f s, theory %.4f s", period, T);
    check("pendulum energy bounded (ideal)", worst < 0.01 * std::fabs(e0 > 0 ? e0 : 1.0) + 0.02, "E wandered %.2e J", worst);
}

// ------------------------------------------------------------------ collisions
static void testCollisions() {
    std::printf("Collisions, restitution and the energy ledger\n");
    {
        Model m = exampleGallery()[6].model;                      // bouncing ball, rubber on concrete (e = max = 0.8)
        World w(m);
        double top = 0, hmax = 0;
        bool bounced = false, falling = true;
        double floorTop = 2.0, r = 0.25, h0 = 2.0;
        double prevY = w.state()[1].pos.y;
        double peak = 0;
        long long n = (long long)(3.0 / m.env.dt);
        for (long long i = 0; i < n; ++i) {
            w.step();
            double y = w.state()[1].pos.y, vy = w.state()[1].vel.y;
            if (falling && vy < 0 && prevY > y - 1e-9 && y < floorTop - r - 0.2) { /* still above */ }
            if (!bounced && vy < 0) bounced = true;               // moving up after the first impact
            if (bounced) { double height = floorTop - r - y; peak = std::fmax(peak, height); if (vy > 0 && peak > 0.3) break; }
            prevY = y;
            (void)top; (void)hmax;
        }
        double expect = 0.8 * 0.8 * h0;
        check("rubber ball rebounds to e^2 h", std::fabs(peak - expect) / expect < 0.03, "first rebound %.4f m, theory %.4f m", peak, expect);
        World w2(m);
        run(w2, 0.7);                                             // the first impact happens near t = 0.64 s
        double loss = 0.5 * 9.81 * 2.0 * (1 - 0.64);
        check("impact loss = m g h (1 - e^2) in the ledger", std::fabs(w2.ledger().dImpact - loss) / loss < 0.05, "ledger %.3f J, theory %.3f J", w2.ledger().dImpact, loss);
        run(w2, 5.0);
        check("ledger closes through bounces and rest", std::fabs(w2.ledger().numerical()) < 0.03 * std::fabs(w2.ledger().E0 + 5.0), "numerical error %.3f J (start %.2f J)", w2.ledger().numerical(), w2.ledger().E0);
        check("ball comes to rest on the floor", length(w2.state()[1].vel) < 0.05 && std::fabs(w2.state()[1].pos.y - (2.0 - 0.25)) < 0.01, "speed %.4f, y %.4f", length(w2.state()[1].vel), w2.state()[1].pos.y);
    }
    {
        Model m = exampleGallery()[6].model;
        m.env.setIdeal();
        m.bodies[1].mat = M("Ideal (frictionless, elastic)");
        m.bodies[0].mat = M("Ideal (frictionless, elastic)");
        m.env.restThreshold = 0.05;
        World w(m);
        double peak = 0;
        bool up = false;
        long long n = (long long)(1.6 / m.env.dt);
        for (long long i = 0; i < n; ++i) {
            w.step();
            if (w.state()[1].vel.y < 0) up = true;
            if (up) peak = std::fmax(peak, 2.0 - 0.25 - w.state()[1].pos.y);
        }
        check("ideal ball bounces back to its drop height", std::fabs(peak - 2.0) / 2.0 < 0.02, "rebound %.4f m of 2.0 m", peak);
    }
    {
        Model m = exampleGallery()[8].model;                      // two balls, elastic
        World w(m);
        double p0 = w.state()[3].vel.x * 1.0 + w.state()[4].vel.x * 1.0;
        double ke0 = w.ledger().T;
        run(w, 1.6);
        double v1 = w.state()[3].vel.x, v2 = w.state()[4].vel.x;
        check("equal-mass elastic collision swaps velocities", std::fabs(v1) < 0.02 && std::fabs(v2 - 4.0) < 0.02, "cue %.4f, target %.4f m/s", v1, v2);
        check("momentum and energy conserved (e = 1)", std::fabs(v1 + v2 - p0) < 0.01 && std::fabs(w.ledger().T - ke0) / ke0 < 0.005, "p %.4f -> %.4f, KE %.3f -> %.3f", p0, v1 + v2, ke0, w.ledger().T);
        Model m2 = m;
        m2.env.setRealistic();
        m2.bodies[3].mat.restitution = m2.bodies[4].mat.restitution = 0.5;
        m2.bodies[0].mat.restitution = m2.bodies[1].mat.restitution = m2.bodies[2].mat.restitution = 0.5;
        World w2(m2);
        run(w2, 1.6);
        // two equal masses, e = 0.5, one at rest: KE loss fraction = (1 - e^2)/2 of the initial KE
        double expectLoss = 0.5 * 1.0 * 16.0 * (1 - 0.25) / 2.0;
        check("collision with e = 0.5 loses (1 - e^2)/2 of the KE", std::fabs(w2.ledger().dImpact - expectLoss) / expectLoss < 0.03, "ledger %.3f J, theory %.3f J", w2.ledger().dImpact, expectLoss);
    }
}

// ------------------------------------------------------------------ friction
static void testFriction() {
    std::printf("Friction on an incline\n");
    Model m = exampleGallery()[7].model;                          // four blocks on a 28 degree wood ramp
    double th = 28 * kPi / 180;
    auto displacement = [&](World& w, int body, double secs) {
        Vec2 p0 = w.state()[body].pos;
        run(w, secs);
        return dot(w.state()[body].pos - p0, Rot(th).apply(Vec2(1, 0)));
    };
    {
        World w(m);
        double dR = displacement(w, 1, 2.0);                      // rubber: mu_s = sqrt(1.0*0.5) = 0.71 > tan 28 = 0.53
        World w2(m);
        double dS = displacement(w2, 2, 2.0);                     // steel: 0.61 > 0.53
        check("rubber and steel blocks stay put (mu_s > tan)", std::fabs(dR) < 0.01 && std::fabs(dS) < 0.01, "slip %.4f m and %.4f m", dR, dS);
    }
    {
        World w(m);
        run(w, 0.5);
        double d0 = dot(w.state()[3].pos, Rot(th).apply(Vec2(1, 0)));
        run(w, 1.0);
        double d1 = dot(w.state()[3].pos, Rot(th).apply(Vec2(1, 0)));
        // wood on wood ramp: mu_k = sqrt(0.35*0.35) = 0.35 ; (the sim starts the block already sliding at t = 0.5, so use the velocity)
        double v = dot(w.state()[3].vel, Rot(th).apply(Vec2(1, 0)));
        double a = 9.81 * (std::sin(th) - 0.35 * std::cos(th));
        run(w, 0.5);
        double v2 = dot(w.state()[3].vel, Rot(th).apply(Vec2(1, 0)));
        double acc = (v2 - v) / 0.5;
        (void)d0; (void)d1;
        check("sliding block: a = g (sin - mu_k cos)", std::fabs(acc - a) / a < 0.03, "measured %.3f m/s2, theory %.3f", acc, a);
    }
    {
        Model mi = m;
        mi.env.setIdeal();
        World w(mi);
        run(w, 0.5);
        double v = dot(w.state()[1].vel, Rot(th).apply(Vec2(1, 0)));
        double a = 9.81 * std::sin(th);
        run(w, 0.5);
        double acc = (dot(w.state()[1].vel, Rot(th).apply(Vec2(1, 0))) - v) / 0.5;
        check("ideal (frictionless) incline: a = g sin(angle)", std::fabs(acc - a) / a < 0.02, "measured %.3f, theory %.3f", acc, a);
    }
    {
        Model m2;                                                 // friction energy: block sliding to a stop on a flat floor
        m2.env.setRealistic();
        m2.addBody(makeFloor(40, 0, M("Wood"), {0, 0.0}));
        Body b = withMass(makeBox(0.5, 0.5, M("Wood"), {-15, -0.25}), 2.0);
        b.vel = {6.0, 0};
        m2.addBody(b);
        World w(m2);
        double x0 = w.state()[1].pos.x;
        run(w, 6.0);
        double dist = w.state()[1].pos.x - x0, mu = 0.35;
        double expectDist = 36.0 / (2 * mu * 9.81);
        double expectHeat = mu * 2.0 * 9.81 * expectDist;
        check("stopping distance v^2 / (2 mu g)", std::fabs(dist - expectDist) / expectDist < 0.03, "%.3f m vs %.3f m", dist, expectDist);
        check("friction heat = mu m g d in the ledger", std::fabs(w.ledger().dFriction - expectHeat) / expectHeat < 0.03, "%.3f J vs %.3f J", w.ledger().dFriction, expectHeat);
    }
    {
        Model mr = exampleGallery()[9].model;                     // rolling ball: a = (2/3) g sin(angle) for a solid disc
        mr.env.rolling = false;
        World w(mr);
        run(w, 1.0);
        Rot rr(20 * kPi / 180);
        double v = dot(w.state()[1].vel, rr.apply(Vec2(1, 0)));
        run(w, 0.5);
        double acc = (dot(w.state()[1].vel, rr.apply(Vec2(1, 0))) - v) / 0.5;
        double a = 2.0 / 3.0 * 9.81 * std::sin(20 * kPi / 180);
        check("rolling disc: a = (2/3) g sin(angle)", std::fabs(acc - a) / a < 0.03, "measured %.3f, theory %.3f", acc, a);
    }
}

// ------------------------------------------------------------------ analysis
static void testAnalysis() {
    std::printf("Analysis\n");
    {
        ModalResult r = modalAnalysis(exampleGallery()[0].model);
        double f = std::sqrt(30.0 / 2.0) / (2 * kPi), z = 1.5 / (2 * std::sqrt(30.0 * 2.0));
        check("modal: 1-DOF frequency sqrt(k/m)/2pi", r.modes.size() == 1 && std::fabs(r.modes[0].freqHz - f) / f < 1e-4, "%.5f Hz vs %.5f", r.modes.empty() ? 0.0 : r.modes[0].freqHz, f);
        check("modal: 1-DOF damping ratio c/(2 sqrt(km))", r.modes.size() == 1 && std::fabs(r.modes[0].dampingRatio - z) < 1e-3, "%.5f vs %.5f", r.modes.empty() ? 0.0 : r.modes[0].dampingRatio, z);
    }
    {
        ModalResult r = modalAnalysis(exampleGallery()[1].model);  // masses 1, 1: K = [[65,-25],[-25,65]]
        double f1 = std::sqrt(40.0) / (2 * kPi), f2 = std::sqrt(90.0) / (2 * kPi);
        check("modal: 2-DOF natural frequencies", r.modes.size() == 2 && std::fabs(r.modes[0].freqHz - f1) / f1 < 1e-4 && std::fabs(r.modes[1].freqHz - f2) / f2 < 1e-4,
              "%.4f, %.4f Hz vs %.4f, %.4f", r.modes[0].freqHz, r.modes[1].freqHz, f1, f2);
    }
    {
        std::vector<double> t, y;
        for (int i = 0; i < 1200; ++i) { t.push_back(i / 100.0); y.push_back(std::sin(2 * kPi * 2.5 * i / 100.0) * std::exp(-0.05 * i / 100.0) + 3.0); }
        Spectrum s = computeSpectrum(t, y);
        check("FFT finds the 2.5 Hz tone (offset removed)", std::fabs(s.peakHz - 2.5) < 0.05, "peak %.3f Hz", s.peakHz);
    }
    {
        Model m = exampleGallery()[0].model;
        World w(m);
        Telemetry tel;
        tel.begin(m);
        long long n = (long long)(12.0 / m.env.dt), every = (long long)(0.01 / m.env.dt);
        for (long long i = 0; i < n; ++i) { w.step(); if (i % every == 0) tel.append(w); }
        int c = tel.find("Mass x");
        double z = estimateDampingRatio(tel.t, tel.col[c]);
        double zt = 1.5 / (2 * std::sqrt(30.0 * 2.0));
        check("log-decrement damping estimate", z > 0 && std::fabs(z - zt) / zt < 0.05, "%.4f vs %.4f", z, zt);
        Spectrum s = computeSpectrum(tel.t, tel.col[c]);
        double fd = std::sqrt(15.0) * std::sqrt(1 - zt * zt) / (2 * kPi);
        check("telemetry spectrum peak = damped frequency", std::fabs(s.peakHz - fd) < 0.05, "%.3f Hz vs %.3f", s.peakHz, fd);
    }
    {
        FrfRequest rq;
        rq.driveBody = 1; rq.responseBody = 1; rq.f0 = 0.4; rq.f1 = 0.9; rq.points = 6; rq.forceAmp = 1.0; rq.settleSeconds = 15;
        Model m = exampleGallery()[0].model;
        std::vector<FrfPoint> pts = frequencyResponse(m, rq, nullptr, nullptr);
        double bestF = 0, bestA = 0;
        for (auto& p : pts) if (p.amplitude > bestA) { bestA = p.amplitude; bestF = p.freqHz; }
        double fr = std::sqrt(15.0) * std::sqrt(1 - 2 * 0.0968 * 0.0968) / (2 * kPi);
        double wn = std::sqrt(15.0), zeta = 1.5 / (2 * std::sqrt(60.0)), f = 0.6;
        double w0 = 2 * kPi * f, theory = 1.0 / std::sqrt(std::pow(30.0 - 2.0 * w0 * w0, 2) + std::pow(1.5 * w0, 2));
        double got = 0;
        for (auto& p : pts) if (std::fabs(p.freqHz - 0.6) < 1e-9) got = p.amplitude;
        (void)wn; (void)zeta; (void)fr; (void)bestF;
        check("frequency response matches |H(w)| = 1/sqrt((k-m w^2)^2+(c w)^2)", std::fabs(got - theory) / theory < 0.02, "%.5f m/N vs %.5f", got, theory);
    }
}

// ------------------------------------------------------------------ beams, supports, line floors
static void testStructures() {
    std::printf("Beams, supports and line floors\n");
    {
        Model m;                                                  // a ball dropped on a zero-thickness floor line stops on it
        m.env.setRealistic();
        m.addBody(makeFloor(10, 0, M("Concrete"), {0, 2.0}));
        m.addBody(withMass(makeCircle(0.25, M("Clay / putty"), {0, 0}), 1.0));
        World w(m);
        run(w, 3.0);
        double rest = 2.0 - 0.25;
        check("ball comes to rest ON a zero-thickness floor line", std::fabs(w.state()[1].pos.y - rest) < 0.01, "y %.4f vs %.4f", w.state()[1].pos.y, rest);
    }
    {
        Model m;                                                  // an unsupported beam falls
        m.env.setRealistic();
        m.addBody(withMass(makeBeam(3, 0.15, M("Wood"), {0, 0}), 5));
        World w(m);
        run(w, 1.0);
        double d = 0.5 * 9.81;
        check("an unsupported beam falls freely (y = g t^2 / 2)", std::fabs(w.state()[0].pos.y - d) < 0.02, "fell %.4f m vs %.4f m", w.state()[0].pos.y, d);
    }
    {
        Model m;                                                  // beam hinged at one end: compound pendulum, T = 2 pi sqrt(2L / 3g) (thin beam)
        m.env.setIdeal();
        double L = 2.0;
        double a0 = kPi / 2 + 0.1;                                // hanging almost straight down, released 0.1 rad off the vertical
        Body b = withMass(makeBeam(L, 0.01, M("Steel"), {L / 2 * std::cos(a0), L / 2 * std::sin(a0)}), 4.0);
        b.angle = a0;
        int bi = m.addBody(b);
        m.addSupport(LinkKind::Pin, bi, {0, 0}, 0);
        World w(m);
        double tFirst = 0, tLast = 0, period = 0;
        int crossings = 0;
        double prev = w.state()[0].angle - kPi / 2;
        for (long long i = 0; i < 80000 && crossings < 3; ++i) {
            w.step();
            double a = w.state()[0].angle - kPi / 2;
            if ((prev < 0 && a >= 0) || (prev > 0 && a <= 0)) { ++crossings; if (crossings == 1) tFirst = w.time(); tLast = w.time(); }
            prev = a;
        }
        if (crossings >= 3) period = tLast - tFirst;              // first to third zero crossing = one full period
        double T = 2 * kPi * std::sqrt(2 * L / (3 * 9.81));
        check("hinged beam period T = 2 pi sqrt(2L/3g)", crossings >= 3 && std::fabs(period - T) / T < 0.03, "%.4f s vs %.4f s", period, T);
        check("hinged beam: ideal energy ledger closes", std::fabs(w.ledger().numerical()) < 0.05, "numerical %.5f J", w.ledger().numerical());
    }
    {
        Model m = exampleGallery()[13].model;                     // simply supported beam: reactions sum to the total weight
        World w(m);
        run(w, 4.0);
        double Ry = 0;
        for (size_t i = 0; i < m.links.size(); ++i) Ry += w.link((int)i).ry;
        double W = (20.0 + 30.0) * 9.81;
        check("simply supported beam: reactions sum to the weight", std::fabs(std::fabs(Ry) - W) / W < 0.03, "%.1f N vs %.1f N", std::fabs(Ry), W);
        double r0 = w.link(0).ry, r1 = w.link(1).ry;
        check("symmetric load: both supports carry half", std::fabs(r0 - r1) / W < 0.05, "%.1f N and %.1f N", r0, r1);
    }
    {
        Model m = exampleGallery()[14].model;                     // cantilever: the fixed support holds the weight and the moment
        World w(m);
        run(w, 4.0);
        double W = (12.0 + 25.0) * 9.81;
        LinkReading r = w.link(0);
        double M0 = 12.0 * 9.81 * 1.5 + 25.0 * 9.81 * 2.8;
        double drop = w.state()[0].pos.y;
        check("cantilever holds: fixed support force = total weight", std::fabs(std::fabs(r.ry) - W) / W < 0.03, "%.1f N vs %.1f N", std::fabs(r.ry), W);
        check("cantilever moment = sum of weight x distance", std::fabs(std::fabs(r.moment) - M0) / M0 < 0.05, "%.1f N m vs %.1f N m", std::fabs(r.moment), M0);
        check("cantilever stays put (rigid, welded)", std::fabs(drop) < 0.01, "moved %.4f m", drop);
    }
}

// ------------------------------------------------------------------ files, determinism, the whole gallery
static void testSystem() {
    std::printf("Files, determinism and the example gallery\n");
    {
        Model a = exampleGallery()[7].model, b;
        std::string err;
        bool ok = modelFromText(modelToText(a), b, &err);
        ok = ok && a.bodies.size() == b.bodies.size() && a.links.size() == b.links.size() && b.bodies[1].mat.name == a.bodies[1].mat.name &&
             std::fabs(b.bodies[1].mat.muS - a.bodies[1].mat.muS) < 1e-9 && std::fabs(b.bodies[0].angle - a.bodies[0].angle) < 1e-9 && b.env.dt == a.env.dt;
        check("save / load round trip", ok, "%zu bodies, %zu links", b.bodies.size(), b.links.size());
        Model junk;
        bool refused = !modelFromText("hello\nworld\n", junk, &err);          // (evaluate first: the message is filled in by this call)
        check("a damaged file is refused, not crashed on", refused, "\"%s\"", err.c_str());
    }
    {
        Model m = exampleGallery()[7].model;
        World w1(m), w2(m);
        run(w1, 1.5);
        run(w2, 1.5);
        bool same = true;
        for (size_t i = 0; i < w1.state().size(); ++i)
            same = same && w1.state()[i].pos.x == w2.state()[i].pos.x && w1.state()[i].pos.y == w2.state()[i].pos.y && w1.state()[i].angle == w2.state()[i].angle;
        check("deterministic: two runs are bit-identical", same, "%lld steps", w1.stepCount());
    }
    {
        bool allOk = true;
        std::string worstName;
        double worstErr = 0;
        for (const Example& e : exampleGallery()) {
            World w(e.model);
            double peakT = 0;
            for (int k = 0; k < 200; ++k) { run(w, 0.01); peakT = std::max(peakT, w.ledger().T); }   // the energy that actually moved around
            if (!w.healthy()) { allOk = false; worstName = e.title; }
            double rel = std::fabs(w.ledger().numerical()) / (std::fabs(w.ledger().E0) + std::fabs(w.ledger().workIn) + w.ledger().dissipated() + std::fabs(w.ledger().unclassified) + std::fabs(w.ledger().posCorr) + peakT + 1.0);
            if (rel > worstErr) { worstErr = rel; worstName = e.title; }
            std::printf("      %-46s numerical %9.4f J  (dissipated %8.3f, pos-correction %7.3f, reversible contact exchange %7.3f)\n", e.title.c_str(), w.ledger().numerical(), w.ledger().dissipated(), w.ledger().posCorr, w.ledger().unclassified);
        }
        check("all gallery examples run 2 s without blowing up", allOk, "%zu examples %s", exampleGallery().size(), allOk ? "" : worstName.c_str());
        check("every example's energy ledger stays honest", worstErr < 0.05, "worst relative error %.4f (%s)", worstErr, worstName.c_str());
    }
    {
        Model m = exampleGallery()[7].model;                      // live edit while running: change a friction coefficient
        World w(m);
        run(w, 0.2);
        w.model().bodies[3].mat.muS = 0.0;
        w.model().bodies[3].mat.muK = 0.0;
        w.rebaseLedger();
        run(w, 1.0);
        double v = length(w.state()[3].vel);
        check("live edit: removing friction mid-run makes the block slide", v > 1.0 && std::fabs(w.ledger().numerical()) < 0.2, "speed %.3f m/s", v);
    }
}

int main() {
    std::printf("Vibration Lab Suite: validation suite\n\n");
    testShapes();
    testSmooth();
    testPendulum();
    testCollisions();
    testFriction();
    testStructures();
    testAnalysis();
    testSystem();
    std::printf("\n%d of %d checks passed\n", gTotal - gFail, gTotal);
    return gFail ? 1 : 0;
}
