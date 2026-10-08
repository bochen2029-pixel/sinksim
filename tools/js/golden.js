'use strict';
// Golden trace of a case from the frozen oracle, in the sinksim.trace format the engine also writes.
//   node tools/js/golden.js [caseId=titanic] [denseSteps=400] [sampleEvery=60]
// Output: data/golden/titanic64/<caseId>.trace.json
// The trace carries the complete dynamic state at every sample (pose, rates, vol[], level[]), so a port
// can be restarted from any sample and checked window by window. See docs/DETERMINISM.md.
const path = require('path');
const { C, ROOT, coreSha256, engineInfo, writeJson } = require('./lib/oracle');
const { hashArray, hashChain } = require('./lib/fnv');
const { createCase } = require('./scenario_catalog');

const SHIP_ID = 'titanic64';

// The record is the complete dynamic state. The centroids belong to it: the oracle keeps a node's last computed
// centroid while the node holds less water than the solver tolerance, and still uses it in the loads.
const FIELDS = ['t', 'zO', 'pitch', 'roll', 'vz', 'wth', 'wph', 'inflow', 'Vb', 'hullTop', 'vol', 'level', 'cx', 'cy', 'cz'];
function makeColumnar() {
  const col = { n: 0 };
  for (const f of FIELDS) col[f] = [];
  return col;
}
function record(col, sim, t0extras) {
  col.n++;
  col.t.push(sim.t); col.zO.push(sim.zO); col.pitch.push(sim.pitch); col.roll.push(sim.roll);
  col.vz.push(sim.vz); col.wth.push(sim.wth); col.wph.push(sim.wph);
  col.inflow.push(t0extras ? 0 : sim.inflow); col.Vb.push(t0extras ? t0extras.Vb : sim.Vb); col.hullTop.push(t0extras ? t0extras.top : sim.hullTop);
  for (let n = 0; n < sim.vol.length; n++) col.vol.push(sim.vol[n]);
  for (let n = 0; n < sim.level.length; n++) col.level.push(sim.level[n]);
  for (let n = 0; n < sim.cx.length; n++) col.cx.push(sim.cx[n]);
  for (let n = 0; n < sim.cy.length; n++) col.cy.push(sim.cy[n]);
  for (let n = 0; n < sim.cz.length; n++) col.cz.push(sim.cz[n]);
}

function goldenTrace(caseId, denseSteps, sampleEvery) {
  const { case: cs, sim } = createCase(caseId);
  const dt = sim.params.dt;
  const dense = makeColumnar(), samples = makeColumnar();
  // Before the first step the oracle has not evaluated buoyancy; evaluate it at the initial pose.
  const hb0 = C.hydroPass(sim, C.frameOf(sim), {});
  const t0 = { Vb: hb0.V, top: hb0.top };
  record(dense, sim, t0); record(samples, sim, t0);
  let next = sampleEvery, steps = 0;
  while (!sim.foundered && sim.t < 5 * 3600) {
    C.step(sim); steps++;
    if (steps <= denseSteps) record(dense, sim);
    if (sim.t >= next - 1e-9) { record(samples, sim); next += sampleEvery; }
  }
  const hashed = [];
  for (const [name, col] of [['dense', dense], ['samples', samples]]) {
    for (const f of FIELDS) hashed.push([`${name}.${f}`, 'f64', col[f]]);
  }
  const hash = {};
  for (const [name, kd, values] of hashed) hash[name] = hashArray(kd, values);
  hash.all = hashChain(hashed.map(([, kd, values]) => [kd, values]));
  const doc = {
    format: 'sinksim.trace', formatVersion: 1, role: 'golden', ship: SHIP_ID, sim: caseId,
    meta: { title: cs.label, producer: 'oracle/js via tools/js/golden.js', ...engineInfo(), oracle: { core_sha256: coreSha256 }, created: new Date().toISOString() },
    dt, nodes: sim.vol.length, denseSteps, sampleEvery, steps, tEnd: sim.t,
    foundered: sim.foundered, founderT: sim.founderT,
    events: sim.events.map(e => ({ t: e.t, id: e.id, label: e.label })),
    dense, samples, hash,
  };
  const file = path.join(ROOT, 'data', 'golden', SHIP_ID, `${caseId}.trace.json`);
  writeJson(file, doc);
  console.log('wrote', path.relative(ROOT, file), 'steps', steps, 'dense', dense.n, 'samples', samples.n, 'founder', sim.foundered ? (sim.founderT / 60).toFixed(2) + ' min' : 'afloat', 'hash', hash.all);
  return doc;
}

if (require.main === module) {
  goldenTrace(process.argv[2] || 'titanic', +(process.argv[3] || 400), +(process.argv[4] || 60));
}

module.exports = { goldenTrace };
