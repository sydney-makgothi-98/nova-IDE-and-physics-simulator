// math.hpp -- 2-D vector maths used by the whole engine. No OS headers, no globals.
#pragma once
#include <cmath>

namespace vlab {

constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    double x = 0, y = 0;
    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator-() const { return {-x, -y}; }
    Vec2 operator*(double s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(double s) { x *= s; y *= s; return *this; }
};
inline Vec2 operator*(double s, Vec2 v) { return {v.x * s, v.y * s}; }

inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }          // scalar z of a x b
inline Vec2 cross(double s, Vec2 v) { return {-s * v.y, s * v.x}; }            // s (z) x v
inline Vec2 cross(Vec2 v, double s) { return {s * v.y, -s * v.x}; }            // v x s (z)
inline double length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline double lengthSq(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline Vec2 normalized(Vec2 v) { double l = length(v); return l > 1e-12 ? Vec2(v.x / l, v.y / l) : Vec2(1, 0); }
inline Vec2 perp(Vec2 v) { return {-v.y, v.x}; }                                // rotate +90 degrees
inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

// A rotation stored as cosine and sine so it is computed once per body per step.
struct Rot {
    double c = 1, s = 0;
    Rot() = default;
    explicit Rot(double angle) : c(std::cos(angle)), s(std::sin(angle)) {}
    Vec2 apply(Vec2 v) const { return {c * v.x - s * v.y, s * v.x + c * v.y}; }
    Vec2 applyInv(Vec2 v) const { return {c * v.x + s * v.y, -s * v.x + c * v.y}; }
};

}  // namespace vlab
