// Pose: the rotation R = Ry(pitch) * Rx(roll) and the translation that place the ship frame in the world.
#pragma once

#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

template <class Math>
SS_HD inline Frame pose_frame(const Body& b, Real zO, Real th, Real ph) {
  const Real cth = Math::cos(th), sth = Math::sin(th), cph = Math::cos(ph), sph = Math::sin(ph);
  Frame F;
  F.R00 = cth; F.R01 = sth * sph; F.R02 = sth * cph;
  F.R10 = 0;   F.R11 = cph;       F.R12 = -sph;
  F.R20 = -sth; F.R21 = cth * sph; F.R22 = cth * cph;
  F.tx = b.XO - (F.R00 * b.g0x + F.R01 * b.g0y + F.R02 * b.g0z);
  F.ty = b.YO - (F.R10 * b.g0x + F.R11 * b.g0y + F.R12 * b.g0z);
  F.tz = zO - (F.R20 * b.g0x + F.R21 * b.g0y + F.R22 * b.g0z);
  F.cth = cth;
  return F;
}

// World height of a ship-frame point.
SS_HD inline Real world_z(const Frame& F, Real x, Real y, Real z) {
  return F.R20 * x + F.R21 * y + F.R22 * z + F.tz;
}

}  // namespace sinksim::kernel
