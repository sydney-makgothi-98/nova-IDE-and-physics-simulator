#include "vlab/section.hpp"
#include <algorithm>
#include <cmath>

namespace vlab {

const char* sectionName(int type) {
    static const char* n[SEC_COUNT] = {"Slab (rectangle, 1 m deep)", "Rectangle", "Solid circle", "Hollow rectangle (box)", "Hollow circle (tube)", "I-beam", "T-beam", "Channel (C)"};
    return n[std::clamp(type, 0, (int)SEC_COUNT - 1)];
}

int sectionDims(int type) {
    switch (type) {
        case SEC_RECT: return 1 | 2;
        case SEC_CIRCLE: return 2;
        case SEC_BOX: return 1 | 2 | 4;
        case SEC_TUBE: return 2 | 4;
        case SEC_I: case SEC_T: case SEC_CHANNEL: return 1 | 2 | 4 | 8;
        default: return 0;
    }
}

double sectionHeight(const Body& b) { return b.secType == SEC_SLAB ? b.shape.h : std::max(0.005, b.secH); }

static double chord(double r, double d) { return d >= r ? 0.0 : 2.0 * std::sqrt(std::max(0.0, r * r - d * d)); }   // width of a circle of radius r at distance d from its centre

Section makeSection(const Body& body) {
    Section s;
    const int N = 400;
    s.n = N;
    s.H = sectionHeight(body);
    s.dy = s.H / N;
    s.b.assign(N, 0.0);
    const double H = s.H, B = std::max(0.002, body.secB);
    const double tf = std::clamp(body.secTf, 0.001, H / 2 - 1e-4), tw = std::clamp(body.secTw, 0.001, B);
    const double r = H / 2;
    for (int k = 0; k < N; ++k) {
        const double y = s.y(k);
        double w = 0;
        switch (body.secType) {
            case SEC_SLAB: w = 1.0; break;
            case SEC_RECT: w = B; break;
            case SEC_CIRCLE: w = chord(r, std::fabs(y - r)); break;
            case SEC_BOX: { double t = std::clamp(body.secTf, 0.001, std::min(B, H) / 2 - 1e-4); w = (y < t || y > H - t) ? B : 2.0 * t; break; }
            case SEC_TUBE: { double t = std::clamp(body.secTf, 0.001, r - 1e-4); w = chord(r, std::fabs(y - r)) - chord(r - t, std::fabs(y - r)); break; }
            case SEC_I: case SEC_CHANNEL: w = (y < tf || y > H - tf) ? B : tw; break;
            case SEC_T: w = y < tf ? B : tw; break;
            default: w = 1.0;
        }
        s.b[k] = std::max(0.0, w);
    }
    double A = 0, My = 0;
    for (int k = 0; k < N; ++k) { A += s.b[k] * s.dy; My += s.b[k] * s.dy * s.y(k); }
    s.A = A;
    s.yc = A > 0 ? My / A : H / 2;
    double I = 0;
    for (int k = 0; k < N; ++k) { double d = s.y(k) - s.yc; I += s.b[k] * s.dy * d * d + s.b[k] * s.dy * s.dy * s.dy / 12.0; }
    s.I = I;
    s.cTop = s.yc;
    s.cBot = H - s.yc;
    s.cMax = std::max(s.cTop, s.cBot);
    s.Z = s.cMax > 0 ? I / s.cMax : 0.0;
    s.rg = A > 0 ? std::sqrt(I / A) : 0.0;
    return s;
}

void sectionStress(const Section& s, double M, double V, double Nax, SectionStress& out) {
    const int N = s.n;
    out.y.assign(N, 0.0);
    out.sigma.assign(N, 0.0);
    out.tau.assign(N, 0.0);
    out.sigMax = -1e300; out.sigMin = 1e300; out.tauMax = 0;
    double Q = 0;                                                    // first moment of everything above this strip, about the neutral axis
    for (int k = 0; k < N; ++k) {
        const double y = s.y(k), d = y - s.yc;
        Q += s.b[k] * s.dy * 0.5 * (s.yc - y);                        // half of this strip
        double q = Q;
        Q += s.b[k] * s.dy * 0.5 * (s.yc - y);                        // the other half
        out.y[k] = y;
        out.sigma[k] = s.A > 0 ? Nax / s.A + (s.I > 0 ? M * d / s.I : 0.0) : 0.0;
        out.tau[k] = (s.I > 0 && s.b[k] > 1e-9) ? V * q / (s.I * s.b[k]) : 0.0;
        out.sigMax = std::max(out.sigMax, out.sigma[k]);
        out.sigMin = std::min(out.sigMin, out.sigma[k]);
        out.tauMax = std::max(out.tauMax, std::fabs(out.tau[k]));
    }
}

}  // namespace vlab
