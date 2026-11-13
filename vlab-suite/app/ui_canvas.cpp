// ui_canvas.cpp -- the drawing area: shapes, floors, springs, dampers, joints; tools; mouse interaction.
#include <algorithm>
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
                Vec2 q = b.pos + rr.apply(v);
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
            int id = pickBody(mw);
            if (id < 0) { linkFirst = -1; }
            else if (linkFirst < 0) {
                linkFirst = id;
                Vec2 p;
                double a;
                bodyPose(id, p, a);
                linkFirstLocal = Rot(a).applyInv(mw - p);
                if (length(linkFirstLocal) < 0.25 * std::max(0.2, m.bodies[id].shape.boundingRadius())) linkFirstLocal = Vec2();   // near the centre: attach to the centre
            } else if (id == linkFirst) {
                say("Click a different body for the other end.");
            } else if (canEditStructure()) {
                Vec2 pa, pb;
                double aa, ab;
                bodyPose(linkFirst, pa, aa);
                bodyPose(id, pb, ab);
                Vec2 wa = pa + Rot(aa).apply(linkFirstLocal), wb = mw;
                if (length(Rot(ab).applyInv(mw - pb)) < 0.25 * std::max(0.2, m.bodies[id].shape.boundingRadius())) wb = pb;
                LinkKind k = tool == Tool::Spring ? LinkKind::Spring : tool == Tool::Damper ? LinkKind::Damper : tool == Tool::Rod ? LinkKind::Rod : tool == Tool::Weld ? LinkKind::Weld : LinkKind::Pin;
                if (k == LinkKind::Pin || k == LinkKind::Weld) wa = wb;
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
void App::drawCanvas() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImVec2 p0 = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    if (size.x < 50) size.x = 50;
    if (size.y < 50) size.y = 50;
    canvasMin = p0;
    canvasMax = ImVec2(p0.x + size.x, p0.y + size.y);
    ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasMin, canvasMax, true);
    modeClock += ImGui::GetIO().DeltaTime;

    if (needFit && canvasMax.x - canvasMin.x > 100) { fitView(); needFit = false; }
    canvasInteract(ImGui::GetIO().MousePos, hovered);
    Model& m = design();

    // ---- background and grid
    dl->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 26, 255));
    if (showGrid) {
        double step = 0.1;
        const double steps[] = {0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 50};
        for (double s : steps) { step = s; if (s * scale >= 22) break; }
        Vec2 w0 = toWorld(canvasMin), w1 = toWorld(canvasMax);
        for (double x = std::floor(w0.x / step) * step; x <= w1.x; x += step) {
            bool major = std::fabs(std::fmod(std::round(x / step), 5.0)) < 1e-6;
            float sx = toScreen(Vec2(x, 0)).x;
            dl->AddLine(ImVec2(sx, canvasMin.y), ImVec2(sx, canvasMax.y), std::fabs(x) < 1e-9 ? IM_COL32(90, 100, 125, 255) : (major ? IM_COL32(40, 45, 58, 255) : IM_COL32(28, 31, 40, 255)));
            if (major && showLabels) { char b[24]; std::snprintf(b, sizeof b, "%g", x); dl->AddText(ImVec2(sx + 3, canvasMax.y - 16), IM_COL32(100, 110, 130, 255), b); }
        }
        for (double y = std::floor(w0.y / step) * step; y <= w1.y; y += step) {
            bool major = std::fabs(std::fmod(std::round(y / step), 5.0)) < 1e-6;
            float sy = toScreen(Vec2(0, y)).y;
            dl->AddLine(ImVec2(canvasMin.x, sy), ImVec2(canvasMax.x, sy), std::fabs(y) < 1e-9 ? IM_COL32(90, 100, 125, 255) : (major ? IM_COL32(40, 45, 58, 255) : IM_COL32(28, 31, 40, 255)));
            if (major && showLabels) { char b[24]; std::snprintf(b, sizeof b, "%g", y); dl->AddText(ImVec2(canvasMin.x + 4, sy + 2), IM_COL32(100, 110, 130, 255), b); }
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
        ImU32 col = sel ? IM_COL32(90, 205, 255, 255) : IM_COL32(225, 228, 238, 255);
        float th = sel ? 3.0f : 2.0f;
        if (l.kind == LinkKind::Spring) {
            if (ses.hasWorld() && !sel) {                         // colour by load: warm = pulling, cool = pushing
                double f = clampd(ses.world()->link((int)i).force / forceRef, -1, 1);
                col = f >= 0 ? IM_COL32((int)(225 + 30 * f), (int)(228 - 120 * f), (int)(238 - 200 * f), 255) : IM_COL32((int)(225 + 140 * f), (int)(228 + 20 * f), 245, 255);
            }
            drawSpring(dl, sa, sb, col, th);
        } else if (l.kind == LinkKind::Damper) {
            drawDamper(dl, sa, sb, sel ? col : IM_COL32(150, 158, 178, 255), th);
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
        if (ses.hasWorld() && l.kind != LinkKind::Spring && l.kind != LinkKind::Damper && l.kind != LinkKind::Rod) {   // reaction force arrow at a joint
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
            if (b.kind == BodyKind::Anchor) {
                LinkKind sup = LinkKind::Spring;                                       // is this anchor a support symbol?
                for (const Link& l : m.links)
                    if (l.a == (int)i && l.kind != LinkKind::Spring && l.kind != LinkKind::Damper && l.kind != LinkKind::Rod) { sup = l.kind; break; }
                const float ca = (float)std::cos(a), sa_ = (float)std::sin(a);
                auto R = [&](float x, float y) { return ImVec2(c.x + x * ca - y * sa_, c.y + x * sa_ + y * ca); };
                ImU32 ink = sel ? IM_COL32(90, 205, 255, 255) : IM_COL32(200, 205, 215, 255);
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
                ImU32 ink = sel ? IM_COL32(90, 205, 255, 255) : IM_COL32(205, 212, 228, 255);
                float L = std::sqrt((e1.x - e0.x) * (e1.x - e0.x) + (e1.y - e0.y) * (e1.y - e0.y));
                if (L > 1) {
                    ImVec2 u((e1.x - e0.x) / L, (e1.y - e0.y) / L), nn(-u.y, u.x);          // nn: screen-side of local +y (the hatched side)
                    ImVec2 side = ImVec2((float)(rr.apply(Vec2(0, 1)).x), (float)(rr.apply(Vec2(0, 1)).y));
                    if (nn.x * side.x + nn.y * side.y < 0) { nn.x = -nn.x; nn.y = -nn.y; }
                    for (float t = 0; t <= L; t += 11.0f) {
                        ImVec2 q0(e0.x + u.x * t, e0.y + u.y * t);
                        dl->AddLine(q0, ImVec2(q0.x + nn.x * 9.0f - u.x * 7.0f, q0.y + nn.y * 9.0f - u.y * 7.0f), IM_COL32(120, 128, 148, 255), 1.3f);
                    }
                }
                dl->AddLine(e0, e1, ink, sel ? 4.0f : 3.0f);
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
            if (showLabels && b.kind != BodyKind::Anchor) {
                char lab[96];
                if (b.kind == BodyKind::Static) std::snprintf(lab, sizeof lab, "%s  mu %.2f", b.name.c_str(), b.mat.muK);
                else std::snprintf(lab, sizeof lab, "%s  %.3g kg", b.name.c_str(), b.massKg);
                ImVec2 ts = ImGui::CalcTextSize(lab);
                float off = b.shape.isCircle() ? (float)(b.shape.radius * scale) + 4 : (float)(b.shape.boundingRadius() * scale) * 0.55f + 4;
                if (b.kind == BodyKind::Static) {
                    ImVec2 lp = c;                                           // label at the upper-left corner of a floor, away from whatever sits on it
                    Rot rr(a);
                    float best = 1e9f;
                    for (const Vec2& vv : b.shape.verts) { ImVec2 q = toScreen(p + rr.apply(vv)); if (q.x < best) { best = q.x; lp = q; } }
                    dl->AddText(ImVec2(lp.x + 4, lp.y - ts.y - 3), IM_COL32(170, 178, 198, 230), lab);
                } else {
                    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - off - ts.y), IM_COL32(205, 210, 225, 230), lab);
                }
            }
            if (showVectors && ses.hasWorld() && b.isDynamic()) {
                const BodyState& s = ses.world()->state()[i];
                drawArrow(dl, c, toScreen(p + s.vel * 0.25), IM_COL32(80, 220, 120, 255));
            }
        }
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
            int hover = pickBody(toWorld(mp));
            if (hover >= 0) {
                Vec2 p;
                double a;
                bodyPose(hover, p, a);
                dl->AddCircle(toScreen(p), 8, gh, 16, 2.0f);
            }
            if (linkFirst >= 0) {
                Vec2 p;
                double a;
                bodyPose(linkFirst, p, a);
                ImVec2 sa = toScreen(p + Rot(a).apply(linkFirstLocal));
                dl->AddLine(sa, mp, gh, 2.0f);
                dl->AddCircleFilled(sa, 5, gh);
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
    dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 8), IM_COL32(200, 205, 220, 255), hud);
    if (tool == Tool::Select) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 26), IM_COL32(120, 128, 148, 255), state == RunState::Design ? "Click to select, drag to move.  Wheel zooms, right-drag pans." : "Drag a body to push it with the mouse.");
    else if (linkFirst >= 0) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 26), IM_COL32(90, 205, 255, 255), "Now click the second body.");
    else if (tool == Tool::Spring || tool == Tool::Damper || tool == Tool::Rod || tool == Tool::Pin || tool == Tool::Weld) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 26), IM_COL32(120, 128, 148, 255), "Click one body, then another. Esc cancels.");
    else if (tool == Tool::SupportPin || tool == Tool::SupportFixed || tool == Tool::SupportRoller) dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 26), IM_COL32(120, 128, 148, 255), "Click a beam or body: the support snaps to its ends and middle. Q / E turn it. Shift keeps the tool.");
    else dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 26), IM_COL32(120, 128, 148, 255), "Click to place. Q / E (or Ctrl+wheel) rotate it first. Shift keeps placing; Esc returns to Select; Alt ignores the grid.");

    char st[160];
    double tnow = ses.hasWorld() ? ses.simTime() : (haveResults && !ses.telemetry().t.empty() ? ses.telemetry().t.back() : 0.0);
    std::snprintf(st, sizeof st, "%s   t = %.2f s / %.1f s   %s", stateName(), tnow, m.run.duration, m.env.realistic ? "REALISTIC" : "IDEAL");
    ImVec2 ts = ImGui::CalcTextSize(st);
    ImU32 sc = state == RunState::Running ? IM_COL32(110, 230, 150, 255) : (state == RunState::Design ? IM_COL32(255, 214, 120, 255) : IM_COL32(120, 215, 255, 255));
    dl->AddText(ImVec2(canvasMax.x - ts.x - 12, canvasMin.y + 8), sc, st);
    if (recording) {
        dl->AddCircleFilled(ImVec2(canvasMax.x - ts.x - 26, canvasMin.y + 16), 6, IM_COL32(235, 60, 60, 255));
        char rb[64];
        std::snprintf(rb, sizeof rb, "REC %02d:%02d", (recFrames / 60) / 60, (recFrames / 60) % 60);
        dl->AddText(ImVec2(canvasMax.x - ImGui::CalcTextSize(rb).x - 12, canvasMin.y + 26), IM_COL32(255, 130, 120, 255), rb);
        dl->AddRect(canvasMin, canvasMax, IM_COL32(235, 60, 60, 160), 0, 3.0f);
    }
    if (m.bodies.empty())
        dl->AddText(ImVec2((canvasMin.x + canvasMax.x) / 2 - 190, (canvasMin.y + canvasMax.y) / 2 - 10), IM_COL32(150, 158, 178, 255), "Empty canvas: choose a shape in the Library, then click here.");
    // scale bar
    {
        double len = 1.0;
        if (scale < 30) len = 5; else if (scale > 200) len = 0.5;
        float px = (float)(len * scale);
        ImVec2 a(canvasMin.x + 12, canvasMax.y - 28);
        dl->AddLine(a, ImVec2(a.x + px, a.y), IM_COL32(150, 158, 178, 255), 2.0f);
        char sb[32];
        std::snprintf(sb, sizeof sb, "%g m", len);
        dl->AddText(ImVec2(a.x + px + 6, a.y - 8), IM_COL32(150, 158, 178, 255), sb);
    }
    if (modeShown >= 0 && haveModal && state == RunState::Design && modeShown < (int)modal.modes.size()) {
        char mb[120];
        std::snprintf(mb, sizeof mb, "Animating mode %d: %.3f Hz", modeShown + 1, modal.modes[modeShown].freqHz);
        dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 44), IM_COL32(255, 214, 120, 255), mb);
    }
    dl->PopClipRect();
    ImGui::End();
}

}  // namespace vlab
