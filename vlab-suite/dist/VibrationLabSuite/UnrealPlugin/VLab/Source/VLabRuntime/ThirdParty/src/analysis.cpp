#include "vlab/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include "vlab/world.hpp"

namespace vlab {

// ====================================================================== statistics
Stats computeStats(const std::vector<double>& t, const std::vector<double>& y) {
    Stats s;
    if (y.empty()) return s;
    s.min = s.max = y[0];
    s.tMin = s.tMax = t.empty() ? 0 : t[0];
    double sum = 0, sq = 0;
    for (size_t i = 0; i < y.size(); ++i) {
        if (y[i] < s.min) { s.min = y[i]; s.tMin = t[i]; }
        if (y[i] > s.max) { s.max = y[i]; s.tMax = t[i]; }
        sum += y[i];
        sq += y[i] * y[i];
    }
    s.mean = sum / y.size();
    s.rms = std::sqrt(sq / y.size());
    s.last = y.back();
    return s;
}

// ====================================================================== FFT
static void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {                      // bit-reversal permutation
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * kPi / (double)len;
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t k = 0; k < len / 2; ++k) {
                std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

Spectrum computeSpectrum(const std::vector<double>& t, const std::vector<double>& y) {
    Spectrum sp;
    const size_t N = y.size();
    if (N < 16 || t.size() != N) return sp;
    double dt = (t.back() - t.front()) / (double)(N - 1);
    if (dt <= 0) return sp;
    double mean = 0;
    for (double v : y) mean += v;
    mean /= (double)N;
    size_t M = 1;
    while (M < N * 2) M <<= 1;                                   // zero-pad for a smoother curve
    std::vector<std::complex<double>> a(M);
    double wsum = 0;
    for (size_t i = 0; i < N; ++i) {
        double w = 0.5 - 0.5 * std::cos(2.0 * kPi * (double)i / (double)(N - 1));   // Hann window
        a[i] = (y[i] - mean) * w;
        wsum += w;
    }
    fft(a);
    double fs = 1.0 / dt;
    for (size_t k = 1; k < M / 2; ++k) {
        double f = (double)k * fs / (double)M;
        double amp = 2.0 * std::abs(a[k]) / wsum;                // amplitude of an equivalent sine wave
        sp.freq.push_back(f);
        sp.amp.push_back(amp);
        if (amp > sp.peakAmp) { sp.peakAmp = amp; sp.peakHz = f; }
    }
    return sp;
}

double estimateDampingRatio(const std::vector<double>& t, const std::vector<double>& y) {
    (void)t;
    if (y.size() < 20) return -1;
    // successive extrema (peak, trough, peak ...): the gap between neighbours shrinks by exp(-delta/2) every half cycle,
    // which does not depend on where the equilibrium sits
    std::vector<double> ext;
    for (size_t i = 1; i + 1 < y.size(); ++i) {
        bool mx = y[i] > y[i - 1] && y[i] >= y[i + 1], mn = y[i] < y[i - 1] && y[i] <= y[i + 1];
        if (mx || mn) ext.push_back(y[i]);
    }
    if (ext.size() < 4) return -1;
    std::vector<double> d;
    for (size_t i = 0; i + 1 < ext.size(); ++i) d.push_back(std::fabs(ext[i] - ext[i + 1]));
    size_t last = std::min<size_t>(d.size() - 1, 12);
    if (last == 0 || d[0] <= 0 || d[last] <= 0 || d[last] >= d[0]) return -1;
    double delta = 2.0 * std::log(d[0] / d[last]) / (double)last;      // logarithmic decrement per full cycle
    return delta / std::sqrt(4.0 * kPi * kPi + delta * delta);
}

// ====================================================================== modal analysis
namespace {
// Jacobi eigenvalue method for a small symmetric matrix. Returns eigenvalues; V holds eigenvectors in columns.
void jacobiEigen(std::vector<std::vector<double>>& A, std::vector<double>& evals, std::vector<std::vector<double>>& V) {
    const size_t n = A.size();
    V.assign(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) V[i][i] = 1.0;
    for (int sweep = 0; sweep < 100; ++sweep) {
        double off = 0;
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j) off += A[i][j] * A[i][j];
        if (off < 1e-24) break;
        for (size_t p = 0; p < n; ++p) {
            for (size_t q = p + 1; q < n; ++q) {
                if (std::fabs(A[p][q]) < 1e-30) continue;
                double theta = (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
                double tt = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                double c = 1.0 / std::sqrt(tt * tt + 1.0), s = tt * c;
                for (size_t k = 0; k < n; ++k) {
                    double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (size_t k = 0; k < n; ++k) {
                    double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (size_t k = 0; k < n; ++k) {
                    double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp - s * vkq;
                    V[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    evals.resize(n);
    for (size_t i = 0; i < n; ++i) evals[i] = A[i][i];
}
}  // namespace

ModalResult modalAnalysis(const Model& model) {
    ModalResult res;
    Model m = model;
    for (Body& b : m.bodies) b.driveF = 0;                       // free vibration: no pushes
    m.updateMass();
    World w(m);
    const std::vector<BodyState> base = w.state();

    struct Dof { int body; int kind; double mass; };
    std::vector<Dof> dofs;
    for (size_t i = 0; i < m.bodies.size(); ++i) {
        const Body& b = m.bodies[i];
        if (!b.isDynamic()) continue;
        if (!b.lockX) { dofs.push_back({(int)i, 0, b.massKg}); res.dofNames.push_back(b.name + " x"); }
        if (!b.lockY) { dofs.push_back({(int)i, 1, b.massKg}); res.dofNames.push_back(b.name + " y"); }
        if (!b.lockRot) { dofs.push_back({(int)i, 2, b.inertia}); res.dofNames.push_back(b.name + " angle"); }
    }
    const size_t n = dofs.size();
    if (n == 0) { res.note = "No moving bodies to analyse."; return res; }
    if (n > 60) { res.note = "Too many degrees of freedom for modal analysis (limit 60)."; return res; }
    for (const Dof& d : dofs) { res.dofBody.push_back(d.body); res.dofKind.push_back(d.kind); }
    {
        bool hasCollider = false;
        for (const Body& b : m.bodies) hasCollider = hasCollider || (!b.isDynamic() && b.kind == BodyKind::Static);
        if (hasCollider || m.env.collisions)
            res.note = "Contacts and joints are ignored; the springs and dampers are linearised about the drawn pose. ";
    }

    auto generalized = [&](const std::vector<BodyState>& S, std::vector<double>& f) {
        std::vector<Vec2> F;
        std::vector<double> tau;
        w.forcesAt(S, 0.0, F, tau);
        f.assign(n, 0.0);
        for (size_t k = 0; k < n; ++k) {
            const Dof& d = dofs[k];
            f[k] = d.kind == 0 ? F[d.body].x : (d.kind == 1 ? F[d.body].y : tau[d.body]);
        }
    };
    auto perturb = [&](std::vector<BodyState> S, size_t k, double eps, bool velocity) {
        const Dof& d = dofs[k];
        BodyState& s = S[d.body];
        if (velocity) {
            if (d.kind == 0) s.vel.x += eps; else if (d.kind == 1) s.vel.y += eps; else s.omega += eps;
        } else {
            if (d.kind == 0) s.pos.x += eps; else if (d.kind == 1) s.pos.y += eps; else { s.angle += eps; s.rot = Rot(s.angle); }
        }
        return S;
    };

    const double eps = 1e-6;
    std::vector<std::vector<double>> K(n, std::vector<double>(n)), C(n, std::vector<double>(n));
    for (size_t j = 0; j < n; ++j) {
        std::vector<double> fp, fm;
        generalized(perturb(base, j, eps, false), fp);
        generalized(perturb(base, j, -eps, false), fm);
        for (size_t i = 0; i < n; ++i) K[i][j] = -(fp[i] - fm[i]) / (2 * eps);        // K = -dF/dq
        generalized(perturb(base, j, eps, true), fp);
        generalized(perturb(base, j, -eps, true), fm);
        for (size_t i = 0; i < n; ++i) C[i][j] = -(fp[i] - fm[i]) / (2 * eps);        // C = -dF/dv
    }
    std::vector<std::vector<double>> S(n, std::vector<double>(n));
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            double kij = 0.5 * (K[i][j] + K[j][i]);                                  // symmetrise
            S[i][j] = kij / std::sqrt(dofs[i].mass * dofs[j].mass);                  // M^-1/2 K M^-1/2
        }
    std::vector<double> ev;
    std::vector<std::vector<double>> V;
    jacobiEigen(S, ev, V);
    for (size_t k = 0; k < n; ++k) {
        Mode md;
        md.freqHz = ev[k] > 1e-9 ? std::sqrt(ev[k]) / (2.0 * kPi) : 0.0;
        std::vector<double> phi(n);
        double mx = 0;
        for (size_t i = 0; i < n; ++i) { phi[i] = V[i][k] / std::sqrt(dofs[i].mass); mx = std::max(mx, std::fabs(phi[i])); }
        double cm = 0;                                                               // modal damping phi^T C phi
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) cm += phi[i] * 0.5 * (C[i][j] + C[j][i]) * phi[j];
        double w0 = std::sqrt(std::max(ev[k], 0.0));
        md.dampingRatio = w0 > 1e-9 ? cm / (2.0 * w0) : 0.0;
        md.shape.resize(n);
        for (size_t i = 0; i < n; ++i) md.shape[i] = mx > 0 ? phi[i] / mx : 0.0;
        res.modes.push_back(md);
    }
    std::sort(res.modes.begin(), res.modes.end(), [](const Mode& a, const Mode& b) { return a.freqHz < b.freqHz; });
    if (!res.modes.empty() && res.modes.front().freqHz == 0.0)
        res.note += "A mode with 0 Hz means part of the system is free to drift (no spring holds it).";
    return res;
}

// ====================================================================== frequency response
std::vector<FrfPoint> frequencyResponse(const Model& model, const FrfRequest& req, const std::function<void(float)>& progress,
                                        const std::function<bool()>& cancel) {
    std::vector<FrfPoint> out;
    if (req.driveBody < 0 || req.driveBody >= (int)model.bodies.size() || req.responseBody < 0 ||
        req.responseBody >= (int)model.bodies.size() || req.points < 2)
        return out;
    for (int p = 0; p < req.points; ++p) {
        if (cancel && cancel()) break;
        double f = req.f0 + (req.f1 - req.f0) * p / (req.points - 1);
        Model m = model;
        for (Body& b : m.bodies) { b.driveF = 0; b.vel = Vec2(); b.omega = 0; }
        Body& d = m.bodies[req.driveBody];
        d.driveF = req.forceAmp;
        d.driveAngleDeg = req.driveAngleDeg;
        d.driveW = 2.0 * kPi * f;
        {   // smooth scenes (springs and dampers only) are integrated with RK4, which is accurate with a much larger step: 4x faster sweeps
            World probe(m);
            if (!probe.usesConstraintSolver()) m.env.dt = std::max(m.env.dt, 0.001);
        }
        World w(m);
        const double measure = std::min(std::max(5.0 / f, 2.0), 12.0);          // a few cycles, but never a minute at very low frequency
        const double T = req.settleSeconds + measure;
        const double dt = m.env.dt;
        const long long steps = (long long)(T / dt), measureFrom = (long long)(req.settleSeconds / dt);
        const Body& rb = m.bodies[req.responseBody];
        double a = 0, b = 0;
        long long cnt = 0;
        for (long long s = 0; s < steps; ++s) {
            w.step();
            if (s >= measureFrom) {
                const BodyState& st = w.state()[req.responseBody];
                double y = req.component == 0 ? st.pos.x - rb.pos.x : (req.component == 1 ? st.pos.y - rb.pos.y : st.angle - rb.angle);
                double ph = 2.0 * kPi * f * w.time();
                a += y * std::cos(ph);
                b += y * std::sin(ph);
                ++cnt;
            }
        }
        FrfPoint pt;
        pt.freqHz = f;
        if (cnt > 0) {
            a *= 2.0 / (double)cnt;
            b *= 2.0 / (double)cnt;
            pt.amplitude = std::hypot(a, b);
            pt.gain = pt.amplitude / req.forceAmp;
            pt.phaseDeg = std::atan2(b, a) * 180.0 / kPi;
        }
        out.push_back(pt);
        if (progress) progress((float)(p + 1) / (float)req.points);
    }
    return out;
}

}  // namespace vlab
