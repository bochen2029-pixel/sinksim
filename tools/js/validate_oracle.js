'use strict';
// Run the oracle's validation cases on this machine's Node and write the table the engine must reproduce.
//   node tools/js/validate_oracle.js
// Output: data/validation/titanic64/validation.oracle.json (same record layout as the oracle's out/validation.json)
//         data/validation/titanic64/validation.oracle.txt  (the human table)
const fs = require('fs');
const path = require('path');
const { C, SC, ROOT, coreSha256, engineInfo, writeJson } = require('./lib/oracle');
const { CASES, createCase } = require('./scenario_catalog');

const fmtT = t => `${Math.floor(t / 3600)}h${String(Math.floor(t / 60) % 60).padStart(2, '0')}m`;

function runCase(cs) {
  const { sim } = createCase(cs.id);
  const res = C.run(sim, { tMax: cs.tMaxH * 3600, every: 30, stopWhenStable: true });
  const r = res.final;
  const rec = {
    label: cs.label, foundered: res.foundered, founderMin: res.foundered ? +(res.founderT / 60).toFixed(1) : null,
    founderT: res.foundered ? res.founderT : null, endT: sim.t,
    endMin: +(sim.t / 60).toFixed(1), trim: +r.trimDeg.toFixed(2), list: +r.listDeg.toFixed(2), water: Math.round(r.waterT),
    trimDeg: r.trimDeg, listDeg: r.listDeg, waterT: r.waterT,
    events: res.events.map(e => ({ t: +(e.t / 60).toFixed(1), tSec: e.t, id: e.id, label: e.label })),
  };
  if (cs.keepHist) rec.hist = res.hist.filter((h, i) => i % 2 === 0).map(h => [+(h.t / 60).toFixed(2), +h.trim.toFixed(3), +h.list.toFixed(3), Math.round(h.water)]);
  const line = `${cs.label.padEnd(44)} ${res.foundered ? `FOUNDERS at ${fmtT(res.founderT)}` : `afloat (checked to ${fmtT(sim.t)})`}  trim ${r.trimDeg.toFixed(2)}°  list ${r.listDeg.toFixed(2)}°  water ${Math.round(r.waterT)} t`;
  return { rec, line, sim, res };
}

function main() {
  const out = {}, lines = [];
  const t0 = Date.now();
  for (const cs of CASES) {
    const { rec, line, sim, res } = runCase(cs);
    out[cs.id] = rec; lines.push(line); console.log(line);
    if (cs.id === 'titanic') {
      const h16 = res.hist.find(h => h.water >= SC.OBS.wilding40);
      const l = `   floodwater reaches Wilding's 16,000 tons at ${(h16.t / 60).toFixed(1)} min (Wilding assumed 40)`;
      lines.push(l); console.log(l);
      rec.wildingMin = +(h16.t / 60).toFixed(1);
      rec.water40 = Math.round(res.hist.find(h => h.t >= 2400).water);
      for (const e of res.events) { const el = `    ${(e.t / 60).toFixed(1).padStart(6)} min ${e.label}`; lines.push(el); console.log(el); }
    }
  }
  const dir = path.join(ROOT, 'data', 'validation', 'titanic64');
  writeJson(path.join(dir, 'validation.oracle.json'), { format: 'sinksim.validation', formatVersion: 1, ship: 'titanic64', producer: 'oracle/js via tools/js/validate_oracle.js', ...engineInfo(), oracle: { core_sha256: coreSha256 }, created: new Date().toISOString(), cases: out });
  fs.writeFileSync(path.join(dir, 'validation.oracle.txt'), lines.join('\n') + '\n');
  console.log(`wrote ${path.relative(ROOT, dir)}/validation.oracle.{json,txt} in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
}

if (require.main === module) main();
module.exports = { runCase };
