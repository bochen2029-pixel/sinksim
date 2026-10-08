#include "sinksim/cuda/batch.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include <cuda_runtime.h>

#include "device_step.cuh"

namespace sinksim::cuda {

namespace {

void check(cudaError_t e, const char* what) {
  if (e != cudaSuccess) throw std::runtime_error(std::string("CUDA: ") + what + ": " + cudaGetErrorString(e));
}

template <class T>
class DeviceArray {
 public:
  DeviceArray() = default;
  ~DeviceArray() { release(); }
  DeviceArray(const DeviceArray&) = delete;
  DeviceArray& operator=(const DeviceArray&) = delete;
  void allocate(std::size_t n) {
    release();
    n_ = n;
    if (n == 0) return;
    check(cudaMalloc(&p_, n * sizeof(T)), "cudaMalloc");
  }
  void upload(const std::vector<T>& v) {
    allocate(v.size());
    if (!v.empty()) check(cudaMemcpy(p_, v.data(), v.size() * sizeof(T), cudaMemcpyHostToDevice), "cudaMemcpy to device");
  }
  void upload_at(std::size_t offset, const T* src, std::size_t n) {
    if (n) check(cudaMemcpy(p_ + offset, src, n * sizeof(T), cudaMemcpyHostToDevice), "cudaMemcpy to device");
  }
  std::vector<T> download() const {
    std::vector<T> v(n_);
    if (n_) check(cudaMemcpy(v.data(), p_, n_ * sizeof(T), cudaMemcpyDeviceToHost), "cudaMemcpy to host");
    return v;
  }
  void download_into(std::vector<T>& v) const { v = download(); }
  T* get() const { return p_; }
  std::size_t size() const { return n_; }

 private:
  T* p_ = nullptr;
  std::size_t n_ = 0;
  void release() {
    if (p_) cudaFree(p_);
    p_ = nullptr;
    n_ = 0;
  }
};

template <class T>
std::vector<T> repeat(const std::vector<T>& v, std::size_t times) {
  std::vector<T> out;
  out.reserve(v.size() * times);
  for (std::size_t i = 0; i < times; ++i) out.insert(out.end(), v.begin(), v.end());
  return out;
}

}  // namespace

bool available(std::string* why) {
  int n = 0;
  const cudaError_t e = cudaGetDeviceCount(&n);
  if (e != cudaSuccess) {
    if (why) *why = cudaGetErrorString(e);
    return false;
  }
  if (n == 0) {
    if (why) *why = "no CUDA device";
    return false;
  }
  return true;
}

DeviceInfo device_info() {
  DeviceInfo d;
  cudaDeviceProp p{};
  check(cudaGetDeviceProperties(&p, 0), "cudaGetDeviceProperties");
  d.name = p.name;
  d.major = p.major; d.minor = p.minor; d.multiprocessors = p.multiProcessorCount;
  d.globalMemory = p.totalGlobalMem; d.sharedPerBlockOptIn = p.sharedMemPerBlockOptin;
  cudaRuntimeGetVersion(&d.runtimeVersion);
  cudaDriverGetVersion(&d.driverVersion);
  return d;
}

struct Batch::Impl {
  std::shared_ptr<const CompiledShip> ship;
  CompiledSim sim;
  Incidence inc;
  BatchOptions opt;
  Index NN = 0, NC = 0, NG = 0, nMon = 0, nMarks = 0, evCap = 0, maxGroup = 0;
  SmemLayout layout{};
  int recSize = 0, maxSamples = 0;
  std::int64_t totalSteps = 0;

  // ship
  DeviceArray<Real> x, y, dxdy, zlo, zhi, segA, segE, mu, vmax, amin, aov;
  DeviceArray<Index> segCol, segStart, segCount;
  // sim (immutable)
  DeviceArray<Index> gNodeStart, gNodeCount, gNodes, ca, cb, cIdx, cSkip, cMon, incStart, incCount, incConn, incSea;
  DeviceArray<Byte> gMode, cLaw, cKind, incSide, mkWhen;
  DeviceArray<Real> gpx, gpy, gpz, cx, cy, cz, cCoef, mkx, mky, mkz;
  // per instance
  DeviceArray<Byte> en, foundered;
  DeviceArray<Real> area, tOn, t, zO, pitch, roll, vz, wth, wph, vol, level, ccx, ccy, ccz, inflow, Vb, hullTop, mw, bx, by, bz, overT, markT, founderT, nextSample, denseRec, sampleRec;
  DeviceArray<Event> evItems;
  DeviceArray<Index> evCount;
  DeviceArray<long long> steps;
  DeviceArray<int> denseCount, sampleCount;
  int curveMax = 0;
  DeviceArray<Real> curveRec, nextCurve;
  DeviceArray<int> curveCount;

  // host staging of per-instance mutable arrays before run()
  std::vector<Byte> hEn;
  std::vector<Real> hArea, hTOn, hT, hZO, hPitch, hRoll, hVz, hWth, hWph, hVol, hLevel, hCx, hCy, hCz;
  bool ran = false;

  ShipView shipView() const {
    ShipView v;
    v.cols = ColumnsView{ship->columns(), x.get(), y.get(), dxdy.get(), zlo.get(), zhi.get()};
    v.segs = SegmentsView{ship->segments(), segCol.get(), segA.get(), segE.get()};
    v.nodes = NodesView{NN, mu.get(), vmax.get(), amin.get(), aov.get(), segStart.get(), segCount.get()};
    return v;
  }

  DevSim devSim() const {
    DevSim d;
    d.phys = sim.phys; d.dt = sim.dt;
    d.groups = GroupsView{NG, gNodeStart.get(), gNodeCount.get(), gNodes.get(), gMode.get(), gpx.get(), gpy.get(), gpz.get(), sim.gMargin};
    d.nConn = NC; d.a = ca.get(); d.b = cb.get(); d.law = cLaw.get(); d.x = cx.get(); d.y = cy.get(); d.z = cz.get(); d.coef = cCoef.get();
    d.kind = cKind.get(); d.idx = cIdx.get(); d.skipGroup = cSkip.get(); d.monitor = cMon.get();
    d.inc = IncidenceView{incStart.get(), incCount.get(), incConn.get(), incSide.get(), static_cast<Index>(inc.sea.size()), incSea.get()};
    d.monitors = MonitorsView{nMon, sim.monitorThreshold};
    d.marks = MarksView{nMarks, mkx.get(), mky.get(), mkz.get(), mkWhen.get()};
    d.founder = sim.founder; d.body = sim.body; d.readout = sim.readout;
    d.nNodes = NN; d.nGroups = NG; d.maxGroup = maxGroup; d.nMon = nMon; d.nMarks = nMarks; d.evCap = evCap;
    return d;
  }

  DevBatch devBatch() const {
    DevBatch b;
    b.count = opt.count;
    b.en = en.get(); b.area = area.get(); b.tOn = tOn.get();
    b.t = t.get(); b.zO = zO.get(); b.pitch = pitch.get(); b.roll = roll.get(); b.vz = vz.get(); b.wth = wth.get(); b.wph = wph.get();
    b.vol = vol.get(); b.level = level.get(); b.cx = ccx.get(); b.cy = ccy.get(); b.cz = ccz.get();
    b.inflow = inflow.get(); b.Vb = Vb.get(); b.hullTop = hullTop.get(); b.mw = mw.get(); b.bx = bx.get(); b.by = by.get(); b.bz = bz.get();
    b.evItems = evItems.get(); b.evCount = evCount.get(); b.overT = overT.get(); b.markT = markT.get(); b.foundered = foundered.get(); b.founderT = founderT.get();
    b.steps = steps.get();
    b.traced = opt.tracedInstances; b.denseSteps = opt.denseSteps; b.sampleEvery = opt.sampleEvery; b.maxSamples = maxSamples; b.recSize = recSize;
    b.denseRec = denseRec.get(); b.sampleRec = sampleRec.get();
    b.denseCount = denseCount.get(); b.sampleCount = sampleCount.get(); b.nextSample = nextSample.get();
    b.curveEvery = opt.curveEvery; b.curveMax = curveMax;
    b.curveRec = curveRec.get(); b.curveCount = curveCount.get(); b.nextCurve = nextCurve.get();
    return b;
  }
};

Batch::Batch(std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, const BatchOptions& opt) : impl_(new Impl) {
  Impl& I = *impl_;
  if (!ship) throw std::invalid_argument("cuda::Batch: null ship");
  if (opt.count < 1) throw std::invalid_argument("cuda::Batch: count must be at least 1");
  std::string why;
  if (!available(&why)) throw std::runtime_error("cuda::Batch: " + why);
  I.ship = std::move(ship);
  I.sim = sim;
  I.inc = build_incidence(sim);
  I.opt = opt;
  if (I.opt.tracedInstances > I.opt.count) I.opt.tracedInstances = I.opt.count;
  I.NN = I.ship->nodes(); I.NC = sim.connections(); I.NG = sim.groups();
  I.nMon = sim.monitors(); I.nMarks = sim.marks(); I.evCap = I.nMon + I.nMarks + 1; I.maxGroup = sim.maxGroupSize();
  if (sim.nodes() != I.NN) throw std::invalid_argument("cuda::Batch: ship and simulation disagree on the node count");
  I.layout = smem_layout(I.NN, I.NC, I.NG, I.nMon, I.nMarks, I.maxGroup, I.evCap);
  const DeviceInfo info = device_info();
  if (I.layout.total > info.sharedPerBlockOptIn)
    throw std::runtime_error("cuda::Batch: this model needs " + std::to_string(I.layout.total) + " bytes of shared memory per block, the device allows " + std::to_string(info.sharedPerBlockOptIn));
  I.recSize = 10 + 5 * I.NN;
  I.maxSamples = static_cast<int>(std::floor(I.opt.tMax / I.opt.sampleEvery)) + 2;

  // ship
  I.x.upload(I.ship->x); I.y.upload(I.ship->y); I.dxdy.upload(I.ship->dxdy); I.zlo.upload(I.ship->zlo); I.zhi.upload(I.ship->zhi);
  I.segCol.upload(I.ship->segCol); I.segA.upload(I.ship->segA); I.segE.upload(I.ship->segE);
  I.mu.upload(I.ship->mu); I.vmax.upload(I.ship->vmax); I.amin.upload(I.ship->amin); I.aov.upload(I.ship->aov);
  I.segStart.upload(I.ship->segStart); I.segCount.upload(I.ship->segCount);
  // sim
  I.gNodeStart.upload(sim.gNodeStart); I.gNodeCount.upload(sim.gNodeCount); I.gNodes.upload(sim.gNodes); I.gMode.upload(sim.gMode);
  I.gpx.upload(sim.gpx); I.gpy.upload(sim.gpy); I.gpz.upload(sim.gpz);
  I.ca.upload(sim.ca); I.cb.upload(sim.cb); I.cLaw.upload(sim.cLaw); I.cx.upload(sim.cx); I.cy.upload(sim.cy); I.cz.upload(sim.cz);
  I.cCoef.upload(sim.cCoef); I.cKind.upload(sim.cKind); I.cIdx.upload(sim.cIdx); I.cSkip.upload(sim.cSkipGroup); I.cMon.upload(sim.cMonitor);
  I.incStart.upload(I.inc.start); I.incCount.upload(I.inc.count); I.incConn.upload(I.inc.conn); I.incSide.upload(I.inc.side); I.incSea.upload(I.inc.sea);
  I.mkx.upload(sim.mkx); I.mky.upload(sim.mky); I.mkz.upload(sim.mkz); I.mkWhen.upload(sim.mkWhen);
  // per instance, staged on the host until run()
  const auto count = static_cast<std::size_t>(I.opt.count);
  I.hEn = repeat(sim.cEn, count); I.hArea = repeat(sim.cArea, count); I.hTOn = repeat(sim.cTOn, count);
  I.hT.assign(count, sim.t0); I.hZO.assign(count, sim.zO0); I.hPitch.assign(count, sim.pitch0); I.hRoll.assign(count, sim.roll0);
  I.hVz.assign(count, sim.vz0); I.hWth.assign(count, sim.wth0); I.hWph.assign(count, sim.wph0);
  I.hVol = repeat(sim.vol0, count); I.hLevel = repeat(sim.level0, count); I.hCx = repeat(sim.cx0, count); I.hCy = repeat(sim.cy0, count); I.hCz = repeat(sim.cz0, count);
}

Batch::~Batch() = default;

const BatchOptions& Batch::options() const { return impl_->opt; }
std::int64_t Batch::total_steps() const { return impl_->totalSteps; }
std::size_t Batch::shared_memory_bytes() const { return impl_->layout.total; }

void Batch::restore(Index instance, const StateSnapshot& s) {
  Impl& I = *impl_;
  if (I.ran) throw std::logic_error("cuda::Batch: restore after run");
  if (instance < 0 || instance >= I.opt.count) throw std::out_of_range("cuda::Batch: instance");
  const auto i = static_cast<std::size_t>(instance);
  const auto off = i * static_cast<std::size_t>(I.NN);
  I.hT[i] = s.t; I.hZO[i] = s.zO; I.hPitch[i] = s.pitch; I.hRoll[i] = s.roll; I.hVz[i] = s.vz; I.hWth[i] = s.wth; I.hWph[i] = s.wph;
  for (Index n = 0; n < I.NN; ++n) {
    I.hVol[off + n] = s.vol[n]; I.hLevel[off + n] = s.level[n]; I.hCx[off + n] = s.cx[n]; I.hCy[off + n] = s.cy[n]; I.hCz[off + n] = s.cz[n];
  }
}

void Batch::set_connection_enabled(Index instance, Index c, bool en) {
  Impl& I = *impl_;
  if (I.ran) throw std::logic_error("cuda::Batch: set after run");
  I.hEn[static_cast<std::size_t>(instance) * I.NC + static_cast<std::size_t>(c)] = en ? 1 : 0;
}
void Batch::set_connection_area(Index instance, Index c, Real area) {
  Impl& I = *impl_;
  if (I.ran) throw std::logic_error("cuda::Batch: set after run");
  I.hArea[static_cast<std::size_t>(instance) * I.NC + static_cast<std::size_t>(c)] = area;
}
void Batch::set_connection_activation(Index instance, Index c, Real tOn) {
  Impl& I = *impl_;
  if (I.ran) throw std::logic_error("cuda::Batch: set after run");
  I.hTOn[static_cast<std::size_t>(instance) * I.NC + static_cast<std::size_t>(c)] = tOn;
}

double Batch::run() {
  Impl& I = *impl_;
  if (I.ran) throw std::logic_error("cuda::Batch: run twice");
  I.ran = true;
  const auto count = static_cast<std::size_t>(I.opt.count);
  // upload the per-instance state
  I.en.upload(I.hEn); I.area.upload(I.hArea); I.tOn.upload(I.hTOn);
  I.t.upload(I.hT); I.zO.upload(I.hZO); I.pitch.upload(I.hPitch); I.roll.upload(I.hRoll); I.vz.upload(I.hVz); I.wth.upload(I.hWth); I.wph.upload(I.hWph);
  I.vol.upload(I.hVol); I.level.upload(I.hLevel); I.ccx.upload(I.hCx); I.ccy.upload(I.hCy); I.ccz.upload(I.hCz);
  const std::vector<Real> zeros(count, 0);
  I.inflow.upload(zeros); I.Vb.upload(zeros); I.hullTop.upload(zeros); I.mw.upload(zeros); I.bx.upload(zeros); I.by.upload(zeros); I.bz.upload(zeros);
  I.evItems.upload(std::vector<Event>(count * static_cast<std::size_t>(I.evCap), Event{}));
  I.evCount.upload(std::vector<Index>(count, 0));
  I.overT.upload(std::vector<Real>(count * static_cast<std::size_t>(I.nMon), -1));
  I.markT.upload(std::vector<Real>(count * static_cast<std::size_t>(I.nMarks), -1));
  I.foundered.upload(std::vector<Byte>(count, 0));
  I.founderT.upload(std::vector<Real>(count, -1));
  I.steps.upload(std::vector<long long>(count, 0));
  const auto traced = static_cast<std::size_t>(I.opt.tracedInstances);
  const auto rec = static_cast<std::size_t>(I.recSize);
  I.denseRec.allocate(traced * static_cast<std::size_t>(I.opt.denseSteps + 1) * rec);
  I.sampleRec.allocate(traced * static_cast<std::size_t>(I.maxSamples) * rec);
  I.denseCount.upload(std::vector<int>(traced, 0));
  I.sampleCount.upload(std::vector<int>(traced, 0));
  I.nextSample.upload(std::vector<Real>(traced, I.opt.sampleEvery));
  if (I.opt.curveEvery > 0) {
    I.curveMax = static_cast<int>(std::floor(I.opt.tMax / I.opt.curveEvery)) + 2;
    I.curveRec.allocate(count * static_cast<std::size_t>(I.curveMax) * kCurveValues);
    I.curveCount.upload(std::vector<int>(count, 0));
    I.nextCurve.upload(std::vector<Real>(count, 0));   // the first record is the initial state, as in the oracle's run
  }

  const ShipView ship = I.shipView();
  const DevSim sim = I.devSim();
  const DevBatch batch = I.devBatch();
  if (I.layout.total > 48 * 1024) {
    check(cudaFuncSetAttribute(run_chunk<true>, cudaFuncAttributeMaxDynamicSharedMemorySize, static_cast<int>(I.layout.total)), "cudaFuncSetAttribute");
    check(cudaFuncSetAttribute(run_chunk<false>, cudaFuncAttributeMaxDynamicSharedMemorySize, static_cast<int>(I.layout.total)), "cudaFuncSetAttribute");
  }

  const auto t0 = std::chrono::steady_clock::now();
  int firstLaunch = 1;
  for (;;) {
    if (I.opt.mixedPrecision)
      run_chunk<true><<<I.opt.count, kBlockThreads, I.layout.total>>>(ship, sim, batch, I.layout, I.opt.stepsPerLaunch, I.opt.tMax, firstLaunch);
    else
      run_chunk<false><<<I.opt.count, kBlockThreads, I.layout.total>>>(ship, sim, batch, I.layout, I.opt.stepsPerLaunch, I.opt.tMax, firstLaunch);
    check(cudaGetLastError(), "kernel launch");
    check(cudaDeviceSynchronize(), "kernel execution");
    firstLaunch = 0;
    // any instance still running?
    const std::vector<Byte> f = I.foundered.download();
    const std::vector<Real> tt = I.t.download();
    bool active = false;
    for (std::size_t i = 0; i < count; ++i)
      if (!f[i] && tt[i] < I.opt.tMax) { active = true; break; }
    if (!active) break;
  }
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const std::vector<long long> steps = I.steps.download();
  I.totalSteps = 0;
  for (const long long s : steps) I.totalSteps += s;
  return seconds;
}

InstanceResult Batch::result(Index instance) const {
  const Impl& I = *impl_;
  if (!I.ran) throw std::logic_error("cuda::Batch: result before run");
  if (instance < 0 || instance >= I.opt.count) throw std::out_of_range("cuda::Batch: instance");
  const auto i = static_cast<std::size_t>(instance);
  InstanceResult r;
  r.foundered = I.foundered.download()[i] != 0;
  r.founderT = I.founderT.download()[i];
  r.t = I.t.download()[i]; r.tEnd = r.t;
  r.steps = I.steps.download()[i];
  r.zO = I.zO.download()[i]; r.pitch = I.pitch.download()[i]; r.roll = I.roll.download()[i];
  r.vz = I.vz.download()[i]; r.wth = I.wth.download()[i]; r.wph = I.wph.download()[i];
  const auto nn = static_cast<std::size_t>(I.NN);
  auto slice = [&](const DeviceArray<Real>& a) { const std::vector<Real> all = a.download(); return std::vector<Real>(all.begin() + static_cast<std::ptrdiff_t>(i * nn), all.begin() + static_cast<std::ptrdiff_t>((i + 1) * nn)); };
  r.vol = slice(I.vol); r.level = slice(I.level); r.cx = slice(I.ccx); r.cy = slice(I.ccy); r.cz = slice(I.ccz);
  r.outputs.inflow = I.inflow.download()[i]; r.outputs.Vb = I.Vb.download()[i]; r.outputs.hullTop = I.hullTop.download()[i]; r.outputs.mw = I.mw.download()[i];
  r.outputs.bx = I.bx.download()[i]; r.outputs.by = I.by.download()[i]; r.outputs.bz = I.bz.download()[i];
  const std::vector<Event> items = I.evItems.download();
  const Index n = I.evCount.download()[i];
  for (Index k = 0; k < n; ++k) r.events.push_back(items[i * static_cast<std::size_t>(I.evCap) + static_cast<std::size_t>(k)]);
  return r;
}

io::Trace Batch::trace(Index instance) const {
  const Impl& I = *impl_;
  if (!I.ran) throw std::logic_error("cuda::Batch: trace before run");
  if (instance < 0 || instance >= I.opt.tracedInstances) throw std::out_of_range("cuda::Batch: instance was not traced");
  const auto i = static_cast<std::size_t>(instance);
  const auto rec = static_cast<std::size_t>(I.recSize);
  io::Trace tr;
  tr.ship = I.ship->id; tr.sim = I.sim.id; tr.title = I.sim.title;
  tr.producer = I.opt.mixedPrecision ? "sinksim CUDA engine, numerics portable32" : "sinksim CUDA engine, numerics portable";
  const DeviceInfo info = device_info();
  tr.engine = "sinksim " + std::to_string(kEngineVersion.major) + "." + std::to_string(kEngineVersion.minor) + "." + std::to_string(kEngineVersion.patch) + " on " + info.name;
  tr.dt = I.sim.dt; tr.nodes = I.NN; tr.denseSteps = I.opt.denseSteps; tr.sampleEvery = I.opt.sampleEvery;
  const InstanceResult r = result(instance);
  tr.steps = r.steps; tr.tEnd = r.tEnd; tr.foundered = r.foundered; tr.founderT = r.founderT;
  for (const Event& e : r.events) tr.events.push_back(io::TraceEvent{e.t, event_id(e), event_label(e)});
  auto unpack = [&](const std::vector<Real>& buf, std::size_t base, int n, io::TraceColumns& c) {
    c.n = n;
    for (int k = 0; k < n; ++k) {
      const Real* p = buf.data() + base + static_cast<std::size_t>(k) * rec;
      c.t.push_back(p[0]); c.zO.push_back(p[1]); c.pitch.push_back(p[2]); c.roll.push_back(p[3]); c.vz.push_back(p[4]); c.wth.push_back(p[5]); c.wph.push_back(p[6]);
      c.inflow.push_back(p[7]); c.Vb.push_back(p[8]); c.hullTop.push_back(p[9]);
      const Real* q = p + 10;
      c.vol.insert(c.vol.end(), q, q + I.NN); q += I.NN;
      c.level.insert(c.level.end(), q, q + I.NN); q += I.NN;
      c.cx.insert(c.cx.end(), q, q + I.NN); q += I.NN;
      c.cy.insert(c.cy.end(), q, q + I.NN); q += I.NN;
      c.cz.insert(c.cz.end(), q, q + I.NN);
    }
  };
  const std::vector<Real> dense = I.denseRec.download();
  const std::vector<Real> samples = I.sampleRec.download();
  unpack(dense, i * static_cast<std::size_t>(I.opt.denseSteps + 1) * rec, I.denseCount.download()[i], tr.dense);
  unpack(samples, i * static_cast<std::size_t>(I.maxSamples) * rec, I.sampleCount.download()[i], tr.samples);
  return tr;
}

CurveSample Batch::final_readouts(Index instance) const {
  const Impl& I = *impl_;
  const InstanceResult r = result(instance);
  // the same function the device uses, on the host, with the portable math: the bits are identical
  const ShipView ship = I.ship->view();
  SimView M{};
  M.phys = I.sim.phys; M.dt = I.sim.dt; M.body = I.sim.body;
  State st{};
  st.t = r.t; st.zO = r.zO; st.pitch = r.pitch; st.roll = r.roll; st.vz = r.vz; st.wth = r.wth; st.wph = r.wph;
  std::vector<Real> vol = r.vol, level = r.level;
  st.vol = vol.data(); st.level = level.data();
  const kernel::Readouts ro = kernel::readouts<PortableMath>(ship, M, I.sim.readout, st, r.outputs);
  return CurveSample{ro.t, ro.trimDeg, ro.listDeg, ro.waterT, ro.inflowTpm, ro.draftF, ro.draftA};
}

std::vector<CurveSample> Batch::curve(Index instance) const {
  const Impl& I = *impl_;
  if (!I.ran) throw std::logic_error("cuda::Batch: curve before run");
  if (I.opt.curveEvery <= 0) throw std::logic_error("cuda::Batch: curves were not recorded (curveEvery is 0)");
  if (instance < 0 || instance >= I.opt.count) throw std::out_of_range("cuda::Batch: instance");
  const auto i = static_cast<std::size_t>(instance);
  const std::vector<Real> all = I.curveRec.download();
  const int n = I.curveCount.download()[i];
  std::vector<CurveSample> out;
  out.reserve(static_cast<std::size_t>(n) + 1);
  for (int k = 0; k < n; ++k) {
    const Real* p = all.data() + (i * static_cast<std::size_t>(I.curveMax) + static_cast<std::size_t>(k)) * kCurveValues;
    out.push_back(CurveSample{p[0], p[1], p[2], p[3], p[4], p[5], p[6]});
  }
  out.push_back(final_readouts(instance));
  return out;
}

std::string Batch::event_id(const Event& e) const {
  switch (e.kind) {
    case EventKind::Overflow: return impl_->sim.monitorId[static_cast<std::size_t>(e.id)];
    case EventKind::Mark: return impl_->sim.markId[static_cast<std::size_t>(e.id)];
    case EventKind::Founder: return "founder";
  }
  return "?";
}

std::string Batch::event_label(const Event& e) const {
  switch (e.kind) {
    case EventKind::Overflow: return impl_->sim.monitorLabel[static_cast<std::size_t>(e.id)];
    case EventKind::Mark: return impl_->sim.markLabel[static_cast<std::size_t>(e.id)];
    case EventKind::Founder: return "Foundered";
  }
  return "?";
}

}  // namespace sinksim::cuda
