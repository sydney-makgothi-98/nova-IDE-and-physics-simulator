#include "vlab/model.hpp"
#include "vlab/section.hpp"
#include <algorithm>

namespace vlab {

// ------------------------------------------------------------------ materials
const std::vector<Material>& materialLibrary() {
    static const std::vector<Material> lib = {
        // name           density  muS   muK   e     rollRes  E (Pa)  alpha (1/K)
        {"Ideal (frictionless, elastic)", 1000, 0.00, 0.00, 1.00, 0.000, 12e-6, 200e9},
        {"Steel",         7850, 0.74, 0.57, 0.65, 0.002, 12e-6, 200e9},
        {"Aluminium",     2700, 0.61, 0.47, 0.60, 0.003, 23e-6, 69e9},
        {"Rubber",        1100, 1.00, 0.80, 0.80, 0.020, 150e-6, 0.05e9},
        {"Wood",           700, 0.50, 0.35, 0.50, 0.010, 5e-6, 11e9},
        {"Concrete",      2400, 0.70, 0.60, 0.20, 0.015, 10e-6, 30e9},
        {"Glass",         2500, 0.90, 0.40, 0.70, 0.003, 9e-6, 70e9},
        {"Ice",            917, 0.10, 0.03, 0.30, 0.001, 51e-6, 9e9},
        {"Teflon (PTFE)", 2200, 0.04, 0.04, 0.50, 0.002, 120e-6, 0.5e9},
        {"Clay / putty",  1800, 0.60, 0.50, 0.05, 0.030, 20e-6, 0.05e9},
    };
    return lib;
}

const Material* findMaterial(const std::string& name) {
    for (const Material& m : materialLibrary())
        if (m.name == name) return &m;
    return nullptr;
}

// ------------------------------------------------------------------ shapes
static void finishPolygon(Shape& s, std::vector<Vec2> v) {
    const size_t n = v.size();
    double A = 0, J = 0;
    Vec2 C;
    for (size_t i = 0; i < n; ++i) {
        Vec2 a = v[i], b = v[(i + 1) % n];
        double cr = cross(a, b);
        A += cr;
        C += (a + b) * cr;
        J += cr * (dot(a, a) + dot(a, b) + dot(b, b));
    }
    A *= 0.5;
    C = C * (1.0 / (6.0 * A));
    J /= 12.0;                                  // polar second moment of area about the origin
    for (Vec2& p : v) p -= C;                   // centre the polygon on its centroid
    double Jc = J - A * dot(C, C);              // parallel-axis theorem
    s.verts = v;
    s.normals.resize(n);
    for (size_t i = 0; i < n; ++i) {
        Vec2 e = v[(i + 1) % n] - v[i];
        s.normals[i] = normalized(Vec2(e.y, -e.x));   // outward for a counter-clockwise polygon
    }
    s.area = A;
    s.inertiaPerMass = Jc / A;
}

void Shape::rebuild() {
    verts.clear();
    normals.clear();
    if (preset == ShapePreset::Circle) {
        radius = std::max(0.01, radius);
        area = kPi * radius * radius;
        inertiaPerMass = 0.5 * radius * radius;
        return;
    }
    if (preset == ShapePreset::Line) {
        w = std::max(0.05, w);
        h = 0;
        verts = {Vec2(-w / 2, 0), Vec2(w / 2, 0)};            // a degenerate two-corner polygon: two faces back to back
        normals = {Vec2(0, -1), Vec2(0, 1)};
        area = 0;
        inertiaPerMass = 0;
        return;
    }
    if (preset == ShapePreset::Box) {
        w = std::max(0.01, w);
        h = std::max(0.01, h);
        double hw = w / 2, hh = h / 2;
        finishPolygon(*this, {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}});
    } else if (preset == ShapePreset::Regular) {
        sides = std::max(3, std::min(12, sides));
        radius = std::max(0.01, radius);
        std::vector<Vec2> v;
        for (int i = 0; i < sides; ++i) {
            double a = 2.0 * kPi * i / sides + kPi / sides;
            v.push_back({radius * std::cos(a), radius * std::sin(a)});
        }
        finishPolygon(*this, v);
    } else {  // Wedge: right triangle with legs w (along x) and h (along y)
        w = std::max(0.01, w);
        h = std::max(0.01, h);
        finishPolygon(*this, {{0, 0}, {w, 0}, {0, h}});
    }
}

double Shape::boundingRadius() const {
    if (isCircle()) return radius;
    double r = 0;
    for (const Vec2& v : verts) r = std::max(r, length(v));
    return r;
}

// ------------------------------------------------------------------ environment
void Environment::setIdeal() {
    realistic = false;
    friction = false;
    inelastic = false;
    rolling = false;
    damping = false;
    airDrag = false;
}
void Environment::setRealistic() {
    realistic = true;
    friction = true;
    inelastic = true;
    rolling = true;
    damping = true;
}

// ------------------------------------------------------------------ model
void Model::updateMass() {
    for (Body& b : bodies) {
        if (b.secType != 0 && b.shape.preset == ShapePreset::Box) b.shape.h = std::max(0.01, b.secH);    // the drawn thickness follows the chosen section
        b.shape.rebuild();
        if (b.kind != BodyKind::Dynamic) {
            b.massKg = 0;
            b.invMass = 0;
            b.inertia = 0;
            b.invI = 0;
            continue;
        }
        double area = b.shape.area;
        if (b.secType != 0 && b.shape.preset == ShapePreset::Box) area = makeSection(b).A * b.shape.w;   // a real section: mass = density x section area x length
        double m = b.massFromDensity ? b.mat.density * area : b.mass;
        m = std::max(1e-6, m);
        b.massKg = m;
        b.invMass = 1.0 / m;
        b.inertia = m * b.shape.inertiaPerMass;
        b.invI = b.lockRot ? 0.0 : 1.0 / std::max(1e-9, b.inertia);
    }
}

std::string Model::uniqueName(const std::string& base) const {
    for (int n = 1;; ++n) {
        std::string cand = base + " " + std::to_string(n);
        bool used = false;
        for (const Body& b : bodies) used = used || (b.name == cand);
        if (!used) return cand;
    }
}

int Model::addBody(const Body& b) {
    bodies.push_back(b);
    updateMass();
    return (int)bodies.size() - 1;
}

void Model::removeBody(int i) {
    if (i < 0 || i >= (int)bodies.size()) return;
    for (int j = (int)links.size() - 1; j >= 0; --j)
        if (links[j].a == i || links[j].b == i) links.erase(links.begin() + j);
    for (Link& l : links) {
        if (l.a > i) --l.a;
        if (l.b > i) --l.b;
    }
    bodies.erase(bodies.begin() + i);
}

Vec2 Model::worldPoint(int body, Vec2 local) const {
    const Body& b = bodies[body];
    return b.pos + Rot(b.angle).apply(local);
}

int Model::addLink(LinkKind kind, int a, int b, Vec2 wpA, Vec2 wpB) {
    if (a == b || a < 0 || b < 0 || a >= (int)bodies.size() || b >= (int)bodies.size()) return -1;
    if (!bodies[a].isDynamic() && !bodies[b].isDynamic()) return -1;   // nothing could move
    Link l;
    l.kind = kind;
    l.a = a;
    l.b = b;
    l.la = Rot(bodies[a].angle).applyInv(wpA - bodies[a].pos);
    l.lb = Rot(bodies[b].angle).applyInv(wpB - bodies[b].pos);
    for (const Link& o : links)
        if (o.kind == kind && o.a == a && o.b == b && length(o.la - l.la) < 1e-6 && length(o.lb - l.lb) < 1e-6) return -1;
    if (kind == LinkKind::Spring || kind == LinkKind::Rod) l.L0 = std::max(0.05, length(wpB - wpA));
    if (kind == LinkKind::Damper) l.c = 2.0;
    if (kind != LinkKind::Spring && kind != LinkKind::Damper) l.autoRest = false;
    if (kind == LinkKind::Weld) l.refAngle = bodies[b].angle - bodies[a].angle;
    links.push_back(l);
    return (int)links.size() - 1;
}

int Model::addSupport(LinkKind kind, int body, Vec2 wp, double angleRad) {
    if (body < 0 || body >= (int)bodies.size() || !bodies[body].isDynamic()) return -1;
    if (kind != LinkKind::Pin && kind != LinkKind::Weld && kind != LinkKind::Roller) return -1;
    Body g = makeAnchor(wp);
    g.angle = angleRad;
    const char* nm = kind == LinkKind::Pin ? "Pin support" : (kind == LinkKind::Weld ? "Fixed support" : "Roller support");
    g.name = uniqueName(nm);
    int gi = addBody(g);
    int li = addLink(kind, gi, body, wp, wp);
    if (li < 0) removeBody(gi);
    return li;
}

void Model::removeLink(int i) {
    if (i >= 0 && i < (int)links.size()) links.erase(links.begin() + i);
}

void Model::refreshAutoRest() {
    for (Link& l : links)
        if (l.kind == LinkKind::Weld) l.refAngle = bodies[l.b].angle - bodies[l.a].angle;
    for (Link& l : links)
        if (l.kind == LinkKind::Spring && l.autoRest)
            l.L0 = std::max(0.05, length(worldPoint(l.b, l.lb) - worldPoint(l.a, l.la)));
}

int Model::dynamicCount() const {
    int n = 0;
    for (const Body& b : bodies) n += b.isDynamic();
    return n;
}

// ------------------------------------------------------------------ factories
static unsigned paletteColor(const Material& m) {
    if (m.name.rfind("Steel", 0) == 0) return 0xFF8A97A8;
    if (m.name.rfind("Alum", 0) == 0) return 0xFFB8C2CC;
    if (m.name.rfind("Rubber", 0) == 0) return 0xFF2E2E36;
    if (m.name.rfind("Wood", 0) == 0) return 0xFFB5834A;
    if (m.name.rfind("Concrete", 0) == 0) return 0xFF9A9A94;
    if (m.name.rfind("Glass", 0) == 0) return 0xFF7FD0D8;
    if (m.name.rfind("Ice", 0) == 0) return 0xFFB5E3F5;
    if (m.name.rfind("Teflon", 0) == 0) return 0xFFEDEDED;
    if (m.name.rfind("Clay", 0) == 0) return 0xFFC77B5A;
    return 0xFF3574F0;
}

static Body baseBody(const char* name, const Material& m, Vec2 pos) {
    Body b;
    b.name = name;
    b.mat = m;
    b.pos = pos;
    b.color = paletteColor(m);
    return b;
}

Body makeCircle(double radius, const Material& m, Vec2 pos) {
    Body b = baseBody("Ball", m, pos);
    b.shape.preset = ShapePreset::Circle;
    b.shape.radius = radius;
    b.shape.rebuild();
    return b;
}
Body makeBox(double w, double h, const Material& m, Vec2 pos) {
    Body b = baseBody("Block", m, pos);
    b.shape.preset = ShapePreset::Box;
    b.shape.w = w;
    b.shape.h = h;
    b.shape.rebuild();
    return b;
}
Body makeRegular(int sides, double radius, const Material& m, Vec2 pos) {
    Body b = baseBody("Polygon", m, pos);
    b.shape.preset = ShapePreset::Regular;
    b.shape.sides = sides;
    b.shape.radius = radius;
    b.shape.rebuild();
    return b;
}
Body makeWedge(double w, double h, const Material& m, Vec2 pos) {
    Body b = baseBody("Wedge", m, pos);
    b.shape.preset = ShapePreset::Wedge;
    b.shape.w = w;
    b.shape.h = h;
    b.shape.rebuild();
    return b;
}
Body makeFloor(double length, double angleDeg, const Material& m, Vec2 pos) {
    Body b = baseBody("Floor", m, pos);
    b.kind = BodyKind::Static;
    b.shape.preset = ShapePreset::Line;
    b.shape.w = length;
    b.shape.rebuild();
    b.angle = angleDeg * kPi / 180.0;
    b.color = 0xFFC8CEDA;
    return b;
}
Body makeBeam(double length, double thickness, const Material& m, Vec2 pos) {
    Body b = baseBody("Beam", m, pos);
    b.shape.preset = ShapePreset::Box;
    b.shape.w = length;
    b.shape.h = thickness;
    b.shape.rebuild();
    b.massFromDensity = true;                 // a longer or thicker beam is heavier
    return b;
}
Body makeAnchor(Vec2 pos) {
    Body b;
    b.name = "Anchor";
    b.kind = BodyKind::Anchor;
    b.shape.preset = ShapePreset::Circle;
    b.shape.radius = 0.06;
    b.shape.rebuild();
    b.pos = pos;
    b.color = 0xFFDDDDDD;
    return b;
}

}  // namespace vlab
