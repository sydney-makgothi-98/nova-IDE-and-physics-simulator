// app_core.cpp -- the application's brain: frame loop, undo, files, running, exporting, recording, analysis.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <fstream>
#include "app.hpp"
#include "encoder.hpp"
#include "imgui_internal.h"
#include "report.hpp"

namespace vlab {

static std::string stamp() {
    std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%Y%m%d_%H%M%S", std::localtime(&t));
    return b;
}
static std::string safeName(std::string s) {
    for (char& c : s) if (std::string("\\/:*?\"<>|").find(c) != std::string::npos || c == ' ') c = '_';
    return s.empty() ? "design" : s;
}

App::App() = default;
App::~App() = default;

// ====================================================================== lifecycle
void App::init(Platform* p) {
    platform = p;
    docsDir = documentsFolder() + "\\Vibration Lab Suite";
    designsDir = docsDir + "\\Designs";
    exportsDir = docsDir + "\\Exports";
    recordingsDir = docsDir + "\\Recordings";
    ensureDirectory(docsDir);
    ensureDirectory(designsDir);
    ensureDirectory(exportsDir);
    ensureDirectory(recordingsDir);
    Model m;
    std::string seen = docsDir + "\\welcome_seen.txt";
    bool first = !std::ifstream(seen).good();
    std::string err;
    if (!first && loadModelFile(docsDir + "\\last_session.vlab", m, &err)) ses.setDesign(m);
    else ses.setDesign(exampleGallery()[0].model);
    showWelcome = first;
    if (first) std::ofstream(seen) << "shown\n";
    frameSnapshot = design();
    frfReq.f0 = 0.1;
    frfReq.f1 = 3.0;
    statusLine = "Design a system, set how long to run it, press Run.";
}

void App::shutdown() {
    stopRecording();
    frfCancel = true;
    if (frfHandle) { WaitForSingleObject((HANDLE)frfHandle, 5000); CloseHandle((HANDLE)frfHandle); frfHandle = nullptr; }
    saveModelFile(docsDir + "\\last_session.vlab", design());
}

void App::say(const std::string& s, bool error) {
    toast.text = s;
    toast.error = error;
    toast.until = now + (error ? 7.0 : 4.0);
}

const char* App::stateName() const {
    switch (state) {
        case RunState::Design: return "DESIGN";
        case RunState::Running: return "RUNNING";
        case RunState::Paused: return "PAUSED";
        case RunState::Computing: return "COMPUTING";
        default: return "DONE";
    }
}

// ====================================================================== undo and edit tracking
void App::pushUndo() {
    if (undoPushed) return;
    undoStack.push_back(frameSnapshot);
    if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
    redoStack.clear();
    undoPushed = true;
}

void App::edited() {
    dirty = true;
    if (haveResults && !ses.hasWorld()) resultsStale = true;
    if (ses.hasWorld()) ses.syncLiveEdits();       // property edits take effect immediately in a running world
}

void App::undo() {
    if (!canEditStructure() || undoStack.empty()) return;
    redoStack.push_back(design());
    design() = undoStack.back();
    design().updateMass();
    undoStack.pop_back();
    selBody = selLink = -1;
    edited();
}
void App::redo() {
    if (!canEditStructure() || redoStack.empty()) return;
    undoStack.push_back(design());
    design() = redoStack.back();
    design().updateMass();
    redoStack.pop_back();
    selBody = selLink = -1;
    edited();
}

bool App::canEditStructure() {
    if (state == RunState::Design) return true;
    if (state == RunState::Done) { stopRun(); return true; }       // the run is over: editing simply returns to design mode
    say("Press Stop first: the layout can only be changed in design mode. (Sliders and fields still work while it runs.)", false);
    return false;
}

// ====================================================================== files
void App::newDesign() {
    if (!canEditStructure()) return;
    Model m;
    m.name = "Untitled";
    m.env.setRealistic();
    ses.setDesign(m);
    undoStack.clear();
    redoStack.clear();
    selBody = selLink = -1;
    currentPath.clear();
    haveResults = false;
    dirty = false;
    haveSpectrum = haveModal = haveFrf = false;
    state = RunState::Design;
    statusLine = "Empty canvas: pick a shape in the Library (left), then click the canvas.";
}

void App::openFile(const std::string& path) {
    Model m;
    std::string err;
    if (!loadModelFile(path, m, &err)) { say("Could not open: " + err, true); return; }
    stopRun();
    ses.setDesign(m);
    undoStack.clear();
    redoStack.clear();
    selBody = selLink = -1;
    currentPath = path;
    needFit = true;
    haveResults = false;
    dirty = false;
    haveSpectrum = haveModal = haveFrf = false;
    say("Opened " + path);
}

void App::loadDesignDialog() {
    if (!canEditStructure()) return;
    std::string p = openFileDialog("Vibration Lab designs (*.vlab)\0*.vlab\0All files\0*.*\0\0", designsDir.c_str());
    if (!p.empty()) openFile(p);
}

bool App::saveDesign(bool saveAs) {
    std::string p = currentPath;
    if (p.empty() || saveAs) {
        p = saveFileDialog("Vibration Lab designs (*.vlab)\0*.vlab\0\0", "vlab", designsDir, safeName(design().name) + ".vlab");
        if (p.empty()) return false;
    }
    if (!saveModelFile(p, design())) { say("Could not save the file.", true); return false; }
    currentPath = p;
    dirty = false;
    say("Saved " + p);
    return true;
}

void App::loadExample(int index) {
    if (!canEditStructure()) return;
    if (index < 0 || index >= (int)exampleGallery().size()) return;
    ses.setDesign(exampleGallery()[index].model);
    undoStack.clear();
    redoStack.clear();
    selBody = selLink = -1;
    currentPath.clear();
    haveResults = false;
    dirty = false;
    haveSpectrum = haveModal = haveFrf = false;
    needFit = true;
    statusLine = exampleGallery()[index].summary;
    state = RunState::Design;
}

// ====================================================================== running
static void defaultPlotSelection(App& a) {
    const Telemetry& t = a.ses.telemetry();
    a.plotSel.assign(t.channels.size(), 0);
    int shown = 0;
    for (const Body& b : a.runModel.bodies) {
        if (!b.isDynamic()) continue;
        if (!b.lockX) { int c = t.find(b.name + " dx"); if (c >= 0 && shown < 6) { a.plotSel[c] = 1; ++shown; } }
        if (!b.lockY) { int c = t.find(b.name + " dy"); if (c >= 0 && shown < 6) { a.plotSel[c] = 1; ++shown; } }
    }
    if (shown == 0 && !t.channels.empty()) a.plotSel[0] = 1;
}

void App::startRun(bool realTime) {
    if (state == RunState::Paused && realTime) { state = RunState::Running; return; }
    if (state == RunState::Running || state == RunState::Computing) return;
    Model& m = design();
    if (m.dynamicCount() == 0) { say("Add at least one moving body (a ball, block or polygon) first.", true); return; }
    if (m.run.duration <= 0) { say("Set a run time greater than zero.", true); return; }
    runModel = m;
    ses.start();
    haveResults = true;
    resultsStale = false;
    haveSpectrum = haveModal = false;
    defaultPlotSelection(*this);
    specChannel = 0;
    phaseBody = 0;
    state = realTime ? RunState::Running : RunState::Computing;
    statusLine = realTime ? "Running in real time. Drag a body with the mouse to disturb it." : "Computing the whole run...";
    plotAutoFit = true;
}

void App::pause() {
    if (state == RunState::Running) state = RunState::Paused;
    else if (state == RunState::Paused) state = RunState::Running;
}

void App::stopRun() {
    if (ses.hasWorld()) ses.world()->clearMouse();
    ses.stop();
    dragBody = -1;
    modeShown = -1;
    state = RunState::Design;
    if (haveResults) statusLine = "Stopped. The data from the last run is still available to plot and export.";
}

void App::stepOnce() {
    if (state == RunState::Design) {
        startRun(true);
        if (state == RunState::Running) state = RunState::Paused;
    }
    if (state == RunState::Paused) { ses.advance(1.0 / 60.0); finishRunIfDone(); }
}

void App::finishRunIfDone() {
    if (!ses.hasWorld()) return;
    if (!ses.world()->healthy()) {
        state = RunState::Paused;
        say("The numbers blew up (a very light body on a very stiff spring, or a huge force). Lower the stiffness, raise the mass or shorten the step.", true);
        return;
    }
    if ((state == RunState::Running || state == RunState::Computing) && ses.finished()) {
        state = RunState::Done;
        char b[200];
        std::snprintf(b, sizeof b, "Run complete: %.1f s simulated, %zu data rows. Export to Excel from the Run panel.", ses.simTime(), ses.telemetry().rows());
        statusLine = b;
        say(b);
    }
}

void App::advanceRun(float dt) {
    if (!ses.hasWorld()) return;
    if (state == RunState::Running) {
        if (recording) {
            recAccum += dt;
            const double tick = 1.0 / 60.0;
            if (recAccum >= tick) {                                    // while recording, every video frame is exactly 1/60 s of physics
                ses.advance(tick * timeScale);
                recAccum -= tick;
                if (recAccum > tick) recAccum = 0;
                wantCapture = true;
            }
        } else {
            ses.advance(std::min<double>(dt * timeScale, 0.1));
        }
        finishRunIfDone();
    } else if (state == RunState::Computing) {
        auto t0 = std::chrono::steady_clock::now();
        while (!ses.finished() && ses.world()->healthy()) {
            ses.advance(0.05);
            if (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > 0.02) break;
        }
        finishRunIfDone();
    } else if (state == RunState::Paused || state == RunState::Design) {
        if (recording) {
            recAccum += dt;
            if (recAccum >= 1.0 / 60.0) { recAccum = 0; wantCapture = true; }
        }
    }
}

// ====================================================================== exporting
bool App::exportExcelTo(const std::string& p) {
    ExportExtras x;
    if (haveSpectrum) { x.spectrum = &spectrum; x.spectrumChannel = specName; }
    if (haveModal) x.modal = &modal;
    if (haveFrf) { x.frf = &frf; x.frfNote = frfNote; }
    std::string err;
    if (!exportExcel(p, runModel, ses.telemetry(), runModel.run.duration, x, &err)) { say("Export failed: " + err, true); return false; }
    lastExportPath = p;
    say("Excel workbook saved: " + p);
    return true;
}

bool App::exportCsvTo(const std::string& p) {
    std::string err;
    if (!exportCsv(p, ses.telemetry(), &err)) { say("Export failed: " + err, true); return false; }
    lastExportPath = p;
    say("CSV saved: " + p);
    return true;
}

void App::exportExcelDialog() {
    if (!haveResults || ses.telemetry().rows() == 0) { say("Nothing to export yet. Run the model first.", true); return; }
    std::string def = safeName(runModel.name) + "_" + stamp() + ".xlsx";
    std::string p = saveFileDialog("Excel workbook (*.xlsx)\0*.xlsx\0\0", "xlsx", exportsDir, def);
    if (!p.empty()) exportExcelTo(p);
}

void App::exportCsvDialog() {
    if (!haveResults || ses.telemetry().rows() == 0) { say("Nothing to export yet. Run the model first.", true); return; }
    std::string def = safeName(runModel.name) + "_" + stamp() + ".csv";
    std::string p = saveFileDialog("CSV file (*.csv)\0*.csv\0\0", "csv", exportsDir, def);
    if (!p.empty()) exportCsvTo(p);
}

// ====================================================================== recording
void App::startRecording() {
    if (recording) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (recWholeWindow) { recX = 0; recY = 0; recW = (int)vp->Size.x; recH = (int)vp->Size.y; }
    else { recX = (int)canvasMin.x; recY = (int)canvasMin.y; recW = (int)(canvasMax.x - canvasMin.x); recH = (int)(canvasMax.y - canvasMin.y); }
    recX -= recX & 1;
    recY -= recY & 1;
    recW &= ~1;
    recH &= ~1;
    if (recW < 64 || recH < 64) { say("The area to record is too small.", true); return; }
    recPath = recordingsDir + "\\VibrationLab_" + stamp() + ".mp4";
    encoder = std::make_unique<VideoEncoder>();
    std::string err;
    if (!encoder->open(recPath, recW, recH, 60, err)) { say("Cannot record: " + err, true); encoder.reset(); return; }
    recording = true;
    recFrames = 0;
    recAccum = 0;
    say("Recording... press Record again (or F9) to finish. Video frames are exactly 1/60 s of physics.");
}

void App::stopRecording() {
    if (!recording) return;
    recording = false;
    bool ok = encoder && encoder->close();
    encoder.reset();
    if (ok) { lastExportPath = recPath; say("Video saved: " + recPath); }
    else say("The video could not be finished.", true);
}

void App::endOfFrameCapture() {
    autotestCapture();
    if (!recording || !wantCapture || !encoder || !platform) { wantCapture = false; return; }
    wantCapture = false;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (recX + recW > (int)vp->Size.x || recY + recH > (int)vp->Size.y) { stopRecording(); say("The window became smaller than the video: recording stopped.", true); return; }
    const void* px = nullptr;
    int rowBytes = 0;
    if (!platform->grabFrame(recX, recY, recW, recH, &px, &rowBytes)) { stopRecording(); say("Could not read the screen: recording stopped.", true); return; }
    bool ok = encoder->addFrame(px, rowBytes);
    platform->releaseFrame();
    if (!ok) { stopRecording(); say("The encoder reported an error: recording stopped.", true); return; }
    ++recFrames;
}

// ====================================================================== analysis
void App::computeSpectrum() {
    const Telemetry& t = ses.telemetry();
    if (t.rows() < 16 || specChannel < 0 || specChannel >= (int)t.channels.size()) { say("Run the model for a few seconds first.", true); return; }
    spectrum = vlab::computeSpectrum(t.t, t.col[specChannel]);
    specName = t.channels[specChannel].name;
    haveSpectrum = !spectrum.freq.empty();
}

void App::computeModal() {
    modal = modalAnalysis(design());
    haveModal = true;
    modeShown = modal.modes.empty() ? -1 : 0;
}

static DWORD WINAPI frfThreadProc(LPVOID p) {
    App* a = (App*)p;
    a->frfResult = frequencyResponse(a->frfJobModel, a->frfJobReq, [a](float f) { a->frfProgress = f; }, [a]() { return a->frfCancel.load(); });
    a->frfDone = true;
    return 0;
}

void App::startFrf() {
    if (frfBusy) return;
    if (frfHandle) { WaitForSingleObject((HANDLE)frfHandle, INFINITE); CloseHandle((HANDLE)frfHandle); frfHandle = nullptr; }
    frfJobModel = design();                    // the worker gets its own copy: the user can keep editing
    frfJobReq = frfReq;
    frfBusy = true;
    frfDone = false;
    frfCancel = false;
    frfProgress = 0;
    frfHandle = CreateThread(nullptr, 0, frfThreadProc, this, 0, nullptr);
    if (!frfHandle) { frfBusy = false; say("Could not start the frequency sweep.", true); }
}

// ====================================================================== shortcuts
void App::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    bool typing = io.WantTextInput;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) redo(); else undo(); }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) saveDesign(io.KeyShift);
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) loadDesignDialog();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) newDesign();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E, false)) exportExcelDialog();
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false) && !io.WantTextInput) needFit = true;
    if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) { if (recording) stopRecording(); else startRecording(); }
    if (typing) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        if (state == RunState::Design || state == RunState::Done) { if (state == RunState::Done) stopRun(); startRun(true); }
        else pause();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (linkFirst >= 0) linkFirst = -1;
        else if (tool != Tool::Select) tool = Tool::Select;
        else if (state != RunState::Design) stopRun();
        else selBody = selLink = -1;
    }
    if (!io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) tool = Tool::Select;
        if (ImGui::IsKeyPressed(ImGuiKey_C, false)) tool = Tool::Circle;
        if (ImGui::IsKeyPressed(ImGuiKey_B, false)) tool = Tool::Box;
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) tool = Tool::Polygon;
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) tool = Tool::Wedge;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) tool = Tool::Floor;
        if (ImGui::IsKeyPressed(ImGuiKey_A, false)) tool = Tool::Anchor;
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) tool = Tool::Spring;
        if (ImGui::IsKeyPressed(ImGuiKey_D, false)) tool = Tool::Damper;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) tool = Tool::Rod;
        if (ImGui::IsKeyPressed(ImGuiKey_J, false)) tool = Tool::Pin;
        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) tool = Tool::Beam;
        if (ImGui::IsKeyPressed(ImGuiKey_1, false)) tool = Tool::SupportPin;
        if (ImGui::IsKeyPressed(ImGuiKey_2, false)) tool = Tool::SupportFixed;
        if (ImGui::IsKeyPressed(ImGuiKey_3, false)) tool = Tool::SupportRoller;
        if (tool != Tool::Select) {                                           // turn the object being placed
            if (ImGui::IsKeyPressed(ImGuiKey_Q, true)) nextAngle = std::fmod(nextAngle - 15.0f + 360.0f, 360.0f);
            if (ImGui::IsKeyPressed(ImGuiKey_E, true)) nextAngle = std::fmod(nextAngle + 15.0f, 360.0f);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
        if (canEditStructure()) {
            if (selBody >= 0) { pushUndo(); design().removeBody(selBody); selBody = -1; selLink = -1; edited(); }
            else if (selLink >= 0) { pushUndo(); design().removeLink(selLink); selLink = -1; edited(); }
        }
    }
}

// ====================================================================== automated UI test
static bool writeBmp(const std::string& path, const unsigned char* px, int w, int h, int rowBytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    unsigned char hdr[54] = {'B', 'M'};
    unsigned size = 54 + (unsigned)(w * h * 4);
    std::memcpy(hdr + 2, &size, 4);
    unsigned off = 54, dib = 40;
    std::memcpy(hdr + 10, &off, 4);
    std::memcpy(hdr + 14, &dib, 4);
    int nh = -h;
    std::memcpy(hdr + 18, &w, 4);
    std::memcpy(hdr + 22, &nh, 4);
    unsigned short planes = 1, bpp = 32;
    std::memcpy(hdr + 26, &planes, 2);
    std::memcpy(hdr + 28, &bpp, 2);
    std::fwrite(hdr, 1, 54, f);
    for (int y = 0; y < h; ++y) std::fwrite(px + (size_t)y * rowBytes, 1, (size_t)w * 4, f);
    std::fclose(f);
    return true;
}

void App::autotestStep() {
    if (clickStage > 0) {                                           // finish an injected click: move, press, release
        ImGuiIO& io = ImGui::GetIO();
        if (clickStage == 3) { io.AddMousePosEvent(clickPos.x, clickPos.y); clickStage = 2; return; }
        if (clickStage == 2) { io.AddMouseButtonEvent(0, true); clickStage = 1; return; }
        io.AddMouseButtonEvent(0, false);
        clickStage = 0;
        autotestWait = 2;
        return;
    }
    if (autotestWait > 0) { --autotestWait; return; }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    auto click = [&](ImVec2 p, int) { clickPos = p; clickStage = 3; };
    auto shot = [&](const char* name) { pendingShot = name; };
    (void)vp;
    switch (autotestPhase) {
        case 0: showWelcome = false; loadExample(6); autotestPhase = 1; autotestWait = 5; break;            // bouncing ball
        case 1: design().run.duration = 8; startRun(false); autotestPhase = 2; break;
        case 2: if (state == RunState::Done) { exportExcelTo(autotestDir + "\\bounce.xlsx"); exportCsvTo(autotestDir + "\\bounce.csv"); computeSpectrum(); plotTab = 0; autotestPhase = 3; autotestWait = 6; } break;
        case 3: shot("shot_bounce.bmp"); autotestPhase = 4; break;
        case 4: stopRun(); loadExample(7); autotestPhase = 5; autotestWait = 5; break;                         // friction on a ramp
        case 5: startRun(false); autotestPhase = 6; break;
        case 6: if (state == RunState::Done) { exportExcelTo(autotestDir + "\\incline.xlsx"); autotestPhase = 7; autotestWait = 6; } break;
        case 7: shot("shot_incline.bmp"); autotestPhase = 8; break;
        case 8: stopRun(); loadExample(5); selBody = 2; autotestPhase = 9; autotestWait = 5; break;           // quarter car, live run
        case 9: startRun(true); autotestPhase = 10; autotestWait = 120; break;
        case 10: shot("shot_live.bmp"); autotestPhase = 11; break;
        case 11: startRecording(); autotestPhase = 12; autotestWait = 150; break;                               // record 2.5 s of the live run
        case 12: stopRecording(); autotestPhase = 13; autotestWait = 3; break;
        case 13: stopRun(); newDesign(); autotestPhase = 14; autotestWait = 4; break;                            // now: design by CLICKING
        case 14: tool = Tool::Floor; floorLength = 10; nextAngle = 0; floorMaterial = 5; click(toScreen(Vec2(0, 2.0)), 0); autotestPhase = 15; break;
        case 15: tool = Tool::Circle; nextRadius = 0.3f; nextMaterial = 3; nextMass = 0.5f; click(toScreen(Vec2(-2.0, -1.5)), 0); autotestPhase = 16; break;
        case 16: tool = Tool::Box; nextW = 0.8f; nextH = 0.5f; nextMaterial = 4; nextMass = 1.0f; click(toScreen(Vec2(2.0, 1.6)), 0); autotestPhase = 17; break;
        case 17: tool = Tool::Anchor; click(toScreen(Vec2(3.0, -2.0)), 0); autotestPhase = 18; break;
        case 18: tool = Tool::Spring; click(toScreen(Vec2(3.0, -2.0)), 0); autotestPhase = 19; break;          // spring: anchor -> block
        case 19: click(toScreen(Vec2(2.0, 1.6)), 0); autotestPhase = 20; break;
        case 20: tool = Tool::Select; design().run.duration = 6; design().env.setRealistic(); autotestPhase = 21; autotestWait = 2; break;
        case 21: startRun(false); autotestPhase = 22; break;
        case 22: if (state == RunState::Done) { exportExcelTo(autotestDir + "\\clicked.xlsx"); autotestPhase = 23; autotestWait = 6; } break;
        case 23: shot("shot_clicked.bmp"); autotestPhase = 24; break;
        case 24: stopRun(); loadExample(0); autotestPhase = 25; autotestWait = 3; break;                          // long run: 5 minutes at 20 rows/s
        case 25: design().run.duration = 300; design().run.sampleHz = 20; startRun(false); autotestPhase = 26; break;
        case 26: if (state == RunState::Done) { exportExcelTo(autotestDir + "\\long_300s.xlsx"); autotestPhase = 27; } break;
        case 27: stopRun(); loadExample(2); autotestPhase = 28; autotestWait = 4; break;                          // tuned mass damper: analysis tabs
        case 28: design().run.duration = 40; design().run.sampleHz = 50; startRun(false); autotestPhase = 29; break;
        case 29: if (state == RunState::Done) {
                     specChannel = std::max(0, ses.telemetry().find("Main mass dx")); computeSpectrum(); computeModal(); modeShown = -1;
                     plotSel.assign(ses.telemetry().channels.size(), 0);
                     for (const char* n : {"Main mass dx", "Absorber dx"}) { int ch = ses.telemetry().find(n); if (ch >= 0) plotSel[ch] = 1; }
                     forceTab = 0; autotestPhase = 30; autotestWait = 4; } break;
        case 30: shot("shot_tmd_plots.bmp"); forceTab = 3; autotestPhase = 31; autotestWait = 4; break;
        case 31: shot("shot_spectrum.bmp"); forceTab = 5; autotestPhase = 32; autotestWait = 4; break;
        case 32: shot("shot_modal.bmp"); forceTab = 1; autotestPhase = 33; autotestWait = 4; break;
        case 33: shot("shot_energy.bmp"); stopRun(); frfReq.driveBody = 1; frfReq.responseBody = 1; frfReq.f0 = 0.3; frfReq.f1 = 1.6; frfReq.points = 30; frfReq.settleSeconds = 8; frfReq.driveAngleDeg = 0; frfReq.forceAmp = 5;
                 startFrf(); forceTab = 6; autotestPhase = 34; autotestWait = 10; break;
        case 34: if (!frfBusy && haveFrf) { autotestPhase = 35; autotestWait = 6; } break;
        case 35: shot("shot_frf.bmp"); autotestPhase = 36; break;
        case 36: stopRun(); loadExample(1); autotestPhase = 37; autotestWait = 6; break;
        case 37: computeModal(); modeShown = 0; modeClock = 0.35; forceTab = 5; autotestPhase = 38; autotestWait = 6; break;
        case 38: shot("shot_design.bmp"); autotestPhase = 39; break;
        case 39: stopRun(); newDesign(); autotestPhase = 40; autotestWait = 4; break;                             // floors, beams, supports by clicking
        case 40: tool = Tool::Floor; floorLength = 8; nextAngle = 0; click(toScreen(Vec2(0, 2.5)), 0); autotestPhase = 41; break;
        case 41: tool = Tool::Floor; floorLength = 5; nextAngle = 20; click(toScreen(Vec2(-5, 1.0)), 0); autotestPhase = 42; break;
        case 42: tool = Tool::Floor; floorLength = 4; nextAngle = -30; click(toScreen(Vec2(5, 1.0)), 0); autotestPhase = 43; break;
        case 43: tool = Tool::Select; click(toScreen(Vec2(0, 2.5)), 0); autotestPhase = 44; break;               // can the first floor still be selected?
        case 44: { FILE* f = std::fopen((autotestDir + "\\newfeatures.txt").c_str(), "w");
                   if (f) { std::fprintf(f, "floors placed: %d (expect 3)\nfirst floor selectable after 3 floors: %s\n", (int)design().bodies.size(), selBody == 0 ? "YES" : "NO"); std::fclose(f); }
                   tool = Tool::Beam; beamLength = 3; beamThick = 0.15f; nextMaterial = 4; nextAngle = 0; click(toScreen(Vec2(0, -1.0)), 0); autotestPhase = 45; } break;
        case 45: tool = Tool::SupportPin; nextAngle = 0; click(toScreen(Vec2(-1.5, -1.0)), 0); autotestPhase = 46; break;
        case 46: { FILE* f = std::fopen((autotestDir + "\\newfeatures.txt").c_str(), "a");
                   if (f) { std::fprintf(f, "bodies %d (floors+beam+support anchor), links %d (expect 1)\n", (int)design().bodies.size(), (int)design().links.size()); std::fclose(f); }
                   tool = Tool::Select; selBody = selLink = -1; design().run.duration = 3; autotestPhase = 47; autotestWait = 2; } break;
        case 47: startRun(true); autotestPhase = 48; autotestWait = 70; break;
        case 48: shot("shot_beam_swing.bmp"); autotestPhase = 49; break;
        case 49: stopRun(); loadExample(16); autotestPhase = 50; autotestWait = 4; break;                         // gantry crane
        case 50: startRun(true); autotestPhase = 51; autotestWait = 200; break;
        case 51: shot("shot_gantry.bmp"); autotestPhase = 52; break;
        case 52: stopRun(); loadExample(17); autotestPhase = 53; autotestWait = 4; break;                         // robotic arm
        case 53: startRun(true); autotestPhase = 54; autotestWait = 160; break;
        case 54: shot("shot_arm.bmp"); autotestPhase = 55; break;
        case 55: stopRun(); loadExample(13); autotestPhase = 56; autotestWait = 4; break;                         // simply supported beam: reactions
        case 56: startRun(true); autotestPhase = 57; autotestWait = 120; break;
        case 57: selLink = 0; selBody = -1; autotestPhase = 58; autotestWait = 3; break;
        case 58: shot("shot_support.bmp"); autotestPhase = 59; break;
        case 59: stopRun(); tool = Tool::Box; nextAngle = 30; nextW = 1.2f; nextH = 0.5f; autotestPhase = 60; autotestWait = 2; break;    // the rotating ghost
        case 60: ImGui::GetIO().AddMousePosEvent(toScreen(Vec2(0, -2.0)).x, toScreen(Vec2(0, -2.0)).y); autotestPhase = 61; autotestWait = 4; break;
        case 61: shot("shot_ghost.bmp"); autotestPhase = 62; break;
        case 62: tool = Tool::Select; navMenu = 3; autotestPhase = 63; autotestWait = 5; break;                    // the Examples drop-down
        case 63: shot("shot_navbar.bmp"); autotestPhase = 64; break;
        case 64: say("autotest finished"); autotestQuit = true; autotestPhase = 65; break;
        default: break;
    }
}

void App::autotestCapture() {
    if (pendingShot.empty() || !platform) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const void* px = nullptr;
    int rb = 0;
    if (platform->grabFrame(0, 0, (int)vp->Size.x, (int)vp->Size.y, &px, &rb)) {
        writeBmp(autotestDir + "\\" + pendingShot, (const unsigned char*)px, (int)vp->Size.x, (int)vp->Size.y, rb);
        platform->releaseFrame();
    }
    pendingShot.clear();
}

// ====================================================================== the frame
void App::frame(float dt) {
    now += dt;
    frameSnapshot = design();
    if (frfDone.exchange(false)) {
        frf = frfResult;
        haveFrf = !frf.empty();
        frfBusy = false;
        char b[160];
        std::snprintf(b, sizeof b, "Drive: %s, response: %s", design().bodies.empty() ? "" : design().bodies[std::min<int>(frfReq.driveBody, (int)design().bodies.size() - 1)].name.c_str(),
                      design().bodies.empty() ? "" : design().bodies[std::min<int>(frfReq.responseBody, (int)design().bodies.size() - 1)].name.c_str());
        frfNote = b;
        say(haveFrf ? "Frequency response finished." : "Frequency response was cancelled.");
    }
    if (!autotestDir.empty()) autotestStep();
    handleShortcuts();
    advanceRun(dt);
    if (platform) {
        std::string title = "Vibration Lab Suite  -  " + design().name + (dirty ? " *" : "") + "  [" + stateName() + "]";
        static std::string lastTitle;
        if (title != lastTitle) { platform->setTitle(title); lastTitle = title; }
    }

    // ---- dock space + layout
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiID dock = ImGui::DockSpaceOverViewport(0, vp);
    if (firstFrame || resetLayout) {
        if (resetLayout || ImGui::DockBuilderGetNode(dock) == nullptr || ImGui::DockBuilderGetNode(dock)->IsLeafNode()) buildDefaultLayout(dock);
        firstFrame = false;
        resetLayout = false;
    }
    drawMenuBar();
    drawCanvas();
    drawLibrary();
    drawRunPanel();
    drawProperties();
    drawEnvironment();
    drawTelemetry();
    if (showWelcome) drawWelcome();
    if (showAbout) drawAbout();
    if (showHelp) drawHelp();
    drawToast();
    if (!ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(0)) undoPushed = false;
}

void App::buildDefaultLayout(unsigned rootId) {
    ImGuiID root = rootId;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(root);
    ImGui::DockBuilderAddNode(root, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(root, vp->Size);
    ImGuiID left, rest, right, centre, bottom, rtop, rbottom;
    ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.16f, &left, &rest);
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.26f, &right, &centre);
    ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.36f, &bottom, &centre);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.62f, &rtop, &rbottom);
    ImGui::DockBuilderDockWindow("Library", left);
    ImGui::DockBuilderDockWindow("Simulation", centre);
    ImGui::DockBuilderDockWindow("Telemetry", bottom);
    ImGui::DockBuilderDockWindow("Run", rtop);
    ImGui::DockBuilderDockWindow("Properties", rbottom);
    ImGui::DockBuilderDockWindow("Environment", rbottom);
    ImGui::DockBuilderFinish(root);
}

}  // namespace vlab
