// ui_plots.cpp -- the Telemetry panel: time-series plots, energy ledger, trajectories, spectrum, statistics, modal analysis, frequency response.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "app.hpp"
#include "implot.h"

namespace vlab {

static bool channelCombo(const char* id, const Telemetry& t, int* idx) {
    if (t.channels.empty()) return false;
    *idx = std::clamp(*idx, 0, (int)t.channels.size() - 1);
    bool changed = false;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(id, t.channels[*idx].name.c_str())) {
        for (int i = 0; i < (int)t.channels.size(); ++i)
            if (ImGui::Selectable(t.channels[i].name.c_str(), i == *idx)) { *idx = i; changed = true; }
        ImGui::EndCombo();
    }
    return changed;
}

static bool bodyCombo(const char* id, const Model& m, int* idx, bool dynamicOnly) {
    if (m.bodies.empty()) return false;
    *idx = std::clamp(*idx, 0, (int)m.bodies.size() - 1);
    bool changed = false;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(id, m.bodies[*idx].name.c_str())) {
        for (int i = 0; i < (int)m.bodies.size(); ++i) {
            if (dynamicOnly && !m.bodies[i].isDynamic()) continue;
            if (ImGui::Selectable(m.bodies[i].name.c_str(), i == *idx)) { *idx = i; changed = true; }
        }
        ImGui::EndCombo();
    }
    return changed;
}

static void plotSeries(const char* name, const std::vector<double>& x, const std::vector<double>& y) {
    if (x.empty() || y.size() != x.size()) return;
    int step = std::max<int>(1, (int)(x.size() / 6000));
    int count = (int)((x.size() + step - 1) / step);
    ImPlot::PlotLine(name, x.data(), y.data(), count, ImPlotSpec(ImPlotProp_Stride, (int)(sizeof(double) * step)));
}

// range (max - min) of a column
static double columnRange(const std::vector<double>& v) {
    if (v.empty()) return 0;
    auto mm = std::minmax_element(v.begin(), v.end());
    return *mm.second - *mm.first;
}
static bool endsWith(const std::string& s, const char* suf) {
    size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}
// the displacement or angle channel that moves the most (so a plot never opens on a flat line)
static int mostActiveChannel(const Telemetry& t) {
    int best = -1;
    double bestRange = 0;
    for (size_t c = 0; c < t.channels.size(); ++c) {
        const std::string& n = t.channels[c].name;
        if (!(endsWith(n, " dx") || endsWith(n, " dy") || endsWith(n, " angle") || endsWith(n, " deflection"))) continue;
        double r = columnRange(t.col[c]) * (endsWith(n, " angle") ? 0.017 : 1.0);
        if (r > bestRange) { bestRange = r; best = (int)c; }
    }
    return best;
}

// ---- steady axes. Auto-fit in ImPlot collapses to a zero-width range when there is no data or the data is a flat line, and then
// re-fits differently every frame: the whole graph flickers (a hazard for photosensitive users). So the range is measured here first;
// a degenerate one gets fixed, sensible limits instead of auto-fit.
struct Rng {
    double lo = 1e300, hi = -1e300;
    void add(double v) { if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); } }
    void add(const std::vector<double>& v) { for (double x : v) add(x); }
    bool none() const { return hi < lo; }
    bool flat() const { return none() || hi - lo <= 1e-12 * (1.0 + 0.5 * std::fabs(hi + lo)); }
};
static void setupAxis(ImAxis ax, const char* label, const Rng& r, ImPlotAxisFlags extra = 0, bool fit = true, double defLo = 0, double defHi = 1) {
    if (r.flat()) {
        ImPlot::SetupAxis(ax, label, extra);
        double c = r.none() ? 0.5 * (defLo + defHi) : 0.5 * (r.lo + r.hi);
        double half = r.none() ? 0.5 * (defHi - defLo) : (std::fabs(c) * 0.05 > 0 ? std::fabs(c) * 0.05 : 1.0);
        ImPlot::SetupAxisLimits(ax, c - half, c + half, ImPlotCond_Always);
    } else {
        ImPlot::SetupAxis(ax, label, fit ? (extra | ImPlotAxisFlags_AutoFit) : extra);
    }
}

static void quickPick(App& a, int which) {
    const Telemetry& t = a.ses.telemetry();
    a.plotSel.assign(t.channels.size(), 0);
    int n = 0;
    for (size_t c = 0; c < t.channels.size() && n < 12; ++c) {
        const Channel& ch = t.channels[c];
        bool take = false;
        if (which == 0) take = ch.name.size() > 3 && (ch.name.compare(ch.name.size() - 3, 3, " dx") == 0 || ch.name.compare(ch.name.size() - 3, 3, " dy") == 0);
        if (which == 1) take = ch.name.size() > 3 && (ch.name.compare(ch.name.size() - 3, 3, " vx") == 0 || ch.name.compare(ch.name.size() - 3, 3, " vy") == 0);
        if (which == 2) take = ch.group == "Energy" && (ch.name.find("T kinetic") != std::string::npos || ch.name.find("V potential") != std::string::npos || ch.name.find("E total") != std::string::npos);
        if (which == 3) take = ch.unit == "N";
        if (which == 4) take = ch.unit == "deg";
        if (take) { a.plotSel[c] = 1; ++n; }
    }
}

// ---------------------------------------------------------------------------------------------- tabs
static void tabTimeSeries(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() == 0) { ImGui::TextDisabled("Run the model (Run panel) and the data appears here."); return; }
    if (a.plotSel.size() != t.channels.size()) a.plotSel.assign(t.channels.size(), 0);
    if (!a.plotPicked && t.rows() >= 3) {                       // first data of this run: tick the movements that are not flat
        static size_t lastScan = 0;
        if (t.rows() != lastScan) {
            lastScan = t.rows();
            std::vector<char> pick(t.channels.size(), 0);
            int n = 0;
            for (size_t c = 0; c < t.channels.size() && n < 6; ++c) {
                const std::string& nm = t.channels[c].name;
                if (!(endsWith(nm, " dx") || endsWith(nm, " dy"))) continue;
                double rg = columnRange(t.col[c]);
                bool drift = rg > 0.5 && std::fabs(t.col[c].back() - t.col[c].front()) > 0.9 * rg;     // a body just travelling: not a vibration
                if (rg > 1e-12 && !drift) { pick[c] = 1; ++n; }
            }
            if (n == 0)                                                      // everything travels: then show the travel
                for (size_t c = 0; c < t.channels.size() && n < 3; ++c)
                    if ((endsWith(t.channels[c].name, " dx") || endsWith(t.channels[c].name, " dy")) && columnRange(t.col[c]) > 1e-12) { pick[c] = 1; ++n; }
            if (n > 0) { a.plotSel = pick; a.plotPicked = true; }
        }
    }
    ImGui::BeginChild("channels", ImVec2(230, 0), true);
    ImGui::TextDisabled("Pick what to plot");
    const char* names[] = {"Positions", "Velocities", "Energy", "Forces", "Angles"};
    for (int i = 0; i < 5; ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::SmallButton(names[i])) quickPick(a, i);
    }
    if (ImGui::SmallButton("Clear")) a.plotSel.assign(t.channels.size(), 0);
    ImGui::Separator();
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
            bool v = a.plotSel[c] != 0;
            ImGui::PushID((int)c);
            if (ImGui::Checkbox(label.c_str(), &v)) a.plotSel[c] = v ? 1 : 0;
            ImGui::PopID();
        }
    }
    if (open) ImGui::TreePop();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Checkbox("Auto-fit axes", &a.plotAutoFit);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu rows, %.2f s of data. Scroll to zoom, drag to pan, double-click to fit.", t.rows(), t.t.back());
    std::string unit;
    bool mixed = false;
    int shown = 0;
    for (size_t c = 0; c < t.channels.size(); ++c)
        if (a.plotSel[c]) { ++shown; if (unit.empty()) unit = t.channels[c].unit; else if (unit != t.channels[c].unit) mixed = true; }
    Rng tsX, tsY;
    if (shown > 0) { tsX.add(t.t.front()); tsX.add(t.t.back()); for (size_t c = 0; c < t.channels.size(); ++c) if (a.plotSel[c]) tsY.add(t.col[c]); }
    if (ImPlot::BeginPlot("##ts", ImVec2(-1, -1))) {
        setupAxis(ImAxis_X1, "Time (s)", tsX, 0, a.plotAutoFit);
        setupAxis(ImAxis_Y1, mixed ? "mixed units" : unit.c_str(), tsY, 0, a.plotAutoFit);
        for (size_t c = 0; c < t.channels.size(); ++c)
            if (a.plotSel[c]) plotSeries(t.channels[c].name.c_str(), t.t, t.col[c]);
        if (shown == 0) ImPlot::PlotText("Tick channels on the left to plot them", 0.5, 0.5);
        ImPlot::EndPlot();
    }
    ImGui::EndGroup();
}

static void tabEnergy(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() == 0) { ImGui::TextDisabled("Run the model to see where the energy goes."); return; }
    auto ch = [&](const char* n) { return t.find(std::string("Energy ") + n); };
    float h = (ImGui::GetContentRegionAvail().y - 4) / 2;
    float w = ImGui::GetContentRegionAvail().x * 0.62f;
    Rng eX, eY1, eY3;
    eX.add(t.t.front()); eX.add(t.t.back());
    for (const char* n : {"T kinetic", "V potential", "E total"}) { int c = ch(n); if (c >= 0) eY1.add(t.col[c]); }
    for (const char* n : {"work in", "lost viscous", "lost drag", "lost friction", "lost impact", "lost total"}) { int c = ch(n); if (c >= 0) eY3.add(t.col[c]); }
    if (ImPlot::BeginPlot("Energy of the system", ImVec2(w, h))) {
        setupAxis(ImAxis_X1, "Time (s)", eX);
        setupAxis(ImAxis_Y1, "J", eY1);
        const char* n[3] = {"T kinetic", "V potential", "E total"};
        for (int i = 0; i < 3; ++i) { int c = ch(n[i]); if (c >= 0) plotSeries(n[i], t.t, t.col[c]); }
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    // final ledger as bars
    {
        const char* labels[] = {"work in", "dampers", "air drag", "friction", "impacts"};
        const char* src[] = {"work in", "lost viscous", "lost drag", "lost friction", "lost impact"};
        double vals[5];
        for (int i = 0; i < 5; ++i) { int c = ch(src[i]); vals[i] = c >= 0 ? t.col[c].back() : 0; }
        if (ImPlot::BeginPlot("Where the energy went (end of run)", ImVec2(-1, h))) {
            Rng bY;
            bY.add(0.0);
            for (double v : vals) bY.add(v);
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
            ImPlot::SetupAxisLimits(ImAxis_X1, -0.7, 4.7, ImPlotCond_Always);
            setupAxis(ImAxis_Y1, "J", bY);
            double ticks[5] = {0, 1, 2, 3, 4};
            ImPlot::SetupAxisTicks(ImAxis_X1, ticks, 5, labels);
            ImPlot::PlotBars("joules", vals, 5, 0.6);
            ImPlot::EndPlot();
        }
    }
    if (ImPlot::BeginPlot("Energy ledger (cumulative)", ImVec2(w, -1))) {
        setupAxis(ImAxis_X1, "Time (s)", eX);
        setupAxis(ImAxis_Y1, "J", eY3);
        const char* n[6] = {"work in", "lost viscous", "lost drag", "lost friction", "lost impact", "lost total"};
        for (int i = 0; i < 6; ++i) { int c = ch(n[i]); if (c >= 0) plotSeries(n[i], t.t, t.col[c]); }
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    ImGui::BeginChild("ledgertext", ImVec2(-1, -1), true);
    auto last = [&](const char* n) { int c = ch(n); return c >= 0 ? t.col[c].back() : 0.0; };
    double e0 = t.col[ch("E total")].front(), e1 = last("E total"), win = last("work in"), lost = last("lost total"), num = last("numerical error");
    ImGui::TextUnformatted("The law of energy");
    ImGui::TextWrapped("E(end) = E(start) + work in - energy lost");
    ImGui::Separator();
    ImGui::Text("E start         %10.4f J", e0);
    ImGui::Text("+ work in       %10.4f J", win);
    ImGui::Text("- lost          %10.4f J", lost);
    ImGui::Text("= expected      %10.4f J", e0 + win - lost);
    ImGui::Text("E end (actual)  %10.4f J", e1);
    ImGui::Separator();
    ImGui::Text("numerical error %10.2e J", num);
    ImGui::TextDisabled("(this should be tiny: it is the program checking itself)");
    ImGui::Spacing();
    ImGui::TextDisabled("lost to dampers %.4f, air drag %.4f,\nfriction %.4f, impacts %.4f", last("lost viscous"), last("lost drag"), last("lost friction"), last("lost impact"));
    ImGui::EndChild();
}

static void tabPath(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() == 0) { ImGui::TextDisabled("Run the model to see paths and phase portraits."); return; }
    ImGui::SetNextItemWidth(220);
    const Model& m = a.runModel;
    ImGui::TextUnformatted("Body:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    bodyCombo("##pathbody", m, &a.phaseBody, true);
    const std::string nm = m.bodies.empty() ? "" : m.bodies[a.phaseBody].name;
    int cx = t.find(nm + " x"), cy = t.find(nm + " y"), cdx = t.find(nm + " dx"), cdy = t.find(nm + " dy"), cvx = t.find(nm + " vx"), cvy = t.find(nm + " vy");
    if (cx < 0) { ImGui::TextDisabled("Pick a moving body."); return; }
    float w = (ImGui::GetContentRegionAvail().x - 8) / 2;
    if (ImPlot::BeginPlot("Path of the body (x, y)  y points down", ImVec2(w, -1), ImPlotFlags_Equal)) {
        Rng px, py;
        px.add(t.col[cx]); py.add(t.col[cy]);
        setupAxis(ImAxis_X1, "x (m)", px);
        setupAxis(ImAxis_Y1, "y (m)", py, ImPlotAxisFlags_Invert);
        plotSeries("path", t.col[cx], t.col[cy]);
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    if (ImPlot::BeginPlot("Phase portrait: velocity vs displacement", ImVec2(-1, -1))) {
        bool movesX = false, movesY = false;
        for (double v : t.col[cdx]) if (std::fabs(v) > 1e-9) { movesX = true; break; }
        for (double v : t.col[cdy]) if (std::fabs(v) > 1e-9) { movesY = true; break; }
        Rng phx, phy;
        if (movesX) { phx.add(t.col[cdx]); phy.add(t.col[cvx]); }
        if (movesY) { phx.add(t.col[cdy]); phy.add(t.col[cvy]); }
        setupAxis(ImAxis_X1, "displacement (m)", phx);
        setupAxis(ImAxis_Y1, "velocity (m/s)", phy);
        if (movesX) plotSeries("x", t.col[cdx], t.col[cvx]);
        if (movesY) plotSeries("y", t.col[cdy], t.col[cvy]);
        ImPlot::EndPlot();
    }
}

static void tabSpectrum(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() < 16) { ImGui::TextDisabled("Run the model for a few seconds first."); return; }
    a.specChannel = std::clamp(a.specChannel, 0, (int)t.channels.size() - 1);
    if (!a.specPicked || columnRange(t.col[a.specChannel]) < 1e-12) {          // never open on a signal that does not move
        int best = mostActiveChannel(t);
        if (best >= 0 && best != a.specChannel && columnRange(t.col[a.specChannel]) < 1e-12) { a.specChannel = best; a.haveSpectrum = false; }
        if (best >= 0) a.specPicked = true;
    }
    static double lastCompute = -10;
    const bool running = a.state == RunState::Running;
    if (t.rows() >= 16 && (!a.haveSpectrum || (running && a.now - lastCompute > 1.0))) { a.computeSpectrum(); lastCompute = a.now; }   // automatic: no button press needed
    ImGui::TextUnformatted("Signal to analyse:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260);
    if (channelCombo("##specch", t, &a.specChannel)) { a.haveSpectrum = false; a.specPicked = true; }
    ImGui::SameLine();
    if (ImGui::Button("Recompute")) a.computeSpectrum();
    if (a.haveSpectrum) {
        ImGui::SameLine();
        ImGui::Text("Strongest frequency: %.4f Hz  (%.4f rad/s)   amplitude %.4g", a.spectrum.peakHz, a.spectrum.peakHz * 2 * kPi, a.spectrum.peakAmp);
        double z = estimateDampingRatio(t.t, t.col[std::clamp(a.specChannel, 0, (int)t.channels.size() - 1)]);
        if (z > 0) { ImGui::SameLine(); ImGui::Text("  Damping ratio from the decay: %.4f", z); }
        if (a.spectrum.peakAmp < 1e-14) ImGui::TextColored(ImVec4(1, 0.85f, 0.5f, 1), "This signal does not vibrate (it is constant). Pick another channel above, for example a body's dx or dy.");
    }
    Rng spY;
    if (a.haveSpectrum && a.spectrum.peakAmp > 1e-14) { spY.add(0.0); spY.add(a.spectrum.amp); }
    if (ImPlot::BeginPlot("Frequency content", ImVec2(-1, -1))) {
        ImPlot::SetupAxis(ImAxis_X1, "Frequency (Hz)");
        setupAxis(ImAxis_Y1, "Amplitude", spY);
        if (!a.haveSpectrum) ImPlot::SetupAxisLimits(ImAxis_X1, 0, 1, ImPlotCond_Always);
        if (a.haveSpectrum) {
            double fmax = std::min(30.0, a.spectrum.freq.empty() ? 30.0 : a.spectrum.freq.back());
            double pk = a.spectrum.peakHz * 4;
            if (pk > 2 && pk < fmax) fmax = pk;
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, fmax, ImPlotCond_Once);
            ImPlot::PlotLine("spectrum", a.spectrum.freq.data(), a.spectrum.amp.data(), (int)a.spectrum.freq.size());
            double px[2] = {a.spectrum.peakHz, a.spectrum.peakHz}, py[2] = {0, a.spectrum.peakAmp};
            ImPlot::PlotLine("peak", px, py, 2);
            if (a.haveModal)
                for (const Mode& md : a.modal.modes) { double mx[2] = {md.freqHz, md.freqHz}, my[2] = {0, a.spectrum.peakAmp * 0.6}; ImPlot::PlotLine("natural freq.", mx, my, 2); }
        } else {
            ImPlot::PlotText("Press 'Compute spectrum'", 0.5, 0.5);
        }
        ImPlot::EndPlot();
    }
}

static void tabStats(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() == 0) { ImGui::TextDisabled("Run the model to see statistics."); return; }
    static std::vector<Stats> cache;
    static size_t cacheRows = 0, cacheCh = 0;
    static double cacheAt = -10;
    if (cacheRows != t.rows() && (a.now - cacheAt > 0.5 || a.state != RunState::Running) || cacheCh != t.channels.size() || cache.size() != t.channels.size()) {
        cache.resize(t.channels.size());
        for (size_t c = 0; c < t.channels.size(); ++c) cache[c] = computeStats(t.t, t.col[c]);
        cacheRows = t.rows();
        cacheCh = t.channels.size();
        cacheAt = a.now;
    }
    if (ImGui::BeginTable("stats", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        const char* heads[] = {"Channel", "Unit", "Minimum", "Maximum", "Mean", "RMS", "Final", "Time of max (s)"};
        for (const char* h : heads) ImGui::TableSetupColumn(h);
        ImGui::TableHeadersRow();
        for (size_t c = 0; c < t.channels.size(); ++c) {
            const Stats& s = cache[c];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.channels[c].name.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.channels[c].unit.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.5g", s.min);
            ImGui::TableNextColumn(); ImGui::Text("%.5g", s.max);
            ImGui::TableNextColumn(); ImGui::Text("%.5g", s.mean);
            ImGui::TableNextColumn(); ImGui::Text("%.5g", s.rms);
            ImGui::TableNextColumn(); ImGui::Text("%.5g", s.last);
            ImGui::TableNextColumn(); ImGui::Text("%.4g", s.tMax);
        }
        ImGui::EndTable();
    }
}

static void tabModal(App& a) {
    if (ImGui::Button("Compute natural frequencies and mode shapes")) a.computeModal();
    ImGui::SameLine();
    ImGui::TextDisabled("Linearises the springs and dampers about the drawn pose (contacts are ignored).");
    if (!a.haveModal) { ImGui::TextDisabled("Press the button. The mode shapes can then be animated on the canvas."); return; }
    if (!a.modal.note.empty()) ImGui::TextColored(ImVec4(1, 0.85f, 0.5f, 1), "%s", a.modal.note.c_str());
    if (a.modal.modes.empty()) return;
    if (ImGui::BeginTable("modes", 3 + (int)std::min<size_t>(a.modal.dofNames.size(), 8), ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Mode");
        ImGui::TableSetupColumn("Frequency (Hz)");
        ImGui::TableSetupColumn("Damping ratio");
        for (size_t d = 0; d < a.modal.dofNames.size() && d < 8; ++d) ImGui::TableSetupColumn(a.modal.dofNames[d].c_str());
        ImGui::TableHeadersRow();
        for (size_t k = 0; k < a.modal.modes.size(); ++k) {
            const Mode& md = a.modal.modes[k];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char lab[32];
            std::snprintf(lab, sizeof lab, "Mode %zu", k + 1);
            if (ImGui::Selectable(lab, a.modeShown == (int)k, ImGuiSelectableFlags_SpanAllColumns)) a.modeShown = (int)k;
            ImGui::TableNextColumn(); ImGui::Text("%.4f", md.freqHz);
            ImGui::TableNextColumn(); ImGui::Text("%.4f", md.dampingRatio);
            for (size_t d = 0; d < md.shape.size() && d < 8; ++d) { ImGui::TableNextColumn(); ImGui::Text("%+.3f", md.shape[d]); }
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Click a mode to animate it on the canvas (in design mode). Shape values are scaled so the largest is 1.");
}

static void tabFrf(App& a) {
    Model& m = a.design();
    if (m.dynamicCount() == 0) { ImGui::TextDisabled("Add a moving body first."); return; }
    {   // the push and the response must be moving bodies, otherwise the sweep measures nothing
        int firstDyn = 0;
        for (int i = 0; i < (int)m.bodies.size(); ++i) if (m.bodies[i].isDynamic()) { firstDyn = i; break; }
        for (int* idx : {&a.frfReq.driveBody, &a.frfReq.responseBody})
            if (*idx < 0 || *idx >= (int)m.bodies.size() || !m.bodies[*idx].isDynamic()) *idx = firstDyn;
    }
    ImGui::BeginChild("frfctl", ImVec2(260, 0), true);
    ImGui::TextUnformatted("Push (the input)");
    bodyCombo("##fdrive", m, &a.frfReq.driveBody, true);
    double ang = a.frfReq.driveAngleDeg, amp = a.frfReq.forceAmp;
    ImGui::SetNextItemWidth(-1);
    ImGui::InputDouble("##fang", &ang, 5, 15, "direction %.0f deg");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputDouble("##famp", &amp, 0.5, 5, "force %.2f N");
    a.frfReq.driveAngleDeg = ang;
    a.frfReq.forceAmp = std::max(1e-6, amp);
    ImGui::TextUnformatted("Response (the output)");
    bodyCombo("##fresp", m, &a.frfReq.responseBody, true);
    const char* comps[] = {"x displacement", "y displacement", "rotation angle"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##fcomp", &a.frfReq.component, comps, 3);
    double f0 = a.frfReq.f0, f1 = a.frfReq.f1, settle = a.frfReq.settleSeconds;
    ImGui::SetNextItemWidth(-1);
    ImGui::InputDouble("##f0", &f0, 0.1, 1, "from %.2f Hz");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputDouble("##f1", &f1, 0.1, 1, "to %.2f Hz");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##fpts", &a.frfReq.points, 5, 200, "%d frequencies");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputDouble("##fsettle", &settle, 1, 5, "settle %.1f s");
    a.frfReq.f0 = std::max(0.01, f0);
    a.frfReq.f1 = std::max(a.frfReq.f0 + 0.01, f1);
    a.frfReq.settleSeconds = std::max(1.0, settle);
    if (a.frfBusy) {
        ImGui::ProgressBar(a.frfProgress, ImVec2(-1, 0));
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) a.frfCancel = true;
    } else {
        if (ImGui::Button("Run frequency sweep", ImVec2(-1, 30))) a.startFrf();
    }
    ImGui::TextDisabled("Each frequency is a full steady-state simulation.");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    float h = (ImGui::GetContentRegionAvail().y - 4) / 2;
    Rng frX, frA, frP;
    if (a.haveFrf) for (const FrfPoint& p : a.frf) { frX.add(p.freqHz); frA.add(p.amplitude); frP.add(p.phaseDeg); }
    if (frA.hi < 1e-14 && !frA.none()) frA.add(1.0);                                  // nothing responds: show a calm 0..1 axis
    if (ImPlot::BeginPlot("Amplitude of the response", ImVec2(-1, h))) {
        setupAxis(ImAxis_X1, "Frequency (Hz)", frX);
        setupAxis(ImAxis_Y1, "Amplitude", frA);
        if (a.haveFrf) {
            std::vector<double> fx, fy;
            for (const FrfPoint& p : a.frf) { fx.push_back(p.freqHz); fy.push_back(p.amplitude); }
            ImPlot::PlotLine("amplitude", fx.data(), fy.data(), (int)fx.size());
            if (a.haveModal) for (const Mode& md : a.modal.modes) { double mx[2] = {md.freqHz, md.freqHz}, my[2] = {0, *std::max_element(fy.begin(), fy.end())}; ImPlot::PlotLine("natural freq.", mx, my, 2); }
            if (!fy.empty() && *std::max_element(fy.begin(), fy.end()) < 1e-14) ImPlot::PlotText("No response: that body does not move in that direction. Change the response body or component.", 0.5, 0.5);
        } else ImPlot::PlotText("Run a frequency sweep", 0.5, 0.5);
        ImPlot::EndPlot();
    }
    if (ImPlot::BeginPlot("Phase lag", ImVec2(-1, -1))) {
        setupAxis(ImAxis_X1, "Frequency (Hz)", frX);
        setupAxis(ImAxis_Y1, "degrees", frP);
        if (a.haveFrf) {
            std::vector<double> fx, fy;
            for (const FrfPoint& p : a.frf) { fx.push_back(p.freqHz); fy.push_back(p.phaseDeg); }
            ImPlot::PlotLine("phase", fx.data(), fy.data(), (int)fx.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::EndGroup();
}

// How each body deformed: beams bend (deflection curve), everything else squashes (compression).
static void tabDeform(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() == 0 || t.deform.empty()) { ImGui::TextDisabled("Run the model (Run panel): the bending of every beam and the squashing of every moving body appears here."); return; }
    a.deformBody = std::clamp(a.deformBody, 0, (int)t.deform.size() - 1);
    ImGui::TextUnformatted("Body:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("##defbody", t.deform[a.deformBody].name.c_str())) {
        for (int i = 0; i < (int)t.deform.size(); ++i)
            if (ImGui::Selectable(t.deform[i].name.c_str(), i == a.deformBody)) { a.deformBody = i; a.deformRow = -1; }
        ImGui::EndCombo();
    }
    const DeformSeries& ds = t.deform[a.deformBody];
    const std::vector<double>& dcol = t.col[ds.chanDeform];
    const std::vector<double>& fcol = t.col[ds.chanLoad];
    // peak of |deformation| and where it happened
    size_t peakRow = 0;
    double peak = 0;
    for (size_t r = 0; r < dcol.size(); ++r) if (std::fabs(dcol[r]) > peak) { peak = std::fabs(dcol[r]); peakRow = r; }
    double fpeak = 0;
    for (double v : fcol) fpeak = std::max(fpeak, v);
    const double unitScale = peak < 1e-7 ? 1e9 : (peak < 1e-4 ? 1e6 : 1e3);          // nm, um or mm
    const char* unit = peak < 1e-7 ? "nm" : (peak < 1e-4 ? "um" : "mm");
    int row = a.deformRow < 0 ? (int)peakRow : std::clamp(a.deformRow, 0, (int)t.rows() - 1);
    ImGui::SameLine();
    ImGui::TextDisabled("  E = %.3g GPa    peak %s: %.4g %s at t = %.2f s    peak contact force %.1f N", ds.E / 1e9, ds.beam ? "deflection" : "compression", peak * unitScale, unit, t.t[peakRow], fpeak);
    if (ds.beam && ds.rows() > 0) {
        double sigmaPeak = 0;
        for (size_t r = 0; r < ds.rows(); ++r) sigmaPeak = std::max(sigmaPeak, bendingStress(ds.fe, ds.w.data() + r * ds.fe.x.size()));
        ImGui::Text("Span / deflection = %.0f     peak bending stress %.3f MPa     (small-deflection beam theory, response to the load at each instant)",
                    peak > 1e-15 ? ds.length / peak : 0.0, sigmaPeak / 1e6);
    } else if (!ds.beam) {
        ImGui::TextDisabled("Not a beam: the squash is estimated as  delta = F L / (E A)  along the line of the largest contact load.");
    }
    // ---- left: deformation and force against time
    const float w = (ImGui::GetContentRegionAvail().x - 24) * (ds.beam ? 0.31f : 0.31f);
    ImGui::BeginGroup();
    {
        const int step = std::max<int>(1, (int)(t.t.size() / 4000));
        std::vector<double> xs, ys, fs;
        for (size_t r = 0; r < t.t.size(); r += step) { xs.push_back(t.t[r]); ys.push_back(dcol[r] * unitScale); fs.push_back(fcol[r]); }
        char title[96], ylab[32];
        std::snprintf(title, sizeof title, "%s: %s over time", ds.name.c_str(), ds.beam ? "deflection" : "compression");
        std::snprintf(ylab, sizeof ylab, "%s", unit);
        Rng dX, dY, dF;
        dX.add(xs); dY.add(ys); dF.add(fs);
        if (ImPlot::BeginPlot(title, ImVec2(w, -1))) {
            setupAxis(ImAxis_X1, "Time (s)", dX);
            setupAxis(ImAxis_Y1, ylab, dY);
            ImPlot::PlotLine(ds.beam ? "deflection" : "compression", xs.data(), ys.data(), (int)xs.size());
            double tsel = t.t[row];
            ImPlot::PlotInfLines("shown", &tsel, 1);
            ImPlot::EndPlot();
        }
        ImGui::SameLine();
        if (ImPlot::BeginPlot("Contact force on the body", ImVec2(w, -1))) {
            setupAxis(ImAxis_X1, "Time (s)", dX);
            setupAxis(ImAxis_Y1, "N", dF);
            ImPlot::PlotLine("force", xs.data(), fs.data(), (int)xs.size());
            double tsel = t.t[row];
            ImPlot::PlotInfLines("shown", &tsel, 1);
            ImPlot::EndPlot();
        }
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    // ---- right: the deflection curve at the chosen instant (beams)
    ImGui::BeginGroup();
    if (ds.beam && ds.rows() > (size_t)row) {
        const int nodes = (int)ds.fe.x.size();
        std::vector<double> ws(nodes);
        for (int i = 0; i < nodes; ++i) ws[i] = ds.w[(size_t)row * nodes + i] * unitScale;
        char title[96], ylab[40];
        std::snprintf(title, sizeof title, "Shape of %s at t = %.3f s (true scale, down is positive)", ds.name.c_str(), t.t[row]);
        std::snprintf(ylab, sizeof ylab, "deflection (%s)", unit);
        if (ImPlot::BeginPlot(title, ImVec2(-1, ImGui::GetContentRegionAvail().y - 30))) {
            Rng sX, sY;
            sX.add(ds.fe.x.front()); sX.add(ds.fe.x.back()); sY.add(ws); sY.add(0.0);
            setupAxis(ImAxis_X1, "position along the beam (m)", sX);
            setupAxis(ImAxis_Y1, ylab, sY, ImPlotAxisFlags_Invert);
            double zx[2] = {ds.fe.x.front(), ds.fe.x.back()}, zy[2] = {0, 0};
            ImPlot::PlotLine("undeformed", zx, zy, 2);
            ImPlot::PlotLine("deflected", ds.fe.x.data(), ws.data(), nodes);
            ImPlot::EndPlot();
        }
        int r = row;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 150);
        if (ImGui::SliderInt("##defrow", &r, 0, (int)t.rows() - 1, "row %d")) a.deformRow = r;
        ImGui::SameLine();
        if (ImGui::Button("Go to the peak")) a.deformRow = -1;
    } else if (!ds.beam) {
        ImGui::BeginChild("squashtext", ImVec2(-1, -1), true);
        ImGui::TextUnformatted("Compression of a compact body");
        ImGui::Separator();
        ImGui::TextWrapped("The solver treats this body as rigid. Its squash is estimated from the largest contact load:");
        ImGui::TextWrapped("delta = F L / (E A)");
        ImGui::TextWrapped("F is the force pressing from one side, L the body's size along that line, A its cross-section there (area / L, 1 m deep) and E the material stiffness.");
        ImGui::Spacing();
        ImGui::Text("Young's modulus E   %.3g GPa", ds.E / 1e9);
        ImGui::Text("Peak compression    %.4g %s", peak * unitScale, unit);
        ImGui::Text("Peak contact force  %.1f N", fpeak);
        ImGui::Spacing();
        ImGui::TextDisabled("Set the stiffness in Properties (Material). Rubber is about 1000 times softer than steel.");
        ImGui::TextDisabled("On the canvas, use Show deformation to see it exaggerated.");
        ImGui::EndChild();
    } else {
        ImGui::TextDisabled("No deflection data for this row.");
    }
    ImGui::EndGroup();
}

// ---------------------------------------------------------------------------------------------- the panel
void App::drawTelemetry() {
    ImGui::Begin("Telemetry");
    if (ImGui::BeginTabBar("##teltabs")) {
        struct TabDef { const char* name; void (*fn)(App&); };
        static const TabDef tabs[] = {{"Plots", tabTimeSeries}, {"Energy ledger", tabEnergy}, {"Path and phase", tabPath}, {"Deformation", tabDeform}, {"Spectrum", tabSpectrum},
                                      {"Statistics", tabStats}, {"Modal analysis", tabModal}, {"Frequency response", tabFrf}};
        for (int i = 0; i < 8; ++i) {
            ImGuiTabItemFlags fl = (forceTab == i) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(tabs[i].name, nullptr, fl)) { tabs[i].fn(*this); ImGui::EndTabItem(); }
        }
        ImGui::EndTabBar();
    }
    forceTab = -1;
    ImGui::End();
}

}  // namespace vlab
