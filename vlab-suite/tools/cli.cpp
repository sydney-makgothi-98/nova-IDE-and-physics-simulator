// vlab_cli -- run a design from the command line and export the results (no window needed).
//   vlab_cli --list
//   vlab_cli --example 7 --duration 8 --out bounce.xlsx [--ideal | --realistic] [--csv] [--sample-hz 200]
//   vlab_cli --file my.vlab --duration 20 --out result.xlsx
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../io/report.hpp"
#include "vlab/project_io.hpp"
#include "vlab/session.hpp"

using namespace vlab;

int main(int argc, char** argv) {
    std::string file, out, saveDesignPath;
    int example = 0;
    double duration = -1, hz = -1;
    bool ideal = false, realistic = false, csv = false, spectrum = false, modal = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--list") {
            int n = 1;
            for (const Example& e : exampleGallery()) std::printf("%2d  %s\n    %s\n", n++, e.title.c_str(), e.summary.c_str());
            return 0;
        }
        else if (a == "--example") example = std::atoi(next().c_str());
        else if (a == "--file") file = next();
        else if (a == "--duration") duration = std::atof(next().c_str());
        else if (a == "--sample-hz") hz = std::atof(next().c_str());
        else if (a == "--out") out = next();
        else if (a == "--save-design") saveDesignPath = next();
        else if (a == "--ideal") ideal = true;
        else if (a == "--realistic") realistic = true;
        else if (a == "--csv") csv = true;
        else if (a == "--spectrum") spectrum = true;
        else if (a == "--modal") modal = true;
        else { std::printf("unknown option %s\n", a.c_str()); return 2; }
    }
    Model m;
    if (!file.empty()) {
        std::string err;
        if (!loadModelFile(file, m, &err)) { std::printf("cannot load %s: %s\n", file.c_str(), err.c_str()); return 1; }
    } else if (example >= 1 && example <= (int)exampleGallery().size()) {
        m = exampleGallery()[example - 1].model;
    } else {
        std::printf("usage: vlab_cli --list | --example N | --file design.vlab  [--duration S] [--sample-hz H] [--ideal|--realistic] [--spectrum] [--modal] --out file.xlsx|file.csv\n");
        return 2;
    }
    if (!saveDesignPath.empty()) {            // just write the (possibly modified) design as a .vlab file and stop
        if (ideal) m.env.setIdeal();
        if (realistic) m.env.setRealistic();
        bool ok = saveModelFile(saveDesignPath, m);
        std::printf("%s %s\n", ok ? "wrote" : "FAILED to write", saveDesignPath.c_str());
        return ok ? 0 : 1;
    }
    if (ideal) m.env.setIdeal();
    if (realistic) m.env.setRealistic();
    if (duration > 0) m.run.duration = duration;
    if (hz > 0) m.run.sampleHz = hz;
    Session s;
    s.setDesign(m);
    s.start();
    bool ok = s.runToEnd([](double f) { std::printf("\r  simulating %3.0f%%", f * 100); std::fflush(stdout); return true; });
    std::printf("\r  simulated %.2f s, %zu rows, %s\n", s.simTime(), s.telemetry().rows(), ok ? "done" : "STOPPED (numbers blew up)");
    const Ledger& L = s.world()->ledger();
    std::printf("  energy: E = %.4f J  lost: viscous %.4f, drag %.4f, friction %.4f, impact %.4f  numerical error %.2e\n", L.E, L.dViscous, L.dDrag, L.dFriction, L.dImpact, L.numerical());
    if (out.empty()) return ok ? 0 : 1;
    std::string err;
    bool saved;
    if (csv || (out.size() > 4 && out.substr(out.size() - 4) == ".csv")) {
        saved = exportCsv(out, s.telemetry(), &err);
    } else {
        ExportExtras x;
        Spectrum sp;
        ModalResult mr;
        if (spectrum && s.telemetry().channels.size() > 0) {
            int c = s.telemetry().find(s.design().bodies.empty() ? "" : [&] { for (const Body& b : s.design().bodies) if (b.isDynamic()) return b.name + " dx"; return std::string(); }());
            if (c >= 0) { sp = computeSpectrum(s.telemetry().t, s.telemetry().col[c]); x.spectrum = &sp; x.spectrumChannel = s.telemetry().channels[c].name; }
        }
        if (modal) { mr = modalAnalysis(s.design()); x.modal = &mr; }
        saved = exportExcel(out, s.design(), s.telemetry(), s.design().run.duration, x, &err);
    }
    std::printf("  %s %s\n", saved ? "wrote" : "FAILED to write", out.c_str());
    if (!saved) std::printf("  %s\n", err.c_str());
    return saved && ok ? 0 : 1;
}
