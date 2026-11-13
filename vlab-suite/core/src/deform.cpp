#include "vlab/deform.hpp"
#include <algorithm>
#include <cmath>

namespace vlab {

bool isBeamBody(const Body& b) {
    return b.kind == BodyKind::Dynamic && b.shape.preset == ShapePreset::Box && b.shape.h > 1e-6 && b.shape.w >= 4.0 * b.shape.h;
}

// ====================================================================== the beam model
void BeamFE::build(int bodyIndex, const Body& b) {
    body = bodyIndex;
    L = b.shape.w;
    h = b.shape.h;
    E = b.mat.youngs;
    I = h * h * h / 12.0;                          // per metre of depth
    EI = E * I;
    mu = b.massKg / std::max(1e-9, L);
    const int N = elements;
    x.resize(N + 1);
    for (int i = 0; i <= N; ++i) x[i] = -L / 2 + L * i / N;
    valid = E > 0 && EI > 0 && L > 0;
    chol.clear();
    if (!valid) return;
    const int n = 2 * N;                           // dofs: w and theta at every node, minus the two at node 0 (clamped for the solve)
    std::vector<double> K((size_t)n * n, 0.0);
    const double l = L / N, c = EI / (l * l * l);
    const double ke[4][4] = {{12, 6 * l, -12, 6 * l}, {6 * l, 4 * l * l, -6 * l, 2 * l * l}, {-12, -6 * l, 12, -6 * l}, {6 * l, 2 * l * l, -6 * l, 4 * l * l}};
    for (int e = 0; e < N; ++e)
        for (int a = 0; a < 4; ++a)
            for (int d = 0; d < 4; ++d) {
                int ra = 2 * e + a - 2, rd = 2 * e + d - 2;
                if (ra >= 0 && rd >= 0) K[(size_t)ra * n + rd] += c * ke[a][d];
            }
    for (int i = 0; i < n; ++i) {                  // Cholesky K = L L^T, in place in the lower triangle
        for (int j = 0; j <= i; ++j) {
            double s = K[(size_t)i * n + j];
            for (int k = 0; k < j; ++k) s -= K[(size_t)i * n + k] * K[(size_t)j * n + k];
            if (i == j) {
                if (s <= 0) { valid = false; return; }
                K[(size_t)i * n + i] = std::sqrt(s);
            } else {
                K[(size_t)i * n + j] = s / K[(size_t)j * n + j];
            }
        }
    }
    chol = std::move(K);
}

namespace {
struct Load { double x, P, M; };
struct Support { double x; bool fixed; };
}

void beamDeflection(const World& world, const BeamFE& fe, Deflection& out) {
    const int N = fe.elements;
    out.w.assign(fe.x.size(), 0.0);
    out.maxAbs = out.maxSigned = out.atX = out.load = 0;
    if (!fe.valid || fe.body < 0 || fe.body >= (int)world.state().size()) return;
    const Model& m = world.model();
    const BodyState& S = world.state()[fe.body];
    const Vec2 ex = S.rot.apply(Vec2(1, 0)), ey = S.rot.apply(Vec2(0, 1));
    const double L = fe.L, l = L / N;
    auto xOf = [&](Vec2 wp) { return clampd(dot(wp - S.pos, ex), -L / 2, L / 2); };

    std::vector<Load> loads;
    std::vector<Support> sups;
    const double dt = m.env.dt;
    for (const ContactInfo& c : world.contacts()) {                       // contact forces (normal part)
        double sign;
        int other;
        if (c.a == fe.body) { sign = -1; other = c.b; }
        else if (c.b == fe.body) { sign = 1; other = c.a; }
        else continue;
        double F = c.normalImpulse / dt;
        double x = xOf(c.point);
        loads.push_back({x, dot(c.normal * (sign * F), ey), 0.0});
        out.load += F;
        if (other >= 0 && other < (int)m.bodies.size() && m.bodies[other].kind != BodyKind::Dynamic) sups.push_back({x, false});
    }
    for (size_t i = 0; i < m.links.size(); ++i) {                         // springs, dampers, supports and joints
        const Link& l_ = m.links[i];
        if ((l_.a != fe.body && l_.b != fe.body) || l_.a == l_.b || l_.kind == LinkKind::Rod) continue;
        const Vec2 pa = world.attachmentWorld(l_.a, l_.la), pb = world.attachmentWorld(l_.b, l_.lb);
        LinkReading r = world.link((int)i);
        if (l_.kind == LinkKind::Spring || l_.kind == LinkKind::Damper) {
            Vec2 d = pb - pa;
            double len = length(d);
            if (len < 1e-9) continue;
            Vec2 u = d * (1.0 / len);
            if (l_.b == fe.body) loads.push_back({xOf(pb), dot(u * (-r.force), ey), 0.0});
            if (l_.a == fe.body) loads.push_back({xOf(pa), dot(u * r.force, ey), 0.0});
            continue;
        }
        Vec2 R(r.rx, r.ry);                                               // reaction on body B
        int other;
        double x;
        if (l_.b == fe.body) { x = xOf(pb); loads.push_back({x, dot(R, ey), r.moment}); other = l_.a; }
        else { x = xOf(pa); loads.push_back({x, dot(R * -1.0, ey), -r.moment}); other = l_.b; }
        if (other >= 0 && other < (int)m.bodies.size() && m.bodies[other].kind != BodyKind::Dynamic) sups.push_back({x, l_.kind == LinkKind::Weld});
    }
    const double qg = fe.mu * m.env.gravity * ey.y;                       // own weight, transverse part per metre
    double Fn = qg * L, Mn = 0;
    for (const Load& ld : loads) { Fn += ld.P; Mn += ld.P * ld.x + ld.M; }
    const double q0 = qg - Fn / L, q1 = -12.0 * Mn / (L * L * L);         // d'Alembert: q(x) = q0 + q1 x makes the free beam balance

    std::vector<double> f(2 * (N + 1), 0.0);
    for (int e = 0; e < N; ++e) {                                         // linearly varying distributed load
        double a = q0 + q1 * fe.x[e], b = q0 + q1 * fe.x[e + 1];
        f[2 * e] += l * (7 * a + 3 * b) / 20;
        f[2 * e + 1] += l * l * (3 * a + 2 * b) / 60;
        f[2 * e + 2] += l * (3 * a + 7 * b) / 20;
        f[2 * e + 3] -= l * l * (2 * a + 3 * b) / 60;
    }
    for (const Load& ld : loads) {                                        // point forces and couples
        int e = std::clamp((int)std::floor((ld.x + L / 2) / l), 0, N - 1);
        double xi = (ld.x - fe.x[e]) / l, x2 = xi * xi, x3 = x2 * xi;
        f[2 * e] += ld.P * (1 - 3 * x2 + 2 * x3) + ld.M * (-6 * xi + 6 * x2) / l;
        f[2 * e + 1] += ld.P * l * (xi - 2 * x2 + x3) + ld.M * (1 - 4 * xi + 3 * x2);
        f[2 * e + 2] += ld.P * (3 * x2 - 2 * x3) + ld.M * (6 * xi - 6 * x2) / l;
        f[2 * e + 3] += ld.P * l * (-x2 + x3) + ld.M * (-2 * xi + 3 * x2);
    }
    {   // remove the rounding left by the discretisation so the nodal loads balance exactly
        double R = 0, Mr = 0, Sxx = 0;
        for (int i = 0; i <= N; ++i) { R += f[2 * i]; Mr += f[2 * i] * fe.x[i] + f[2 * i + 1]; Sxx += fe.x[i] * fe.x[i]; }
        double c0 = -R / (N + 1), c1 = Sxx > 0 ? -Mr / Sxx : 0.0;
        for (int i = 0; i <= N; ++i) f[2 * i] += c0 + c1 * fe.x[i];
    }

    const int n = 2 * N;                                                  // solve K u = f with node 0 clamped
    std::vector<double> u(2 * (N + 1), 0.0), y(n);
    for (int i = 0; i < n; ++i) {
        double s = f[i + 2];
        for (int k = 0; k < i; ++k) s -= fe.chol[(size_t)i * n + k] * y[k];
        y[i] = s / fe.chol[(size_t)i * n + i];
    }
    for (int i = n - 1; i >= 0; --i) {
        double s = y[i];
        for (int k = i + 1; k < n; ++k) s -= fe.chol[(size_t)k * n + i] * u[k + 2];
        u[i + 2] = s / fe.chol[(size_t)i * n + i];
    }

    auto eval = [&](double xq, double* slope) {                           // Hermite interpolation of w and w'
        int e = std::clamp((int)std::floor((xq + L / 2) / l), 0, N - 1);
        double xi = (xq - fe.x[e]) / l, x2 = xi * xi, x3 = x2 * xi;
        double w1 = u[2 * e], t1 = u[2 * e + 1], w2 = u[2 * e + 2], t2 = u[2 * e + 3];
        if (slope) *slope = w1 * (-6 * xi + 6 * x2) / l + t1 * (1 - 4 * xi + 3 * x2) + w2 * (6 * xi - 6 * x2) / l + t2 * (-2 * xi + 3 * x2);
        return w1 * (1 - 3 * x2 + 2 * x3) + t1 * l * (xi - 2 * x2 + x3) + w2 * (3 * x2 - 2 * x3) + t2 * l * (-x2 + x3);
    };
    // choose the rigid-body part (a + b x) to subtract: it is whatever the supports fix
    double a = 0, b = 0;
    auto globalFit = [&](double* aa, double* bb) {
        double mx = 0, mw = 0, sxx = 0, sxw = 0;
        for (int i = 0; i <= N; ++i) { mx += fe.x[i]; mw += u[2 * i]; }
        mx /= (N + 1); mw /= (N + 1);
        for (int i = 0; i <= N; ++i) { sxx += (fe.x[i] - mx) * (fe.x[i] - mx); sxw += (fe.x[i] - mx) * (u[2 * i] - mw); }
        *bb = sxx > 0 ? sxw / sxx : 0.0;
        *aa = mw - *bb * mx;
    };
    const Support* fixedSup = nullptr;
    for (const Support& s : sups) if (s.fixed) { fixedSup = &s; break; }
    if (fixedSup) {
        double slope;
        double wf = eval(fixedSup->x, &slope);
        b = slope;
        a = wf - b * fixedSup->x;
    } else if (!sups.empty()) {
        double xmin = sups[0].x, xmax = sups[0].x;
        for (const Support& s : sups) { xmin = std::min(xmin, s.x); xmax = std::max(xmax, s.x); }
        if (xmax - xmin > 0.05 * L) {                                     // two or more separate supports: the chord through them
            double mx = 0, mw = 0, sxx = 0, sxw = 0;
            std::vector<double> sw;
            for (const Support& s : sups) { sw.push_back(eval(s.x, nullptr)); mx += s.x; mw += sw.back(); }
            mx /= sups.size(); mw /= sups.size();
            for (size_t k = 0; k < sups.size(); ++k) { sxx += (sups[k].x - mx) * (sups[k].x - mx); sxw += (sups[k].x - mx) * (sw[k] - mw); }
            b = sxx > 0 ? sxw / sxx : 0.0;
            a = mw - b * mx;
        } else {                                                          // one support point: pinned there, free to turn
            double aa, bb;
            globalFit(&aa, &bb);
            double ws = eval(sups[0].x, nullptr);
            b = bb;
            a = ws - b * sups[0].x;
        }
    } else {
        globalFit(&a, &b);                                                // nothing holds it: show the shape about its own mean line
    }
    for (int i = 0; i <= N; ++i) {
        double w = u[2 * i] - (a + b * fe.x[i]);
        out.w[i] = w;
        if (std::fabs(w) > out.maxAbs) { out.maxAbs = std::fabs(w); out.maxSigned = w; out.atX = fe.x[i]; }
    }
}

template <class T>
static double stressFrom(const BeamFE& fe, const T* w) {
    const int N = fe.elements;
    const double l = fe.L / N;
    double kmax = 0;
    for (int i = 1; i < N; ++i) kmax = std::max(kmax, std::fabs((double)w[i - 1] - 2.0 * w[i] + w[i + 1]) / (l * l));
    return fe.E * kmax * fe.h / 2.0;
}
double bendingStress(const BeamFE& fe, const std::vector<double>& w) { return w.size() == fe.x.size() ? stressFrom(fe, w.data()) : 0.0; }
double bendingStress(const BeamFE& fe, const float* w) { return stressFrom(fe, w); }

// ====================================================================== squash of a compact body
double contactForce(const World& world, int body) {
    double sum = 0;
    const double dt = world.model().env.dt;
    for (const ContactInfo& c : world.contacts())
        if (c.a == body || c.b == body) sum += c.normalImpulse / dt;
    return sum;
}

Squash bodySquash(const World& world, int body) {
    Squash s;
    const Model& m = world.model();
    if (body < 0 || body >= (int)m.bodies.size()) return s;
    const Body& bd = m.bodies[body];
    if (!bd.isDynamic() || bd.shape.area <= 1e-9 || bd.mat.youngs <= 0) return s;
    const double dt = m.env.dt;
    struct Fc { Vec2 dir; double F; };                                    // direction of the force on this body, magnitude
    std::vector<Fc> fc;
    double best = 0;
    Vec2 axis;
    for (const ContactInfo& c : world.contacts()) {
        if (c.a != body && c.b != body) continue;
        double F = c.normalImpulse / dt;
        Vec2 dir = c.b == body ? c.normal : c.normal * -1.0;
        fc.push_back({dir, F});
        if (F > best) { best = F; axis = dir; }
    }
    if (best <= 0) return s;
    double up = 0, down = 0;                                              // forces pressing from the two opposite sides
    for (const Fc& f : fc) {
        double d = dot(f.dir, axis);
        if (d > 0) up += f.F * d; else down -= f.F * d;
    }
    s.axis = axis;
    s.load = std::max(up, down);
    const BodyState& S = world.state()[body];
    double lo = 1e30, hi = -1e30;
    if (bd.shape.isCircle()) { lo = -bd.shape.radius; hi = bd.shape.radius; }
    else for (const Vec2& v : bd.shape.verts) { double d = dot(S.rot.apply(v), axis); lo = std::min(lo, d); hi = std::max(hi, d); }
    s.length = std::max(1e-6, hi - lo);
    s.compression = s.load * s.length * s.length / (bd.mat.youngs * bd.shape.area);   // F L / (E A) with A = area / L
    s.strain = s.compression / s.length;
    return s;
}

}  // namespace vlab
