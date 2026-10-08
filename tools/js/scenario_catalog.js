'use strict';
// The validation and golden scenarios, defined once. Case ids are the keys the oracle's
// validate.js used in out/validation.json, so tables stay comparable across the port.
const { C, SC, getShip } = require('./lib/oracle');

const P = SC.PRESETS;
const allBulkheads = () => C.BULKHEADS.map(b => b.id);

const CASES = [
  { id: 'titanic', label: 'Titanic 1912 (calibrated)', tMaxH: 8, keepHist: true,
    build: () => ({ scen: P.titanic.build(), params: {} }) },
  { id: 'four', label: 'Four forward compartments (Wilding: floats)', tMaxH: 8,
    build: () => ({ scen: P.four.build(), params: {} }) },
  { id: 'five', label: 'Five forward compartments (Wilding: sinks)', tMaxH: 8,
    build: () => ({ scen: P.five.build(), params: {} }) },
  { id: 'hawke', label: 'Olympic-Hawke 1911 (returned to port)', tMaxH: 8,
    build: () => ({ scen: P.hawke.build(), params: {} }) },
  { id: 'britannic', label: 'Britannic 1916, approx. (sank in ~55 min)', tMaxH: 8, keepHist: true,
    build: () => ({ scen: P.britannic.build(), params: {} }) },
  { id: 'britannicClosed', label: 'Britannic, same damage, portholes shut', tMaxH: 8,
    build: () => { const scen = P.britannic.build(); scen.openings = scen.openings.filter(o => o.kind !== 'porthole'); return { scen, params: {} }; } },
  { id: 'titanicNoBR5', label: 'Titanic damage without the BR5 seam', tMaxH: 8,
    build: () => { const scen = P.titanic.build(); scen.openings = scen.openings.filter(o => C.zoneAt(o.x) !== 5); return { scen, params: {} }; } },
  { id: 'titanicFour', label: 'Titanic damage stopping at hold 3', tMaxH: 8,
    build: () => { const scen = P.titanic.build(); scen.openings = scen.openings.filter(o => C.zoneAt(o.x) <= 3); return { scen, params: {} }; } },
  { id: 'titanicD', label: 'Titanic damage, all bulkheads to D deck', tMaxH: 8,
    build: () => { const scen = P.titanic.build(); scen.bulkTop = { C: 'D', D: 'D', E: 'D', F: 'D', G: 'D', H: 'D', J: 'D' }; return { scen, params: {} }; } },
  { id: 'titanicB', label: 'Titanic damage, all bulkheads to B deck', tMaxH: 8,
    build: () => { const scen = P.titanic.build(); scen.bulkTop = Object.fromEntries(allBulkheads().map(id => [id, 'B'])); return { scen, params: {} }; } },
  { id: 'titanicDoors', label: 'Titanic damage, watertight doors left open', tMaxH: 8,
    build: () => { const scen = P.titanic.build(); scen.doorsOpen = allBulkheads(); return { scen, params: {} }; } },
];

function byId(id) {
  const c = CASES.find(x => x.id === id);
  if (!c) throw new Error('unknown case ' + id + '; known: ' + CASES.map(x => x.id).join(', '));
  return c;
}

// Builds a fresh oracle simulation for a case, exactly as the oracle's validate.js did.
function createCase(id) {
  const c = byId(id);
  const { scen, params } = c.build();
  const ship = getShip();
  const sim = C.createSim(ship, params, scen);
  return { case: c, scen, params, ship, sim };
}

module.exports = { CASES, byId, createCase };
