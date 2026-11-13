// app_core.cpp -- the application's brain: frame loop, undo, files, running, exporting, recording, analysis.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <fstream>
#include "app.hpp"
#include "encoder.hpp"
#include "imgui_internal.h"
#include "report.hpp"
#include "schematic.hpp"

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
    loadDataSettings();                                              // folders and file names chosen in the Data panel last time
    ensureDirectory(exportsDir);
    ensureDirectory(recordingsDir);
    Model m;
    std::string seen = docsDir + "\\welcome_seen.txt";
    bool first = !std::ifstream(seen).good();
    std::string err;
    if (!first && loadModelFile(docsDir + "\\last_session.vlab", m, &err)) ses.setDesign(m);
    else ses.setDesign(exampleGallery()[0].model);
    showWelcome = false;                     // the Dashboard is the front page now
    if (first) std::ofstream(seen) << "shown";
    {   // theme, and a layout reset when the panel arrangement changed (the sidebar now holds Run / Properties / Environment)
        std::ifstream th(docsDir + "\\theme.txt");
        std::string t;
        if (th >> t) darkTheme = t != "light";
        applyTheme();
        std::string lv = docsDir + "\\layout_v6.txt";                  // bump this name whenever the default panel arrangement changes
        if (!std::ifstream(lv).good()) {
            resetLayout = true;
            if (!std::strstr(GetCommandLineA(), "--autotest")) std::ofstream(lv) << "6";     // test runs must not mark a real user's layout as done
        }
    }
    frameSnapshot = design();
    frfReq.f0 = 0.1;
    frfReq.f1 = 3.0;
    statusLine = "Design a system, set how long to run it, press Run.";
}

void App::shutdown() {
    stopRecording();
    cover.reset();
    if (coverTex && platform) { platform->destroyTexture(coverTex); coverTex = nullptr; }
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
    focusSim = true;
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
    focusSim = true;
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
    focusSim = true;
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
    deformPeak = 0;
    diagX = -1;
    diagLive = true;
    plotPicked = false;
    specPicked = false;
    deformRow = -1;
    deformBody = 0;
    state = realTime ? RunState::Running : RunState::Computing;
    focusSim = true;
    statusLine = realTime ? "Running in real time. Drag a body with the mouse to disturb it." : "Computing the whole run...";
    plotAutoFit = true;
    if (realTime && autoRecord && !recording) { ensureDirectory(recordingsDir); startRecording(); }
}

void App::pause() {
    if (state == RunState::Running) state = RunState::Paused;
    else if (state == RunState::Paused) state = RunState::Running;
}

void App::stopRun() {
    if (autoRecord && recording) stopRecording();
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
        if (autoExportCsv) { ensureDirectory(exportsDir); exportCsvTo(exportFileName(".csv")); }          // the Data panel can save the files by itself
        if (autoExportXlsx) { ensureDirectory(exportsDir); exportExcelTo(exportFileName(".xlsx")); }
        if (autoRecord && recording) stopRecording();
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
            // simulate what this frame is owed, but never spend more than ~12 ms of wall time on it: a heavy scene then runs
            // slower than real time instead of freezing the window and falling further behind every frame
            double want = std::min<double>(dt * timeScale, 0.1);
            auto t0 = std::chrono::steady_clock::now();
            slowRun = false;
            while (want > 1e-9 && !ses.finished()) {
                double chunk = std::min(want, 0.004);
                ses.advance(chunk);
                want -= chunk;
                if (want > 1e-9 && std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > 0.012) { slowRun = true; break; }
            }
        }
        finishRunIfDone();
    } else if (state == RunState::Computing) {
        auto t0 = std::chrono::steady_clock::now();
        while (!ses.finished() && ses.world()->healthy()) {
            ses.advance(0.05);
            if (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > 0.02) break;
        }
        finishRunIfDone();
    } else if (state == RunState::Paused || state == RunState::Design || state == RunState::Done) {
        if (recording) {
            recAccum += dt;
            if (recAccum >= 1.0 / 60.0) { recAccum = 0; wantCapture = true; }
        }
    }
}

// ====================================================================== exporting
// ====================================================================== data extraction settings
// What the Data panel remembers between sessions: where files go, the name, which formats to save by themselves.
// The design as a labelled figure (SVG): shapes, springs, dampers, supports and loads, each named with a leader line.
void App::exportSchematic() {
    ensureDirectory(exportsDir);
    SchematicOptions o;
    o.names = schemNames;
    o.values = schemValues;
    o.arrowheads = schemArrows;
    o.loads = schemLoads;
    o.title = design().name;
    std::string base = safeName(exportBase.empty() ? design().name : exportBase);
    if (exportStamp) base += "_" + stamp();
    const std::string p = exportsDir + "\\" + base + "_schematic.svg";
    std::string err;
    if (!exportSchematicSvg(p, design(), o, &err)) { say("Could not save the schematic: " + err, true); return; }
    lastExportPath = p;
    say("Schematic saved: " + p);
    openInShell(p, false);
}

void App::saveDataSettings() {
    std::ofstream f(docsDir + "\\data_settings.txt");
    f << "exports=" << exportsDir << "\n"
      << "recordings=" << recordingsDir << "\n"
      << "base=" << exportBase << "\n"
      << "stamp=" << (exportStamp ? 1 : 0) << "\n"
      << "all=" << (exportAll ? 1 : 0) << "\n"
      << "autocsv=" << (autoExportCsv ? 1 : 0) << "\n"
      << "autoxlsx=" << (autoExportXlsx ? 1 : 0) << "\n"
      << "autorec=" << (autoRecord ? 1 : 0) << "\n";
}

void App::loadDataSettings() {
    std::ifstream f(docsDir + "\\data_settings.txt");
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (!v.empty() && v.back() == '\r') v.pop_back();
        if (k == "exports" && !v.empty()) exportsDir = v;
        else if (k == "recordings" && !v.empty()) recordingsDir = v;
        else if (k == "base") exportBase = v;
        else if (k == "stamp") exportStamp = v != "0";
        else if (k == "all") exportAll = v != "0";
        else if (k == "autocsv") autoExportCsv = v != "0";
        else if (k == "autoxlsx") autoExportXlsx = v != "0";
        else if (k == "autorec") autoRecord = v != "0";
    }
}

std::string App::exportBaseName() const {
    std::string base = safeName(exportBase.empty() ? runModel.name : exportBase);
    if (exportStamp) base += "_" + stamp();
    return base;
}

std::string App::exportFileName(const char* ext) const { return exportsDir + "\\" + exportBaseName() + ext; }

// The telemetry as it will be exported: every channel, or only the ticked ones (for Excel the Energy channels always come along:
// the summary sheet is built from them).
const Telemetry* App::exportSource(Telemetry& tmp, bool excel) const {
    const Telemetry& t = ses.telemetry();
    if (exportAll || exportSel.size() != t.channels.size()) return &t;
    tmp.channels.clear();
    tmp.col.clear();
    tmp.deform.clear();
    tmp.t = t.t;
    for (size_t c = 0; c < t.channels.size(); ++c)
        if (exportSel[c] || (excel && t.channels[c].group == "Energy")) { tmp.channels.push_back(t.channels[c]); tmp.col.push_back(t.col[c]); }
    return &tmp;
}

bool App::exportExcelTo(const std::string& p) {
    ExportExtras x;
    if (haveSpectrum) { x.spectrum = &spectrum; x.spectrumChannel = specName; }
    if (haveModal) x.modal = &modal;
    if (haveFrf) { x.frf = &frf; x.frfNote = frfNote; }
    std::string err;
    Telemetry tmp;
    const Telemetry* src = exportSource(tmp, true);
    if (src->channels.empty()) { say("No channels are ticked: open the Data panel and tick what to extract.", true); return false; }
    if (!exportExcel(p, runModel, *src, runModel.run.duration, x, &err)) { say("Export failed: " + err, true); return false; }
    lastExportPath = p;
    say("Excel workbook saved: " + p);
    return true;
}

bool App::exportCsvTo(const std::string& p) {
    std::string err;
    Telemetry tmp;
    const Telemetry* src = exportSource(tmp, false);
    if (src->channels.empty()) { say("No channels are ticked: open the Data panel and tick what to extract.", true); return false; }
    if (!exportCsv(p, *src, &err)) { say("Export failed: " + err, true); return false; }
    lastExportPath = p;
    say("CSV saved: " + p);
    return true;
}

void App::exportExcelDialog() {
    if (!haveResults || ses.telemetry().rows() == 0) { say("Nothing to export yet. Run the model first.", true); return; }
    std::string def = exportBaseName() + ".xlsx";
    std::string p = saveFileDialog("Excel workbook (*.xlsx)\0*.xlsx\0\0", "xlsx", exportsDir, def);
    if (!p.empty()) exportExcelTo(p);
}

void App::exportCsvDialog() {
    if (!haveResults || ses.telemetry().rows() == 0) { say("Nothing to export yet. Run the model first.", true); return; }
    std::string def = exportBaseName() + ".csv";
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
    bool opened = false;
    try { opened = encoder->open(recPath, recW, recH, 60, err); } catch (const std::exception& e) { err = e.what(); } catch (...) { err = "unknown error"; }
    if (!opened) { say("Cannot record: " + err, true); encoder.reset(); DeleteFileA(recPath.c_str()); return; }
    recording = true;
    recFrames = 0;
    recAccum = 0;
    say("Recording... press Record again (or F9) to finish. Video frames are exactly 1/60 s of physics.");
}

void App::stopRecording() {
    if (!recording) return;
    recording = false;
    const bool empty = recFrames == 0;
    bool ok = false;
    try { ok = encoder && encoder->close(); } catch (...) { ok = false; }
    encoder.reset();
    if (ok) { lastExportPath = recPath; say("Video saved: " + recPath); }
    else {
        DeleteFileA(recPath.c_str());                                             // never leave an unplayable 0-byte file behind
        say(empty ? "Nothing was recorded: press Run, then Record video, so there is motion to film." : "The video could not be finished.", true);
    }
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
    bool ok = false;
    try { ok = encoder->addFrame(px, rowBytes); } catch (...) { ok = false; }
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

static int frames = 0, perfTab = 0, perfFrames0 = 0;
static double perfT0 = 0;
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
    ++frames;
    {   // VLAB_AUTOTEST_PHASE=100 jumps straight to the deformation scenario
        static bool once = false;
        if (!once) { once = true; if (const char* e = std::getenv("VLAB_AUTOTEST_PHASE")) autotestPhase = std::atoi(e); }
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
                     plotSel.assign(ses.telemetry().channels.size(), 0); plotPicked = true;
                     for (const char* n : {"Main mass dx", "Absorber dx"}) { int ch = ses.telemetry().find(n); if (ch >= 0) plotSel[ch] = 1; }
                     forceTab = 0; autotestPhase = 30; autotestWait = 4; } break;
        case 30: shot("shot_tmd_plots.bmp"); forceTab = 5; autotestPhase = 31; autotestWait = 4; break;
        case 31: shot("shot_spectrum.bmp"); forceTab = 7; autotestPhase = 32; autotestWait = 4; break;
        case 32: shot("shot_modal.bmp"); forceTab = 1; autotestPhase = 33; autotestWait = 4; break;
        case 33: shot("shot_energy.bmp"); stopRun(); frfReq.driveBody = 1; frfReq.responseBody = 1; frfReq.f0 = 0.3; frfReq.f1 = 1.6; frfReq.points = 30; frfReq.settleSeconds = 8; frfReq.driveAngleDeg = 0; frfReq.forceAmp = 5;
                 startFrf(); forceTab = 8; autotestPhase = 34; autotestWait = 10; break;
        case 34: if (!frfBusy && haveFrf) { autotestPhase = 35; autotestWait = 6; } break;
        case 35: shot("shot_frf.bmp"); autotestPhase = 36; break;
        case 36: stopRun(); loadExample(1); autotestPhase = 37; autotestWait = 6; break;
        case 37: computeModal(); modeShown = 0; modeClock = 0.35; forceTab = 7; autotestPhase = 38; autotestWait = 6; break;
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
        case 100: {                                                                                                  // deformation: 200 kg block dropped on a supported beam
            showWelcome = false;
            newDesign();
            Model& m = design();
            const Material& st = *findMaterial("Steel");
            Body bm = makeBeam(3.0, 0.15, st, Vec2(0, 2.4));
            bm.massFromDensity = false;
            bm.mass = 50;
            int beam = m.addBody(bm);
            Body bk = makeBox(1.0, 0.6, st, Vec2(0, 1.0));
            bk.massFromDensity = false;
            bk.mass = 200;
            m.addBody(bk);
            m.addSupport(LinkKind::Pin, beam, Vec2(-1.5, 2.4), 0);
            m.addSupport(LinkKind::Roller, beam, Vec2(1.5, 2.4), 0);
            m.run.duration = std::getenv("VLAB_AUTOTEST_DUR") ? std::atof(std::getenv("VLAB_AUTOTEST_DUR")) : 4;
            m.updateMass();
            needFit = true;
            autotestPhase = std::getenv("VLAB_AUTOTEST_TIMING") ? 160 : 101;
            autotestWait = 4;
        } break;
        case 101: startRun(false); autotestPhase = 102; break;
        case 102: if (state == RunState::Done) { forceTab = std::getenv("VLAB_AUTOTEST_TAB") ? std::atoi(std::getenv("VLAB_AUTOTEST_TAB")) : 3; autotestPhase = 103; autotestWait = 6; } break;
        case 103: shot("def_tab.bmp"); autotestPhase = 104; break;
        case 104: stopRun(); startRun(true); autotestPhase = 105; autotestWait = 2; break;
        case 105: { static int f0 = 0; static double t0 = 0; f0 = frames; t0 = now; autotestPhase = 106; autotestWait = 120;
                    FILE* f = std::fopen((autotestDir + "/perf.txt").c_str(), "w"); if (f) { std::fprintf(f, "%d %f", f0, t0); std::fclose(f); } } break;
        case 106: { shot("def_live.bmp");
                    FILE* f = std::fopen((autotestDir + "/perf.txt").c_str(), "r"); int f0 = 0; double t0 = 0;
                    if (f) { std::fscanf(f, "%d %lf", &f0, &t0); std::fclose(f); }
                    f = std::fopen((autotestDir + "/perf.txt").c_str(), "w");
                    if (f) { std::fprintf(f, "live run, frames %d, average frame %.2f ms, peak deform factor %.0f ", frames - f0, (now - t0) * 1000.0 / std::max(1, frames - f0), deformFactor); std::fclose(f); }
                    autotestPhase = 107; autotestWait = 3; } break;
        case 110: loadExample(2); design().run.duration = 300; design().run.sampleHz = 100; showWelcome = false; autotestPhase = 111; autotestWait = 3; break;      // perf: 5 minute run, every tab
        case 111: startRun(false); autotestPhase = 112; break;
        case 112: if (state == RunState::Done) { autotestPhase = 113; autotestWait = 5; perfTab = 0; } break;
        case 113: forceTab = perfTab; perfFrames0 = frames; perfT0 = now; autotestPhase = 114; autotestWait = 80; break;
        case 114: { FILE* f = std::fopen((autotestDir + "/perf_tabs.txt").c_str(), "a");
                    if (f) { std::fprintf(f, "tab %d: average frame %.2f ms over %d frames ", perfTab, (now - perfT0) * 1000.0 / std::max(1, frames - perfFrames0), frames - perfFrames0); std::fclose(f); }
                    ++perfTab; autotestPhase = perfTab < 8 ? 113 : 107; autotestWait = 2; } break;
        case 130: showWelcome = false; loadExample(20); forceTab = 3; autotestPhase = 131; autotestWait = 4; break;                   // the five new examples
        case 131: startRun(true); autotestPhase = 132; autotestWait = 430; break;
        case 132: shot("ex_bridge.bmp"); autotestPhase = 133; break;
        case 133: stopRun(); loadExample(18); forceTab = 0; autotestPhase = 134; autotestWait = 4; break;
        case 134: startRun(true); autotestPhase = 135; autotestWait = 200; break;
        case 135: shot("ex_car.bmp"); autotestPhase = 136; break;
        case 136: stopRun(); loadExample(19); startRun(true); autotestPhase = 137; autotestWait = 45; break;
        case 137: shot("ex_rail.bmp"); autotestPhase = 138; break;
        case 138: stopRun(); loadExample(21); startRun(true); autotestPhase = 139; autotestWait = 200; break;
        case 139: shot("ex_engine.bmp"); autotestPhase = 140; break;
        case 140: stopRun(); loadExample(22); startRun(true); autotestPhase = 141; autotestWait = 420; break;
        case 141: shot("ex_domino.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 150: showWelcome = false; loadExample(13); autotestPhase = 151; autotestWait = 4; break;                                // flicker check
        case 151: startRun(false); autotestPhase = 152; break;
        case 152: if (state == RunState::Done) { forceTab = 8; autotestPhase = 153; autotestWait = 8; } break;
        case 153: shot("flick_frf_a.bmp"); autotestPhase = 154; autotestWait = 9; break;
        case 154: shot("flick_frf_b.bmp"); forceTab = 2; autotestPhase = 155; autotestWait = 9; break;
        case 155: shot("flick_path_a.bmp"); autotestPhase = 156; autotestWait = 9; break;
        case 156: shot("flick_path_b.bmp"); forceTab = 5; autotestPhase = 157; autotestWait = 9; break;
        case 157: shot("flick_spec_a.bmp"); autotestPhase = 158; autotestWait = 9; break;
        case 158: shot("flick_spec_b.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 160: startRun(true); perfT0 = now; perfTab = 0; autotestPhase = 161; break;                                                // timing of the sag after contact
        case 161: { static const double at[] = {0.30, 0.55, 0.95, 1.60, 3.20};
                    if (now - perfT0 >= at[perfTab]) { char nm[48]; std::snprintf(nm, sizeof nm, "timing_%d.bmp", perfTab); pendingShot = nm; if (++perfTab >= 5) autotestPhase = 107; } } break;
        case 170: showWelcome = false; designsDir = autotestDir; coverTried = false;                                                   // dashboard and theme
                  for (int k : {0, 5, 18, 19, 20}) saveModelFile(autotestDir + "/" + exampleGallery()[k].title.substr(0, 20) + ".vlab", exampleGallery()[k].model);
                  loadExample(19); projectsScanned = -100; focusDashboard = true; autotestPhase = 171; autotestWait = 150; break;
        case 171: shot("ui_dash_dark.bmp"); autotestPhase = 172; autotestWait = 3; break;
        case 172: setTheme(false); autotestPhase = 173; autotestWait = 8; break;
        case 173: shot("ui_dash_light.bmp"); focusSim = true; autotestPhase = 174; autotestWait = 6; break;
        case 174: startRun(true); autotestPhase = 175; autotestWait = 60; break;
        case 175: shot("ui_sim_light.bmp"); autotestPhase = 176; autotestWait = 4; break;
        case 176: setTheme(true); autotestPhase = 177; autotestWait = 8; break;
        case 177: shot("ui_sim_dark.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 180: showWelcome = false; autotestPhase = 182; autotestWait = 12; break;
        case 182: { newDesign(); Model& m = design(); Body bx = makeBox(1.0, 0.6, *findMaterial("Steel"), Vec2(0, 0)); bx.massFromDensity = false; bx.mass = 20;
                    bx.loadN = 250; bx.loadAngleDeg = 0; m.addBody(bx); m.addBody(makeFloor(8, 0, *findMaterial("Concrete"), Vec2(0, 0.5))); m.env.gravity = 9.81;
                    selBody = 0; needFit = true; autotestPhase = 181; autotestWait = 14; } break;                                           // applied loads
        case 181: shot("ui_load.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 190: showWelcome = false; autotestPhase = 191; autotestWait = 12; break;                                                      // distributed loads
        case 191: { newDesign(); Model& m = design(); Body bm = makeBeam(4.0, 0.2, *findMaterial("Steel"), Vec2(0, 1.0)); bm.massFromDensity = false; bm.mass = 100;
                    bm.dloadType = DL_Equation; bm.dloadExpr = "500*sin(pi*x/L)"; int bi = m.addBody(bm);
                    m.addSupport(LinkKind::Pin, bi, Vec2(-2, 1.0), 0); m.addSupport(LinkKind::Roller, bi, Vec2(2, 1.0), 0);
                    m.run.duration = 4; selBody = bi; needFit = true; autotestPhase = 192; autotestWait = 14; } break;
        case 192: shot("ui_dload_design.bmp"); startRun(true); autotestPhase = 193; autotestWait = 260; break;
        case 193: shot("ui_dload_run.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 200: showWelcome = false; autotestPhase = 201; autotestWait = 12; break;                                                      // cross-sections, diagrams, temperature
        case 201: { newDesign(); Model& m = design(); Body bm = makeBeam(4.0, 0.2, *findMaterial("Steel"), Vec2(0, 1.0)); bm.massFromDensity = true;
                    bm.secType = SEC_I; bm.secB = 0.12; bm.secH = 0.24; bm.secTf = 0.012; bm.secTw = 0.007; bm.dloadType = DL_Uniform; bm.dloadN = 6000; bm.tempGrad = 40; bm.tempDelta = 30;
                    int bi = m.addBody(bm); m.addSupport(LinkKind::Pin, bi, Vec2(-2, 1.0), 0); m.addSupport(LinkKind::Pin, bi, Vec2(2, 1.0), 0);
                    m.run.duration = 3; m.updateMass(); selBody = bi; needFit = true; propScrollY = 520; autotestPhase = 202; autotestWait = 14; } break;
        case 202: shot("ui_section_props.bmp"); startRun(false); autotestPhase = 203; break;
        case 203: if (state == RunState::Done) { forceTab = 4; diagSubForce = 0; autotestPhase = 204; autotestWait = 8; } break;
        case 204: shot("ui_diagrams.bmp"); diagSubForce = 1; autotestPhase = 205; autotestWait = 8; break;
        case 205: shot("ui_sectionstress.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 210: showWelcome = false; autotestPhase = 211; autotestWait = 12; break;                                                      // the segmented sidebar
        case 211: loadExample(19); startRun(false); autotestPhase = 212; break;
        case 212: if (state == RunState::Done) { exportAll = false; sideOpen = 4; autotestPhase = 213; autotestWait = 10; } break;
        case 213: shot("nav_data.bmp"); sideOpen = 2; autotestPhase = 214; autotestWait = 8; break;
        case 214: shot("nav_setup.bmp"); sideOpen = 3; autotestPhase = 215; autotestWait = 8; break;
        case 215: shot("nav_grid.bmp"); sideOpen = 0; autotestPhase = 216; autotestWait = 8; break;
        case 216: shot("nav_physical.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 220: showWelcome = false; autotestPhase = 221; autotestWait = 12; break;                                                      // the Object section and a drop-down
        case 221: { newDesign(); Model& m = design(); Body bx = makeBox(1.0, 0.6, *findMaterial("Steel"), Vec2(0, 0)); bx.massFromDensity = false; bx.mass = 20; m.addBody(bx);
                    selBody = 0; needFit = true; autotestPhase = 222; autotestWait = 14; } break;
        case 222: shot("nav_object.bmp"); navMenu = 0; autotestPhase = 223; autotestWait = 6; break;
        case 223: shot("nav_popup.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 230: showWelcome = false; autotestPhase = 231; autotestWait = 12; break;                                                      // a real click on Run
        case 231: loadExample(18); autotestPhase = 232; autotestWait = 14; break;
        case 232: click(ImVec2(canvasMin.x + 60, canvasMin.y + 21), 0); autotestPhase = 233; autotestWait = 5; break;
        case 233: { FILE* f = std::fopen((autotestDir + "/clickrun.txt").c_str(), "w"); if (f) { std::fprintf(f, "after clicking Run: state %s, t = %.3f s", stateName(), ses.hasWorld() ? ses.simTime() : 0.0); std::fclose(f); }
                    autotestPhase = 234; autotestWait = 20; } break;
        case 234: shot("click_run.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 240: showWelcome = false; autotestPhase = 241; autotestWait = 12; break;                                                      // connections at chosen points
        case 241: { newDesign(); Model& m = design();
                    Body big = makeBox(3.0, 1.2, *findMaterial("Steel"), Vec2(0, 1.0)); big.massFromDensity = false; big.mass = 60; big.name = "Structural mass"; m.addBody(big);
                    Body sm = makeBox(1.6, 0.6, *findMaterial("Steel"), Vec2(0, -1.4)); sm.massFromDensity = false; sm.mass = 8; sm.name = "Damper mass"; m.addBody(sm);
                    m.addBody(makeFloor(7, 0, *findMaterial("Concrete"), Vec2(0, 2.6)));
                    needFit = true; autotestPhase = 242; autotestWait = 14; } break;
        case 242: tool = Tool::Spring; linkAttach = 1; linkDir = 1; click(toScreen(Vec2(-0.7, 0.43)), 0); autotestPhase = 243; autotestWait = 8; break;
        case 243: shot("conn_prompt.bmp"); click(toScreen(Vec2(-0.6, -1.15)), 0); autotestPhase = 244; autotestWait = 8; break;
        case 244: tool = Tool::Damper; click(toScreen(Vec2(0.7, 0.43)), 0); autotestPhase = 245; autotestWait = 8; break;
        case 245: click(toScreen(Vec2(0.6, -1.15)), 0); autotestPhase = 246; autotestWait = 8; break;
        case 246: tool = Tool::Spring; click(toScreen(Vec2(-1.0, 1.55)), 0); autotestPhase = 247; autotestWait = 8; break;
        case 247: click(toScreen(Vec2(-1.0, 2.58)), 0); autotestPhase = 248; autotestWait = 8; break;
        case 248: tool = Tool::Damper; click(toScreen(Vec2(1.0, 1.55)), 0); autotestPhase = 249; autotestWait = 8; break;
        case 249: click(toScreen(Vec2(1.0, 2.58)), 0); autotestPhase = 250; autotestWait = 8; break;
        case 250: tool = Tool::Select; { FILE* f = std::fopen((autotestDir + "/conn.txt").c_str(), "w");
                    if (f) { std::fprintf(f, "links %d: ", (int)design().links.size()); for (const Link& l : design().links) std::fprintf(f, "[kind %d a%d b%d  la(%.2f,%.2f) lb(%.2f,%.2f)] ", (int)l.kind, l.a, l.b, l.la.x, l.la.y, l.lb.x, l.lb.y); std::fclose(f); } }
                  autotestPhase = 251; autotestWait = 6; break;
        case 251: shot("conn_done.bmp"); autotestPhase = 107; autotestWait = 3; break;
        case 260: showWelcome = false; autotestPhase = 261; autotestWait = 12; break;                                                      // re-orient a placed connection
        case 261: { newDesign(); Model& m = design(); Body bx = makeBox(1.6, 0.6, *findMaterial("Steel"), Vec2(0, 0)); bx.massFromDensity = false; bx.mass = 20; m.addBody(bx);
                    int fl = m.addBody(makeFloor(6, 0, *findMaterial("Concrete"), Vec2(0, 2.0)));
                    int dm = m.addLink(LinkKind::Damper, 0, fl, Vec2(0.5, 0.3), Vec2(1.7, 2.0)); (void)dm;                              // diagonal, like the screenshot
                    selLink = 0; selBody = -1; sideOpen = 1; needFit = true; autotestPhase = 262; autotestWait = 14; } break;
        case 262: shot("orient_before.bmp"); reorientLink(0, Vec2(0, 1), true); edited(); autotestPhase = 263; autotestWait = 8; break;
        case 263: { const Link& l = design().links[0]; FILE* f = std::fopen((autotestDir + "/orient.txt").c_str(), "w");
                    if (f) { std::fprintf(f, "after Make it vertical: la(%.3f,%.3f) lb(%.3f,%.3f)", l.la.x, l.la.y, l.lb.x, l.lb.y); std::fclose(f); }
                    shot("orient_after.bmp"); autotestPhase = 107; autotestWait = 3; } break;
        case 270: showWelcome = false; pageDashboard = false; loadExample(6); design().run.duration = 1; autotestPhase = 271; autotestWait = 3; break;   // record on a FINISHED run
        case 271: startRun(false); autotestPhase = 272; break;
        case 272: if (state == RunState::Done) { startRecording(); autotestPhase = 273; autotestWait = 60; } break;
        case 273: stopRecording(); autotestPhase = 274; autotestWait = 3; break;
        case 274: { FILE* f = std::fopen((autotestDir + "/rec_done.txt").c_str(), "w");
                    if (f) { std::fprintf(f, "frames=%d path=%s", recFrames, recPath.c_str()); std::fclose(f); } autotestPhase = 107; } break;
        case 107: say("autotest finished"); autotestQuit = true; autotestPhase = 108; break;
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
        std::string title = "VL Lab Mechanics Suite  -  " + design().name + (dirty ? " *" : "") + "  [" + stateName() + "]";
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
    if (pageDashboard) drawDashboard();
    else drawCanvas();
    drawSidebar();
    if (ImGuiWindow* sw = ImGui::FindWindowByName("Sidebar"))
        if (sw->DockNode) sw->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;      // re-applied every frame: ImGui does not store it in the layout file
    if (!pageDashboard) drawTelemetry();
    if (showWelcome) drawWelcome();
    if (showAbout) drawAbout();
    if (showHelp) drawHelp();
    drawToast();
    if (!ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(0)) undoPushed = false;
    // bring the right tab forward: Properties when something new is selected, Run when a run starts, the Dashboard on start-up
    if ((selBody >= 0 && selBody != lastSelBody) || (selLink >= 0 && selLink != lastSelLink)) sideOpen = 1;
    lastSelBody = selBody;
    lastSelLink = selLink;
    if (focusSim) { pageDashboard = false; focusSim = false; }
    if (focusDashboard) { pageDashboard = true; dashJustShown = true; focusDashboard = false; }
    if (focusWindow) { ImGui::SetWindowFocus(focusWindow); focusWindow = nullptr; }
}

void App::buildDefaultLayout(unsigned rootId) {
    ImGuiID root = rootId;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(root);
    ImGui::DockBuilderAddNode(root, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(root, vp->Size);
    ImGuiID left, rest, centre, bottom;
    ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.21f, &left, &rest);          // one sidebar on the left, canvas and telemetry to its right
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Down, 0.42f, &bottom, &centre);
    ImGui::DockBuilderDockWindow("Sidebar", left);
    ImGui::DockBuilderDockWindow("Simulation", centre);
    ImGui::DockBuilderDockWindow("Telemetry", bottom);
    if (ImGuiDockNode* ln = ImGui::DockBuilderGetNode(left)) ln->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;      // the sidebar needs no tab
    focusDashboard = true;
    ImGui::DockBuilderFinish(root);
}

}  // namespace vlab
