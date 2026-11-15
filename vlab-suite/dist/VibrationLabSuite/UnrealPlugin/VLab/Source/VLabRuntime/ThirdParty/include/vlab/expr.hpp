// expr.hpp -- a tiny formula evaluator for equation loads:  q(x) = 200 + 50 x^2,   500 sin(pi x / L),   300 * exp(-x) ...
//
//   variables   x  distance along the body from its left end (m)      L  length of the body (m)      t  time (s)
//   constants   pi  e
//   operators   + - * / ^   (^ binds tighter than unary minus: -x^2 = -(x^2)), parentheses
//   functions   sin cos tan asin acos atan sinh cosh tanh sqrt abs exp ln log floor ceil sign step
//               pow(a,b) min(a,b) max(a,b) atan2(y,x) mod(a,b)
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "model.hpp"

namespace vlab {

class Expr {
public:
    // Compiles a formula. Returns nullptr and fills *error (a short sentence) if it is not valid.
    static std::shared_ptr<const Expr> parse(const std::string& text, std::string* error = nullptr);
    // Same, but remembers the result by text (the interface asks for the same formulas every frame).
    static std::shared_ptr<const Expr> cached(const std::string& text, std::string* error = nullptr);
    double eval(double x, double L, double t) const;
    bool usesTime() const { return usesT_; }

    struct Node {
        enum Kind { Num, VarX, VarL, VarT, Neg, Add, Sub, Mul, Div, Pow, Fn1, Fn2 } kind = Num;
        double value = 0;
        int fn = 0, a = -1, b = -1;
    };

private:
    std::vector<Node> nodes_;
    int root_ = -1;
    bool usesT_ = false;
    double evalNode(int i, double x, double L, double t) const;
    friend struct ExprParser;
};

// What shape a distributed load has (Body::dloadType).
enum DLoadType { DL_None = 0, DL_Uniform, DL_TriRising, DL_TriFalling, DL_TriPeak, DL_Equation };

// Intensity (N per metre, positive along the load direction) of a body's distributed load at x metres from its left end.
// `ex` is the compiled formula for DL_Equation (may be nullptr otherwise).
double distributedLoadAt(const Body& b, const Expr* ex, double x, double t);
// Span of the load along the body: [from, to] in metres from the left end.
void distributedLoadSpan(const Body& b, double* from, double* to);

}  // namespace vlab
