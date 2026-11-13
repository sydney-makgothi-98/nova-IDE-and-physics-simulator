// ui_canvas.cpp -- the drawing area: shapes, floors, springs, dampers, joints; tools; mouse interaction.
#include <algorithm>
#include <cctype>
#include <functional>
#include <cmath>
#include <cstdio>
#include "app.hpp"

namespace vlab {

static ImVec2 operator+(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
static ImVec2 operator-(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }
static ImVec2 operator*(ImVec2 a, float s) { return ImVec2(a.x * s, a.y * s); }

ImVec2 App::toScreen(Vec2 w) const {
    ImVec2 c((canvasMin.x + canvasMax.x) * 0.5f, (canvasMin.y + canvasMax.y) * 0.5f);
    return ImVec2(c.x + (float)((w.x - viewCenter.x) * scale), c.y + (float)((w.y - viewCenter.y) * scale));
}
Vec2 App::toWorld(ImVec2 s) const {
    ImVec2 c((canvasMin.x + canvasMax.x) * 0.5f, (canvasMin.y + canvasMax.y) * 0.5f);
    return Vec2(viewCenter.x + (s.x - c.x) / scale, viewCenter.y + (s.y - c.y) / scale);
}
Vec2 App::snapped(Vec2 w) const {
    if (!snap || ImGui::GetIO().KeyAlt) return w;
    return Vec2(std::round(w.x / snapStep) * snapStep, std::round(w.y / snapStep) * snapStep);
}

// pose of a body as it should be drawn: live from the world, or the design pose (with the optional mode-shape animation)
bool App::bodyPose(int i, Vec2& pos, double& angle) const {
    const Model& m = ses.design();
    if (i < 0 || i >= (int)m.bodies.size()) return false;
    if (ses.hasWorld() && i < (int)ses.world()->state().size()) {
        const BodyState& s = ses.world()->state()[i];
        pos = s.pos;
        angle = s.angle;
        return true;
    }
    pos = m.bodies[i].pos;
    angle = m.bodies[i].angle;
    if (state == RunState::Design && modeShown >= 0 && haveModal && modeShown < (int)modal.modes.size() && m.bodies[i].isDynamic()) {
        const Mode& md = modal.modes[modeShown];
        double ph = std::sin(modeClock * 2.0 * kPi * 0.7) * 0.4;
        for (size_t k = 0; k < md.shape.size(); ++k) {
            if (modal.dofBody[k] != i) continue;
            if (modal.dofKind[k] == 0) pos.x += md.shape[k] * ph;
            else if (modal.dofKind[k] == 1) pos.y += md.shape[k] * ph;
            else angle += md.shape[k] * ph;
        }
    }
    return true;
}

int App::pickBody(Vec2 w) const {
    const Model& m = ses.design();
    for (int i = (int)m.bodies.size() - 1; i >= 0; --i) {
        const Body& b = m.bodies[i];
        Vec2 p;
        double a;
        if (!bodyPose(i, p, a)) continue;
        if (b.kind == BodyKind::Anchor) { if (length(w - p) < std::max(0.18, 10.0 / scale)) return i; continue; }
        if (b.shape.isCircle()) { if (length(w - p) <= b.shape.radius) return i; continue; }
        if (b.shape.preset == ShapePreset::Line) {                               // zero thickness: pick by distance to the line
            Vec2 l = Rot(a).applyInv(w - p);
            if (std::fabs(l.x) <= b.shape.w / 2 + 6.0 / scale && std::fabs(l.y) < 9.0 / scale) return i;
            continue;
        }
        Vec2 l = Rot(a).applyInv(w - p);
        bool inside = true;
        for (size_t k = 0; k < b.shape.verts.size() && inside; ++k)
            if (dot(b.shape.normals[k], l - b.shape.verts[k]) > 0) inside = false;
        if (inside) return i;
    }
    return -1;
}

static double segDist(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 d = b - a;
    double L2 = dot(d, d), t = L2 > 1e-12 ? clampd(dot(p - a, d) / L2, 0, 1) : 0;
    return length(p - (a + d * t));
}

int App::pickLink(Vec2 w) const {
    const Model& m = ses.design();
    int best = -1;
    double bd = 8.0 / scale;
    for (size_t i = 0; i < m.links.size(); ++i) {
        const Link& l = m.links[i];
        Vec2 pa, pb;
        double aa, ab;
        if (!bodyPose(l.a, pa, aa) || !bodyPose(l.b, pb, ab)) continue;
        double d = segDist(w, pa + Rot(aa).apply(l.la), pb + Rot(ab).apply(l.lb));
        if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

void App::fitView() {
    const Model& m = ses.design();
    if (m.bodies.empty()) { viewCenter = Vec2(0, 0); scale = 70; return; }
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (size_t i = 0; i < m.bodies.size(); ++i) {
        const Body& b = m.bodies[i];
        if (b.shape.isCircle() || b.kind == BodyKind::Anchor) {                // true extents (a long floor line must not inflate the box)
            double r = b.kind == BodyKind::Anchor ? 0.2 : b.shape.radius;
            x0 = std::min(x0, b.pos.x - r); x1 = std::max(x1, b.pos.x + r);
            y0 = std::min(y0, b.pos.y - r); y1 = std::max(y1, b.pos.y + r);
        } else {
            Rot rr(b.angle);
            for (const Vec2& v : b.shape.verts) {
                Vec2 vv = v;
                if (b.shape.preset == ShapePreset::Line && b.shape.w >= 5000.0) vv = v * (12.0 / b.shape.w);   // an indefinite floor: frame only the middle 24 m
                Vec2 q = b.pos + rr.apply(vv);
                x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
                y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
            }
        }
    }
    double w = std::max(1.0, x1 - x0), h = std::max(1.0, y1 - y0);
    double cw = std::max(200.0f, canvasMax.x - canvasMin.x), ch = std::max(150.0f, canvasMax.y - canvasMin.y);
    scale = clampd(std::min(cw / (w * 1.3), ch / (h * 1.45)), 18.0, 160.0);
    viewCenter = Vec2((x0 + x1) / 2, (y0 + y1) / 2);
}

// ====================================================================== placing things
// a placement tool places once and returns to Select (hold Shift to keep placing), so existing things stay selectable
void App::afterPlace() {
    if (!ImGui::GetIO().KeyShift) tool = Tool::Select;
}

void App::placeAt(Vec2 w) {
    if (!canEditStructure()) return;
    Model& m = design();
    const std::vector<Material>& lib = materialLibrary();
    const Material& mat = lib[std::clamp(nextMaterial, 0, (int)lib.size() - 1)];
    Vec2 p = snapped(w);
    Body b;
    switch (tool) {
        case Tool::Circle: b = makeCircle(nextRadius, mat, p); b.name = m.uniqueName("Ball"); break;
        case Tool::Box: b = makeBox(nextW, nextH, mat, p); b.name = m.uniqueName("Block"); break;
        case Tool::Polygon: b = makeRegular(nextSides, nextRadius, mat, p); b.name = m.uniqueName("Polygon"); break;
        case Tool::Wedge: b = makeWedge(nextW, nextH, mat, p); b.name = m.uniqueName("Wedge"); break;
        case Tool::Beam: b = makeBeam(beamLength, beamThick, mat, p); b.name = m.uniqueName("Beam"); break;
        case Tool::Floor: b = makeFloor(floorLength, 0, lib[std::clamp(floorMaterial, 0, (int)lib.size() - 1)], p); b.name = m.uniqueName("Floor"); break;
        case Tool::Anchor: b = makeAnchor(p); b.name = m.uniqueName("Anchor"); break;
        default: return;
    }
    b.angle = nextAngle * kPi / 180.0;
    if (b.isDynamic() && tool != Tool::Beam) b.mass = nextMass;
    if ((int)m.bodies.size() >= 60) { say("That is plenty of bodies for one design (limit 60).", true); return; }
    pushUndo();
    selBody = m.addBody(b);
    selLink = -1;
    edited();
    afterPlace();
}

// Where a support attaches: the nearest of the body's centre, edge middles and corners if within 16 px, otherwise the click itself.
Vec2 App::supportPoint(int body, Vec2 w) const {
    const Model& m = ses.design();
    Vec2 pos;
    double ang;
    if (!bodyPose(body, pos, ang)) return w;
    const Body& b = m.bodies[body];
    std::vector<Vec2> cand;
    cand.push_back(Vec2(0, 0));
    if (b.shape.preset == ShapePreset::Box) {
        double hw = b.shape.w / 2, hh = b.shape.h / 2;
        const Vec2 c[] = {{-hw, 0}, {hw, 0}, {0, -hh}, {0, hh}, {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
        cand.insert(cand.end(), c, c + 8);
    } else {
        for (const Vec2& v : b.shape.verts) cand.push_back(v);
    }
    Rot r(ang);
    double best = 16.0 / scale;
    Vec2 out = w;
    for (const Vec2& c : cand) {
        Vec2 wp = pos + r.apply(c);
        double d = length(wp - w);
        if (d < best) { best = d; out = wp; }
    }
    return out;
}

void App::placeSupport(Vec2 w) {
    if (!canEditStructure()) return;
    Model& m = design();
    int id = pickBody(w);
    if (id < 0 || !m.bodies[id].isDynamic()) {                         // a click just outside an end or corner still counts
        id = -1;
        for (int i = 0; i < (int)m.bodies.size() && id < 0; ++i)
            if (m.bodies[i].isDynamic() && length(supportPoint(i, w) - w) > 1e-9 && length(supportPoint(i, w) - w) < 16.0 / scale) id = i;
    }
    if (id < 0) { say("Click on a beam or a body to support it.", true); return; }
    Vec2 wp = supportPoint(id, w);
    LinkKind k = tool == Tool::SupportPin ? LinkKind::Pin : (tool == Tool::SupportFixed ? LinkKind::Weld : LinkKind::Roller);
    if ((int)m.bodies.size() >= 60) { say("That is plenty of bodies for one design (limit 60).", true); return; }
    pushUndo();
    int li = m.addSupport(k, id, wp, nextAngle * kPi / 180.0);
    if (li < 0) { say("That support could not be placed.", true); return; }
    selLink = li;
    selBody = -1;
    edited();
    afterPlace();
}

// ====================================================================== drawing helpers
static void drawSpring(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float th) {
    float dx = b.x - a.x, dy = b.y - a.y, L = std::sqrt(dx * dx + dy * dy);
    if (L < 6) { dl->AddLine(a, b, col, th); return; }
    ImVec2 u(dx / L, dy / L), n(-u.y, u.x);
    float lead = std::min(12.0f, L * 0.18f), span = L - 2 * lead;
    int zig = (int)std::clamp(L / 13.0f, 5.0f, 28.0f);
    std::vector<ImVec2> pts;
    pts.push_back(a);
    pts.push_back(a + u * lead);
    for (int i = 0; i < zig * 2; ++i) {
        float t = lead + span * (i + 0.5f) / (zig * 2);
        pts.push_back(a + u * t + n * ((i % 2) ? 7.0f : -7.0f));
    }
    pts.push_back(b - u * lead);
    pts.push_back(b);
    dl->AddPolyline(pts.data(), (int)pts.size(), col, th);
}

static void drawDamper(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float th) {
    float dx = b.x - a.x, dy = b.y - a.y, L = std::sqrt(dx * dx + dy * dy);
    if (L < 24) { dl->AddLine(a, b, col, th); return; }
    ImVec2 u(dx / L, dy / L), n(-u.y, u.x);
    ImVec2 c0 = a + u * (L * 0.12f), c1 = a + u * (L * 0.58f), pist = a + u * (L * 0.46f);
    dl->AddLine(a, c0, col, th);
    dl->AddLine(c0 + n * 7.0f, c1 + n * 7.0f, col, th);
    dl->AddLine(c0 - n * 7.0f, c1 - n * 7.0f, col, th);
    dl->AddLine(c0 + n * 7.0f, c0 - n * 7.0f, col, th);
    dl->AddLine(pist, b, col, th);
    dl->AddLine(pist + n * 5.0f, pist - n * 5.0f, col, th + 1);
}

static void drawArrow(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddLine(a, b, col, 2.0f);
    float dx = b.x - a.x, dy = b.y - a.y, L = std::sqrt(dx * dx + dy * dy);
    if (L < 6) return;
    ImVec2 u(dx / L, dy / L), n(-u.y, u.x);
    dl->AddTriangleFilled(b, b - u * 8.0f + n * 4.0f, b - u * 8.0f - n * 4.0f, col);
}

// ====================================================================== connection points
// The point of a body's outline nearest to w (world), the outward normal there, and which edge / how far along it.
static Vec2 nearestBoundary(const Body& b, Vec2 p, double a, Vec2 w, Vec2* nrm, int* edgeIndex) {
    Rot r(a);
    if (b.kind == BodyKind::Anchor) { if (nrm) *nrm = Vec2(0, -1); if (edgeIndex) *edgeIndex = -1; return p; }
    if (b.shape.isCircle()) {
        Vec2 d = w - p;
        double L = length(d);
        Vec2 u = L > 1e-9 ? d * (1.0 / L) : Vec2(0, -1);
        if (nrm) *nrm = u;
        if (edgeIndex) *edgeIndex = -1;
        return p + u * b.shape.radius;
    }
    const size_t n = b.shape.verts.size();
    Vec2 best = p, bn(0, -1);
    double bd = 1e30;
    int bi = 0;
    for (size_t i = 0; i < n; ++i) {
        Vec2 va = p + r.apply(b.shape.verts[i]), vb = p + r.apply(b.shape.verts[(i + 1) % n]);
        Vec2 ab = vb - va;
        double L2 = lengthSq(ab), t = L2 > 0 ? clampd(dot(w - va, ab) / L2, 0.0, 1.0) : 0.0;
        Vec2 q = va + ab * t;
        double d = length(w - q);
        if (d < bd) { bd = d; best = q; bi = (int)i; if (i < b.shape.normals.size()) bn = r.apply(b.shape.normals[i]); }
    }
    if (b.shape.preset == ShapePreset::Line) { bn = r.apply(Vec2(0, -1)); if (dot(bn, w - best) < 0) bn = bn * -1.0; }
    if (nrm) *nrm = bn;
    if (edgeIndex) *edgeIndex = bi;
    return best;
}

// where the infinite line P1 + t d crosses the outline of a body (pose p, angle a)
static void lineBodyHits(const Body& b, Vec2 p, double a, Vec2 P1, Vec2 d, std::vector<Vec2>& hits) {
    if (b.shape.isCircle()) {
        Vec2 f = P1 - p;
        double B = dot(f, d), C = dot(f, f) - b.shape.radius * b.shape.radius, disc = B * B - C;
        if (disc >= 0) { double sq = std::sqrt(disc); hits.push_back(P1 + d * (-B - sq)); hits.push_back(P1 + d * (-B + sq)); }
        return;
    }
    Rot r(a);
    const size_t nv = b.shape.verts.size();
    for (size_t i = 0; i < nv; ++i) {
        Vec2 A = p + r.apply(b.shape.verts[i]), Bv = p + r.apply(b.shape.verts[(i + 1) % nv]), e = Bv - A;
        double den = cross(d, e);
        if (std::fabs(den) < 1e-12) continue;
        double t = cross(A - P1, e) / den, sidx = cross(A - P1, d) / den;
        if (sidx >= -1e-9 && sidx <= 1.0 + 1e-9) hits.push_back(P1 + d * t);
    }
}

bool App::reorientLink(int li, Vec2 axis, bool keepFirst) {
    Model& m = design();
    if (li < 0 || li >= (int)m.links.size() || lengthSq(axis) < 0.25) return false;
    Link& l = m.links[li];
    axis = normalized(axis);
    const int fixedBody = keepFirst ? l.a : l.b, movBody = keepFirst ? l.b : l.a;
    Vec2& fixedLocal = keepFirst ? l.la : l.lb;
    Vec2& movLocal = keepFirst ? l.lb : l.la;
    const Body& mb = m.bodies[movBody];
    if (mb.kind == BodyKind::Anchor) { say("That end is a fixed point: keep the other end fixed instead, or move the point.", true); return false; }
    const Vec2 P = m.worldPoint(fixedBody, fixedLocal), cur = m.worldPoint(movBody, movLocal);
    std::vector<Vec2> hits;
    lineBodyHits(mb, mb.pos, mb.angle, P, axis, hits);
    if (hits.empty()) { say("The line from the fixed end in that direction misses the other body.", true); return false; }
    Vec2 best = hits[0];
    for (const Vec2& h : hits) if (length(h - cur) < length(best - cur)) best = h;
    movLocal = Rot(mb.angle).applyInv(best - mb.pos);
    if (l.kind == LinkKind::Rod) l.L0 = std::max(0.05, length(m.worldPoint(l.b, l.lb) - m.worldPoint(l.a, l.la)));
    m.refreshAutoRest();
    return true;
}

Vec2 App::attachPointFor(int body, Vec2 w, Vec2* normal) const {
    const Body& b = ses.design().bodies[body];
    Vec2 p;
    double a;
    bodyPose(body, p, a);
    int ei = 0;
    Vec2 n(0, -1);
    Vec2 edge = nearestBoundary(b, p, a, w, &n, &ei);
    if (normal) *normal = n;
    if (b.kind == BodyKind::Anchor) return p;
    switch (linkAttach) {
        case 0: return w;                                                          // exactly where the click is
        case 3: return p;                                                          // the centre
        case 2: {                                                                  // key points: corners, middle and thirds of an edge (circles: every 45 degrees)
            Rot r(a);
            const double thr = 18.0 / scale;
            Vec2 bestK = edge;
            double bd = thr;
            auto consider = [&](Vec2 k) { double d = length(k - edge); if (d < bd) { bd = d; bestK = k; } };
            if (b.shape.isCircle()) {
                for (int k = 0; k < 8; ++k) consider(p + r.apply(Vec2(std::cos(k * kPi / 4), std::sin(k * kPi / 4)) * b.shape.radius));
            } else {
                const size_t nv = b.shape.verts.size();
                Vec2 va = p + r.apply(b.shape.verts[ei]), vb = p + r.apply(b.shape.verts[(ei + 1) % nv]);
                consider(va); consider(vb);
                consider(va + (vb - va) * 0.5); consider(va + (vb - va) * (1.0 / 3.0)); consider(va + (vb - va) * (2.0 / 3.0));
            }
            return bestK;
        }
        default: return edge;                                                      // the nearest point of the outline
    }
}

Vec2 App::linkAxis() const {
    switch (linkDir) {
        case 1: return lengthSq(linkFirstNormal) > 0.25 ? normalized(linkFirstNormal) : Vec2(0, 1);
        case 2: return Vec2(1, 0);
        case 3: return Vec2(0, 1);
        case 4: return Vec2(std::cos(linkAngle * kPi / 180.0), std::sin(linkAngle * kPi / 180.0));
        default: return Vec2(0, 0);
    }
}

bool App::constrainedSecond(int body, Vec2 mw, Vec2& out) const {
    Vec2 d = linkAxis();
    if (linkFirst < 0 || lengthSq(d) < 0.25) { out = attachPointFor(body, mw, nullptr); return true; }
    const Body& b = ses.design().bodies[body];
    Vec2 p;
    double a;
    bodyPose(body, p, a);
    if (b.kind == BodyKind::Anchor) { out = p; return true; }
    const Vec2 P1 = linkFirstWorld;
    std::vector<Vec2> hits;
    lineBodyHits(b, p, a, P1, d, hits);
    if (hits.empty()) { out = attachPointFor(body, mw, nullptr); return false; }   // the line misses it: attach freely
    out = hits[0];
    for (const Vec2& h : hits) if (length(h - mw) < length(out - mw)) out = h;
    return true;
}

int App::pickBodyForLink(Vec2 w) const {
    int id = pickBody(w);
    if (id >= 0) return id;
    const Model& m = ses.design();
    double bd = 12.0 / scale;
    int best = -1;
    for (int i = 0; i < (int)m.bodies.size(); ++i) {
        Vec2 p;
        double a;
        if (!bodyPose(i, p, a)) continue;
        Vec2 q = nearestBoundary(m.bodies[i], p, a, w, nullptr, nullptr);
        double d = length(w - q);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// ====================================================================== interaction
void App::canvasInteract(ImVec2 mp, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    Vec2 mw = toWorld(mp);
    Model& m = design();

    const bool placing = tool != Tool::Select;
    if (hovered && io.MouseWheel != 0 && io.KeyCtrl && placing) {   // Ctrl + wheel turns the object being placed
        nextAngle = std::fmod(nextAngle + (io.MouseWheel > 0 ? 5.0f : -5.0f) + 360.0f, 360.0f);
    } else if (hovered && io.MouseWheel != 0) {                  // zoom about the cursor
        Vec2 before = toWorld(mp);
        scale = clampd(scale * std::pow(1.12, io.MouseWheel), 12.0, 500.0);
        Vec2 after = toWorld(mp);
        viewCenter += before - after;
    }
    if (hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)))
        viewCenter -= Vec2(io.MouseDelta.x / scale, io.MouseDelta.y / scale);

    const bool linkTool = tool == Tool::Spring || tool == Tool::Damper || tool == Tool::Rod || tool == Tool::Pin || tool == Tool::Weld;
    const bool supportTool = tool == Tool::SupportPin || tool == Tool::SupportFixed || tool == Tool::SupportRoller;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (tool == Tool::Select) {
            int id = pickBody(mw);
            if (id >= 0 && m.bodies[id].kind == BodyKind::Anchor) {            // a support's symbol selects its connection (force, moment, motor)
                for (size_t li = 0; li < m.links.size(); ++li)
                    if (m.links[li].a == id && m.links[li].kind != LinkKind::Spring && m.links[li].kind != LinkKind::Damper) { selLink = (int)li; selBody = -1; id = -2; break; }
            }
            if (id == -2) {
                dragBody = -1;
            } else if (id >= 0) {
                selBody = id;
                selLink = -1;
                Vec2 p;
                double a;
                bodyPose(id, p, a);
                dragBody = id;
                dragLocal = Rot(a).applyInv(mw - p);
                if (ses.hasWorld() && m.bodies[id].isDynamic()) ses.world()->setMouse(id, dragLocal, mw);
            } else {
                int l = pickLink(mw);
                selLink = l;
                selBody = -1;
                dragBody = -1;
            }
        } else if (supportTool) {
            placeSupport(mw);
        } else if (linkTool) {
            int id = pickBodyForLink(mw);
            if (id < 0) { linkFirst = -1; }
            else if (linkFirst < 0) {
                linkFirst = id;
                Vec2 p;
                double a;
                bodyPose(id, p, a);
                const bool joint = tool == Tool::Pin || tool == Tool::Weld;
                const int saveMode = linkAttach;
                if (joint && linkAttach == 1) linkAttach = 0;                      // a hinge goes exactly where you click
                Vec2 n1;
                Vec2 P1 = attachPointFor(id, mw, &n1);
                linkAttach = saveMode;
                linkFirstWorld = P1;
                linkFirstNormal = n1;
                linkFirstLocal = Rot(a).applyInv(P1 - p);
            } else if (id == linkFirst) {
                say("Click a different body for the other end.");
            } else if (canEditStructure()) {
                Vec2 pa, pb;
                double aa, ab;
                bodyPose(linkFirst, pa, aa);
                bodyPose(id, pb, ab);
                (void)ab;
                LinkKind k = tool == Tool::Spring ? LinkKind::Spring : tool == Tool::Damper ? LinkKind::Damper : tool == Tool::Rod ? LinkKind::Rod : tool == Tool::Weld ? LinkKind::Weld : LinkKind::Pin;
                const bool joint = k == LinkKind::Pin || k == LinkKind::Weld;
                Vec2 wa = pa + Rot(aa).apply(linkFirstLocal), wb;
                if (joint) {
                    const int saveMode = linkAttach;
                    if (linkAttach == 1) linkAttach = 0;
                    wb = attachPointFor(id, mw, nullptr);
                    linkAttach = saveMode;
                    wa = wb;
                } else {
                    constrainedSecond(id, mw, wb);                                  // under the chosen direction: free, perpendicular, horizontal, vertical or at an angle
                }
                pushUndo();
                int li = m.addLink(k, linkFirst, id, wa, wb);
                if (li < 0) say("Those two cannot be connected (two fixed things, or the same connection already exists).", true);
                else {
                    if (k == LinkKind::Spring) m.links[li].k = nextK;
                    selLink = li;
                    selBody = -1;
                    edited();
                    afterPlace();
                }
                linkFirst = -1;
            }
        } else {
            placeAt(mw);
        }
    }

    if (dragBody >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (ses.hasWorld()) {
            if (state == RunState::Running || state == RunState::Paused) ses.world()->setMouse(dragBody, dragLocal, mw);
        } else if (canEditStructure() || state == RunState::Design) {
            Body& b = m.bodies[dragBody];
            Vec2 target = snapped(mw - Rot(b.angle).apply(dragLocal));
            if (length(target - b.pos) > 1e-9 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
                pushUndo();
                b.pos = target;
                m.refreshAutoRest();
                edited();
            }
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (ses.hasWorld()) ses.world()->clearMouse();
        dragBody = -1;
    }
}

// ====================================================================== the panel
// Reads the running world: how far every beam is bent and every compact body squashed right now, and picks the exaggeration to draw them with.
void App::updateLiveDeformation() {
    liveDef.clear();
    liveSquash.clear();
    liveFE.clear();
    if (!ses.hasWorld()) { baseW.clear(); baseLatched.clear(); smoothW.clear(); smoothStrain.clear(); smoothLoad.clear(); lastSimT = -1; return; }
    const World& w = *ses.world();
    const Model& m = w.model();
    const size_t n = m.bodies.size();
    liveDef.resize(n);
    liveSquash.resize(n);
    liveFE.resize(n);
    bool anyBeam = false;
    double cur = 0;
    for (size_t i = 0; i < n; ++i) {
        const Body& b = m.bodies[i];
        if (!b.isDynamic()) continue;
        if (isBeamBody(b)) {
            liveFE[i].build((int)i, b, m.env.deltaT);
            beamDeflection(w, liveFE[i], liveDef[i]);
            anyBeam = true;
            cur = std::max(cur, liveDef[i].maxAbs / b.shape.w);
        } else {
            liveSquash[i] = bodySquash(w, (int)i);
            if (!anyBeam) cur = std::max(cur, liveSquash[i].strain);
        }
    }
    if (ses.simTime() + 1e-9 < lastSimT) curFilt = 0;
    curFilt += (cur - curFilt) * (1.0 - std::exp(-std::min(0.1, (double)ImGui::GetIO().DeltaTime) / 0.05));
    if (state == RunState::Running || state == RunState::Computing || state == RunState::Paused) deformPeak = std::max(deformPeak, curFilt);
    // The picture follows the physics with a time constant of 0.55 s: after a block lands the sag builds up over about 2.5 s
    // (95 % in 1.7 s, 99 % in 2.5 s) so the collision and the two bodies deforming together can be watched.
    {
        const double alpha = 1.0 - std::exp(-std::min(0.1, (double)ImGui::GetIO().DeltaTime) / 0.55);
        if (ses.simTime() + 1e-9 < lastSimT) { baseW.assign(n, {}); baseLatched.assign(n, 0); }
        if (ses.simTime() + 1e-9 < lastSimT || smoothW.size() != n) { smoothW.assign(n, {}); smoothStrain.assign(n, 0.0); smoothLoad.assign(n, 0.0); }
        lastSimT = ses.simTime();
        liveTarget.assign(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            if (!m.bodies[i].isDynamic()) continue;
            if (liveFE[i].valid) {
                Deflection& d = liveDef[i];
                liveTarget[i] = d.maxAbs;
                std::vector<double>& sw = smoothW[i];
                if (sw.size() != d.w.size()) sw.assign(d.w.size(), 0.0);
                d.maxAbs = d.maxSigned = d.atX = 0;
                for (size_t k = 0; k < d.w.size(); ++k) {
                    sw[k] += (d.w[k] - sw[k]) * alpha;
                    d.w[k] = sw[k];
                    if (std::fabs(sw[k]) > d.maxAbs) { d.maxAbs = std::fabs(sw[k]); d.maxSigned = sw[k]; d.atX = liveFE[i].x[k]; }
                }
            } else {
                Squash& s = liveSquash[i];
                smoothStrain[i] += (s.strain - smoothStrain[i]) * alpha;
                smoothLoad[i] += (s.load - smoothLoad[i]) * alpha;
                if (s.strain > 0) s.compression *= smoothStrain[i] / s.strain; else s.compression = 0;
                s.strain = smoothStrain[i];
                s.load = smoothLoad[i];
            }
        }
    }
    const double target = anyBeam ? 0.06 : 0.12;                      // automatic: the worst bend shows as 6 % of the beam length
    // bending below 0.1 micrometre per metre of length is numerical dust, not something to magnify: draw it true-scale
    deformFactor = deformAuto ? (deformPeak > 1e-7 ? clampd(target / deformPeak, 1.0, 1e6) : 1.0) : (double)deformExag;
}

void App::drawCanvas() {
    updateLiveDeformation();
    const Pal pal = palette();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImVec2 p0 = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    if (size.x < 50) size.x = 50;
    if (size.y < 50) size.y = 50;
    canvasMin = p0;
    canvasMax = ImVec2(p0.x + size.x, p0.y + size.y);
    ImGui::SetNextItemAllowOverlap();                                         // the Run buttons and the deformation controls sit on top of the canvas and must get their clicks
    ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    if (hovered && (ImGui::IsMouseHoveringRect(overlayMin, overlayMax, false) || ImGui::IsMouseHoveringRect(toolbarMin, toolbarMax, false) || ImGui::IsMouseHoveringRect(promptMin, promptMax, false))) hovered = false;   // the deformation controls sit on the canvas
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasMin, canvasMax, true);
    modeClock += ImGui::GetIO().DeltaTime;

    if (needFit && canvasMax.x - canvasMin.x > 100) { fitView(); needFit = false; }
    canvasInteract(ImGui::GetIO().MousePos, hovered);
    Model& m = design();

    // ---- background and grid
    dl->AddRectFilled(canvasMin, canvasMax, pal.bg);
    if (showGrid) {
        double step = 0.1;
        const double steps[] = {0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 50};
        for (double s : steps) { step = s; if (s * scale >= 22) break; }
        Vec2 w0 = toWorld(canvasMin), w1 = toWorld(canvasMax);
        for (double x = std::floor(w0.x / step) * step; x <= w1.x; x += step) {
            bool major = std::fabs(std::fmod(std::round(x / step), 5.0)) < 1e-6;
            float sx = toScreen(Vec2(x, 0)).x;
            dl->AddLine(ImVec2(sx, canvasMin.y), ImVec2(sx, canvasMax.y), std::fabs(x) < 1e-9 ? pal.gridAxis : (major ? pal.gridMajor : pal.gridMinor));
            if (major && showGridNumbers) { char b[24]; std::snprintf(b, sizeof b, "%g", x); dl->AddText(ImVec2(sx + 3, canvasMax.y - 16), pal.gridText, b); }
        }
        for (double y = std::floor(w0.y / step) * step; y <= w1.y; y += step) {
            bool major = std::fabs(std::fmod(std::round(y / step), 5.0)) < 1e-6;
            float sy = toScreen(Vec2(0, y)).y;
            dl->AddLine(ImVec2(canvasMin.x, sy), ImVec2(canvasMax.x, sy), std::fabs(y) < 1e-9 ? pal.gridAxis : (major ? pal.gridMajor : pal.gridMinor));
            if (major && showGridNumbers) { char b[24]; std::snprintf(b, sizeof b, "%g", y); dl->AddText(ImVec2(canvasMin.x + 4, sy + 2), pal.gridText, b); }
        }
    }

    // ---- links
    double forceRef = 5;
    if (ses.hasWorld())
        for (size_t i = 0; i < m.links.size(); ++i)
            if (m.links[i].kind == LinkKind::Spring) forceRef = std::max(forceRef, std::fabs(ses.world()->link((int)i).force) * 1.6);
    double reacRef = 1;
    if (ses.hasWorld())
        for (size_t i = 0; i < m.links.size(); ++i)
            if (m.links[i].kind != LinkKind::Spring && m.links[i].kind != LinkKind::Damper && m.links[i].kind != LinkKind::Rod) reacRef = std::max(reacRef, ses.world()->link((int)i).force);
    for (size_t i = 0; i < m.links.size(); ++i) {
        const Link& l = m.links[i];
        Vec2 pa, pb;
        double aa, ab;
        if (!bodyPose(l.a, pa, aa) || !bodyPose(l.b, pb, ab)) continue;
        ImVec2 sa = toScreen(pa + Rot(aa).apply(l.la)), sb = toScreen(pb + Rot(ab).apply(l.lb));
        bool sel = (int)i == selLink;
        ImU32 col = sel ? IM_COL32(90, 205, 255, 255) : pal.link;
        float th = sel ? 3.0f : 2.0f;
        if (l.kind == LinkKind::Spring) {
            if (ses.hasWorld() && !sel) {                         // colour by load: warm = pulling, cool = pushing
                double f = clampd(ses.world()->link((int)i).force / forceRef, -1, 1);
                col = f >= 0 ? IM_COL32((int)(225 + 30 * f), (int)(228 - 120 * f), (int)(238 - 200 * f), 255) : IM_COL32((int)(225 + 140 * f), (int)(228 + 20 * f), 245, 255);
            }
            drawSpring(dl, sa, sb, col, th);
        } else if (l.kind == LinkKind::Damper) {
            drawDamper(dl, sa, sb, sel ? col : pal.mid, th);
        } else if (l.kind == LinkKind::Rod) {
            dl->AddLine(sa, sb, sel ? col : IM_COL32(210, 190, 120, 255), th + 2);
            dl->AddCircleFilled(sa, 4, IM_COL32(40, 40, 40, 255));
            dl->AddCircleFilled(sb, 4, IM_COL32(40, 40, 40, 255));
        } else if (l.kind == LinkKind::Weld) {
            dl->AddRectFilled(ImVec2(sa.x - 6, sa.y - 6), ImVec2(sa.x + 6, sa.y + 6), IM_COL32(30, 30, 30, 255));
            dl->AddRect(ImVec2(sa.x - 6, sa.y - 6), ImVec2(sa.x + 6, sa.y + 6), sel ? col : IM_COL32(235, 235, 240, 255), 0.0f, 2.0f);
        } else if (l.kind == LinkKind::Roller) {
            dl->AddCircleFilled(sb, 5, IM_COL32(30, 30, 30, 255));
            dl->AddCircle(sb, 5, sel ? col : IM_COL32(235, 235, 240, 255), 14, 2.0f);
        } else {
            dl->AddCircleFilled(sa, 6, IM_COL32(30, 30, 30, 255));
            dl->AddCircle(sa, 6, sel ? col : IM_COL32(235, 235, 240, 255), 16, 2.0f);
        }
        if (showReactions && ses.hasWorld() && l.kind != LinkKind::Spring && l.kind != LinkKind::Damper && l.kind != LinkKind::Rod) {   // reaction force arrow at a joint
            LinkReading rd = ses.world()->link((int)i);
            if (rd.force > 1e-6) drawArrow(dl, sa, ImVec2(sa.x + (float)(rd.rx / reacRef) * 70.0f, sa.y + (float)(rd.ry / reacRef) * 70.0f), IM_COL32(255, 170, 60, 230));
        }
    }

    // ---- bodies (static first, then moving ones, then anchors)
    for (int pass = 0; pass < 3; ++pass) {
        for (size_t i = 0; i < m.bodies.size(); ++i) {
            const Body& b = m.bodies[i];
            int want = b.kind == BodyKind::Static ? 0 : (b.kind == BodyKind::Dynamic ? 1 : 2);
            if (want != pass) continue;
            Vec2 p;
            double a;
            bodyPose((int)i, p, a);
            bool sel = (int)i == selBody;
            ImU32 fill = argbToImU32(b.color);
            ImU32 line = sel ? IM_COL32(90, 205, 255, 255) : IM_COL32(12, 14, 18, 255);
            float lw = sel ? 3.0f : 1.6f;
            ImVec2 c = toScreen(p);
            const bool beamDef = showDeform && ses.hasWorld() && i < liveFE.size() && liveFE[i].valid && liveDef[i].w.size() == liveFE[i].x.size();
            // a compact body pressed by a contact: shorten it along the load line (and widen it a little) by the exaggerated strain
            auto squashed = [&](std::vector<ImVec2>& out) {
                if (!showDeform || !ses.hasWorld() || i >= liveSquash.size() || !b.isDynamic() || beamDef) return false;
                const Squash& sq = liveSquash[i];
                double e = std::min(0.5, sq.strain * deformFactor);
                const bool squash = e >= 5e-4;
                // REALISTIC: a body resting on a bending beam sinks with it and its underside takes the shape of the bent surface
                int bj = -1;
                double bestF = 0;
                if (deformRealistic && !b.shape.isCircle() && b.shape.preset != ShapePreset::Line)
                    for (const ContactInfo& ci : ses.world()->contacts()) {
                        int o = ci.a == (int)i ? ci.b : (ci.b == (int)i ? ci.a : -1);
                        if (o >= 0 && o < (int)liveFE.size() && liveFE[o].valid && ci.normalImpulse > bestF) { bestF = ci.normalImpulse; bj = o; }
                    }
                if (!squash && bj < 0) return false;
                std::vector<Vec2> r;                                              // vertex offsets from the body's centre (world axes)
                if (b.shape.isCircle()) for (int k = 0; k < 40; ++k) r.push_back(Vec2(std::cos(k * 2 * kPi / 40), std::sin(k * 2 * kPi / 40)) * b.shape.radius);
                else for (const Vec2& v : b.shape.verts) r.push_back(Rot(a).apply(v));
                if (squash) {
                    Vec2 ax = sq.axis, pr = perp(ax);
                    for (Vec2& v : r) { double u = dot(v, ax), q = dot(v, pr); v = ax * (u * (1 - e)) + pr * (q * (1 + 0.3 * e)); }
                }
                std::vector<Vec2> world;
                bool conformed = false;
                if (bj >= 0) {
                    Vec2 pj;
                    double aj;
                    bodyPose(bj, pj, aj);
                    const BeamFE& fe = liveFE[bj];
                    const Deflection& dd = liveDef[bj];
                    Rot rj(aj);
                    Vec2 exj = rj.apply(Vec2(1, 0)), eyj = rj.apply(Vec2(0, 1));
                    const double k = deformFactor, tol = 0.15 * b.shape.boundingRadius();
                    auto wAt = [&](double x) {                                       // beam deflection at x, interpolated between nodes
                        double f = (x + fe.L / 2) / fe.L * (double)(fe.x.size() - 1);
                        int n0 = std::clamp((int)std::floor(f), 0, (int)fe.x.size() - 2);
                        double t = clampd(f - n0, 0.0, 1.0);
                        return dd.w[n0] * (1 - t) + dd.w[n0 + 1] * t;
                    };
                    const size_t nv = r.size();
                    std::vector<char> onFace(nv, 0);
                    std::vector<double> lx(nv);
                    double xa = 1e30, xb = -1e30;
                    int faceCount = 0;
                    for (size_t q = 0; q < nv; ++q) {
                        Vec2 u = p + r[q] - pj;
                        lx[q] = dot(u, exj);
                        double ly = dot(u, eyj);
                        if (std::fabs(ly + fe.h / 2) < tol && std::fabs(lx[q]) <= fe.L / 2 + tol) { onFace[q] = 1; ++faceCount; xa = std::min(xa, lx[q]); xb = std::max(xb, lx[q]); }
                    }
                    if (faceCount >= 2 && xb - xa > 1e-3 && faceCount < (int)nv) {
                        const Vec2 T = eyj * (k * wAt(0.5 * (xa + xb)));            // the whole body goes down with the beam
                        size_t s0 = 0;
                        while (onFace[s0]) ++s0;                                      // start the walk on a vertex that is not on the face
                        std::vector<Vec2> pts;
                        bool done = false;
                        double firstX = 0, lastX = 0;
                        for (size_t q = 0; q < nv; ++q) {
                            size_t id = (s0 + q) % nv;
                            if (onFace[id]) { if (lastX == 0 && firstX == 0) firstX = lx[id]; lastX = lx[id]; }
                        }
                        for (size_t q = 0; q < nv; ++q) {
                            size_t id = (s0 + q) % nv;
                            if (!onFace[id]) { pts.push_back(p + r[id] + T); continue; }
                            if (done) continue;
                            done = true;                                               // the face becomes a polyline lying on the bent beam
                            const int NS = 14;
                            for (int m = 0; m <= NS; ++m) {
                                double x = firstX + (lastX - firstX) * m / NS;
                                pts.push_back(pj + exj * x + eyj * (k * wAt(x) - fe.h / 2));
                            }
                        }
                        world = pts;
                        conformed = true;
                    }
                }
                if (!conformed) {
                    if (!squash) return false;
                    for (const Vec2& v : r) world.push_back(p + v);
                }
                for (const Vec2& v : world) out.push_back(toScreen(v));
                return true;
            };
            std::vector<ImVec2> sqPts;
            if (beamDef) {                                                              // a beam: drawn bent, coloured by bending stress
                const BeamFE& fe = liveFE[i];
                const Deflection& dd = liveDef[i];
                const int NN = (int)fe.x.size();
                const double k = deformFactor, l = fe.L / (NN - 1);
                // thermal growth about the held end, exaggerated like the bending (own factor so a 0.1 % growth is visible)
                const double eps = dd.epsT, tfac = deformAuto ? clampd(0.03 / std::max(std::fabs(eps), 1e-12), 1.0, 2.0e4) : (double)deformExag;
                const double sx = 1.0 + eps * tfac, anc = dd.anchorX;
                auto xs = [&](int j) { return anc + (fe.x[j] - anc) * sx; };
                Rot rr(a);
                Vec2 ex = rr.apply(Vec2(1, 0)), ey = rr.apply(Vec2(0, 1));
                std::vector<Vec2> top(NN), bot(NN);
                std::vector<double> kap(NN, 0.0);
                double kmax = 1e-30;
                for (int j = 0; j < NN; ++j) {
                    double slope = j == 0 ? (dd.w[1] - dd.w[0]) / l : (j == NN - 1 ? (dd.w[j] - dd.w[j - 1]) / l : (dd.w[j + 1] - dd.w[j - 1]) / (2 * l));
                    Vec2 cl = p + ex * xs(j) + ey * (k * dd.w[j]);
                    Vec2 nn = normalized(ey - ex * (k * slope));
                    top[j] = cl - nn * (fe.h / 2);
                    bot[j] = cl + nn * (fe.h / 2);
                    if (j > 0 && j < NN - 1) { kap[j] = std::fabs(dd.w[j - 1] - 2 * dd.w[j] + dd.w[j + 1]) / (l * l); kmax = std::max(kmax, kap[j]); }
                }
                kap[0] = kap[1];
                kap[NN - 1] = kap[NN - 2];
                if (k > 1.0001 && dd.maxAbs > 0) {                                      // the undeformed outline, so the movement is visible
                    ImVec2 g[4] = {toScreen(p - ex * (fe.L / 2) - ey * (fe.h / 2)), toScreen(p + ex * (fe.L / 2) - ey * (fe.h / 2)), toScreen(p + ex * (fe.L / 2) + ey * (fe.h / 2)), toScreen(p - ex * (fe.L / 2) + ey * (fe.h / 2))};
                    dl->AddPolyline(g, 4, IM_COL32(150, 160, 185, 110), 1.0f, ImDrawFlags_Closed);
                }
                ImVec4 base = ImGui::ColorConvertU32ToFloat4(fill), hot(1.0f, 0.30f, 0.15f, 1.0f);
                for (int j = 0; j + 1 < NN; ++j) {
                    // stress colours only in Realistic mode, and only when the bending is big enough to matter (1e-9 .. 1e-6 of the length fades in)
                    const double rel = dd.maxAbs / fe.L, signif = clampd((std::log10(std::max(rel, 1e-30)) + 9.0) / 3.0, 0.0, 1.0);
                    float t = m.env.realistic ? (float)(signif * std::min(1.0, 0.5 * (kap[j] + kap[j + 1]) / kmax)) : 0.0f;
                    ImVec4 cc(base.x + (hot.x - base.x) * t, base.y + (hot.y - base.y) * t, base.z + (hot.z - base.z) * t, 1.0f);
                    ImVec2 q[4] = {toScreen(top[j]), toScreen(top[j + 1]), toScreen(bot[j + 1]), toScreen(bot[j])};
                    dl->AddConvexPolyFilled(q, 4, ImGui::ColorConvertFloat4ToU32(cc));
                }
                std::vector<ImVec2> outline;
                for (int j = 0; j < NN; ++j) outline.push_back(toScreen(top[j]));
                for (int j = NN - 1; j >= 0; --j) outline.push_back(toScreen(bot[j]));
                dl->AddPolyline(outline.data(), (int)outline.size(), line, lw, ImDrawFlags_Closed);
                dl->AddCircleFilled(c, 2.5f, IM_COL32(10, 12, 16, 220));
            } else if (b.kind == BodyKind::Anchor) {
                LinkKind sup = LinkKind::Spring;                                       // is this anchor a support symbol?
                for (const Link& l : m.links)
                    if (l.a == (int)i && l.kind != LinkKind::Spring && l.kind != LinkKind::Damper && l.kind != LinkKind::Rod) { sup = l.kind; break; }
                const float ca = (float)std::cos(a), sa_ = (float)std::sin(a);
                auto R = [&](float x, float y) { return ImVec2(c.x + x * ca - y * sa_, c.y + x * sa_ + y * ca); };
                ImU32 ink = sel ? IM_COL32(90, 205, 255, 255) : pal.ink;
                if (sup == LinkKind::Weld) {                                           // fixed support: a hatched wall block
                    dl->AddRectFilled(R(-16, 0), R(16, 0), ink);
                    ImVec2 q[4] = {R(-16, 0), R(16, 0), R(16, 8), R(-16, 8)};
                    dl->AddConvexPolyFilled(q, 4, IM_COL32(70, 76, 90, 255));
                    for (int h = -16; h <= 12; h += 6) dl->AddLine(R((float)h, 8), R((float)h + 6, 0), ink, 1.3f);
                    dl->AddLine(R(-16, 0), R(16, 0), ink, 3.0f);
                } else if (sup == LinkKind::Roller) {                                  // roller support: triangle on two wheels
                    dl->AddTriangleFilled(c, R(-10, 14), R(10, 14), IM_COL32(120, 126, 140, 255));
                    dl->AddCircle(R(-5, 19), 4.5f, ink, 12, 1.8f);
                    dl->AddCircle(R(5, 19), 4.5f, ink, 12, 1.8f);
                    dl->AddLine(R(-16, 24), R(16, 24), ink, 2.0f);
                    for (int h = -16; h <= 12; h += 6) dl->AddLine(R((float)h, 30), R((float)h + 6, 24), ink, 1.2f);
                } else if (sup == LinkKind::Pin) {                                     // pin support: triangle on hatched ground
                    dl->AddTriangleFilled(c, R(-10, 14), R(10, 14), IM_COL32(120, 126, 140, 255));
                    dl->AddLine(R(-16, 14), R(16, 14), ink, 2.0f);
                    for (int h = -16; h <= 12; h += 6) dl->AddLine(R((float)h, 20), R((float)h + 6, 14), ink, 1.2f);
                } else {
                    dl->AddTriangleFilled(ImVec2(c.x - 9, c.y + 14), ImVec2(c.x + 9, c.y + 14), ImVec2(c.x, c.y), IM_COL32(120, 126, 140, 255));
                    dl->AddLine(ImVec2(c.x - 12, c.y + 14), ImVec2(c.x + 12, c.y + 14), ink, 2.0f);
                }
                dl->AddCircleFilled(c, 5, sel ? IM_COL32(90, 205, 255, 255) : IM_COL32(235, 238, 245, 255));
                dl->AddCircle(c, 5, IM_COL32(10, 10, 12, 255), 12, 1.5f);
            } else if (b.shape.preset == ShapePreset::Line) {                          // a floor: a bright line with hatching on its underside
                Rot rr(a);
                ImVec2 e0 = toScreen(p + rr.apply(Vec2(-b.shape.w / 2, 0))), e1 = toScreen(p + rr.apply(Vec2(b.shape.w / 2, 0)));
                ImU32 ink = sel ? IM_COL32(90, 205, 255, 255) : pal.ink;
                float L = std::sqrt((e1.x - e0.x) * (e1.x - e0.x) + (e1.y - e0.y) * (e1.y - e0.y));
                if (L > 1) {
                    ImVec2 u((e1.x - e0.x) / L, (e1.y - e0.y) / L), nn(-u.y, u.x);          // nn: screen-side of local +y (the hatched side)
                    ImVec2 side = ImVec2((float)(rr.apply(Vec2(0, 1)).x), (float)(rr.apply(Vec2(0, 1)).y));
                    if (nn.x * side.x + nn.y * side.y < 0) { nn.x = -nn.x; nn.y = -nn.y; }
                    // only hatch the part that is on screen (an "infinite" floor is thousands of metres long)
                    float tLo = 1e30f, tHi = -1e30f;
                    for (int cx = 0; cx < 2; ++cx) for (int cy = 0; cy < 2; ++cy) {
                        float tt = ((cx ? canvasMax.x : canvasMin.x) - e0.x) * u.x + ((cy ? canvasMax.y : canvasMin.y) - e0.y) * u.y;
                        tLo = std::min(tLo, tt); tHi = std::max(tHi, tt);
                    }
                    float t0 = std::max(0.0f, std::floor((tLo - 20.0f) / 11.0f) * 11.0f), t1 = std::min(L, tHi + 20.0f);
                    for (float t = t0; t <= t1; t += 11.0f) {
                        ImVec2 q0(e0.x + u.x * t, e0.y + u.y * t);
                        dl->AddLine(q0, ImVec2(q0.x + nn.x * 9.0f - u.x * 7.0f, q0.y + nn.y * 9.0f - u.y * 7.0f), pal.dim, 1.3f);
                    }
                }
                dl->AddLine(e0, e1, ink, sel ? 4.0f : 3.0f);
            } else if (squashed(sqPts)) {
                dl->AddConvexPolyFilled(sqPts.data(), (int)sqPts.size(), fill);
                dl->AddPolyline(sqPts.data(), (int)sqPts.size(), line, lw, ImDrawFlags_Closed);
                dl->AddCircleFilled(c, 2.5f, IM_COL32(10, 12, 16, 220));
            } else if (b.shape.isCircle()) {
                float r = (float)(b.shape.radius * scale);
                dl->AddCircleFilled(c, r, fill, 48);
                dl->AddCircle(c, r, line, 48, lw);
                Vec2 e = p + Rot(a).apply(Vec2(b.shape.radius, 0));
                dl->AddLine(c, toScreen(e), IM_COL32(10, 12, 16, 200), 2.0f);          // a radius line shows the rotation
                dl->AddCircleFilled(c, 2.5f, IM_COL32(10, 12, 16, 220));
            } else {
                std::vector<ImVec2> pts;
                Rot r(a);
                for (const Vec2& v : b.shape.verts) pts.push_back(toScreen(p + r.apply(v)));
                dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), fill);
                dl->AddPolyline(pts.data(), (int)pts.size(), line, lw, ImDrawFlags_Closed);
                if (b.kind == BodyKind::Static) {                                       // hatching marks a fixed object
                    for (size_t k = 0; k + 1 < pts.size(); ++k) (void)k;
                    ImVec2 q0 = pts[0], q1 = pts[1];
                    (void)q0; (void)q1;
                }
                dl->AddCircleFilled(c, 2.5f, IM_COL32(10, 12, 16, 220));
            }
            if (b.isDynamic() && b.shape.preset == ShapePreset::Box && (b.dloadType != DL_None || (sel && m.env.deadLoad && isBeamBody(b)))) {
                Rot rr(a);
                const Vec2 exw = rr.apply(Vec2(1, 0)), eyw = rr.apply(Vec2(0, 1));
                const double L = b.shape.w;
                auto diagram = [&](Vec2 dir, int n, double x0, double x1, std::function<double(double)> qf, ImU32 col, const char* name) {
                    double qmax = 1e-12;
                    for (int k = 0; k <= 60; ++k) qmax = std::max(qmax, std::fabs(qf(x0 + (x1 - x0) * k / 60.0)));
                    std::vector<ImVec2> env;
                    for (int k = 0; k <= n; ++k) {
                        double xm = x0 + (x1 - x0) * k / n, q = qf(xm);
                        if (std::fabs(q) < 1e-9 * qmax) continue;
                        Vec2 de = q > 0 ? dir : dir * -1.0;
                        double half = std::fabs(dot(dir, eyw)) * b.shape.h / 2;
                        Vec2 head = p + exw * (xm - L / 2) - de * half;
                        double len = (8.0 + 46.0 * std::fabs(q) / qmax) / scale;
                        Vec2 tail = head - de * len;
                        drawArrow(dl, toScreen(tail), toScreen(head), col);
                        env.push_back(toScreen(tail));
                    }
                    if (env.size() > 1) dl->AddPolyline(env.data(), (int)env.size(), col, 1.5f, 0);
                    if (!env.empty()) dl->AddText(ImVec2(env.front().x, env.front().y - 18), col, name);
                };
                if (b.dloadType != DL_None) {
                    std::string err;
                    std::shared_ptr<const Expr> ex = b.dloadType == DL_Equation ? Expr::cached(b.dloadExpr, &err) : nullptr;
                    const double tnow2 = ses.hasWorld() ? ses.simTime() : 0.0;
                    double x0, x1;
                    distributedLoadSpan(b, &x0, &x1);
                    const double ang = b.dloadAngleDeg * kPi / 180.0;
                    char nm[96];
                    if (b.dloadType == DL_Equation) std::snprintf(nm, sizeof nm, "q(x) = %s", b.dloadExpr.c_str());
                    else std::snprintf(nm, sizeof nm, "%.0f N/m (%.1f kg/m)", b.dloadN, b.dloadN / 9.80665);
                    diagram(Vec2(std::cos(ang), std::sin(ang)), 16, x0, x1, [&](double x) { return distributedLoadAt(b, ex.get(), x, tnow2); }, IM_COL32(255, 150, 40, 255), nm);
                }
                if (sel && m.env.deadLoad && isBeamBody(b)) {
                    const double mu = b.massKg / L, w = mu * m.env.gravity;
                    char nm[96];
                    std::snprintf(nm, sizeof nm, "dead load %.0f N/m (%.1f kg/m)", w, mu);
                    diagram(Vec2(0, 1), 10, 0, L, [&](double) { return w; }, pal.mid, nm);
                }
            }
            if (b.isDynamic() && b.loadN > 0) {                                       // an applied load: an arrow from the centre, longer for a bigger load
                double a = (b.loadAngleDeg + (b.loadPull ? 180.0 : 0.0)) * kPi / 180.0;
                float len = 46.0f + 22.0f * (float)std::log10(1.0 + b.loadN / 10.0);
                ImVec2 tip(c.x + (float)std::cos(a) * len, c.y + (float)std::sin(a) * len);
                ImU32 lc = IM_COL32(255, 150, 40, 255);
                drawArrow(dl, c, tip, lc);
                char lb[64];
                std::snprintf(lb, sizeof lb, "%s %.0f N (%.1f kg)", b.loadPull ? "pull" : "push", b.loadN, b.loadN / 9.80665);
                dl->AddText(ImVec2(tip.x + 6, tip.y - 8), lc, lb);
            }
            if (showVectors && ses.hasWorld() && b.isDynamic()) {
                const BodyState& s = ses.world()->state()[i];
                drawArrow(dl, c, toScreen(p + s.vel * 0.25), IM_COL32(80, 220, 120, 255));
            }
        }
    }

    // ---- legend (bottom left): replaces the names that used to float over every body
    if (showLabels && !m.bodies.empty()) {
        struct Row { ImU32 col; std::string text; };
        std::vector<Row> rows;
        auto baseName = [](std::string n) {                                            // "Domino 7", "Block 2-1" -> "Domino", "Block"
            while (!n.empty() && (std::isdigit((unsigned char)n.back()) || n.back() == '-' || n.back() == ' ')) n.pop_back();
            return n.empty() ? std::string("Body") : n;
        };
        std::vector<std::string> seen;
        for (size_t i = 0; i < m.bodies.size(); ++i) {
            const Body& b = m.bodies[i];
            if (b.kind == BodyKind::Anchor) continue;
            std::string bn = baseName(b.name);
            int count = 0;
            for (const Body& o : m.bodies) if (o.kind != BodyKind::Anchor && baseName(o.name) == bn) ++count;
            if (std::find(seen.begin(), seen.end(), bn) != seen.end()) continue;
            seen.push_back(bn);
            char t[160];
            const std::string shown = count > 1 ? bn : b.name;
            if (b.kind == BodyKind::Static) std::snprintf(t, sizeof t, "%s%s   mu %.2f", shown.c_str(), count > 1 ? (" x" + std::to_string(count)).c_str() : "", b.mat.muK);
            else std::snprintf(t, sizeof t, "%s%s   %.3g kg%s", shown.c_str(), count > 1 ? (" x" + std::to_string(count)).c_str() : "", b.massKg, count > 1 ? " each" : "");
            ImU32 sw = b.kind == BodyKind::Static ? pal.ink : argbToImU32(b.color);
            rows.push_back({sw, t});
        }
        const float rowH = 19.0f;
        const int maxRows = std::max(2, (int)((canvasMax.y - canvasMin.y - 190) / rowH));
        size_t shownRows = std::min<size_t>(rows.size(), (size_t)maxRows);
        float w = 120;
        for (size_t k = 0; k < shownRows; ++k) w = std::max(w, ImGui::CalcTextSize(rows[k].text.c_str()).x + 34);
        std::string more;
        if (rows.size() > shownRows) { more = "+ " + std::to_string(rows.size() - shownRows) + " more"; w = std::max(w, ImGui::CalcTextSize(more.c_str()).x + 34); }
        float h = (float)(shownRows + (more.empty() ? 0 : 1)) * rowH + 10;
        ImVec2 p0(canvasMin.x + 10, canvasMax.y - 46 - h);
        dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), pal.panel, 5.0f);
        dl->AddRect(p0, ImVec2(p0.x + w, p0.y + h), pal.gridAxis, 5.0f);
        for (size_t k = 0; k < shownRows; ++k) {
            float y = p0.y + 5 + k * rowH;
            dl->AddRectFilled(ImVec2(p0.x + 8, y + 4), ImVec2(p0.x + 20, y + 15), rows[k].col, 2.0f);
            dl->AddRect(ImVec2(p0.x + 8, y + 4), ImVec2(p0.x + 20, y + 15), pal.gridAxis, 2.0f);
            dl->AddText(ImVec2(p0.x + 27, y + 1), pal.text, rows[k].text.c_str());
        }
        if (!more.empty()) dl->AddText(ImVec2(p0.x + 27, p0.y + 5 + shownRows * rowH + 1), pal.dim, more.c_str());
    }

    // ---- contacts
    if (showContacts && ses.hasWorld())
        for (const ContactInfo& ci : ses.world()->contacts()) {
            ImVec2 q = toScreen(ci.point);
            dl->AddCircleFilled(q, 3.5f, IM_COL32(255, 80, 70, 255));
            dl->AddLine(q, ImVec2(q.x + (float)ci.normal.x * 22, q.y + (float)ci.normal.y * 22), IM_COL32(255, 120, 100, 200), 1.5f);
        }

    // ---- tool preview: the ghost follows the cursor and is turned by nextAngle (Q / E, Ctrl+wheel)
    ImVec2 mp = ImGui::GetIO().MousePos;
    if (hovered && tool != Tool::Select) {
        Vec2 w = snapped(toWorld(mp));
        ImVec2 c = toScreen(w);
        ImU32 gh = IM_COL32(90, 205, 255, 200);
        const std::vector<Material>& lib = materialLibrary();
        const Material& gm = lib[std::clamp(nextMaterial, 0, (int)lib.size() - 1)];
        Rot gr(nextAngle * kPi / 180.0);
        auto ghostPoly = [&](const Body& tb) {
            std::vector<ImVec2> pts;
            for (const Vec2& v : tb.shape.verts) pts.push_back(toScreen(w + gr.apply(v)));
            dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), IM_COL32(90, 205, 255, 45));
            dl->AddPolyline(pts.data(), (int)pts.size(), gh, 2.0f, ImDrawFlags_Closed);
        };
        switch (tool) {
            case Tool::Circle: dl->AddCircle(c, (float)(nextRadius * scale), gh, 40, 2.0f); dl->AddLine(c, toScreen(w + gr.apply(Vec2(nextRadius, 0))), gh, 2.0f); break;
            case Tool::Polygon: ghostPoly(makeRegular(nextSides, nextRadius, gm, {})); break;
            case Tool::Box: ghostPoly(makeBox(nextW, nextH, gm, {})); break;
            case Tool::Wedge: ghostPoly(makeWedge(nextW, nextH, gm, {})); break;
            case Tool::Beam: ghostPoly(makeBeam(beamLength, beamThick, gm, {})); break;
            case Tool::Floor: {
                Vec2 hw = gr.apply(Vec2(floorLength / 2, 0));
                dl->AddLine(toScreen(w - hw), toScreen(w + hw), gh, 3.0f);
                break;
            }
            case Tool::Anchor: dl->AddCircle(c, 6, gh, 16, 2.0f); break;
            case Tool::SupportPin: case Tool::SupportFixed: case Tool::SupportRoller: {
                int hb = pickBody(toWorld(mp));
                if (hb >= 0 && design().bodies[hb].isDynamic()) {
                    ImVec2 q = toScreen(supportPoint(hb, toWorld(mp)));
                    dl->AddCircle(q, 9, gh, 16, 2.5f);
                    Vec2 d = gr.apply(Vec2(1, 0));
                    if (tool == Tool::SupportRoller) dl->AddLine(ImVec2(q.x - (float)d.x * 26, q.y - (float)d.y * 26), ImVec2(q.x + (float)d.x * 26, q.y + (float)d.y * 26), gh, 2.0f);
                    if (tool == Tool::SupportFixed) { Vec2 nn = gr.apply(Vec2(0, 1)); dl->AddLine(ImVec2(q.x - (float)d.x * 16, q.y - (float)d.y * 16), ImVec2(q.x + (float)d.x * 16, q.y + (float)d.y * 16), gh, 3.0f); dl->AddLine(q, ImVec2(q.x + (float)nn.x * 14, q.y + (float)nn.y * 14), gh, 1.5f); }
                } else dl->AddCircle(c, 9, IM_COL32(255, 120, 100, 160), 16, 2.0f);
                break;
            }
            default: break;
        }
        if (tool == Tool::Spring || tool == Tool::Damper || tool == Tool::Rod || tool == Tool::Pin || tool == Tool::Weld) {
            const bool joint = tool == Tool::Pin || tool == Tool::Weld;
            const int saveMode = linkAttach;
            if (joint && linkAttach == 1) linkAttach = 0;
            int hover = pickBodyForLink(toWorld(mp));
            Vec2 target = toWorld(mp), hp;
            bool haveTarget = false;
            if (hover >= 0 && hover != linkFirst) {                                 // the point it would attach to if you clicked now
                Vec2 nrm(0, -1);
                if (linkFirst >= 0 && !joint) constrainedSecond(hover, target, hp);
                else hp = attachPointFor(hover, target, &nrm);
                dl->AddCircle(toScreen(hp), 8, gh, 16, 2.5f);
                dl->AddCircleFilled(toScreen(hp), 3, gh);
                if (linkFirst < 0 && linkDir == 1 && !joint) dl->AddLine(toScreen(hp), toScreen(hp + nrm * (30.0 / scale)), gh, 1.8f);   // the face normal
                target = hp;
                haveTarget = true;
            } else if (hover >= 0 && linkFirst < 0) {
                hp = attachPointFor(hover, target, nullptr);
                dl->AddCircle(toScreen(hp), 8, gh, 16, 2.5f);
                dl->AddCircleFilled(toScreen(hp), 3, gh);
            }
            linkAttach = saveMode;
            if (linkFirst >= 0) {
                ImVec2 sa = toScreen(linkFirstWorld);
                const Vec2 ax = linkAxis();
                if (!joint && lengthSq(ax) > 0.25) {
                    dl->AddLine(toScreen(linkFirstWorld - ax * 60.0), toScreen(linkFirstWorld + ax * 60.0), IM_COL32(90, 205, 255, 70), 1.0f);   // the axis it must follow
                    if (!haveTarget) target = linkFirstWorld + ax * dot(target - linkFirstWorld, ax);
                }
                dl->AddLine(sa, toScreen(target), gh, 2.5f);
                dl->AddCircleFilled(sa, 5, gh);
                Vec2 dd = target - linkFirstWorld;
                if (length(dd) > 0.02) {
                    char ab[48];
                    double ang = std::atan2(dd.y, dd.x) * 180.0 / kPi;
                    std::snprintf(ab, sizeof ab, "%.0f deg   %.2f m", ang < 0 ? ang + 360.0 : ang, length(dd));
                    ImVec2 mid = toScreen(linkFirstWorld + dd * 0.5);
                    dl->AddText(ImVec2(mid.x + 8, mid.y - 18), IM_COL32(150, 215, 255, 240), ab);
                }
            }
        }
        if (tool != Tool::Spring && tool != Tool::Damper && tool != Tool::Rod && tool != Tool::Pin && tool != Tool::Weld) {
            char rb[48];
            std::snprintf(rb, sizeof rb, "%.0f deg   (Q / E turn)", nextAngle);
            dl->AddText(ImVec2(c.x + 14, c.y + 12), IM_COL32(150, 215, 255, 230), rb);
        }
    }

    // ---- overlays
    const char* toolNames[] = {"Select / move", "Ball", "Block", "Polygon", "Wedge", "Floor line", "Anchor point", "Beam", "Spring", "Damper", "Rigid rod", "Pin joint", "Fixed (weld) joint", "Pin support", "Fixed support (cantilever)", "Roller support"};
    char hud[200];
    std::snprintf(hud, sizeof hud, "Tool: %s", toolNames[(int)tool]);
    dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 44), pal.text, hud);
    if (tool == Tool::Select) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 62), pal.dim, state == RunState::Design ? "Click to select, drag to move.  Wheel zooms, right-drag pans." : "Drag a body to push it with the mouse.");
    else if (linkFirst >= 0) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 62), IM_COL32(90, 205, 255, 255), "Now click the second body.");
    else if (tool == Tool::Spring || tool == Tool::Damper || tool == Tool::Rod || tool == Tool::Pin || tool == Tool::Weld) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 62), pal.dim, "Click one body, then another. Esc cancels.");
    else if (tool == Tool::SupportPin || tool == Tool::SupportFixed || tool == Tool::SupportRoller) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 62), pal.dim, "Click a beam or body: the support snaps to its ends and middle. Q / E turn it. Shift keeps the tool.");
    else dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 62), pal.dim, "Click to place. Q / E (or Ctrl+wheel) rotate it first. Shift keeps placing; Esc returns to Select; Alt ignores the grid.");

    char st[160];
    double tnow = ses.hasWorld() ? ses.simTime() : (haveResults && !ses.telemetry().t.empty() ? ses.telemetry().t.back() : 0.0);
    std::snprintf(st, sizeof st, "%s   t = %.2f s / %.1f s   %s%s", stateName(), tnow, m.run.duration, m.env.realistic ? "REALISTIC" : "IDEAL", (slowRun && state == RunState::Running) ? "   (slower than real time)" : "");
    ImVec2 ts = ImGui::CalcTextSize(st);
    ImU32 sc = state == RunState::Running ? IM_COL32(110, 230, 150, 255) : (state == RunState::Design ? pal.amber : IM_COL32(120, 215, 255, 255));
    dl->AddText(ImVec2(canvasMax.x - ts.x - 12, canvasMin.y + 8), sc, st);
    if (recording) {
        dl->AddCircleFilled(ImVec2(canvasMax.x - ts.x - 26, canvasMin.y + 16), 6, IM_COL32(235, 60, 60, 255));
        char rb[64];
        std::snprintf(rb, sizeof rb, "REC %02d:%02d", (recFrames / 60) / 60, (recFrames / 60) % 60);
        dl->AddText(ImVec2(canvasMax.x - ImGui::CalcTextSize(rb).x - 12, canvasMin.y + 26), IM_COL32(255, 130, 120, 255), rb);
        dl->AddRect(canvasMin, canvasMax, IM_COL32(235, 60, 60, 160), 0, 3.0f);
    }
    if (m.bodies.empty())
        dl->AddText(ImVec2((canvasMin.x + canvasMax.x) / 2 - 190, (canvasMin.y + canvasMax.y) / 2 - 10), pal.mid, "Empty canvas: choose a shape in the Library, then click here.");
    // scale bar
    {
        double len = 1.0;
        if (scale < 30) len = 5; else if (scale > 200) len = 0.5;
        float px = (float)(len * scale);
        ImVec2 a(canvasMin.x + 12, canvasMax.y - 28);
        dl->AddLine(a, ImVec2(a.x + px, a.y), pal.mid, 2.0f);
        char sb[32];
        std::snprintf(sb, sizeof sb, "%g m", len);
        dl->AddText(ImVec2(a.x + px + 6, a.y - 8), pal.mid, sb);
    }
    if (modeShown >= 0 && haveModal && state == RunState::Design && modeShown < (int)modal.modes.size()) {
        char mb[120];
        std::snprintf(mb, sizeof mb, "Animating mode %d: %.3f Hz", modeShown + 1, modal.modes[modeShown].freqHz);
        dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 44), pal.amber, mb);
    }
    // ---- the small box for connections: where it attaches and which way it runs
    promptMin = promptMax = ImVec2(0, 0);
    if (!recording && (tool == Tool::Spring || tool == Tool::Damper || tool == Tool::Rod || tool == Tool::Pin || tool == Tool::Weld)) {
        const bool joint = tool == Tool::Pin || tool == Tool::Weld;
        const char* what = tool == Tool::Spring ? "Spring" : tool == Tool::Damper ? "Damper" : tool == Tool::Rod ? "Rigid rod" : tool == Tool::Pin ? "Pin joint" : "Fixed joint";
        ImVec2 pos(canvasMin.x + 10, canvasMin.y + 104);
        if (linkFirst >= 0) {                                                          // after the first click it sits beside the first attachment point
            ImVec2 a0 = toScreen(linkFirstWorld);
            pos = ImVec2(std::min(a0.x + 22, canvasMax.x - 360), std::min(a0.y + 16, canvasMax.y - 230));
            pos.x = std::max(pos.x, canvasMin.x + 6);
            pos.y = std::max(pos.y, canvasMin.y + 100);
        }
        const float bw = 340.0f;
        ImVec2 start = pos;
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 10, pos.y + 8));
        ImGui::BeginGroup();
        ImGui::PushID("linkprompt");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushTextWrapPos(pos.x + bw - 12 - ImGui::GetWindowPos().x);
        ImGui::PushItemWidth(bw - 20);
        ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s: %s", what, linkFirst < 0 ? "click the first body" : "now click the second body");
        ImGui::TextUnformatted("Attach to");
        ImGui::RadioButton("Edge", &linkAttach, 1); ImGui::SameLine();
        ImGui::RadioButton("Key points", &linkAttach, 2); ImGui::SameLine();
        ImGui::RadioButton("Centre", &linkAttach, 3); ImGui::SameLine();
        ImGui::RadioButton("Exact", &linkAttach, 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exact: where you click. Edge: the nearest point of the body's outline.\nKey points: corners, the middle and the thirds of an edge. Centre: the middle of the body.");
        if (!joint) {
            ImGui::TextUnformatted("Direction");
            ImGui::RadioButton("Free", &linkDir, 0); ImGui::SameLine();
            ImGui::RadioButton("Perpendicular", &linkDir, 1); ImGui::SameLine();
            ImGui::RadioButton("Horizontal", &linkDir, 2); ImGui::SameLine();
            ImGui::RadioButton("Vertical", &linkDir, 3);
            ImGui::RadioButton("Angle", &linkDir, 4);
            if (linkDir == 4) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90);
                ImGui::DragFloat("##lang", &linkAngle, 1.0f, -360.0f, 360.0f, "%.0f deg");
                ImGui::SameLine();
                if (ImGui::SmallButton("0")) linkAngle = 0;
                ImGui::SameLine();
                if (ImGui::SmallButton("45")) linkAngle = 45;
                ImGui::SameLine();
                if (ImGui::SmallButton("90")) linkAngle = 90;
                ImGui::SameLine();
                if (ImGui::SmallButton("135")) linkAngle = 135;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped(linkDir == 0 ? "Free: the %s runs straight from point to point." :
                               linkDir == 1 ? "Perpendicular: the %s leaves the first face at a right angle, and ends where that line meets the second body." :
                               "The %s follows this direction; it ends where the line meets the second body.", joint ? "joint" : "connection");
            ImGui::PopStyleColor();
        }
        ImGui::PopItemWidth();
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();
        ImGui::EndGroup();
        ImVec2 end(start.x + bw, ImGui::GetItemRectMax().y + 8);
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(start, end, pal.panel, 8.0f);
        dl->AddRect(start, end, IM_COL32(90, 205, 255, 200), 8.0f, 1.5f);
        dl->ChannelsMerge();
        promptMin = start;
        promptMax = end;
    }

    // ---- run controls (top left of the canvas): Run, Pause, Stop, Step, Compute
    toolbarMin = toolbarMax = ImVec2(0, 0);
    if (!recording) {
        const bool hasDyn = m.dynamicCount() > 0, computing = state == RunState::Computing;
        auto colored = [](float r, float g, float b) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(r, g, b, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(std::min(1.0f, r + 0.10f), std::min(1.0f, g + 0.10f), std::min(1.0f, b + 0.10f), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        };
        ImGui::SetCursorScreenPos(ImVec2(canvasMin.x + 10, canvasMin.y + 6));
        ImGui::BeginGroup();
        colored(0.14f, 0.56f, 0.30f);
        ImGui::BeginDisabled(!hasDyn || state == RunState::Running || computing);
        const char* rl = state == RunState::Paused ? "Resume" : (state == RunState::Done ? "Run again" : "Run");
        if (ImGui::Button(rl, ImVec2(104, 30))) {
            if (state == RunState::Design) startRun(true);
            else if (state == RunState::Done) { stopRun(); startRun(true); }
            else if (state == RunState::Paused) pause();
        }
        ImGui::EndDisabled();
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run in real time (Space). You can drag bodies while it runs.");
        ImGui::SameLine();
        colored(0.80f, 0.55f, 0.08f);
        ImGui::BeginDisabled(state != RunState::Running);
        if (ImGui::Button("Pause", ImVec2(88, 30))) pause();
        ImGui::EndDisabled();
        ImGui::PopStyleColor(4);
        ImGui::SameLine();
        colored(0.78f, 0.20f, 0.20f);
        ImGui::BeginDisabled(state == RunState::Design);
        if (ImGui::Button("Stop", ImVec2(88, 30))) stopRun();
        ImGui::EndDisabled();
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop and go back to the drawn start (Esc). The data of the run stays available.");
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasDyn || state == RunState::Running || computing);
        if (ImGui::Button("Step", ImVec2(70, 30))) stepOnce();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Advance by 1/60 of a second.");
        ImGui::SameLine();
        if (ImGui::Button("Compute all", ImVec2(110, 30))) { if (state == RunState::Done || state == RunState::Paused) stopRun(); startRun(false); }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Runs the whole duration as fast as the computer can, then shows all the data.");
        ImGui::EndGroup();
        toolbarMin = ImGui::GetItemRectMin();
        toolbarMax = ImGui::GetItemRectMax();
        toolbarMin.x -= 4; toolbarMin.y -= 4; toolbarMax.x += 4; toolbarMax.y += 4;
    }

    // ---- deformation controls and readout (hidden while recording so the video stays clean)
    static float lastStripW = 800.0f;
    overlayMin = overlayMax = ImVec2(0, 0);
    bool anyDeformable = false;
    for (const Body& b : m.bodies) anyDeformable = anyDeformable || b.isDynamic();
    if (!recording && anyDeformable) {
        // the readout lines are collected first so the control strip can sit at the bottom right with the lines stacked above it
        struct Ln { std::string t; ImU32 c; };
        std::vector<Ln> lines;
        if (showDeform) {
            char db[200];
            if (!ses.hasWorld()) {
                lines.push_back({"Run the model to see bodies bend (beams) and squash (blocks).", pal.mid});
            } else {
                std::snprintf(db, sizeof db, "Drawn x%.0f larger than real%s", deformFactor, deformFactor > 1.5 ? "  (grey outline = undeformed)" : "  (true scale)");
                lines.push_back({db, pal.amber});
                std::vector<size_t> order;                                           // the three most bent bodies
                for (size_t i = 0; i < m.bodies.size() && i < liveDef.size(); ++i)
                    if (m.bodies[i].isDynamic() && i < liveFE.size() && liveFE[i].valid) order.push_back(i);
                std::sort(order.begin(), order.end(), [&](size_t p, size_t q) { return liveDef[p].maxAbs / liveFE[p].L > liveDef[q].maxAbs / liveFE[q].L; });
                if (order.size() > 3) order.resize(3);
                for (size_t i : order) {
                    double real = i < liveTarget.size() ? liveTarget[i] : liveDef[i].maxAbs;       // the physical value, not the eased drawing
                    double mm = real * 1000.0;
                    if (real < 1e-12) std::snprintf(db, sizeof db, "%s: not loaded, drawn straight", m.bodies[i].name.c_str());
                    else std::snprintf(db, sizeof db, "%s bends %.4g %s at x = %.2f m   (L / %.0f)", m.bodies[i].name.c_str(), mm < 0.1 ? mm * 1000.0 : mm, mm < 0.1 ? "um" : "mm", liveDef[i].atX, liveFE[i].L / real);
                    lines.push_back({db, pal.text});
                    if (std::fabs(liveDef[i].epsT) > 0) {                                   // thermal expansion
                        const double e0 = liveDef[i].epsT;
                        if (liveDef[i].restrained) std::snprintf(db, sizeof db, "%s: held at both ends, heating gives axial stress %.1f MPa", m.bodies[i].name.c_str(), liveDef[i].axialSigma / 1e6);
                        else std::snprintf(db, sizeof db, "%s: free to grow %.3f mm (%.0f microstrain)", m.bodies[i].name.c_str(), e0 * liveFE[i].L * 1000.0, e0 * 1e6);
                        lines.push_back({db, IM_COL32(255, 150, 60, 255)});
                    }
                }
            }
        }
        const float y0 = canvasMax.y - 34;                                           // the strip: bottom right of the canvas
        float stripW = lastStripW;
        ImGui::SetCursorScreenPos(ImVec2(std::max(canvasMin.x + 10, canvasMax.x - stripW - 12), y0));
        ImGui::BeginGroup();
        ImGui::Checkbox("Show deformation", &showDeform);
        if (showDeform) {
            ImGui::SameLine();
            ImGui::Checkbox("Realistic", &deformRealistic);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Blocks sink with the beam and their underside takes the shape of its bent surface, so the two deform together.");
            ImGui::SameLine();
            if (ImGui::Checkbox("Dead load", &design().env.deadLoad)) edited();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Optional: include each beam's own weight (its mass per metre x g) as a distributed load in the bending. Off (the default): only the loads you apply bend it.");
            ImGui::SameLine();
            ImGui::Checkbox("Auto", &deformAuto);
            ImGui::SameLine();
            ImGui::BeginDisabled(deformAuto);
            ImGui::SetNextItemWidth(170);
            ImGui::SliderFloat("##exag", &deformExag, 1.0f, 1.0e7f, "exaggerate x %.0f", ImGuiSliderFlags_Logarithmic);
            ImGui::EndDisabled();
            const struct { const char* n; float v; } presets[] = {{"True", 1}, {"x10", 10}, {"x100", 100}, {"x1k", 1e3}, {"x10k", 1e4}, {"x100k", 1e5}};
            for (const auto& ps : presets) {
                ImGui::SameLine();
                if (ImGui::SmallButton(ps.n)) { deformAuto = false; deformExag = ps.v; }
            }
        }
        ImGui::EndGroup();
        overlayMin = ImGui::GetItemRectMin();
        overlayMax = ImGui::GetItemRectMax();
        lastStripW = overlayMax.x - overlayMin.x;
        overlayMin.x -= 4; overlayMin.y -= 4; overlayMax.x += 4; overlayMax.y += 4;
        float yy = y0 - 8 - 16.0f * (float)lines.size();                             // the readout, right aligned, just above the strip
        for (const Ln& l : lines) {
            dl->AddText(ImVec2(canvasMax.x - ImGui::CalcTextSize(l.t.c_str()).x - 14, yy), l.c, l.t.c_str());
            yy += 16;
        }
    }
    dl->PopClipRect();
    ImGui::End();
}

}  // namespace vlab
