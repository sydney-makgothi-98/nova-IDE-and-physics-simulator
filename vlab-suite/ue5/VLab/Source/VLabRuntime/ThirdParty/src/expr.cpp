#include "vlab/expr.hpp"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>

namespace vlab {

namespace {
struct Fn1Def { const char* name; double (*f)(double); };
double fSign(double v) { return v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0); }
double fStep(double v) { return v >= 0 ? 1.0 : 0.0; }
double fLog10(double v) { return std::log10(v); }
double fLn(double v) { return std::log(v); }
const Fn1Def kFn1[] = {{"sin", [](double v) { return std::sin(v); }}, {"cos", [](double v) { return std::cos(v); }}, {"tan", [](double v) { return std::tan(v); }},
                       {"asin", [](double v) { return std::asin(v); }}, {"acos", [](double v) { return std::acos(v); }}, {"atan", [](double v) { return std::atan(v); }},
                       {"sinh", [](double v) { return std::sinh(v); }}, {"cosh", [](double v) { return std::cosh(v); }}, {"tanh", [](double v) { return std::tanh(v); }},
                       {"sqrt", [](double v) { return std::sqrt(v); }}, {"abs", [](double v) { return std::fabs(v); }}, {"exp", [](double v) { return std::exp(v); }},
                       {"ln", fLn}, {"log", fLog10}, {"floor", [](double v) { return std::floor(v); }}, {"ceil", [](double v) { return std::ceil(v); }},
                       {"sign", fSign}, {"step", fStep}};
struct Fn2Def { const char* name; double (*f)(double, double); };
const Fn2Def kFn2[] = {{"pow", [](double a, double b) { return std::pow(a, b); }}, {"min", [](double a, double b) { return a < b ? a : b; }},
                       {"max", [](double a, double b) { return a > b ? a : b; }}, {"atan2", [](double a, double b) { return std::atan2(a, b); }},
                       {"mod", [](double a, double b) { return b != 0 ? std::fmod(a, b) : 0.0; }}};
}  // namespace

struct ExprParser {
    const std::string& s;
    size_t p = 0;
    std::vector<Expr::Node>& nodes;
    bool& usesT;
    std::string err;
    ExprParser(const std::string& text, std::vector<Expr::Node>& n, bool& ut) : s(text), nodes(n), usesT(ut) {}

    void skip() { while (p < s.size() && std::isspace((unsigned char)s[p])) ++p; }
    int add(Expr::Node n) { nodes.push_back(n); return (int)nodes.size() - 1; }
    bool fail(const std::string& m) { if (err.empty()) err = m; return false; }

    int parseExpr() {                                    // sum
        int l = parseTerm();
        while (l >= 0) {
            skip();
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) {
                char op = s[p++];
                int r = parseTerm();
                if (r < 0) return -1;
                Expr::Node n;
                n.kind = op == '+' ? Expr::Node::Add : Expr::Node::Sub;
                n.a = l; n.b = r;
                l = add(n);
            } else break;
        }
        return l;
    }
    int parseTerm() {                                    // product (explicit * and /, or implicit: 2x, 3(x+1), 2 sin(x))
        int l = parseUnary();
        while (l >= 0) {
            skip();
            if (p >= s.size()) break;
            char c = s[p];
            Expr::Node n;
            if (c == '*' || c == '/') { ++p; n.kind = c == '*' ? Expr::Node::Mul : Expr::Node::Div; }
            else if (std::isalpha((unsigned char)c) || c == '(') n.kind = Expr::Node::Mul;
            else break;
            int r = parseUnary();
            if (r < 0) return -1;
            n.a = l; n.b = r;
            l = add(n);
        }
        return l;
    }
    int parseUnary() {
        skip();
        if (p < s.size() && (s[p] == '-' || s[p] == '+')) {
            char op = s[p++];
            int v = parseUnary();
            if (v < 0) return -1;
            if (op == '+') return v;
            Expr::Node n;
            n.kind = Expr::Node::Neg;
            n.a = v;
            return add(n);
        }
        return parsePower();
    }
    int parsePower() {
        int base = parseAtom();
        if (base < 0) return -1;
        skip();
        if (p < s.size() && s[p] == '^') {
            ++p;
            int e = parseUnary();                        // right associative, allows 2^-3
            if (e < 0) return -1;
            Expr::Node n;
            n.kind = Expr::Node::Pow;
            n.a = base; n.b = e;
            return add(n);
        }
        return base;
    }
    int parseAtom() {
        skip();
        if (p >= s.size()) { fail("the formula ends too early"); return -1; }
        char c = s[p];
        if (std::isdigit((unsigned char)c) || c == '.') {
            char* end = nullptr;
            double v = std::strtod(s.c_str() + p, &end);
            if (end == s.c_str() + p) { fail("bad number"); return -1; }
            p = (size_t)(end - s.c_str());
            Expr::Node n;
            n.kind = Expr::Node::Num;
            n.value = v;
            return add(n);
        }
        if (c == '(') {
            ++p;
            int v = parseExpr();
            if (v < 0) return -1;
            skip();
            if (p >= s.size() || s[p] != ')') { fail("a closing bracket is missing"); return -1; }
            ++p;
            return v;
        }
        if (std::isalpha((unsigned char)c)) {
            size_t q = p;
            while (q < s.size() && (std::isalnum((unsigned char)s[q]) || s[q] == '_')) ++q;
            std::string name = s.substr(p, q - p);
            for (char& ch : name) ch = (char)std::tolower((unsigned char)ch);
            p = q;
            Expr::Node n;
            if (name == "x") { n.kind = Expr::Node::VarX; return add(n); }
            if (name == "l") { n.kind = Expr::Node::VarL; return add(n); }
            if (name == "t") { n.kind = Expr::Node::VarT; usesT = true; return add(n); }
            if (name == "pi") { n.kind = Expr::Node::Num; n.value = 3.14159265358979323846; return add(n); }
            if (name == "e") { n.kind = Expr::Node::Num; n.value = 2.71828182845904523536; return add(n); }
            skip();
            if (p < s.size() && s[p] == '(') {
                ++p;
                int a = parseExpr();
                if (a < 0) return -1;
                skip();
                int b = -1;
                if (p < s.size() && s[p] == ',') { ++p; b = parseExpr(); if (b < 0) return -1; skip(); }
                if (p >= s.size() || s[p] != ')') { fail("a closing bracket is missing after " + name); return -1; }
                ++p;
                if (b < 0) {
                    for (size_t k = 0; k < sizeof kFn1 / sizeof kFn1[0]; ++k)
                        if (name == kFn1[k].name) { n.kind = Expr::Node::Fn1; n.fn = (int)k; n.a = a; return add(n); }
                } else {
                    for (size_t k = 0; k < sizeof kFn2 / sizeof kFn2[0]; ++k)
                        if (name == kFn2[k].name) { n.kind = Expr::Node::Fn2; n.fn = (int)k; n.a = a; n.b = b; return add(n); }
                }
                fail("unknown function " + name + (b < 0 ? " (with one argument)" : " (with two arguments)"));
                return -1;
            }
            fail("unknown name '" + name + "' (use x, L, t, pi, e)");
            return -1;
        }
        fail(std::string("unexpected character '") + c + "'");
        return -1;
    }
};

std::shared_ptr<const Expr> Expr::parse(const std::string& text, std::string* error) {
    auto e = std::make_shared<Expr>();
    ExprParser ps(text, e->nodes_, e->usesT_);
    int root = ps.parseExpr();
    if (root >= 0) {
        ps.skip();
        if (ps.p < text.size()) { ps.fail(std::string("unexpected '") + text[ps.p] + "'"); root = -1; }
    }
    if (root < 0) {
        if (error) *error = ps.err.empty() ? "empty formula" : ps.err;
        return nullptr;
    }
    e->root_ = root;
    return e;
}

std::shared_ptr<const Expr> Expr::cached(const std::string& text, std::string* error) {
    static std::mutex mu;
    static std::map<std::string, std::pair<std::shared_ptr<const Expr>, std::string>> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto it = cache.find(text);
    if (it == cache.end()) {
        if (cache.size() > 256) cache.clear();
        std::string err;
        auto e = parse(text, &err);
        it = cache.emplace(text, std::make_pair(e, err)).first;
    }
    if (error) *error = it->second.second;
    return it->second.first;
}

double Expr::evalNode(int i, double x, double L, double t) const {
    const Node& n = nodes_[i];
    switch (n.kind) {
        case Node::Num: return n.value;
        case Node::VarX: return x;
        case Node::VarL: return L;
        case Node::VarT: return t;
        case Node::Neg: return -evalNode(n.a, x, L, t);
        case Node::Add: return evalNode(n.a, x, L, t) + evalNode(n.b, x, L, t);
        case Node::Sub: return evalNode(n.a, x, L, t) - evalNode(n.b, x, L, t);
        case Node::Mul: return evalNode(n.a, x, L, t) * evalNode(n.b, x, L, t);
        case Node::Div: { double d = evalNode(n.b, x, L, t); return d != 0 ? evalNode(n.a, x, L, t) / d : 0.0; }
        case Node::Pow: return std::pow(evalNode(n.a, x, L, t), evalNode(n.b, x, L, t));
        case Node::Fn1: return kFn1[n.fn].f(evalNode(n.a, x, L, t));
        case Node::Fn2: return kFn2[n.fn].f(evalNode(n.a, x, L, t), evalNode(n.b, x, L, t));
    }
    return 0;
}

double Expr::eval(double x, double L, double t) const {
    if (root_ < 0) return 0;
    double v = evalNode(root_, x, L, t);
    return std::isfinite(v) ? v : 0.0;                   // a formula that blows up at one point (1/x at 0) gives no load there
}

// ---------------------------------------------------------------------------------------------- the load shapes
void distributedLoadSpan(const Body& b, double* from, double* to) {
    double L = b.shape.w;
    double a = std::max(0.0, std::min(L, b.dloadX0));
    double z = (b.dloadX1 < 0 || b.dloadX1 > L) ? L : b.dloadX1;
    if (z < a) std::swap(a, z);
    *from = a;
    *to = z;
}

double distributedLoadAt(const Body& b, const Expr* ex, double x, double t) {
    if (b.dloadType == DL_None) return 0;
    double a, z;
    distributedLoadSpan(b, &a, &z);
    if (x < a || x > z || z - a < 1e-9) return 0;
    const double w = b.dloadN, s = (x - a) / (z - a);
    switch (b.dloadType) {
        case DL_Uniform: return w;
        case DL_TriRising: return w * s;
        case DL_TriFalling: return w * (1.0 - s);
        case DL_TriPeak: return w * (1.0 - std::fabs(2.0 * s - 1.0));
        case DL_Equation: return ex ? ex->eval(x, b.shape.w, t) : 0.0;
    }
    return 0;
}

}  // namespace vlab
