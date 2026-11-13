// report.hpp -- turns a finished run into an Excel workbook (data + charts) or a CSV file.
#pragma once
#include <string>
#include <vector>
#include "vlab/analysis.hpp"
#include "vlab/model.hpp"
#include "vlab/telemetry.hpp"

namespace vlab {

struct ExportExtras {
    const Spectrum* spectrum = nullptr;            // spectrum of one channel, if the user computed it
    std::string spectrumChannel;
    const ModalResult* modal = nullptr;            // natural frequencies and mode shapes
    const std::vector<FrfPoint>* frf = nullptr;    // frequency-response sweep
    std::string frfNote;
};

// Workbook sheets: Summary (design, environment, energy ledger, statistics) | Telemetry (every sample) | Charts |
// optional Spectrum, Modal, FrequencyResponse. Returns false and fills `error` on failure.
bool exportExcel(const std::string& path, const Model& m, const Telemetry& t, double requestedDuration, const ExportExtras& extras, std::string* error);
bool exportCsv(const std::string& path, const Telemetry& t, std::string* error);

}  // namespace vlab
