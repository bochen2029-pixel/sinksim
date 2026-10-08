// Numerical constants of the time-stepping scheme ("scheme v1": the explicit, limited scheme of the JS oracle).
// These are properties of the method, not of a ship, and the calibration depends on them. Changing any
// of them is a physics change: it needs a new scheme version, a regenerated golden trace and a recalibration
// (see docs/DETERMINISM.md and the package HANDOFF). The step size dt is data and lives in the compiled sim.
#pragma once

#include "sinksim/config.hpp"

namespace sinksim::scheme {

inline constexpr int kVersion = 1;

inline constexpr Real kEmptyVolume = 1e-6;           // m3: below this a space has no free surface and no head
inline constexpr Real kFullFraction = 1.0 - 1e-7;     // a space at vmax * kFullFraction is full (pressed up)
inline constexpr Real kLimiterShare = 0.5;            // a flow moves at most this share of the levelling volume
inline constexpr Real kOrificeHeadFloor = 0.01;       // m: the orifice law is linearised below this head
inline constexpr Real kWeirCoefficient = 2.0 / 3.0;   // broad-crested weir prefactor
inline constexpr Real kWeirExponent = 1.5;            // Villemonte: (H2/H1)^1.5
inline constexpr Real kVillemonteExponent = 0.385;    // Villemonte: (1 - r^1.5)^0.385
inline constexpr int kNewtonFast = 6;                 // free-surface iterations per step
inline constexpr int kNewtonFull = 40;                // free-surface iterations at equilibration and diagnostics
inline constexpr Real kSlopeMin = 1e-9;               // m2: below this the Newton update falls back to bisection
inline constexpr Real kCentroidMinVolume = 1e-9;      // m3: centroids are only updated above this volume
inline constexpr Real kNoHead = -1e9;                 // level assigned to an empty space
inline constexpr Real kNoHeadTest = -1e8;             // "is empty" test on an effective level
inline constexpr Real kPitchClamp = 1.35;             // rad, about 77 degrees
inline constexpr Real kRollClamp = 1.4;               // rad, about 80 degrees
inline constexpr Real kPi = 3.141592653589793;

}  // namespace sinksim::scheme
