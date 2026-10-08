'use strict';
// Self-consistency check of the oracle against its own golden trace: restart from each recorded state
// (pose, rates, vol[], level[]) and step to the next record. Because the trace carries the complete
// dynamic state, the oracle must reproduce itself bit for bit; any nonzero difference means the trace
// misses a state variable. The C++ engine is held to the same window-by-window test (apps/sinksim_check).
//   node tools/js/perstep.js [caseId=titanic]
const fs = require('fs');
const path = require('path');
const { C, ROOT } = require('./lib/oracle');
const { createCase } = require('./scenario_catalog');

function setState(sim, col, i) {
  const N = sim.vol.length;
  sim.t = col.t[i]; sim.zO = col.zO[i]; sim.pitch = col.pitch[i]; sim.roll = col.roll[i];
  sim.vz = col.vz[i]; sim.wth = col.wth[i]; sim.wph = col.wph[i];
  for (let n = 0; n < N; n++) {
    sim.vol[n] = col.vol[i * N + n]; sim.level[n] = col.level[i * N + n];
    sim.cx[n] = col.cx[i * N + n]; sim.cy[n] = col.cy[i * N + n]; sim.cz[n] = col.cz[i * N + n];
  }
  // Event bookkeeping does not feed back into the physics; reset it so the founder flag cannot stop a window early.
  sim.foundered = false; sim.founderT = -1; sim.events = []; sim.overT.fill(-1);
  for (const k of Object.keys(sim.markT)) sim.markT[k] = -1;
}

function compare(sim, col, i) {
  const N = sim.vol.length;
  let worst = 0, where = '';
  const chk = (name, a, b) => { const d = Math.abs(a - b); if (d > worst) { worst = d; where = name; } };
  chk('t', sim.t, col.t[i]); chk('zO', sim.zO, col.zO[i]); chk('pitch', sim.pitch, col.pitch[i]); chk('roll', sim.roll, col.roll[i]);
  chk('vz', sim.vz, col.vz[i]); chk('wth', sim.wth, col.wth[i]); chk('wph', sim.wph, col.wph[i]);
  chk('inflow', sim.inflow, col.inflow[i]); chk('Vb', sim.Vb, col.Vb[i]); chk('hullTop', sim.hullTop, col.hullTop[i]);
  for (let n = 0; n < N; n++) {
    chk('vol[' + n + ']', sim.vol[n], col.vol[i * N + n]); chk('level[' + n + ']', sim.level[n], col.level[i * N + n]);
    chk('cx[' + n + ']', sim.cx[n], col.cx[i * N + n]); chk('cy[' + n + ']', sim.cy[n], col.cy[i * N + n]); chk('cz[' + n + ']', sim.cz[n], col.cz[i * N + n]);
  }
  return { worst, where };
}

function run(caseId) {
  const file = path.join(ROOT, 'data', 'golden', 'titanic64', `${caseId}.trace.json`);
  const G = JSON.parse(fs.readFileSync(file, 'utf8'));
  const { sim } = createCase(caseId);
  const dt = G.dt;
  let worst = 0, worstAt = '';
  const t0 = Date.now();
  // dense head: single steps
  for (let i = 0; i + 1 < G.dense.n; i++) {
    setState(sim, G.dense, i); C.step(sim);
    const r = compare(sim, G.dense, i + 1);
    if (r.worst > worst) { worst = r.worst; worstAt = `dense step ${i + 1} (${r.where})`; }
  }
  // sampled windows
  let windows = 0;
  for (let i = 0; i + 1 < G.samples.n; i++) {
    setState(sim, G.samples, i);
    const steps = Math.round((G.samples.t[i + 1] - G.samples.t[i]) / dt);
    for (let s = 0; s < steps; s++) C.step(sim);
    const r = compare(sim, G.samples, i + 1);
    if (r.worst > worst) { worst = r.worst; worstAt = `window ${i} -> ${i + 1} (${r.where})`; }
    windows++;
  }
  console.log(`perstep ${caseId}: ${G.dense.n - 1} single steps and ${windows} windows in ${Date.now() - t0} ms; worst abs diff ${worst}${worst > 0 ? ' at ' + worstAt : ''}`);
  return worst;
}

if (require.main === module) {
  const worst = run(process.argv[2] || 'titanic');
  if (worst !== 0) { console.error('FAIL: the golden trace does not carry the complete state'); process.exit(1); }
  console.log('OK: the oracle reproduces its golden trace exactly from every recorded state');
}

module.exports = { run, setState, compare };
