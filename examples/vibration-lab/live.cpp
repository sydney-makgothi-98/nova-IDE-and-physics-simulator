// live.cpp -- Vibration Lab: design your own spring / mass / damper system, run it, watch the telemetry,
// and record the whole thing to an .mp4 video.  Windows only (plain Win32 + GDI+ + Media Foundation).
//
//   engine.hpp    the Lagrangian physics
//   model_io.hpp  save / load / example designs
//   app.hpp       state + what every click does
//   render.hpp    all the drawing
//   encoder.hpp   H.264 video writer
//   live.cpp      (this file) window, main loop, recording, CSV export, self-test
//
//   Command line:   vibration_live --selftest
//                   vibration_live --shot out.png  [--preset N] [--run SECONDS] [--select node:I|link:I] [--tab N]
//                   vibration_live --demo out.mp4  [--preset N] [--run SECONDS]     (records without opening a window)
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <windows.h>
#include <gdiplus.h>

#include <ctime>
#include <fstream>

#include "app.hpp"
#include "encoder.hpp"
#include "render.hpp"

static HWND gWnd = nullptr;

// ------------------------------------------------------------------ off-screen picture the program draws into
struct Canvas {
    HDC dc = nullptr; HBITMAP bmp = nullptr, old = nullptr; void* bits = nullptr; int w = 0, h = 0;
    void make(int nw, int nh) {
        destroy();
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = nw; bi.bmiHeader.biHeight = -nh;            // negative height = rows run top to bottom
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        HDC screen = GetDC(nullptr);
        dc = CreateCompatibleDC(screen);
        bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screen);
        old = (HBITMAP)SelectObject(dc, bmp);
        w = nw; h = nh;
    }
    void destroy() { if (dc) { SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc); dc = nullptr; } }
} canvas;

static void drawFrameToCanvas() {
    if (canvas.w != W || canvas.h != H) canvas.make(W, H);
    Graphics g(canvas.dc);
    render(g, W, H);
}
static void present() {
    if (!gWnd || W < 10 || H < 10) return;
    drawFrameToCanvas();
    HDC hdc = GetDC(gWnd);
    BitBlt(hdc, 0, 0, W, H, canvas.dc, 0, 0, SRCCOPY);
    ReleaseDC(gWnd, hdc);
}

// ------------------------------------------------------------------ files in the "recordings" folder
static std::string stamp() {
    time_t t = time(nullptr);
    char b[32];
    strftime(b, sizeof b, "%Y%m%d_%H%M%S", localtime(&t));
    return b;
}
static std::string recordingsDir() {
    std::string d = std::string(LAB_DIR) + "/recordings";
    CreateDirectoryA(d.c_str(), nullptr);
    return d;
}

// ------------------------------------------------------------------ recording
static VideoEncoder encoder;
static int recW = 0, recH = 0;
static const int REC_FPS = 60;

static void startRecording() {
    recW = (W - PANEL_W) & ~1;          // record the drawing + plots only (not the control panel), even size
    recH = H & ~1;
    std::string path = recordingsDir() + "/vibration_" + stamp() + ".mp4", err;
    if (!encoder.open(path, recW, recH, REC_FPS, err)) { toast("Cannot record: " + err); return; }
    A.recording = true; A.recFrames = 0; A.recPath = path;
    toast("Recording... press Record again (or F9) to finish");
}
static void stopRecording() {
    if (!A.recording) return;
    A.recording = false;
    bool ok = encoder.close();
    toast(ok ? "Saved video: recordings/" + A.recPath.substr(A.recPath.find_last_of('/') + 1) : "Video could not be finalised");
}
void toggleRecording() { if (A.recording) stopRecording(); else startRecording(); }

// called once per drawn frame while recording
static void captureFrame() {
    if (!A.recording) return;
    if (canvas.w < recW || canvas.h < recH) { stopRecording(); toast("Window got smaller than the video: recording stopped"); return; }
    if (!encoder.addFrame(canvas.bits, canvas.w * 4)) { stopRecording(); toast("Encoder error: recording stopped"); return; }
    ++A.recFrames;
}

// ------------------------------------------------------------------ CSV export of the whole run
void exportCsv() {
    if (A.log.empty()) { toast("Nothing to export yet: press Run first"); return; }
    std::string path = recordingsDir() + "/telemetry_" + stamp() + ".csv";
    std::ofstream f(path);
    if (!f) { toast("Could not write the CSV file"); return; }
    const lag::Model& M = A.model;
    f << "t,T,V,E,dissipated,work_in";
    int k = 0;
    for (size_t i = 0; i < M.nodes.size(); ++i) {
        if (M.nodes[i].kind != lag::NodeKind::Mass) continue;
        f << ",m" << k << "_x,m" << k << "_y,m" << k << "_vx,m" << k << "_vy";
        ++k;
    }
    for (size_t li = 0; li < M.links.size(); ++li) {
        const char* kind = M.links[li].kind == lag::LinkKind::Spring ? "spring" : "damper";
        f << "," << kind << li << (M.links[li].kind == lag::LinkKind::Spring ? "_stretch" : "_rate") << "," << kind << li << "_force," << kind << li << "_length";
    }
    f << "\n";
    f.precision(7);
    for (const Frame& fr : A.log) {
        f << fr.t << "," << fr.T << "," << fr.V << "," << fr.T + fr.V << "," << fr.dissipated << "," << fr.workIn;
        for (size_t i = 0; i < M.nodes.size(); ++i)
            if (M.nodes[i].kind == lag::NodeKind::Mass && 4 * i + 3 < fr.nd.size())
                f << "," << fr.nd[4 * i] << "," << fr.nd[4 * i + 1] << "," << fr.nd[4 * i + 2] << "," << fr.nd[4 * i + 3];
        for (size_t li = 0; li < M.links.size(); ++li)
            if (3 * li + 2 < fr.lk.size()) f << "," << fr.lk[3 * li] << "," << fr.lk[3 * li + 1] << "," << fr.lk[3 * li + 2];
        f << "\n";
    }
    toast("Telemetry saved: recordings/" + path.substr(path.find_last_of('/') + 1));
}

// ------------------------------------------------------------------ dominant frequency of the selected mass
static void updateDominantFrequency() {
    if (A.mode != MODE_RUN || A.selKind != SEL_NODE || A.sel < 0 || A.sel >= (int)A.model.nodes.size()) { A.domFreq = 0; return; }
    if (A.model.nodes[A.sel].kind != lag::NodeKind::Mass || A.log.size() < 64) { A.domFreq = 0; return; }
    int comp = ((int)std::lround(A.model.nodes[A.sel].rail) == lag::RAIL_Y) ? 1 : 0;
    size_t n = std::min<size_t>(A.log.size(), 360), start = A.log.size() - n;
    std::vector<double> x;
    for (size_t i = start; i < A.log.size(); ++i) x.push_back(A.log[i].nd[4 * (size_t)A.sel + comp]);
    double span = A.log.back().t - A.log[start].t;
    A.domFreq = span > 0.5 ? lag::dominantFrequency(x, (double)(n - 1) / span) : 0;
}

// ------------------------------------------------------------------ keyboard + window procedure
static void onKey(WPARAM k) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (ctrl && k == 'S') { saveDesign(); return; }
    if (ctrl && k == 'O') { loadDesign(); return; }
    switch (k) {
        case VK_SPACE: toggleRun(); break;
        case VK_ESCAPE:
            if (A.linkStart >= 0) A.linkStart = -1;
            else if (A.mode != MODE_EDIT) stopRun();
            else setTool(TOOL_SELECT);
            break;
        case 'S': setTool(TOOL_SELECT); break;
        case 'M': setTool(TOOL_MASS); break;
        case 'G': setTool(TOOL_GROUND); break;
        case 'K': setTool(TOOL_SPRING); break;
        case 'D': setTool(TOOL_DAMPER); break;
        case VK_DELETE: case VK_BACK: deleteSelection(); break;
        case VK_F9: toggleRecording(); break;
        case VK_TAB: A.plotMode = (A.plotMode + 1) % 4; break;
        case VK_HOME: resetView(); break;
        case VK_OEM_COMMA:  A.speed = std::fmax(0.125, A.speed / 2); break;
        case VK_OEM_PERIOD: A.speed = std::fmin(4.0, A.speed * 2); break;
        case '1': case '2': case '3': case '4': loadPreset((int)(k - '0')); break;
        case VK_NUMPAD1: case VK_NUMPAD2: case VK_NUMPAD3: case VK_NUMPAD4: loadPreset((int)(k - VK_NUMPAD0)); break;
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
        case WM_SIZE:   W = LOWORD(lp); H = HIWORD(lp); return 0;
        case WM_GETMINMAXINFO: { MINMAXINFO* mi = (MINMAXINFO*)lp; mi->ptMinTrackSize.x = 1100; mi->ptMinTrackSize.y = 700; return 0; }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:  { PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); present(); return 0; }
        case WM_LBUTTONDOWN: SetCapture(h); onMouseDown((short)LOWORD(lp), (short)HIWORD(lp)); return 0;
        case WM_MOUSEMOVE:   onMouseMove((short)LOWORD(lp), (short)HIWORD(lp)); return 0;
        case WM_LBUTTONUP:   ReleaseCapture(); onMouseUp(); return 0;
        case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            SetCapture(h); A.panning = true; A.panX = (short)LOWORD(lp); A.panY = (short)HIWORD(lp); return 0;
        case WM_RBUTTONUP: case WM_MBUTTONUP: ReleaseCapture(); A.panning = false; return 0;
        case WM_MOUSEWHEEL: {
            POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
            ScreenToClient(h, &p);
            onWheel(p.x, p.y, GET_WHEEL_DELTA_WPARAM(wp) / 120);
            return 0;
        }
        case WM_KEYDOWN: onKey(wp); return 0;
        case WM_DESTROY:
            stopRecording();
            lag::saveModel(designPath(), A.model);          // your design is still there next time
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(h, m, wp, lp);
}

// ------------------------------------------------------------------ self-test (no window needed)
static int gFail = 0;
static void check(const char* name, bool ok, const char* detail) {
    std::printf("  [%s] %s  %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) ++gFail;
}
static int selfTest() {
    std::printf("Vibration Lab self-test\n");
    char d[200];

    {   // 1) single mass vs the textbook formula
        lag::Model M = lag::presetOneDof();
        lag::State s = lag::initialState(M);
        lag::Meter meter; meter.reset(M, s);
        std::vector<double> ext(s.q.size(), 0.0);
        const double m = 2.0, k = 30.0, c = 1.5, wn = std::sqrt(k / m), zeta = c / (2 * std::sqrt(k * m)), wd = wn * std::sqrt(1 - zeta * zeta);
        double worst = 0;
        for (int i = 0; i < 10000; ++i) {
            lag::advance(M, s, ext, 0.001, meter);
            double t = s.t, u = std::exp(-zeta * wn * t) * (std::cos(wd * t) + zeta * wn / wd * std::sin(wd * t));   // x0 = 1, v0 = 0
            worst = std::fmax(worst, std::fabs((-1.0 + u) - s.q[2]));
        }
        std::snprintf(d, sizeof d, "worst gap %.2e m after 10 s", worst);
        check("1-DOF matches theory", worst < 1e-6, d);
        std::snprintf(d, sizeof d, "energy-law error %.2e J", std::fabs(meter.residual()));
        check("1-DOF energy law", std::fabs(meter.residual()) < 1e-4, d);
    }
    {   // 2) two masses, driven, with mouse-like extra force: the energy law must still hold
        lag::Model M = lag::presetTwoDof();
        M.nodes[1].driveF = 6; M.nodes[1].driveW = 5;
        lag::State s = lag::initialState(M);
        lag::Meter meter; meter.reset(M, s);
        std::vector<double> ext(s.q.size(), 0.0);
        double worst = 0;
        for (int i = 0; i < 20000; ++i) {
            ext[2 * 2] = 2.0 * std::sin(0.002 * i);                       // wiggle the second mass in x as well
            lag::advance(M, s, ext, 0.001, meter);
            worst = std::fmax(worst, std::fabs(meter.residual()));
        }
        std::snprintf(d, sizeof d, "worst error %.2e J over 20 s (E up to %.1f J)", worst, meter.E);
        check("2-DOF driven energy law", worst < 5e-3, d);
    }
    {   // 3) swinging spring pendulum without any damper: total energy must stay put
        lag::Model M = lag::presetSpringPendulum();
        for (size_t i = M.links.size(); i-- > 0;) if (M.links[i].kind == lag::LinkKind::Damper) M.removeLink((int)i);
        lag::State s = lag::initialState(M);
        lag::Meter meter; meter.reset(M, s);
        std::vector<double> ext(s.q.size(), 0.0);
        double worst = 0;
        for (int i = 0; i < 30000; ++i) { lag::advance(M, s, ext, 0.001, meter); worst = std::fmax(worst, std::fabs(meter.E - meter.E0)); }
        std::snprintf(d, sizeof d, "energy wandered by %.2e J (E0 = %.3f J)", worst, meter.E0);
        check("spring pendulum conserves energy", worst < 5e-3, d);
    }
    {   // 4) rails really remove a degree of freedom
        lag::Model M = lag::presetQuarterCar();
        lag::State s = lag::initialState(M);
        lag::Meter meter; meter.reset(M, s);
        std::vector<double> ext(s.q.size(), 0.0);
        for (int i = 0; i < 3000; ++i) lag::advance(M, s, ext, 0.001, meter);
        double sideways = std::fabs(s.q[2]) + std::fabs(s.q[4]);          // x of wheel and body: must be exactly where they started (0)
        std::snprintf(d, sizeof d, "sideways drift %.1e m", sideways);
        check("Y-rail masses never move sideways", sideways < 1e-12, d);
    }
    {   // 5) frequency finder
        std::vector<double> x;
        for (int i = 0; i < 360; ++i) x.push_back(std::sin(2 * lag::PI * 2.5 * i / 60.0) * std::exp(-0.05 * i / 60.0));
        double f = lag::dominantFrequency(x, 60.0);
        std::snprintf(d, sizeof d, "found %.3f Hz (true 2.5)", f);
        check("frequency finder", std::fabs(f - 2.5) < 0.06, d);
    }
    {   // 6) save / load round trip
        lag::Model a = lag::presetQuarterCar(), b;
        std::string p = recordingsDir() + "/_selftest.vlab";
        bool ok = lag::saveModel(p, a) && lag::loadModel(p, b);
        ok = ok && a.nodes.size() == b.nodes.size() && a.links.size() == b.links.size() && std::fabs(a.links[2].k - b.links[2].k) < 1e-6 && std::fabs(a.gravity - b.gravity) < 1e-9;
        std::remove(p.c_str());
        check("design file round trip", ok, "");
    }
    {   // 7) the video encoder
        std::string p = recordingsDir() + "/_selftest.mp4", err;
        VideoEncoder e;
        bool ok = e.open(p, 320, 240, 30, err);
        std::vector<unsigned char> px(320 * 240 * 4, 128);
        for (int f = 0; ok && f < 30; ++f) ok = e.addFrame(px.data(), 320 * 4);
        ok = e.close() && ok;
        std::ifstream in(p, std::ios::binary | std::ios::ate);
        long long size = in ? (long long)in.tellg() : 0;
        in.close();
        std::remove(p.c_str());
        std::snprintf(d, sizeof d, "%s (%lld bytes)", ok ? "30 frames written" : err.c_str(), size);
        check("H.264 video encoder", ok && size > 1000, d);
    }
    {   // 8) design a system from scratch using only (simulated) mouse clicks, then run it
        W = 1320; H = 800;
        resetView();
        A.model = lag::Model();
        A.mode = MODE_EDIT; A.selKind = SEL_NONE; A.sel = -1; A.linkStart = -1;
        auto click = [](double wx, double wy) { onMouseDown(toScreenX(wx), toScreenY(wy)); onMouseUp(); };
        doAction(ACT_TOOL_GROUND); click(-2, 0);                            // a wall
        doAction(ACT_TOOL_MASS);   click(1, 0);                             // a block
        doAction(ACT_TOOL_SPRING); click(-2, 0); click(1, 0);               // spring wall -> block
        doAction(ACT_TOOL_DAMPER); click(-2, 0); click(1, 0);               // damper wall -> block
        doAction(ACT_TOOL_SELECT); click(1, 0);                             // select the block
        bool built = A.model.nodes.size() == 2 && A.model.links.size() == 2 && A.selKind == SEL_NODE;
        A.model.nodes[1].rail = lag::RAIL_X;
        A.model.nodes[1].vx0 = 2.0;
        A.model.links[0].k = 25;
        startRun();
        for (int i = 0; i < 600; ++i) advanceSim(1.0 / 60);                  // 10 s at 60 frames per second
        bool ran = A.mode == MODE_RUN && A.st.t > 9.9 && A.log.size() > 500 && std::fabs(A.meter.residual()) < 1e-4;
        // a click on empty space with the Spring tool must not crash or connect anything
        stopRun();
        doAction(ACT_TOOL_SPRING); click(3.3, 1.1);
        std::snprintf(d, sizeof d, "nodes %zu, links %zu, t = %.2f s, energy error %.1e J", A.model.nodes.size(), A.model.links.size(), A.st.t, std::fabs(A.meter.residual()));
        check("design by clicking, then run", built && ran, d);
    }
    std::printf(gFail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", gFail);
    return gFail ? 1 : 0;
}

// ------------------------------------------------------------------ headless picture / video (for testing and for the book)
static int pngEncoder(CLSID* clsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (!size) return -1;
    std::vector<char> buf(size);
    ImageCodecInfo* info = (ImageCodecInfo*)buf.data();
    GetImageEncoders(num, size, info);
    for (UINT i = 0; i < num; ++i)
        if (wcscmp(info[i].MimeType, L"image/png") == 0) { *clsid = info[i].Clsid; return (int)i; }
    return -1;
}
static bool savePng(const char* path) {
    Bitmap bm(canvas.w, canvas.h, canvas.w * 4, PixelFormat32bppRGB, (BYTE*)canvas.bits);
    CLSID png;
    pngEncoder(&png);
    WCHAR wp[512];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 512);
    return bm.Save(wp, &png, nullptr) == Ok;
}

struct CliOptions { int preset = 0; double runSeconds = 0; std::string select; int tab = 0; bool empty = false; };
static CliOptions parseOptions(int argc, char** argv) {
    CliOptions o;
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--preset") o.preset = std::atoi(argv[i + 1]);
        else if (a == "--run") o.runSeconds = std::atof(argv[i + 1]);
        else if (a == "--select") o.select = argv[i + 1];
        else if (a == "--tab") o.tab = std::atoi(argv[i + 1]);
        else if (a == "--empty") o.empty = std::atoi(argv[i + 1]) != 0;
    }
    return o;
}
static void applyOptions(const CliOptions& o) {
    if (o.preset > 0) A.model = lag::presetByNumber(o.preset);
    if (o.empty) A.model = lag::Model();
    A.plotMode = o.tab;
    if (o.select.rfind("node:", 0) == 0) { A.selKind = SEL_NODE; A.sel = std::atoi(o.select.c_str() + 5); }
    if (o.select.rfind("link:", 0) == 0) { A.selKind = SEL_LINK; A.sel = std::atoi(o.select.c_str() + 5); }
}
static void stepOffscreen(double seconds) {            // advance exactly like the recorder does: 1/60 s per frame
    for (int i = 0; i < (int)(seconds * REC_FPS); ++i) {
        advanceSim(1.0 / REC_FPS);
        if (i % 18 == 0) updateDominantFrequency();
    }
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) return selfTest();

    GdiplusStartupInput gsi; ULONG_PTR tok;
    GdiplusStartup(&tok, &gsi, nullptr);
    gMono = new FontFamily(L"Consolas");
    fSmall = new Font(gMono, 12, FontStyleRegular, UnitPixel);
    fNorm  = new Font(gMono, 14, FontStyleBold, UnitPixel);
    fBig   = new Font(gMono, 20, FontStyleBold, UnitPixel);

    if (!lag::loadModel(designPath(), A.model)) A.model = lag::presetTwoDof();     // last session's design, or an example
    resetView();

    int rc = 0;
    if (argc > 2 && (std::strcmp(argv[1], "--shot") == 0 || std::strcmp(argv[1], "--demo") == 0)) {
        bool video = std::strcmp(argv[1], "--demo") == 0;
        CliOptions o = parseOptions(argc, argv);
        W = 1320; H = 800;
        if (o.preset == 0 && !video) o.preset = 2;
        if (o.preset == 0) o.preset = 4;
        resetView();
        applyOptions(o);
        A.mx = A.my = -100;
        if (video) {
            std::string err;
            recW = (W - PANEL_W) & ~1; recH = H & ~1;
            if (!encoder.open(argv[2], recW, recH, REC_FPS, err)) { std::printf("cannot record: %s\n", err.c_str()); rc = 1; }
            else {
                startRun();
                A.recording = true;
                int frames = (int)(std::fmax(1.0, o.runSeconds) * REC_FPS);
                for (int i = 0; i < frames; ++i) {
                    advanceSim(1.0 / REC_FPS);
                    if (i % 18 == 0) updateDominantFrequency();
                    drawFrameToCanvas();
                    captureFrame();
                }
                A.recording = false;
                bool ok = encoder.close();
                std::printf("demo %s: %d frames (%s)\n", argv[2], A.recFrames, ok ? "ok" : "FAILED");
                rc = ok ? 0 : 1;
            }
        } else {
            if (o.runSeconds > 0) { startRun(); stepOffscreen(o.runSeconds); }
            drawFrameToCanvas();
            bool ok = savePng(argv[2]);
            std::printf("shot %s (%s)\n", argv[2], ok ? "ok" : "FAILED");
            rc = ok ? 0 : 1;
        }
    } else {
        std::printf("Vibration Lab: a window has opened. Design a system, press Run.\n");
        WNDCLASSA wc{};
        wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = "VibLab";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassA(&wc);
        RECT r = {0, 0, W, H};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        gWnd = CreateWindowA("VibLab", "NOVA Vibration Lab  -  design, simulate, record", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
        SetForegroundWindow(gWnd);
        double last = nowSeconds(), fpsT = last, domT = last;
        int frames = 0;
        MSG msg;
        bool running = true;
        while (running) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) running = false;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            double now = nowSeconds(), dt = clampd(now - last, 0.0, 0.05);
            last = now;
            // while recording, every video frame is exactly 1/60 s of simulated time, so the movie is smooth and true-to-life
            advanceSim(A.recording ? 1.0 / REC_FPS : dt);
            if (now - domT > 0.3) { updateDominantFrequency(); domT = now; }
            present();
            captureFrame();
            ++frames;
            if (now - fpsT >= 0.5) { A.fps = frames / (now - fpsT); frames = 0; fpsT = now; }
            if (A.recording) {                                    // pace to the video frame rate so dragging feels natural
                double spent = nowSeconds() - now;
                if (spent < 1.0 / REC_FPS) Sleep((DWORD)((1.0 / REC_FPS - spent) * 1000));
            } else Sleep(1);
        }
    }
    canvas.destroy();
    GdiplusShutdown(tok);
    return rc;
}
