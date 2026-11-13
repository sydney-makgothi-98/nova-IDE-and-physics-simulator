// dashboard.cpp -- the front page: a cover video, the design being worked on, and every saved project.
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "app.hpp"
#include "imgui_internal.h"

namespace vlab {

static std::string exeFolder() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf, n);
    size_t k = s.find_last_of("\\/");
    return k == std::string::npos ? std::string(".") : s.substr(0, k);
}

static bool fileExists(const std::string& p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

static std::string whenText(const FILETIME& ft) {
    FILETIME lf;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &lf);
    FileTimeToSystemTime(&lf, &st);
    static const char* mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char b[64];
    std::snprintf(b, sizeof b, "%d %s %d  %02d:%02d", (int)st.wDay, mon[(st.wMonth + 11) % 12], (int)st.wYear, (int)st.wHour, (int)st.wMinute);
    return b;
}

// ---------------------------------------------------------------------------------------------- the cover video
void App::loadCover() {
    cover.reset();
    if (coverTex && platform) { platform->destroyTexture(coverTex); coverTex = nullptr; }
    std::string path;
    for (const std::string& c : {coverTarget(), exeFolder() + "\\dashboard_cover.mp4", exeFolder() + "\\Cover\\dashboard_cover.mp4"})
        if (fileExists(c)) { path = c; break; }
    if (path.empty()) return;
    auto vp = std::make_unique<VideoPlayer>();
    if (!vp->open(path)) { say("The cover video could not be opened (use an H.264 .mp4).", true); return; }
    coverTex = platform ? platform->createTexture(vp->width(), vp->height()) : nullptr;
    if (!coverTex) return;
    coverTexW = vp->width();
    coverTexH = vp->height();
    cover = std::move(vp);
}

// ---------------------------------------------------------------------------------------------- the project list
void App::scanProjects() {
    projects.clear();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((designsDir + "\\*.vlab").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            Project p;
            p.path = designsDir + "\\" + fd.cFileName;
            p.name = fd.cFileName;
            if (p.name.size() > 5) p.name.resize(p.name.size() - 5);
            p.mtime = ((long long)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
            p.bytes = ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            p.when = whenText(fd.ftLastWriteTime);
            std::string err;
            if (loadModelFile(p.path, p.model, &err)) projects.push_back(std::move(p));
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(projects.begin(), projects.end(), [](const Project& a, const Project& b) { return a.mtime > b.mtime; });
    projectsScanned = now;
}

// a small picture of a design: its bodies drawn to fit the box
static void drawPreview(ImDrawList* dl, const Model& m, ImVec2 p0, ImVec2 p1, ImU32 floorInk, ImU32 frame) {
    dl->AddRect(p0, p1, frame, 4.0f);
    Vec2 lo(1e30, 1e30), hi(-1e30, -1e30);
    auto grow = [&](Vec2 v) { lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); };
    bool any = false;
    for (const Body& b : m.bodies) {
        if (b.kind != BodyKind::Dynamic) continue;                              // fit to the moving things, floors may be huge
        any = true;
        double r = b.shape.boundingRadius();
        grow(b.pos + Vec2(r, r));
        grow(b.pos - Vec2(r, r));
    }
    if (!any) for (const Body& b : m.bodies) { any = true; grow(b.pos + Vec2(1, 1)); grow(b.pos - Vec2(1, 1)); }
    if (!any) return;
    double bw = std::max(0.5, hi.x - lo.x), bh = std::max(0.5, hi.y - lo.y);
    bw *= 1.5; bh *= 1.5;
    Vec2 c((lo.x + hi.x) / 2, (lo.y + hi.y) / 2);
    float W = p1.x - p0.x - 8, H = p1.y - p0.y - 8;
    double s = std::min(W / bw, H / bh);
    auto tp = [&](Vec2 v) { return ImVec2((float)((p0.x + p1.x) / 2 + (v.x - c.x) * s), (float)((p0.y + p1.y) / 2 + (v.y - c.y) * s)); };
    dl->PushClipRect(p0, p1, true);
    for (const Body& b : m.bodies) {
        Rot r(b.angle);
        if (b.kind == BodyKind::Anchor) { dl->AddCircleFilled(tp(b.pos), 2.5f, floorInk); continue; }
        if (b.shape.preset == ShapePreset::Line) { dl->AddLine(tp(b.pos + r.apply(Vec2(-b.shape.w / 2, 0))), tp(b.pos + r.apply(Vec2(b.shape.w / 2, 0))), floorInk, 2.0f); continue; }
        ImU32 col = b.kind == BodyKind::Static ? floorInk : argbToImU32(b.color);
        if (b.shape.isCircle()) { dl->AddCircleFilled(tp(b.pos), std::max(1.5f, (float)(b.shape.radius * s)), col, 20); continue; }
        std::vector<ImVec2> pts;
        for (const Vec2& v : b.shape.verts) pts.push_back(tp(b.pos + r.apply(v)));
        if (pts.size() >= 3) dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), col);
    }
    dl->PopClipRect();
}

// ---------------------------------------------------------------------------------------------- the window
void App::drawDashboard() {
    // a full page: it fills everything to the right of the sidebar, like a page of a web app
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float x0 = vp->WorkPos.x + vp->WorkSize.x * 0.22f;
    if (ImGuiWindow* lw = ImGui::FindWindowByName("Library")) x0 = lw->Pos.x + lw->Size.x + 1.0f;
    ImGui::SetNextWindowPos(ImVec2(x0, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(std::max(300.0f, vp->WorkPos.x + vp->WorkSize.x - x0), vp->WorkSize.y));
    if (dashJustShown) { ImGui::SetNextWindowFocus(); dashJustShown = false; }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    bool open = ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    if (!open) { ImGui::End(); return; }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const Pal pal = palette();
    if (!coverTried) { coverTried = true; loadCover(); }
    if (now - projectsScanned > 4.0) scanProjects();

    // ---- cover
    const float W = ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(ImGui::GetFont(), 30.0f);
    ImGui::TextUnformatted("Dashboard");
    ImGui::PopFont();
    ImGui::TextDisabled("Welcome back. Pick up where you left off, or start something new.");
    ImGui::Spacing();
    const float H = std::clamp(W * 0.22f, 190.0f, 320.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + W, p0.y + H);
    if (cover && coverTex) {
        if (cover->advance(std::min<double>(ImGui::GetIO().DeltaTime, 0.1))) platform->updateTexture(coverTex, cover->pixels().data(), coverTexW * 4);
        double ai = (double)coverTexW / coverTexH, at = (double)W / H;                  // fill the banner, cropping the video's edges
        ImVec2 uv0(0, 0), uv1(1, 1);
        if (ai > at) { float k = (float)(at / ai); uv0.x = (1 - k) / 2; uv1.x = 1 - uv0.x; }
        else { float k = (float)(ai / at); uv0.y = (1 - k) / 2; uv1.y = 1 - uv0.y; }
        dl->AddImageRounded((ImTextureID)platform->textureId(coverTex), p0, p1, uv0, uv1, IM_COL32_WHITE, 10.0f);
    } else {
        dl->AddRectFilledMultiColor(p0, p1, IM_COL32(18, 52, 140, 255), IM_COL32(40, 120, 235, 255), IM_COL32(20, 70, 190, 255), IM_COL32(10, 28, 90, 255));
        dl->AddText(ImVec2(p0.x + W - 360, p0.y + 12), IM_COL32(210, 218, 240, 200), "Cover video: use \"Choose cover video\" below");
    }
    dl->AddRectFilledMultiColor(ImVec2(p0.x, p0.y + H * 0.35f), p1, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(8, 10, 18, 215), IM_COL32(8, 10, 18, 215));
    dl->AddText(ImGui::GetFont(), 38.0f, ImVec2(p0.x + 26, p1.y - 84), IM_COL32(255, 255, 255, 255), "VL Lab Mechanics Suite");
    dl->AddText(ImGui::GetFont(), 19.0f, ImVec2(p0.x + 28, p1.y - 40), IM_COL32(222, 228, 245, 235), "Design a system, run it, and understand how it moves, bends and vibrates.");
    ImGui::Dummy(ImVec2(W, H));
    ImGui::Spacing();

    // ---- actions
    if (ImGui::Button("New design", ImVec2(130, 34))) { newDesign(); focusSim = true; }
    ImGui::SameLine();
    if (ImGui::Button("Open design...", ImVec2(130, 34))) { loadDesignDialog(); focusSim = true; }
    ImGui::SameLine();
    if (ImGui::Button("Examples", ImVec2(110, 34))) ImGui::OpenPopup("dash_examples");
    if (ImGui::BeginPopup("dash_examples")) {
        const char* cats[3] = {"Basic", "Intermediate", "Complex"};
        for (int cat = 0; cat < 3; ++cat) {
            ImGui::TextDisabled("%s", cats[cat]);
            int n = 0;
            for (const Example& e : exampleGallery()) {
                if (e.category == cat && ImGui::Selectable(e.title.c_str())) { loadExample(n); focusSim = true; }
                if (e.category == cat && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.summary.c_str());
                ++n;
            }
            ImGui::Spacing();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Choose cover video...", ImVec2(180, 34))) {
        std::string src = openFileDialog("Video (*.mp4;*.m4v;*.mov;*.wmv)\0*.mp4;*.m4v;*.mov;*.wmv\0\0", nullptr);
        if (!src.empty()) {
            if (CopyFileA(src.c_str(), coverTarget().c_str(), FALSE)) { loadCover(); say(cover ? "Cover video set." : "That video could not be played.", !cover); }
            else say("Could not copy the video into the documents folder.", true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Designs folder", ImVec2(130, 34))) openInShell(designsDir, false);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---- tiles
    {
        const float tw = (W - 2 * 14.0f) / 3.0f;
        struct Tile { const char* label; std::string value; };
        Tile tiles[3] = {{"Saved projects", std::to_string(projects.size())},
                         {"Bodies in the open design", std::to_string(design().bodies.size())},
                         {"Examples in the library", std::to_string(exampleGallery().size())}};
        for (int k = 0; k < 3; ++k) {
            if (k) ImGui::SameLine(0, 14);
            ImGui::BeginChild(tiles[k].label, ImVec2(tw, 78), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushFont(ImGui::GetFont(), 32.0f);
            ImGui::TextUnformatted(tiles[k].value.c_str());
            ImGui::PopFont();
            ImGui::TextDisabled("%s", tiles[k].label);
            ImGui::EndChild();
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();

    // ---- the design being worked on right now
    ImGui::TextUnformatted("Working on now");
    {
        const Model& m = design();
        ImGui::BeginChild("##current", ImVec2(-1, 108), true, ImGuiWindowFlags_NoScrollbar);
        ImVec2 q0 = ImGui::GetCursorScreenPos();
        drawPreview(ImGui::GetWindowDrawList(), m, q0, ImVec2(q0.x + 150, q0.y + 86), pal.ink, pal.gridAxis);
        ImGui::SetCursorScreenPos(ImVec2(q0.x + 164, q0.y));
        ImGui::BeginGroup();
        ImGui::Text("%s%s", m.name.c_str(), dirty ? "   (unsaved changes)" : "");
        ImGui::TextDisabled("%d bodies, %d connections, run %.0f s    state: %s", (int)m.bodies.size(), (int)m.links.size(), m.run.duration, stateName());
        ImGui::TextDisabled("%s", currentPath.empty() ? "not saved to a file yet" : currentPath.c_str());
        if (ImGui::Button("Continue", ImVec2(110, 28))) focusSim = true;
        ImGui::SameLine();
        if (ImGui::Button("Save", ImVec2(80, 28))) saveDesign(false);
        ImGui::EndGroup();
        ImGui::EndChild();
    }
    ImGui::Spacing();

    // ---- every saved project
    ImGui::Text("Your projects  (%d)", (int)projects.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) scanProjects();
    if (projects.empty()) ImGui::TextDisabled("Nothing saved yet. Design something, then File > Save (Ctrl+S): it appears here.");
    const float cw = 270, ch = 172, gap = 12;
    const int perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (cw + gap)));
    int col = 0;
    for (size_t i = 0; i < projects.size(); ++i) {
        const Project& pr = projects[i];
        if (col > 0) ImGui::SameLine(0, gap);
        ImGui::PushID((int)i);
        ImGui::BeginChild("##card", ImVec2(cw, ch), true, ImGuiWindowFlags_NoScrollbar);
        ImVec2 q0 = ImGui::GetCursorScreenPos();
        drawPreview(ImGui::GetWindowDrawList(), pr.model, q0, ImVec2(q0.x + cw - 18, q0.y + 84), pal.ink, pal.gridAxis);
        ImGui::Dummy(ImVec2(cw - 18, 88));
        ImGui::TextUnformatted(pr.name.c_str());
        ImGui::TextDisabled("%d bodies, %d links   %s", (int)pr.model.bodies.size(), (int)pr.model.links.size(), pr.when.c_str());
        bool current = !currentPath.empty() && _stricmp(currentPath.c_str(), pr.path.c_str()) == 0;
        if (current) { ImGui::SameLine(cw - 96); ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.5f, 1), "open now"); }
        bool hot = ImGui::IsWindowHovered();
        ImGui::EndChild();
        if (hot) dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(60, 130, 255, 255), 6.0f, 2.0f);
        if (hot && ImGui::IsMouseClicked(0)) { openFile(pr.path); focusSim = true; }
        ImGui::PopID();
        if (++col >= perRow) col = 0;
    }
    ImGui::End();
}

}  // namespace vlab
