'use strict';
// FNV-1a 64-bit over raw bytes, implemented on two 32-bit halves so it runs at native speed
// without BigInt. The C++ loader (engine/include/sinksim/hash.hpp) computes the same function
// over the little-endian bytes of the parsed arrays; equal hashes prove the JSON round trip is exact.

const OFFSET_HI = 0xcbf29ce4, OFFSET_LO = 0x84222325;   // 0xcbf29ce484222325
const PRIME_LO = 0x1b3;                                  // prime = 2^40 + 0x1b3

class Fnv1a64 {
  constructor() { this.hi = OFFSET_HI; this.lo = OFFSET_LO; }
  update(bytes) {
    let hi = this.hi, lo = this.lo;
    for (let i = 0; i < bytes.length; i++) {
      lo = (lo ^ bytes[i]) >>> 0;
      // (hi:lo) * (2^40 + 0x1b3) mod 2^64
      const pl = lo * PRIME_LO;                       // < 2^41, exact in a double
      const carry = Math.floor(pl / 4294967296);
      const newLo = pl % 4294967296;
      const newHi = (hi * PRIME_LO + carry + (lo & 0xffffff) * 256) % 4294967296;
      lo = newLo; hi = newHi;
    }
    this.hi = hi; this.lo = lo;
    return this;
  }
  hex() { return this.hi.toString(16).padStart(8, '0') + this.lo.toString(16).padStart(8, '0'); }
}

function bytesOf(typed) { return new Uint8Array(typed.buffer, typed.byteOffset, typed.byteLength); }

// kind: 'f64' | 'i32' | 'u8'. Values are plain JS numbers; they are packed little-endian exactly as the
// engine stores them after parsing.
function pack(kind, values) {
  // JSON cannot tell -0 from 0 (JSON.stringify(-0) is "0"), so negative zero is normalised before hashing;
  // the engine does the same on its side.
  if (kind === 'f64') return Float64Array.from(values, v => (v === 0 ? 0 : v));
  if (kind === 'i32') return Int32Array.from(values);
  if (kind === 'u8') return Uint8Array.from(values);
  throw new Error('unknown array kind ' + kind);
}

function hashArray(kind, values) { return new Fnv1a64().update(bytesOf(pack(kind, values))).hex(); }

// Hash several arrays in order with one running state (the "chained" hash of a whole document).
function hashChain(entries) {
  const h = new Fnv1a64();
  for (const [kind, values] of entries) h.update(bytesOf(pack(kind, values)));
  return h.hex();
}

module.exports = { Fnv1a64, hashArray, hashChain, bytesOf, pack };
