// model.hpp -- the DESIGN: what the user drew (bodies, shapes, materials, links, environment).
// The design never changes while a simulation runs, except through live property edits.
#pragma once
#include <string>
#include <vector>
#include "math.hpp"

namespace vlab {

// ------------------------------------------------------------------ materials
struct Material {
    std::string name = "Custom";
    double density = 1000;     // kg/m^3 (2-D bodies have 1 m depth, so mass = density * area)
    double muS = 0.5;          // static friction coefficient
    double muK = 0.4;          // kinetic (sliding) friction coefficient
    double restitution = 0.3;  // bounciness: 0 = dead, 1 = perfectly elastic
    double rollRes = 0.0;      // rolling resistance coefficient (dimensionless, about 0.001 to 0.05)
};

// Generic textbook-level values. Real values depend on surface finish, temperature and speed.
const std::vector<Material>& materialLibrary();
const Material* findMaterial(const std::string& name);

// ------------------------------------------------------------------ shapes
enum class ShapePreset { Circle, Box, Regular, Wedge, Line };   // Line: a zero-thickness segment (floors, walls, ramps)

struct Shape {
    ShapePreset preset = ShapePreset::Circle;
    double radius = 0.5;       // Circle radius, or circumradius of a Regular polygon
    double w = 1.0, h = 1.0;   // Box width/height, Wedge legs
    int sides = 5;             // Regular polygon: 3..12
    // Derived polygon data (empty for circles). Vertices are CCW, centred on the centroid.
    std::vector<Vec2> verts, normals;
    double area = 0;           // m^2
    double inertiaPerMass = 0; // (moment of inertia) / mass, m^2

    bool isCircle() const { return preset == ShapePreset::Circle; }
    void rebuild();            // recompute verts, normals, area, inertiaPerMass from the parameters
    double boundingRadius() const;
};

// ------------------------------------------------------------------ bodies
enum class BodyKind { Dynamic, Static, Anchor };   // Anchor = a fixed point (a pin in the wall)

struct Body {
    std::string name = "Body";
    BodyKind kind = BodyKind::Dynamic;
    Shape shape;
    Material mat;
    bool massFromDensity = false;   // false: use `mass`; true: density * area
    double mass = 1.0;              // kg (used when !massFromDensity)
    Vec2 pos;                       // initial position, metres (y points DOWN, like the screen)
    double angle = 0;               // initial angle, radians
    Vec2 vel;                       // initial velocity, m/s
    double omega = 0;               // initial angular velocity, rad/s
    bool lockX = false, lockY = false, lockRot = false;   // rails / no spin
    bool collide = true;            // may this body collide with other bodies?
    double driveF = 0;              // harmonic push: amplitude (N) ...
    double driveAngleDeg = 0;       // ... direction (0 = right, 90 = down) ...
    double driveW = 0;              // ... and angular frequency (rad/s)
    unsigned color = 0xFF3574F0;    // ARGB for drawing

    // derived by Model::updateMass()
    double massKg = 1, invMass = 1, inertia = 1, invI = 1;
    bool isDynamic() const { return kind == BodyKind::Dynamic; }
    bool collides() const { return kind != BodyKind::Anchor && collide; }
};

// ------------------------------------------------------------------ links
enum class LinkKind {
    Spring, Damper,
    Rod,       // rigid bar of fixed length
    Pin,       // hinge: two points coincide, bodies may rotate about it (optionally with a motor and rotary damping)
    Weld,      // fixed joint: points coincide AND the relative angle is held (a cantilever support when one body is ground)
    Roller     // a point of body B is held on a line through A's point: slides along A's x axis, rotation free
};

struct Link {
    LinkKind kind = LinkKind::Spring;
    int a = -1, b = -1;       // body indices
    Vec2 la, lb;              // attachment points in each body's local frame (metres)
    double k = 50;            // spring stiffness N/m
    double k3 = 0;            // cubic stiffness N/m^3 (0 = ideal linear spring)
    double L0 = 1;            // spring rest length (m) or rod length (m)
    bool autoRest = true;     // while true, L0 follows the drawn length
    double c = 1.0;           // damper coefficient N s/m
    bool collideConnected = false;   // may the two joined bodies collide with each other?
    double refAngle = 0;      // Weld: held relative angle (B minus A), follows the drawing
    double jointC = 0;        // Pin: rotary damping N m s/rad
    double motorTorque = 0;   // Pin: constant torque on B (and -torque on A), N m
    double motorAmp = 0;      // Pin: harmonic torque amplitude, N m
    double motorW = 0;        // Pin: harmonic torque frequency, rad/s
};

// ------------------------------------------------------------------ environment
enum class Combine { Geometric, Average, Min, Max };

struct Environment {
    double gravity = 9.81;           // m/s^2 toward +y (down)
    bool realistic = true;           // the master toggle (the individual switches below are what the engine reads)
    bool collisions = true;
    bool friction = true;            // off: every contact is frictionless (mu = 0)
    bool inelastic = true;           // off: every impact is perfectly elastic (e = 1)
    bool rolling = true;             // rolling resistance of circles
    bool damping = true;             // off: dampers are ignored
    bool airDrag = false;            // F = -(c1 + c2 |v|) v on every moving body
    double drag1 = 0.0;              // N s/m
    double drag2 = 0.0;              // N s^2/m^2
    Combine muCombine = Combine::Geometric;
    Combine eCombine = Combine::Max;
    double restThreshold = 0.25;     // impacts slower than this (m/s) do not bounce (lets things come to rest)
    int solverIterations = 14;
    double dt = 0.00025;             // physics step in seconds

    void setIdeal();                 // frictionless, perfectly elastic, no drag, no damping
    void setRealistic();             // friction, inelastic impacts, damping, rolling resistance
};

struct RunSettings {
    double duration = 10.0;          // seconds of simulated time to run
    double sampleHz = 100.0;         // telemetry rows per simulated second
};

// ------------------------------------------------------------------ the model
struct Model {
    std::string name = "Untitled";
    std::string notes;
    std::vector<Body> bodies;
    std::vector<Link> links;
    Environment env;
    RunSettings run;

    void updateMass();                               // recompute mass, inertia of every body
    int addBody(const Body& b);                      // returns index
    void removeBody(int i);                          // also removes links that touch it
    int addLink(LinkKind kind, int a, int b, Vec2 worldPointA, Vec2 worldPointB);   // -1 if refused
    // A support: makes a fixed ground point at worldPoint (angleRad orients its symbol and a roller's sliding axis) and joins `body` to it
    // with a Pin (hinge), Weld (fixed / cantilever) or Roller. Returns the link index, or -1.
    int addSupport(LinkKind kind, int body, Vec2 worldPoint, double angleRad);
    void removeLink(int i);
    void refreshAutoRest();                          // springs without a hand-set rest length follow their drawn length
    Vec2 worldPoint(int body, Vec2 local) const;     // initial-pose attachment point in world coordinates
    int dynamicCount() const;
    std::string uniqueName(const std::string& base) const;
};

// Ready-made bodies the user can drop onto the canvas.
Body makeCircle(double radius, const Material& m, Vec2 pos);
Body makeBox(double w, double h, const Material& m, Vec2 pos);
Body makeRegular(int sides, double radius, const Material& m, Vec2 pos);
Body makeWedge(double w, double h, const Material& m, Vec2 pos);
Body makeFloor(double length, double angleDeg, const Material& m, Vec2 pos);   // a zero-thickness hatched line: floor, wall or ramp
Body makeBeam(double length, double thickness, const Material& m, Vec2 pos);   // a rigid beam: it FALLS unless supported
Body makeAnchor(Vec2 pos);

}  // namespace vlab
