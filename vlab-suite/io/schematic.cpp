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
    bool near = false;     // drawn right beside the part (target + off), no leader line: how a textbook figure names its variables
    Pt off{0, 0};
};

// "m_{1}" -> m with a subscript 1 (SVG tspans); "x^{2}" -> superscript. Everything else is escaped text.
std::string rich(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((s[i] == '_' || s[i] == '^') && i + 1 < s.size() && s[i + 1] == '{') {
            size_t e = s.find('}', i + 2);
            if (e != std::string::npos) {
                o += s[i] == '_' ? "<tspan baseline-shift=\"sub\" font-size=\"72%\">" : "<tspan baseline-shift=\"super\" font-size=\"72%\">";
                o += esc(s.substr(i + 2, e - i - 2)) + "</tspan>";
                i = e;
                continue;
            }
        }
        o += esc(std::string(1, s[i]));
    }
    return o;
}

size_t visLen(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((s[i] == '_' || s[i] == '^') && i + 1 < s.size() && s[i + 1] == '{') { ++i; continue; }
        if (s[i] == '}') continue;
        if (((unsigned char)s[i] & 0xC0) == 0x80) continue;                      // UTF-8 continuation byte
        ++n;
    }
    return n;
}

}  // namespace

SymbolMap makeSymbols(const Model& m) {
    SymbolMap s;
    s.body.assign(m.bodies.size(), 0);
    s.link.assign(m.links.size(), 0);
    for (size_t i = 0; i < m.bodies.size(); ++i)
        if (m.bodies[i].isDynamic()) s.body[i] = ++s.moving;
    for (size_t j = 0; j < m.links.size(); ++j) {
        const Link& l = m.links[j];
        switch (l.kind) {
            case LinkKind::Spring: s.link[j] = l.dist ? ++s.springDists : ++s.springs; break;
            case LinkKind::Damper: s.link[j] = l.dist ? ++s.damperDists : ++s.dampers; break;
            case LinkKind::Rod: s.link[j] = ++s.rods; break;
            case LinkKind::Pin: s.link[j] = l.kt != 0 ? ++s.torsions : ++s.joints; break;
            default: s.link[j] = ++s.joints; break;
        }
    }
    return s;
}

std::string schematicSvgString(const Model& m, const SchematicOptions& o) {
    if (m.bodies.empty()) return std::string();
    const SymbolMap sym = makeSymbols(m);
    const bool S_ = o.symbols;

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
        if (o.loads && (b.loadN > 0 || b.loadTimeVar || b.driveF > 0 || b.momentN != 0 || b.momentTimeVar)) grow(b.pos + Vec2(0.9, 0.9)), grow(b.pos - Vec2(0.9, 0.9));
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
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << num(W) << "\" height=\"@@H@@\" viewBox=\"0 0 " << num(W) << " @@H@@\" font-family=\"" << (o.serif ? "Cambria Math, STIX Two Text, Times New Roman, serif" : "Segoe UI, Arial, sans-serif") << "\">\n";
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

    const int fsz = S_ ? 27 : 17;                                                   // label size: the derivation sheet scales the picture down, so its letters start larger
    std::vector<Label> labels;
    auto addLabel = [&](const std::string& text, Pt target, bool inside, Pt anchor, bool nearLabel = false, Pt off = Pt{0, 0}) {
        Label l;
        l.text = text; l.target = target; l.inside = inside; l.anchor = anchor; l.near = nearLabel; l.off = off;
        l.w = 0.5 * fsz * (double)visLen(text);
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
                if (o.names && !S_) addLabel(o.values && !S_ ? b.name + " (mu = " + num(b.mat.muK) + ")" : b.name, (a + c) * 0.5 + Pt{0, 0}, false, (a + c) * 0.5);
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
                if (S_ && b.isDynamic() && sym.body[i]) std::snprintf(txt, sizeof txt, "m_{%d}", sym.body[i]);
                else if (S_) std::snprintf(txt, sizeof txt, "%s", b.name.c_str());
                else if (o.values && b.isDynamic()) std::snprintf(txt, sizeof txt, "%s (%s kg)", b.name.c_str(), num(b.massKg).c_str());
                else std::snprintf(txt, sizeof txt, "%s", b.name.c_str());
                Pt c = S(b.pos);
                double bw, bh;
                if (b.shape.isCircle()) { bw = bh = 2 * b.shape.radius * scale; }
                else {
                    double lx = 1e30, hx = -1e30, ly = 1e30, hy = -1e30;
                    for (const Pt& q : pts) { lx = std::min(lx, q.x); hx = std::max(hx, q.x); ly = std::min(ly, q.y); hy = std::max(hy, q.y); }
                    bw = hx - lx; bh = hy - ly;
                }
                bool fits = bw > 0.5 * fsz * visLen(txt) + 24 && bh > 2 * fsz;
                if (!S_ || b.isDynamic()) addLabel(txt, c, fits, c);
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
            auto drawElem = [&](Pt A, Pt Bp) {
                Pt d = Bp - A;
                double L = len(d);
                if (L < 2) return;
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
            };
            Pt mid = (A + Bp) * 0.5;
            if (l.dist && l.kind != LinkKind::Rod) {                                  // a distribution: a row of elements spread along the span
                Pt A2 = S(worldOf(l.a, l.la2)), B2 = S(worldOf(l.b, l.lb2));
                const int ne = std::max(3, std::min(8, l.distN() / 3));
                for (int e = 0; e < ne; ++e) {
                    double t = (e + 0.5) / ne;
                    drawElem(A + (A2 - A) * t, Bp + (B2 - Bp) * t);
                }
                mid = (A + Bp + A2 + B2) * 0.25;
            } else {
                if (L < 2) continue;
                drawElem(A, Bp);
            }
            if (o.names) {
                const int j = sym.link[li];
                char sy[64];
                if (S_) {
                    if (l.kind == LinkKind::Spring) std::snprintf(sy, sizeof sy, l.dist ? (l.dist == 2 ? "k\xE2\x80\xB2_{%d}(s)" : "k\xE2\x80\xB2_{%d}") : "k_{%d}", j);
                    else if (l.kind == LinkKind::Damper) std::snprintf(sy, sizeof sy, l.dist ? (l.dist == 2 ? "c\xE2\x80\xB2_{%d}(s)" : "c\xE2\x80\xB2_{%d}") : "c_{%d}", j);
                    else std::snprintf(sy, sizeof sy, "L_{%d}", j);
                    Pt off{0, -24};
                    if (l.kind == LinkKind::Damper) off = Pt{0, 40};
                    const Pt dd = Bp - A;
                    const double dl = len(dd);
                    if (dl > 1 && std::fabs(dd.x) < 0.35 * dl) off = l.kind == LinkKind::Damper ? Pt{-40, 8} : Pt{40, 8};      // a vertical element: its labels go beside it
                    addLabel(sy, mid, false, mid, true, off);
                } else {
                    std::string t, v;
                    if (l.kind == LinkKind::Spring) { t = l.dist ? "Spring distribution" : "Spring"; v = "k = " + eng(l.k, l.dist ? "N/m per m" : "N/m"); }
                    else if (l.kind == LinkKind::Damper) { t = l.dist ? "Damper distribution" : "Damping element"; v = "c = " + eng(l.c, l.dist ? "N s/m per m" : "N s/m"); }
                    else { t = "Rigid rod"; v = "L = " + num(l.L0) + " m"; }
                    addLabel(o.values ? t + " (" + v + ")" : t, mid, false, mid);
                }
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
            if (o.names) {
                if (S_ && l.kind == LinkKind::Pin && l.kt != 0) { char sy[32]; std::snprintf(sy, sizeof sy, "k_{t,%d}", sym.link[li]); addLabel(sy, at, false, at, true, Pt{34, -26}); }
                else if (!S_) addLabel(l.kind == LinkKind::Pin ? "Pin support" : (l.kind == LinkKind::Weld ? "Fixed support" : "Roller support"), at, false, at);
            }
        } else if (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld) {            // a joint between two bodies
            Pt at = A;
            if (l.kind == LinkKind::Pin) svg << "<circle cx=\"" << num(at.x) << "\" cy=\"" << num(at.y) << "\" r=\"6\" fill=\"#fff\" stroke=\"" << ink << "\" stroke-width=\"2\"/>\n";
            else svg << "<rect x=\"" << num(at.x - 6) << "\" y=\"" << num(at.y - 6) << "\" width=\"12\" height=\"12\" fill=\"#222\"/>\n";
            if (o.names) {
                if (S_ && l.kind == LinkKind::Pin && l.kt != 0) { char sy[32]; std::snprintf(sy, sizeof sy, "k_{t,%d}", sym.link[li]); addLabel(sy, at, false, at, true, Pt{34, -26}); }
                else if (!S_) addLabel(l.kind == LinkKind::Pin ? "Pin joint" : "Fixed joint", at, false, at);
            }
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
            const int mi = sym.body[&b - &m.bodies[0]];
            auto arrow = [&](double deg, const char* label) {
                double a = deg * 3.14159265358979 / 180.0;
                Pt dir{std::cos(a), std::sin(a)};
                double rb = std::max(0.3, b.shape.boundingRadius()) * scale * 0.85;
                Pt head = c - dir * rb, tail = head - dir * 95;
                svg << "<line x1=\"" << num(tail.x) << "\" y1=\"" << num(tail.y) << "\" x2=\"" << num(head.x) << "\" y2=\"" << num(head.y) << "\" stroke=\"#c0392b\" stroke-width=\"3\" marker-end=\"url(#ahl)\"/>\n";
                if (o.names) {
                    if (S_) addLabel(label, tail + (head - tail) * 0.5, false, tail, true, Pt{0, -52});
                    else addLabel(label, tail + (head - tail) * 0.5, false, tail);
                }
            };
            if (b.loadN > 0 || b.loadTimeVar) {
                char t[96];
                if (S_) std::snprintf(t, sizeof t, b.loadTimeVar ? "F_{%d}(t)" : "F_{%d}", mi);
                else if (b.loadTimeVar) std::snprintf(t, sizeof t, "%s F(t)", b.loadPull ? "Pull" : "Push");
                else if (o.values) std::snprintf(t, sizeof t, "%s %.0f N (%.1f kg)", b.loadPull ? "Pull" : "Push", b.loadN, b.loadN / 9.80665);
                else std::snprintf(t, sizeof t, "%s", b.loadPull ? "Pull" : "Push");
                arrow(b.loadAngleDeg + (b.loadPull ? 180.0 : 0.0), t);
            }
            if (b.driveF > 0) {
                char t[64];
                if (S_) std::snprintf(t, sizeof t, "F_{0,%d} cos \xCF\x89t", mi);
                else std::snprintf(t, sizeof t, "Harmonic push");
                arrow(b.driveAngleDeg + 180.0, t);
            }
            if (b.momentN != 0 || b.momentTimeVar) {                                   // a curved arrow round the centre
                const double R = std::max(0.3, b.shape.boundingRadius()) * scale * 0.55 + 14;
                const bool cw = b.momentTimeVar || b.momentN > 0;
                Pt p0 = c + Pt{R * std::cos(-2.3), R * std::sin(-2.3)}, p1 = c + Pt{R * std::cos(0.7), R * std::sin(0.7)};
                svg << "<path d=\"M " << num(p0.x) << " " << num(p0.y) << " A " << num(R) << " " << num(R) << " 0 1 1 " << num(p1.x) << " " << num(p1.y)
                    << "\" fill=\"none\" stroke=\"#c0392b\" stroke-width=\"3\"" << (cw ? " marker-end=\"url(#ahl)\"" : " marker-start=\"url(#ahl)\"") << "/>\n";
                if (o.names) {
                    char t[96];
                    if (S_) std::snprintf(t, sizeof t, b.momentTimeVar ? "M_{%d}(t)" : "M_{%d}", mi);
                    else if (b.momentTimeVar) std::snprintf(t, sizeof t, "Moment M(t)");
                    else std::snprintf(t, sizeof t, o.values ? "Moment %.0f N m" : "Moment", std::fabs(b.momentN));
                    if (S_) addLabel(t, c + Pt{0, -R}, false, c, true, Pt{0, -12});
                    else addLabel(t, c + Pt{0, -R}, false, c + Pt{0, -R});
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
                    if (S_) std::snprintf(t, sizeof t, "q_{%d}(x)", mi);
                    else if (!o.values) std::snprintf(t, sizeof t, "Distributed load");
                    else if (b.dloadType == DL_Equation) std::snprintf(t, sizeof t, "Distributed load q(x) = %s", b.dloadExpr.c_str());
                    else std::snprintf(t, sizeof t, "Distributed load %.0f N/m%s", b.dloadN, b.dloadType == DL_Uniform ? "" : " (peak)");
                    if (S_) addLabel(t, env[env.size() / 2], false, env[env.size() / 2], true, Pt{0, -14});
                    else addLabel(t, env[env.size() / 2], false, env[env.size() / 2]);
                }
            }
        }
    }

    // ---- labels: inside shapes where they fit, otherwise in the margins with a leader line to the part
    const double midX = W / 2;
    std::vector<size_t> sideIdx[2];
    for (size_t i = 0; i < labels.size(); ++i) {
        Label& l = labels[i];
        if (l.inside || l.near) continue;
        l.side = l.target.x < midX ? -1 : 1;
        sideIdx[l.side < 0 ? 0 : 1].push_back(i);
    }
    for (int sd = 0; sd < 2; ++sd) {
        auto& v = sideIdx[sd];
        std::sort(v.begin(), v.end(), [&](size_t a, size_t b) { return labels[a].anchor.y < labels[b].anchor.y; });
        double lastY = -1e30;
        for (size_t i : v) {
            double y = std::max(labels[i].anchor.y, lastY + (S_ ? 44 : 30));                    // keep the labels apart
            labels[i].y = y;
            lastY = y;
        }
    }
    double Hf = H;                                                                  // the picture grows to hold the labels (many labels in a column need more height than the drawing)
    for (const Label& l : labels) {
        if (l.near) Hf = std::max(Hf, l.target.y + l.off.y + 36);
        else if (!l.inside) Hf = std::max(Hf, l.y + 28);
    }
    for (const Label& l : labels) {
        if (l.near) {
            svg << "<text x=\"" << num(l.target.x + l.off.x) << "\" y=\"" << num(l.target.y + l.off.y) << "\" font-size=\"" << fsz << "\" text-anchor=\"middle\" fill=\"#222\"" << (o.serif ? " font-style=\"italic\"" : "") << ">" << rich(l.text) << "</text>\n";
            continue;
        }
        if (l.inside) {
            svg << "<text x=\"" << num(l.anchor.x) << "\" y=\"" << num(l.anchor.y + 5) << "\" font-size=\"" << fsz << "\" text-anchor=\"middle\" fill=\"#222\"" << (o.serif ? " font-style=\"italic\"" : "") << ">" << rich(l.text) << "</text>\n";
            continue;
        }
        double tx = l.side < 0 ? 18 + l.w : W - 18 - l.w;                           // the text spans [18, 18 + w] on the left, [W - 18 - w, W - 18] on the right
        Pt from{l.side < 0 ? std::min(tx + 8, margin - 12) : std::max(tx - 8, W - margin + 12), l.y - 5};
        svg << "<line x1=\"" << num(from.x) << "\" y1=\"" << num(from.y) << "\" x2=\"" << num(l.target.x) << "\" y2=\"" << num(l.target.y) << "\" stroke=\"#222\" stroke-width=\"1.3\""
            << (o.arrowheads ? " marker-end=\"url(#ah)\"" : "") << "/>\n";
        const char* fs = o.serif ? " font-style=\"italic\"" : "";
        if (l.side < 0) svg << "<text x=\"" << num(18) << "\" y=\"" << num(l.y) << "\" font-size=\"" << fsz << "\" text-anchor=\"start\" fill=\"#222\"" << fs << ">" << rich(l.text) << "</text>\n";
        else svg << "<text x=\"" << num(W - 18) << "\" y=\"" << num(l.y) << "\" font-size=\"" << fsz << "\" text-anchor=\"end\" fill=\"#222\"" << fs << ">" << rich(l.text) << "</text>\n";
    }
    if (S_) {                                                                       // the coordinate axes (y points down) and gravity
        const Pt o0{W - 150, Hf - 100};
        svg << "<line x1=\"" << num(o0.x) << "\" y1=\"" << num(o0.y) << "\" x2=\"" << num(o0.x + 62) << "\" y2=\"" << num(o0.y) << "\" stroke=\"#222\" stroke-width=\"2\" marker-end=\"url(#ah)\"/>\n"
            << "<line x1=\"" << num(o0.x) << "\" y1=\"" << num(o0.y) << "\" x2=\"" << num(o0.x) << "\" y2=\"" << num(o0.y + 62) << "\" stroke=\"#222\" stroke-width=\"2\" marker-end=\"url(#ah)\"/>\n"
            << "<text x=\"" << num(o0.x + 68) << "\" y=\"" << num(o0.y + 6) << "\" font-size=\"17\" font-style=\"italic\" fill=\"#222\">x</text>\n"
            << "<text x=\"" << num(o0.x - 5) << "\" y=\"" << num(o0.y + 82) << "\" font-size=\"17\" font-style=\"italic\" fill=\"#222\">y</text>\n"
            << "<line x1=\"" << num(o0.x + 100) << "\" y1=\"" << num(o0.y) << "\" x2=\"" << num(o0.x + 100) << "\" y2=\"" << num(o0.y + 62) << "\" stroke=\"#222\" stroke-width=\"2\" stroke-dasharray=\"5 4\" marker-end=\"url(#ah)\"/>\n"
            << "<text x=\"" << num(o0.x + 92) << "\" y=\"" << num(o0.y + 82) << "\" font-size=\"17\" font-style=\"italic\" fill=\"#222\">g</text>\n";
    }
    svg << "</svg>\n";
    std::string text = svg.str();
    for (size_t at; (at = text.find("@@H@@")) != std::string::npos;) text.replace(at, 5, num(Hf));
    return text;
}

}  // namespace vlab
