#include "vlab/world.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace vlab {

static const double kSlop = 0.0005;          // allowed overlap before position correction (m)
static const double kBaumgarte = 0.2;        // fraction of overlap removed per correction pass

// Inverse mass per axis: a body on a rail has zero inverse mass along the locked axis, so the solver can never move it that way.
static inline Vec2 invMv(const Body& b) { return Vec2(b.lockX ? 0.0 : b.invMass, b.lockY ? 0.0 : b.invMass); }
static inline double kDir(const Body& b, Vec2 r, Vec2 d) {   // effective inverse mass of body b along direction d at lever arm r
    Vec2 im = invMv(b);
    double rn = cross(r, d);
    return d.x * d.x * im.x + d.y * d.y * im.y + b.invI * rn * rn;
}

// ====================================================================== construction / reset
World::World(const Model& m) : model_(m) {
    model_.updateMass();
    rebuildExprs();
    reset();
}

void World::reset() {
    model_.updateMass();
    model_.refreshAutoRest();
    const size_t n = model_.bodies.size();
    S_.assign(n, BodyState());
    for (size_t i = 0; i < n; ++i) {
        const Body& b = model_.bodies[i];
        BodyState& s = S_[i];
        s.pos = b.pos;
        s.angle = b.angle;
        s.vel = b.isDynamic() ? b.vel : Vec2();
        s.omega = b.isDynamic() ? b.omega : 0.0;
        if (b.lockX) s.vel.x = 0;
        if (b.lockY) s.vel.y = 0;
        if (b.lockRot) s.omega = 0;
        s.rot = Rot(s.angle);
    }
    wv_.assign(n, {});
    wn_.assign(n, {});
    F_.assign(n, Vec2());
    tau_.assign(n, 0.0);
    contacts_.clear();
    contactInfo_.clear();
    warm_.clear();
    ledger_ = Ledger();
    mouse_.active = false;
    t_ = 0;
    steps_ = 0;
    healthy_ = true;
    measure();
    ledger_.E0 = ledger_.E;
}

bool World::usesConstraintSolver() const {
    for (const Link& l : model_.links)
        if (l.kind == LinkKind::Rod || l.kind == LinkKind::Pin || l.kind == LinkKind::Weld || l.kind == LinkKind::Roller) return true;
    if (!model_.env.collisions) return false;
    int dyn = 0, col = 0;
    for (const Body& b : model_.bodies) {
        if (b.collides()) ++col;
        if (b.isDynamic() && b.collides()) ++dyn;
    }
    return dyn >= 1 && col >= 2;
}

void World::setMouse(int body, Vec2 local, Vec2 target) {
    mouse_.active = body >= 0 && body < (int)model_.bodies.size() && model_.bodies[body].isDynamic();
    mouse_.body = body;
    mouse_.local = local;
    mouse_.target = target;
}

// ====================================================================== smooth forces (the Lagrangian part)
void World::computeForces(const std::vector<BodyState>& S, double t, std::vector<Vec2>& F, std::vector<double>& tau,
                          double* pViscous, double* pDrag, double* pIn) const {
    const Environment& env = model_.env;
    const size_t n = model_.bodies.size();
    F.assign(n, Vec2());
    tau.assign(n, 0.0);
    double pv = 0, pd = 0, pin = 0;

    for (size_t i = 0; i < n; ++i) {
        const Body& b = model_.bodies[i];
        if (!b.isDynamic()) continue;
        const BodyState& s = S[i];
        F[i].y += b.massKg * env.gravity;                                        // -dV/dy of the gravity term
        {
            Vec2 d = externalForce((int)i, t);                                   // harmonic push, pulse and constant load
            if (d.x != 0 || d.y != 0) { F[i] += d; pin += dot(d, s.vel); }
            Vec2 fd;
            double td = 0;
            distributedForce(s, (int)i, t, fd, td);                              // distributed load: its resultant force and torque
            if (fd.x != 0 || fd.y != 0 || td != 0) { F[i] += fd; tau[i] += td; pin += dot(fd, s.vel) + td * s.omega; }
        }
        if (env.airDrag) {                                                       // F = -(c1 + c2 |v|) v
            double sp = length(s.vel), c = env.drag1 + env.drag2 * sp;
            F[i] += s.vel * (-c);
            pd += c * sp * sp;
        }
    }

    for (const Link& l : model_.links) {
        if (l.kind != LinkKind::Spring && l.kind != LinkKind::Damper) continue;
        const BodyState &A = S[l.a], &B = S[l.b];
        Vec2 ra = A.rot.apply(l.la), rb = B.rot.apply(l.lb);
        Vec2 d = (B.pos + rb) - (A.pos + ra);
        double len = length(d);
        Vec2 u = len > 1e-9 ? d * (1.0 / len) : Vec2(1, 0);
        double f;
        if (l.kind == LinkKind::Spring) {
            double s = len - l.L0;
            f = l.k * s + l.k3 * s * s * s;                                      // dV/ds
        } else {
            if (!env.damping) continue;
            Vec2 va = A.vel + cross(A.omega, ra), vb = B.vel + cross(B.omega, rb);
            double rate = dot(u, vb - va);                                       // ds/dt
            f = l.c * rate;                                                      // dD/d(sdot)
            pv += l.c * rate * rate;                                             // heat power 2D
        }
        Vec2 Fa = u * f;                                                         // force on a: +f u, on b: -f u
        if (model_.bodies[l.a].isDynamic()) { F[l.a] += Fa; tau[l.a] += cross(ra, Fa); }
        if (model_.bodies[l.b].isDynamic()) { F[l.b] -= Fa; tau[l.b] += cross(rb, -Fa); }
    }

    for (const Link& l : model_.links) {                                         // hinge motors and rotary damping
        if (l.kind != LinkKind::Pin) continue;
        const BodyState &A = S[l.a], &B = S[l.b];
        const bool da = model_.bodies[l.a].isDynamic(), db = model_.bodies[l.b].isDynamic();
        double T = l.motorTorque + l.motorAmp * std::cos(l.motorW * t);
        double wrel = (db ? B.omega : 0.0) - (da ? A.omega : 0.0);
        double Tc = 0;
        if (l.jointC > 0 && env.damping) { Tc = -l.jointC * wrel; pv += l.jointC * wrel * wrel; }
        if (db) tau[l.b] += T + Tc;
        if (da) tau[l.a] -= T + Tc;
        pin += T * wrel;
    }

    if (mouse_.active && mouse_.body >= 0 && mouse_.body < (int)n) {
        const Body& b = model_.bodies[mouse_.body];
        const BodyState& s = S[mouse_.body];
        Vec2 r = s.rot.apply(mouse_.local), pw = s.pos + r, vp = s.vel + cross(s.omega, r);
        Vec2 d = mouse_.target - pw;
        double dl = length(d);
        if (dl > 3.0) d = d * (3.0 / dl);                                        // never pull from further than 3 m
        Vec2 Fm = (d * 700.0 - vp * 45.0) * b.massKg;                            // critically-damped rubber band
        F[mouse_.body] += Fm;
        tau[mouse_.body] += cross(r, Fm);
        pin += dot(Fm, vp);
    }
    if (pViscous) *pViscous = pv;
    if (pDrag) *pDrag = pd;
    if (pIn) *pIn = pin;
}

double World::potentialEnergy(const std::vector<BodyState>& S) const {
    double V = 0;
    for (const Link& l : model_.links) {
        if (l.kind != LinkKind::Spring) continue;
        const BodyState &A = S[l.a], &B = S[l.b];
        double len = length((B.pos + B.rot.apply(l.lb)) - (A.pos + A.rot.apply(l.la)));
        double s = len - l.L0;
        V += 0.5 * l.k * s * s + 0.25 * l.k3 * s * s * s * s;
    }
    for (size_t i = 0; i < model_.bodies.size(); ++i) {
        const Body& b = model_.bodies[i];
        if (b.isDynamic()) V -= b.massKg * model_.env.gravity * (S[i].pos.y - b.pos.y);   // y is down: lower = less energy
    }
    return V;
}

void World::measure() {
    double T = 0;
    for (size_t i = 0; i < model_.bodies.size(); ++i) {
        const Body& b = model_.bodies[i];
        if (b.isDynamic()) T += 0.5 * b.massKg * lengthSq(S_[i].vel) + 0.5 * b.inertia * S_[i].omega * S_[i].omega * (b.lockRot ? 0 : 1);
    }
    ledger_.T = T;
    ledger_.V = potentialEnergy(S_);
    ledger_.E = ledger_.T + ledger_.V;
}

void World::rebaseLedger() {
    rebuildExprs();
    measure();
    ledger_.E0 = ledger_.E - (ledger_.workIn - ledger_.dissipated());
}

void World::rebuildExprs() {
    dexpr_.assign(model_.bodies.size(), nullptr);
    for (size_t i = 0; i < model_.bodies.size(); ++i)
        if (model_.bodies[i].dloadType == DL_Equation) dexpr_[i] = Expr::parse(model_.bodies[i].dloadExpr);
}

Vec2 World::distributedDir(int i) const {
    double a = model_.bodies[i].dloadAngleDeg * kPi / 180.0;
    return Vec2(std::cos(a), std::sin(a));
}

double World::distributedAt(int i, double x, double t) const {
    const Body& b = model_.bodies[i];
    return distributedLoadAt(b, i < (int)dexpr_.size() ? dexpr_[i].get() : nullptr, x, t);
}

void World::distributedForce(const BodyState& s, int i, double t, Vec2& F, double& tau) const {
    F = Vec2();
    tau = 0;
    const Body& b = model_.bodies[i];
    if (b.dloadType == DL_None || b.shape.preset != ShapePreset::Box || !b.isDynamic()) return;
    double a, z;
    distributedLoadSpan(b, &a, &z);
    if (z - a < 1e-9) return;
    const int N = 48;
    const double dx = (z - a) / N, L = b.shape.w;
    const Vec2 dir = distributedDir(i), ex = s.rot.apply(Vec2(1, 0));
    const Expr* e = i < (int)dexpr_.size() ? dexpr_[i].get() : nullptr;
    for (int k = 0; k < N; ++k) {
        double xm = a + (k + 0.5) * dx;
        double q = distributedLoadAt(b, e, xm, t);
        if (q == 0) continue;
        Vec2 f = dir * (q * dx);
        F += f;
        tau += cross(ex * (xm - L / 2), f);
    }
}

Vec2 World::externalForce(int i, double t) const {
    const Body& b = model_.bodies[i];
    Vec2 d;
    if (b.driveF != 0) {
        double f = b.driveT > 0 ? (t >= 0 && t < b.driveT ? b.driveF * std::sin(kPi * t / b.driveT) : 0.0) : b.driveF * std::cos(b.driveW * t);
        double a = b.driveAngleDeg * kPi / 180.0;
        d += Vec2(f * std::cos(a), f * std::sin(a));
    }
    if (b.loadN != 0) {
        double a = (b.loadAngleDeg + (b.loadPull ? 180.0 : 0.0)) * kPi / 180.0;
        d += Vec2(b.loadN * std::cos(a), b.loadN * std::sin(a));
    }
    return d;
}

LinkReading World::link(int i) const {
    LinkReading r;
    if (i < 0 || i >= (int)model_.links.size()) return r;
    const Link& l = model_.links[i];
    const BodyState &A = S_[l.a], &B = S_[l.b];
    Vec2 ra = A.rot.apply(l.la), rb = B.rot.apply(l.lb);
    Vec2 d = (B.pos + rb) - (A.pos + ra);
    r.length = length(d);
    Vec2 u = r.length > 1e-9 ? d * (1.0 / r.length) : Vec2(1, 0);
    r.rate = dot(u, (B.vel + cross(B.omega, rb)) - (A.vel + cross(A.omega, ra)));
    if (l.kind == LinkKind::Spring) {
        r.stretch = r.length - l.L0;
        r.force = l.k * r.stretch + l.k3 * r.stretch * r.stretch * r.stretch;
    } else if (l.kind == LinkKind::Damper) {
        r.force = model_.env.damping ? l.c * r.rate : 0.0;
        r.power = r.force * r.rate;
    } else if (l.kind == LinkKind::Rod) {
        r.stretch = r.length - l.L0;
    }
    if (l.kind != LinkKind::Spring && l.kind != LinkKind::Damper && i < (int)jointImp_.size()) {
        const double h = model_.env.dt;
        r.rx = jointImp_[i].x / h;
        r.ry = jointImp_[i].y / h;
        r.moment = i < (int)jointAng_.size() ? jointAng_[i] / h : 0.0;
        r.force = length(Vec2(r.rx, r.ry));
    }
    return r;
}

// ====================================================================== RK4 for smooth scenes
void World::stepSmooth(double h) {
    const size_t n = model_.bodies.size();
    struct K { Vec2 dp; double da = 0; Vec2 dv; double dw = 0; };
    std::vector<K> k1(n), k2(n), k3(n), k4(n);
    std::vector<BodyState> T = S_;

    auto eval = [&](const std::vector<BodyState>& s, double t, std::vector<K>& k) {
        computeForces(s, t, F_, tau_, nullptr, nullptr, nullptr);
        for (size_t i = 0; i < n; ++i) {
            const Body& b = model_.bodies[i];
            k[i] = K();
            if (!b.isDynamic()) continue;
            k[i].dp = s[i].vel;
            k[i].da = s[i].omega;
            k[i].dv = Vec2(b.lockX ? 0.0 : F_[i].x * b.invMass, b.lockY ? 0.0 : F_[i].y * b.invMass);
            k[i].dw = b.lockRot ? 0.0 : tau_[i] * b.invI;
        }
    };
    auto trial = [&](const std::vector<K>& k, double f) {
        for (size_t i = 0; i < n; ++i) {
            if (!model_.bodies[i].isDynamic()) continue;
            T[i].pos = S_[i].pos + k[i].dp * f;
            T[i].angle = S_[i].angle + k[i].da * f;
            T[i].vel = S_[i].vel + k[i].dv * f;
            T[i].omega = S_[i].omega + k[i].dw * f;
            T[i].rot = Rot(T[i].angle);
        }
    };
    eval(S_, t_, k1);
    trial(k1, 0.5 * h);
    eval(T, t_ + 0.5 * h, k2);
    trial(k2, 0.5 * h);
    eval(T, t_ + 0.5 * h, k3);
    trial(k3, h);
    eval(T, t_ + h, k4);
    for (size_t i = 0; i < n; ++i) {
        if (!model_.bodies[i].isDynamic()) continue;
        S_[i].pos += (k1[i].dp + k2[i].dp * 2.0 + k3[i].dp * 2.0 + k4[i].dp) * (h / 6.0);
        S_[i].angle += (k1[i].da + 2 * k2[i].da + 2 * k3[i].da + k4[i].da) * (h / 6.0);
        S_[i].vel += (k1[i].dv + k2[i].dv * 2.0 + k3[i].dv * 2.0 + k4[i].dv) * (h / 6.0);
        S_[i].omega += (k1[i].dw + 2 * k2[i].dw + 2 * k3[i].dw + k4[i].dw) * (h / 6.0);
        S_[i].rot = Rot(S_[i].angle);
    }
}

// ====================================================================== collision detection
double World::combine(double a, double b, Combine mode) const {
    switch (mode) {
        case Combine::Geometric: return std::sqrt(std::max(0.0, a) * std::max(0.0, b));
        case Combine::Average: return 0.5 * (a + b);
        case Combine::Min: return std::min(a, b);
        default: return std::max(a, b);
    }
}

namespace {
struct CV { Vec2 v; int id; };

int clipSegment(CV out[2], const CV in[2], Vec2 normal, double offset) {
    int n = 0;
    double d0 = dot(normal, in[0].v) - offset, d1 = dot(normal, in[1].v) - offset;
    if (d0 <= 0) out[n++] = in[0];
    if (d1 <= 0) out[n++] = in[1];
    if (d0 * d1 < 0 && n < 2) {
        double t = d0 / (d0 - d1);
        out[n].v = in[0].v + (in[1].v - in[0].v) * t;
        out[n].id = in[0].id;
        ++n;
    }
    return n;
}

double maxSeparation(const std::vector<Vec2>& vA, const std::vector<Vec2>& nA, const std::vector<Vec2>& vB, int* edge) {
    double best = -1e300;
    int bi = 0;
    for (size_t i = 0; i < vA.size(); ++i) {
        double s = 1e300;
        for (size_t j = 0; j < vB.size(); ++j) s = std::min(s, dot(nA[i], vB[j] - vA[i]));
        if (s > best) { best = s; bi = (int)i; }
    }
    *edge = bi;
    return best;
}
}  // namespace

bool World::collide(int ia, int ib, Manifold& m) const {
    const Body& A = model_.bodies[ia];
    const Body& B = model_.bodies[ib];
    const BodyState &SA = S_[ia], &SB = S_[ib];
    m = Manifold();

    if (A.shape.isCircle() && B.shape.isCircle()) {
        Vec2 d = SB.pos - SA.pos;
        double dist = length(d), rs = A.shape.radius + B.shape.radius;
        if (dist > rs) return false;
        Vec2 n = dist > 1e-9 ? d * (1.0 / dist) : Vec2(1, 0);
        m.count = 1;
        m.n = n;
        m.sep[0] = dist - rs;
        m.pA[0] = SA.pos + n * A.shape.radius;
        m.pB[0] = SB.pos - n * B.shape.radius;
        return true;
    }

    if (A.shape.isCircle() != B.shape.isCircle()) {
        const bool circleIsA = A.shape.isCircle();
        const int ip = circleIsA ? ib : ia;                       // polygon index
        const Body& C = circleIsA ? A : B;
        const Vec2 c = (circleIsA ? SA : SB).pos;
        const double r = C.shape.radius;
        const std::vector<Vec2>& v = wv_[ip];
        const std::vector<Vec2>& nn = wn_[ip];
        const size_t cnt = v.size();
        int fi = 0;
        double s = -1e300;
        for (size_t i = 0; i < cnt; ++i) {
            double si = dot(nn[i], c - v[i]);
            if (si > s) { s = si; fi = (int)i; }
        }
        if (s > r) return false;
        Vec2 n, pPoly, pCirc;                                      // n points from polygon toward circle
        double sep;
        Vec2 v1 = v[fi], v2 = v[(fi + 1) % cnt];
        double u1 = dot(c - v1, v2 - v1), u2 = dot(c - v2, v1 - v2);
        if (s < 0) {                                               // centre is inside the polygon
            n = nn[fi];
            pPoly = c - n * s;
            pCirc = c - n * r;
            sep = s - r;
        } else if (u1 <= 0) {
            Vec2 d = c - v1;
            double dist = length(d);
            if (dist > r) return false;
            n = dist > 1e-9 ? d * (1.0 / dist) : nn[fi];
            pPoly = v1;
            pCirc = c - n * r;
            sep = dist - r;
        } else if (u2 <= 0) {
            Vec2 d = c - v2;
            double dist = length(d);
            if (dist > r) return false;
            n = dist > 1e-9 ? d * (1.0 / dist) : nn[fi];
            pPoly = v2;
            pCirc = c - n * r;
            sep = dist - r;
        } else {
            n = nn[fi];
            pPoly = c - n * s;
            pCirc = c - n * r;
            sep = s - r;
        }
        m.count = 1;
        m.sep[0] = sep;
        m.id[0] = fi;
        if (circleIsA) { m.n = -n; m.pA[0] = pCirc; m.pB[0] = pPoly; }
        else { m.n = n; m.pA[0] = pPoly; m.pB[0] = pCirc; }
        return true;
    }

    // polygon vs polygon: separating axis test, then clip the incident edge against the reference face
    const std::vector<Vec2> &vA = wv_[ia], &nA = wn_[ia], &vB = wv_[ib], &nB = wn_[ib];
    int eA, eB;
    double sA = maxSeparation(vA, nA, vB, &eA);
    if (sA > 0) return false;
    double sB = maxSeparation(vB, nB, vA, &eB);
    if (sB > 0) return false;
    const bool refIsA = !(sB > sA + 0.0005);
    const std::vector<Vec2> &vR = refIsA ? vA : vB, &nR = refIsA ? nA : nB, &vI = refIsA ? vB : vA, &nI = refIsA ? nB : nA;
    const int e = refIsA ? eA : eB;
    const Vec2 normal = nR[e];
    int inc = 0;
    double minDot = 1e300;
    for (size_t j = 0; j < nI.size(); ++j) {
        double d = dot(normal, nI[j]);
        if (d < minDot) { minDot = d; inc = (int)j; }
    }
    CV in[2] = {{vI[inc], inc * 2}, {vI[(inc + 1) % vI.size()], inc * 2 + 1}};
    Vec2 v11 = vR[e], v12 = vR[(e + 1) % vR.size()];
    Vec2 tangent = normalized(v12 - v11);
    CV c1[2], c2[2];
    if (clipSegment(c1, in, -tangent, -dot(tangent, v11)) < 2) return false;
    if (clipSegment(c2, c1, tangent, dot(tangent, v12)) < 2) return false;
    for (int k = 0; k < 2; ++k) {
        double sep = dot(normal, c2[k].v) - dot(normal, v11);
        if (sep > 0.001) continue;
        Vec2 pInc = c2[k].v, pRef = pInc - normal * sep;
        int idx = m.count++;
        m.sep[idx] = sep;
        m.id[idx] = (e << 8) | c2[k].id | (refIsA ? 0 : 0x10000);
        if (refIsA) { m.n = normal; m.pA[idx] = pRef; m.pB[idx] = pInc; }
        else { m.n = -normal; m.pA[idx] = pInc; m.pB[idx] = pRef; }
    }
    return m.count > 0;
}

void World::buildContacts() {
    const size_t n = model_.bodies.size();
    for (size_t i = 0; i < n; ++i) {                             // world-space polygons for this step
        const Body& b = model_.bodies[i];
        wv_[i].clear();
        wn_[i].clear();
        if (b.shape.isCircle()) continue;
        for (const Vec2& v : b.shape.verts) wv_[i].push_back(S_[i].pos + S_[i].rot.apply(v));
        for (const Vec2& nn : b.shape.normals) wn_[i].push_back(S_[i].rot.apply(nn));
    }
    std::set<std::pair<int, int>> excluded;
    for (const Link& l : model_.links)
        if (l.kind != LinkKind::Spring && l.kind != LinkKind::Damper && !l.collideConnected)
            excluded.insert({std::min(l.a, l.b), std::max(l.a, l.b)});

    std::vector<Contact> fresh;
    std::vector<double> bound(n);
    for (size_t i = 0; i < n; ++i) bound[i] = model_.bodies[i].shape.boundingRadius();
    for (size_t i = 0; i < n; ++i) {
        const Body& A = model_.bodies[i];
        if (!A.collides()) continue;
        for (size_t j = i + 1; j < n; ++j) {
            const Body& B = model_.bodies[j];
            if (!B.collides() || (!A.isDynamic() && !B.isDynamic())) continue;
            if (excluded.count({(int)i, (int)j})) continue;
            if (length(S_[j].pos - S_[i].pos) > bound[i] + bound[j] + 0.01) continue;
            Manifold m;
            if (!collide((int)i, (int)j, m)) continue;
            for (int k = 0; k < m.count; ++k) {
                Contact c;
                c.a = (int)i;
                c.b = (int)j;
                c.n = m.n;
                c.pA = m.pA[k];
                c.pB = m.pB[k];
                c.sep = m.sep[k];
                c.la = S_[i].rot.applyInv(c.pA - S_[i].pos);
                c.lb = S_[j].rot.applyInv(c.pB - S_[j].pos);
                c.key = ((uint64_t)i << 44) | ((uint64_t)j << 24) | (uint64_t)(m.id[k] & 0xFFFFFF);
                fresh.push_back(c);
            }
        }
    }
    contacts_.swap(fresh);
}

// ====================================================================== impulses
void World::applyImpulse(Contact& c, Vec2 P, double* dKEn, double* dKEt) {
    const Body &A = model_.bodies[c.a], &B = model_.bodies[c.b];
    BodyState &sa = S_[c.a], &sb = S_[c.b];
    Vec2 v0 = (sb.vel + cross(sb.omega, c.rb)) - (sa.vel + cross(sa.omega, c.ra));
    Vec2 imA = invMv(A), imB = invMv(B);
    sa.vel -= Vec2(P.x * imA.x, P.y * imA.y);
    sa.omega -= A.invI * cross(c.ra, P);
    sb.vel += Vec2(P.x * imB.x, P.y * imB.y);
    sb.omega += B.invI * cross(c.rb, P);
    Vec2 v1 = (sb.vel + cross(sb.omega, c.rb)) - (sa.vel + cross(sa.omega, c.ra));
    Vec2 avg = (v0 + v1) * 0.5;                                   // exact: dKE = P . (v0 + v1) / 2
    *dKEn = dot(P, c.n) * dot(avg, c.n);
    *dKEt = dot(P, c.t) * dot(avg, c.t);
    stepDKE_ += *dKEn + *dKEt;                                    // exact kinetic-energy change of this impulse
}

void World::warmStartAndPrestep(double h) {
    (void)h;
    const Environment& env = model_.env;
    for (Contact& c : contacts_) {
        const Body &A = model_.bodies[c.a], &B = model_.bodies[c.b];
        const BodyState &sa = S_[c.a], &sb = S_[c.b];
        Vec2 p = (c.pA + c.pB) * 0.5;
        c.ra = p - sa.pos;
        c.rb = p - sb.pos;
        c.t = Vec2(c.n.y, -c.n.x);
        auto kEff = [&](Vec2 dir) { return kDir(A, c.ra, dir) + kDir(B, c.rb, dir); };
        double kN = kEff(c.n), kT = kEff(c.t), kR = A.invI + B.invI;
        c.massN = kN > 0 ? 1.0 / kN : 0.0;
        c.massT = kT > 0 ? 1.0 / kT : 0.0;
        c.massR = kR > 0 ? 1.0 / kR : 0.0;
        Vec2 vrel = (sb.vel + cross(sb.omega, c.rb)) - (sa.vel + cross(sa.omega, c.ra));
        double vn = dot(vrel, c.n);
        c.vt0 = dot(vrel, c.t);
        c.wrel0 = sb.omega - sa.omega;
        double e = env.inelastic ? combine(A.mat.restitution, B.mat.restitution, env.eCombine) : 1.0;
        c.impact = vn < -env.restThreshold;
        c.bias = (c.impact && e > 0) ? -e * vn : 0.0;
        if (env.friction) {
            c.muS = combine(A.mat.muS, B.mat.muS, env.muCombine);
            c.muK = combine(A.mat.muK, B.mat.muK, env.muCombine);
        } else {
            c.muS = c.muK = 0;
        }
        c.rollLimit = 0;
        if (env.rolling && (A.shape.isCircle() || B.shape.isCircle())) {
            double R = (A.shape.isCircle() && B.shape.isCircle()) ? std::min(A.shape.radius, B.shape.radius)
                       : (A.shape.isCircle() ? A.shape.radius : B.shape.radius);
            c.rollLimit = std::max(A.mat.rollRes, B.mat.rollRes) * R;
        }
        // warm start: re-apply last step's impulses (their energy effect is accounted for exactly)
        auto it = warm_.find(c.key);
        if (it != warm_.end()) {
            c.Pn = it->second.Pn;
            c.Pt = it->second.Pt;
            c.Pr = it->second.Pr;
            double dn, dt;
            applyImpulse(c, c.n * c.Pn + c.t * c.Pt, &dn, &dt);
            if (c.Pr != 0) {                                      // the rolling-resistance impulse is warm-started too
                BodyState &wa = S_[c.a], &wb = S_[c.b];
                double w0 = wb.omega - wa.omega;
                wa.omega -= A.invI * c.Pr;
                wb.omega += B.invI * c.Pr;
                stepDKE_ += c.Pr * (w0 + (wb.omega - wa.omega)) * 0.5;
            }
        }
    }
}

void World::solveContacts() {
    for (Contact& c : contacts_) {
        BodyState &sa = S_[c.a], &sb = S_[c.b];
        const Body &A = model_.bodies[c.a], &B = model_.bodies[c.b];
        double dn, dt;
        // friction first (bounded by the normal impulse of the previous pass)
        {
            Vec2 vrel = (sb.vel + cross(sb.omega, c.rb)) - (sa.vel + cross(sa.omega, c.ra));
            double vt = dot(vrel, c.t), lambda = -c.massT * vt;
            double mu = std::fabs(c.vt0) < 0.02 ? c.muS : c.muK;      // sticking: static bound, sliding: kinetic
            double maxP = mu * c.Pn, want = c.Pt + lambda, np = clampd(want, -maxP, maxP);
            bool slip = std::fabs(want) > maxP + 1e-12;                 // the Coulomb limit is active: the surfaces are sliding
            lambda = np - c.Pt;
            c.Pt = np;
            c.slip = slip;
            if (lambda != 0) {
                applyImpulse(c, c.t * lambda, &dn, &dt);
            }
        }
        // normal (with restitution target `bias`)
        {
            Vec2 vrel = (sb.vel + cross(sb.omega, c.rb)) - (sa.vel + cross(sa.omega, c.ra));
            double vn = dot(vrel, c.n), lambda = -c.massN * (vn - c.bias);
            double np = std::max(c.Pn + lambda, 0.0);
            lambda = np - c.Pn;
            c.Pn = np;
            if (lambda != 0) {
                applyImpulse(c, c.n * lambda, &dn, &dt);
            }
        }
        // rolling resistance: an angular impulse that opposes relative spin, limited by rollRes * R * Pn
        if (c.rollLimit > 0 && c.massR > 0) {
            double wrel = sb.omega - sa.omega, lambda = -c.massR * wrel;
            double maxR = c.rollLimit * c.Pn, want = c.Pr + lambda, np = clampd(want, -maxR, maxR);
            bool slip = std::fabs(want) > maxR + 1e-12;
            lambda = np - c.Pr;
            c.Pr = np;
            if (lambda != 0) {
                double w1 = wrel + lambda * (A.invI + B.invI);
                sa.omega -= A.invI * lambda;
                sb.omega += B.invI * lambda;
                stepDKE_ += lambda * (wrel + w1) * 0.5;
            }
        }
    }
}

void World::solveJoints() {
    for (size_t li = 0; li < model_.links.size(); ++li) {
        const Link& l = model_.links[li];
        if (l.kind == LinkKind::Spring || l.kind == LinkKind::Damper) continue;
        const Body &A = model_.bodies[l.a], &B = model_.bodies[l.b];
        BodyState &sa = S_[l.a], &sb = S_[l.b];
        Vec2 imA = invMv(A), imB = invMv(B);
        double iA = A.invI, iB = B.invI;
        if (l.kind == LinkKind::Weld) {                                         // hold the relative angle first
            double K = iA + iB;
            if (K > 1e-18) {
                double lam = -(sb.omega - sa.omega) / K;
                sa.omega -= iA * lam;
                sb.omega += iB * lam;
                jointAng_[li] += lam;
            }
        }
        Vec2 ra = sa.rot.apply(l.la), rb = sb.rot.apply(l.lb);
        Vec2 vrel = (sb.vel + cross(sb.omega, rb)) - (sa.vel + cross(sa.omega, ra));
        Vec2 P;
        if (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld) {
            double k11 = imA.x + imB.x + iA * ra.y * ra.y + iB * rb.y * rb.y;
            double k12 = -iA * ra.x * ra.y - iB * rb.x * rb.y;
            double k22 = imA.y + imB.y + iA * ra.x * ra.x + iB * rb.x * rb.x;
            double det = k11 * k22 - k12 * k12;
            if (std::fabs(det) < 1e-18) continue;
            P = Vec2(-(k22 * vrel.x - k12 * vrel.y) / det, -(-k12 * vrel.x + k11 * vrel.y) / det);
        } else if (l.kind == LinkKind::Roller) {                                // B's point stays on the line through A's point along A's x axis
            Vec2 nrm = sa.rot.apply(Vec2(0, 1));
            double K = kDir(A, ra, nrm) + kDir(B, rb, nrm);
            if (K < 1e-18) continue;
            P = nrm * (-dot(vrel, nrm) / K);
        } else {
            Vec2 u = normalized((sb.pos + rb) - (sa.pos + ra));
            double K = kDir(A, ra, u) + kDir(B, rb, u);
            if (K < 1e-18) continue;
            P = u * (-dot(vrel, u) / K);
        }
        sa.vel -= Vec2(P.x * imA.x, P.y * imA.y);
        sa.omega -= iA * cross(ra, P);
        sb.vel += Vec2(P.x * imB.x, P.y * imB.y);
        sb.omega += iB * cross(rb, P);
        jointImp_[li] += P;
    }
}

void World::correctPositions() {
    for (int pass = 0; pass < 4; ++pass) {
        for (const Link& l : model_.links) {
            if (l.kind == LinkKind::Spring || l.kind == LinkKind::Damper) continue;
            const Body &A = model_.bodies[l.a], &B = model_.bodies[l.b];
            BodyState &sa = S_[l.a], &sb = S_[l.b];
            Vec2 imA = invMv(A), imB = invMv(B);
            double iA = A.invI, iB = B.invI;
            if (l.kind == LinkKind::Weld && iA + iB > 1e-18) {
                double err = (sb.angle - sa.angle) - l.refAngle;
                double lam = -0.8 * err / (iA + iB);
                sa.angle -= iA * lam;
                sb.angle += iB * lam;
                sa.rot = Rot(sa.angle);
                sb.rot = Rot(sb.angle);
            }
            Vec2 ra = sa.rot.apply(l.la), rb = sb.rot.apply(l.lb);
            Vec2 d = (sb.pos + rb) - (sa.pos + ra), P;
            if (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld) {
                double k11 = imA.x + imB.x + iA * ra.y * ra.y + iB * rb.y * rb.y;
                double k12 = -iA * ra.x * ra.y - iB * rb.x * rb.y;
                double k22 = imA.y + imB.y + iA * ra.x * ra.x + iB * rb.x * rb.x;
                double det = k11 * k22 - k12 * k12;
                if (std::fabs(det) < 1e-18) continue;
                P = Vec2(-(k22 * d.x - k12 * d.y) / det, -(-k12 * d.x + k11 * d.y) / det) * 0.8;
            } else if (l.kind == LinkKind::Roller) {
                Vec2 nrm = sa.rot.apply(Vec2(0, 1));
                double K = kDir(A, ra, nrm) + kDir(B, rb, nrm);
                if (K < 1e-18) continue;
                P = nrm * (-dot(d, nrm) / K * 0.8);
            } else {
                double len = length(d);
                Vec2 u = len > 1e-9 ? d * (1.0 / len) : Vec2(1, 0);
                double K = kDir(A, ra, u) + kDir(B, rb, u);
                if (K < 1e-18) continue;
                P = u * (-(len - l.L0) / K * 0.8);
            }
            sa.pos -= Vec2(P.x * imA.x, P.y * imA.y);
            sa.angle -= iA * cross(ra, P);
            sb.pos += Vec2(P.x * imB.x, P.y * imB.y);
            sb.angle += iB * cross(rb, P);
            sa.rot = Rot(sa.angle);
            sb.rot = Rot(sb.angle);
        }
        for (const Contact& c : contacts_) {
            const Body &A = model_.bodies[c.a], &B = model_.bodies[c.b];
            BodyState &sa = S_[c.a], &sb = S_[c.b];
            Vec2 pa = sa.pos + sa.rot.apply(c.la), pb = sb.pos + sb.rot.apply(c.lb);
            double sep = dot(pb - pa, c.n);
            double C = clampd(kBaumgarte * (sep + kSlop), -0.2, 0.0);
            if (C >= 0) continue;
            Vec2 p = (pa + pb) * 0.5, ra = p - sa.pos, rb = p - sb.pos;
            double K = kDir(A, ra, c.n) + kDir(B, rb, c.n);
            if (K < 1e-18) continue;
            Vec2 P = c.n * (-C / K);
            Vec2 imA = invMv(A), imB = invMv(B);
            sa.pos -= Vec2(P.x * imA.x, P.y * imA.y);
            sa.angle -= A.invI * cross(ra, P);
            sb.pos += Vec2(P.x * imB.x, P.y * imB.y);
            sb.angle += B.invI * cross(rb, P);
            sa.rot = Rot(sa.angle);
            sb.rot = Rot(sb.angle);
        }
    }
}

void World::applyLocks() {
    for (size_t i = 0; i < model_.bodies.size(); ++i) {
        const Body& b = model_.bodies[i];
        if (!b.isDynamic()) continue;
        if (b.lockX) S_[i].vel.x = 0;
        if (b.lockY) S_[i].vel.y = 0;
        if (b.lockRot) S_[i].omega = 0;
    }
}

// ====================================================================== constrained step
void World::stepConstrained(double h) {
    const size_t n = model_.bodies.size();
    computeForces(S_, t_, F_, tau_, nullptr, nullptr, nullptr);
    for (size_t i = 0; i < n; ++i) {                              // smooth forces -> velocities
        const Body& b = model_.bodies[i];
        if (!b.isDynamic()) continue;
        S_[i].vel += F_[i] * (b.invMass * h);
        S_[i].omega += tau_[i] * b.invI * h;
    }
    applyLocks();

    contacts_.clear();
    stepDKE_ = 0;
    if (model_.env.collisions) buildContacts();
    warmStartAndPrestep(h);
    jointImp_.assign(model_.links.size(), Vec2());
    jointAng_.assign(model_.links.size(), 0.0);
    for (int it = 0; it < model_.env.solverIterations; ++it) {
        solveJoints();
        solveContacts();
        applyLocks();
    }
    warm_.clear();
    for (const Contact& c : contacts_) warm_[c.key] = {c.Pn, c.Pt, c.Pr, c.slip};

    // ---- energy accounting for this step's contacts.
    // The TOTAL kinetic energy removed by contact impulses is exact (stepDKE_). It is split into
    //   friction heat  = |Pt| * (|slip speed before| + |slip speed after|) / 2     (plus the same for rolling resistance)
    //   impact loss    = everything else, but only on steps that contain a genuine impact
    // Steps without an impact (resting contact) only exchange energy reversibly; that part stays out of the loss channels.
    {
        double heat = 0;
        bool anyImpact = false;
        for (const Contact& c : contacts_) {
            const BodyState &sa = S_[c.a], &sb = S_[c.b];
            Vec2 vrel = (sb.vel + cross(sb.omega, c.rb)) - (sa.vel + cross(sa.omega, c.ra));
            heat += std::fabs(c.Pt) * (std::fabs(c.vt0) + std::fabs(dot(vrel, c.t))) * 0.5;
            heat += std::fabs(c.Pr) * (std::fabs(c.wrel0) + std::fabs(sb.omega - sa.omega)) * 0.5;
            anyImpact = anyImpact || c.impact;
        }
        double loss = -stepDKE_;
        ledger_.dFriction += heat;
        if (anyImpact) ledger_.dImpact += loss - heat;
        else ledger_.unclassified += loss - heat;
    }

    for (size_t i = 0; i < n; ++i) {                              // velocities -> positions
        if (!model_.bodies[i].isDynamic()) continue;
        S_[i].pos += S_[i].vel * h;
        S_[i].angle += S_[i].omega * h;
        S_[i].rot = Rot(S_[i].angle);
    }
    double vBefore = potentialEnergy(S_);
    correctPositions();
    ledger_.posCorr += potentialEnergy(S_) - vBefore;

    contactInfo_.clear();
    for (const Contact& c : contacts_) {
        ContactInfo ci;
        ci.a = c.a;
        ci.b = c.b;
        ci.point = (c.pA + c.pB) * 0.5;
        ci.normal = c.n;
        ci.separation = c.sep;
        ci.normalImpulse = c.Pn;
        contactInfo_.push_back(ci);
    }
}

// ====================================================================== the public step
void World::step() {
    const double h = model_.env.dt;
    double pv0, pd0, pin0, pv1, pd1, pin1;
    computeForces(S_, t_, F_, tau_, &pv0, &pd0, &pin0);
    if (usesConstraintSolver()) stepConstrained(h);
    else {
        stepSmooth(h);
        contacts_.clear();
        contactInfo_.clear();
    }
    t_ += h;
    ++steps_;
    computeForces(S_, t_, F_, tau_, &pv1, &pd1, &pin1);
    ledger_.workIn += 0.5 * (pin0 + pin1) * h;                    // trapezoid rule for the three power integrals
    ledger_.dViscous += 0.5 * (pv0 + pv1) * h;
    ledger_.dDrag += 0.5 * (pd0 + pd1) * h;

    for (size_t i = 0; i < S_.size(); ++i) {
        const BodyState& s = S_[i];
        if (!std::isfinite(s.pos.x) || !std::isfinite(s.pos.y) || !std::isfinite(s.vel.x) || !std::isfinite(s.vel.y) ||
            !std::isfinite(s.omega) || std::fabs(s.pos.x) > 1e5 || std::fabs(s.pos.y) > 1e5)
            healthy_ = false;
    }
    measure();
}

}  // namespace vlab
