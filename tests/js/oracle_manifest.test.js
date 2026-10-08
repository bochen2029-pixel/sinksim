'use strict';
// The oracle is frozen: every file under oracle/js must match oracle/MANIFEST.sha256, and every file must be listed.
const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const ROOT = path.resolve(__dirname, '..', '..');
const ORACLE = path.join(ROOT, 'oracle', 'js');

function walk(dir) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(e => e.isDirectory() ? walk(path.join(dir, e.name)) : [path.join(dir, e.name)]);
}

test('the frozen oracle matches oracle/MANIFEST.sha256', () => {
  const lines = fs.readFileSync(path.join(ROOT, 'oracle', 'MANIFEST.sha256'), 'utf8').split('\n').filter(Boolean);
  assert.ok(lines.length >= 20, 'manifest looks too short');
  const listed = new Set();
  for (const line of lines) {
    const m = line.match(/^([0-9a-f]{64}) [ *](.+)$/);
    assert.ok(m, 'unparseable manifest line: ' + line);
    const rel = m[2].replace(/^\.\//, '');
    listed.add(rel);
    const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(ORACLE, rel))).digest('hex');
    assert.strictEqual(actual, m[1], rel + ' differs from the manifest; the oracle must not change');
  }
  assert.ok(listed.has('core/core.js'));
  assert.ok(listed.has('core/scenarios.js'));
  for (const f of walk(ORACLE)) {
    const rel = path.relative(ORACLE, f).split(path.sep).join('/');
    assert.ok(listed.has(rel), rel + ' is not in the manifest');
  }
});
