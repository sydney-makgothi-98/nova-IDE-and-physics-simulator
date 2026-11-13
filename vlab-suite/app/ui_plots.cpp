// ui_plots.cpp -- the Telemetry panel: time-series plots, energy ledger, trajectories, spectrum, statistics, modal analysis, frequency response.
#include <algorithm>
#include <cmath>
#include <cstdio>
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
    if (ImPlot::BeginPlot("##ts", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Time (s)", mixed ? "mixed units" : unit.c_str(), a.plotAutoFit ? ImPlotAxisFlags_AutoFit : 0, a.plotAutoFit ? ImPlotAxisFlags_AutoFit : 0);
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
    if (ImPlot::BeginPlot("Energy of the system", ImVec2(w, h))) {
        ImPlot::SetupAxes("Time (s)", "J", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
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
            ImPlot::SetupAxes(nullptr, "J", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_AutoFit);
            double ticks[5] = {0, 1, 2, 3, 4};
            ImPlot::SetupAxisTicks(ImAxis_X1, ticks, 5, labels);
            ImPlot::PlotBars("joules", vals, 5, 0.6);
            ImPlot::EndPlot();
        }
    }
    if (ImPlot::BeginPlot("Energy ledger (cumulative)", ImVec2(w, -1))) {
        ImPlot::SetupAxes("Time (s)", "J", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
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
        ImPlot::SetupAxes("x (m)", "y (m)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);
        plotSeries("path", t.col[cx], t.col[cy]);
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    if (ImPlot::BeginPlot("Phase portrait: velocity vs displacement", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("displacement (m)", "velocity (m/s)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        bool movesX = false, movesY = false;
        for (double v : t.col[cdx]) if (std::fabs(v) > 1e-9) { movesX = true; break; }
        for (double v : t.col[cdy]) if (std::fabs(v) > 1e-9) { movesY = true; break; }
        if (movesX) plotSeries("x", t.col[cdx], t.col[cvx]);
        if (movesY) plotSeries("y", t.col[cdy], t.col[cvy]);
        ImPlot::EndPlot();
    }
}

static void tabSpectrum(App& a) {
    const Telemetry& t = a.ses.telemetry();
    if (t.rows() < 16) { ImGui::TextDisabled("Run the model for a few seconds first."); return; }
    ImGui::TextUnformatted("Signal to analyse:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260);
    if (channelCombo("##specch", t, &a.specChannel)) a.haveSpectrum = false;
    ImGui::SameLine();
    if (ImGui::Button("Compute spectrum")) a.computeSpectrum();
    if (a.haveSpectrum) {
        ImGui::SameLine();
        ImGui::Text("Strongest frequency: %.4f Hz  (%.4f rad/s)", a.spectrum.peakHz, a.spectrum.peakHz * 2 * kPi);
        double z = estimateDampingRatio(t.t, t.col[std::clamp(a.specChannel, 0, (int)t.channels.size() - 1)]);
        if (z > 0) { ImGui::SameLine(); ImGui::Text("  Damping ratio from the decay: %.4f", z); }
    }
    if (ImPlot::BeginPlot("Frequency content", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Frequency (Hz)", "Amplitude", 0, ImPlotAxisFlags_AutoFit);
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
    if (ImPlot::BeginPlot("Amplitude of the response", ImVec2(-1, h))) {
        ImPlot::SetupAxes("Frequency (Hz)", "Amplitude", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        if (a.haveFrf) {
            std::vector<double> fx, fy;
            for (const FrfPoint& p : a.frf) { fx.push_back(p.freqHz); fy.push_back(p.amplitude); }
            ImPlot::PlotLine("amplitude", fx.data(), fy.data(), (int)fx.size());
            if (a.haveModal) for (const Mode& md : a.modal.modes) { double mx[2] = {md.freqHz, md.freqHz}, my[2] = {0, *std::max_element(fy.begin(), fy.end())}; ImPlot::PlotLine("natural freq.", mx, my, 2); }
        } else ImPlot::PlotText("Run a frequency sweep", 0.5, 0.5);
        ImPlot::EndPlot();
    }
    if (ImPlot::BeginPlot("Phase lag", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Frequency (Hz)", "degrees", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        if (a.haveFrf) {
            std::vector<double> fx, fy;
            for (const FrfPoint& p : a.frf) { fx.push_back(p.freqHz); fy.push_back(p.phaseDeg); }
            ImPlot::PlotLine("phase", fx.data(), fy.data(), (int)fx.size());
        }
        ImPlot::EndPlot();
    }
    ImGui::EndGroup();
}

// ---------------------------------------------------------------------------------------------- the panel
void App::drawTelemetry() {
    ImGui::Begin("Telemetry");
    if (ImGui::BeginTabBar("##teltabs")) {
        struct TabDef { const char* name; void (*fn)(App&); };
        static const TabDef tabs[] = {{"Plots", tabTimeSeries}, {"Energy ledger", tabEnergy}, {"Path and phase", tabPath}, {"Spectrum", tabSpectrum},
                                      {"Statistics", tabStats}, {"Modal analysis", tabModal}, {"Frequency response", tabFrf}};
        for (int i = 0; i < 7; ++i) {
            ImGuiTabItemFlags fl = (forceTab == i) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(tabs[i].name, nullptr, fl)) { tabs[i].fn(*this); ImGui::EndTabItem(); }
        }
        ImGui::EndTabBar();
    }
    forceTab = -1;
    ImGui::End();
}

}  // namespace vlab
