// session.hpp -- one experiment: the design, the running world, the recorded telemetry and the run clock.
// The desktop app and the command-line runner both drive a Session; it knows nothing about windows.
#pragma once
#include <memory>
#include "model.hpp"
#include "telemetry.hpp"
#include "world.hpp"

namespace vlab {

class Session {
public:
    Session() { design_.updateMass(); }

    Model& design() { return design_; }                 // edit this while stopped
    const Model& design() const { return design_; }
    void setDesign(const Model& m) { stop(); design_ = m; design_.updateMass(); tel_.begin(design_); }

    bool hasWorld() const { return (bool)world_; }       // true after start() until stop()
    World* world() { return world_.get(); }
    const World* world() const { return world_.get(); }
    Telemetry& telemetry() { return tel_; }
    const Telemetry& telemetry() const { return tel_; }

    void start();                                        // build a fresh World from the design, clear telemetry, sample t = 0
    void stop() { world_.reset(); }                      // forget the world (telemetry stays, so it can still be exported)
    bool finished() const { return finished_; }          // the requested duration has been simulated
    double simTime() const { return world_ ? world_->time() : (tel_.rows() ? tel_.t.back() : 0.0); }

    // Advance the simulation by `seconds` of simulated time (clamped at the duration). Returns the seconds actually simulated.
    double advance(double seconds);
    // Run to the end as fast as possible; calls progress(fraction) about every 50 ms of wall time. Return false from it to cancel.
    template <class F> bool runToEnd(F progress);

    void syncLiveEdits();                                // copy design edits into the running world (properties only, not structure)
    double sampleInterval() const;                       // seconds between telemetry rows (bounded so a run never exceeds ~1M rows)

private:
    Model design_;
    std::unique_ptr<World> world_;
    Telemetry tel_;
    bool finished_ = false;
    double acc_ = 0;                                     // simulated time not yet turned into whole steps
    long long sampleEvery_ = 1;
};

}  // namespace vlab

// ---------------------------------------------------------------------------------------------- template part
#include <chrono>
namespace vlab {
template <class F> bool Session::runToEnd(F progress) {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    while (world_ && !finished_) {
        advance(0.05);
        if (!world_->healthy()) return false;
        auto now = clock::now();
        if (std::chrono::duration<double>(now - last).count() > 0.05) {
            last = now;
            if (!progress(design_.run.duration > 0 ? world_->time() / design_.run.duration : 1.0)) return false;
        }
    }
    return true;
}
}  // namespace vlab
