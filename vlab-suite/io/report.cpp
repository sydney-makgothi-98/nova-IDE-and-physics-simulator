#include "report.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "xlsx.hpp"

namespace vlab {

static std::string fmt(const char* f, double v) {
    char b[64];
    std::snprintf(b, sizeof b, f, v);
    return b;
}
static const char* kindText(const Body& b) {
    if (b.kind == BodyKind::Anchor) return "Anchor (fixed point)";
    if (b.kind == BodyKind::Static) return "Static (floor / wall / ramp)";
    return "Moving body";
}
static std::string shapeText(const Shape& s) {
    switch (s.preset) {
        case ShapePreset::Circle: return "Circle, radius " + fmt("%.4g", s.radius) + " m";
        case ShapePreset::Box: return "Box " + fmt("%.4g", s.w) + " x " + fmt("%.4g", s.h) + " m";
        case ShapePreset::Regular: return std::to_string(s.sides) + "-sided polygon, radius " + fmt("%.4g", s.radius) + " m";
        default: return "Wedge " + fmt("%.4g", s.w) + " x " + fmt("%.4g", s.h) + " m";
    }
}
static const char* linkText(LinkKind k) {
    switch (k) {
        case LinkKind::Spring: return "Spring";
        case LinkKind::Damper: return "Damper";
        case LinkKind::Rod: return "Rigid rod";
        default: return "Pin joint";
    }
}
static std::string combineText(Combine c) {
    switch (c) {
        case Combine::Geometric: return "geometric mean";
        case Combine::Average: return "average";
        case Combine::Min: return "minimum";
        default: return "maximum";
    }
}
static bool nonZero(const std::vector<double>& v) {
    for (double x : v) if (std::fabs(x) > 1e-9) return true;
    return false;
}
static std::string header(const Channel& c) { return c.unit.empty() ? c.name : c.name + " (" + c.unit + ")"; }

// Row stride that keeps the data under Excel's 1,048,576-row limit.
static size_t strideFor(size_t rows) { return rows <= 1000000 ? 1 : (rows + 999999) / 1000000; }

bool exportCsv(const std::string& path, const Telemetry& t, std::string* error) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        if (error) *error = "Could not create the file (is it open in another program?)";
        return false;
    }
    std::fputs("\xEF\xBB\xBF", f);                                // UTF-8 byte-order mark: Excel then reads the units correctly
    std::fputs("Time (s)", f);
    for (const Channel& c : t.channels) std::fprintf(f, ",\"%s\"", header(c).c_str());
    std::fputs("\r\n", f);
    for (size_t r = 0; r < t.rows(); ++r) {
        std::fprintf(f, "%.9g", t.t[r]);
        for (size_t c = 0; c < t.col.size(); ++c) std::fprintf(f, ",%.9g", t.col[c][r]);
        std::fputs("\r\n", f);
    }
    bool ok = std::fclose(f) == 0;
    if (!ok && error) *error = "Writing the file failed.";
    return ok;
}

bool exportExcel(const std::string& path, const Model& m, const Telemetry& t, double requestedDuration, const ExportExtras& x, std::string* error) {
    if (t.rows() == 0) {
        if (error) *error = "There is no data yet: run the model first.";
        return false;
    }
    XlsxWorkbook wb;
    const size_t stride = strideFor(t.rows());
    std::vector<size_t> rowIdx;
    for (size_t r = 0; r < t.rows(); r += stride) rowIdx.push_back(r);
    const int nRows = (int)rowIdx.size();

    // ---------------------------------------------------------------- Summary
    {
        XlsxSheet& s = wb.addSheet("Summary");
        s.setColumnWidth(0, 34);
        for (int c = 1; c < 12; ++c) s.setColumnWidth(c, 16);
        s.addRow({XCell("Vibration Lab Suite: run report", 2)});
        s.addRow({XCell("Model", 3), XCell(m.name)});
        char tb[64];
        std::time_t now = std::time(nullptr);
        std::strftime(tb, sizeof tb, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        s.addRow({XCell("Exported", 3), XCell(tb)});
        if (!m.notes.empty()) s.addRow({XCell("Notes", 3), XCell(m.notes)});
        s.addRow({});
        s.addRow({XCell("RUN", 1), XCell("", 1)});
        s.addRow({XCell("Requested duration (s)"), XCell(requestedDuration)});
        s.addRow({XCell("Simulated time (s)"), XCell(t.t.back())});
        s.addRow({XCell("Telemetry rows"), XCell((double)nRows)});
        s.addRow({XCell("Sample interval (s)"), XCell(t.rows() > 1 ? (t.t[1] - t.t[0]) * (double)stride : 0.0)});
        s.addRow({XCell("Physics step (s)"), XCell(m.env.dt)});
        s.addRow({});
        s.addRow({XCell("ENVIRONMENT", 1), XCell("", 1)});
        const Environment& e = m.env;
        s.addRow({XCell("Mode"), XCell(e.realistic ? "Realistic" : "Ideal (conservative)")});
        s.addRow({XCell("Gravity (m/s^2)"), XCell(e.gravity)});
        s.addRow({XCell("Collisions"), XCell(e.collisions ? "on" : "off")});
        s.addRow({XCell("Friction"), XCell(e.friction ? "on (" + combineText(e.muCombine) + " of the two surfaces)" : "off (frictionless)")});
        s.addRow({XCell("Impacts"), XCell(e.inelastic ? "inelastic: restitution from materials (" + combineText(e.eCombine) + ")" : "perfectly elastic")});
        s.addRow({XCell("Rolling resistance"), XCell(e.rolling ? "on" : "off")});
        s.addRow({XCell("Dampers"), XCell(e.damping ? "active" : "ignored")});
        s.addRow({XCell("Air drag"), XCell(e.airDrag ? "on: c1 = " + fmt("%.4g", e.drag1) + " N s/m, c2 = " + fmt("%.4g", e.drag2) + " N s^2/m^2" : "off")});
        s.addRow({});
        s.addRow({XCell("BODIES", 1), XCell("Type", 1), XCell("Shape", 1), XCell("Mass (kg)", 1), XCell("Material", 1), XCell("mu static", 1), XCell("mu kinetic", 1),
                  XCell("Restitution", 1), XCell("Start x (m)", 1), XCell("Start y (m)", 1), XCell("Start angle (deg)", 1), XCell("Start vx (m/s)", 1), XCell("Start vy (m/s)", 1)});
        s.setColumnWidth(1, 26);
        s.setColumnWidth(2, 32);
        for (const Body& b : m.bodies)
            s.addRow({XCell(b.name), XCell(kindText(b)), XCell(shapeText(b.shape)), b.isDynamic() ? XCell(b.massKg) : XCell("(fixed)"), XCell(b.mat.name), XCell(b.mat.muS),
                      XCell(b.mat.muK), XCell(b.mat.restitution), XCell(b.pos.x), XCell(b.pos.y), XCell(b.angle * 180.0 / kPi), XCell(b.vel.x), XCell(b.vel.y)});
        s.addRow({});
        if (!m.links.empty()) {
            s.addRow({XCell("CONNECTIONS", 1), XCell("Type", 1), XCell("Between", 1), XCell("Stiffness k (N/m)", 1), XCell("Cubic k3 (N/m^3)", 1), XCell("Rest/rod length (m)", 1), XCell("Damping c (N s/m)", 1)});
            int n = 1;
            for (const Link& l : m.links) {
                std::vector<XCell> row = {XCell(std::string(linkText(l.kind)) + " " + std::to_string(n++)), XCell(linkText(l.kind)),
                                          XCell(m.bodies[l.a].name + " - " + m.bodies[l.b].name)};
                if (l.kind == LinkKind::Spring) { row.push_back(XCell(l.k)); row.push_back(XCell(l.k3)); row.push_back(XCell(l.L0)); }
                else if (l.kind == LinkKind::Damper) { row.push_back(XCell()); row.push_back(XCell()); row.push_back(XCell()); row.push_back(XCell(l.c)); }
                else if (l.kind == LinkKind::Rod) { row.push_back(XCell()); row.push_back(XCell()); row.push_back(XCell(l.L0)); }
                s.addRow(row);
            }
            s.addRow({});
        }
        // energy ledger at the end of the run
        auto last = [&](const char* name) { int c = t.find(std::string("Energy ") + name); return c >= 0 ? t.col[c].back() : 0.0; };
        s.addRow({XCell("ENERGY LEDGER (end of run)", 1), XCell("Joules", 1)});
        s.addRow({XCell("Kinetic T"), XCell(last("T kinetic"))});
        s.addRow({XCell("Potential V"), XCell(last("V potential"))});
        s.addRow({XCell("Total E = T + V"), XCell(last("E total"))});
        s.addRow({XCell("Work put in by pushes and the mouse"), XCell(last("work in"))});
        s.addRow({XCell("Lost to dampers (viscous heat)"), XCell(last("lost viscous"))});
        s.addRow({XCell("Lost to air drag"), XCell(last("lost drag"))});
        s.addRow({XCell("Lost to sliding and rolling friction"), XCell(last("lost friction"))});
        s.addRow({XCell("Lost in impacts"), XCell(last("lost impact"))});
        s.addRow({XCell("Numerical error (should be near 0)"), XCell(last("numerical error"))});
        s.addRow({});
        // statistics of every channel
        s.addRow({XCell("STATISTICS", 1), XCell("Unit", 1), XCell("Minimum", 1), XCell("Maximum", 1), XCell("Mean", 1), XCell("RMS", 1), XCell("Final", 1), XCell("Time of max (s)", 1)});
        for (size_t c = 0; c < t.channels.size(); ++c) {
            Stats st = computeStats(t.t, t.col[c]);
            s.addRow({XCell(t.channels[c].name), XCell(t.channels[c].unit), XCell(st.min), XCell(st.max), XCell(st.mean), XCell(st.rms), XCell(st.last), XCell(st.tMax)});
        }
        s.addRow({});
        s.addRow({XCell("To plot something else: select the Time column and any channel column on the Telemetry sheet, then Insert > Scatter with Straight Lines.", 4)});
    }

    // ---------------------------------------------------------------- Telemetry
    {
        XlsxSheet& s = wb.addSheet("Telemetry");
        s.freezeFirstRow();
        std::vector<XCell> head;
        head.push_back(XCell("Time (s)", 1));
        for (const Channel& c : t.channels) head.push_back(XCell(header(c), 1));
        s.addRow(head);
        s.setColumnWidth(0, 12);
        for (size_t c = 0; c < t.channels.size(); ++c) s.setColumnWidth((int)c + 1, std::max<double>(12, (double)header(t.channels[c]).size() + 2));
        std::vector<XCell> row(t.channels.size() + 1);
        for (size_t r : rowIdx) {
            row[0] = XCell(t.t[r]);
            for (size_t c = 0; c < t.channels.size(); ++c) row[c + 1] = XCell(t.col[c][r]);
            s.addRow(row);
        }
    }

    // ---------------------------------------------------------------- Charts
    {
        XlsxSheet& s = wb.addSheet("Charts");
        s.addRow({XCell("Charts of this run (they stay linked to the Telemetry sheet: edit the data and they update)", 4)});
        auto series = [&](const std::string& channel, XChart& ch, const char* label = nullptr, int lastRow = -1) {
            int c = t.find(channel);
            if (c < 0 || !nonZero(t.col[c]) || ch.series.size() >= 8) return;
            XChartSeries se;
            se.nameSheet = "Telemetry";
            se.nameRow = 0;
            se.nameCol = c + 1;
            se.cachedName = label ? label : header(t.channels[c]);
            se.sheet = "Telemetry";
            se.xCol = 0;
            se.yCol = c + 1;
            se.rowFirst = 1;
            se.rowLast = lastRow > 0 ? lastRow : nRows;
            ch.series.push_back(se);
        };
        std::vector<XChart> charts;
        auto addChart = [&](XChart ch) {
            if (ch.series.empty()) return;
            ch.anchorCol = (charts.size() % 2) * 12;
            ch.anchorRow = 2 + (int)(charts.size() / 2) * 22;
            charts.push_back(ch);
        };
        { XChart c; c.title = "Position: displacement from start"; c.xTitle = "Time (s)"; c.yTitle = "Displacement (m)";
          for (const Body& b : m.bodies) if (b.isDynamic()) { series(b.name + " dx", c); series(b.name + " dy", c); } addChart(c); }
        if (t.t.back() > 20.0) {                                          // long run: the first seconds, where the action usually is
            int zoomLast = 1;
            while (zoomLast < nRows && t.t[rowIdx[zoomLast]] <= 10.0) ++zoomLast;
            XChart c; c.title = "Position, first 10 seconds (zoom)"; c.xTitle = "Time (s)"; c.yTitle = "Displacement (m)";
            for (const Body& b : m.bodies) if (b.isDynamic()) { series(b.name + " dx", c, nullptr, zoomLast); series(b.name + " dy", c, nullptr, zoomLast); }
            addChart(c);
        }
        { XChart c; c.title = "Velocity"; c.xTitle = "Time (s)"; c.yTitle = "Velocity (m/s)";
          for (const Body& b : m.bodies) if (b.isDynamic()) { series(b.name + " vx", c); series(b.name + " vy", c); } addChart(c); }
        { XChart c; c.title = "Rotation angle"; c.xTitle = "Time (s)"; c.yTitle = "Angle (deg)";
          for (const Body& b : m.bodies) if (b.isDynamic()) series(b.name + " angle", c); addChart(c); }
        { XChart c; c.title = "Energy"; c.xTitle = "Time (s)"; c.yTitle = "Energy (J)";
          series("Energy T kinetic", c); series("Energy V potential", c); series("Energy E total", c); addChart(c); }
        { XChart c; c.title = "Energy ledger: where the energy went"; c.xTitle = "Time (s)"; c.yTitle = "Cumulative energy (J)";
          series("Energy work in", c); series("Energy lost viscous", c); series("Energy lost drag", c); series("Energy lost friction", c); series("Energy lost impact", c); series("Energy lost total", c); addChart(c); }
        { XChart c; c.title = "Spring and damper forces"; c.xTitle = "Time (s)"; c.yTitle = "Force (N)";
          for (size_t ch = 0; ch < t.channels.size(); ++ch) if (t.channels[ch].unit == "N") series(t.channels[ch].name, c); addChart(c); }
        { XChart c; c.title = "Damper heat power"; c.xTitle = "Time (s)"; c.yTitle = "Power (W)";
          for (size_t ch = 0; ch < t.channels.size(); ++ch) if (t.channels[ch].unit == "W") series(t.channels[ch].name, c); addChart(c); }
        s.charts = charts;
    }

    // ---------------------------------------------------------------- optional analysis sheets
    if (x.spectrum && !x.spectrum->freq.empty()) {
        XlsxSheet& s = wb.addSheet("Spectrum");
        s.setColumnWidth(0, 16);
        s.setColumnWidth(1, 22);
        s.addRow({XCell("Frequency (Hz)", 1), XCell("Amplitude: " + x.spectrumChannel, 1)});
        int cap = 0;
        for (size_t i = 0; i < x.spectrum->freq.size(); ++i) {
            if (x.spectrum->freq[i] > 60) break;
            s.addRow({XCell(x.spectrum->freq[i]), XCell(x.spectrum->amp[i])});
            ++cap;
        }
        s.addRow({});
        s.addRow({XCell("Peak frequency (Hz)", 3), XCell(x.spectrum->peakHz)});
        XChart c;
        c.title = "Spectrum of " + x.spectrumChannel;
        c.xTitle = "Frequency (Hz)";
        c.yTitle = "Amplitude";
        XChartSeries se;
        se.nameSheet = "Spectrum"; se.nameRow = 0; se.nameCol = 1; se.cachedName = "Amplitude";
        se.sheet = "Spectrum"; se.xCol = 0; se.yCol = 1; se.rowFirst = 1; se.rowLast = cap;
        c.series.push_back(se);
        c.anchorCol = 3; c.anchorRow = 1;
        s.charts.push_back(c);
    }
    if (x.modal && !x.modal->modes.empty()) {
        XlsxSheet& s = wb.addSheet("Modal");
        s.setColumnWidth(0, 12);
        s.addRow({XCell("Mode", 1), XCell("Frequency (Hz)", 1), XCell("Damping ratio", 1)});
        for (size_t d = 0; d < x.modal->dofNames.size(); ++d) s.setColumnWidth(3 + (int)d, 18);
        int n = 1;
        for (const Mode& md : x.modal->modes) {
            std::vector<XCell> row = {XCell((double)n++), XCell(md.freqHz), XCell(md.dampingRatio)};
            s.addRow(row);
        }
        s.addRow({});
        s.addRow({XCell("MODE SHAPES (largest entry scaled to 1)", 1)});
        std::vector<XCell> head = {XCell("Mode", 1)};
        for (const std::string& dn : x.modal->dofNames) head.push_back(XCell(dn, 1));
        s.addRow(head);
        n = 1;
        for (const Mode& md : x.modal->modes) {
            std::vector<XCell> row = {XCell((double)n++)};
            for (double v : md.shape) row.push_back(XCell(v));
            s.addRow(row);
        }
        if (!x.modal->note.empty()) { s.addRow({}); s.addRow({XCell(x.modal->note, 4)}); }
    }
    if (x.frf && !x.frf->empty()) {
        XlsxSheet& s = wb.addSheet("FrequencyResponse");
        for (int c = 0; c < 4; ++c) s.setColumnWidth(c, 20);
        s.addRow({XCell("Frequency (Hz)", 1), XCell("Amplitude (m or rad)", 1), XCell("Gain (per N)", 1), XCell("Phase lag (deg)", 1)});
        for (const FrfPoint& p : *x.frf) s.addRow({XCell(p.freqHz), XCell(p.amplitude), XCell(p.gain), XCell(p.phaseDeg)});
        if (!x.frfNote.empty()) { s.addRow({}); s.addRow({XCell(x.frfNote, 4)}); }
        int last = (int)x.frf->size();
        for (int k = 0; k < 2; ++k) {
            XChart c;
            c.title = k == 0 ? "Frequency response: amplitude" : "Frequency response: phase lag";
            c.xTitle = "Frequency (Hz)";
            c.yTitle = k == 0 ? "Amplitude" : "Phase lag (deg)";
            XChartSeries se;
            se.nameSheet = "FrequencyResponse"; se.nameRow = 0; se.nameCol = k == 0 ? 1 : 3; se.cachedName = k == 0 ? "Amplitude" : "Phase lag";
            se.sheet = "FrequencyResponse"; se.xCol = 0; se.yCol = k == 0 ? 1 : 3; se.rowFirst = 1; se.rowLast = last;
            c.series.push_back(se);
            c.anchorCol = 5; c.anchorRow = 1 + k * 21;
            s.charts.push_back(c);
        }
    }
    return wb.save(path, error);
}

}  // namespace vlab
