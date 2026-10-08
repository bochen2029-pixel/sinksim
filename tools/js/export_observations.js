'use strict';
// Export the oracle's observation targets (Halpern 2023 trim and list against time, the foundering time, the
// timeline events) and the calibration objective's weights, so drivers outside JavaScript score runs the same way.
//   node tools/js/export_observations.js   -> data/observations/titanic-1912.json
const path = require('path');
const { SC, ROOT, scenariosSha256, writeJson, engineInfo } = require('./lib/oracle');

const OBS = SC.OBS;
const doc = {
  format: 'sinksim.observations', formatVersion: 1, ship: 'titanic64', sim: 'titanic',
  meta: { source: 'oracle/js/core/scenarios.js OBS (Halpern 2023, eyewitness-derived)', scenarios_sha256: scenariosSha256, ...engineInfo(), created: new Date().toISOString() },
  units: { time: 'min after 23:40', trim: 'deg bow down', list: 'deg starboard down' },
  trim: OBS.trim.map(([m, v]) => ({ min: m, deg: v })),
  list: OBS.list.map(([m, v]) => ({ min: m, deg: v })),
  founderMin: OBS.founder, breakupMin: OBS.breakup, wildingTonnesIn40Min: OBS.wilding40,
  events: OBS.events.map(e => ({ id: e.id, label: e.label, min: e.t })),
  // the objective of oracle/js/tools/calibrate.js, written down so it can be reproduced exactly
  objective: {
    description: 'J = sum((model - obs)/sigma)^2 over trim points (sigma by time band), founder time (sigma 4 min, afloat counts as 300 min), 0.25 * list points (sigma 3 deg, 0 after foundering), and three events (well 130 +- 8, bridge 155 +- 5, overF 75 +- 8); trim after foundering counts as 40 deg; curves are linearly interpolated from a 30 s history',
    trimSigmaByMinute: [{ upTo: 45, sigma: 0.3 }, { upTo: 100, sigma: 0.4 }, { upTo: 130, sigma: 0.7 }, { upTo: 1e9, sigma: 1.5 }],
    trimAfterFounder: 40, founderSigmaMin: 4, afloatFounderMin: 300,
    listWeight: 0.25, listSigmaDeg: 3, listAfterFounder: 0,
    events: [{ id: 'well', obsMin: 130, sigmaMin: 8 }, { id: 'bridge', obsMin: 155, sigmaMin: 5 }, { id: 'overF', obsMin: 75, sigmaMin: 8 }],
    historyEverySec: 30, tMaxMin: 260,
    bestKnown: { J: 10.726, area: 0.7506, wMul: 1.182, aDown: 0.2769, fOpen: 0.01008, note: 'oracle/js/out/calibration_log.txt; the compiled titanic sim already carries these values, so kind scales of 1 reproduce it' },
  },
};
const file = path.join(ROOT, 'data', 'observations', 'titanic-1912.json');
writeJson(file, doc);
console.log('wrote', path.relative(ROOT, file));
