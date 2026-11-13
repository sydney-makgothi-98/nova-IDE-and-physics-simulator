// model_io.hpp -- saving / loading a design to a small text file, plus ready-made example designs.
//
// File format (one thing per line, '#' starts a comment):
//   vlab 1
//   gravity 9.81
//   mass   x y  m  vx0 vy0  rail  driveF driveAngle driveW
//   ground x y
//   spring a b  k k3 L0 autoRest
//   damper a b  c
#pragma once
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include "engine.hpp"

namespace lag {

inline bool saveModel(const std::string& path, const Model& M) {
    std::ofstream f(path);
    if (!f) return false;
    f.precision(9);
    f << "vlab 1\n# Vibration Lab design file\n";
    f << "gravity " << M.gravity << "\n";
    for (const Node& n : M.nodes) {
        if (n.kind == NodeKind::Mass)
            f << "mass " << n.x << " " << n.y << " " << n.m << " " << n.vx0 << " " << n.vy0 << " " << n.rail << " "
              << n.driveF << " " << n.driveAngle << " " << n.driveW << "\n";
        else
            f << "ground " << n.x << " " << n.y << "\n";
    }
    for (const Link& l : M.links) {
        if (l.kind == LinkKind::Spring)
            f << "spring " << l.a << " " << l.b << " " << l.k << " " << l.k3 << " " << l.L0 << " " << (l.autoRest ? 1 : 0) << "\n";
        else
            f << "damper " << l.a << " " << l.b << " " << l.c << "\n";
    }
    return true;
}

inline bool loadModel(const std::string& path, Model& out) {
    std::ifstream f(path);
    if (!f) return false;
    Model M;
    std::string line;
    bool header = false;
    while (std::getline(f, line)) {
        size_t h = line.find('#');
        if (h != std::string::npos) line.erase(h);
        std::istringstream ss(line);
        std::string word;
        if (!(ss >> word)) continue;
        if (word == "vlab") { header = true; continue; }
        if (word == "gravity") { ss >> M.gravity; }
        else if (word == "mass") {
            Node n; n.kind = NodeKind::Mass;
            ss >> n.x >> n.y >> n.m >> n.vx0 >> n.vy0 >> n.rail >> n.driveF >> n.driveAngle >> n.driveW;
            if (n.m < 0.01) n.m = 0.01;
            if ((int)M.nodes.size() < MAX_NODES) M.nodes.push_back(n);
        } else if (word == "ground") {
            Node n; n.kind = NodeKind::Ground;
            ss >> n.x >> n.y;
            if ((int)M.nodes.size() < MAX_NODES) M.nodes.push_back(n);
        } else if (word == "spring" || word == "damper") {
            Link l; int ar = 1;
            l.a = l.b = -1;
            l.kind = (word == "spring") ? LinkKind::Spring : LinkKind::Damper;
            ss >> l.a >> l.b;
            if (l.kind == LinkKind::Spring) { ss >> l.k >> l.k3 >> l.L0 >> ar; l.autoRest = (ar != 0); }
            else ss >> l.c;
            int n = (int)M.nodes.size();
            if (l.a >= 0 && l.b >= 0 && l.a < n && l.b < n && l.a != l.b) M.links.push_back(l);
        }
    }
    if (!header) return false;
    out = M;
    return true;
}

// ---------------------------------------------------------------- ready-made designs (keys 1-4)
inline Model presetOneDof() {                 // the classic: ground - spring+damper - mass, stretched 1 m then released
    Model M;
    int g = M.addGround(-3, 0), m = M.addMass(0, 0, 2.0);
    M.nodes[m].rail = RAIL_X;
    int s = M.addLink(LinkKind::Spring, g, m), d = M.addLink(LinkKind::Damper, g, m);
    M.links[s].k = 30; M.links[s].L0 = 2.0; M.links[s].autoRest = false;    // rest length 2 m, drawn 3 m -> starts stretched by 1 m
    M.links[d].c = 1.5;
    return M;
}
inline Model presetTwoDof() {                 // two masses on a rail between two walls
    Model M;
    int g1 = M.addGround(-4, 0), a = M.addMass(-1.25, 0, 1.0), b = M.addMass(1.25, 0, 1.0), g2 = M.addGround(4, 0);
    M.nodes[a].rail = M.nodes[b].rail = RAIL_X;
    M.nodes[a].vx0 = 3.0;
    int ids[3][2] = {{g1, a}, {a, b}, {b, g2}};
    double k[3] = {40, 25, 40}, c[3] = {0.6, 0.2, 0.6};
    for (int i = 0; i < 3; ++i) {
        int s = M.addLink(LinkKind::Spring, ids[i][0], ids[i][1]); M.links[s].k = k[i];
        int d = M.addLink(LinkKind::Damper, ids[i][0], ids[i][1]); M.links[d].c = c[i];
    }
    return M;
}
inline Model presetSpringPendulum() {         // a mass on a spring hanging from the ceiling, free to swing and bounce
    Model M;
    M.gravity = 9.81;
    int g = M.addGround(0, -1.8), m = M.addMass(1.6, 0.6, 1.0);
    int s = M.addLink(LinkKind::Spring, g, m), d = M.addLink(LinkKind::Damper, g, m);
    M.links[s].k = 60; M.links[s].L0 = 2.0; M.links[s].autoRest = false;
    M.links[d].c = 0.25;
    return M;
}
inline Model presetQuarterCar() {             // road - tyre spring - wheel - suspension spring+damper - car body
    Model M;
    M.gravity = 9.81;
    int road = M.addGround(0, 2.2), wheel = M.addMass(0, 1.0, 1.0), body = M.addMass(0, -1.0, 4.0);
    M.nodes[wheel].rail = M.nodes[body].rail = RAIL_Y;
    M.nodes[wheel].vy0 = -2.0;                // the wheel hits a bump
    int t = M.addLink(LinkKind::Spring, road, wheel); M.links[t].k = 220;
    int s = M.addLink(LinkKind::Spring, wheel, body); M.links[s].k = 70;
    int d = M.addLink(LinkKind::Damper, wheel, body); M.links[d].c = 5.0;
    return M;
}
inline Model presetByNumber(int n) {
    switch (n) {
        case 1: return presetOneDof();
        case 2: return presetTwoDof();
        case 3: return presetSpringPendulum();
        default: return presetQuarterCar();
    }
}

}  // namespace lag
