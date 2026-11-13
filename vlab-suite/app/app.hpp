// app.hpp -- the desktop application: all the state of the interface and the panels that edit it.
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "imgui.h"
#include "vlab/analysis.hpp"
#include "vlab/deform.hpp"
#include "vlab/project_io.hpp"
#include "vlab/session.hpp"
#include "video.hpp"

class VideoEncoder;      // encoder.hpp (Media Foundation H.264 writer)

namespace vlab {

enum class Tool { Select, Circle, Box, Polygon, Wedge, Floor, Anchor, Beam, Spring, Damper, Rod, Pin, Weld, SupportPin, SupportFixed, SupportRoller };
enum class RunState { Design, Running, Paused, Computing, Done };

// Everything the platform layer (main.cpp) must provide to the application.
struct Platform {
    virtual ~Platform() = default;
    virtual bool grabFrame(int x, int y, int w, int h, const void** pixels, int* rowBytes) = 0;   // read pixels back from the screen buffer
    virtual void releaseFrame() = 0;
    virtual void setTitle(const std::string& title) = 0;
    virtual void requestQuit() = 0;
    // textures for the dashboard video: createTexture returns an opaque handle; textureId gives the value to hand to ImGui::Image
    virtual void* createTexture(int w, int h) = 0;
    virtual void updateTexture(void* tex, const void* bgra, int pitchBytes) = 0;
    virtual void destroyTexture(void* tex) = 0;
    virtual unsigned long long textureId(void* tex) = 0;
};

// Colours the canvas draws with; they change with the light / dark theme.
struct Pal {
    ImU32 bg, gridMinor, gridMajor, gridAxis, gridText, text, dim, mid, ink, link, amber, panel;
};

struct App {
    // ---------------------------------------------------------------- experiment
    Session ses;
    Model runModel;                      // the design that produced the telemetry (so exports match the data)
    bool haveResults = false;
    bool resultsStale = false;           // the design was edited after the run
    RunState state = RunState::Design;
    double timeScale = 1.0;
    bool slowRun = false;                // the last frames could not keep up with real time
    std::string statusLine;

    // ---------------------------------------------------------------- view and tools
    Tool tool = Tool::Select;
    Vec2 viewCenter{0, 0};
    double scale = 70.0;                 // pixels per metre
    bool snap = true;
    double snapStep = 0.1;
    // ---------------------------------------------------------------- dashboard
    struct Project { std::string path, name, when; long long mtime = 0, bytes = 0; Model model; };
    std::vector<Project> projects;       // the .vlab files in the designs folder, newest first
    double projectsScanned = -100;
    std::unique_ptr<VideoPlayer> cover;  // the cover video (dashboard_cover.mp4)
    void* coverTex = nullptr;
    int coverTexW = 0, coverTexH = 0;
    bool coverTried = false;
    bool pageDashboard = true;           // the Dashboard page is showing (it replaces the canvas and telemetry, the sidebar stays)
    bool dashJustShown = true;
    bool focusSim = false;               // bring the Simulation tab forward (after opening or running something)
    bool focusDashboard = true;          // bring the Dashboard tab forward on the next frame
    const char* focusWindow = nullptr;   // bring this window (a docked tab) forward on the next frame
    int lastSelBody = -1, lastSelLink = -1;
    std::string coverTarget() const { return docsDir + "\\dashboard_cover.mp4"; }
    void drawDashboard();
    void scanProjects();
    void loadCover();
    bool darkTheme = true;               // light / dark interface theme (saved in the documents folder)
    Pal palette() const;
    void applyTheme();                   // push the theme into ImGui and ImPlot
    void setTheme(bool dark);
    bool showGridNumbers = true;
    bool showReactions = false;          // orange reaction-force arrows at joints and supports (off: they clutter a running scene)
    ImVec2 toolbarMin{0, 0}, toolbarMax{0, 0};   // the Run / Pause / Stop strip on the canvas (clicks there are not canvas clicks)
    // data extraction settings (Data panel)
    std::string exportBase;              // file name, empty: the design name
    bool exportStamp = true;             // add the date and time to the file name
    bool exportAll = true;               // every channel, or only the ticked ones
    std::vector<char> exportSel;         // one flag per telemetry channel (when not all)
    bool autoExportCsv = false, autoExportXlsx = false, autoRecord = false;
    void saveDataSettings();
    void loadDataSettings();
    std::string exportBaseName() const;
    std::string exportFileName(const char* ext) const;
    const Telemetry* exportSource(Telemetry& tmp, bool excel) const;
    bool showGrid = true, showContacts = true, showLabels = true, showVectors = false;
    int selBody = -1, selLink = -1;
    int linkFirst = -1;                  // first body clicked while drawing a link
    Vec2 linkFirstLocal;
    int dragBody = -1;                   // body being dragged with the mouse
    Vec2 dragLocal;                      // grab point in the body's frame
    bool panning = false;
    bool needFit = true;                 // frame the whole design in the canvas on the next draw
    void fitView();
    ImVec2 canvasMin{0, 0}, canvasMax{0, 0};   // screen rectangle of the canvas (for recording)

    // settings for the next object to be placed
    float nextRadius = 0.4f, nextW = 1.0f, nextH = 0.6f, nextMass = 1.0f;
    int nextSides = 5;
    int nextMaterial = 1;                // index into materialLibrary()
    float floorLength = 8.0f;
    int floorMaterial = 5;
    float beamLength = 3.0f, beamThick = 0.15f;
    float nextAngle = 0.0f;              // rotation (degrees) of the object being placed: Q / E or Ctrl+wheel turn the ghost
    int navMenu = -1;                    // which navbar dropdown is open (-1 none)
    int exampleTab = 0;                  // 0 Basic, 1 Intermediate, 2 Complex
    float nextK = 50.0f;

    // ---------------------------------------------------------------- undo
    std::vector<Model> undoStack, redoStack;
    Model frameSnapshot;
    bool undoPushed = false;

    // ---------------------------------------------------------------- files
    std::string currentPath;             // current design file, empty = unsaved
    bool dirty = false;
    std::string docsDir, designsDir, exportsDir, recordingsDir;
    std::string lastExportPath;

    // ---------------------------------------------------------------- plots and analysis
    std::vector<char> plotSel;           // one flag per telemetry channel
    int plotTab = 0;
    int forceTab = -1;                   // select this Telemetry tab on the next draw (-1 = leave alone)
    bool plotAutoFit = true;
    int specChannel = 0;
    Spectrum spectrum;
    bool haveSpectrum = false;
    std::string specName;
    ModalResult modal;
    bool haveModal = false;
    int modeShown = -1;
    double modeClock = 0;
    // frequency response (runs on a worker thread)
    FrfRequest frfReq;
    std::vector<FrfPoint> frf;
    bool haveFrf = false;
    std::atomic<float> frfProgress{0};
    std::atomic<bool> frfBusy{false}, frfCancel{false};
    void* frfHandle = nullptr;           // Win32 worker thread running the sweep
    FrfRequest frfJobReq;
    Model frfJobModel;
    std::vector<FrfPoint> frfResult;
    std::atomic<bool> frfDone{false};
    std::string frfNote;
    int phaseBody = 0;
    bool plotPicked = false;             // the Plots tab has chosen its default channels for this run
    bool specPicked = false;             // the Spectrum tab has chosen a signal that moves
    int deformBody = 0;                  // Deformation tab: which body
    float propScrollY = -1;              // (test hook) scroll the Properties panel to this position on the next frame
    int diagSubForce = -1;               // (test hook) select a sub-tab of Beam diagrams on the next frame
    int sideOpen = 1;                    // which SETTINGS section is open in the sidebar: 0 Physical, 1 Object, 2 Sim setup, 3 Grid, 4 Data (-1: none)
    bool toolsOpen = true;               // the DESIGN TOOLS group is expanded
    int diagBody = 0;                    // Beam diagrams tab: which beam
    bool diagLive = true;                // ... follow the latest sample
    double diagX = -1;                   // ... where along the beam the cross-section is examined (m from the left end, < 0: mid-span)
    int deformRow = -1;                  // Deformation tab: telemetry row shown in the shape plot (-1 = the row of peak deflection)

    // ---------------------------------------------------------------- deformation view (canvas)
    bool showDeform = true;
    bool deformRealistic = true;         // blocks sink with the beam and their underside follows its bent surface
    bool deformAuto = true;              // pick the exaggeration so the bending is easy to see
    float deformExag = 100.0f;           // manual exaggeration factor
    double deformPeak = 0;               // largest relative bending seen in this run (keeps the automatic factor steady)
    double deformFactor = 1;             // the factor in use this frame
    std::vector<Deflection> liveDef;     // per body, from the running world
    std::vector<Squash> liveSquash;
    std::vector<BeamFE> liveFE;
    std::vector<std::vector<double>> smoothW;   // what is drawn: eases toward the real deflection over about 2.5 s so contact can be watched
    std::vector<double> smoothStrain, smoothLoad, liveTarget;   // squash easing, and the real (un-eased) peak deflection per body
    double lastSimT = -1;
    std::vector<char> baseLatched;       // the beam has been touched once: its baseline is frozen
    std::vector<std::vector<double>> baseW;   // each beam's shape while nothing touches it (its own weight): not drawn, so bending starts at contact
    double curFilt = 0;                  // bending seen lately, low-passed (50 ms) so one-step contact spikes do not set the automatic exaggeration
    ImVec2 overlayMin{0, 0}, overlayMax{0, 0};   // the control strip on the canvas (clicks there are not canvas clicks)
    void updateLiveDeformation();

    // ---------------------------------------------------------------- recording
    bool recording = false;
    int recFrames = 0;
    double recAccum = 0;
    int recX = 0, recY = 0, recW = 0, recH = 0;
    std::string recPath;
    bool recWholeWindow = false;
    bool wantCapture = false;            // set by frame(): the platform layer should grab this frame
    std::unique_ptr<VideoEncoder> encoder;

    // ---------------------------------------------------------------- automated self-test of the running UI (--autotest DIR)
    std::string autotestDir;
    int autotestPhase = 0, autotestWait = 0;
    bool autotestQuit = false;
    ImVec2 clickPos{0, 0};
    int clickStage = 0;
    std::string pendingShot;             // screenshot to write once the frame has been drawn
    void autotestStep();
    void autotestCapture();
    bool exportExcelTo(const std::string& path);
    bool exportCsvTo(const std::string& path);

    // ---------------------------------------------------------------- ui bits
    struct Toast { std::string text; double until = 0; bool error = false; };
    Toast toast;
    bool showWelcome = true, showAbout = false, showHelp = false, firstFrame = true, resetLayout = false;
    double now = 0;                      // seconds since start (updated every frame)
    Platform* platform = nullptr;

    // ---------------------------------------------------------------- lifecycle
    App();
    ~App();
    void init(Platform* p);
    void frame(float dt);                // one frame: advance the physics, draw every panel
    void shutdown();
    void endOfFrameCapture();            // called after the screen is drawn: writes one video frame if one is due

    // ---------------------------------------------------------------- helpers shared by the panels (app_core.cpp)
    Model& design() { return ses.design(); }
    void pushUndo();                     // remember the state before an edit (once per interaction)
    void edited();                       // call after any change to the design
    void undo();
    void redo();
    void say(const std::string& s, bool error = false);
    void newDesign();
    void loadDesignDialog();
    bool saveDesign(bool saveAs);
    void openFile(const std::string& path);
    void loadExample(int index);
    void startRun(bool realTime);        // realTime = false: compute the whole run now
    void pause();
    void stopRun();
    void stepOnce();
    void exportExcelDialog();
    void exportCsvDialog();
    void startRecording();
    void stopRecording();
    void computeSpectrum();
    void computeModal();
    void startFrf();
    bool structureLocked() const { return state != RunState::Design; }
    bool canEditStructure();             // false (with a message) while a run exists
    const char* stateName() const;
    int channelColorIndex(int channel) const { return channel; }

    // ---------------------------------------------------------------- panels
    void drawMenuBar();
    void drawCanvas();
    void drawSidebar();
    void drawProperties();
    void drawEnvironment();
    void drawRunPanel();
    void drawGrid();
    void drawData();
    void drawTelemetry();
    void drawWelcome();
    void drawAbout();
    void drawHelp();
    void drawToast();
    void buildDefaultLayout(unsigned rootId);

    // canvas internals (ui_canvas.cpp)
    ImVec2 toScreen(Vec2 w) const;
    Vec2 toWorld(ImVec2 s) const;
    Vec2 snapped(Vec2 w) const;
    int pickBody(Vec2 w) const;
    int pickLink(Vec2 w) const;
    bool bodyPose(int i, Vec2& pos, double& angle) const;
    void placeAt(Vec2 w);
    Vec2 supportPoint(int body, Vec2 w) const;   // snap a support to a beam end, edge middle or centre when close
    void placeSupport(Vec2 w);
    void afterPlace();
    void canvasInteract(ImVec2 mouse, bool hovered);
    void handleShortcuts();
    void advanceRun(float dt);
    void finishRunIfDone();
    void afterRunEdit();
};

// platform helpers (dialogs.cpp)
std::string pickFolderDialog(const char* title, const std::string& initial);
std::string openFileDialog(const char* filter, const char* initialDir);
std::string saveFileDialog(const char* filter, const char* defExt, const std::string& initialDir, const std::string& defaultName);
std::string documentsFolder();
bool ensureDirectory(const std::string& path);
void openInShell(const std::string& path, bool selectInExplorer);
ImU32 argbToImU32(unsigned argb);
unsigned imU32ToArgb(ImU32 c);

// the cross-section of a beam drawn into the current window, optionally coloured by normal stress (section_ui.cpp)
void drawSectionSketch(const Section& s, ImVec2 origin, ImVec2 size, const SectionStress* stress, const Pal& pal);

}  // namespace vlab
