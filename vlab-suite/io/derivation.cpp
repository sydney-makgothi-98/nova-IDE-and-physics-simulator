// derivation.cpp -- builds the system sheet (diagram + specifications + derivation) and renders it to JPG / PDF.
//
// The derivation is Lagrange's method written for planar rigid bodies in "maximal" coordinates (x, y, theta per body), which is the
// same formulation the engine integrates (core/src/world.cpp). Every number printed comes from the design the user drew.
#include "derivation.hpp"
#include "vlab/expr.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace vlab {

// =====================================================================================================================
//  math markup -> HTML
// =====================================================================================================================
namespace {

std::string hesc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

bool isFunc(const std::string& w) {
    static const std::set<std::string> f{"sin", "cos", "tan", "atan", "atan2", "ln", "log", "exp", "sgn", "min", "max", "diag", "det", "sinh", "cosh", "cot", "arg"};
    return f.count(w) > 0;
}

struct Parser {
    const std::string& s;
    size_t i = 0;
    explicit Parser(const std::string& str) : s(str) {}
    bool more() const { return i < s.size(); }
};

std::string seq(Parser& p, bool upright, bool untilBrace);

// text between the braces at p.i (which must be '{'): returns its raw text and moves past the closing brace
std::string rawGroup(Parser& p) {
    std::string out;
    if (!p.more() || p.s[p.i] != '{') return out;
    int depth = 0;
    for (; p.more(); ++p.i) {
        char c = p.s[p.i];
        if (c == '{') { if (depth++ == 0) continue; }
        else if (c == '}') { if (--depth == 0) { ++p.i; break; } }
        out += c;
    }
    return out;
}

std::string group(Parser& p, bool upright) {
    std::string raw = rawGroup(p);
    Parser q(raw);
    return seq(q, upright, false);
}

// One argument: a {group}, a \command (with its own letters), or a single character. Leading spaces are skipped.
std::string argOf(Parser& p, bool upright) {
    while (p.more() && p.s[p.i] == ' ') ++p.i;
    if (p.more() && p.s[p.i] == '{') return group(p, upright);
    if (!p.more()) return "";
    std::string one;
    if (p.s[p.i] == '\\') {
        one += p.s[p.i++];
        while (p.more() && std::isalpha((unsigned char)p.s[p.i])) one += p.s[p.i++];
    } else if (((unsigned char)p.s[p.i]) == 0xCE || ((unsigned char)p.s[p.i]) == 0xCF) {
        one = p.s.substr(p.i, 2);
        p.i += 2;
    } else {
        one = std::string(1, p.s[p.i++]);
    }
    Parser q(one);
    return seq(q, upright, false);
}

// \name symbols: Greek letters (italic) and operators (upright)
const char* greekOf(const std::string& n) {
    static const struct { const char* n; const char* g; } t[] = {
        {"alpha", "\xCE\xB1"}, {"beta", "\xCE\xB2"}, {"gamma", "\xCE\xB3"}, {"delta", "\xCE\xB4"}, {"epsilon", "\xCE\xB5"}, {"zeta", "\xCE\xB6"},
        {"theta", "\xCE\xB8"}, {"lambda", "\xCE\xBB"}, {"mu", "\xCE\xBC"}, {"xi", "\xCE\xBE"}, {"pi", "\xCF\x80"}, {"rho", "\xCF\x81"},
        {"sigma", "\xCF\x83"}, {"tau", "\xCF\x84"}, {"phi", "\xCF\x86"}, {"varphi", "\xCF\x86"}, {"omega", "\xCF\x89"}};
    for (auto& e : t) if (n == e.n) return e.g;
    return nullptr;
}
const char* opOf(const std::string& n) {
    static const struct { const char* n; const char* g; } t[] = {
        {"times", "\xC3\x97"}, {"cdot", "\xC2\xB7"}, {"ge", "\xE2\x89\xA5"}, {"le", "\xE2\x89\xA4"}, {"perp", "\xE2\x8A\xA5"}, {"to", "\xE2\x86\x92"},
        {"partial", "\xE2\x88\x82"}, {"infty", "\xE2\x88\x9E"}, {"Phi", "\xCE\xA6"}, {"Delta", "\xCE\x94"}, {"Omega", "\xCE\xA9"}, {"ell", "\xE2\x84\x93"}};
    for (auto& e : t) if (n == e.n) return e.g;
    return nullptr;
}

std::string seq(Parser& p, bool upright, bool untilBrace) {
    (void)untilBrace;
    std::string o;
    while (p.more()) {
        const unsigned char c = (unsigned char)p.s[p.i];
        if (c == '\\') {
            ++p.i;
            if (!p.more()) break;
            const char n = p.s[p.i];
            if (n == '(' || n == ')' || n == '[' || n == ']') { o += std::string("<span class=\"bd\">") + n + "</span>"; ++p.i; continue; }
            if (n == ',') { o += "<span class=\"th\"></span>"; ++p.i; continue; }
            if (n == ';') { o += "<span class=\"th w\"></span>"; ++p.i; continue; }
            if (n == '|') { o += "<span class=\"bd\">|</span>"; ++p.i; continue; }
            std::string cmd;
            while (p.more() && std::isalpha((unsigned char)p.s[p.i])) cmd += p.s[p.i++];
            if (cmd == "f" || cmd == "tfrac") { std::string a = argOf(p, upright), b = argOf(p, upright); o += "<span class=\"fr\"><span class=\"n\">" + a + "</span><span class=\"d\">" + b + "</span></span>"; }
            else if (cmd == "dot") o += "<span class=\"ac dot\">" + argOf(p, upright) + "</span>";
            else if (cmd == "ddot") o += "<span class=\"ac ddot\">" + argOf(p, upright) + "</span>";
            else if (cmd == "hat") o += "<span class=\"ac hat\">" + argOf(p, upright) + "</span>";
            else if (cmd == "vec") o += "<b class=\"vec\">" + argOf(p, upright) + "</b>";
            else if (cmd == "rm") o += "<span class=\"rm\">" + group(p, true) + "</span>";
            else if (cmd == "text") o += "<span class=\"rm\">" + hesc(rawGroup(p)) + "</span>";
            else if (cmd == "mathcal") { rawGroup(p); o += "<span class=\"rm\">\xE2\x84\x9B</span>"; }
            else if (cmd == "sqrt") o += "<span class=\"sq\">\xE2\x88\x9A<span class=\"rad\">" + group(p, upright) + "</span></span>";
            else if (cmd == "sin" || cmd == "cos" || cmd == "tan" || cmd == "ln" || cmd == "exp" || cmd == "min" || cmd == "max" || cmd == "arg") o += "<span class=\"rm\">" + cmd + "</span>";
            else if (const char* g = greekOf(cmd)) o += upright ? std::string(g) : std::string("<i>") + g + "</i>";
            else if (const char* op = opOf(cmd)) o += std::string("<span class=\"rm\">") + op + "</span>";
            else if (cmd == "sum" && (!p.more() || p.s[p.i] != '{')) o += "<span class=\"rm\">\xE2\x88\x91</span>";
            else if (cmd == "sum" || cmd == "int") {
                std::string lo = group(p, upright), hi = group(p, upright);
                o += std::string("<span class=\"big\"><span class=\"hi\">") + hi + "</span><span class=\"sg\">" + (cmd == "sum" ? "\xE2\x88\x91" : "\xE2\x88\xAB") + "</span><span class=\"lo\">" + lo + "</span></span>";
            } else if (cmd == "mat") {
                std::string raw = rawGroup(p);
                std::vector<std::vector<std::string>> rows(1);
                std::string cell;
                int depth = 0;
                auto flush = [&] { rows.back().push_back(cell); cell.clear(); };
                for (char ch : raw) {
                    if (ch == '{') ++depth;
                    if (ch == '}') --depth;
                    if (depth == 0 && ch == '&') flush();
                    else if (depth == 0 && ch == ';') { flush(); rows.emplace_back(); }
                    else cell += ch;
                }
                flush();
                o += "<span class=\"mat\"><span class=\"mb\"></span><span class=\"mg\">";
                for (auto& r : rows) {
                    o += "<span class=\"mr\">";
                    for (auto& cl : r) { Parser q(cl); o += "<span class=\"mc\">" + seq(q, upright, false) + "</span>"; }
                    o += "</span>";
                }
                o += "</span><span class=\"mb r\"></span></span>";
            }
            continue;
        }
        if (c == '_' || c == '^') {
            ++p.i;
            std::string a = argOf(p, upright);
            o += std::string(c == '_' ? "<sub>" : "<sup>") + a + (c == '_' ? "</sub>" : "</sup>");
            continue;
        }
        if (c == '{') { o += group(p, upright); continue; }
        if (c == '}') { ++p.i; continue; }
        if (std::isalpha(c)) {
            std::string w;
            while (p.more() && std::isalpha((unsigned char)p.s[p.i])) w += p.s[p.i++];
            if (upright || isFunc(w)) o += "<span class=\"rm\">" + w + "</span>";
            else o += "<i>" + w + "</i>";
            continue;
        }
        if (c == 0xCE || c == 0xCF) {                                              // Greek letters: italic
            std::string g = p.s.substr(p.i, 2);
            p.i += 2;
            o += upright ? g : "<i>" + g + "</i>";
            continue;
        }
        if (p.s.compare(p.i, 3, "\xE2\x84\x93") == 0) { o += upright ? "\xE2\x84\x93" : "<i>\xE2\x84\x93</i>"; p.i += 3; continue; }   // script l
        if (p.s.compare(p.i, 3, "==>") == 0) { o += "<span class=\"imp\">\xE2\x9F\xB9</span>"; p.i += 3; continue; }
        if (c == '-') { o += "\xE2\x88\x92"; ++p.i; continue; }
        if (c == '&') { o += "&amp;"; ++p.i; continue; }
        if (c == '<') { o += "&lt;"; ++p.i; continue; }
        if (c == '>') { o += "&gt;"; ++p.i; continue; }
        o += (char)c;
        ++p.i;
    }
    return o;
}

// plain text with $...$ pieces of math
std::string txt(const std::string& s) {
    std::string o, cur;
    bool math = false;
    for (char c : s) {
        if (c == '$') {
            o += math ? mathMarkup(cur) : hesc(cur);
            cur.clear();
            math = !math;
        } else cur += c;
    }
    o += math ? mathMarkup(cur) : hesc(cur);
    return o;
}

// A formula the user typed (3*t^2 + 2*sin(2*pi*t)) as typeset math: pi -> the letter, * -> a dot, ^ -> a superscript, function names upright.
std::string fmtExpr(const std::string& e) {
    std::string o;
    for (size_t i = 0; i < e.size();) {
        const char c = e[i];
        if (std::isalpha((unsigned char)c)) {
            size_t j = i;
            while (j < e.size() && std::isalpha((unsigned char)e[j])) ++j;
            const std::string w = e.substr(i, j - i);
            if (w == "pi") o += "\\pi ";
            else if (isFunc(w) || w == "sqrt" || w == "abs" || w == "asin" || w == "acos") o += "\\rm{" + w + "}";
            else o += w;                                                        // t, x, L stay italic variables
            i = j;
        } else if (c == '*') { o += "\\cdot "; ++i; }
        else if (c == '^') {
            ++i;
            std::string ex;
            if (i < e.size() && e[i] == '(') {
                int d = 0;
                size_t j = i;
                for (; j < e.size(); ++j) { if (e[j] == '(') ++d; else if (e[j] == ')' && --d == 0) { ++j; break; } }
                ex = e.substr(i + 1, j > i + 2 ? j - i - 2 : 0);
                i = j;
            } else {
                size_t j = i;
                if (j < e.size() && (e[j] == '-' || e[j] == '+')) ++j;
                while (j < e.size() && (std::isalnum((unsigned char)e[j]) || e[j] == '.')) ++j;
                ex = e.substr(i, j - i);
                i = j;
            }
            o += "^{" + fmtExpr(ex) + "}";
        } else if (c == '{' || c == '}' || c == '_' || c == '&' || c == ';') { ++i; }
        else { o += c; ++i; }
    }
    return o;
}

std::string fnum(double v) {                                                  // 4 significant digits; powers of ten for the very large and small
    if (v == 0 || std::fabs(v) < 1e-12) return "0";
    char b[48];
    const double a = std::fabs(v);
    if (a >= 1e5 || a < 1e-3) {
        int e = (int)std::floor(std::log10(a));
        double mant = v / std::pow(10.0, e);
        std::snprintf(b, sizeof b, "%.3g", mant);
        return std::string(b) + "\xC3\x97" "10^{" + std::to_string(e) + "}";
    }
    if (a >= 1e4) std::snprintf(b, sizeof b, "%.0f", v);                         // never "4.5e+04"
    else std::snprintf(b, sizeof b, "%.4g", v);
    return b;
}

std::string I(int v) { return std::to_string(v); }

double combine(Combine c, double a, double b) {
    switch (c) {
        case Combine::Average: return 0.5 * (a + b);
        case Combine::Min: return std::min(a, b);
        case Combine::Max: return std::max(a, b);
        default: return std::sqrt(std::max(0.0, a) * std::max(0.0, b));
    }
}

// =====================================================================================================================
//  the sheet
// =====================================================================================================================
const char* kCss = R"X(
*{box-sizing:border-box}
html,body{margin:0;background:#fff}
body{font-family:"Cambria Math","STIX Two Text","Latin Modern Roman","Times New Roman",serif;color:#1a1a1a;font-size:21px;line-height:1.5}
.page{width:1000px;margin:0 auto;padding:34px 36px 40px}
h1{font-size:23px;font-weight:600;margin:0 0 6px;font-family:"Segoe UI",Arial,sans-serif;color:#222}
h2{font-size:31px;font-weight:400;color:#9b2220;margin:30px 0 10px;break-after:avoid}
.fig{text-align:center;margin:6px 0 4px}
.fig svg{max-width:100%;height:auto}
.specs{display:grid;grid-template-columns:1fr 1fr;gap:3px 34px;margin:4px 0 6px}
.spec{display:flex;gap:14px;align-items:baseline;padding:2px 0;border-bottom:1px dotted #cfcfcf;break-inside:avoid}
.spec .v{white-space:nowrap}
.spec .t{color:#555;font-size:16.5px;font-family:"Segoe UI",Arial,sans-serif;margin-left:auto;text-align:right}
.note{font-size:18px;color:#333;margin:4px 0}
.ln{margin:13px 0 13px 0;font-size:22px;line-height:2.1;break-inside:avoid}
.ln.txt{font-size:20px;line-height:1.6;margin:16px 0 4px}
.ln .lab{color:#9b2220;margin-right:.6em}
.ln.final{margin:22px 0 14px;padding:10px 14px;border:1.5px solid #333;display:inline-block}
.ln.under{border-bottom:2px solid #333;display:inline-block}
i{font-style:italic}
.rm{font-style:normal}
sub,sup{font-size:.68em;line-height:0}
.fr{display:inline-flex;flex-direction:column;vertical-align:middle;text-align:center;margin:0 .15em;line-height:1.35}
.fr .n{border-bottom:1.2px solid #222;padding:0 .2em .08em}
.fr .d{padding:.08em .2em 0}
.imp{margin:0 .75em;font-style:normal}
.bd{font-size:2.05em;line-height:0;vertical-align:-.19em;font-weight:200}
.th{display:inline-block;width:.18em}.th.w{width:.5em}
.ac{position:relative;display:inline-block}
.ac.dot::before{content:"\02D9";position:absolute;left:68%;top:-.4em;transform:translateX(-50%);font-style:normal}
.ac.ddot::before{content:"\00A8";position:absolute;left:58%;top:-.36em;transform:translateX(-50%);font-style:normal}
.ac.hat::before{content:"\02C6";position:absolute;left:62%;top:-.42em;transform:translateX(-50%);font-style:normal}
b.vec{font-weight:700;font-style:italic}
b.vec i{font-weight:700}
.big{display:inline-flex;flex-direction:column;align-items:center;vertical-align:middle;line-height:1.1;margin:0 .2em}
.big .sg{font-size:1.9em;line-height:1}.big .hi,.big .lo{font-size:.7em}
.mat{display:inline-flex;vertical-align:middle;align-items:stretch;margin:0 .15em}
.mat .mb{width:.38em;border:1.4px solid #222;border-right:none}.mat .mb.r{border:1.4px solid #222;border-left:none}
.mat .mg{display:flex;flex-direction:column;padding:.05em .3em}
.mat .mr{display:flex;gap:1.1em;justify-content:center}.mat .mc{min-width:1em;text-align:center}
.sq{white-space:nowrap}.sq .rad{border-top:1.2px solid #222;padding:0 .1em;margin-left:-.05em}
.ref{font-size:15px;color:#666;font-family:"Segoe UI",Arial,sans-serif;margin-top:22px;border-top:1px solid #ddd;padding-top:6px}
@media print{ .page{zoom:.66;margin:0} @page{size:A4;margin:11mm} }
)X";

}  // namespace

std::string mathMarkup(const std::string& s) {
    Parser p(s);
    return seq(p, false, false);
}

Sheet buildSheet(const Model& m0, SheetKind kind, const SchematicOptions& diagram) {
    Sheet out;
    Model m = m0;
    m.updateMass();
    m.refreshAutoRest();
    const SymbolMap sym = makeSymbols(m);

    auto parseSize = [](const std::string& svg, double& w, double& h) {
        w = 1320; h = 700;
        size_t a = svg.find("width=\""), b = svg.find("height=\"");
        if (a != std::string::npos) w = std::atof(svg.c_str() + a + 7);
        if (b != std::string::npos) h = std::atof(svg.c_str() + b + 8);
    };

    // ------------------------------------------------------------------ the picture only
    if (kind == SheetKind::SystemOnly) {
        SchematicOptions o = diagram;
        o.symbols = false;
        if (o.title.empty()) o.title = m.name;
        const std::string svg = schematicSvgString(m, o);
        double w, h;
        parseSize(svg, w, h);
        std::ostringstream h2;
        h2 << "<!doctype html><html><head><meta charset=\"utf-8\"><title>" << hesc(m.name) << "</title><style>@page{size:" << (int)w + 2 << "px " << (int)h + 2
           << "px;margin:0}html,body{margin:0;background:#fff;overflow:hidden}svg{display:block}</style></head><body>" << svg
           << "<script>window.onload=function(){document.body.setAttribute('data-h',Math.ceil(document.documentElement.scrollHeight))}</script></body></html>";
        out.html = h2.str();
        out.width = (int)w + 2;
        return out;
    }

    // ------------------------------------------------------------------ the full sheet
    SchematicOptions o;
    o.symbols = true;
    o.serif = true;
    o.names = true;
    o.loads = true;
    o.values = false;
    o.title = "";
    const std::string svg = schematicSvgString(m, o);

    std::ostringstream H;
    auto line = [&](const std::string& markup, const char* cls = "") { H << "<div class=\"ln " << cls << "\">" << mathMarkup(markup) << "</div>\n"; };
    auto text = [&](const std::string& t) { H << "<div class=\"ln txt\">" << txt(t) << "</div>\n"; };
    auto heading = [&](const std::string& t) { H << "<h2>" << hesc(t) << "</h2>\n"; };

    H << "<!doctype html><html><head><meta charset=\"utf-8\"><title>" << hesc(m.name) << " - governing equations</title><style>" << kCss << "</style></head><body><div class=\"page\">\n";
    H << "<h1>" << hesc(m.name) << "</h1>\n";
    H << "<div class=\"fig\">" << svg << "</div>\n";

    // ------------------------------------------------------------------ collect the facts
    std::vector<int> dyn;                                  // indices of moving bodies, in symbol order
    for (size_t i = 0; i < m.bodies.size(); ++i) if (m.bodies[i].isDynamic()) dyn.push_back((int)i);
    auto mi = [&](int body) { return sym.body[body]; };
    auto bname = [&](int body) { return m.bodies[body].name; };
    auto rotFree = [&](int body) { return m.bodies[body].isDynamic() && !m.bodies[body].lockRot; };

    // ------------------------------------------------------------------ SPECIFICATIONS (where the problem statement used to be)
    struct Spec { std::string sym, val, what; };
    std::vector<Spec> specs;
    auto add = [&](const std::string& s, const std::string& v, const std::string& w) { specs.push_back({s, v, w}); };
    {
        add("g", std::string("= ") + fnum(m.env.gravity) + "\\,\\rm{m/s}^{2}", "gravity (acts in +y, downward)");
        for (int bi : dyn) {
            const Body& b2 = m.bodies[bi];
            const std::string k = I(mi(bi));
            add("m_{" + k + "}", "= " + fnum(b2.massKg) + "\\,\\rm{kg}", bname(bi));
            if (!b2.lockRot) add("J_{" + k + "}", "= " + fnum(b2.inertia) + "\\,\\rm{kg\\,m}^{2}", "moment of inertia about the centre of mass");
            std::string fixedSym;
            if (b2.lockX) fixedSym += "x_{" + k + "}";
            if (b2.lockY) fixedSym += std::string(fixedSym.empty() ? "" : ",\\,") + "y_{" + k + "}";
            if (b2.lockRot) fixedSym += std::string(fixedSym.empty() ? "" : ",\\,") + "\\theta_{" + k + "}";
            if (!fixedSym.empty()) add(fixedSym, "\\rm{\\ held\\ fixed}", bname(bi) + ": rail or no spin");
            if (b2.vel.x != 0 || b2.vel.y != 0 || b2.omega != 0) {
                std::string v = "= (" + fnum(b2.vel.x) + ", " + fnum(b2.vel.y) + ", " + fnum(b2.omega) + ")";
                add("(\\dot x_{" + k + "},\\dot y_{" + k + "},\\dot \\theta_{" + k + "})(0)", v, "initial velocity of " + bname(bi));
            }
        }
        for (size_t j = 0; j < m.links.size(); ++j) {
            const Link& l = m.links[j];
            const std::string k = I(sym.link[j]);
            const std::string between = bname(l.a) + " and " + bname(l.b);
            if (l.kind == LinkKind::Spring && !l.dist) {
                add("k_{" + k + "}", "= " + fnum(l.k) + "\\,\\rm{N/m}", "spring between " + between);
                add("L_{0," + k + "}", "= " + fnum(l.L0) + "\\,\\rm{m}", l.autoRest ? "its rest length (the drawn length)" : "its rest length (set by hand)");
                if (l.k3 != 0) add("k_{3," + k + "}", "= " + fnum(l.k3) + "\\,\\rm{N/m}^{3}", "cubic stiffness");
            } else if (l.kind == LinkKind::Spring) {
                add("k\xE2\x80\xB2_{" + k + "}", "= " + fnum(l.k) + "\\,\\rm{N/m\\,per\\,m}" + (l.dist == 2 ? "" : ""), std::string(l.dist == 2 ? "linear" : "constant") + " spring distribution, " + between);
                if (l.dist == 2) add("\\mu_{k" + k + "}", "= " + fnum(l.distM) + "\\,\\rm{N/m\\,per\\,m}^{2}", "slope: k\xE2\x80\xB2(s) = k\xE2\x80\xB2_0 + \xCE\xBC s");
                add("S_{" + k + "},\\,N_{" + k + "}", "= " + fnum(l.distSpan()) + "\\,\\rm{m},\\ " + I(l.distN()), "span and number of elements");
            } else if (l.kind == LinkKind::Damper && !l.dist) {
                add("c_{" + k + "}", "= " + fnum(l.c) + "\\,\\rm{N\\,s/m}", "damper between " + between);
            } else if (l.kind == LinkKind::Damper) {
                add("c\xE2\x80\xB2_{" + k + "}", "= " + fnum(l.c) + "\\,\\rm{N\\,s/m\\,per\\,m}", std::string(l.dist == 2 ? "linear" : "constant") + " damper distribution, " + between);
                if (l.dist == 2) add("\\mu_{c" + k + "}", "= " + fnum(l.distM), "slope of c\xE2\x80\xB2(s) per metre");
                add("S_{" + k + "},\\,N_{" + k + "}", "= " + fnum(l.distSpan()) + "\\,\\rm{m},\\ " + I(l.distN()), "span and number of elements");
            } else if (l.kind == LinkKind::Rod) {
                add("L_{" + k + "}", "= " + fnum(l.L0) + "\\,\\rm{m}", "rigid rod between " + between);
            } else if (l.kind == LinkKind::Pin && l.kt != 0) {
                add("k_{t," + k + "}", "= " + fnum(l.kt) + "\\,\\rm{N\\,m/rad}", "torsional spring at the pin, " + between);
                add("\\theta_{0," + k + "}", "= " + fnum(l.refAngle) + "\\,\\rm{rad}", "its rest angle (the drawn angle)");
            } else if (l.kind == LinkKind::Pin && l.jointC != 0) {
                add("c_{\\theta," + k + "}", "= " + fnum(l.jointC) + "\\,\\rm{N\\,m\\,s/rad}", "rotary damping at the pin");
            }
        }
        for (int bi : dyn) {
            const Body& b2 = m.bodies[bi];
            const std::string k = I(mi(bi));
            const double alpha = b2.loadAngleDeg + (b2.loadPull ? 180.0 : 0.0);
            if (b2.loadTimeVar) {
                add("F_{" + k + "}(t)", "= " + fmtExpr(b2.loadExpr) + "\\,\\rm{N}", std::string(b2.loadPull ? "pull" : "push") + " on " + bname(bi) + " (t in seconds)");
                add("\\alpha_{" + k + "}", "= " + fnum(alpha) + "\xC2\xB0", "direction it acts in (0\xC2\xB0 right, 90\xC2\xB0 down)");
            } else if (b2.loadN > 0) {
                add("F_{" + k + "}", "= " + fnum(b2.loadN) + "\\,\\rm{N}", std::string(b2.loadPull ? "pull" : "push") + " on " + bname(bi));
                add("\\alpha_{" + k + "}", "= " + fnum(alpha) + "\xC2\xB0", "direction it acts in (0\xC2\xB0 right, 90\xC2\xB0 down)");
            }
            if (b2.momentTimeVar) add("M_{" + k + "}(t)", "= " + fmtExpr(b2.momentExpr) + "\\,\\rm{N\\,m}", "moment on " + bname(bi) + " (clockwise positive)");
            else if (b2.momentN != 0) add("M_{" + k + "}", "= " + fnum(b2.momentN) + "\\,\\rm{N\\,m}", "moment on " + bname(bi) + " (clockwise positive)");
            if (b2.driveF > 0) {
                if (b2.driveT > 0) {
                    add("F_{0," + k + "},\\,T_{" + k + "}", "= " + fnum(b2.driveF) + "\\,\\rm{N},\\ " + fnum(b2.driveT) + "\\,\\rm{s}", "half-sine pulse on " + bname(bi));
                } else {
                    add("F_{0," + k + "},\\,\\omega_{" + k + "}", "= " + fnum(b2.driveF) + "\\,\\rm{N},\\ " + fnum(b2.driveW) + "\\,\\rm{rad/s}", "harmonic push on " + bname(bi));
                }
                add("\\beta_{" + k + "}", "= " + fnum(b2.driveAngleDeg) + "\xC2\xB0", "direction of that push");
            }
            if (b2.dloadType != 0 && b2.shape.preset == ShapePreset::Box) {
                if (b2.dloadType == DL_Equation) add("q_{" + k + "}(x)", "= " + fmtExpr(b2.dloadExpr) + "\\,\\rm{N/m}", "distributed load on " + bname(bi));
                else add("q_{" + k + "}", "= " + fnum(b2.dloadN) + "\\,\\rm{N/m}", "peak of the distributed load on " + bname(bi));
            }
        }
        // surfaces
        const bool contacts = m.env.collisions;
        if (contacts) {
            for (size_t i = 0; i < m.bodies.size(); ++i) {
                const Body& s = m.bodies[i];
                if (s.kind != BodyKind::Static || !s.collides()) continue;
                const std::string nm = bname((int)i);
                add("\\mu_{s}, \\mu_{k}, e", "= " + fnum(s.mat.muS) + ", " + fnum(s.mat.muK) + ", " + fnum(s.mat.restitution), "friction and restitution of " + nm);
            }
        }
        if (m.env.airDrag) add("c_{a1},\\,c_{a2}", "= " + fnum(m.env.drag1) + ",\\ " + fnum(m.env.drag2), "air drag F = \xE2\x88\x92(c_a1 + c_a2 |v|) v");
    }
    heading("Specifications:");
    H << "<div class=\"specs\">\n";
    for (const Spec& s : specs) {
        H << "<div class=\"spec\">";
        if (!s.sym.empty()) H << "<span class=\"v\">" << mathMarkup(s.sym + " " + s.val) << "</span>";
        H << "<span class=\"t\">" << hesc(s.what) << "</span></div>\n";
    }
    H << "</div>\n";

    if (dyn.empty()) {
        text("The design has no moving bodies, so there are no equations of motion to derive.");
        H << "</div><script>window.onload=function(){document.body.setAttribute('data-h',Math.ceil(document.documentElement.scrollHeight))}</script></body></html>";
        out.html = H.str();
        return out;
    }

    // ------------------------------------------------------------------ SOLUTION
    // geometry helpers --------------------------------------------------------------------------------------------
    struct End { bool dyn; int body; int k; bool rotf; Vec2 sRot, local, w0; };
    auto endOf = [&](int body, Vec2 local) {
        End e;
        const Body& b = m.bodies[body];
        e.dyn = b.isDynamic();
        e.body = body;
        e.k = mi(body);
        e.rotf = rotFree(body);
        e.local = local;
        e.sRot = Rot(b.angle).apply(local);
        e.w0 = b.pos + e.sRot;
        return e;
    };
    auto vecNum = [&](Vec2 v) { return "\\mat{" + fnum(v.x) + ";" + fnum(v.y) + "}"; };
    auto pExpr = [&](const End& e, const std::string& tag) {
        if (!e.dyn) return vecNum(e.w0);
        if (e.rotf) return "\\vec{r}_{" + I(e.k) + "} + R(\\theta_{" + I(e.k) + "})\\,\\vec{s}_{" + tag + "}";
        return "\\vec{r}_{" + I(e.k) + "} + " + vecNum(e.sRot);
    };
    auto sNum = [&](const End& e, const std::string& tag) {
        return "\\vec{s}_{" + tag + "} = " + vecNum(e.local);
    };
    auto rhoName = [&](const std::string& tag) { return "\\vec{\\rho}_{" + tag + "}"; };

    // which elements act on a body --------------------------------------------------------------------------------
    bool anyJoint = false, anyContactPossible = false, anyTorsion = false, anyDist = false, anyDamper = false;
    for (const Link& l : m.links) {
        if (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld || l.kind == LinkKind::Roller || l.kind == LinkKind::Rod) anyJoint = true;
        if (l.kind == LinkKind::Pin && l.kt != 0) anyTorsion = true;
        if (l.dist) anyDist = true;
        if (l.kind == LinkKind::Damper || (l.kind == LinkKind::Pin && l.jointC != 0)) anyDamper = true;
    }
    if (m.env.collisions) {
        for (int bi : dyn) if (m.bodies[bi].collides()) {
            for (size_t o2 = 0; o2 < m.bodies.size(); ++o2)
                if ((int)o2 != bi && m.bodies[o2].collides()) anyContactPossible = true;
        }
    }
    if (!m.env.damping) anyDamper = false;
    bool anySpring = false;
    for (const Link& l : m.links) if (l.kind == LinkKind::Spring) anySpring = true;

    // 1. assumptions and coordinates ------------------------------------------------------------------------------
    heading("Solution:");
    {
        std::string a = "Assumptions: planar motion; rigid bodies; y measured downward and angles clockwise; springs follow F = k\xCE\xB4";
        if (anySpring) { for (const Link& l : m.links) if (l.kind == LinkKind::Spring && l.k3 != 0) { a += " + k\xE2\x82\x83\xCE\xB4\xC2\xB3"; break; } }
        if (anyDamper) a += ", dampers F = c \xCE\xB4\xCC\x87";
        if (anyContactPossible) a += "; Coulomb friction and Newton impact at contacts";
        a += ".";
        H << "<div class=\"ln txt\">" << hesc(a) << "</div>\n";
    }
    {
        std::string q, free;
        for (int bi : dyn) {
            const Body& b = m.bodies[bi];
            const std::string k = I(mi(bi));
            if (!b.lockX) { q += (q.empty() ? "" : ";") + std::string("x_{") + k + "}"; }
            if (!b.lockY) { q += (q.empty() ? "" : ";") + std::string("y_{") + k + "}"; }
            if (!b.lockRot) { q += (q.empty() ? "" : ";") + std::string("\\theta_{") + k + "}"; }
        }
        text("Generalised coordinates: the free position and angle of every moving body, with each position written $\\vec{r}_{i} = (x_{i}, y_{i})$.");
        if (q.empty()) q = "\\ ";
        const int ndof = (int)std::count(q.begin(), q.end(), ';') + 1;
        line("\\vec{q} = \\mat{" + q + "}, ==> \\rm{" + I(ndof) + (ndof == 1 ? "\\ degree\\ of\\ freedom}" : "\\ degrees\\ of\\ freedom}"));
        text("An attachment point on body $i$ sits at $\\vec{p} = \\vec{r}_{i} + R(\\theta_{i})\\,\\vec{s}$, where $\\vec{s}$ is its fixed position in the body and $R$ is the rotation:");
        line("R(\\theta) = \\mat{\\cos\\theta & -\\sin\\theta; \\sin\\theta & \\cos\\theta}, \\; \\vec{\\rho} = \\vec{p} - \\vec{r}_{i}, \\; \\vec{v}_{p} = \\dot{\\vec{r}}_{i} + \\dot\\theta_{i}\\,\\vec{\\rho}^{\\,\\perp}, \\; \\vec{\\rho}^{\\,\\perp} = \\mat{-\\rho_{y};\\rho_{x}}, \\; \\vec{a}\\times\\vec{b} = a_{x}b_{y} - a_{y}b_{x}");
    }

    // 2. kinematics of every element -----------------------------------------------------------------------------
    struct Elem { size_t li; End A, B; std::string tag; };
    std::vector<Elem> elems;                                  // springs, dampers, rods and torsion pins
    for (size_t j = 0; j < m.links.size(); ++j) {
        const Link& l = m.links[j];
        std::string tag;
        const std::string k = I(sym.link[j]);
        switch (l.kind) {
            case LinkKind::Spring: tag = (l.dist ? "d" : "k") + k; break;
            case LinkKind::Damper: tag = (l.dist ? "e" : "c") + k; break;
            case LinkKind::Rod: tag = "L" + k; break;
            case LinkKind::Pin: tag = (l.kt != 0 ? "t" : "J") + k; break;
            default: tag = "J" + k; break;
        }
        elems.push_back({j, endOf(l.a, l.la), endOf(l.b, l.lb), tag});
    }
    bool shownGeom = false;
    for (const Elem& e : elems) {
        const Link& l = m.links[e.li];
        if (!(l.kind == LinkKind::Spring || l.kind == LinkKind::Damper)) continue;
        if (!shownGeom) { text("Geometry of the springs and dampers (the points they attach to, and the length and direction between them):"); shownGeom = true; }
        std::string nm = (l.kind == LinkKind::Spring ? "spring " : "damper ") + std::string(l.dist ? "distribution " : "") + I(sym.link[e.li]);
        H << "<div class=\"ln txt\" style=\"margin:8px 0 0\">" << hesc(nm) << ":</div>\n";
        line("\\vec{p}_{" + e.tag + ",A} = " + pExpr(e.A, e.tag + ",A") + ", \\;\\; \\vec{p}_{" + e.tag + ",B} = " + pExpr(e.B, e.tag + ",B"));
        if (e.A.dyn && e.A.rotf) line(sNum(e.A, e.tag + ",A"));
        if (e.B.dyn && e.B.rotf) line(sNum(e.B, e.tag + ",B"));
        if (l.dist) {
            const End A2 = endOf(l.a, l.la2), B2 = endOf(l.b, l.lb2);
            text("The distribution is cut into $N$ = " + I(l.distN()) + " short elements along its span $S$ = " + fnum(l.distSpan()) + " m (exact for a constant or straight-line distribution); element $e$ sits a fraction $\\sigma_{e} = (e - \\tfrac12)/N$ of the way along, where the far ends of the span are:");
            line("\\vec{s}_{" + e.tag + ",A}(\\sigma) = \\vec{s}_{" + e.tag + ",A} + \\sigma\\,(" + vecNum(A2.local) + " - \\vec{s}_{" + e.tag + ",A}), \\;\\; \\vec{s}_{" + e.tag + ",B}(\\sigma) \\text{ likewise}");
        }
        line("\\ell_{" + e.tag + "} = |\\vec{p}_{" + e.tag + ",B} - \\vec{p}_{" + e.tag + ",A}|, \\;\\; \\hat{n}_{" + e.tag + "} = \\f{\\vec{p}_{" + e.tag + ",B} - \\vec{p}_{" + e.tag + ",A}}{\\ell_{" + e.tag + "}}, \\;\\; \\dot\\ell_{" + e.tag + "} = \\hat{n}_{" + e.tag + "}\\cdot(\\vec{v}_{" + e.tag + ",B} - \\vec{v}_{" + e.tag + ",A})");
    }

    // 3. energies --------------------------------------------------------------------------------------------------
    text("Energy of the system: kinetic $T$, potential $V$ (gravity and the springs), and the Rayleigh dissipation function $\\mathcal{R}$ of the dampers.");
    {
        std::string T = "T = ", gl;
        bool first = true;
        for (int bi : dyn) {
            const Body& b = m.bodies[bi];
            const std::string k = I(mi(bi));
            std::string tr;
            if (!b.lockX && !b.lockY) tr = "\\dot x_{" + k + "}^{2} + \\dot y_{" + k + "}^{2}";
            else if (!b.lockX) tr = "\\dot x_{" + k + "}^{2}";
            else if (!b.lockY) tr = "\\dot y_{" + k + "}^{2}";
            if (!tr.empty()) { T += std::string(first ? "" : " + ") + "\\f{1}{2}m_{" + k + "}(" + tr + ")"; first = false; }
            if (!b.lockRot) { T += std::string(first ? "" : " + ") + "\\f{1}{2}J_{" + k + "}\\dot\\theta_{" + k + "}^{2}"; first = false; }
            if (!b.lockY) gl += (gl.empty() ? "" : " - ") + std::string("m_{") + k + "}\\,g\\,y_{" + k + "}";
        }
        if (first) T += "0";
        line(T);
        std::string V = "V = ";
        bool vf = true;
        if (!gl.empty()) { V += "-" + gl; vf = false; }
        for (const Elem& e : elems) {
            const Link& l = m.links[e.li];
            if (l.kind == LinkKind::Spring && !l.dist) {
                V += std::string(vf ? "" : " + ") + "\\f{1}{2}k_{" + I(sym.link[e.li]) + "}(\\ell_{" + e.tag + "} - L_{0," + I(sym.link[e.li]) + "})^{2}";
                if (l.k3 != 0) V += " + \\f{1}{4}k_{3," + I(sym.link[e.li]) + "}(\\ell_{" + e.tag + "} - L_{0," + I(sym.link[e.li]) + "})^{4}";
                vf = false;
            } else if (l.kind == LinkKind::Spring) {
                V += std::string(vf ? "" : " + ") + "\\sum{e=1}{N}\\f{1}{2}k_{e}(\\ell_{" + e.tag + ",e} - L_{0,e})^{2}";
                vf = false;
            } else if (l.kind == LinkKind::Pin && l.kt != 0) {
                V += std::string(vf ? "" : " + ") + "\\f{1}{2}k_{t," + I(sym.link[e.li]) + "}(\\theta_{B} - \\theta_{A} - \\theta_{0," + I(sym.link[e.li]) + "})^{2}";
                vf = false;
            }
        }
        if (vf) V += "0";
        line(V);
        if (anyDist) line("k_{e} = k\xE2\x80\xB2(s_{e})\\,\\f{S}{N}, \\;\\; s_{e} = \\sigma_{e}S, \\;\\; k\xE2\x80\xB2(s) = k\xE2\x80\xB2_{0} + \\mu\\,s \\;\\;(\\mu = 0\\ \\rm{for\\ a\\ constant\\ distribution}), \\;\\; L_{0,e} = \\ell_{e}(0)");
        std::string R = "\\mathcal{R} = ";
        bool rf = true;
        if (m.env.damping) {
            for (const Elem& e : elems) {
                const Link& l = m.links[e.li];
                if (l.kind == LinkKind::Damper && !l.dist) { R += std::string(rf ? "" : " + ") + "\\f{1}{2}c_{" + I(sym.link[e.li]) + "}\\dot\\ell_{" + e.tag + "}^{2}"; rf = false; }
                else if (l.kind == LinkKind::Damper) { R += std::string(rf ? "" : " + ") + "\\sum{e=1}{N}\\f{1}{2}c_{e}\\dot\\ell_{" + e.tag + ",e}^{2}"; rf = false; }
                else if (l.kind == LinkKind::Pin && l.jointC != 0) { R += std::string(rf ? "" : " + ") + "\\f{1}{2}c_{\\theta," + I(sym.link[e.li]) + "}(\\dot\\theta_{B} - \\dot\\theta_{A})^{2}"; rf = false; }
            }
        }
        if (rf) R += "0";
        line(R);
    }
    // generalised forces
    {
        bool anyQ = false;
        for (int bi : dyn) {
            const Body& b = m.bodies[bi];
            if (b.loadN > 0 || b.loadTimeVar || b.momentN != 0 || b.momentTimeVar || b.driveF > 0 || b.dloadType != 0) anyQ = true;
        }
        if (anyQ || m.env.airDrag) text("Everything that is not a spring, damper or gravity enters as a generalised force $Q_{k}$, the work of the applied loads per unit change of coordinate $q_{k}$.");
    }

    // 4. Lagrange's equations -----------------------------------------------------------------------------------------
    text("Lagrange's equation for every free coordinate $q_{k}$:");
    line("\\f{\\rm{d}}{\\rm{d}t}\\\\(\\f{\xE2\x88\x82T}{\xE2\x88\x82\\dot q_{k}}\\\\) - \\f{\xE2\x88\x82T}{\xE2\x88\x82 q_{k}} + \\f{\xE2\x88\x82V}{\xE2\x88\x82 q_{k}} + \\f{\xE2\x88\x82\\mathcal{R}}{\xE2\x88\x82\\dot q_{k}} = Q_{k}");
    // (the mathcal macro is plain italic: replace by R)
    text("The kinetic energy depends only on the velocities, so its two terms reduce to mass times acceleration:");
    {
        std::string s = "";
        for (int bi : dyn) {
            const Body& b = m.bodies[bi];
            const std::string k = I(mi(bi));
            if (!b.lockX) s += (s.empty() ? "" : ",\\;\\; ") + std::string("\\f{\\rm{d}}{\\rm{d}t}\\\\(\\f{\xE2\x88\x82T}{\xE2\x88\x82\\dot x_{") + k + "}}\\\\) = m_{" + k + "}\\ddot x_{" + k + "}";
            if (!b.lockY) s += (s.empty() ? "" : ",\\;\\; ") + std::string("\\f{\\rm{d}}{\\rm{d}t}\\\\(\\f{\xE2\x88\x82T}{\xE2\x88\x82\\dot y_{") + k + "}}\\\\) = m_{" + k + "}\\ddot y_{" + k + "}";
            if (!b.lockRot) s += (s.empty() ? "" : ",\\;\\; ") + std::string("\\f{\\rm{d}}{\\rm{d}t}\\\\(\\f{\xE2\x88\x82T}{\xE2\x88\x82\\dot \\theta_{") + k + "}}\\\\) = J_{" + k + "}\\ddot \\theta_{" + k + "}";
            if (bi != dyn.back() && !s.empty()) { line(s); s.clear(); }
        }
        if (!s.empty()) line(s);
    }
    if (anySpring || anyDamper || anyTorsion) {
        text("Springs and dampers act along the line between their two attachment points. Because $\\ell$ changes with a coordinate only through the points that move,");
        line("\\f{\xE2\x88\x82\\ell}{\xE2\x88\x82\\vec{r}_{A}} = -\\hat{n}, \\;\\; \\f{\xE2\x88\x82\\ell}{\xE2\x88\x82\\vec{r}_{B}} = +\\hat{n}, \\;\\; \\f{\xE2\x88\x82\\ell}{\xE2\x88\x82\\theta_{A}} = -\\vec{\\rho}_{A}\\times\\hat{n}, \\;\\; \\f{\xE2\x88\x82\\ell}{\xE2\x88\x82\\theta_{B}} = +\\vec{\\rho}_{B}\\times\\hat{n}");
        if (anySpring) line("\\f{\xE2\x88\x82V}{\xE2\x88\x82 q} = k\\,(\\ell - L_{0})\\,\\f{\xE2\x88\x82\\ell}{\xE2\x88\x82 q} \\;\\;==>\\;\\; f_{s} = k\\,(\\ell - L_{0}), \\;\\; \\rm{force\\ on\\ A:}\\; +f_{s}\\hat{n}, \\;\\; \\rm{on\\ B:}\\; -f_{s}\\hat{n}");
        if (anyDamper) line("\\f{\xE2\x88\x82\\mathcal{R}}{\xE2\x88\x82\\dot q} = c\\,\\dot\\ell\\,\\f{\xE2\x88\x82\\ell}{\xE2\x88\x82 q} \\;\\;==>\\;\\; f_{d} = c\\,\\dot\\ell, \\;\\; \\rm{same\\ pattern\\ with\\ }f_{d}");
        if (anyTorsion) line("\\f{\xE2\x88\x82V}{\xE2\x88\x82\\theta_{B}} = k_{t}(\\theta_{B} - \\theta_{A} - \\theta_{0}) = -\\tau_{t}\\;\\;==>\\;\\; \\rm{torque\\ on\\ A:}\\; +\\tau_{t}, \\;\\; \\rm{on\\ B:}\\; -\\tau_{t}, \\;\\; \\tau_{t} = k_{t}(\\theta_{B} - \\theta_{A} - \\theta_{0})");
    }

    // 5. the equations of motion, body by body ----------------------------------------------------------------------
    text("Collecting every term, the equations of motion of each body (signs: an element's force acts on its A end as written and on its B end with the opposite sign):");
    auto signOf = [](bool plus) { return plus ? std::string(" + ") : std::string(" - "); };
    for (int bi : dyn) {
        const Body& b = m.bodies[bi];
        const std::string k = I(mi(bi));
        std::string tx, ty, tt;
        auto addTerm = [&](std::string& dst, const std::string& sg, const std::string& term) { dst += (dst.empty() ? (sg == " - " ? "-" : "") : sg) + term; };
        if (!b.lockY) addTerm(ty, " + ", "m_{" + k + "}\\,g");
        for (const Elem& e : elems) {
            const Link& l = m.links[e.li];
            const bool isA = (e.A.body == bi), isB = (e.B.body == bi);
            if (!isA && !isB) continue;
            const std::string tg = e.tag, j = I(sym.link[e.li]);
            const std::string sgn = isA ? " + " : " - ";
            auto force = [&](const std::string& f, bool sumElems) {
                const std::string nb = sumElems ? tg + ",e" : tg;
                const std::string nvec = "\\hat{n}_{" + nb + "}";
                const std::string pre = sumElems ? "\\sum{e}{}" : "";
                if (!b.lockX) addTerm(tx, sgn, pre + f + "\\hat{n}_{" + nb + ",x}");
                if (!b.lockY) addTerm(ty, sgn, pre + f + "\\hat{n}_{" + nb + ",y}");
                if (!b.lockRot) addTerm(tt, sgn, pre + f + "(" + rhoName(tg + (isA ? ",A" : ",B")) + "\\times " + nvec + ")");
            };
            if (l.kind == LinkKind::Spring) force(l.dist ? "f_{s," + tg + ",e}" : "f_{s," + j + "}", l.dist != 0);
            else if (l.kind == LinkKind::Damper && m.env.damping) force(l.dist ? "f_{d," + tg + ",e}" : "f_{d," + j + "}", l.dist != 0);
            else if (l.kind == LinkKind::Pin && l.kt != 0 && !b.lockRot) addTerm(tt, sgn, "\\tau_{t," + j + "}");
            else if (l.kind == LinkKind::Pin && l.jointC != 0 && m.env.damping && !b.lockRot) addTerm(tt, sgn, "c_{\\theta," + j + "}(\\dot\\theta_{B} - \\dot\\theta_{A})");
        }
        // applied loads
        if (b.loadN > 0 || b.loadTimeVar) {
            const std::string F = b.loadTimeVar ? "F_{" + k + "}(t)" : "F_{" + k + "}";
            if (!b.lockX) addTerm(tx, " + ", F + "\\cos\\alpha_{" + k + "}");
            if (!b.lockY) addTerm(ty, " + ", F + "\\sin\\alpha_{" + k + "}");
        }
        if (b.driveF > 0) {
            const std::string F = b.driveT > 0 ? "F_{0," + k + "}\\sin(\\pi t/T_{" + k + "})" : "F_{0," + k + "}\\cos(\\omega_{" + k + "}t)";
            if (!b.lockX) addTerm(tx, " + ", F + "\\cos\\beta_{" + k + "}");
            if (!b.lockY) addTerm(ty, " + ", F + "\\sin\\beta_{" + k + "}");
        }
        if (b.momentN != 0 || b.momentTimeVar) { if (!b.lockRot) addTerm(tt, " + ", b.momentTimeVar ? "M_{" + k + "}(t)" : "M_{" + k + "}"); }
        if (b.dloadType != 0 && b.shape.preset == ShapePreset::Box) {
            if (!b.lockX) addTerm(tx, " + ", "\\cos\\gamma_{" + k + "}\\int{0}{w}q_{" + k + "}(x)\\,\\rm{d}x");
            if (!b.lockY) addTerm(ty, " + ", "\\sin\\gamma_{" + k + "}\\int{0}{w}q_{" + k + "}(x)\\,\\rm{d}x");
            if (!b.lockRot) addTerm(tt, " + ", "\\int{0}{w}q_{" + k + "}(x)\\,(x - x_{c})\\,\\rm{d}x");
        }
        if (m.env.airDrag) {
            if (!b.lockX) addTerm(tx, " - ", "(c_{a1} + c_{a2}|\\vec{v}_{" + k + "}|)\\dot x_{" + k + "}");
            if (!b.lockY) addTerm(ty, " - ", "(c_{a1} + c_{a2}|\\vec{v}_{" + k + "}|)\\dot y_{" + k + "}");
        }
        if (anyJoint) {
            if (!b.lockX) addTerm(tx, " + ", "\\sum_{c}\\lambda_{c,x}");
            if (!b.lockY) addTerm(ty, " + ", "\\sum_{c}\\lambda_{c,y}");
            if (!b.lockRot) addTerm(tt, " + ", "\\sum_{c}(\\vec{\\rho}_{c}\\times\\vec{\\lambda}_{c}) + \\sum_{c}\\lambda_{c,\\theta}");
        }
        if (anyContactPossible && b.collides()) {
            if (!b.lockX) addTerm(tx, " + ", "\\sum_{q}(\\lambda_{n,q}\\hat{n}_{q} + \\lambda_{t,q}\\hat{t}_{q})_{x}");
            if (!b.lockY) addTerm(ty, " + ", "\\sum_{q}(\\lambda_{n,q}\\hat{n}_{q} + \\lambda_{t,q}\\hat{t}_{q})_{y}");
            if (!b.lockRot) addTerm(tt, " + ", "\\sum_{q}\\vec{\\rho}_{q}\\times(\\lambda_{n,q}\\hat{n}_{q} + \\lambda_{t,q}\\hat{t}_{q})");
        }
        (void)signOf;
        if (!b.lockX) line("m_{" + k + "}\\ddot x_{" + k + "} = " + (tx.empty() ? std::string("0") : tx));
        if (!b.lockY) line("m_{" + k + "}\\ddot y_{" + k + "} = " + (ty.empty() ? std::string("0") : ty));
        if (!b.lockRot) line("J_{" + k + "}\\ddot \\theta_{" + k + "} = " + (tt.empty() ? std::string("0") : tt));
    }
    {
        // definitions of the symbols that appear
        std::string defs;
        if (anySpring) defs += "f_{s} = k(\\ell - L_{0}) \\;\\;" + std::string(anyDist ? "(each element of a distribution uses k_{e}, L_{0,e}) " : "");
        if (anyDamper) defs += std::string(defs.empty() ? "" : ", \\;\\; ") + "f_{d} = c\\,\\dot\\ell";
        if (!defs.empty()) line(defs);
    }
    if (anyJoint) {
        text("Joints. A pin forces its two points together, a weld also fixes the relative angle, a roller lets one point slide along a line, a rod keeps a fixed length. Each is an equation $\\Phi(\\vec{q}) = 0$ and brings an unknown reaction $\\lambda$:");
        std::string c;
        for (const Elem& e : elems) {
            const Link& l = m.links[e.li];
            const std::string j = I(sym.link[e.li]);
            if (l.kind == LinkKind::Pin) c += "\\Phi_{J" + j + "} = \\vec{p}_{B} - \\vec{p}_{A} = \\vec{0},\\;\\;";
            else if (l.kind == LinkKind::Weld) c += "\\Phi_{J" + j + "} = \\mat{\\vec{p}_{B} - \\vec{p}_{A}; \\theta_{B} - \\theta_{A} - \\theta_{\\rm{ref}}} = \\vec{0},\\;\\;";
            else if (l.kind == LinkKind::Roller) c += "\\Phi_{J" + j + "} = \\hat{e}_{y}(\\theta_{A})\\cdot(\\vec{p}_{B} - \\vec{p}_{A}) = 0,\\;\\;";
            else if (l.kind == LinkKind::Rod) c += "\\Phi_{L" + j + "} = |\\vec{p}_{B} - \\vec{p}_{A}| - L_{" + j + "} = 0,\\;\\;";
        }
        if (!c.empty()) line(c.substr(0, c.size() - 3));
        text("Differentiating twice gives an equation for the accelerations, $J\\ddot{\\vec q} = -\\dot J\\dot{\\vec q}$ (with a small correction that stops numerical drift), and the reactions $\\lambda$ follow from solving the two sets together:");
        line("\\mat{M & -J^{T}; J & 0}\\mat{\\ddot{\\vec{q}}; \\vec\\lambda} = \\mat{\\vec{f}(\\vec{q},\\dot{\\vec q},t); -\\dot J\\dot{\\vec q}}");
    }
    if (anyContactPossible) {
        text("Contacts. With a signed gap $g_{q}(\\vec{q}) \\ge 0$ between two surfaces, the normal force can only push, and only when the gap is closed; friction is limited by the normal force; an impact reverses the approach speed by the restitution $e$:");
        line("g_{q} \\ge 0, \\;\\; \\lambda_{n,q} \\ge 0, \\;\\; g_{q}\\,\\lambda_{n,q} = 0, \\;\\; |\\lambda_{t,q}| \\le \\mu\\,\\lambda_{n,q}, \\;\\; v_{n}^{+} = -e\\,v_{n}^{-}");
        std::string rows;
        int shown = 0;
        for (int bi : dyn) {
            const Body& a = m.bodies[bi];
            if (!a.collides()) continue;
            for (size_t o2 = 0; o2 < m.bodies.size(); ++o2) {
                if ((int)o2 == bi || !m.bodies[o2].collides()) continue;
                if (m.bodies[o2].isDynamic() && (int)o2 < bi) continue;
                if (shown >= 6) break;
                const Body& c = m.bodies[o2];
                const double mu = combine(m.env.muCombine, a.mat.muK, c.mat.muK);
                const double e = combine(m.env.eCombine, a.mat.restitution, c.mat.restitution);
                char buf[160];
                std::snprintf(buf, sizeof buf, "%s on %s:  \xCE\xBC = %s,  e = %s", a.name.c_str(), c.name.c_str(), m.env.friction ? fnum(mu).c_str() : "0", m.env.inelastic ? fnum(e).c_str() : "1");
                H << "<div class=\"ln txt\" style=\"margin:2px 0 2px 24px;font-size:18px\">" << hesc(buf) << "</div>\n";
                ++shown;
            }
        }
    }

    // 6. matrix form -----------------------------------------------------------------------------------------------
    heading("Result:");
    {
        std::string diag, qd;
        for (int bi : dyn) {
            const Body& b = m.bodies[bi];
            if (!b.lockX) diag += (diag.empty() ? "" : ",\\,") + fnum(b.massKg);
            if (!b.lockY) diag += (diag.empty() ? "" : ",\\,") + fnum(b.massKg);
            if (!b.lockRot) diag += (diag.empty() ? "" : ",\\,") + fnum(b.inertia);
        }
        text("In matrix form, with the numbers of this design:");
        line("M\\,\\ddot{\\vec{q}} = \\vec{f}(\\vec{q},\\dot{\\vec{q}},t)" + std::string(anyJoint ? " + J^{T}\\vec\\lambda" : "") + std::string(anyContactPossible ? " + \\vec{f}_{\\rm{contact}}" : "") + ", \\;\\; M = \\rm{diag}(" + diag + ")", "final");
    }

    // 7. the single-degree-of-freedom reduction --------------------------------------------------------------------
    bool reduced = false;
    if (dyn.size() == 1 && !anyJoint && !anyTorsion) {
        const int bi = dyn[0];
        const Body& b = m.bodies[bi];
        const bool xFree = !b.lockX, yFree = !b.lockY;
        if (b.lockRot && (xFree != yFree)) {
            const Vec2 ax = xFree ? Vec2(1, 0) : Vec2(0, 1);
            double K = 0, C = 0, preload = 0;                  // preload: force the springs already exert along the axis at the drawn position
            bool ok = true;
            for (const Elem& e : elems) {
                const Link& l = m.links[e.li];
                if (l.kind != LinkKind::Spring && l.kind != LinkKind::Damper) continue;
                if (e.A.body != bi && e.B.body != bi) continue;
                if (l.k3 != 0 && l.kind == LinkKind::Spring) { ok = false; break; }
                const int nEl = l.dist ? l.distN() : 1;
                for (int el = 0; el < nEl && ok; ++el) {
                    Vec2 pa = e.A.w0, pb = e.B.w0;
                    double kk = l.kind == LinkKind::Spring ? l.k : l.c;
                    if (l.dist) {
                        const double sg = (el + 0.5) / nEl;
                        const End A2 = endOf(l.a, l.la2), B2 = endOf(l.b, l.lb2);
                        pa = e.A.w0 + (A2.w0 - e.A.w0) * sg;
                        pb = e.B.w0 + (B2.w0 - e.B.w0) * sg;
                        const double s = sg * l.distSpan();
                        kk = (kk + (l.dist == 2 ? l.distM * s : 0)) * l.distSpan() / nEl;
                    }
                    const Vec2 d = pb - pa;
                    if (length(d) < 1e-9 || std::fabs(dot(normalized(d), ax)) < 0.9999) ok = false;
                    if (!ok) break;
                    if (l.kind == LinkKind::Spring) {
                        K += kk;
                        if (!l.dist) {                                                      // a hand-set rest length leaves the spring stretched or squeezed at the drawn position
                            const double ell = length(d), f = l.k * (ell - l.L0);
                            preload += (e.A.body == bi ? 1.0 : -1.0) * f * dot(d * (1.0 / ell), ax);
                        }
                    } else if (m.env.damping) C += kk;
                }
                if (!ok) break;
            }
            if (ok && K > 0) {
                reduced = true;
                const std::string u = xFree ? "x" : "y";
                const std::string k = I(mi(bi));
                double Fc = (yFree ? b.massKg * m.env.gravity : 0.0);
                const double alpha = (b.loadAngleDeg + (b.loadPull ? 180.0 : 0.0)) * kPi / 180.0;
                const Vec2 Fl = b.loadTimeVar ? Vec2(0, 0) : Vec2(std::cos(alpha), std::sin(alpha)) * b.loadN;
                Fc += dot(Fl, ax);
                Fc += preload;
                const bool harm = b.driveF > 0 && b.driveT <= 0;
                const double Fh = harm ? b.driveF * dot(Vec2(std::cos(b.driveAngleDeg * kPi / 180), std::sin(b.driveAngleDeg * kPi / 180)), ax) : 0.0;
                const double mm = b.massKg, wn = std::sqrt(K / mm), zeta = C / (2 * std::sqrt(K * mm));
                heading("Reduction to one degree of freedom:");
                text("Every spring and damper lies along the free axis, so with $" + u + "$ measured from the drawn position they combine: $k_{eq} = \\sum k$ and $c_{eq} = \\sum c$.");
                line("k_{eq} = " + fnum(K) + "\\,\\rm{N/m}, \\;\\; c_{eq} = " + fnum(C) + "\\,\\rm{N\\,s/m}, \\;\\; F_{c} = " + fnum(Fc) + "\\,\\rm{N}\\;\\;\\rm{(weight,\\ constant\\ loads\\ and\\ spring\\ preload\\ along\\ the\\ axis)}");
                line("m_{" + k + "}\\,\\ddot " + u + " + c_{eq}\\,\\dot " + u + " + k_{eq}\\," + u + " = F_{c}" + std::string(b.loadTimeVar ? " + F_{" + k + "}(t)\\cos\\alpha_{" + k + "}" : "") + std::string(harm ? " + F_{0," + k + "}\\cos(\\omega_{" + k + "}t)\\cos\\beta" : "") + ", \\;\\; " + fnum(mm) + "\\,\\ddot " + u + " + " + fnum(C) + "\\,\\dot " + u + " + " + fnum(K) + "\\," + u + " = " + fnum(Fc) + (b.loadTimeVar ? " + F(t)" : ""));
                text("Shifting to the equilibrium position gives the standard form, with natural frequency, damping ratio and static offset:");
                line("\\xi = " + u + " - \\f{F_{c}}{k_{eq}}, \\;\\; \\ddot\\xi + 2\\zeta\\omega_{n}\\dot\\xi + \\omega_{n}^{2}\\xi = \\f{F(t)}{m}, \\;\\; \\omega_{n} = \\sqrt{\\f{k_{eq}}{m}} = " + fnum(wn) + "\\,\\rm{rad/s}, \\;\\; \\zeta = \\f{c_{eq}}{2\\sqrt{k_{eq}m}} = " + fnum(zeta) + ", \\;\\; " + u + "_{eq} = " + fnum(Fc / K) + "\\,\\rm{m}");
                const double v0 = dot(b.vel, ax), xi0 = -Fc / K;
                if (!b.loadTimeVar && !harm) {
                    if (zeta < 1 - 1e-9) {
                        const double wd = wn * std::sqrt(1 - zeta * zeta);
                        const double Bc = (v0 + zeta * wn * xi0) / wd;
                        line("\\omega_{d} = \\omega_{n}\\sqrt{1 - \\zeta^{2}} = " + fnum(wd) + "\\,\\rm{rad/s}, \\;\\; T_{d} = \\f{2\\pi}{\\omega_{d}} = " + fnum(2 * kPi / wd) + "\\,\\rm{s}");
                        line(u + "(t) = " + fnum(Fc / K) + " + e^{-" + fnum(zeta * wn) + "\\,t}\\\\[" + fnum(xi0) + "\\cos(" + fnum(wd) + "\\,t) + " + fnum(Bc) + "\\sin(" + fnum(wd) + "\\,t)\\\\]  \\rm{m}", "final");
                    } else {
                        line("\\zeta \\ge 1:\\;\\; \\rm{no\\ oscillation;\\ the\\ mass\\ creeps\\ to\\ }" + u + "_{eq} = " + fnum(Fc / K) + "\\,\\rm{m}", "final");
                    }
                } else if (harm && !b.loadTimeVar) {
                    const double w = b.driveW, X = Fh / std::sqrt((K - mm * w * w) * (K - mm * w * w) + C * w * C * w);
                    const double ph = std::atan2(C * w, K - mm * w * w);
                    line("\\rm{steady\\ state:}\\;\\; X = \\f{F_{0}\\cos\\beta}{\\sqrt{(k_{eq} - m\\omega^{2})^{2} + (c_{eq}\\omega)^{2}}} = " + fnum(X) + "\\,\\rm{m}, \\;\\; \\varphi = \\rm{atan2}(c_{eq}\\omega,\\ k_{eq} - m\\omega^{2}) = " + fnum(ph) + "\\,\\rm{rad}");
                    line(u + "(t) \\to " + fnum(Fc / K) + " + " + fnum(X) + "\\cos(" + fnum(w) + "\\,t - " + fnum(ph) + ")\\;\\rm{m}\\;\\;\\rm{plus\\ the\\ decaying\\ transient\\ above}", "final");
                } else {
                    text("The load $F(t)$ is a formula, so the response is the sum of the free vibration above and the convolution $\\xi_p(t) = \\int_0^t \\f{F(\\tau)}{m\\omega_d}e^{-\\zeta\\omega_n(t-\\tau)}\\sin\\omega_d(t-\\tau)\\,\\rm{d}\\tau$; the simulation integrates it numerically.");
                }
            }
        }
    }
    (void)reduced;

    H << "<div class=\"ref\">Generated by VL Lab from the design \"" << hesc(m.name) << "\". Lagrange's equations in maximal coordinates, the formulation the simulator integrates.</div>\n";
    H << "</div><script>window.onload=function(){document.body.setAttribute('data-h',Math.ceil(document.documentElement.scrollHeight))}</script></body></html>";
    out.html = H.str();
    return out;
}

// =====================================================================================================================
//  rendering with the system's Edge / Chrome
// =====================================================================================================================
#ifdef _WIN32
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

bool fileExists(const std::string& p) { DWORD a = GetFileAttributesW(widen(p).c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }

std::string findBrowser() {
    const char* roots[] = {"PROGRAMFILES(X86)", "PROGRAMFILES", "LOCALAPPDATA"};
    const char* rel[] = {"\\Microsoft\\Edge\\Application\\msedge.exe", "\\Google\\Chrome\\Application\\chrome.exe", "\\BraveSoftware\\Brave-Browser\\Application\\brave.exe"};
    for (const char* r : rel)
        for (const char* root : roots) {
            char buf[MAX_PATH] = {0};
            if (!GetEnvironmentVariableA(root, buf, MAX_PATH)) continue;
            std::string p = std::string(buf) + r;
            if (fileExists(p)) return p;
        }
    return std::string();
}

// What happened when a command was run
struct RunInfo { bool started = false; bool timedOut = false; DWORD exitCode = 0; DWORD error = 0; };

// Runs a command with no window. The program has no console of its own, so the child is given explicit standard handles
// (nothing to read, and a log file for what it prints): a browser started without valid handles can quit at once.
RunInfo runWait(const std::wstring& cmdline, DWORD timeoutMs, const std::wstring& logPath = std::wstring()) {
    RunInfo info;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    HANDLE out = logPath.empty() ? CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr)
                                 : CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = in;
    si.hStdOutput = out;
    si.hStdError = out;
    PROCESS_INFORMATION pi{};
    std::wstring c = cmdline;
    // The child gets our environment minus the Windows compatibility-shim variables (__COMPAT_LAYER and friends). Windows sets them on a program
    // it has put under a compatibility layer (for example "DetectorsAppHealth"); a browser that inherits one exits at once without doing anything.
    std::wstring envBlock;
    if (LPWCH env = GetEnvironmentStringsW()) {
        for (const wchar_t* e = env; *e; e += wcslen(e) + 1) {
            const std::wstring entry = e;
            if (entry.compare(0, 9, L"__COMPAT_") == 0 || entry.compare(0, 10, L"__PROCESS_") == 0) continue;
            envBlock += entry;
            envBlock += L'\0';
        }
        envBlock += L'\0';
        FreeEnvironmentStringsW(env);
    }
    const BOOL ok = CreateProcessW(nullptr, &c[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, envBlock.empty() ? nullptr : (LPVOID)envBlock.data(), nullptr, &si, &pi);
    info.error = ok ? 0 : GetLastError();
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (!ok) return info;
    info.started = true;
    DWORD r = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (r == WAIT_TIMEOUT) { info.timedOut = true; TerminateProcess(pi.hProcess, 1); }
    GetExitCodeProcess(pi.hProcess, &info.exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return info;
}

std::string describe(const RunInfo& r) {
    if (!r.started) return "could not start (Windows error " + std::to_string(r.error) + ")";
    if (r.timedOut) return "timed out after the allowed time";
    char b[64];
    std::snprintf(b, sizeof b, "exit code 0x%lX", (unsigned long)r.exitCode);
    return b;
}

// the last few lines of a log, for an error message
std::string logTail(const std::string& path, size_t n = 300) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string t = ss.str();
    std::string keep;
    for (char c : t) keep += (c == '\r' ? ' ' : c);
    if (keep.size() > n) keep = keep.substr(keep.size() - n);
    while (!keep.empty() && (keep.back() == '\n' || keep.back() == ' ')) keep.pop_back();
    return keep;
}

std::string fileUrl(const std::string& p) {
    std::string u = "file:///";
    for (char c : p) {
        if (c == '\\') u += '/';
        else if (c == ' ') u += "%20";
        else u += c;
    }
    return u;
}

void rmTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            const std::wstring full = dir + L"\\" + n;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rmTree(full);
            else { SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW(full.c_str()); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

}  // namespace

bool renderSheet(const Sheet& s, const std::string& outPath, SheetFormat format, std::string* error) {
    const bool pdf = format == SheetFormat::Pdf;
    auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
    const std::string exe = findBrowser();
    if (exe.empty()) return fail("Microsoft Edge (or Chrome) was not found; it is needed to draw the page as an image or PDF.");

    char tmp[MAX_PATH] = {0};
    GetTempPathA(MAX_PATH, tmp);
    const std::string dir = std::string(tmp) + "vlab_sheet_" + std::to_string(GetTickCount64());
    CreateDirectoryW(widen(dir).c_str(), nullptr);
    const std::string page = dir + "\\page.html", dom = dir + "\\dom.txt", png = dir + "\\shot.png", ps = dir + "\\convert.ps1", log = dir + "\\browser.log";
    {
        std::ofstream f(page, std::ios::binary);
        if (!f) return fail("cannot write the temporary page in " + dir);
        f << s.html;
    }
    std::string trace;                                                        // what was tried, for sheet_debug.txt
    auto note = [&](const std::string& t) { trace += t + "\r\n"; };
    note("browser: " + exe);
    note("page: " + page + " (" + std::to_string(s.html.size()) + " bytes, width " + std::to_string(s.width) + ")");
    note("output: " + outPath);
    {
        char cwd[MAX_PATH] = {0};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        note(std::string("working folder: ") + cwd);
        for (const char* v : {"__COMPAT_LAYER", "TEMP", "USERPROFILE", "SESSIONNAME", "MSEDGE_OUTER_TAB_TAG"}) {
            char b[512] = {0};
            note(std::string("env ") + v + " = " + (GetEnvironmentVariableA(v, b, sizeof b) ? b : "(not set)"));
        }
        const RunInfo pr = runWait(widen("\"" + exe + "\" --version"), 20000, widen(log));
        note("browser --version: " + describe(pr) + ", prints: " + logTail(log, 120));
        DeleteFileA(log.c_str());
    }

    const std::string q = "\"";
    if (!pdf) {
    {
        std::ofstream f(ps, std::ios::binary);
        f << "param($in,$out,$fmt)\r\n"
             "Add-Type -AssemblyName System.Drawing\r\n"
             "$b = New-Object System.Drawing.Bitmap($in)\r\n"
             "$r = New-Object System.Drawing.Rectangle(0,0,$b.Width,$b.Height)\r\n"
             "$d = $b.LockBits($r,[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)\r\n"
             "$n = [Math]::Abs($d.Stride) * $b.Height\r\n"
             "$buf = New-Object byte[] $n\r\n"
             "[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0,$buf,0,$n)\r\n"
             "$stride = [Math]::Abs($d.Stride); $b.UnlockBits($d)\r\n"
             "Add-Type -TypeDefinition 'public static class VlBox { public static int[] Find(byte[] p,int w,int h,int stride){ int x0=w,y0=h,x1=-1,y1=-1; for(int y=0;y<h;y++){ int o=y*stride; for(int x=0;x<w;x++){ int i=o+x*4; if(p[i]<245||p[i+1]<245||p[i+2]<245){ if(x<x0)x0=x; if(x>x1)x1=x; if(y<y0)y0=y; if(y>y1)y1=y; } } } return new int[]{x0,y0,x1,y1}; } }'\r\n"
             "$bb = [VlBox]::Find($buf,$b.Width,$b.Height,$stride)\r\n"
             "$pad = 44\r\n"
             "if ($bb[2] -lt 0) { $bb = @(0,0,($b.Width-1),($b.Height-1)) }\r\n"
             "$x0=[Math]::Max(0,$bb[0]-$pad); $y0=[Math]::Max(0,$bb[1]-$pad); $x1=[Math]::Min($b.Width-1,$bb[2]+$pad); $y1=[Math]::Min($b.Height-1,$bb[3]+$pad)\r\n"
             "$w=$x1-$x0+1; $h=$y1-$y0+1\r\n"
             "$o = New-Object System.Drawing.Bitmap($w,$h)\r\n"
             "$g = [System.Drawing.Graphics]::FromImage($o)\r\n"
             "$g.Clear([System.Drawing.Color]::White)\r\n"
             "$g.DrawImage($b,(New-Object System.Drawing.Rectangle(0,0,$w,$h)),(New-Object System.Drawing.Rectangle($x0,$y0,$w,$h)),[System.Drawing.GraphicsUnit]::Pixel)\r\n"
             "$g.Dispose()\r\n"
             "$enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }\r\n"
             "$ep = New-Object System.Drawing.Imaging.EncoderParameters(1)\r\n"
             "$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality,[long]94)\r\n"
             "if ($fmt -eq 'png') { $o.Save($out,[System.Drawing.Imaging.ImageFormat]::Png) } else { $o.Save($out,$enc,$ep) }\r\n"
             "$o.Dispose(); $b.Dispose()\r\n";
    }
    }

    // Two tries: normal flags, then without the browser's own sandbox (a browser started by a program that runs elevated, or under some
    // security software, refuses to start with it).
    const char* variants[] = {"", " --no-sandbox --disable-setuid-sandbox", " --no-sandbox --disable-setuid-sandbox"};   // the third goes through cmd.exe
    bool done = false;
    std::string why;
    for (int attempt = 0; attempt < 3 && !done; ++attempt) {
        const std::string prof = dir + "\\profile" + std::to_string(attempt);
        const std::string common = q + exe + q + " --headless --disable-gpu --no-first-run --disable-extensions --user-data-dir=" + q + prof + q + variants[attempt];
        note(std::string("attempt ") + std::to_string(attempt + 1) + (variants[attempt][0] ? std::string(" with ") + variants[attempt] : std::string(" with the normal flags")));
        DeleteFileA(outPath.c_str());
        DeleteFileA(png.c_str());
        const bool viaCmd = attempt == 2;
        const auto launch = [&](const std::string& cmdline) {
            const auto t0 = GetTickCount64();
            const RunInfo ri = viaCmd ? runWait(widen("cmd.exe /s /c " + q + cmdline + q), 90000, widen(log)) : runWait(widen(cmdline), 90000, widen(log));
            note("    took " + std::to_string((unsigned long long)(GetTickCount64() - t0)) + " ms" + (viaCmd ? " (through cmd.exe)" : ""));
            return ri;
        };
        if (pdf) {
            const std::string cmd = common + " --no-pdf-header-footer --virtual-time-budget=6000 --print-to-pdf=" + q + outPath + q + " " + q + fileUrl(page) + q;
            const RunInfo r = launch(cmd);
            note("  print to PDF: " + describe(r));
            done = fileExists(outPath);
            if (!done) why = "the browser did not produce the PDF (" + describe(r) + ")";
            continue;
        }
        // the page measures itself into <body data-h>: read it with --dump-dom so the screenshot is exactly tall enough
        int height = 2400;
        {
            const std::string inner = q + common + " --window-size=" + std::to_string(s.width) + ",900 --virtual-time-budget=6000 --dump-dom " + q + fileUrl(page) + q + " > " + q + dom + q + q;
            const RunInfo r = runWait(widen("cmd.exe /s /c " + inner), 90000, widen(log));
            std::ifstream f(dom, std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            const std::string t = ss.str();
            size_t a = t.find("data-h=\"");
            if (a != std::string::npos) height = std::max(300, std::atoi(t.c_str() + a + 8));
            note("  measure the page: " + describe(r) + ", " + std::to_string(t.size()) + " bytes of output, height " + (a == std::string::npos ? std::string("not found (using 2400)") : std::to_string(height)));
        }
        const std::string shot = common + " --hide-scrollbars --force-device-scale-factor=2 --default-background-color=FFFFFFFF --window-size=" + std::to_string(s.width) + "," + std::to_string(height + 40)
                                 + " --virtual-time-budget=6000 --screenshot=" + q + png + q + " " + q + fileUrl(page) + q;
        const RunInfo r = launch(shot);
        note("  screenshot: " + describe(r));
        if (!fileExists(png)) { why = "the browser did not produce the picture (" + describe(r) + ")"; continue; }
        const std::string conv = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " + q + ps + q + " " + q + png + q + " " + q + outPath + q + (format == SheetFormat::Png ? " png" : " jpg");
        const RunInfo rc = runWait(widen(conv), 90000, widen(log));
        note("  convert: " + describe(rc));
        done = fileExists(outPath);
        if (!done) why = std::string("could not convert the picture to ") + (format == SheetFormat::Png ? "PNG" : "JPG") + " (" + describe(rc) + ")";
    }

    if (!done) {
        const std::string tail = logTail(log);
        why += tail.empty() ? std::string() : ": " + tail;
        // leave the details next to where the file should have gone, so they can be sent for diagnosis
        const size_t slash = outPath.find_last_of("\\/");
        const std::string where = slash == std::string::npos ? std::string() : outPath.substr(0, slash + 1);
        std::ofstream d(where + "sheet_debug.txt", std::ios::binary);
        d << "System sheet export failed\r\n" << trace << "\r\nbrowser output:\r\n" << logTail(log, 4000) << "\r\n";
        std::ofstream h(where + "sheet_debug.html", std::ios::binary);
        h << s.html;
        why += "  (details: " + where + "sheet_debug.txt)";
    }
    rmTree(widen(dir));
    if (!done) return fail(why);
    return true;
}
#else
bool renderSheet(const Sheet&, const std::string&, SheetFormat, std::string* error) {
    if (error) *error = "rendering the sheet needs Windows";
    return false;
}
#endif

}  // namespace vlab
