'use strict';
// Single entry point to the frozen JS oracle. Every tool loads the core through here so that
// the oracle's location and its content hashes are defined in exactly one place.
const path = require('path');
const fs = require('fs');
const crypto = require('crypto');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const ORACLE = path.join(ROOT, 'oracle', 'js');

const C = require(path.join(ORACLE, 'core', 'core.js'));
const SC = require(path.join(ORACLE, 'core', 'scenarios.js'));

function sha256File(p) {
  return crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
}

const coreSha256 = sha256File(path.join(ORACLE, 'core', 'core.js'));
const scenariosSha256 = sha256File(path.join(ORACLE, 'core', 'scenarios.js'));

let shipCache = null;
function getShip() {
  if (!shipCache) shipCache = C.buildShip();
  return shipCache;
}

function engineInfo() {
  return { engine: `node ${process.version}`, v8: process.versions.v8, platform: `${process.platform}-${process.arch}` };
}

function writeJson(file, doc) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, stringifyCompactArrays(doc) + '\n');
}

// Pretty JSON for humans, with every array on a single line so large numeric arrays stay compact.
// Numbers are emitted by JSON.stringify, i.e. the shortest string that round-trips the double.
function stringifyCompactArrays(value, indent = '') {
  if (Array.isArray(value)) return JSON.stringify(value);
  if (value && typeof value === 'object') {
    const keys = Object.keys(value);
    if (keys.length === 0) return '{}';
    const inner = indent + '  ';
    const parts = keys.map(k => inner + JSON.stringify(k) + ': ' + stringifyCompactArrays(value[k], inner));
    return '{\n' + parts.join(',\n') + '\n' + indent + '}';
  }
  return JSON.stringify(value);
}

module.exports = { ROOT, ORACLE, C, SC, sha256File, coreSha256, scenariosSha256, getShip, engineInfo, writeJson, stringifyCompactArrays };
