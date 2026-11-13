// engine.hpp -- a Lagrangian physics engine for planar mass / spring / damper networks.
//
// WHAT IT SIMULATES
//   Point masses and fixed ground anchors scattered on a 2-D plane, joined by springs and
//   dampers.  y points DOWN (like the screen).  A mass can move freely, or be forced onto a
//   horizontal rail (X only) or a vertical rail (Y only).
//
// HOW (the Lagrangian recipe, derived fully in the book)
//   generalized coordinates   q = (x0, y0, x1, y1, ...)           positions of every node
//   kinetic energy            T = sum  1/2 m_i |v_i|^2
//   potential energy          V = sum  [ 1/2 k s^2 + 1/4 k3 s^4 ]   (springs, s = length - rest length)
//                                 + sum  ( - m g (y - y_design) )   (gravity, y is down)
//   Rayleigh dissipation      D = sum  1/2 c (sdot)^2               (dampers, sdot = rate of length change)
//   outside forces            Q_i = drive force + mouse force
//   Euler-Lagrange equations  d/dt( dL/dqdot ) - dL/dq + dD/dqdot = Q       with L = T - V
//   which, because T depends only on velocities and V only on positions, collapse to
//                             m_i a_i = - dV/dq_i - dD/dqdot_i + Q_i
//   accel() evaluates exactly that, step() integrates it with 4th-order Runge-Kutta.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace lag {

constexpr int    MAX_NODES = 24;
constexpr double PI        = 3.14159265358979323846;

enum class NodeKind { Mass, Ground };
enum class LinkKind { Spring, Damper };
enum Rail { FREE = 0, RAIL_X = 1, RAIL_Y = 2 };

// ------------------------------------------------------------------ the design (what the user draws)
struct Node {
    NodeKind kind = NodeKind::Mass;
    double x = 0, y = 0;          // design position in metres
    double m = 1.0;               // mass in kg (ignored for ground)
    double vx0 = 0, vy0 = 0;      // starting velocity (m/s)
    double rail = FREE;           // 0 free, 1 X-rail, 2 Y-rail  (a double so UI sliders can edit it)
    double driveF = 0;            // harmonic push amplitude (N)
    double driveAngle = 0;        // push direction in degrees: 0 = right, 90 = down
    double driveW = 0;            // push frequency (rad/s)
};

struct Link {
    LinkKind kind = LinkKind::Spring;
    int a = -1, b = -1;           // indices of the two nodes it joins
    double k = 20.0;              // spring stiffness (N/m)
    double k3 = 0.0;              // cubic stiffness (N/m^3): 0 = ideal linear spring
    double L0 = 1.0;              // spring rest length (m)
    bool   autoRest = true;       // while true, L0 follows the drawn length
    double c = 0.8;               // damper coefficient (N s/m)
};

struct Model {
    std::vector<Node> nodes;
    std::vector<Link> links;
    double gravity = 0.0;         // m/s^2, pulls toward +y (down)

    int massCount() const {
        int n = 0;
        for (const Node& nd : nodes) n += (nd.kind == NodeKind::Mass);
        return n;
    }
    double distance(int a, int b) const {
        return std::hypot(nodes[b].x - nodes[a].x, nodes[b].y - nodes[a].y);
    }
    int addMass(double x, double y, double m = 1.0) {
        if ((int)nodes.size() >= MAX_NODES) return -1;
        Node n; n.kind = NodeKind::Mass; n.x = x; n.y = y; n.m = m;
        nodes.push_back(n);
        return (int)nodes.size() - 1;
    }
    int addGround(double x, double y) {
        if ((int)nodes.size() >= MAX_NODES) return -1;
        Node n; n.kind = NodeKind::Ground; n.x = x; n.y = y;
        nodes.push_back(n);
        return (int)nodes.size() - 1;
    }
    // Returns the new link's index, or -1 if it makes no sense (same node twice, or a duplicate).
    int addLink(LinkKind kind, int a, int b) {
        if (a == b || a < 0 || b < 0 || a >= (int)nodes.size() || b >= (int)nodes.size()) return -1;
        if (nodes[a].kind == NodeKind::Ground && nodes[b].kind == NodeKind::Ground) return -1;  // nothing can move
        for (const Link& l : links)
            if (l.kind == kind && ((l.a == a && l.b == b) || (l.a == b && l.b == a))) return -1;
        Link l; l.kind = kind; l.a = a; l.b = b;
        l.L0 = std::fmax(0.05, distance(a, b));
        links.push_back(l);
        return (int)links.size() - 1;
    }
    void removeLink(int i) { if (i >= 0 && i < (int)links.size()) links.erase(links.begin() + i); }
    void removeNode(int i) {
        if (i < 0 || i >= (int)nodes.size()) return;
        for (int j = (int)links.size() - 1; j >= 0; --j)             // links touching it die with it
            if (links[j].a == i || links[j].b == i) links.erase(links.begin() + j);
        for (Link& l : links) { if (l.a > i) --l.a; if (l.b > i) --l.b; }   // later nodes shift down by one
        nodes.erase(nodes.begin() + i);
    }
    // Springs that have never been given a rest length by hand follow their drawn length.
    void refreshAutoRest() {
        for (Link& l : links)
            if (l.kind == LinkKind::Spring && l.autoRest) l.L0 = std::fmax(0.05, distance(l.a, l.b));
    }
};

// ------------------------------------------------------------------ the moving state
// q and v are flat arrays:  q[2i] = x_i,  q[2i+1] = y_i   (same for v).
struct State {
    std::vector<double> q, v;
    double t = 0;
};

inline State initialState(const Model& M) {
    State s;
    const int n = (int)M.nodes.size();
    s.q.assign(2 * n, 0.0);
    s.v.assign(2 * n, 0.0);
    for (int i = 0; i < n; ++i) {
        const Node& nd = M.nodes[i];
        s.q[2 * i] = nd.x;
        s.q[2 * i + 1] = nd.y;
        if (nd.kind == NodeKind::Mass) {
            int rail = (int)std::lround(nd.rail);
            s.v[2 * i]     = (rail == RAIL_Y) ? 0.0 : nd.vx0;
            s.v[2 * i + 1] = (rail == RAIL_X) ? 0.0 : nd.vy0;
        }
    }
    return s;
}

// ------------------------------------------------------------------ geometry of one link
struct LinkInfo {
    double len = 0, ux = 1, uy = 0;   // length and unit vector pointing a -> b
    double stretch = 0;               // len - L0              (springs)
    double rate = 0;                  // d(len)/dt             (dampers; also reported for springs)
    double force = 0;                 // tension along the link: > 0 means it pulls its ends together
};

inline LinkInfo linkInfo(const Model& M, const std::vector<double>& q, const std::vector<double>& v, int li) {
    const Link& L = M.links[li];
    LinkInfo r;
    double dx = q[2 * L.b] - q[2 * L.a], dy = q[2 * L.b + 1] - q[2 * L.a + 1];
    r.len = std::hypot(dx, dy);
    if (r.len > 1e-9) { r.ux = dx / r.len; r.uy = dy / r.len; }          // guard: two nodes on top of each other
    double dvx = v[2 * L.b] - v[2 * L.a], dvy = v[2 * L.b + 1] - v[2 * L.a + 1];
    r.rate = r.ux * dvx + r.uy * dvy;                                      // component of relative velocity along the link
    if (L.kind == LinkKind::Spring) {
        r.stretch = r.len - L.L0;
        r.force = L.k * r.stretch + L.k3 * r.stretch * r.stretch * r.stretch;   // dV/ds
    } else {
        r.force = L.c * r.rate;                                                  // dD/d(sdot)
    }
    return r;
}

// ------------------------------------------------------------------ the three energy-like functions
inline double kinetic(const Model& M, const std::vector<double>& v) {
    double T = 0;
    for (size_t i = 0; i < M.nodes.size(); ++i)
        if (M.nodes[i].kind == NodeKind::Mass)
            T += 0.5 * M.nodes[i].m * (v[2 * i] * v[2 * i] + v[2 * i + 1] * v[2 * i + 1]);
    return T;
}

inline double potential(const Model& M, const std::vector<double>& q) {
    double V = 0;
    std::vector<double> zero(q.size(), 0.0);
    for (size_t li = 0; li < M.links.size(); ++li) {
        if (M.links[li].kind != LinkKind::Spring) continue;
        LinkInfo in = linkInfo(M, q, zero, (int)li);
        const Link& L = M.links[li];
        V += 0.5 * L.k * in.stretch * in.stretch + 0.25 * L.k3 * in.stretch * in.stretch * in.stretch * in.stretch;
    }
    for (size_t i = 0; i < M.nodes.size(); ++i)
        if (M.nodes[i].kind == NodeKind::Mass)
            V -= M.nodes[i].m * M.gravity * (q[2 * i + 1] - M.nodes[i].y);   // y is down, so going down lowers V
    return V;
}

inline double rayleigh(const Model& M, const std::vector<double>& q, const std::vector<double>& v) {
    double D = 0;
    for (size_t li = 0; li < M.links.size(); ++li) {
        if (M.links[li].kind != LinkKind::Damper) continue;
        LinkInfo in = linkInfo(M, q, v, (int)li);
        D += 0.5 * M.links[li].c * in.rate * in.rate;
    }
    return D;
}

// ------------------------------------------------------------------ outside forces (generalized forces Q)
inline void driveForce(const Node& n, double t, double& fx, double& fy) {
    double f = n.driveF * std::cos(n.driveW * t);
    double a = n.driveAngle * PI / 180.0;
    fx = f * std::cos(a);
    fy = f * std::sin(a);
}

// Power fed in by drive + mouse forces:  P = sum Q . v
inline double inputPower(const Model& M, double t, const std::vector<double>& v, const std::vector<double>& ext) {
    double P = 0;
    for (size_t i = 0; i < M.nodes.size(); ++i) {
        if (M.nodes[i].kind != NodeKind::Mass) continue;
        double fx, fy;
        driveForce(M.nodes[i], t, fx, fy);
        if (2 * i + 1 < ext.size()) { fx += ext[2 * i]; fy += ext[2 * i + 1]; }
        P += fx * v[2 * i] + fy * v[2 * i + 1];
    }
    return P;
}

// ------------------------------------------------------------------ Euler-Lagrange equations -> accelerations
inline void accel(const Model& M, double t, const std::vector<double>& q, const std::vector<double>& v,
                  const std::vector<double>& ext, std::vector<double>& a) {
    const size_t n = M.nodes.size();
    std::vector<double> F(2 * n, 0.0);                        // total force on every node

    for (size_t li = 0; li < M.links.size(); ++li) {          // springs and dampers
        const Link& L = M.links[li];
        LinkInfo in = linkInfo(M, q, v, (int)li);             // in.force = dV/ds (spring) or dD/dsdot (damper)
        // s depends on q_b with slope +u and on q_a with slope -u, so:  -dV/dq_b = -f u,  -dV/dq_a = +f u
        F[2 * L.a]     += in.force * in.ux;
        F[2 * L.a + 1] += in.force * in.uy;
        F[2 * L.b]     -= in.force * in.ux;
        F[2 * L.b + 1] -= in.force * in.uy;
    }
    a.assign(2 * n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        const Node& nd = M.nodes[i];
        if (nd.kind != NodeKind::Mass) continue;              // ground never accelerates
        double fx = F[2 * i], fy = F[2 * i + 1];
        fy += nd.m * M.gravity;                               // -dV/dy of the gravity term
        double dx, dy;
        driveForce(nd, t, dx, dy);
        fx += dx; fy += dy;
        if (2 * i + 1 < ext.size()) { fx += ext[2 * i]; fy += ext[2 * i + 1]; }
        double ax = fx / nd.m, ay = fy / nd.m;
        int rail = (int)std::lround(nd.rail);
        if (rail == RAIL_X) ay = 0;                           // a rail removes one degree of freedom
        if (rail == RAIL_Y) ax = 0;
        a[2 * i] = ax; a[2 * i + 1] = ay;
    }
}

// ------------------------------------------------------------------ one RK4 step
inline void rk4(const Model& M, State& s, const std::vector<double>& ext, double h) {
    const size_t N = s.q.size();
    std::vector<double> a1, a2, a3, a4, qt(N), vt(N), v2, v3, v4;
    accel(M, s.t, s.q, s.v, ext, a1);
    for (size_t i = 0; i < N; ++i) { qt[i] = s.q[i] + 0.5 * h * s.v[i]; vt[i] = s.v[i] + 0.5 * h * a1[i]; }
    v2 = vt;
    accel(M, s.t + 0.5 * h, qt, vt, ext, a2);
    for (size_t i = 0; i < N; ++i) { qt[i] = s.q[i] + 0.5 * h * v2[i]; vt[i] = s.v[i] + 0.5 * h * a2[i]; }
    v3 = vt;
    accel(M, s.t + 0.5 * h, qt, vt, ext, a3);
    for (size_t i = 0; i < N; ++i) { qt[i] = s.q[i] + h * v3[i]; vt[i] = s.v[i] + h * a3[i]; }
    v4 = vt;
    accel(M, s.t + h, qt, vt, ext, a4);
    for (size_t i = 0; i < N; ++i) {
        s.q[i] += h / 6.0 * (s.v[i] + 2 * v2[i] + 2 * v3[i] + v4[i]);
        s.v[i] += h / 6.0 * (a1[i] + 2 * a2[i] + 2 * a3[i] + a4[i]);
    }
    s.t += h;
}

// ------------------------------------------------------------------ energy book-keeping (the "meter")
// Law of energy:   E(t) = E(0) + work_in(t) - dissipated(t).   residual() should stay near 0.
struct Meter {
    double T = 0, V = 0, E = 0, dissipated = 0, workIn = 0, E0 = 0;
    double residual() const { return E - (E0 + workIn - dissipated); }
    void measure(const Model& M, const State& s) { T = kinetic(M, s.v); V = potential(M, s.q); E = T + V; }
    void reset(const Model& M, const State& s) { measure(M, s); dissipated = workIn = 0; E0 = E; }
    // After a live edit (a slider moved) energy jumps for non-physical reasons: restart the books.
    void rebase(const Model& M, const State& s) { measure(M, s); E0 = E - (workIn - dissipated); }
};

// Step the simulation AND keep the meter up to date (trapezoid rule for the two power integrals).
inline void advance(const Model& M, State& s, const std::vector<double>& ext, double h, Meter& meter) {
    double p0 = inputPower(M, s.t, s.v, ext), d0 = 2.0 * rayleigh(M, s.q, s.v);
    rk4(M, s, ext, h);
    double p1 = inputPower(M, s.t, s.v, ext), d1 = 2.0 * rayleigh(M, s.q, s.v);
    meter.workIn     += 0.5 * (p0 + p1) * h;
    meter.dissipated += 0.5 * (d0 + d1) * h;
    meter.measure(M, s);
}

// ------------------------------------------------------------------ frequency finder (telemetry)
// Slow discrete Fourier transform, good enough for a few hundred samples. Returns Hz, or 0 if no clear tone.
inline double dominantFrequency(const std::vector<double>& x, double sampleRate) {
    const size_t N = x.size();
    if (N < 32) return 0;
    double mean = 0;
    for (double d : x) mean += d;
    mean /= (double)N;
    double best = 0, bestF = 0, total = 0;
    int count = 0;
    for (double f = 0.05; f <= std::fmin(30.0, sampleRate / 2.2); f += 0.025, ++count) {
        double re = 0, im = 0;
        for (size_t i = 0; i < N; ++i) {
            double w = 0.5 - 0.5 * std::cos(2 * PI * (double)i / (double)(N - 1));   // Hann window
            double ang = 2 * PI * f * (double)i / sampleRate;
            re += (x[i] - mean) * w * std::cos(ang);
            im -= (x[i] - mean) * w * std::sin(ang);
        }
        double mag = std::hypot(re, im);
        total += mag;
        if (mag > best) { best = mag; bestF = f; }
    }
    // a real tone sticks out: its peak must be well above the average of all frequencies tried
    return (best > 1e-6 && count > 0 && best > 4.0 * total / count) ? bestF : 0;
}

}  // namespace lag
