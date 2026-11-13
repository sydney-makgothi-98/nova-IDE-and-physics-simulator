// world.hpp -- the RUNTIME: moves a Model forward in time.
//
//   * Smooth forces (springs, dampers, gravity, drag, pushes, the mouse) come from the Lagrangian equations
//         m a = -dV/dq - dD/dqdot + Q            (see the guide, chapters 2 and 3)
//   * Scenes with no contacts and no joints are integrated with RK4 (4th order, validated against theory).
//   * Scenes with collisions or joints use a fixed small step: forces -> velocities, then a sequential-impulse
//     solver for contacts (restitution + Coulomb friction + rolling resistance) and joints, then positions.
//   * The energy LEDGER accounts for every joule: kinetic, potential, work in, and each loss channel.
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "model.hpp"

namespace vlab {

struct BodyState {
    Vec2 pos;
    double angle = 0;
    Vec2 vel;
    double omega = 0;
    Rot rot;                       // cached cos/sin of angle
};

// E(t) = E0 + workIn - dViscous - dDrag - dFriction - dImpact + (numerical error)
struct Ledger {
    double T = 0, V = 0, E = 0;
    double workIn = 0;             // work done by pushes and the mouse
    double dViscous = 0;           // heat made by dampers
    double dDrag = 0;              // heat made by air drag
    double dFriction = 0;          // heat made by sliding and rolling friction
    double dImpact = 0;            // energy lost in collisions (restitution < 1)
    double E0 = 0;
    double unclassified = 0;       // diagnostic: contact work that is a reversible exchange (resting contact, sticking friction)
    double posCorr = 0;            // diagnostic: potential energy created by pushing overlapping bodies apart
    double numerical() const { return E - (E0 + workIn - dViscous - dDrag - dFriction - dImpact); }
    double dissipated() const { return dViscous + dDrag + dFriction + dImpact; }
};

struct ContactInfo {
    int a = 0, b = 0;
    Vec2 point, normal;            // normal points from a to b
    double separation = 0;         // negative = overlap
    double normalImpulse = 0;
};

struct LinkReading {
    double length = 0, stretch = 0, rate = 0, force = 0, power = 0;
    double rx = 0, ry = 0, moment = 0;   // joints: reaction force on body B (N) and reaction moment (N m), averaged over the last step
};

class World {
public:
    explicit World(const Model& m);

    Model& model() { return model_; }                 // live edits (sliders) go through here
    const Model& model() const { return model_; }
    void reset();                                     // back to the model's initial state, time 0, empty ledger
    void step();                                      // advance by model().env.dt
    double time() const { return t_; }
    long long stepCount() const { return steps_; }
    bool healthy() const { return healthy_; }         // false once numbers blow up (NaN / huge)

    const std::vector<BodyState>& state() const { return S_; }
    const Ledger& ledger() const { return ledger_; }
    const std::vector<ContactInfo>& contacts() const { return contactInfo_; }
    LinkReading link(int i) const;
    Vec2 attachmentWorld(int body, Vec2 local) const { return S_[body].pos + S_[body].rot.apply(local); }

    void measure();                                   // recompute T, V, E from the current state
    void rebaseLedger();                              // after a live edit: restart the energy books
    bool usesConstraintSolver() const;
    // forces and torques of the smooth model (springs, dampers, gravity, drag, pushes) at an arbitrary state; used by modal analysis
    void forcesAt(const std::vector<BodyState>& S, double t, std::vector<Vec2>& F, std::vector<double>& tau) const {
        computeForces(S, t, F, tau, nullptr, nullptr, nullptr);
    }

    // The mouse "rubber band": pulls point `local` of `body` toward `target` (world). Call clearMouse() to let go.
    void setMouse(int body, Vec2 local, Vec2 target);
    void clearMouse() { mouse_.active = false; }

private:
    struct Contact {
        int a = 0, b = 0;
        Vec2 n, la, lb, pA, pB;       // normal a->b; local surface points; world points at detection
        double sep = 0;
        uint64_t key = 0;
        double Pn = 0, Pt = 0, Pr = 0;                 // accumulated impulses (normal, tangent, rolling)
        bool slip = false;                             // friction at the Coulomb limit (sliding)
        // set in prestep
        Vec2 ra, rb, t;
        double massN = 0, massT = 0, massR = 0, bias = 0, vt0 = 0, wrel0 = 0, muS = 0, muK = 0, rollLimit = 0;
        bool impact = false;
    };
    struct Manifold {
        int count = 0;
        Vec2 n;
        Vec2 pA[2], pB[2];
        double sep[2] = {0, 0};
        int id[2] = {0, 0};
    };
    struct Mouse { bool active = false; int body = -1; Vec2 local, target; };

    void computeForces(const std::vector<BodyState>& S, double t, std::vector<Vec2>& F, std::vector<double>& tau,
                       double* pViscous, double* pDrag, double* pIn) const;
    double potentialEnergy(const std::vector<BodyState>& S) const;
    void stepSmooth(double h);
    void stepConstrained(double h);
    void buildContacts();
    void warmStartAndPrestep(double h);
    void solveJoints();
    void solveContacts();
    void correctPositions();
    void applyLocks();
    void refreshRot(int i) { S_[i].rot = Rot(S_[i].angle); }
    // applies impulse P at the contact (equal and opposite) and returns the exact kinetic-energy change split into
    // the part along the normal and the part along the tangent
    void applyImpulse(Contact& c, Vec2 P, double* dKEn, double* dKEt);
    bool collide(int ia, int ib, Manifold& m) const;
    double combine(double a, double b, Combine mode) const;

    Model model_;
    std::vector<BodyState> S_;
    std::vector<std::vector<Vec2>> wv_, wn_;           // world-space polygon vertices and normals
    std::vector<Vec2> F_;
    std::vector<double> tau_;
    std::vector<Contact> contacts_;
    std::vector<ContactInfo> contactInfo_;
    struct Warm { double Pn, Pt, Pr; bool slip; };
    std::unordered_map<uint64_t, Warm> warm_;
    std::vector<Vec2> jointImp_;       // impulse each joint applied to body B during the last step
    std::vector<double> jointAng_;     // angular impulse (weld) on body B
    Ledger ledger_;
    Mouse mouse_;
    double t_ = 0;
    double stepDKE_ = 0;           // kinetic-energy change caused by this step's contact impulses
    long long steps_ = 0;
    bool healthy_ = true;
};

}  // namespace vlab
