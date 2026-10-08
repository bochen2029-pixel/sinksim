// gen_ref.js: dump Node/V8 Math.* results for pseudo-random inputs as IEEE-754 bit patterns.
// Usage: node gen_ref.js [out.txt] [count]
// Each line: <fn> <hex x> <hex y> <hex result>; y is 0 for one-argument functions.
// The input domains are the ones the Titanic flooding core actually exercises.
'use strict';
const fs = require('fs');
const out = process.argv[2] || 'ref_node.txt';
const N = +(process.argv[3] || 20000);

let s = 0x9E3779B9 >>> 0;   // mulberry32: the same inputs on every run and every machine
function rnd() {
  s = (s + 0x6D2B79F5) >>> 0;
  let t = s;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}
const buf = new ArrayBuffer(8), f64 = new Float64Array(buf), u32 = new Uint32Array(buf);
function hex(x) { f64[0] = x; return u32[1].toString(16).padStart(8, '0') + u32[0].toString(16).padStart(8, '0'); }
const lines = [];
const emit = (fn, x, y, r) => lines.push(`${fn} ${hex(x)} ${hex(y)} ${hex(r)}`);

for (let i = 0; i < N; i++) {
  const u = rnd(), n = 0.9 + 2.2 * rnd();          // halfBreadth(): pow(u, n), u in (0,1), n in [0.9, 3.1]
  emit('pow', u, n, Math.pow(u, n));
  const r = rnd();                                 // Villemonte factor: pow(r, 1.5), then pow(1 - that, 0.385)
  const r15 = Math.pow(r, 1.5);
  emit('pow', r, 1.5, r15);
  const q = Math.max(0, 1 - r15);
  emit('pow', q, 0.385, Math.pow(q, 0.385));
  const b = 10 + 120 * rnd();                      // inertia terms: pow(x, 4) and x ** 2
  emit('pow', b, 4, Math.pow(b, 4));
  emit('pow', b, 2, b ** 2);
  const th = -1.4 + 2.8 * rnd();                   // pitch and roll in radians
  emit('sin', th, 0, Math.sin(th));
  emit('cos', th, 0, Math.cos(th));
  emit('tan', 0.5 * th, 0, Math.tan(0.5 * th));
  const a = -0.25 + 0.5 * rnd();                   // asin of a trim ratio
  emit('asin', a, 0, Math.asin(a));
  const e = -20 + 40 * rnd();
  emit('exp', e, 0, Math.exp(e));
  const l = 1e-6 + 1e4 * rnd();
  emit('log', l, 0, Math.log(l));
  const ay = -5 + 10 * rnd(), ax = -5 + 10 * rnd();
  emit('atan2', ay, ax, Math.atan2(ay, ax));
  const sq = 1e-3 + 1e3 * rnd();
  emit('sqrt', sq, 0, Math.sqrt(sq));
}
for (const [x, y] of [[0, 0.5], [1, 1e300], [-8, 1 / 3], [2, 0.5], [0.5, -1], [1e-300, 2], [10, -1], [-2, 3], [0.1, 0.2]]) {
  emit('pow', x, y, Math.pow(x, y));
}
fs.writeFileSync(out, lines.join('\n') + '\n');
console.log(`wrote ${out}: ${lines.length} lines  node ${process.version}  v8 ${process.versions.v8}`);
