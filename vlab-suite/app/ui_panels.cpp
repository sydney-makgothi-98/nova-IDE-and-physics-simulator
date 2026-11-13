// ui_panels.cpp -- menu bar, Library, Properties, Environment, Run panel, and the small dialogs.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>
#include "app.hpp"

namespace vlab {

// ====================================================================== small widgets
// every navbar button is the same blue; the one that is active gets a white outline
static void pushNavStyle(bool active) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.40f, 0.88f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.50f, 0.98f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.32f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, active ? ImVec4(1, 1, 1, 0.95f) : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 2.0f : 0.0f);
}
static void popNavStyle() { ImGui::PopStyleVar(); ImGui::PopStyleColor(5); }

static bool toolButton(App& a, const char* label, Tool t, const char* tip, float w = 0) {
    bool active = a.tool == t;
    pushNavStyle(active);
    bool clicked = ImGui::Button(label, ImVec2(w, 0));
    popNavStyle();
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
        if (ImGui::MenuItem("Dashboard")) focusDashboard = true;
        ImGui::Separator();
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
        if (ImGui::MenuItem("Light theme", nullptr, !darkTheme)) setTheme(false);
        if (ImGui::MenuItem("Dark theme", nullptr, darkTheme)) setTheme(true);
        ImGui::Separator();
        ImGui::MenuItem("Grid", nullptr, &showGrid);
        ImGui::MenuItem("Snap to grid", nullptr, &snap);
        ImGui::MenuItem("Contact points", nullptr, &showContacts);
        ImGui::MenuItem("Legend", nullptr, &showLabels);
        ImGui::MenuItem("Velocity arrows", nullptr, &showVectors);
        ImGui::MenuItem("Reaction force arrows", nullptr, &showReactions);
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

// ---- the sidebar: a brand block, then groups, each with a heading and a line saying what it is for; the SETTINGS items open their panel in place
static ImVec4 gCardBg;
static ImVec2 gCardStart;
static float gCardW = 0;
// a card: a rounded panel behind a group of widgets. The background is added once the group's height is known (no child window needed).
static void beginCard(const char*) {
    gCardStart = ImGui::GetCursorScreenPos();
    gCardW = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0, 3));
    ImGui::Indent(9);
}
static void endCard() {
    ImGui::Unindent(9);
    ImGui::Dummy(ImVec2(0, 5));
    ImGui::EndGroup();
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 p1(gCardStart.x + gCardW + 9, ImGui::GetItemRectMax().y);   // the widgets end at the window content edge: the card reaches 9 px past it, so the padding is even
    d->ChannelsSetCurrent(0);
    d->AddRectFilled(gCardStart, p1, ImGui::ColorConvertFloat4ToU32(gCardBg), 8.0f);
    d->AddRect(gCardStart, p1, ImGui::GetColorU32(ImGuiCol_Border), 8.0f);
    d->ChannelsMerge();
}
static void dimWrapped(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
// a group heading (small capitals) with its description under it
static void sideHeading(const char* title, const char* desc) {
    ImGui::Dummy(ImVec2(0, 6));
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    d->AddText(ImGui::GetFont(), 14.5f, p, ImGui::GetColorU32(ImGuiCol_TextDisabled), title);
    ImGui::Dummy(ImVec2(0, 20));
    if (desc && *desc) dimWrapped(desc);
    ImGui::Dummy(ImVec2(0, 2));
}
// a collapsible group header: heading text with a chevron, click to fold or unfold
static bool sideHeader(const char* id, const char* title, bool open) {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(0, 6));
    p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##hdr", ImVec2(w, 24));
    ImGui::PopID();
    const ImU32 col = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    d->AddText(ImGui::GetFont(), 14.5f, ImVec2(p.x, p.y + 3), col, title);
    const float cx = p.x + w - 12, cy = p.y + 12;
    if (open) { d->AddLine(ImVec2(cx - 5, cy - 2), ImVec2(cx, cy + 3), col, 2.0f); d->AddLine(ImVec2(cx, cy + 3), ImVec2(cx + 5, cy - 2), col, 2.0f); }
    else { d->AddLine(ImVec2(cx - 2, cy - 5), ImVec2(cx + 3, cy), col, 2.0f); d->AddLine(ImVec2(cx + 3, cy), ImVec2(cx - 2, cy + 5), col, 2.0f); }
    return clicked;
}
static bool sideButton(const char* label, bool active, int chevron /* 0 none, 1 closed, 2 open */) {
    char text[120];
    std::snprintf(text, sizeof text, "%s%s##%s", label, chevron == 0 ? "" : (chevron == 1 ? "   >" : "   v"), label);
    pushNavStyle(active);
    bool c = ImGui::Button(text, ImVec2(ImGui::GetContentRegionAvail().x, 34));
    popNavStyle();
    return c;
}

void App::drawSidebar() {
    gCardBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    if (propScrollY >= 0) { ImGui::SetNextWindowScroll(ImVec2(0, propScrollY)); propScrollY = -1; }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::Begin("Sidebar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    const std::vector<Material>& lib = materialLibrary();
    const float full = ImGui::GetContentRegionAvail().x;
    const ImVec2 bsz(full, 34);

    {   // brand block: glossy blue logo, name
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        d->AddRectFilled(p, ImVec2(p.x + 38, p.y + 38), IM_COL32(28, 92, 224, 255), 9.0f);
        d->AddRectFilled(ImVec2(p.x, p.y + 19), ImVec2(p.x + 38, p.y + 38), IM_COL32(8, 38, 150, 170), 9.0f, ImDrawFlags_RoundBottom);
        d->AddRectFilled(ImVec2(p.x + 2, p.y + 2), ImVec2(p.x + 36, p.y + 17), IM_COL32(255, 255, 255, 78), 7.0f, ImDrawFlags_RoundTop);
        d->AddRect(p, ImVec2(p.x + 38, p.y + 38), IM_COL32(150, 200, 255, 210), 9.0f, 1.5f);
        d->AddText(ImGui::GetFont(), 19.0f, ImVec2(p.x + 9, p.y + 11), IM_COL32(0, 30, 110, 150), "VL");
        d->AddText(ImGui::GetFont(), 19.0f, ImVec2(p.x + 8, p.y + 9), IM_COL32(255, 255, 255, 255), "VL");
        d->AddText(ImGui::GetFont(), 19.0f, ImVec2(p.x + 48, p.y + 1), ImGui::GetColorU32(ImGuiCol_Text), "VL Lab");
        d->AddText(ImVec2(p.x + 48, p.y + 22), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Mechanics suite");
        ImGui::Dummy(ImVec2(full, 44));
    }
    if (sideButton("Dashboard", pageDashboard, 0)) focusDashboard = true;
    if (sideButton("Workspace", !pageDashboard, 0)) focusSim = true;

    // ---- DESIGN TOOLS
    if (sideHeader("tools", "DESIGN TOOLS", toolsOpen)) toolsOpen = !toolsOpen;
    dimWrapped("Build the system on the canvas: bodies, ground and supports, connections, or start from an example.");
    ImGui::Dummy(ImVec2(0, 2));

    // ---- the navbar: each button opens a drop-down list to its right
    auto navButton = [&](const char* label, const char* popupId, bool active, int index) {
        pushNavStyle(active);
        char text[96];
        std::snprintf(text, sizeof text, "%s   >##%s", label, popupId);
        bool clicked = ImGui::Button(text, bsz);
        popNavStyle();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        if (clicked || navMenu == index) { ImGui::OpenPopup(popupId); if (navMenu == index) navMenu = -1; }
        ImGui::SetNextWindowPos(ImVec2(mx.x + 6, mn.y), ImGuiCond_Always);
        return std::make_pair(ImGui::BeginPopup(popupId), clicked);
    };

    if (toolsOpen) {
    {
        pushNavStyle(tool == Tool::Select);
        bool clicked = ImGui::Button("Select / move (V)", bsz);
        popNavStyle();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to select, drag to move. While running, drag to push.");
        if (clicked) { tool = Tool::Select; linkFirst = -1; }
    }
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

    }                                                                         // end of the DESIGN TOOLS group
    if (tool == Tool::Select) {
        dimWrapped("Pick a tool above, then click the canvas to place it.");
    } else {
    beginCard("##nextcard");
    ImGui::PushID("nextone");
    ImGui::Text("Placing: %s", toolTitle(tool));
    ImGui::TextDisabled("Esc cancels. Shift keeps placing.");
    ImGui::Separator();
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
            {
                bool inf = floorLength >= 5000.0f;
                if (ImGui::Checkbox("Indefinite length (as long as the eye can see)", &inf)) floorLength = inf ? 20000.0f : 8.0f;
                if (!inf) {
                    labelled("Length (m)");
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##fl", &floorLength, 0.05f, 0.2f, 60.0f, "%.2f");
                }
            }
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
    ImGui::PopID();
    endCard();
    }

    // ---- SETTINGS: five sections that open in place under their button
    sideHeading("SETTINGS", "Set up the physics, the selected object, the run, the grid and the data you take away.");
    static const char* setNames[5] = {"Physical settings", "Object properties", "Simulation setup", "Grid settings", "Data extraction"};
    static const char* setDesc[5] = {"Ideal or realistic, friction, air drag, temperature, dead load, gravity.",
                                     "Size, material, cross-section, loads and temperature of the selected object.",
                                     "Run time, data rate, physics step, speed and the starting state.",
                                     "The grid, snapping and what is drawn on the canvas.",
                                     "Where the files go, which data, CSV or Excel, screen recording."};
    for (int i = 0; i < 5; ++i) {
        if (sideButton(setNames[i], sideOpen == i, sideOpen == i ? 2 : 1)) sideOpen = sideOpen == i ? -1 : i;
        if (sideOpen != i) continue;
        dimWrapped(setDesc[i]);
        char cid[24];
        std::snprintf(cid, sizeof cid, "##card%d", i);
        beginCard(cid);
        ImGui::PushID(i);
        switch (i) {
            case 0: drawEnvironment(); break;
            case 1: drawProperties(); break;
            case 2: drawRunPanel(); break;
            case 3: drawGrid(); break;
            default: drawData(); break;
        }
        ImGui::PopID();
        endCard();
    }

    // ---- VIEW
    sideHeading("VIEW", "Light or dark interface.");
    if (sideButton(darkTheme ? "Switch to light mode" : "Switch to dark mode", false, 0)) setTheme(!darkTheme);

    // ---- the strip at the bottom (like the signed-in user and Log out of a web app): the open design and Save
    {
        const float yEnd = ImGui::GetScrollY() + ImGui::GetWindowHeight() - 62;
        if (ImGui::GetCursorPosY() < yEnd) ImGui::SetCursorPosY(yEnd);
        ImGui::Separator();
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        d->AddCircleFilled(ImVec2(p.x + 18, p.y + 22), 16, IM_COL32(28, 92, 224, 255));
        d->AddCircleFilled(ImVec2(p.x + 18, p.y + 17), 11, IM_COL32(255, 255, 255, 60));
        const std::string nm = design().name.empty() ? std::string("Untitled") : design().name;
        d->AddText(ImGui::GetFont(), 17.0f, ImVec2(p.x + 42, p.y + 5), IM_COL32(255, 255, 255, 0), "");
        d->AddText(ImVec2(p.x + 42, p.y + 5), ImGui::GetColorU32(ImGuiCol_Text), (nm.substr(0, 22) + (dirty ? " *" : "")).c_str());
        d->AddText(ImVec2(p.x + 42, p.y + 24), ImGui::GetColorU32(ImGuiCol_TextDisabled), stateName());
        ImGui::SetCursorScreenPos(ImVec2(p.x + full - 56, p.y + 10));
        if (ImGui::SmallButton("Save")) saveDesign(false);
        ImGui::Dummy(ImVec2(0, 30));
    }
    ImGui::End();
}

// ====================================================================== Properties
void App::drawProperties() {
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
        if (b.isDynamic()) {
            ImGui::TextUnformatted("Apply a load  (a steady push or pull on this object)");
            {
                bool c2 = false;
                const float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
                if (!b.loadPull) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.40f, 0.88f, 1.0f));
                if (ImGui::Button("Push", ImVec2(hw, 0))) { b.loadPull = false; c2 = true; }
                if (!b.loadPull) ImGui::PopStyleColor();
                ImGui::SameLine();
                if (b.loadPull) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.40f, 0.88f, 1.0f));
                if (ImGui::Button("Pull", ImVec2(hw, 0))) { b.loadPull = true; c2 = true; }
                if (b.loadPull) ImGui::PopStyleColor();
                // the same load in newtons and in kilograms-force: edit either one
                const double kgPerN = 1.0 / 9.80665;
                double kg = b.loadN * kgPerN;
                ImGui::TextUnformatted("Load (newtons)");
                ImGui::SameLine(hw + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().WindowPadding.x);
                ImGui::TextUnformatted("= kilograms");
                ImGui::SetNextItemWidth(hw);
                if (ImGui::InputDouble("##loadN", &b.loadN, 10.0, 100.0, "%.2f N")) { b.loadN = std::clamp(b.loadN, 0.0, 1.0e7); c2 = true; }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(hw);
                if (ImGui::InputDouble("##loadKg", &kg, 1.0, 10.0, "%.3f kg")) { b.loadN = std::clamp(kg, 0.0, 1.0e6) * 9.80665; c2 = true; }
                labelled("Direction (deg: 0 = right, 90 = down, 180 = left, 270 = up)");
                c2 |= numField("##loadA", &b.loadAngleDeg, 5, "%.1f", -360, 360);
                const char* dirs[4] = {"Right", "Down", "Left", "Up"};
                const float dirAngle[4] = {0, 90, 180, 270};
                const float qw = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4;
                for (int k = 0; k < 4; ++k) {
                    if (k) ImGui::SameLine();
                    if (ImGui::Button(dirs[k], ImVec2(qw, 0))) { b.loadAngleDeg = dirAngle[k]; c2 = true; }
                }
                if (b.loadN > 0) ImGui::TextDisabled("%s %.1f N (%.2f kg) along %.0f deg%s", b.loadPull ? "Pulling with" : "Pushing with", b.loadN, b.loadN * kgPerN, b.loadAngleDeg, b.loadPull ? ", the opposite way" : "");
                if (c2) commit();
            }
            if (b.shape.preset == ShapePreset::Box) {
                ImGui::Spacing();
                ImGui::TextUnformatted("Distributed load  (spread along the length)");
                bool c3 = false;
                const char* types[] = {"None", "Uniform (rectangular)", "Triangular, rising to the right", "Triangular, falling to the right", "Triangular, peak in the middle", "Equation  q(x)"};
                int ty = b.dloadType;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##dltype", &ty, types, 6)) {
                    b.dloadType = ty;
                    if (ty != DL_None && ty != DL_Equation && b.dloadN <= 0) b.dloadN = 500;
                    if (ty == DL_Equation && b.dloadExpr.empty()) b.dloadExpr = "500*sin(pi*x/L)";
                    c3 = true;
                }
                if (b.dloadType != DL_None) {
                    const float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
                    if (b.dloadType != DL_Equation) {
                        ImGui::TextUnformatted("Peak intensity (N per metre)");
                        ImGui::SameLine(hw + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().WindowPadding.x);
                        ImGui::TextUnformatted("= kg per metre");
                        double kgm = b.dloadN / 9.80665;
                        ImGui::SetNextItemWidth(hw);
                        if (ImGui::InputDouble("##dlN", &b.dloadN, 50.0, 500.0, "%.1f N/m")) { b.dloadN = std::clamp(b.dloadN, 0.0, 1.0e7); c3 = true; }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(hw);
                        if (ImGui::InputDouble("##dlKg", &kgm, 5.0, 50.0, "%.2f kg/m")) { b.dloadN = std::clamp(kgm, 0.0, 1.0e6) * 9.80665; c3 = true; }
                    } else {
                        ImGui::TextUnformatted("q(x) in N per metre:");
                        char buf[200];
                        std::snprintf(buf, sizeof buf, "%s", b.dloadExpr.c_str());
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##dlexpr", buf, sizeof buf)) { b.dloadExpr = buf; c3 = true; }
                        std::string err;
                        std::shared_ptr<const Expr> ex = Expr::cached(b.dloadExpr, &err);
                        if (!ex) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "Not a valid formula: %s", err.c_str());
                        const struct { const char* n; const char* f; } presets[] = {{"Sine", "500*sin(pi*x/L)"}, {"Parabola", "3200*x*(L-x)/L^2"}, {"Ramp", "300*x/L"}, {"Polynomial", "100+60*x+40*x^2"}, {"Cosine", "400+400*cos(2*pi*x/L)"}};
                        const float pw = (ImGui::GetContentRegionAvail().x - 4 * ImGui::GetStyle().ItemSpacing.x) / 5;
                        for (int k = 0; k < 5; ++k) {
                            if (k) ImGui::SameLine();
                            if (ImGui::Button(presets[k].n, ImVec2(pw, 0))) { b.dloadExpr = presets[k].f; c3 = true; }
                        }
                        ImGui::TextDisabled("x = distance from the left end (m), L = length (m), t = time (s), pi, e.   + - * / ^ ( )   sin cos tan sqrt abs exp ln pow(a,b) min(a,b) max(a,b) step(x)");
                    }
                    labelled("Direction (deg: 90 = down, 270 = up)");
                    c3 |= numField("##dlA", &b.dloadAngleDeg, 5, "%.1f", -360, 360);
                    double x0, x1;
                    distributedLoadSpan(b, &x0, &x1);
                    const float hw2 = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
                    ImGui::TextUnformatted("From x (m)");
                    ImGui::SameLine(hw2 + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().WindowPadding.x);
                    ImGui::TextUnformatted("To x (m)");
                    ImGui::SetNextItemWidth(hw2);
                    double ax0 = x0, ax1 = x1;
                    if (ImGui::InputDouble("##dlx0", &ax0, 0.1, 1.0, "%.2f")) { b.dloadX0 = std::clamp(ax0, 0.0, b.shape.w); c3 = true; }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(hw2);
                    if (ImGui::InputDouble("##dlx1", &ax1, 0.1, 1.0, "%.2f")) { b.dloadX1 = std::clamp(ax1, 0.0, b.shape.w); c3 = true; }
                    if (ImGui::Button("Whole length", ImVec2(-1, 0))) { b.dloadX0 = 0; b.dloadX1 = -1; c3 = true; }
                    {   // what the load adds up to
                        std::string err;
                        std::shared_ptr<const Expr> ex = b.dloadType == DL_Equation ? Expr::cached(b.dloadExpr, &err) : nullptr;
                        double tot = 0, mom = 0, peak = 0;
                        const int ns = 400;
                        for (int k = 0; k < ns; ++k) {
                            double xm = x0 + (x1 - x0) * (k + 0.5) / ns, q = distributedLoadAt(b, ex.get(), xm, 0.0), dx = (x1 - x0) / ns;
                            tot += q * dx; mom += q * dx * xm; peak = std::max(peak, std::fabs(q));
                        }
                        if (std::fabs(tot) > 1e-9) ImGui::TextDisabled("Resultant %.1f N (%.2f kg) acting %.2f m from the left end.  Peak %.1f N/m (%.2f kg/m)", tot, tot / 9.80665, mom / tot, peak, peak / 9.80665);
                        else ImGui::TextDisabled("This load adds up to zero.");
                    }
                }
                if (c3) commit();
            }
            if (isBeamBody(b)) {                                                      // the dead load: the beam's own weight, spread along its length
                const double mu = b.massKg / b.shape.w;
                ImGui::Spacing();
                ImGui::TextDisabled("Dead load (own weight): %.4g kg/m  =  %.4g N/m over the %.2f m length.  %s", mu, mu * m.env.gravity, b.shape.w, m.env.deadLoad ? "Included in the bending." : "Not applied: switch on Dead load on the canvas to include it.");
            }
            ImGui::Separator();
        }
        if (b.kind != BodyKind::Anchor) {
            ImGui::TextUnformatted(b.shape.preset == ShapePreset::Line ? "Size" : "Shape and size");
            bool c = false;
            switch (b.shape.preset) {
                case ShapePreset::Circle: labelled("Radius (m)"); c |= numField("##rad", &b.shape.radius, 0.05, "%.3f", 0.02, 10); break;
                case ShapePreset::Box: labelled("Width (m)"); c |= numField("##bw", &b.shape.w, 0.1, "%.3f", 0.02, 60); labelled("Height (m)"); c |= numField("##bh", &b.shape.h, 0.1, "%.3f", 0.02, 60, b.secType != 0); break;
                case ShapePreset::Regular: {
                    labelled("Radius (m)"); c |= numField("##prad", &b.shape.radius, 0.05, "%.3f", 0.02, 10);
                    labelled("Sides"); ImGui::SetNextItemWidth(-1);
                    int sd = b.shape.sides;
                    if (ImGui::SliderInt("##sides", &sd, 3, 12)) { b.shape.sides = sd; c = true; }
                    break;
                }
                case ShapePreset::Line: {
                    bool inf = b.shape.w >= 5000.0;
                    if (ImGui::Checkbox("Indefinite length (as long as the eye can see)", &inf)) { b.shape.w = inf ? 20000.0 : 10.0; c = true; }
                    if (!inf) { labelled("Length (m)"); c |= numField("##ll", &b.shape.w, 0.1, "%.3f", 0.05, 4000); }
                    else ImGui::TextDisabled("Nothing can fall off either end.");
                    break;
                }
                default: labelled("Leg along x (m)"); c |= numField("##ww", &b.shape.w, 0.1, "%.3f", 0.02, 20); labelled("Leg along y (m)"); c |= numField("##wh", &b.shape.h, 0.1, "%.3f", 0.02, 20); break;
            }
            if (c) commit();
        }
        if (b.isDynamic() && b.shape.preset == ShapePreset::Box) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Cross-section  (seen end-on, bending about its horizontal axis)");
            bool cs = false;
            int ty = b.secType;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##sectype", sectionName(ty))) {
                for (int k = 0; k < SEC_COUNT; ++k) if (ImGui::Selectable(sectionName(k), k == ty)) ty = k;
                ImGui::EndCombo();
            }
            if (ty != b.secType) {
                if (b.secType == SEC_SLAB && ty != SEC_SLAB) {                       // start from sensible proportions of the current height
                    b.secH = std::max(0.02, b.shape.h);
                    b.secB = std::max(0.02, b.secH * 0.5);
                    b.secTf = std::max(0.002, b.secH * 0.08);
                    b.secTw = std::max(0.002, b.secH * 0.05);
                }
                b.secType = ty;
                cs = true;
            }
            const int dims = sectionDims(b.secType);
            const bool flanged = b.secType == SEC_I || b.secType == SEC_T || b.secType == SEC_CHANNEL;
            if (dims & 1) { labelled(flanged ? "Flange width (m)" : "Width (m)"); cs |= numField("##secB", &b.secB, 0.01, "%.4f", 0.005, 5); }
            if (dims & 2) { labelled((b.secType == SEC_CIRCLE || b.secType == SEC_TUBE) ? "Diameter (m)" : "Height (m)"); cs |= numField("##secH", &b.secH, 0.01, "%.4f", 0.02, 5); }
            if (dims & 4) { labelled((b.secType == SEC_BOX || b.secType == SEC_TUBE) ? "Wall thickness (m)" : "Flange thickness (m)"); cs |= numField("##secTf", &b.secTf, 0.001, "%.4f", 0.001, 1); }
            if (dims & 8) { labelled("Web thickness (m)"); cs |= numField("##secTw", &b.secTw, 0.001, "%.4f", 0.001, 1); }
            if (cs) commit();
            const Section sc = makeSection(b);
            {
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                const float sw = ImGui::GetContentRegionAvail().x;
                drawSectionSketch(sc, p0, ImVec2(sw, 118), nullptr, palette());
                ImGui::Dummy(ImVec2(sw, 122));
            }
            ImGui::TextDisabled("Area A = %.4g cm2     I (second moment of area) = %.5g cm4", sc.A * 1e4, sc.I * 1e8);
            ImGui::TextDisabled("Section modulus Z = %.4g cm3     radius of gyration %.2f mm", sc.Z * 1e6, sc.rg * 1000.0);
            ImGui::TextDisabled("Neutral axis %.2f mm from the top, %.2f mm from the bottom", sc.cTop * 1000.0, sc.cBot * 1000.0);
            if (b.secType == SEC_SLAB) ImGui::TextDisabled("A slab is a rectangle 1 m wide (the plain 2-D beam). Pick a real section to design it.");
            else if (b.massFromDensity) ImGui::TextDisabled("Mass = density x section area x length = %.4g kg", b.massKg);
            if (isBeamBody(b)) ImGui::TextDisabled("E I = %.4g N m2 (bending stiffness)", b.mat.youngs * sc.I);
            else ImGui::TextDisabled("This body is shorter than 4 x its height, so it is not treated as a beam.");

            ImGui::Spacing();
            ImGui::TextUnformatted("Temperature  (thermal expansion)");
            bool ct = false;
            labelled("Temperature change of this body (deg C)");
            ct |= numField("##tdelta", &b.tempDelta, 5, "%.1f", -1000, 3000);
            labelled("Lower face hotter than the upper face by (deg C)");
            ct |= numField("##tgrad", &b.tempGrad, 5, "%.1f", -1000, 1000);
            double al = b.mat.alphaT * 1e6;
            labelled("Expansion coefficient alpha (x 1e-6 per K)");
            if (numField("##alpha", &al, 1, "%.2f", 0, 1000)) { b.mat.alphaT = al * 1e-6; ct = true; }
            if (ct) commit();
            {
                const double dT = b.tempDelta + m.env.deltaT, eps = b.mat.alphaT * dT, Lb = b.shape.w;
                if (dT != 0) ImGui::TextDisabled("Free growth: strain %.0f microstrain = %.3f mm over %.2f m  (total change %.1f C)", eps * 1e6, eps * Lb * 1000.0, Lb, dT);
                if (b.tempGrad != 0 && isBeamBody(b)) ImGui::TextDisabled("Free bow of a simply supported span: %.3f mm  (alpha dT L^2 / 8 h)", std::fabs(b.mat.alphaT * b.tempGrad) * Lb * Lb / (8 * sc.H) * 1000.0);
                if (dT == 0 && b.tempGrad == 0) ImGui::TextDisabled("0 = no thermal effect. Pinned at both ends a beam cannot grow: heating it puts -E alpha dT of stress in it.");
            }
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
            double gpa = b.mat.youngs / 1e9;
            labelled("Stiffness E (GPa): how much it bends or squashes");
            bool ce = numField("##young", &gpa, 1.0, "%.3f", 0.001, 2000);
            if (ce) b.mat.youngs = gpa * 1e9;
            if (c || ce) { if (c) b.mat.name = "Custom"; commit(); }
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
            ImGui::TextUnformatted("Guide (like a real tuned mass damper on a rail)");
            {
                const float gw = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
                if (ImGui::Button("Vertical guide", ImVec2(gw, 0))) { b.lockX = false; b.lockY = false; b.lockRot = true; b.lockX = true; c = true; }
                ImGui::SameLine();
                if (ImGui::Button("Horizontal guide", ImVec2(gw, 0))) { b.lockX = false; b.lockY = true; b.lockRot = true; c = true; }
                ImGui::SameLine();
                if (ImGui::Button("No guide", ImVec2(gw, 0))) { b.lockX = b.lockY = b.lockRot = false; c = true; }
            }
            ImGui::TextDisabled("A mass hung on springs with nothing guiding it tilts and topples, as in real life. A guide keeps it upright.");
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
            labelled("Pulse length (s): 0 = continuous"); c |= numField("##dt", &b.driveT, 0.005, "%.3f", 0, 100);
            if (b.driveT > 0) ImGui::TextDisabled("one half-sine pulse of force at the start of the run (frequency is ignored)");
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
        if (l.kind == LinkKind::Spring || l.kind == LinkKind::Damper || l.kind == LinkKind::Rod) {
            ImGui::Separator();
            ImGui::TextUnformatted("Where it attaches and which way it runs");
            const Vec2 wa = m.worldPoint(l.a, l.la), wb = m.worldPoint(l.b, l.lb), dd = wb - wa;
            double ang = std::atan2(dd.y, dd.x) * 180.0 / kPi;
            if (ang < 0) ang += 360.0;
            ImGui::TextDisabled("Now: %.1f deg from the first end to the second, %.3f m long  (0 = right, 90 = down)", ang, length(dd));
            static int keepSecond = 0;
            ImGui::TextUnformatted("Keep fixed:");
            ImGui::SameLine();
            ImGui::RadioButton("First end", &keepSecond, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Second end", &keepSecond, 1);
            ImGui::TextDisabled("The other end slides along the new direction to the edge of its body.");
            const float qw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
            bool moved = false;
            if (ImGui::Button("Make it vertical", ImVec2(qw, 0))) moved = reorientLink(selLink, Vec2(0, 1), keepSecond == 0);
            ImGui::SameLine();
            if (ImGui::Button("Make it horizontal", ImVec2(qw, 0))) moved = reorientLink(selLink, Vec2(1, 0), keepSecond == 0);
            static float setAng = 90.0f;
            ImGui::SetNextItemWidth(110);
            ImGui::DragFloat("##setang", &setAng, 1.0f, -360.0f, 360.0f, "%.0f deg");
            ImGui::SameLine();
            if (ImGui::Button("Set this angle")) moved = reorientLink(selLink, Vec2(std::cos(setAng * kPi / 180.0), std::sin(setAng * kPi / 180.0)), keepSecond == 0);
            if (moved) c = true;
            // the two attachment points, in each body's own frame (centre = 0, 0)
            labelled("First end on its body: x, y (m)");
            {
                const float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
                ImGui::SetNextItemWidth(hw);
                if (ImGui::InputDouble("##lax", &l.la.x, 0.05, 0.25, "%.3f")) { m.refreshAutoRest(); c = true; }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(hw);
                if (ImGui::InputDouble("##lay", &l.la.y, 0.05, 0.25, "%.3f")) { m.refreshAutoRest(); c = true; }
            }
            labelled("Second end on its body: x, y (m)");
            {
                const float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
                ImGui::SetNextItemWidth(hw);
                if (ImGui::InputDouble("##lbx", &l.lb.x, 0.05, 0.25, "%.3f")) { m.refreshAutoRest(); c = true; }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(hw);
                if (ImGui::InputDouble("##lby", &l.lb.y, 0.05, 0.25, "%.3f")) { m.refreshAutoRest(); c = true; }
            }
        }
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
}

// ====================================================================== Environment
void App::drawEnvironment() {
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
    c |= ImGui::Checkbox("Dead load: each beam's own weight bends it", &e.deadLoad);
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
    ImGui::Spacing();
    labelled("Temperature change of everything (deg C)");
    if (numField("##envdt", &e.deltaT, 5, "%.1f", -1000, 3000)) commit();
    ImGui::TextDisabled("Added to each body's own change: beams expand, held beams build up stress.");

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
}

// ====================================================================== Run panel
// a dim paragraph that wraps inside a narrow panel
static void wrapDisabled(const char* fmt, ...) {
    char buf[600];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", buf);
    ImGui::PopStyleColor();
}

void App::drawRunPanel() {
    Model& m = design();
    ImGui::TextUnformatted("Run time");
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
    ImGui::TextDisabled("about %.0f rows of data", rows);
    ImGui::Separator();

    ImGui::TextUnformatted("Speed and accuracy");
    double dtms = m.env.dt * 1000.0;
    labelled("Physics step (ms)");
    if (numField("##dtms", &dtms, 0.05, "%.3f", 0.02, 2.0)) { pushUndo(); m.env.dt = dtms / 1000.0; edited(); }
    helpMarker("Smaller is more accurate and slower. 0.25 ms is a good default.");
    float ts = (float)timeScale;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##ts", &ts, 0.1f, 4.0f, "real-time speed %.2fx")) timeScale = ts;
    wrapDisabled("Real-time speed slows or speeds up what you watch. The numbers do not change.");
    ImGui::Separator();

    ImGui::TextUnformatted("Initialization");
    ImGui::TextWrapped("Every run starts from the drawn state: each body at its drawn position, with the starting velocity and spin set in Object > Starting motion, and the loads, temperatures and gravity set in Object and Physical.");
    {
        int fixedN = 0;
        for (const Body& b : m.bodies) if (b.kind != BodyKind::Dynamic) ++fixedN;
        ImGui::TextDisabled("%d moving bodies, %d fixed, %d connections", m.dynamicCount(), fixedN, (int)m.links.size());
    }
    if (ImGui::Button("Reset to the drawn start", ImVec2(-1, 0))) stopRun();
    ImGui::Separator();

    // ---- progress
    double tnow = ses.hasWorld() ? ses.simTime() : (haveResults && !ses.telemetry().t.empty() ? ses.telemetry().t.back() : 0.0);
    float frac = (float)clampd(m.run.duration > 0 ? tnow / m.run.duration : 0, 0, 1);
    char pb[96];
    std::snprintf(pb, sizeof pb, "%.2f / %.2f s   (%d%%)", tnow, m.run.duration, (int)(frac * 100));
    ImGui::ProgressBar(frac, ImVec2(-1, 0), pb);
    ImGui::TextDisabled("Run, Pause and Stop are at the top of the canvas.");
    if (state == RunState::Computing) { if (ImGui::Button("Cancel", ImVec2(-1, 0))) stopRun(); }
    ImGui::TextWrapped("%s", statusLine.c_str());
    if (resultsStale && haveResults) ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "The design changed after this run: run again to update the data.");

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
}

// ====================================================================== Grid panel
void App::drawGrid() {
    ImGui::TextUnformatted("Grid");
    ImGui::Checkbox("Show the grid", &showGrid);
    ImGui::Checkbox("Show the grid numbers", &showGridNumbers);
    ImGui::Spacing();
    ImGui::TextUnformatted("Snapping");
    ImGui::Checkbox("Snap to grid", &snap);
    ImGui::BeginDisabled(!snap);
    labelled("Snap step (m)");
    double st = snapStep;
    if (numField("##snapstep", &st, 0.05, "%.3f m", 0.005, 5)) snapStep = st;
    const double presetsS[5] = {0.05, 0.1, 0.25, 0.5, 1.0};
    const float pw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4) / 5;
    for (int i = 0; i < 5; ++i) {
        if (i) ImGui::SameLine();
        char lb[16];
        std::snprintf(lb, sizeof lb, "%g", presetsS[i]);
        bool cur = std::fabs(snapStep - presetsS[i]) < 1e-9;
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.40f, 0.88f, 1.0f));
        if (ImGui::Button(lb, ImVec2(pw, 0))) snapStep = presetsS[i];
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::EndDisabled();
    wrapDisabled("Hold Alt while placing or dragging to ignore the snap.");
    ImGui::Separator();
    ImGui::TextUnformatted("What is drawn on the canvas");
    ImGui::Checkbox("Legend (bottom left)", &showLabels);
    ImGui::Checkbox("Contact points", &showContacts);
    ImGui::Checkbox("Velocity arrows", &showVectors);
    ImGui::Checkbox("Reaction force arrows (joints, supports)", &showReactions);
    ImGui::Separator();
    ImGui::TextUnformatted("View");
    if (ImGui::Button("Fit the view to the design", ImVec2(-1, 0))) needFit = true;
    wrapDisabled("Zoom: mouse wheel.  Pan: right or middle button drag.  Home fits the view.");
    ImGui::TextDisabled("Scale now: %.0f pixels per metre", scale);
    if (ImGui::Button("Reset the panel layout", ImVec2(-1, 0))) resetLayout = true;
}

// ====================================================================== Data extraction panel
static bool endsWithText(const std::string& s, const char* suf) {
    size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

void App::drawData() {
    bool settingsChanged = false;
    auto pathRow = [&](const char* label, const char* id, std::string& dir) {
        ImGui::TextUnformatted(label);
        char buf[600];
        std::snprintf(buf, sizeof buf, "%s", dir.c_str());
        const float sp = ImGui::GetStyle().ItemSpacing.x, bw = 74.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * bw - 2 * sp);
        if (ImGui::InputText(id, buf, sizeof buf)) { dir = buf; settingsChanged = true; }
        ImGui::SameLine();
        ImGui::PushID(id);
        if (ImGui::Button("Browse", ImVec2(bw, 0))) { std::string p = pickFolderDialog(label, dir); if (!p.empty()) { dir = p; settingsChanged = true; } }
        ImGui::SameLine();
        if (ImGui::Button("Open", ImVec2(bw, 0))) { ensureDirectory(dir); openInShell(dir, false); }
        ImGui::PopID();
    };
    ImGui::TextUnformatted("Where the files go");
    pathRow("Data files (CSV and Excel)", "##expdir", exportsDir);
    pathRow("Screen recordings (.mp4)", "##recdir", recordingsDir);
    {
        char nb[200];
        std::snprintf(nb, sizeof nb, "%s", exportBase.c_str());
        labelled("File name (empty: the design name)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##expbase", nb, sizeof nb)) { exportBase = nb; settingsChanged = true; }
        if (ImGui::Checkbox("Add the date and time to the name", &exportStamp)) settingsChanged = true;
        ImGui::TextDisabled("e.g. %s%s.csv", exportBaseName().c_str(), "");
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Which data to extract");
    if (ImGui::RadioButton("All channels", exportAll)) { exportAll = true; settingsChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Only the channels I tick", !exportAll)) { exportAll = false; settingsChanged = true; }
    const Telemetry& t = ses.telemetry();
    if (!exportAll) {
        if (t.channels.empty()) {
            wrapDisabled("Run the model once: its channels are listed here.");
        } else {
            if (exportSel.size() != t.channels.size()) {
                exportSel.assign(t.channels.size(), 0);
                for (size_t c = 0; c < t.channels.size(); ++c) { const std::string& n = t.channels[c].name; if (endsWithText(n, " dx") || endsWithText(n, " dy")) exportSel[c] = 1; }
            }
            auto pick = [&](std::initializer_list<const char*> suffixes, const char* group, const char* unit) {
                exportSel.assign(t.channels.size(), 0);
                for (size_t c = 0; c < t.channels.size(); ++c) {
                    const Channel& ch = t.channels[c];
                    bool take = group && ch.group == group;
                    if (unit && ch.unit == unit) take = true;
                    for (const char* s : suffixes) if (endsWithText(ch.name, s)) take = true;
                    if (take) exportSel[c] = 1;
                }
            };
            const char* quick[7] = {"All", "None", "Positions", "Velocities", "Forces", "Energy", "Deformation"};
            for (int qi = 0; qi < 7; ++qi) {
                if (qi) {
                    ImGui::SameLine();
                    if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(quick[qi]).x + 2 * ImGui::GetStyle().FramePadding.x) ImGui::NewLine();
                }
                if (!ImGui::SmallButton(quick[qi])) continue;
                switch (qi) {
                    case 0: exportSel.assign(t.channels.size(), 1); break;
                    case 1: exportSel.assign(t.channels.size(), 0); break;
                    case 2: pick({" x", " y", " dx", " dy", " angle"}, nullptr, nullptr); break;
                    case 3: pick({" vx", " vy", " speed", " omega"}, nullptr, nullptr); break;
                    case 4: pick({}, nullptr, "N"); break;
                    case 5: pick({}, "Energy", nullptr); break;
                    default: pick({" deflection", " compression"}, nullptr, nullptr); break;
                }
            }
            int on = 0;
            for (char v : exportSel) on += v ? 1 : 0;
            wrapDisabled("%d of %d channels ticked. (Excel always carries the Energy channels too: its summary sheet needs them.)", on, (int)t.channels.size());
            ImGui::BeginChild("##exchan", ImVec2(-1, 190), true);
            std::string group;
            bool open = false;
            for (size_t c = 0; c < t.channels.size(); ++c) {
                if (t.channels[c].group != group) {
                    if (open) ImGui::TreePop();
                    group = t.channels[c].group;
                    open = ImGui::TreeNodeEx(group.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                }
                if (open) {
                    std::string label = t.channels[c].name.substr(group.size() + 1) + (t.channels[c].unit.empty() ? "" : "  (" + t.channels[c].unit + ")");
                    bool v = exportSel[c] != 0;
                    ImGui::PushID((int)c);
                    if (ImGui::Checkbox(label.c_str(), &v)) exportSel[c] = v ? 1 : 0;
                    ImGui::PopID();
                }
            }
            if (open) ImGui::TreePop();
            ImGui::EndChild();
        }
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Take the data now");
    ImGui::BeginDisabled(!haveResults || ses.telemetry().rows() == 0);
    {
        const float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.25f, 1));
        if (ImGui::Button("Excel (.xlsx) with charts", ImVec2(hw, 34))) { ensureDirectory(exportsDir); exportExcelTo(exportFileName(".xlsx")); }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("A workbook: Summary, Telemetry (every sample), Charts, plus Spectrum / Modal / Frequency response if you computed them.");
        ImGui::SameLine();
        if (ImGui::Button("CSV table (.csv)", ImVec2(hw, 34))) { ensureDirectory(exportsDir); exportCsvTo(exportFileName(".csv")); }
    }
    ImGui::EndDisabled();
    if (!haveResults) ImGui::TextDisabled("Run the model first.");
    if (ImGui::Checkbox("Save a CSV automatically when a run finishes", &autoExportCsv)) settingsChanged = true;
    if (ImGui::Checkbox("Save an Excel workbook automatically when a run finishes", &autoExportXlsx)) settingsChanged = true;
    if (!lastExportPath.empty()) {
        ImGui::TextDisabled("Last saved:");
        ImGui::TextWrapped("%s", lastExportPath.c_str());
        if (ImGui::SmallButton("Open file")) openInShell(lastExportPath, false);
        ImGui::SameLine();
        if (ImGui::SmallButton("Show in folder")) openInShell(lastExportPath, true);
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Schematic diagram");
    wrapDisabled("The design as a labelled figure: shapes, springs, dampers, supports and loads, each named with a leader line. Saved as an SVG, which opens in a browser, Word or PowerPoint and stays sharp at any size.");
    ImGui::Checkbox("Names on the parts", &schemNames);
    ImGui::Checkbox("Their values (mass, k, c, loads)", &schemValues);
    ImGui::Checkbox("Arrowheads on the label lines", &schemArrows);
    ImGui::Checkbox("Show the applied loads", &schemLoads);
    if (ImGui::Button("Save the schematic (.svg)", ImVec2(-1, 34))) exportSchematic();
    ImGui::Separator();

    ImGui::TextUnformatted("Screen recording");
    if (recording) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1));
    if (ImGui::Button(recording ? "Stop recording" : "Record video (F9)", ImVec2(-1, 32))) { if (recording) stopRecording(); else { ensureDirectory(recordingsDir); startRecording(); } }
    if (recording) ImGui::PopStyleColor();
    ImGui::Checkbox("Record the whole window, not just the canvas", &recWholeWindow);
    if (ImGui::Checkbox("Record automatically during every run", &autoRecord)) settingsChanged = true;
    wrapDisabled("Video frames are exactly 1/60 s of physics, so the movie plays at the right speed.");
    if (settingsChanged) saveDataSettings();
}


// ====================================================================== dialogs
void App::drawWelcome() {
    ImGui::OpenPopup("Welcome to VL Lab Mechanics Suite");
    ImVec2 c = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Welcome to VL Lab Mechanics Suite", &showWelcome, ImGuiWindowFlags_AlwaysAutoResize)) {
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
    ImGui::OpenPopup("About VL Lab Mechanics Suite");
    ImVec2 c = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About VL Lab Mechanics Suite", &showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("VL Lab Mechanics Suite  1.2");
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
