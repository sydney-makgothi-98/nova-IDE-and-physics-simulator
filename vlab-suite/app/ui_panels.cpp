// ui_panels.cpp -- menu bar, Library, Properties, Environment, Run panel, and the small dialogs.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include "app.hpp"

namespace vlab {

// ====================================================================== small widgets
static bool toolButton(App& a, const char* label, Tool t, const char* tip, float w = 0) {
    bool active = a.tool == t;
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.52f, 1.0f, 1.0f));
    }
    bool clicked = ImGui::Button(label, ImVec2(w, 0));
    if (active) ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered() && tip && *tip) ImGui::SetTooltip("%s", tip);
    if (clicked) { a.tool = t; a.linkFirst = -1; }
    return clicked;
}

static bool numField(const char* label, double* v, double step, const char* fmt, double lo, double hi, bool disabled = false) {
    if (disabled) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(-1);
    double old = *v;
    bool c = ImGui::InputDouble(label, v, step, step * 10, fmt, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    if (!c && ImGui::IsItemDeactivatedAfterEdit()) c = true;
    if (disabled) ImGui::EndDisabled();
    if (c) *v = clampd(*v, lo, hi);
    return c && *v != old;
}

static void labelled(const char* text) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
}

static void helpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static int materialIndexOf(const Material& m) {
    const std::vector<Material>& lib = materialLibrary();
    for (size_t i = 0; i < lib.size(); ++i)
        if (lib[i].name == m.name) return (int)i;
    return -1;
}

static bool materialCombo(const char* id, int* idx, const char* custom = nullptr) {
    const std::vector<Material>& lib = materialLibrary();
    bool changed = false;
    ImGui::SetNextItemWidth(-1);
    const char* cur = (*idx >= 0 && *idx < (int)lib.size()) ? lib[*idx].name.c_str() : (custom ? custom : "Custom");
    if (ImGui::BeginCombo(id, cur)) {
        for (int i = 0; i < (int)lib.size(); ++i) {
            char line[200];
            std::snprintf(line, sizeof line, "%s   (mu %.2f, e %.2f)", lib[i].name.c_str(), lib[i].muK, lib[i].restitution);
            if (ImGui::Selectable(line, *idx == i)) { *idx = i; changed = true; }
        }
        ImGui::EndCombo();
    }
    return changed;
}

// ====================================================================== menu bar
void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New design", "Ctrl+N")) newDesign();
        if (ImGui::MenuItem("Open design...", "Ctrl+O")) loadDesignDialog();
        if (ImGui::MenuItem("Save", "Ctrl+S")) saveDesign(false);
        if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S")) saveDesign(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Export telemetry to Excel...", "Ctrl+E", false, haveResults)) exportExcelDialog();
        if (ImGui::MenuItem("Export telemetry to CSV...", nullptr, false, haveResults)) exportCsvDialog();
        if (ImGui::MenuItem("Open exports folder")) openInShell(exportsDir, false);
        if (ImGui::MenuItem("Open recordings folder")) openInShell(recordingsDir, false);
        if (ImGui::MenuItem("Open designs folder")) openInShell(designsDir, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) platform->requestQuit();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack.empty())) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redoStack.empty())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Delete selected", "Del", false, selBody >= 0 || selLink >= 0)) {
            if (canEditStructure()) {
                pushUndo();
                if (selBody >= 0) design().removeBody(selBody); else design().removeLink(selLink);
                selBody = selLink = -1;
                edited();
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Examples")) {
        const char* cats[3] = {"Basic", "Intermediate", "Complex"};
        for (int cat = 0; cat < 3; ++cat) {
            if (!ImGui::BeginMenu(cats[cat])) continue;
            int n = 0;
            for (const Example& e : exampleGallery()) {
                if (e.category == cat && ImGui::MenuItem(e.title.c_str())) loadExample(n);
                if (e.category == cat && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.summary.c_str());
                ++n;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Run")) {
        if (ImGui::MenuItem("Run / pause", "Space")) { if (state == RunState::Design || state == RunState::Done) { if (state == RunState::Done) stopRun(); startRun(true); } else pause(); }
        if (ImGui::MenuItem("Compute the whole run now")) { if (state == RunState::Done || state == RunState::Paused) stopRun(); startRun(false); }
        if (ImGui::MenuItem("Stop", "Esc", false, state != RunState::Design)) stopRun();
        ImGui::Separator();
        if (ImGui::MenuItem(recording ? "Stop recording video" : "Record video", "F9")) { if (recording) stopRecording(); else startRecording(); }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Grid", nullptr, &showGrid);
        ImGui::MenuItem("Snap to grid", nullptr, &snap);
        ImGui::MenuItem("Contact points", nullptr, &showContacts);
        ImGui::MenuItem("Names and masses", nullptr, &showLabels);
        ImGui::MenuItem("Velocity arrows", nullptr, &showVectors);
        ImGui::Separator();
        if (ImGui::MenuItem("Fit view to the design", "Home")) needFit = true;
        if (ImGui::MenuItem("Reset view")) { viewCenter = Vec2(0, 0); scale = 70; }
        if (ImGui::MenuItem("Reset window layout")) resetLayout = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Quick start")) showWelcome = true;
        if (ImGui::MenuItem("Keyboard shortcuts and tips")) showHelp = true;
        if (ImGui::MenuItem("About")) showAbout = true;
        ImGui::EndMenu();
    }
    // state chip on the right
    char chip[96];
    std::snprintf(chip, sizeof chip, "%s%s", stateName(), recording ? "   REC" : "");
    float w = ImGui::CalcTextSize(chip).x + 24;
    ImGui::SameLine(ImGui::GetWindowWidth() - w);
    ImGui::TextColored(state == RunState::Running ? ImVec4(0.43f, 0.9f, 0.58f, 1) : (state == RunState::Design ? ImVec4(1, 0.84f, 0.47f, 1) : ImVec4(0.47f, 0.84f, 1, 1)), "%s", chip);
    ImGui::EndMainMenuBar();
}

// ====================================================================== Library (a navbar with drop-down menus)
static const char* toolTitle(Tool t) {
    switch (t) {
        case Tool::Select: return "Select / move";
        case Tool::Circle: return "Ball";
        case Tool::Box: return "Block";
        case Tool::Polygon: return "Polygon";
        case Tool::Wedge: return "Wedge";
        case Tool::Floor: return "Floor line";
        case Tool::Anchor: return "Anchor point";
        case Tool::Beam: return "Beam";
        case Tool::Spring: return "Spring";
        case Tool::Damper: return "Damper";
        case Tool::Rod: return "Rigid rod";
        case Tool::Pin: return "Pin joint";
        case Tool::Weld: return "Fixed (weld) joint";
        case Tool::SupportPin: return "Pin support";
        case Tool::SupportFixed: return "Fixed support (cantilever)";
        default: return "Roller support";
    }
}

// one row of a drop-down: a title and a grey one-line description; returns true when picked
static bool menuItem(App& a, Tool t, const char* key, const char* desc) {
    char label[96];
    std::snprintf(label, sizeof label, "%s##%d", toolTitle(t), (int)t);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));
    bool picked = ImGui::Selectable(label, a.tool == t, 0, ImVec2(300, 0));
    if (key && *key) { ImGui::SameLine(250); ImGui::TextDisabled("%s", key); }
    ImGui::TextDisabled("   %s", desc);
    ImGui::Dummy(ImVec2(0, 3));
    ImGui::PopStyleVar();
    if (picked) { a.tool = t; a.linkFirst = -1; ImGui::CloseCurrentPopup(); }
    return picked;
}

void App::drawLibrary() {
    ImGui::Begin("Library");
    const std::vector<Material>& lib = materialLibrary();
    const float full = ImGui::GetContentRegionAvail().x;
    const ImVec2 bsz(full, 34);

    // ---- the navbar: each button opens a drop-down list to its right
    auto navButton = [&](const char* label, const char* popupId, bool active, int index) {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.95f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.52f, 1.0f, 1.0f));
        }
        char text[96];
        std::snprintf(text, sizeof text, "%s   >##%s", label, popupId);
        bool clicked = ImGui::Button(text, bsz);
        if (active) ImGui::PopStyleColor(2);
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        if (clicked || navMenu == index) { ImGui::OpenPopup(popupId); if (navMenu == index) navMenu = -1; }
        ImGui::SetNextWindowPos(ImVec2(mx.x + 6, mn.y), ImGuiCond_Always);
        return std::make_pair(ImGui::BeginPopup(popupId), clicked);
    };

    if (toolButton(*this, "Select / move (V)", Tool::Select, "Click to select, drag to move. While running, drag to push.", full)) {}
    ImGui::Spacing();

    const bool isObject = tool == Tool::Circle || tool == Tool::Box || tool == Tool::Polygon || tool == Tool::Wedge || tool == Tool::Beam;
    if (navButton("Objects", "nav_objects", isObject, 0).first) {
        ImGui::TextUnformatted("OBJECTS  (they move and fall)");
        ImGui::Separator();
        menuItem(*this, Tool::Circle, "C", "A circle with a radius you choose.");
        menuItem(*this, Tool::Box, "B", "A rectangle: width and height you choose.");
        menuItem(*this, Tool::Polygon, "P", "A regular polygon, 3 to 12 sides.");
        menuItem(*this, Tool::Wedge, "W", "A right-angled triangle.");
        menuItem(*this, Tool::Beam, "T", "A rigid beam. It FALLS unless supported or hinged.");
        ImGui::EndPopup();
    }
    const bool isGround = tool == Tool::Floor || tool == Tool::Anchor || tool == Tool::SupportPin || tool == Tool::SupportFixed || tool == Tool::SupportRoller;
    if (navButton("Ground and supports", "nav_ground", isGround, 1).first) {
        ImGui::TextUnformatted("GROUND AND SUPPORTS  (they never move)");
        ImGui::Separator();
        menuItem(*this, Tool::Floor, "F", "A hatched line with zero thickness. It has a material (friction, bounce). Rotate it for ramps and walls.");
        menuItem(*this, Tool::Anchor, "A", "A fixed point to hang springs, dampers and rods from.");
        menuItem(*this, Tool::SupportPin, "1", "Pins a beam to the ground: it can turn, but not move.");
        menuItem(*this, Tool::SupportFixed, "2", "Cantilever: the beam cannot move OR turn. Holds a force and a moment.");
        menuItem(*this, Tool::SupportRoller, "3", "Holds the beam up but lets it slide along the support's axis.");
        ImGui::EndPopup();
    }
    const bool isLink = tool == Tool::Spring || tool == Tool::Damper || tool == Tool::Rod || tool == Tool::Pin || tool == Tool::Weld;
    if (navButton("Connections", "nav_links", isLink, 2).first) {
        ImGui::TextUnformatted("CONNECTIONS  (click one body, then another)");
        ImGui::Separator();
        menuItem(*this, Tool::Spring, "S", "Pushes and pulls with a stiffness you choose.");
        menuItem(*this, Tool::Damper, "D", "A shock absorber: resists speed and makes heat.");
        menuItem(*this, Tool::Rod, "R", "A rigid bar of fixed length.");
        menuItem(*this, Tool::Pin, "J", "A hinge: two bodies share a point and can swivel. Add a motor in Properties.");
        menuItem(*this, Tool::Weld, "", "Glues two bodies together at a point, angle included.");
        ImGui::EndPopup();
    }
    static const char* tabNames[3] = {"Basic", "Intermediate", "Complex"};
    if (navButton("Examples", "nav_examples", false, 3).first) {
        ImGui::TextUnformatted("EXAMPLES");
        ImGui::Separator();
        if (ImGui::BeginTabBar("##exTabs")) {
            for (int tb = 0; tb < 3; ++tb) {
                if (!ImGui::BeginTabItem(tabNames[tb])) continue;
                exampleTab = tb;
                ImGui::BeginChild("##exList", ImVec2(430, 330));
                int n = 0;
                for (const Example& e : exampleGallery()) {
                    if (e.category == tb) {
                        char id[160];
                        std::snprintf(id, sizeof id, "%s##ex%d", e.title.c_str(), n);
                        if (ImGui::Selectable(id)) { loadExample(n); ImGui::CloseCurrentPopup(); }
                        ImGui::PushTextWrapPos(410);
                        ImGui::TextDisabled("%s", e.summary.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::Dummy(ImVec2(0, 4));
                    }
                    ++n;
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    if (tool == Tool::Select) {
        ImGui::TextDisabled("Pick something from the menus above, then click the canvas.");
    } else {
        ImGui::Text("Placing: %s", toolTitle(tool));
        ImGui::TextDisabled("Esc cancels. Shift keeps placing.");
    }

    // ---- settings for whatever is about to be placed
    ImGui::Spacing();
    ImGui::TextUnformatted("Settings for the next one");
    switch (tool) {
        case Tool::Circle:
        case Tool::Polygon:
            labelled("Radius (m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##r", &nextRadius, 0.01f, 0.02f, 5.0f, "%.3f");
            if (tool == Tool::Polygon) { labelled("Sides"); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##sides", &nextSides, 3, 12); }
            break;
        case Tool::Box:
        case Tool::Wedge:
            labelled("Width (m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##w", &nextW, 0.01f, 0.02f, 10.0f, "%.3f");
            labelled("Height (m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##h", &nextH, 0.01f, 0.02f, 10.0f, "%.3f");
            break;
        case Tool::Beam:
            labelled("Length (m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##bl", &beamLength, 0.05f, 0.2f, 30.0f, "%.2f");
            labelled("Thickness (m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##bt", &beamThick, 0.01f, 0.02f, 2.0f, "%.3f");
            break;
        case Tool::Floor:
            labelled("Length (m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##fl", &floorLength, 0.05f, 0.2f, 60.0f, "%.2f");
            labelled("Surface material");
            materialCombo("##fm", &floorMaterial);
            ImGui::TextDisabled("friction mu_s %.2f, mu_k %.2f, bounce e %.2f", lib[floorMaterial].muS, lib[floorMaterial].muK, lib[floorMaterial].restitution);
            break;
        case Tool::Spring:
            labelled("Stiffness k (N/m)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##k", &nextK, 1.0f, 0.1f, 5000.0f, "%.1f");
            break;
        default: break;
    }
    if (tool == Tool::Circle || tool == Tool::Box || tool == Tool::Polygon || tool == Tool::Wedge) {
        labelled("Mass (kg)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##m", &nextMass, 0.01f, 0.01f, 1000.0f, "%.3f");
    }
    if (tool == Tool::Circle || tool == Tool::Box || tool == Tool::Polygon || tool == Tool::Wedge || tool == Tool::Beam) {
        labelled("Material");
        materialCombo("##nm", &nextMaterial);
        ImGui::TextDisabled("friction mu_s %.2f, mu_k %.2f, bounce e %.2f", lib[nextMaterial].muS, lib[nextMaterial].muK, lib[nextMaterial].restitution);
        if (tool == Tool::Beam) ImGui::TextDisabled("A beam's mass comes from its material density, size and a 1 m depth. Change it in Properties.");
    }
    if (tool != Tool::Select && tool != Tool::Spring && tool != Tool::Damper && tool != Tool::Rod && tool != Tool::Pin && tool != Tool::Weld && tool != Tool::Anchor) {
        labelled("Rotation (deg) - or Q / E, Ctrl+wheel");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##rot", &nextAngle, 0.5f, -360.0f, 360.0f, "%.1f");
        if (ImGui::Button("-15", ImVec2(60, 0))) nextAngle = std::fmod(nextAngle - 15.0f + 360.0f, 360.0f);
        ImGui::SameLine();
        if (ImGui::Button("+15", ImVec2(60, 0))) nextAngle = std::fmod(nextAngle + 15.0f, 360.0f);
        ImGui::SameLine();
        if (ImGui::Button("0", ImVec2(40, 0))) nextAngle = 0;
        if (tool == Tool::SupportRoller) ImGui::TextDisabled("0 deg = the beam may slide sideways (left-right).");
    }
    ImGui::Separator();
    ImGui::TextDisabled("Grid snapping");
    ImGui::Checkbox("Snap to grid", &snap);
    if (snap) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        float s = (float)snapStep;
        if (ImGui::DragFloat("##snap", &s, 0.01f, 0.01f, 1.0f, "%.2f m")) snapStep = s;
    }
    ImGui::End();
}

// ====================================================================== Properties
void App::drawProperties() {
    ImGui::Begin("Properties");
    Model& m = design();
    auto commit = [&]() { pushUndo(); m.updateMass(); edited(); };

    if (selBody >= 0 && selBody < (int)m.bodies.size()) {
        Body& b = m.bodies[selBody];
        const bool design = state == RunState::Design;
        // name
        char nm[64];
        std::snprintf(nm, sizeof nm, "%s", b.name.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##name", nm, sizeof nm)) { pushUndo(); b.name = nm; edited(); }
        ImGui::TextDisabled(b.kind == BodyKind::Dynamic ? "Moving body" : (b.kind == BodyKind::Static ? (b.shape.preset == ShapePreset::Line ? "Floor line (zero thickness)" : "Fixed slab") : "Fixed anchor point"));
        ImGui::Separator();
        if (b.kind != BodyKind::Anchor) {
            ImGui::TextUnformatted(b.shape.preset == ShapePreset::Line ? "Size" : "Shape and size");
            bool c = false;
            switch (b.shape.preset) {
                case ShapePreset::Circle: labelled("Radius (m)"); c |= numField("##rad", &b.shape.radius, 0.05, "%.3f", 0.02, 10); break;
                case ShapePreset::Box: labelled("Width (m)"); c |= numField("##bw", &b.shape.w, 0.1, "%.3f", 0.02, 60); labelled("Height (m)"); c |= numField("##bh", &b.shape.h, 0.1, "%.3f", 0.02, 60); break;
                case ShapePreset::Regular: {
                    labelled("Radius (m)"); c |= numField("##prad", &b.shape.radius, 0.05, "%.3f", 0.02, 10);
                    labelled("Sides"); ImGui::SetNextItemWidth(-1);
                    int sd = b.shape.sides;
                    if (ImGui::SliderInt("##sides", &sd, 3, 12)) { b.shape.sides = sd; c = true; }
                    break;
                }
                case ShapePreset::Line: labelled("Length (m)"); c |= numField("##ll", &b.shape.w, 0.1, "%.3f", 0.05, 200); break;
                default: labelled("Leg along x (m)"); c |= numField("##ww", &b.shape.w, 0.1, "%.3f", 0.02, 20); labelled("Leg along y (m)"); c |= numField("##wh", &b.shape.h, 0.1, "%.3f", 0.02, 20); break;
            }
            if (c) commit();
        }
        if (b.isDynamic()) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Mass");
            bool c = false;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Checkbox("Mass from density x area", &b.massFromDensity)) c = true;
            if (b.massFromDensity) { labelled("Density (kg/m^2 of cross-section)"); c |= numField("##dens", &b.mat.density, 10, "%.1f", 1, 30000); }
            else { labelled("Mass (kg)"); c |= numField("##mass", &b.mass, 0.1, "%.4g", 0.001, 100000); }
            if (c) commit();
            ImGui::TextDisabled("computed: %.4g kg, moment of inertia %.4g kg m^2", b.massKg, b.inertia);
        }
        ImGui::Spacing();
        ImGui::TextUnformatted("Position");
        {
            bool c = false;
            double deg = b.angle * 180.0 / kPi;
            ImGui::BeginDisabled(!design);
            labelled("x (m)"); c |= numField("##px", &b.pos.x, 0.1, "%.3f", -1000, 1000);
            labelled("y (m, down is positive)"); c |= numField("##py", &b.pos.y, 0.1, "%.3f", -1000, 1000);
            if (b.kind != BodyKind::Anchor) { labelled("Angle (deg)"); if (numField("##ang", &deg, 1.0, "%.2f", -360, 360)) { b.angle = deg * kPi / 180.0; c = true; } }
            ImGui::EndDisabled();
            if (c) { commit(); m.refreshAutoRest(); }
            if (!design) ImGui::TextDisabled("(position is set in design mode)");
        }
        if (b.kind != BodyKind::Anchor) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Surface material");
            int idx = materialIndexOf(b.mat);
            if (materialCombo("##mat", &idx, "Custom")) { double dens = b.mat.density; b.mat = materialLibrary()[idx]; if (!b.massFromDensity) b.mat.density = dens; commit(); }
            bool c = false;
            labelled("Static friction (starts sliding)"); c |= numField("##ms", &b.mat.muS, 0.05, "%.3f", 0, 5);
            labelled("Kinetic friction (while sliding)"); c |= numField("##mk", &b.mat.muK, 0.05, "%.3f", 0, 5);
            labelled("Bounciness (0 dead .. 1 elastic)"); c |= numField("##e", &b.mat.restitution, 0.05, "%.3f", 0, 1);
            labelled("Rolling resistance"); c |= numField("##rr", &b.mat.rollRes, 0.001, "%.4f", 0, 0.5);
            if (c) { b.mat.name = "Custom"; commit(); }
            helpMarker("Friction and bounce between two bodies are combined from both surfaces (see Environment). Frictionless floor + frictionless ball = no friction at all.");
        }
        if (b.isDynamic()) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Starting motion (applied when the run starts)");
            bool c = false;
            ImGui::BeginDisabled(!design);
            labelled("Velocity x (m/s)"); c |= numField("##vx", &b.vel.x, 0.1, "%.3f", -500, 500);
            labelled("Velocity y (m/s)"); c |= numField("##vy", &b.vel.y, 0.1, "%.3f", -500, 500);
            labelled("Spin (rad/s)"); c |= numField("##om", &b.omega, 0.1, "%.3f", -500, 500);
            if (ImGui::Checkbox("Lock x (slides up/down only)", &b.lockX)) c = true;
            if (ImGui::Checkbox("Lock y (slides left/right only)", &b.lockY)) c = true;
            if (ImGui::Checkbox("Lock rotation", &b.lockRot)) c = true;
            if (ImGui::Checkbox("Collides with other bodies", &b.collide)) c = true;
            ImGui::EndDisabled();
            if (c) commit();
            ImGui::Spacing();
            ImGui::TextUnformatted("Harmonic push  F cos(w t)");
            c = false;
            labelled("Force amplitude (N)"); c |= numField("##df", &b.driveF, 0.5, "%.3f", 0, 100000);
            labelled("Direction (deg: 0 = right, 90 = down)"); c |= numField("##da", &b.driveAngleDeg, 5, "%.1f", -360, 360);
            labelled("Frequency (rad/s)"); c |= numField("##dw", &b.driveW, 0.1, "%.3f", 0, 5000);
            if (b.driveW > 0) ImGui::TextDisabled("= %.3f Hz", b.driveW / (2 * kPi));
            if (c) commit();
        }
        ImGui::Spacing();
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(argbToImU32(b.color));
        if (ImGui::ColorEdit3("Colour", &col.x, ImGuiColorEditFlags_NoInputs)) { pushUndo(); b.color = imU32ToArgb(ImGui::ColorConvertFloat4ToU32(col)); edited(); }
        ImGui::Separator();
        if (ImGui::Button("Delete this body", ImVec2(-1, 0))) {
            if (canEditStructure()) { pushUndo(); m.removeBody(selBody); selBody = -1; edited(); }
        }
    } else if (selLink >= 0 && selLink < (int)m.links.size()) {
        Link& l = m.links[selLink];
        const bool support = !m.bodies[l.a].isDynamic() && (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld || l.kind == LinkKind::Roller);
        const char* kn = l.kind == LinkKind::Spring ? "Spring" : (l.kind == LinkKind::Damper ? "Damper" : (l.kind == LinkKind::Rod ? "Rigid rod" :
                         (l.kind == LinkKind::Weld ? (support ? "Fixed support (cantilever)" : "Fixed (weld) joint") :
                          (l.kind == LinkKind::Roller ? (support ? "Roller support" : "Roller joint") : (support ? "Pin support" : "Pin joint")))));
        ImGui::Text("%s", kn);
        ImGui::TextDisabled("%s  -  %s", m.bodies[l.a].name.c_str(), m.bodies[l.b].name.c_str());
        ImGui::Separator();
        bool c = false;
        if (l.kind == LinkKind::Spring) {
            labelled("Stiffness k (N/m)"); c |= numField("##k", &l.k, 1, "%.3f", 0, 1e6);
            labelled("Rest length (m)");
            if (numField("##L0", &l.L0, 0.05, "%.4f", 0.05, 200)) { l.autoRest = false; c = true; }
            if (ImGui::Button("Set rest length to the drawn length", ImVec2(-1, 0))) { l.L0 = std::max(0.05, length(m.worldPoint(l.b, l.lb) - m.worldPoint(l.a, l.la))); l.autoRest = false; c = true; }
            labelled("Cubic stiffness k3 (N/m^3)"); c |= numField("##k3", &l.k3, 1, "%.3f", 0, 1e7);
            helpMarker("force = k s + k3 s^3, where s = length - rest length. A positive k3 makes the spring harder the more it stretches.");
            if (l.autoRest) ImGui::TextDisabled("Rest length follows the drawing (change it to preload the spring).");
            else ImGui::TextDisabled("Drawn length now: %.3f m", length(m.worldPoint(l.b, l.lb) - m.worldPoint(l.a, l.la)));
        } else if (l.kind == LinkKind::Damper) {
            labelled("Damping c (N s/m)"); c |= numField("##c", &l.c, 0.1, "%.3f", 0, 1e6);
            if (!m.env.damping) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Dampers are switched OFF in the Environment (ideal mode).");
        } else if (l.kind == LinkKind::Rod) {
            labelled("Rod length (m)"); c |= numField("##rl", &l.L0, 0.05, "%.4f", 0.05, 200);
        }
        if (l.kind == LinkKind::Pin) {
            ImGui::TextUnformatted("Hinge motor and friction");
            labelled("Rotary damping (N m s/rad)"); c |= numField("##jc", &l.jointC, 0.1, "%.3f", 0, 1e6);
            labelled("Constant motor torque (N m)"); c |= numField("##mt", &l.motorTorque, 1, "%.3f", -1e6, 1e6);
            labelled("Sinusoidal torque amplitude (N m)"); c |= numField("##ma", &l.motorAmp, 1, "%.3f", 0, 1e6);
            labelled("Sinusoid frequency (rad/s)"); c |= numField("##mw", &l.motorW, 0.1, "%.3f", 0, 5000);
            helpMarker("Positive torque turns the second body clockwise on the screen, and the first body the other way. A motor on a support turns the beam against the ground.");
        }
        if (l.kind == LinkKind::Weld) ImGui::TextDisabled("Holds the point AND the relative angle (%.1f deg).", l.refAngle * 180.0 / kPi);
        if (l.kind == LinkKind::Roller) ImGui::TextDisabled("The beam's point is held on a line: it can slide along the support's axis and rotate.");
        if (l.kind != LinkKind::Spring && l.kind != LinkKind::Damper) {
            if (ImGui::Checkbox("The joined bodies may collide with each other", &l.collideConnected)) c = true;
        }
        if (c) commit();
        ImGui::Separator();
        if (ses.hasWorld() && selLink < (int)m.links.size()) {
            LinkReading r = ses.world()->link(selLink);
            ImGui::TextUnformatted("Live readings");
            ImGui::Text("length %.4f m", r.length);
            if (l.kind == LinkKind::Spring) ImGui::Text("stretch %+.4f m   force %+.3f N", r.stretch, r.force);
            if (l.kind == LinkKind::Damper) ImGui::Text("rate %+.4f m/s   force %+.3f N   heat %.3f W", r.rate, r.force, r.power);
            if (l.kind == LinkKind::Pin || l.kind == LinkKind::Weld || l.kind == LinkKind::Roller) {
                ImGui::Text("reaction force  x %+.2f N   y %+.2f N", r.rx, r.ry);
                ImGui::Text("resultant %.2f N", r.force);
                if (l.kind == LinkKind::Weld) ImGui::Text("reaction moment %+.2f N m", r.moment);
            }
        }
        if (ImGui::Button("Delete this connection", ImVec2(-1, 0))) {
            if (canEditStructure()) { pushUndo(); m.removeLink(selLink); selLink = -1; edited(); }
        }
    } else {
        ImGui::TextWrapped("Nothing selected.");
        ImGui::TextDisabled("Choose the Select tool and click a body or a spring to edit it here.");
        ImGui::Spacing();
        ImGui::TextUnformatted("This design");
        char nm[96];
        std::snprintf(nm, sizeof nm, "%s", m.name.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##dname", nm, sizeof nm)) { pushUndo(); m.name = nm; dirty = true; }
        char notes[512];
        std::snprintf(notes, sizeof notes, "%s", m.notes.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextMultiline("##notes", notes, sizeof notes, ImVec2(-1, 70))) { m.notes = notes; dirty = true; }
        ImGui::TextDisabled("Notes are saved in the file and in the Excel report.");
        ImGui::Spacing();
        ImGui::Text("%d moving bodies, %d fixed, %d connections", m.dynamicCount(), (int)m.bodies.size() - m.dynamicCount(), (int)m.links.size());
    }
    ImGui::End();
}

// ====================================================================== Environment
void App::drawEnvironment() {
    ImGui::Begin("Environment");
    Environment& e = design().env;
    auto commit = [&]() { pushUndo(); edited(); };

    ImGui::TextUnformatted("Physics mode");
    float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    bool ideal = !e.friction && !e.inelastic && !e.damping && !e.airDrag && !e.rolling;
    bool real = e.friction && e.inelastic && e.damping;
    if (ideal) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.95f, 1));
    if (ImGui::Button("Ideal", ImVec2(w, 0))) { commit(); e.setIdeal(); edited(); }
    if (ideal) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frictionless, perfectly elastic, no damping, no drag.\nEnergy is conserved: ideal for checking theory.");
    ImGui::SameLine();
    if (real && !ideal) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.95f, 1));
    if (ImGui::Button("Realistic", ImVec2(w, 0))) { commit(); e.setRealistic(); edited(); }
    if (real && !ideal) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Friction from the materials, energy lost in impacts,\nactive dampers, rolling resistance.");
    ImGui::TextDisabled("%s", ideal ? "Ideal: energy is conserved" : (real ? "Realistic: energy is lost to heat" : "Custom mix"));
    ImGui::Spacing();

    ImGui::TextUnformatted("Individual effects");
    bool c = false;
    c |= ImGui::Checkbox("Collisions between bodies", &e.collisions);
    c |= ImGui::Checkbox("Friction (sliding and sticking)", &e.friction);
    c |= ImGui::Checkbox("Inelastic impacts (bounciness < 1)", &e.inelastic);
    c |= ImGui::Checkbox("Rolling resistance", &e.rolling);
    c |= ImGui::Checkbox("Dampers active", &e.damping);
    c |= ImGui::Checkbox("Air drag", &e.airDrag);
    if (c) { e.realistic = e.friction && e.inelastic && e.damping; commit(); }
    if (e.airDrag) {
        labelled("Linear drag c1 (N s/m)"); if (numField("##d1", &e.drag1, 0.01, "%.4f", 0, 1000)) commit();
        labelled("Quadratic drag c2 (N s^2/m^2)"); if (numField("##d2", &e.drag2, 0.001, "%.4f", 0, 1000)) commit();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("World");
    labelled("Gravity (m/s^2)");
    if (numField("##g", &e.gravity, 0.1, "%.3f", 0, 100)) commit();
    ImGui::SameLine(0, 0);
    ImGui::Spacing();
    if (ImGui::SmallButton("Earth 9.81")) { commit(); e.gravity = 9.81; edited(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Moon 1.62")) { commit(); e.gravity = 1.62; edited(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Zero-g")) { commit(); e.gravity = 0; edited(); }

    if (ImGui::TreeNode("Advanced")) {
        const char* cmb[] = {"Geometric mean", "Average", "Minimum", "Maximum"};
        int mu = (int)e.muCombine, ec = (int)e.eCombine;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##mucomb", &mu, cmb, 4)) { e.muCombine = (Combine)mu; commit(); }
        ImGui::TextDisabled("how two surfaces' friction combine");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##ecomb", &ec, cmb, 4)) { e.eCombine = (Combine)ec; commit(); }
        ImGui::TextDisabled("how two surfaces' bounciness combine");
        labelled("Bounce threshold (m/s)"); if (numField("##rt", &e.restThreshold, 0.05, "%.3f", 0, 5)) commit();
        helpMarker("Impacts slower than this do not bounce, so resting bodies settle instead of jittering.");
        double dtms = e.dt * 1000.0;
        labelled("Physics step (ms)"); if (numField("##dt", &dtms, 0.05, "%.3f", 0.02, 2.0)) { e.dt = dtms / 1000.0; commit(); }
        helpMarker("Smaller is more accurate and slower. 0.25 ms is a good default.");
        int it = e.solverIterations;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##it", &it, 4, 40, "contact iterations %d")) { e.solverIterations = it; commit(); }
        ImGui::TreePop();
    }
    ImGui::End();
}

// ====================================================================== Run panel
void App::drawRunPanel() {
    ImGui::Begin("Run");
    Model& m = design();
    ImGui::TextUnformatted("How long should the model run?");
    ImGui::SetNextItemWidth(-1);
    double dur = m.run.duration;
    if (numField("##dur", &dur, 1.0, "%.2f seconds", 0.05, 3600)) { pushUndo(); m.run.duration = dur; edited(); }
    float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4) / 5;
    const double presets[5] = {5, 10, 30, 60, 300};
    const char* names[5] = {"5 s", "10 s", "30 s", "1 min", "5 min"};
    for (int i = 0; i < 5; ++i) {
        if (i) ImGui::SameLine();
        bool cur = std::fabs(m.run.duration - presets[i]) < 1e-9;
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.95f, 1));
        if (ImGui::Button(names[i], ImVec2(bw, 0))) { pushUndo(); m.run.duration = presets[i]; edited(); }
        if (cur) ImGui::PopStyleColor();
    }
    labelled("Data rows per second");
    double hz = m.run.sampleHz;
    if (numField("##hz", &hz, 10, "%.0f rows/s", 1, 5000)) { pushUndo(); m.run.sampleHz = hz; edited(); }
    double rows = m.run.duration * std::min(m.run.sampleHz, 1e6 / std::max(1.0, m.run.duration));
    ImGui::TextDisabled("about %.0f rows in the Excel sheet", rows);
    ImGui::Separator();

    // ---- controls
    bool hasDyn = m.dynamicCount() > 0;
    float cw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
    const char* runLabel = state == RunState::Running ? "Pause" : (state == RunState::Paused ? "Resume" : (state == RunState::Done ? "Run again" : "Run"));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.55f, 0.30f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.65f, 0.36f, 1));
    ImGui::BeginDisabled(!hasDyn || state == RunState::Computing);
    if (ImGui::Button(runLabel, ImVec2(cw, 32))) {
        if (state == RunState::Design) startRun(true);
        else if (state == RunState::Done) { stopRun(); startRun(true); }
        else pause();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run in real time (Space). You can drag bodies while it runs.");
    ImGui::SameLine();
    ImGui::BeginDisabled(state == RunState::Design);
    if (ImGui::Button("Stop", ImVec2(cw, 32))) stopRun();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasDyn || state == RunState::Running || state == RunState::Computing);
    if (ImGui::Button("Step 1/60 s", ImVec2(cw, 32))) stepOnce();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!hasDyn || state == RunState::Running || state == RunState::Computing);
    if (ImGui::Button("Compute the whole run now (fast)", ImVec2(-1, 0))) { if (state == RunState::Done || state == RunState::Paused) stopRun(); startRun(false); }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Runs the entire duration as fast as the computer can, then shows all the data.");
    float ts = (float)timeScale;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##ts", &ts, 0.1f, 4.0f, "real-time speed %.2fx")) timeScale = ts;

    // ---- progress
    double tnow = ses.hasWorld() ? ses.simTime() : (haveResults && !ses.telemetry().t.empty() ? ses.telemetry().t.back() : 0.0);
    float frac = (float)clampd(m.run.duration > 0 ? tnow / m.run.duration : 0, 0, 1);
    char pb[96];
    std::snprintf(pb, sizeof pb, "%.2f / %.2f s   (%d%%)", tnow, m.run.duration, (int)(frac * 100));
    ImGui::ProgressBar(frac, ImVec2(-1, 0), pb);
    if (state == RunState::Computing) { if (ImGui::Button("Cancel", ImVec2(-1, 0))) stopRun(); }
    ImGui::TextWrapped("%s", statusLine.c_str());
    if (resultsStale && haveResults) ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "The design changed after this run: run again to update the data.");

    // ---- export
    ImGui::Separator();
    ImGui::TextUnformatted("Take the data with you");
    ImGui::BeginDisabled(!haveResults || ses.telemetry().rows() == 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.25f, 1));
    if (ImGui::Button("Export to Excel (.xlsx) with charts", ImVec2(-1, 34))) exportExcelDialog();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Creates a workbook: Summary, Telemetry (every sample), Charts, plus Spectrum / Modal / Frequency response if you computed them.");
    float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    if (ImGui::Button("Export CSV", ImVec2(hw, 0))) exportCsvDialog();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (recording) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1));
    if (ImGui::Button(recording ? "Stop recording" : "Record video (F9)", ImVec2(hw, 0))) { if (recording) stopRecording(); else startRecording(); }
    if (recording) ImGui::PopStyleColor();
    ImGui::Checkbox("Record the whole window", &recWholeWindow);
    if (!lastExportPath.empty()) {
        ImGui::TextDisabled("Last saved:");
        ImGui::TextWrapped("%s", lastExportPath.c_str());
        if (ImGui::SmallButton("Open")) openInShell(lastExportPath, false);
        ImGui::SameLine();
        if (ImGui::SmallButton("Show in folder")) openInShell(lastExportPath, true);
    }
    // ---- live energy ledger
    ImGui::Separator();
    if (ses.hasWorld()) {
        const Ledger& L = ses.world()->ledger();
        ImGui::TextUnformatted("Energy now (joules)");
        ImGui::Text("kinetic %.3f   potential %.3f", L.T, L.V);
        ImGui::Text("total   %.3f", L.E);
        ImGui::TextDisabled("lost so far: dampers %.3f, drag %.3f, friction %.3f, impacts %.3f", L.dViscous, L.dDrag, L.dFriction, L.dImpact);
        ImGui::TextDisabled("work put in %.3f   numerical error %.1e", L.workIn, L.numerical());
        if (!ses.world()->healthy()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Numbers blew up. Press Stop and soften the springs.");
    } else if (haveResults && ses.telemetry().rows() > 0) {
        const Telemetry& t = ses.telemetry();
        auto last = [&](const char* n) { int c = t.find(std::string("Energy ") + n); return c >= 0 ? t.col[c].back() : 0.0; };
        ImGui::TextUnformatted("Energy at the end of the last run (joules)");
        ImGui::Text("kinetic %.3f   potential %.3f   total %.3f", last("T kinetic"), last("V potential"), last("E total"));
        ImGui::TextDisabled("lost: dampers %.3f, drag %.3f, friction %.3f, impacts %.3f", last("lost viscous"), last("lost drag"), last("lost friction"), last("lost impact"));
    } else {
        ImGui::TextDisabled("Energy numbers appear here once it runs.");
    }

    ImGui::End();
}

// ====================================================================== dialogs
void App::drawWelcome() {
    ImGui::OpenPopup("Welcome to Vibration Lab Suite");
    ImVec2 c = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Welcome to Vibration Lab Suite", &showWelcome, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Build a mechanical system, run it for as long as you like, then take every number to Excel with charts already made.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("1.  Design");
        ImGui::TextWrapped("Pick a shape in the Library (left), click the canvas to place it. Set sizes, radii and materials in Properties. Add floors with their own friction. Join things with springs, dampers, rods and pins.");
        ImGui::Spacing();
        ImGui::TextUnformatted("2.  Choose the physics and the run time");
        ImGui::TextWrapped("Environment: Ideal (frictionless, elastic) or Realistic (friction, energy loss). Run panel: type how many seconds, then press Run, or compute the whole run instantly.");
        ImGui::Spacing();
        ImGui::TextUnformatted("3.  Read the telemetry and export");
        ImGui::TextWrapped("The Telemetry panel plots position, velocity, forces and where the energy went. Press 'Export to Excel' for a workbook with all the data and ready-made charts, or Record to save a video.");
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Start with a worked example", ImVec2(190, 32))) { loadExample(0); showWelcome = false; }
        ImGui::SameLine();
        if (ImGui::Button("Bouncing ball", ImVec2(130, 32))) { loadExample(6); showWelcome = false; }
        ImGui::SameLine();
        if (ImGui::Button("Empty canvas", ImVec2(130, 32))) { newDesign(); showWelcome = false; }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 32))) showWelcome = false;
        ImGui::TextDisabled("Reopen this any time from Help > Quick start.");
        ImGui::EndPopup();
    }
}

void App::drawAbout() {
    ImGui::OpenPopup("About Vibration Lab Suite");
    ImVec2 c = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About Vibration Lab Suite", &showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Vibration Lab Suite  1.0");
        ImGui::TextDisabled("A dynamics and vibrations sandbox by Team NOVA");
        ImGui::Separator();
        ImGui::PushTextWrapPos(520);
        ImGui::TextWrapped("Smooth forces (springs, dampers, gravity, drag, pushes) follow the Lagrangian equations of motion and are integrated with RK4. "
                           "Collisions use a sequential-impulse solver with Newton restitution and Coulomb friction. An energy ledger accounts for every joule. "
                           "The physics is checked against closed-form solutions by the built-in validation suite (vlab_tests.exe).");
        ImGui::Spacing();
        ImGui::TextWrapped("Real-world materials (friction, bounce) are generic textbook values: use them for teaching and exploration, and measure your own surfaces for engineering decisions. "
                           "This is a 2-D simulator: bodies have a depth of 1 m.");
        ImGui::Spacing();
        ImGui::TextDisabled("Built with Dear ImGui (MIT), ImPlot (MIT), zlib, Windows Media Foundation.");
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Close", ImVec2(100, 0))) showAbout = false;
        ImGui::EndPopup();
    }
}

void App::drawHelp() {
    ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_Appearing);
    if (ImGui::Begin("Keyboard shortcuts and tips", &showHelp)) {
        ImGui::TextUnformatted("Tools");
        ImGui::BulletText("V select   C ball   B block   P polygon   W wedge   F floor   A anchor");
        ImGui::BulletText("S spring   D damper   R rod   J pin   Esc back to Select");
        ImGui::Spacing();
        ImGui::TextUnformatted("Running");
        ImGui::BulletText("Space run / pause    Esc stop    F9 record video");
        ImGui::BulletText("While running, drag any moving body with the mouse to push it.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Files");
        ImGui::BulletText("Ctrl+N new   Ctrl+O open   Ctrl+S save   Ctrl+E export to Excel");
        ImGui::BulletText("Ctrl+Z undo   Ctrl+Y redo   Del delete the selection");
        ImGui::Spacing();
        ImGui::TextUnformatted("Canvas");
        ImGui::BulletText("Mouse wheel zoom, right or middle button drag to pan, hold Alt to ignore the grid.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Tips");
        ImGui::BulletText("Springs are born relaxed at the length you draw them. Change the rest length to preload a spring.");
        ImGui::BulletText("Y points down (like the screen): gravity pulls toward +y.");
        ImGui::BulletText("A floor tilted by an angle is a ramp. Give each floor its own material to compare friction.");
        ImGui::BulletText("Ideal mode conserves energy: the energy-law error at the end should be near zero.");
        ImGui::BulletText("The Excel Summary sheet lists your whole setup, so a report is self-explaining.");
        ImGui::BulletText("The command-line runner (vlab_cli.exe) can run saved designs without opening a window.");
    }
    ImGui::End();
}

void App::drawToast() {
    if (now > toast.until || toast.text.empty()) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y - 36), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, toast.error ? ImVec4(0.45f, 0.10f, 0.10f, 0.95f) : ImVec4(0.10f, 0.30f, 0.18f, 0.95f));
    ImGui::Begin("##toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushTextWrapPos(640);
    ImGui::TextUnformatted(toast.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::End();
    ImGui::PopStyleColor();
}

}  // namespace vlab
