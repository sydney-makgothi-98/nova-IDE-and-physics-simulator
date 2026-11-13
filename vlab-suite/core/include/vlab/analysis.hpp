// analysis.hpp -- the vibrations half of the suite: statistics, spectra, damping estimates,
// modal analysis (natural frequencies and mode shapes) and frequency-response sweeps.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "model.hpp"

namespace vlab {

struct Stats {
    double min = 0, max = 0, mean = 0, rms = 0, last = 0, tMin = 0, tMax = 0;
};
Stats computeStats(const std::vector<double>& t, const std::vector<double>& y);

struct Spectrum {
    std::vector<double> freq, amp;   // Hz, and amplitude in the same units as the signal
    double peakHz = 0, peakAmp = 0;
};
// Hann-windowed FFT of a (roughly) uniformly sampled signal. The mean is removed first.
Spectrum computeSpectrum(const std::vector<double>& t, const std::vector<double>& y);

// Damping ratio from the logarithmic decrement of successive positive peaks. Returns -1 if it cannot tell.
double estimateDampingRatio(const std::vector<double>& t, const std::vector<double>& y);

struct Mode {
    double freqHz = 0;
    double dampingRatio = 0;
    std::vector<double> shape;       // one entry per degree of freedom, scaled so the largest is 1
};
struct ModalResult {
    std::vector<Mode> modes;         // sorted by frequency
    std::vector<std::string> dofNames;
    std::vector<int> dofBody;        // body index of each DOF
    std::vector<int> dofKind;        // 0 = x, 1 = y, 2 = angle
    std::string note;
};
// Linearises the smooth model (springs, dampers, gravity) about its drawn configuration.
ModalResult modalAnalysis(const Model& m);

struct FrfPoint {
    double freqHz = 0, amplitude = 0, gain = 0, phaseDeg = 0;
};
struct FrfRequest {
    int driveBody = 0;
    double driveAngleDeg = 0;
    double forceAmp = 1.0;
    int responseBody = 0;
    int component = 0;               // 0 = x, 1 = y, 2 = angle (rad)
    double f0 = 0.1, f1 = 5.0;
    int points = 40;
    double settleSeconds = 10.0;
};
// Runs one steady-state simulation per frequency. `progress` receives 0..1; return true from `cancel` to stop.
std::vector<FrfPoint> frequencyResponse(const Model& m, const FrfRequest& req, const std::function<void(float)>& progress,
                                        const std::function<bool()>& cancel);

}  // namespace vlab
