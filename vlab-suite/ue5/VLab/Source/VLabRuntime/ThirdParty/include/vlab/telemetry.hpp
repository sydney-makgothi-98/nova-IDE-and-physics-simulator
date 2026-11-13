// telemetry.hpp -- the measured numbers of a run: one column per channel, one row per sample.
#pragma once
#include <string>
#include <vector>
#include "deform.hpp"
#include "world.hpp"

namespace vlab {

struct Channel {
    std::string name;     // e.g. "Ball 1 x"
    std::string unit;     // e.g. "m"
    std::string group;    // e.g. "Ball 1", "Energy", "Spring 2"
};

// How one body deformed during the run: its shape at every sample row (beams) next to the scalar deformation channel.
struct DeformSeries {
    int body = -1;
    std::string name;
    bool beam = false;
    BeamFE fe;                                   // beams: nodes, E, I, L
    std::vector<float> w;                        // beams: rows x nodes, deflection (m) at each node
    double length = 0, E = 0;
    int chanDeform = -1, chanLoad = -1;          // indices of the body's deformation and contact-force channels
    size_t rows() const { return fe.x.empty() ? 0 : w.size() / fe.x.size(); }
};

class Telemetry {
public:
    std::vector<Channel> channels;
    std::vector<double> t;                       // sample times (s)
    std::vector<std::vector<double>> col;        // col[channel][row]
    std::vector<DeformSeries> deform;            // one per moving body

    void begin(const Model& m);                  // build the channel list for this design and clear all data
    void append(const World& w);                 // record one row from the world's current state
    void clear();
    size_t rows() const { return t.size(); }
    int find(const std::string& name) const;     // -1 if absent
    int firstOfGroup(const std::string& group) const;
    const DeformSeries* deformOf(int body) const;   // nullptr if the body has none
};

}  // namespace vlab
