'use strict';
// Export the oracle's built ship and each scenario's simulation as compiled files for the engine.
//   node tools/js/export_compiled.js            all cases
//   node tools/js/export_compiled.js titanic    one case
// Output: ships/titanic/titanic64.ship.json and ships/titanic/sims/<id>.sim.json.
// See docs/FORMATS.md for the schema and the hashing order; the engine verifies the hashes on load.
const path = require('path');
const { C, ROOT, coreSha256, scenariosSha256, getShip, engineInfo, writeJson } = require('./lib/oracle');
const { hashArray, hashChain } = require('./lib/fnv');
const { CASES, createCase } = require('./scenario_catalog');

const SHIP_ID = 'titanic64';
const SHIP_DIR = path.join(ROOT, 'ships', 'titanic');

const K = C.K;
const KIND_NAMES = ['door', 'over', 'down', 'xlow', 'xmerge', 'xup', 'top', 'breach', 'open'];
const GROUP_MODE = { single: 0, always: 1, threshold: 2, never: 3 };

const arr = ta => Array.from(ta);

function exportShip() {
  const ship = getShip();
  const cols = ship.cols;
  const nodeLabel = [], nodeZone = [], nodeSide = [], nodeLayer = [];
  for (let n = 0; n < C.NN; n++) {
    const layer = n & 1, side = (n >> 1) & 1, k = n >> 2;
    nodeZone.push(k); nodeSide.push(side); nodeLayer.push(layer);
    nodeLabel.push(`${C.ZONES[k].short}/${side ? 'S' : 'P'}/${layer ? 'upper' : 'lower'}`);
  }
  const columns = { n: cols.N, dy: cols.dy, x: arr(cols.x), y: arr(cols.y), dxdy: arr(cols.dxdy), dx: arr(cols.dx), zlo: arr(cols.zlo), zhi: arr(cols.zhi), zone: arr(cols.zone), side: arr(cols.side) };
  const nodes = { n: C.NN, label: nodeLabel, zone: nodeZone, side: nodeSide, layer: nodeLayer, mu: arr(ship.nodeMu), vmax: arr(ship.nodeVmax), amin: arr(ship.nodeAmin), aov: arr(ship.nodeAov), segStart: arr(ship.nodeStart), segCount: arr(ship.nodeCount) };
  const segments = { n: ship.entCol.length, col: arr(ship.entCol), a: arr(ship.entA), e: arr(ship.entE) };
  const hashed = [
    ['columns.x', 'f64', columns.x], ['columns.y', 'f64', columns.y], ['columns.dxdy', 'f64', columns.dxdy],
    ['columns.zlo', 'f64', columns.zlo], ['columns.zhi', 'f64', columns.zhi], ['columns.zone', 'i32', columns.zone], ['columns.side', 'i32', columns.side],
    ['nodes.mu', 'f64', nodes.mu], ['nodes.vmax', 'f64', nodes.vmax], ['nodes.amin', 'f64', nodes.amin], ['nodes.aov', 'f64', nodes.aov],
    ['nodes.segStart', 'i32', nodes.segStart], ['nodes.segCount', 'i32', nodes.segCount],
    ['segments.col', 'i32', segments.col], ['segments.a', 'f64', segments.a], ['segments.e', 'f64', segments.e],
  ];
  const hash = {};
  for (const [name, kind, values] of hashed) hash[name] = hashArray(kind, values);
  hash.all = hashChain(hashed.map(([, kind, values]) => [kind, values]));
  const doc = {
    format: 'sinksim.compiled-ship', formatVersion: 1, id: SHIP_ID,
    meta: {
      title: 'RMS Titanic, parametric hull, 16 compartments x {port, starboard} x {below, above E deck}',
      generator: 'tools/js/export_compiled.js', ...engineInfo(),
      oracle: { core_sha256: coreSha256, scenarios_sha256: scenariosSha256 }, created: new Date().toISOString(),
    },
    units: { length: 'm', area: 'm2', volume: 'm3', mass: 'kg', time: 's', angle: 'rad' },
    frame: {
      x: 'forward from amidships', y: 'to port', z: 'up from the keel baseline',
      world: 'sea surface at Z = 0', pitch: 'bow down positive', roll: 'starboard down positive', rotation: 'R = Ry(pitch) * Rx(roll)',
      nodeIndex: 'n = (zone * 2 + side) * 2 + layer; side 0 = port; layer 0 = below E deck',
    },
    columns, nodes, segments,
    zones: ship.zones.map(z => ({ k: z.k, name: z.name, short: z.short, kind: z.kind, mu: z.mu, xf: z.xf, xa: z.xa, xm: z.xm, len: z.len, floor: z.floor })),
    hydrostatics: ship.hyd,
    hash,
  };
  const file = path.join(SHIP_DIR, `${SHIP_ID}.ship.json`);
  writeJson(file, doc);
  console.log('wrote', path.relative(ROOT, file), 'columns', cols.N, 'segments', segments.n, 'hash', hash.all);
  return hash.all;
}

function exportSim(id, shipHash) {
  const { case: cs, sim } = createCase(id);
  const S = sim.conn, n = S.n;
  const sub = ta => Array.from(ta.subarray(0, n));
  // Free-surface groups: one per (zone, layer), two nodes each, in the oracle's solve order.
  const groups = { n: 32, nodeStart: [], nodeCount: [], nodes: [], mode: [], px: [], py: [], pz: [], margin: 0.05 };
  for (let k = 0; k < 16; k++) {
    for (let L = 0; L < 2; L++) {
      const g = k * 2 + L;
      groups.nodeStart.push(groups.nodes.length); groups.nodeCount.push(2);
      groups.nodes.push(C.nodeIndex(k, 0, L), C.nodeIndex(k, 1, L));
      const always = sim.mergeAlways[g] === 1;
      groups.mode.push(always ? GROUP_MODE.always : (L === 0 ? GROUP_MODE.threshold : GROUP_MODE.never));
      groups.px.push(sim.ship.zones[k].xm); groups.py.push(0); groups.pz.push(L === 0 ? sim.zMerge[k] : 0);
    }
  }
  const kind = sub(S.kind);
  const idx = sub(S.idx);
  const connections = {
    n, a: sub(S.a), b: sub(S.b), type: sub(S.type), x: sub(S.x), y: sub(S.y), z: sub(S.z), area: sub(S.area), coef: sub(S.coef),
    kind, idx, en: sub(S.en), tOn: sub(S.tOn),
    skipGroup: kind.map((kd, c) => (kd === K.XLOW || kd === K.XMERGE) ? idx[c] * 2 : -1),
    monitor: kind.map((kd, c) => kd === K.OVER ? idx[c] : -1),
    kindNames: KIND_NAMES, typeNames: ['orifice', 'weir'],
  };
  const monitors = {
    n: C.BULKHEADS.length, threshold: 0.05,
    id: C.BULKHEADS.map(b => 'over' + b.id),
    label: C.BULKHEADS.map((b, i) => `Water flows over bulkhead ${b.id} at ${sim.bulkTop[i]} deck`),
  };
  const marks = { n: C.MARKS.length, id: [], label: [], x: [], y: [], z: [], when: [] };
  for (const m of C.MARKS) {
    const p = m.p();
    marks.id.push(m.id); marks.label.push(m.label); marks.x.push(p[0]); marks.y.push(p[1]); marks.z.push(p[2]); marks.when.push(m.when === 'under' ? 0 : 1);
  }
  // The complete dynamic state (centroids included; see tools/js/golden.js).
  const state0 = { t: sim.t, zO: sim.zO, pitch: sim.pitch, roll: sim.roll, vz: sim.vz, wth: sim.wth, wph: sim.wph, vol: arr(sim.vol), level: arr(sim.level), cx: arr(sim.cx), cy: arr(sim.cy), cz: arr(sim.cz) };
  const hashed = [
    ['groups.nodeStart', 'i32', groups.nodeStart], ['groups.nodeCount', 'i32', groups.nodeCount], ['groups.nodes', 'i32', groups.nodes],
    ['groups.mode', 'u8', groups.mode], ['groups.px', 'f64', groups.px], ['groups.py', 'f64', groups.py], ['groups.pz', 'f64', groups.pz],
    ['connections.a', 'i32', connections.a], ['connections.b', 'i32', connections.b], ['connections.type', 'u8', connections.type],
    ['connections.x', 'f64', connections.x], ['connections.y', 'f64', connections.y], ['connections.z', 'f64', connections.z],
    ['connections.area', 'f64', connections.area], ['connections.coef', 'f64', connections.coef], ['connections.kind', 'u8', connections.kind],
    ['connections.idx', 'i32', connections.idx], ['connections.en', 'u8', connections.en], ['connections.tOn', 'f64', connections.tOn],
    ['connections.skipGroup', 'i32', connections.skipGroup], ['connections.monitor', 'i32', connections.monitor],
    ['marks.x', 'f64', marks.x], ['marks.y', 'f64', marks.y], ['marks.z', 'f64', marks.z], ['marks.when', 'u8', marks.when],
    ['state0.vol', 'f64', state0.vol], ['state0.level', 'f64', state0.level],
    ['state0.cx', 'f64', state0.cx], ['state0.cy', 'f64', state0.cy], ['state0.cz', 'f64', state0.cz],
  ];
  const hash = {};
  for (const [name, kd, values] of hashed) hash[name] = hashArray(kd, values);
  hash.all = hashChain(hashed.map(([, kd, values]) => [kd, values]));
  const doc = {
    format: 'sinksim.compiled-sim', formatVersion: 1, id, ship: SHIP_ID, shipHash,
    meta: {
      title: cs.label, generator: 'tools/js/export_compiled.js', ...engineInfo(),
      oracle: { core_sha256: coreSha256, scenarios_sha256: scenariosSha256 }, created: new Date().toISOString(),
      oracleRun: { tMaxH: cs.tMaxH, every: 30, stopWhenStable: true },
    },
    constants: { rho: C.RHO, g: C.GRAV },
    scheme: { dt: sim.params.dt },
    params: Object.assign({}, sim.params),
    groups, connections, monitors, marks,
    founder: { hullTopBelow: -0.5, pitchAbsAbove: 1.2, buoyancyBelow: 1, minT: 10 },
    body: { ms: sim.ms, KG: sim.KG, g0: sim.g0.slice(), XO: sim.XO, YO: sim.YO, mh0: sim.mh0, Ith0: sim.Ith0, Iph0: sim.Iph0, cDz: sim.cDz, cDth: sim.cDth, cDph: sim.cDph, qz: sim.qz, qth: sim.qth, qph: sim.qph },
    readout: { xFP: C.G.xFP, xAP: C.G.xAP },
    state0,
    openings: sim.openings.map(o => ({ id: o.id, kind: o.kind, x: o.x, y: o.y, z: o.z, area: o.area, tOpen: o.tOpen, zone: o.zone, side: o.side, layer: o.layer, node: o.node, conn: o.conn, label: o.label })),
    doors: C.BULKHEADS.map((b, i) => ({ id: b.id, x: b.x, open: sim.doorOpen[i] === 1, top: sim.bulkTop[i] })),
    hash,
  };
  const file = path.join(SHIP_DIR, 'sims', `${id}.sim.json`);
  writeJson(file, doc);
  console.log('wrote', path.relative(ROOT, file), 'connections', n, 'openings', sim.openings.length, 'hash', hash.all);
}

function exportCatalog() {
  const doc = {
    format: 'sinksim.catalog', formatVersion: 1, ship: SHIP_ID, shipFile: `${SHIP_ID}.ship.json`,
    cases: CASES.map(cs => ({ id: cs.id, label: cs.label, file: `sims/${cs.id}.sim.json`, tMaxH: cs.tMaxH, keepHist: !!cs.keepHist })),
  };
  const file = path.join(SHIP_DIR, 'catalog.json');
  writeJson(file, doc);
  console.log('wrote', path.relative(ROOT, file), 'cases', CASES.length);
}

if (require.main === module) {
  const only = process.argv[2];
  const shipHash = exportShip();
  for (const cs of CASES) if (!only || cs.id === only) exportSim(cs.id, shipHash);
  if (!only) exportCatalog();
}

module.exports = { exportShip, exportSim, SHIP_ID };
