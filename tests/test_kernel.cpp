// Kernel unit tests on a synthetic box barge whose hydrostatics are known in closed form, run with the oracle
// numerics and with the portable numerics (GPU reduction order), which must agree to rounding.
#include <cmath>
#include <vector>

#include "sinksim/kernel/readouts.hpp"
#include "sinksim/kernel/step.hpp"
#include "support/check.hpp"

using namespace sinksim;

namespace {

constexpr Real kRho = 1025, kG = 9.81, kDt = 0.25;

// A box barge L x B x H split into `parts` nodes along its length, columns dx by dy, draft T.
struct Rig {
  Real L, B, H, T;
  // geometry
  std::vector<Real> x, y, dxdy, zlo, zhi, mu, vmax, amin, aov, segA, segE;
  std::vector<Index> segStart, segCount, segCol;
  // network
  std::vector<Index> gStart, gCount, gNodes;
  std::vector<Byte> gMode;
  std::vector<Real> gpx, gpy, gpz;
  std::vector<Index> ca, cb, cIdx, cSkip, cMon;
  std::vector<Byte> cLaw, cKind, cEn;
  std::vector<Real> cx, cy, cz, cArea, cCoef, cTOn, q, dV;
  std::vector<Index> incStart, incCount, incConn, incSea;
  std::vector<Byte> incSide;
  // state and scratch
  std::vector<Real> vol, level, area, ccx, ccy, ccz, zmin, zmax, hEff, aEff, acc, excess;
  std::vector<Index> deg;
  std::vector<Byte> merged;
  std::vector<NodePass> passes;
  Event evItems[1];
  Real overT[1], markT[1];

  ShipView S{};
  SimView M{};
  State st{};
  Scratch sc{};
  Outputs out{};
  EventLog ev{};

  Rig(Real L_, Real B_, Real H_, Real T_, Real dx, Real dy, int parts, bool sharedSurface) : L(L_), B(B_), H(H_), T(T_) {
    const int nx = static_cast<int>(std::lround(L / dx)), ny = static_cast<int>(std::lround(B / dy));
    for (int k = 0; k < parts; ++k) {
      segStart.push_back(static_cast<Index>(segCol.size()));
      Real footprint = 0;
      for (int i = 0; i < nx; ++i) {
        if (i * parts / nx != k) continue;
        for (int j = 0; j < ny; ++j) {
          const Index col = static_cast<Index>(x.size());
          x.push_back(-L / 2 + (i + 0.5) * dx); y.push_back(-B / 2 + (j + 0.5) * dy);
          dxdy.push_back(dx * dy); zlo.push_back(0); zhi.push_back(H);
          segCol.push_back(col); segA.push_back(0); segE.push_back(H);
          footprint += dx * dy;
        }
      }
      segCount.push_back(static_cast<Index>(segCol.size()) - segStart.back());
      mu.push_back(1); vmax.push_back(footprint * H); amin.push_back(4); aov.push_back(std::fmax(1.0, 0.03 * footprint));
    }
    if (sharedSurface) {
      gStart.push_back(0); gCount.push_back(parts);
      for (int k = 0; k < parts; ++k) gNodes.push_back(k);
      gMode.push_back(static_cast<Byte>(GroupMode::Always)); gpx.push_back(0); gpy.push_back(0); gpz.push_back(0);
    } else {
      for (int k = 0; k < parts; ++k) {
        gStart.push_back(k); gCount.push_back(1); gNodes.push_back(k);
        gMode.push_back(static_cast<Byte>(GroupMode::Single)); gpx.push_back(0); gpy.push_back(0); gpz.push_back(0);
      }
    }
    const auto nn = static_cast<std::size_t>(parts);
    vol.assign(nn, 0); level.assign(nn, 0); area.assign(nn, 0); ccx.assign(nn, 0); ccy.assign(nn, 0); ccz.assign(nn, 0);
    zmin.assign(nn, 0); zmax.assign(nn, 0); hEff.assign(nn, 0); aEff.assign(nn, 0); acc.assign(nn, 0); excess.assign(nn, 0); deg.assign(nn, 0);
    merged.assign(gCount.size(), 0); passes.assign(nn, NodePass{});
    st.t = 0; st.zO = -T; st.pitch = 0; st.roll = 0; st.vz = 0; st.wth = 0; st.wph = 0;
    overT[0] = -1; markT[0] = -1;
  }

  void connect(Index a, Index b, FlowLaw law, Real px, Real py, Real pz, Real areaOrWidth, Real coef) {
    ca.push_back(a); cb.push_back(b); cLaw.push_back(static_cast<Byte>(law)); cx.push_back(px); cy.push_back(py); cz.push_back(pz);
    cArea.push_back(areaOrWidth); cCoef.push_back(coef); cKind.push_back(0); cIdx.push_back(-1); cEn.push_back(1); cTOn.push_back(0);
    cSkip.push_back(-1); cMon.push_back(-1); q.push_back(0); dV.push_back(0);
  }

  void build_incidence() {
    const std::size_t nn = vol.size();
    incCount.assign(nn, 0); incStart.assign(nn, 0); incConn.clear(); incSide.clear(); incSea.clear();
    for (std::size_t c = 0; c < ca.size(); ++c) { if (ca[c] >= 0) incCount[static_cast<std::size_t>(ca[c])]++; incCount[static_cast<std::size_t>(cb[c])]++; }
    Index total = 0;
    for (std::size_t n = 0; n < nn; ++n) { incStart[n] = total; total += incCount[n]; }
    incConn.assign(static_cast<std::size_t>(total), -1); incSide.assign(static_cast<std::size_t>(total), 0);
    std::vector<Index> fill(nn, 0);
    for (std::size_t c = 0; c < ca.size(); ++c) {
      if (ca[c] >= 0) { const auto n = static_cast<std::size_t>(ca[c]); const auto k = static_cast<std::size_t>(incStart[n] + fill[n]++); incConn[k] = static_cast<Index>(c); incSide[k] = 0; }
      else incSea.push_back(static_cast<Index>(c));
      const auto n = static_cast<std::size_t>(cb[c]); const auto k = static_cast<std::size_t>(incStart[n] + fill[n]++); incConn[k] = static_cast<Index>(c); incSide[k] = 1;
    }
  }

  void bind() {
    build_incidence();
    S.cols = ColumnsView{static_cast<Index>(x.size()), x.data(), y.data(), dxdy.data(), zlo.data(), zhi.data()};
    S.segs = SegmentsView{static_cast<Index>(segCol.size()), segCol.data(), segA.data(), segE.data()};
    S.nodes = NodesView{static_cast<Index>(mu.size()), mu.data(), vmax.data(), amin.data(), aov.data(), segStart.data(), segCount.data()};
    M.phys = Physics{kRho, kG, std::sqrt(2 * kG)};
    M.dt = kDt;
    M.groups = GroupsView{static_cast<Index>(gCount.size()), gStart.data(), gCount.data(), gNodes.data(), gMode.data(), gpx.data(), gpy.data(), gpz.data(), 0.05};
    M.conns = ConnectionsView{static_cast<Index>(ca.size()), ca.data(), cb.data(), cLaw.data(), cx.data(), cy.data(), cz.data(), cArea.data(), cCoef.data(),
                              cKind.data(), cIdx.data(), cEn.data(), cTOn.data(), cSkip.data(), cMon.data(), q.data()};
    M.inc = IncidenceView{incStart.data(), incCount.data(), incConn.data(), incSide.data(), static_cast<Index>(incSea.size()), incSea.data()};
    M.monitors = MonitorsView{0, 0.05};
    M.marks = MarksView{0, nullptr, nullptr, nullptr, nullptr};
    M.founder = FounderRule{-1e9, 10, -1, 1e9};
    // Mass properties of the box with the origin at the keel (KG = 0), added mass as the oracle assumes, and
    // damping at 60 % of critical so that the barge settles within the test horizon.
    const Real ms = kRho * L * B * T;
    const Real mh0 = 2 * ms, Ith0 = 2 * ms * (0.25 * L) * (0.25 * L), Iph0 = 1.2 * ms * (0.38 * B) * (0.38 * B);
    const Real Awp = L * B, V = L * B * T;
    const Real BML = (L * L * L * B / 12) / V, BMT = (L * B * B * B / 12) / V, GM = T / 2 + BMT;
    const Real Kz = kRho * kG * Awp, Kth = kRho * kG * V * BML, Kph = kRho * kG * V * GM;
    const Real cDz = 2 * 0.6 * std::sqrt(Kz * mh0), cDth = 2 * 0.6 * std::sqrt(Kth * Ith0), cDph = 2 * 0.6 * std::sqrt(Kph * Iph0);
    M.body = Body{ms, 0, 0, 0, 0, 0, 0, mh0, Ith0, Iph0, cDz, cDth, cDph, 0, 0, 0};
    st.vol = vol.data(); st.level = level.data();
    sc = Scratch{area.data(), ccx.data(), ccy.data(), ccz.data(), zmin.data(), zmax.data(), hEff.data(), aEff.data(), acc.data(), excess.data(),
                 deg.data(), merged.data(), passes.data(), nullptr, dV.data()};
    ev = EventLog{evItems, 1, 0, overT, markT, 0, -1};
  }

  Frame frame() const { return kernel::pose_frame<OracleMath>(M.body, st.zO, st.pitch, st.roll); }
  Real total() const { Real s = 0; for (const Real v : vol) s += v; return s; }
};

template <class Math, bool Tree>
void breach_run(Rig& rig, int steps) {
  for (int i = 0; i < steps; ++i) kernel::step<Math, Tree>(rig.S, rig.M, rig.st, rig.sc, rig.out, rig.ev);
}

}  // namespace

int main() {
  // Rotation matrices are orthonormal and the translation reduces to heave when the origin is at g0 = 0.
  {
    Body b{};
    const Frame F = kernel::pose_frame<OracleMath>(b, 3.5, 0.1, -0.2);
    const Real r[3][3] = {{F.R00, F.R01, F.R02}, {F.R10, F.R11, F.R12}, {F.R20, F.R21, F.R22}};
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) {
        Real dot = 0;
        for (int k = 0; k < 3; ++k) dot += r[i][k] * r[j][k];
        CHECK_NEAR(dot, i == j ? 1.0 : 0.0, 1e-15);
      }
    CHECK_EQ(F.tz, 3.5);
    CHECK_NEAR(kernel::world_z(F, 0, 0, 0), 3.5, 0.0);
    // the three math policies agree on the pose to rounding
    const Frame P = kernel::pose_frame<PortableMath>(b, 3.5, 0.1, -0.2), Q = kernel::pose_frame<StdMath>(b, 3.5, 0.1, -0.2);
    CHECK_EQ(F.R22, P.R22);
    CHECK_NEAR(F.R22, Q.R22, 1e-15);
  }
  // Buoyancy of a box at even keel, in both summation orders
  {
    Rig rig(100, 20, 10, 4, 5, 1, 2, false);
    rig.bind();
    const Buoyancy hb = kernel::hydro_pass(rig.S.cols, rig.frame());
    CHECK_NEAR(hb.V, 100.0 * 20 * 4, 1e-9);
    CHECK_NEAR(hb.z, 2.0, 1e-12);
    CHECK_NEAR(hb.x, 0.0, 1e-9);
    CHECK_NEAR(hb.y, 0.0, 1e-9);
    CHECK_NEAR(hb.top, 10.0 - 4, 1e-12);
    const Buoyancy ht = kernel::hydro_pass_tree<kernel::kBlockThreads>(rig.S.cols, rig.frame());
    CHECK_NEAR(ht.V, hb.V, 1e-9);
    CHECK_NEAR(ht.z, hb.z, 1e-12);
    CHECK_EQ(ht.top, hb.top);
    // node 0 is the aft half (x from -50 to 0): at world level -1 the water stands 3 m deep in it
    const NodePass p = kernel::node_pass(rig.S, rig.frame(), 0, -1);
    CHECK_NEAR(p.vol, 50.0 * 20 * 3, 1e-9);
    CHECK_NEAR(p.dv, 50.0 * 20, 1e-9);
    CHECK_NEAR(p.zmin, -4.0, 1e-12);
    CHECK_NEAR(p.zmax, 6.0, 1e-12);
    CHECK_NEAR(p.mx, p.vol * -25.0, 1e-6);
    const NodePass pt = kernel::node_pass_tree<kernel::kWarpLanes>(rig.S, rig.frame(), 0, -1);
    CHECK_NEAR(pt.vol, p.vol, 1e-9);
    CHECK_NEAR(pt.dv, p.dv, 1e-9);
    CHECK_EQ(pt.zmin, p.zmin);
    CHECK_EQ(pt.zmax, p.zmax);
  }
  // Free-surface solve: a known volume gives back the level that produced it
  {
    Rig rig(100, 20, 10, 4, 5, 1, 2, false);
    rig.bind();
    rig.vol[0] = 50.0 * 20 * 3;
    rig.level[0] = 0;
    const Frame F = rig.frame();
    const kernel::SerialPasser pass{rig.S, F};
    kernel::solve_levels(rig.S.nodes, rig.M.groups, F, pass, false, rig.st, rig.sc);
    CHECK_NEAR(rig.level[0], -1.0, 1e-9);
    CHECK_NEAR(rig.area[0], 1000.0, 1e-9);
    CHECK_NEAR(rig.ccz[0], 1.5, 1e-9);
    CHECK_NEAR(rig.level[1], -4.0, 1e-12);   // empty node: level rests on its floor
    CHECK_EQ(rig.area[1], 0.0);
  }
  // A shared surface splits the water between two identical halves
  {
    Rig rig(100, 20, 10, 4, 5, 1, 2, true);
    rig.bind();
    rig.vol[0] = 2000; rig.vol[1] = 0; rig.level[0] = -2; rig.level[1] = -2;
    const Frame F = rig.frame();
    const kernel::SerialPasser pass{rig.S, F};
    kernel::solve_levels(rig.S.nodes, rig.M.groups, F, pass, false, rig.st, rig.sc);
    CHECK_EQ(rig.merged[0], static_cast<Byte>(1));
    CHECK_NEAR(rig.vol[0], 1000.0, 1e-9);
    CHECK_NEAR(rig.vol[1], 1000.0, 1e-9);
    CHECK_NEAR(rig.vol[0] + rig.vol[1], 2000.0, 0.0);
    CHECK_EQ(rig.level[0], rig.level[1]);
    CHECK_NEAR(rig.level[0], -4.0 + 1.0, 1e-9);   // 2000 m3 over 2000 m2 is 1 m above the floor
  }
  // Flow through an orifice between two nodes conserves mass and runs downhill
  {
    Rig rig(100, 20, 10, 4, 5, 1, 2, false);
    rig.connect(0, 1, FlowLaw::Orifice, 0, 0, 0.2, 1.0, 0.6);
    rig.bind();
    rig.vol[0] = 2000; rig.vol[1] = 0;
    const Frame F = rig.frame();
    const kernel::SerialPasser pass{rig.S, F};
    kernel::solve_levels(rig.S.nodes, rig.M.groups, F, pass, false, rig.st, rig.sc);
    const Real seaIn = kernel::compute_flows<OracleMath>(rig.S, rig.M, F, rig.st, rig.sc);
    CHECK_EQ(seaIn, 0.0);
    CHECK(rig.q[0] > 0);
    CHECK(rig.acc[1] > 0);
    CHECK_EQ(rig.acc[0], -rig.acc[1]);
    CHECK(rig.acc[1] <= 2000);
  }
  // A breach into the forward half lets the sea in; the inflow accounting matches the water that appears; a
  // floor-level orifice passes water aft; the barge settles deeper and by the head. Both numerics agree.
  Real totalOracle = 0, pitchOracle = 0;
  for (int pass = 0; pass < 2; ++pass) {
    Rig rig(100, 20, 10, 4, 5, 1, 2, false);
    rig.connect(-1, 1, FlowLaw::Orifice, 40, -10, 1.0, 0.5, 0.6);   // sea -> forward node, 3 m below the waterline
    rig.connect(1, 0, FlowLaw::Orifice, 0, 0, 0.0, 1.0, 0.6);       // forward -> aft, at floor level amidships
    rig.bind();
    if (pass == 0) breach_run<OracleMath, false>(rig, 1); else breach_run<PortableMath, true>(rig, 1);
    CHECK_EQ(rig.st.t, kDt);
    CHECK(rig.vol[1] > 0);
    CHECK_EQ(rig.vol[0], 0.0);
    CHECK_NEAR(rig.out.inflow * kDt, rig.vol[1], 0.0);
    CHECK_NEAR(rig.out.Vb, 100.0 * 20 * 4, 1e-6);
    CHECK(rig.st.vz <= 0);                 // the water admitted this step already weighs her down
    CHECK(std::fabs(rig.st.vz) < 1e-3);
    Real seaTotal = rig.out.inflow * kDt;
    for (int i = 0; i < 1200; ++i) {
      if (pass == 0) breach_run<OracleMath, false>(rig, 1); else breach_run<PortableMath, true>(rig, 1);
      seaTotal += rig.out.inflow * kDt;
    }
    CHECK(rig.vol[0] > 0);
    CHECK(rig.vol[1] > rig.vol[0]);
    CHECK_NEAR(rig.total(), seaTotal, 1e-8 * seaTotal);
    CHECK(rig.st.zO < -4);   // she sits deeper with water aboard
    CHECK(rig.st.pitch > 0); // and by the head, the breach being forward
    CHECK_EQ(rig.ev.foundered, static_cast<Byte>(0));
    const kernel::Readouts r = kernel::readouts<OracleMath>(rig.S, rig.M, ReadoutGeometry{50, -50}, rig.st, rig.out);
    CHECK_NEAR(r.waterT, rig.total() * kRho / 1000, 1e-9);
    CHECK(r.draftF > r.draftA);
    if (pass == 0) { totalOracle = rig.total(); pitchOracle = rig.st.pitch; }
    else {
      CHECK_NEAR(rig.total(), totalOracle, 1e-6 * totalOracle);
      CHECK_NEAR(rig.st.pitch, pitchOracle, 1e-9);
    }
  }
  return test::finish("test_kernel");
}
