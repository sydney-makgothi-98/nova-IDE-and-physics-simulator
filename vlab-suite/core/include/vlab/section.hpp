// section.hpp -- the CROSS-SECTION of a beam: area, centroid, second moment of area I, section modulus, and the stress distribution through it.
//
// A section is described by its type and a few dimensions (Body::secType, secB, secH, secTf, secTw), seen end-on, bent about its horizontal
// centroidal axis. y is measured DOWN from the top face. Everything is computed by slicing the section into 400 thin strips of width b(y).
//   sagging moment M > 0: the top face is in compression and the bottom in tension.  sigma(y) = N/A + M (y - yc) / I   (tension positive)
//   shear stress          tau(y) = V Q(y) / (I b(y)),  Q(y) = first moment about the neutral axis of the part of the section above y
#pragma once
#include <string>
#include <vector>
#include "model.hpp"

namespace vlab {

enum SecType { SEC_SLAB = 0, SEC_RECT, SEC_CIRCLE, SEC_BOX, SEC_TUBE, SEC_I, SEC_T, SEC_CHANNEL, SEC_COUNT };

const char* sectionName(int type);
// which dimension fields a type uses: bit 0 width, bit 1 height / diameter, bit 2 flange / wall thickness, bit 3 web thickness
int sectionDims(int type);

struct Section {
    int n = 0;
    double H = 0, dy = 0;               // height, strip height (m)
    double A = 0, yc = 0, I = 0;        // area (m^2), centroid depth below the top face (m), second moment about the centroidal axis (m^4)
    double cTop = 0, cBot = 0, cMax = 0;   // distances from the neutral axis to the top face, the bottom face, and the farther of the two
    double Z = 0, rg = 0;               // section modulus I / cMax (m^3), radius of gyration sqrt(I / A) (m)
    std::vector<double> b;              // width of strip k, centred at y_k = (k + 0.5) dy
    double y(int k) const { return (k + 0.5) * dy; }
};

Section makeSection(const Body& b);
double sectionHeight(const Body& b);    // the beam's height: shape.h for a plain slab, otherwise secH

struct SectionStress {
    std::vector<double> y;              // depth below the top face of each strip (m)
    std::vector<double> sigma, tau;     // normal stress (Pa, tension +) and shear stress (Pa) at each strip
    double sigMax = 0, sigMin = 0, tauMax = 0;
};
// Stresses for a bending moment M (N m, sagging +), shear force V (N) and axial force N (N, tension +).
void sectionStress(const Section& s, double M, double V, double N, SectionStress& out);

}  // namespace vlab
