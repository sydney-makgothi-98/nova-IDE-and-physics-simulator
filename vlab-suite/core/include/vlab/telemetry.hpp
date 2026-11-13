// telemetry.hpp -- the measured numbers of a run: one column per channel, one row per sample.
#pragma once
#include <string>
#include <vector>
#include "world.hpp"

namespace vlab {

struct Channel {
    std::string name;     // e.g. "Ball 1 x"
    std::string unit;     // e.g. "m"
    std::string group;    // e.g. "Ball 1", "Energy", "Spring 2"
};

class Telemetry {
public:
    std::vector<Channel> channels;
    std::vector<double> t;                       // sample times (s)
    std::vector<std::vector<double>> col;        // col[channel][row]

    void begin(const Model& m);                  // build the channel list for this design and clear all data
    void append(const World& w);                 // record one row from the world's current state
    void clear();
    size_t rows() const { return t.size(); }
    int find(const std::string& name) const;     // -1 if absent
    int firstOfGroup(const std::string& group) const;
};

}  // namespace vlab
