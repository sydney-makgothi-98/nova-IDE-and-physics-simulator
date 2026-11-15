// deform.hpp -- how much does each body BEND or SQUASH under the loads the engine finds?
//
// The solver treats every body as rigid. Deformation is computed on top of it, as a post-process, the way an engineer
// does a hand check (quasi-static, small deflection):
//
//   * a slender box (length >= 4 x thickness) is an Euler-Bernoulli BEAM. The loads on it (contact forces, support and joint
//     reactions, spring and damper forces, its own weight) are read from the world at that instant, d'Alembert inertia
//     loads are added so the free beam is in balance, and a finite-element solve gives the deflection w(x).
//     The rigid chord through the supports is removed, so w = 0 at a pin or roller and w = w' = 0 at a fixed support.
//   * any other moving body is SQUASHED along the line of its largest contact load:  delta = F L / (E A).
//
// Needs the Young's modulus E of the material (Material::youngs). 2-D bodies have 1 m depth.
// Honest limit: static response to the instantaneous load. The beam's own vibration (flexural modes) is not simulated.
#pragma once
#include <vector>
#include "world.hpp"

namespace vlab {

bool isBeamBody(const Body& b);     // dynamic, a box, at least 4 times longer than thick

// Elastic model of one beam body: nodes, stiffness (factorised once) and section properties.
struct BeamFE {
    int body = -1;
    int elements = 24;
    double L = 0, h = 0;            // length and thickness (m)
    double E = 0, I = 0, EI = 0;    // Pa, m^4 (per metre of depth), N m^2
    double mu = 0;                  // mass per length (kg/m)
    std::vector<double> x;          // node positions along the beam axis, from -L/2 to +L/2
    std::vector<double> chol;       // Cholesky factor of the reduced stiffness matrix (dense, lower, row-major)
    bool valid = false;
    void build(int bodyIndex, const Body& b);
};

struct Deflection {
    std::vector<double> w;          // deflection at each node (m), positive along the beam's local +y (down for an unrotated beam)
    double maxAbs = 0;              // largest |w|
    double maxSigned = 0;           // w at that place (keeps the sign)
    double atX = 0;                 // where, along the beam
    double load = 0;                // total contact force on the beam (N)
};

// Deflection of the beam from the world's current state.
void beamDeflection(const World& world, const BeamFE& fe, Deflection& out);
// Largest bending stress |sigma| = E |w''| h / 2 (Pa), from the nodal deflections.
double bendingStress(const BeamFE& fe, const std::vector<double>& w);
// Same, from stored single-precision rows.
double bendingStress(const BeamFE& fe, const float* w);

// Squash of a non-beam body.
struct Squash {
    double load = 0;                // compressive force through the body (N)
    Vec2 axis;                      // unit vector (world) of the load line
    double length = 0;              // size of the body along that line (m)
    double compression = 0;         // delta (m)
    double strain = 0;              // delta / length
};
Squash bodySquash(const World& world, int body);
// Sum of the normal contact forces acting on a body (N).
double contactForce(const World& world, int body);

}  // namespace vlab
