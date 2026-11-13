#include "schematic.hpp"
#include "vlab/expr.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace vlab {

namespace {

std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

std::string num(double v) {
    char b[48];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}

std::string eng(double v, const char* unit) {                                  // 45000 N/m -> "45 kN/m"
    char b[64];
    double a = std::fabs(v);
    if (a >= 1e6) std::snprintf(b, sizeof b, "%.3g M%s", v / 1e6, unit);
    else if (a >= 1e3) std::snprintf(b, sizeof b, "%.3g k%s", v / 1e3, unit);
    else std::snprintf(b, sizeof b, "%.3g %s", v, unit);
    return b;
}

struct Pt { double x, y; };
Pt operator+(Pt a, Pt b) { return {a.x + b.x, a.y + b.y}; }
Pt operator-(Pt a, Pt b) { return {a.x - b.x, a.y - b.y}; }
Pt operator*(Pt a, double s) { return {a.x * s, a.y * s}; }
double len(Pt a) { return std::sqrt(a.x * a.x + a.y * a.y); }

struct Label {
    std::string text;
    Pt target;        // where the leader line ends (on the part)
    Pt anchor;        // preferred label position (at the part's height)
    bool inside = false;   // the label sits inside the shape, no leader
    double w = 0;
    int side = 0;     // -1 left margin, +1 right margin
    double y = 0;
};

}  // namespace

bool exportSchematicSvg(const std::string& path, const Model& m, const SchematicOptions& o, std::string* error) {
    if (m.bodies.empty()) { if (error) *error = "the design is empty"; return false; }

    // ---- world bounds
    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    auto grow = [&](Vec2 v) { minx = std::min(minx, v.x); miny = std::min(miny, v.y); maxx = std::max(maxx, v.x); maxy = std::max(maxy, v.y); };
    // an "indefinite" floor (thousands of metres) is drawn only a little wider than the parts it carries
    auto isInf = [](const Body& b) { return b.shape.preset == ShapePreset::Line && b.shape.w >= 5000.0; };
    for (const Body& b : m.bodies) {
        if (isInf(b)) continue;
        Rot r(b.angle);
        if (b.shape.isCircle()) { grow(b.pos + Vec2(b.shape.radius, b.shape.radius)); grow(b.pos - Vec2(b.shape.radius, b.shape.radius)); }
        else if (b.kind == BodyKind::Anchor) { grow(b.pos + Vec2(0.15, 0.3)); grow(b.pos - Vec2(0.15, 0.1)); }
        else for (const Vec2& v : b.shape.verts) grow(b.pos + r.apply(v));
        if (o.loads && b.loadN > 0) grow(b.pos + Vec2(0.9, 0.9)), grow(b.pos - Vec2(0.9, 0.9));
    }
    if (minx > maxx) { minx = -2; maxx = 2; miny = -1; maxy = 1; }                      // only an indefinite floor in the design
    const double infHalf = std::max(2.0, (maxx - minx) * 0.65);                         // half the length shown for an indefinite floor
    for (const Body& b : m.bodies)
        if (isInf(b)) {
            Rot r(b.angle);
            const double cx = 0.5 * (minx + maxx);
            Vec2 c = b.pos;
            if (std::fabs(std::cos(b.angle)) > 0.5) { c.x = cx; c.y = b.pos.y + (cx - b.pos.x) * std::tan(b.angle); }
            grow(c + r.apply(Vec2(-infHalf, 0))); grow(c + r.apply(Vec2(infHalf, 0)));
        }
    const double dw = std::max(0.5, maxx - minx), dh = std::max(0.5, maxy - miny);
    const double W = 1320, margin = 290, padTop = 70, padBot = 60;
    double scale = std::min((W - 2 * margin) / dw, 620.0 / dh);
    const double drawH = dh * scale;
    const double H = drawH + padTop + padBot;
    auto S = [&](Vec2 w) { return Pt{margin + (w.x - minx) * scale + ((W - 2 * margin) - dw * scale) / 2, padTop + (w.y - miny) * scale}; };
    auto worldOf = [&](int body, Vec2 local) { return m.bodies[body].pos + Rot(m.bodies[body].angle).apply(local); };

    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << num(W) << "\" height=\"" << num(H) << "\" viewBox=\"0 0 " << num(W) << " " << num(H) << "\" font-family=\"Segoe UI, Arial, sans-serif\">\n";
    svg << "<defs><marker id=\"ah\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"9\" markerHeight=\"9\" orient=\"auto-start-reverse\"><path d=\"M0,0 L10,5 L0,10 z\" fill=\"#222\"/></marker>"
        << "<marker id=\"ahl\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"5\" markerHeight=\"5\" orient=\"auto\"><path d=\"M0,0 L10,5 L0,10 z\" fill=\"#c0392b\"/></marker></defs>\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    if (!o.title.empty()) svg << "<text x=\"18\" y=\"30\" font-size=\"19\" font-weight=\"600\" fill=\"#222\">" << esc(o.title) << "</text>\n";

    const char* ink = "#222";
    auto line = [&](Pt a, Pt b, double w = 2.2, const char* col = "#222") {
        svg << "<line x1=\"" << num(a.x) << "\" y1=\"" << num(a.y) << "\" x2=\"" << num(b.x) << "\" y2=\"" << num(b.y) << "\" stroke=\"" << col << "\" stroke-width=\"" << num(w) << "\" stroke-linecap=\"round\"/>\n";
    };
    auto poly = [&](const std::vector<Pt>& p, bool closed, const char* fill, double w = 2.2) {
        svg << (closed ? "<polygon points=\"" : "<polyline points=\"");
        for (const Pt& q : p) svg << num(q.x) << "," << num(q.y) << " ";
        svg << "\" fill=\"" << fill << "\" stroke=\"" << ink << "\" stroke-width=\"" << num(w) << "\" stroke-linejoin=\"round\" stroke-linecap=\"round\"/>\n";
    };
    // ground symbol: a short hatched line below a point, rotated by angle
    auto ground = [&](Pt c, double angle, double halfW) {
        Pt ex{std::cos(angle), std::sin(angle)}, ey{-std::sin(angle), std::cos(angle)};
        line(c - ex * halfW, c + ex * halfW, 2.6);
        for (double t = -halfW; t <= halfW - 4; t += 9) line(c + ex * t, c + ex * (t - 7) + ey * 9, 1.3);
    };

    std::vector<Label> labels;
    auto addLabel = [&](const std::string& text, Pt target, bool inside, Pt anchor) {
        Label l;
        l.text = text; l.target = target; l.inside = inside; l.anchor = anchor;
        l.w = 8.6 * (double)text.size();
        labels.push_back(l);
    };

    // ---- bodies: fixed things first
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < m.bodies.size(); ++i) {
            const Body& b = m.bodies[i];
            if ((b.kind == BodyKind::Dynamic) != (pass == 1)) continue;
            Rot r(b.angle);
            if (b.kind == BodyKind::Anchor) continue;                              // supports are drawn with their connections below
            if (b.shape.preset == ShapePreset::Line) {                             // a floor or wall: a thick line with hatching on the underside
                Vec2 fc = b.pos;
                double hl = b.shape.w / 2;
                if (isInf(b)) {
                    hl = infHalf;
                    if (std::fabs(std::cos(b.angle)) > 0.5) { fc.x = 0.5 * (minx + maxx); fc.y = b.pos.y + (fc.x - b.pos.x) * std::tan(b.angle); }
                }
                Pt a = S(fc + r.apply(Vec2(-hl, 0))), c = S(fc + r.apply(Vec2(hl, 0)));
                line(a, c, 3.2);
                Pt u = c - a;
                double L = len(u);
                if (L > 1) {
                    u = u * (1.0 / L);
                    Pt nrm{-u.y, u.x};
                    Vec2 down = r.apply(Vec2(0, 1));
                    if (nrm.x * down.x + nrm.y * down.y < 0) nrm = nrm * -1.0;
                    for (double t = 0; t < L; t += 13) line(a + u * t, a + u * (t - 8) + nrm * 11, 1.3);
                }
                if (o.names) addLabel(o.values ? b.name + " (mu = " + num(b.mat.muK) + ")" : b.name, (a + c) * 0.5 + Pt{0, 0}, false, (a + c) * 0.5);
                continue;
            }
            std::vector<Pt> pts;
            if (b.shape.isCircle()) {
                Pt c = S(b.pos);
                svg << "<circle cx=\"" << num(c.x) << "\" cy=\"" << num(c.y) << "\" r=\"" << num(b.shape.radius * scale) << "\" fill=\"" << (b.kind == BodyKind::Static ? "#cfcfcf" : "#e4e4e4") << "\" stroke=\"" << ink << "\" stroke-width=\"2.2\"/>\n";
                pts.push_back(c);
            } else {
                for (const Vec2& v : b.shape.verts) pts.push_back(S(b.pos + r.apply(v)));
                poly(pts, true, b.kind == BodyKind::Static ? "#cfcfcf" : "#e4e4e4");
            }
            if (o.names) {
                char txt[160];
                if (o.values && b.isDynamic()) std::snprintf(txt, sizeof txt, "%s (%s kg)", b.name.c_str(), num(b.massKg).c_str());
                else std::snprintf(txt, sizeof txt, "%s", b.name.c_str());
                Pt c = S(b.pos);
                double bw, bh;
                if (b.shape.isCircle()) { bw = bh = 2 * b.shape.radius * scale; }
                else {
                    double lx = 1e30, hx = -1e30, ly = 1e30, hy = -1e30;
                    for (const Pt& q : pts) { lx = std::min(lx, q.x); hx = std::max(hx, q.x); ly = std::min(ly, q.y); hy = std::max(hy, q.y); }
                    bw = hx - lx; bh = hy - ly;
                }
                bool fits = bw > 8.6 * std::strlen(txt) + 24 && bh > 34;
                addLabel(txt, c, fits, c);
            }
        }
    }

    // ---- connections
    std::vector<int> supportKind(m.bodies.size(), -1);                              // what an anchor holds: 0 pin, 1 fixed, 2 roller, 3 plain
    for (size_t li = 0; li < m.links.size(); ++li) {
        const Link& l = m.links[li];
        for (int side = 0; side < 2; ++side) {
            int bi = side == 0 ? l.a : l.b;
            if (bi >= 0 && m.bodies[bi].kind == BodyKind::Anchor) {
                int k = l.kind == LinkKind::Pin ? 0 : l.kind == LinkKind::Weld ? 1 : l.kind == LinkKind::Roller ? 2 : 3;
                supportKind[bi] = std::min(supportKind[bi] < 0 ? 9 : supportKind[bi], k);
            }
        }
    }
    for (size_t li = 0; li < m.links.size(); ++li) {
        const Link& l = m.links[li];
        Pt A = S(worldOf(l.a, l.la)), Bp = S(worldOf(l.b, l.lb));
        Pt d = Bp - A;
        double L = len(d);
        const bool isSupportLink = (m.bodies[l.a].kind == BodyKind::Anchor || m.bodies[l.b].kind == BodyKind::Anchor) && (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld || l.kind == LinkKind::Roller);
        if (l.kind == LinkKind::Spring || l.kind == LinkKind::Damper || l.kind == LinkKind::Rod) {
            if (L < 2) continue;
            Pt u = d * (1.0 / L), n{-u.y, u.x};
            if (l.kind == LinkKind::Spring) {
                const double lead = std::min(0.16 * L, 26.0), zone = L - 2 * lead, amp = 9.5;
                const int teeth = std::max(4, std::min(9, (int)(zone / 15)));
                std::vector<Pt> p{A, A + u * lead};
                for (int i = 1; i < 2 * teeth; ++i) p.push_back(A + u * (lead + zone * i / (2.0 * teeth)) + n * (amp * (i % 2 ? 1 : -1)));
                p.push_back(A + u * (L - lead));
                p.push_back(Bp);
                poly(p, false, "none", 2.2);
            } else if (l.kind == LinkKind::Damper) {
                const double c0 = 0.38 * L, c1 = 0.72 * L, pist = 0.50 * L, hw = 10.5;
                line(A, A + u * pist, 2.2);                                          // piston rod
                line(A + u * pist - n * (hw - 3), A + u * pist + n * (hw - 3), 2.8);   // piston plate
                std::vector<Pt> cup{A + u * c0 - n * hw, A + u * c1 - n * hw, A + u * c1 + n * hw, A + u * c0 + n * hw};   // the cylinder, open towards the rod
                poly(cup, false, "none", 2.2);
                line(A + u * c1, Bp, 2.2);
            } else {
                line(A, Bp, 4.4);
                for (Pt q : {A, Bp}) svg << "<circle cx=\"" << num(q.x) << "\" cy=\"" << num(q.y) << "\" r=\"3.6\" fill=\"#fff\" stroke=\"" << ink << "\" stroke-width=\"1.8\"/>\n";
            }
            if (o.names) {
                std::string t, v;
                if (l.kind == LinkKind::Spring) { t = "Spring"; v = "k = " + eng(l.k, "N/m"); }
                else if (l.kind == LinkKind::Damper) { t = "Damping element"; v = "c = " + eng(l.c, "N s/m"); }
                else { t = "Rigid rod"; v = "L = " + num(l.L0) + " m"; }
                addLabel(o.values ? t + " (" + v + ")" : t, (A + Bp) * 0.5, false, (A + Bp) * 0.5);
            }
        } else if (isSupportLink) {
            Pt at = (m.bodies[l.a].kind == BodyKind::Anchor) ? A : Bp;
            double ang = m.bodies[m.bodies[l.a].kind == BodyKind::Anchor ? l.a : l.b].angle;
            if (l.kind == LinkKind::Pin) {                                           // pin support: triangle on hatched ground
                Pt ex{std::cos(ang), std::sin(ang)}, ey{-std::sin(ang), std::cos(ang)};
                poly({at, at + ex * -13 + ey * 20, at + ex * 13 + ey * 20}, true, "#e4e4e4", 2.0);
                ground(at + ey * 20, ang, 24);
            } else if (l.kind == LinkKind::Weld) {                                    // fixed support: a wall of hatching
                Pt ex{std::cos(ang), std::sin(ang)}, ey{-std::sin(ang), std::cos(ang)};
                ground(at, ang, 24);
                (void)ey; (void)ex;
            } else {                                                                 // roller: triangle on two wheels
                Pt ex{std::cos(ang), std::sin(ang)}, ey{-std::sin(ang), std::cos(ang)};
                poly({at, at + ex * -13 + ey * 18, at + ex * 13 + ey * 18}, true, "#e4e4e4", 2.0);
                for (double sgn : {-1.0, 1.0}) { Pt c = at + ex * (6 * sgn) + ey * 23; svg << "<circle cx=\"" << num(c.x) << "\" cy=\"" << num(c.y) << "\" r=\"4.5\" fill=\"#fff\" stroke=\"" << ink << "\" stroke-width=\"1.8\"/>\n"; }
                ground(at + ey * 28, ang, 24);
            }
            svg << "<circle cx=\"" << num(at.x) << "\" cy=\"" << num(at.y) << "\" r=\"4.4\" fill=\"#fff\" stroke=\"" << ink << "\" stroke-width=\"1.8\"/>\n";
            if (o.names) addLabel(l.kind == LinkKind::Pin ? "Pin support" : (l.kind == LinkKind::Weld ? "Fixed support" : "Roller support"), at, false, at);
        } else if (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld) {            // a joint between two bodies
            Pt at = A;
            if (l.kind == LinkKind::Pin) svg << "<circle cx=\"" << num(at.x) << "\" cy=\"" << num(at.y) << "\" r=\"6\" fill=\"#fff\" stroke=\"" << ink << "\" stroke-width=\"2\"/>\n";
            else svg << "<rect x=\"" << num(at.x - 6) << "\" y=\"" << num(at.y - 6) << "\" width=\"12\" height=\"12\" fill=\"#222\"/>\n";
            if (o.names) addLabel(l.kind == LinkKind::Pin ? "Pin joint" : "Fixed joint", at, false, at);
        }
    }
    // plain anchors (a point to hang springs from) that are not supports
    for (size_t i = 0; i < m.bodies.size(); ++i) {
        const Body& b = m.bodies[i];
        if (b.kind != BodyKind::Anchor || supportKind[i] == 0 || supportKind[i] == 1 || supportKind[i] == 2) continue;
        Pt c = S(b.pos);
        ground(c + Pt{0, 0}, 0, 14);
        svg << "<circle cx=\"" << num(c.x) << "\" cy=\"" << num(c.y) << "\" r=\"3.8\" fill=\"" << ink << "\"/>\n";
    }

    // ---- applied loads: arrows
    if (o.loads) {
        for (const Body& b : m.bodies) {
            if (!b.isDynamic()) continue;
            Pt c = S(b.pos);
            if (b.loadN > 0) {
                double a = (b.loadAngleDeg + (b.loadPull ? 180.0 : 0.0)) * 3.14159265358979 / 180.0;
                Pt dir{std::cos(a), std::sin(a)};
                double rb = std::max(0.3, b.shape.boundingRadius()) * scale * 0.85;
                Pt head = c - dir * rb, tail = head - dir * 95;
                svg << "<line x1=\"" << num(tail.x) << "\" y1=\"" << num(tail.y) << "\" x2=\"" << num(head.x) << "\" y2=\"" << num(head.y) << "\" stroke=\"#c0392b\" stroke-width=\"3\" marker-end=\"url(#ahl)\"/>\n";
                if (o.names) {
                    char t[96];
                    if (o.values) std::snprintf(t, sizeof t, "%s %.0f N (%.1f kg)", b.loadPull ? "Pull" : "Push", b.loadN, b.loadN / 9.80665);
                    else std::snprintf(t, sizeof t, "%s", b.loadPull ? "Pull" : "Push");
                    addLabel(t, tail + (head - tail) * 0.5, false, tail);
                }
            }
            if (b.dloadType != 0 && b.shape.preset == ShapePreset::Box) {            // a row of arrows with the load's envelope
                Rot r(b.angle);
                double a0 = std::max(0.0, b.dloadX0), a1 = (b.dloadX1 < 0 || b.dloadX1 > b.shape.w) ? b.shape.w : b.dloadX1;
                double ang = b.dloadAngleDeg * 3.14159265358979 / 180.0;
                Vec2 dv(std::cos(ang), std::sin(ang));
                const int n = 12;
                std::vector<Pt> env;
                for (int k = 0; k <= n; ++k) {
                    double x = a0 + (a1 - a0) * k / n, f = 1.0, s = (a1 > a0) ? (x - a0) / (a1 - a0) : 0;
                    if (b.dloadType == DL_TriRising) f = s; else if (b.dloadType == DL_TriFalling) f = 1 - s; else if (b.dloadType == DL_TriPeak) f = 1 - std::fabs(2 * s - 1);
                    else if (b.dloadType == DL_Equation) f = 0.7 + 0.3 * std::sin(3.14159265 * s);      // schematic: the exact curve is in the model
                    if (f < 0.04) continue;
                    Vec2 onAxis = b.pos + r.apply(Vec2(x - b.shape.w / 2, 0));
                    double half = std::fabs(dv.y * 0 + 1) * b.shape.h / 2;
                    Pt head = S(onAxis - dv * half), tail = head - Pt{dv.x, dv.y} * (14 + 40 * f);
                    svg << "<line x1=\"" << num(tail.x) << "\" y1=\"" << num(tail.y) << "\" x2=\"" << num(head.x) << "\" y2=\"" << num(head.y) << "\" stroke=\"#c0392b\" stroke-width=\"2\" marker-end=\"url(#ahl)\"/>\n";
                    env.push_back(tail);
                }
                if (env.size() > 1) poly(env, false, "none", 1.6);
                if (o.names && !env.empty()) {
                    char t[120];
                    if (!o.values) std::snprintf(t, sizeof t, "Distributed load");
                    else if (b.dloadType == DL_Equation) std::snprintf(t, sizeof t, "Distributed load q(x) = %s", b.dloadExpr.c_str());
                    else std::snprintf(t, sizeof t, "Distributed load %.0f N/m%s", b.dloadN, b.dloadType == DL_Uniform ? "" : " (peak)");
                    addLabel(t, env[env.size() / 2], false, env[env.size() / 2]);
                }
            }
        }
    }

    // ---- labels: inside shapes where they fit, otherwise in the margins with a leader line to the part
    const double midX = W / 2;
    std::vector<size_t> sideIdx[2];
    for (size_t i = 0; i < labels.size(); ++i) {
        Label& l = labels[i];
        if (l.inside) continue;
        l.side = l.target.x < midX ? -1 : 1;
        sideIdx[l.side < 0 ? 0 : 1].push_back(i);
    }
    for (int sd = 0; sd < 2; ++sd) {
        auto& v = sideIdx[sd];
        std::sort(v.begin(), v.end(), [&](size_t a, size_t b) { return labels[a].anchor.y < labels[b].anchor.y; });
        double lastY = -1e30;
        for (size_t i : v) {
            double y = std::max(labels[i].anchor.y, lastY + 30);                    // keep the labels apart
            labels[i].y = y;
            lastY = y;
        }
    }
    for (const Label& l : labels) {
        if (l.inside) {
            svg << "<text x=\"" << num(l.anchor.x) << "\" y=\"" << num(l.anchor.y + 5) << "\" font-size=\"17\" text-anchor=\"middle\" fill=\"#222\">" << esc(l.text) << "</text>\n";
            continue;
        }
        double tx = l.side < 0 ? 18 + l.w : W - 18 - l.w;                           // the text spans [18, 18 + w] on the left, [W - 18 - w, W - 18] on the right
        Pt from{l.side < 0 ? std::min(tx + 8, margin - 12) : std::max(tx - 8, W - margin + 12), l.y - 5};
        svg << "<line x1=\"" << num(from.x) << "\" y1=\"" << num(from.y) << "\" x2=\"" << num(l.target.x) << "\" y2=\"" << num(l.target.y) << "\" stroke=\"#222\" stroke-width=\"1.3\""
            << (o.arrowheads ? " marker-end=\"url(#ah)\"" : "") << "/>\n";
        if (l.side < 0) svg << "<text x=\"" << num(18) << "\" y=\"" << num(l.y) << "\" font-size=\"17\" text-anchor=\"start\" fill=\"#222\">" << esc(l.text) << "</text>\n";
        else svg << "<text x=\"" << num(W - 18) << "\" y=\"" << num(l.y) << "\" font-size=\"17\" text-anchor=\"end\" fill=\"#222\">" << esc(l.text) << "</text>\n";
    }
    svg << "</svg>\n";

    std::ofstream f(path, std::ios::binary);
    if (!f) { if (error) *error = "cannot write " + path; return false; }
    f << svg.str();
    return true;
}

}  // namespace vlab
