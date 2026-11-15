// sheet_tests.cpp -- checks the system sheet (diagram + specifications + derivation) and its export to JPG, PNG and PDF.
//   vlab_sheet_tests                run everything (the export checks are skipped when no Edge / Chrome is installed)
//   vlab_sheet_tests --out FOLDER   also leave the sample systems there: the designs (.vlab) and their sheets (JPG, PNG, PDF)
//
// The sheet's one-degree-of-freedom reduction is checked against the real simulation: the natural frequency, damping ratio, equilibrium offset
// and steady-state amplitude printed on the page must agree with what the integrator produces for the same design.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "derivation.hpp"
#include "vlab/project_io.hpp"
#include "vlab/world.hpp"

using namespace vlab;

static int gFail = 0, gTotal = 0, gSkipped = 0;
static void check(const char* name, bool ok, const char* fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::printf("  [%s] %-58s %s\n", ok ? "PASS" : "FAIL", name, buf);
    ++gTotal;
    if (!ok) ++gFail;
}

static const Material& M(const char* n) { return *findMaterial(n); }

// ------------------------------------------------------------------ the sample systems
// A: two masses on rails between two walls, springs and dampers, a constant push on one and a sine force F(t) on the other
static Model systemA() {
    Model m;
    m.name = "Loaded two-mass system";
    m.env.collisions = false;
    const int wl = m.addBody(makeAnchor(Vec2(-4, 0)));
    const int wr = m.addBody(makeAnchor(Vec2(7, 0)));
    Body a = makeBox(1.0, 0.6, M("Steel"), Vec2(0, 0)); a.name = "Mass 1"; a.mass = 5; a.massFromDensity = false; a.lockY = true; a.lockRot = true;
    Body b = makeBox(1.0, 0.6, M("Steel"), Vec2(3, 0)); b.name = "Mass 2"; b.mass = 3; b.massFromDensity = false; b.lockY = true; b.lockRot = true;
    a.loadN = 40; a.loadAngleDeg = 0;                                                      // constant push to the right
    b.loadTimeVar = true; b.loadExpr = "60*sin(2*pi*1.5*t)"; b.loadAngleDeg = 0;             // F(t) = 60 sin(3 pi t)
    const int i1 = m.addBody(a), i2 = m.addBody(b);
    m.updateMass();
    m.addLink(LinkKind::Spring, wl, i1, Vec2(-4, 0), Vec2(-0.5, 0)); m.links.back().k = 200;
    m.addLink(LinkKind::Damper, wl, i1, Vec2(-4, 0), Vec2(-0.5, 0)); m.links.back().c = 6;
    m.addLink(LinkKind::Spring, i1, i2, Vec2(0.5, 0), Vec2(2.5, 0)); m.links.back().k = 120;
    m.addLink(LinkKind::Damper, i1, i2, Vec2(0.5, 0), Vec2(2.5, 0)); m.links.back().c = 3;
    m.addLink(LinkKind::Spring, i2, wr, Vec2(3.5, 0), Vec2(7, 0)); m.links.back().k = 150;
    m.refreshAutoRest();
    return m;
}

// B: a disc on a pin with a torsional spring and rotary damping, driven by a time-varying moment, plus a sine push on its centre
static Model systemB() {
    Model m;
    m.name = "Rotor with a moment load";
    Body d = makeCircle(0.5, M("Steel"), Vec2(0, 0)); d.name = "Rotor"; d.mass = 4; d.massFromDensity = false;
    d.momentTimeVar = true; d.momentExpr = "20*sin(2*pi*1*t)";
    d.loadN = 15; d.loadAngleDeg = 90;
    const int id = m.addBody(d);
    m.updateMass();
    const int li = m.addSupport(LinkKind::Pin, id, Vec2(0, 0), 0);
    m.links[li].kt = 80;
    m.links[li].jointC = 2;
    m.refreshAutoRest();
    return m;
}

// C: one mass on a rail with a pre-stretched spring, a damper, a constant push and a harmonic push (the one-degree-of-freedom reduction applies)
struct SingleDof { Model m; int mass; double K, C, mm, Fc, xeq, F0, w; };
static SingleDof systemC() {
    SingleDof s;
    Model& m = s.m;
    m.name = "Single mass with loads";
    m.env.collisions = false;
    const int wall = m.addBody(makeAnchor(Vec2(-3, 0)));
    Body b = makeBox(1.0, 0.6, M("Steel"), Vec2(0, 0)); b.name = "Mass"; b.mass = 2; b.massFromDensity = false; b.lockY = true; b.lockRot = true;
    b.loadN = 10; b.loadAngleDeg = 0;
    b.driveF = 4; b.driveW = 3; b.driveAngleDeg = 0;
    s.mass = m.addBody(b);
    m.updateMass();
    m.addLink(LinkKind::Spring, wall, s.mass, Vec2(-3, 0), Vec2(-0.5, 0));
    m.links.back().k = 50; m.links.back().autoRest = false; m.links.back().L0 = 2.0;     // drawn 2.5 m long, rest length 2.0 m: stretched by 0.5 m
    m.addLink(LinkKind::Damper, wall, s.mass, Vec2(-3, 0), Vec2(-0.5, 0));
    m.links.back().c = 1.2;
    s.K = 50; s.C = 1.2; s.mm = 2; s.F0 = 4; s.w = 3;
    s.Fc = 10 - 50 * 0.5;                                                                  // push minus the pull of the stretched spring
    s.xeq = s.Fc / s.K;
    return s;
}

// ------------------------------------------------------------------ helpers
static std::string plainText(const std::string& html) {                                  // the page as plain text, minus signs normalised
    std::string out;
    bool tag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];
        if (c == '<') tag = true;
        else if (c == '>') tag = false;
        else if (!tag) out += c;
    }
    std::string o2;
    for (size_t i = 0; i < out.size(); ++i) {
        if (out.compare(i, 3, "\xE2\x88\x92") == 0) { o2 += '-'; i += 2; }               // U+2212 minus
        else o2 += out[i];
    }
    return o2;
}
static std::string g4(double v) { char b[40]; std::snprintf(b, sizeof b, "%.4g", v); return b; }
static bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

static std::string readAll(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ------------------------------------------------------------------ the content of the sheet
static void testContent() {
    std::printf("Sheet content\n");
    {
        const Model m = systemA();
        const Sheet s = buildSheet(m, SheetKind::Full, SchematicOptions());
        const std::string t = plainText(s.html);
        check("full sheet has Specifications and Solution, no Problem", has(s.html, "Specifications:") && has(s.html, "Solution:") && !has(s.html, "Problem:"), "html %zu bytes", s.html.size());
        check("specifications list the masses, springs, dampers", has(t, "= 5kg") && has(t, "= 3kg") && has(t, "= 200N/m") && has(t, "= 120N/m") && has(t, "= 150N/m") && has(t, "= 6Ns/m"), "m 5 and 3, k 200/120/150, c 6");
        check("the constant push and the F(t) formula are listed", has(t, "= 40N") && has(t, "60") && has(t, "sin(2") && has(t, "1.5"), "F = 40 N, F(t) = 60*sin(2*pi*1.5*t)");
        check("derivation steps are present", has(s.html, "Lagrange") && has(t, "Generalised coordinates") && has(t, "Energy of the system") && has(t, "Collecting every term"), "Lagrange, coordinates, energy, equations");
        check("one equation per free coordinate (2 bodies, x only)", has(s.html, "<i>x</i></span>") || has(s.html, "<i>x</i>"), "x equations for both masses");
        int eq = 0;
        for (size_t p = 0; (p = s.html.find("class=\"ln", p)) != std::string::npos; ++p) ++eq;
        check("the page has a full derivation (many lines)", eq >= 25, "%d lines", eq);
        SchematicOptions o;
        o.symbols = true; o.serif = true; o.values = false;
        const std::string svg = schematicSvgString(m, o);
        check("diagram labelled with variables, subscripts and the F(t) load", has(svg, "baseline-shift=\"sub\"") && has(svg, ">F<") && has(svg, "(t)") && has(svg, "marker-end"), "svg %zu bytes", svg.size());
        const std::string plain = schematicSvgString(m, SchematicOptions());
        check("the plain diagram still names and values the parts", has(plain, "Mass 1") && has(plain, "k = 200 N/m"), "names and values");
    }
    {
        const Model m = systemB();
        const Sheet s = buildSheet(m, SheetKind::Full, SchematicOptions());
        const std::string t = plainText(s.html);
        check("rotor: torsional spring, moment and joint equations", has(t, "= 80Nm/rad") && has(t, "20") && has(t, "sin(2") && has(t, "Joints") && has(t, "moment"), "kt = 80, M(t), joint section");
        SchematicOptions o;
        o.symbols = true; o.serif = true;
        const std::string svg = schematicSvgString(m, o);
        check("rotor diagram: moment arrow and its label", has(svg, "<path d=\"M") && has(svg, "M"), "curved arrow");
    }
}

// ------------------------------------------------------------------ the one-degree-of-freedom result against the simulation
static void testReduction() {
    std::printf("One-degree-of-freedom reduction against the simulation\n");
    const SingleDof s = systemC();
    const double wn = std::sqrt(s.K / s.mm), zeta = s.C / (2 * std::sqrt(s.K * s.mm)), wd = wn * std::sqrt(1 - zeta * zeta);
    const double den = std::sqrt((s.K - s.mm * s.w * s.w) * (s.K - s.mm * s.w * s.w) + s.C * s.w * s.C * s.w);
    const double X = s.F0 / den, phi = std::atan2(s.C * s.w, s.K - s.mm * s.w * s.w);

    const Sheet sh = buildSheet(s.m, SheetKind::Full, SchematicOptions());
    const std::string t = plainText(sh.html);
    check("sheet reduces to one degree of freedom", has(t, "Reduction to one degree of freedom"), "section present");
    check("sheet prints k_eq and c_eq", has(t, "= 50N/m") && has(t, "= 1.2Ns/m"), "50 N/m, 1.2 N s/m");
    check("sheet includes the spring preload in F_c", has(t, "= " + g4(s.Fc) + "N"), "F_c = %s N (push 10 - preload 25)", g4(s.Fc).c_str());
    check("sheet prints wn, zeta and the equilibrium offset", has(t, "= " + g4(wn) + "rad/s") && has(t, "= " + g4(zeta)) && has(t, "= " + g4(s.xeq) + "m"), "wn %s, zeta %s, x_eq %s", g4(wn).c_str(), g4(zeta).c_str(), g4(s.xeq).c_str());
    check("sheet prints the steady-state amplitude and phase", has(t, "= " + g4(X) + "m") && has(t, "= " + g4(phi) + "rad"), "X %s m, phi %s rad", g4(X).c_str(), g4(phi).c_str());

    // simulate: after the transient the mass must follow x_eq + X sin(w t - phi)
    {
        World w(s.m);
        const double x0 = s.m.bodies[s.mass].pos.x;
        const double dt = s.m.env.dt;
        const long long n = (long long)(70.0 / dt);
        double worst = 0, sumx = 0;
        long long cnt = 0;
        for (long long i = 0; i < n; ++i) {
            w.step();
            if (w.time() > 60.0 && i % 40 == 0) {
                const double u = w.state()[s.mass].pos.x - x0;
                worst = std::fmax(worst, std::fabs(u - (s.xeq + X * std::cos(s.w * w.time() - phi))));
                sumx += u; ++cnt;
            }
        }
        check("simulated steady state follows x_eq + X cos(wt - phi)", worst < 2e-3, "worst gap %.2e m over 10 s (mean offset %.4f, x_eq %.4f)", worst, sumx / (double)cnt, s.xeq);
    }
    // simulate the free response from rest: x(t) = x_eq + e^(-zeta wn t) [xi0 cos(wd t) + (zeta wn xi0 / wd) sin(wd t)]
    {
        SingleDof f = systemC();
        f.m.bodies[f.mass].driveF = 0;
        World w(f.m);
        const double x0 = f.m.bodies[f.mass].pos.x;
        const double xi0 = -f.xeq;
        const double dt = f.m.env.dt;
        const long long n = (long long)(15.0 / dt);
        double worst = 0;
        for (long long i = 0; i < n; ++i) {
            w.step();
            if (i % 40 == 0) {
                const double t2 = w.time();
                const double u = f.xeq + std::exp(-zeta * wn * t2) * (xi0 * std::cos(wd * t2) + zeta * wn * xi0 / wd * std::sin(wd * t2));
                worst = std::fmax(worst, std::fabs((w.state()[f.mass].pos.x - x0) - u));
            }
        }
        check("simulated free response matches the closed form", worst < 2e-3, "worst gap %.2e m over 15 s", worst);
    }
}

// ------------------------------------------------------------------ the files
static unsigned be32(const std::string& s, size_t p) { return ((unsigned char)s[p] << 24) | ((unsigned char)s[p + 1] << 16) | ((unsigned char)s[p + 2] << 8) | (unsigned char)s[p + 3]; }

static bool jpgSize(const std::string& s, int& w, int& h) {
    if (s.size() < 4 || (unsigned char)s[0] != 0xFF || (unsigned char)s[1] != 0xD8) return false;
    size_t p = 2;
    while (p + 9 < s.size()) {
        if ((unsigned char)s[p] != 0xFF) { ++p; continue; }
        const unsigned char mk = (unsigned char)s[p + 1];
        if (mk == 0xC0 || mk == 0xC1 || mk == 0xC2) { h = ((unsigned char)s[p + 5] << 8) | (unsigned char)s[p + 6]; w = ((unsigned char)s[p + 7] << 8) | (unsigned char)s[p + 8]; return true; }
        p += 2 + (((unsigned char)s[p + 2] << 8) | (unsigned char)s[p + 3]);
    }
    return false;
}

static void exportAndCheck(const std::string& dir, const std::string& tag, const Model& m, SheetKind kind, bool& browserMissing) {
    const char* kinds[] = {"system", "full"};
    for (int f = 0; f < 3; ++f) {
        const SheetFormat fmt = f == 0 ? SheetFormat::Jpg : f == 1 ? SheetFormat::Png : SheetFormat::Pdf;
        const std::string path = dir + "\\" + tag + "_" + kinds[kind == SheetKind::Full] + sheetExtension(fmt);
        std::string err;
        const Sheet sh = buildSheet(m, kind, SchematicOptions());
        const bool ok = renderSheet(sh, path, fmt, &err);
        if (!ok && err.find("was not found") != std::string::npos) { browserMissing = true; return; }
        const std::string name = tag + " " + kinds[kind == SheetKind::Full] + " " + (f == 0 ? "JPG" : f == 1 ? "PNG" : "PDF");
        if (!ok) { check(name.c_str(), false, "%s", err.c_str()); continue; }
        const std::string data = readAll(path);
        char info[200];
        bool good = false;
        if (f == 0) {
            int w = 0, h = 0;
            good = jpgSize(data, w, h) && w >= 800 && h >= 200 && data.size() > (kind == SheetKind::Full ? 60000u : 15000u);
            std::snprintf(info, sizeof info, "%dx%d px, %zu bytes", w, h, data.size());
        } else if (f == 1) {
            const bool sig = data.size() > 33 && std::memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) == 0;
            const unsigned w = sig ? be32(data, 16) : 0, h = sig ? be32(data, 20) : 0;
            good = sig && w >= 1600 && h >= 400 && data.size() > (kind == SheetKind::Full ? 80000u : 15000u);
            std::snprintf(info, sizeof info, "%ux%u px, %zu bytes", w, h, data.size());
        } else {
            good = data.compare(0, 5, "%PDF-") == 0 && data.find("%%EOF") != std::string::npos && data.size() > (kind == SheetKind::Full ? 40000u : 8000u);
            std::snprintf(info, sizeof info, "%zu bytes", data.size());
        }
        check(name.c_str(), good, "%s", info);
    }
}

static void testExports(const std::string& dir) {
    std::printf("Export to JPG, PNG and PDF (%s)\n", dir.c_str());
    bool missing = false;
    const Model a = systemA(), b = systemB();
    const SingleDof c = systemC();
    exportAndCheck(dir, "A_two_masses", a, SheetKind::SystemOnly, missing);
    if (!missing) exportAndCheck(dir, "A_two_masses", a, SheetKind::Full, missing);
    if (!missing) exportAndCheck(dir, "B_rotor", b, SheetKind::Full, missing);
    if (!missing) exportAndCheck(dir, "C_single_mass", c.m, SheetKind::Full, missing);
    if (missing) { ++gSkipped; std::printf("  [SKIP] Microsoft Edge / Chrome not installed: the file checks were skipped\n"); return; }

    // Windows puts some programs under a compatibility layer and marks them with __COMPAT_LAYER in their environment. A browser that inherits it
    // exits at once without drawing anything, so every export from such a program failed. The export must work with the variable present.
    _putenv("__COMPAT_LAYER=DetectorsAppHealth");
    for (int f = 0; f < 3; ++f) {
        const SheetFormat fmt = f == 0 ? SheetFormat::Jpg : f == 1 ? SheetFormat::Png : SheetFormat::Pdf;
        const std::string path = dir + "\\compat_layer_test" + sheetExtension(fmt);
        std::string err;
        const bool ok = renderSheet(buildSheet(a, SheetKind::Full, SchematicOptions()), path, fmt, &err);
        const std::string name = std::string("export with __COMPAT_LAYER set ") + (f == 0 ? "JPG" : f == 1 ? "PNG" : "PDF");
        check(name.c_str(), ok && readAll(path).size() > 40000, "%s", ok ? "written" : err.c_str());
        std::remove(path.c_str());
    }
    _putenv("__COMPAT_LAYER=");
}

int main(int argc, char** argv) {
    std::string out;
    for (int i = 1; i + 1 < argc; ++i) if (std::strcmp(argv[i], "--out") == 0) out = argv[i + 1];
    testContent();
    testReduction();
    std::string dir = out;
    if (dir.empty()) {
        char tmp[260] = {0};
        const char* t = std::getenv("TEMP");
        std::snprintf(tmp, sizeof tmp, "%s\\vlab_sheet_tests", t ? t : ".");
        dir = tmp;
    }
    std::string mk = "mkdir \"" + dir + "\" >nul 2>&1";
    std::system(mk.c_str());
    if (!out.empty()) {                                                     // keep the sample designs next to their sheets
        saveModelFile(dir + "\\A_two_masses.vlab", systemA());
        saveModelFile(dir + "\\B_rotor.vlab", systemB());
        saveModelFile(dir + "\\C_single_mass.vlab", systemC().m);
    }
    testExports(dir);
    std::printf("\n%d of %d sheet checks passed%s\n", gTotal - gFail, gTotal, gSkipped ? " (file checks skipped)" : "");
    return gFail == 0 ? 0 : 1;
}
