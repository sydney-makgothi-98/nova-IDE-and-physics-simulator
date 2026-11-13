#include "vlab/session.hpp"
#include <algorithm>
#include <cmath>

namespace vlab {

double Session::sampleInterval() const {
    double hz = std::max(1.0, design_.run.sampleHz);
    double byRate = 1.0 / hz;
    double byLimit = design_.run.duration > 0 ? design_.run.duration / 1000000.0 : 0.0;     // at most about a million rows
    return std::max(byRate, byLimit);
}

void Session::start() {
    design_.updateMass();
    world_ = std::make_unique<World>(design_);
    tel_.begin(design_);
    finished_ = false;
    acc_ = 0;
    const double dt = world_->model().env.dt;
    sampleEvery_ = std::max<long long>(1, (long long)std::llround(sampleInterval() / dt));
    tel_.append(*world_);
}

void Session::syncLiveEdits() {
    if (!world_) return;
    Model m = design_;
    if (m.bodies.size() != world_->model().bodies.size() || m.links.size() != world_->model().links.size()) return;   // structure edits need a restart
    m.updateMass();
    world_->model() = m;
    world_->rebaseLedger();
}

double Session::advance(double seconds) {
    if (!world_ || finished_) return 0;
    const double dt = world_->model().env.dt, end = design_.run.duration;
    double simulated = 0;
    acc_ += seconds;
    while (acc_ >= dt) {
        if (world_->time() >= end - 1e-9) { finished_ = true; break; }
        world_->step();
        acc_ -= dt;
        simulated += dt;
        if (world_->stepCount() % sampleEvery_ == 0) tel_.append(*world_);
        if (!world_->healthy()) break;
    }
    if (world_->time() >= end - 1e-9) {
        finished_ = true;
        if (tel_.rows() == 0 || tel_.t.back() < world_->time() - 1e-12) tel_.append(*world_);   // always keep the final state
    }
    return simulated;
}

}  // namespace vlab
