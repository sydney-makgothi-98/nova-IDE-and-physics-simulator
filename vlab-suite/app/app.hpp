// app.hpp -- the desktop application: all the state of the interface and the panels that edit it.
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "imgui.h"
#include "vlab/analysis.hpp"
#include "vlab/project_io.hpp"
#include "vlab/session.hpp"

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
};

struct App {
    // ---------------------------------------------------------------- experiment
    Session ses;
    Model runModel;                      // the design that produced the telemetry (so exports match the data)
    bool haveResults = false;
    bool resultsStale = false;           // the design was edited after the run
    RunState state = RunState::Design;
    double timeScale = 1.0;
    std::string statusLine;

    // ---------------------------------------------------------------- view and tools
    Tool tool = Tool::Select;
    Vec2 viewCenter{0, 0};
    double scale = 70.0;                 // pixels per metre
    bool snap = true;
    double snapStep = 0.1;
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
    void drawLibrary();
    void drawProperties();
    void drawEnvironment();
    void drawRunPanel();
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
std::string openFileDialog(const char* filter, const char* initialDir);
std::string saveFileDialog(const char* filter, const char* defExt, const std::string& initialDir, const std::string& defaultName);
std::string documentsFolder();
bool ensureDirectory(const std::string& path);
void openInShell(const std::string& path, bool selectInExplorer);
ImU32 argbToImU32(unsigned argb);
unsigned imU32ToArgb(ImU32 c);

}  // namespace vlab
