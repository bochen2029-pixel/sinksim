#!/usr/bin/env python3
"""Compare two sinksim.trace files: where they first differ, by how much, and how the difference grows.

    python tools/py/trace_diff.py data/golden/titanic64/titanic.trace.json build/msvc-release/titanic.engine.trace.json

Exit code 0 when the sampled records are identical, 1 otherwise. Standard library only.
"""
import json
import math
import sys

SCALARS = ["t", "zO", "pitch", "roll", "vz", "wth", "wph", "inflow", "Vb", "hullTop"]
PER_NODE = ["vol", "level", "cx", "cy", "cz"]


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        doc = json.load(f)
    if doc.get("format") != "sinksim.trace":
        raise SystemExit(f"{path}: not a sinksim.trace file")
    return doc


def first_difference(a, b, nodes):
    n = min(a["n"], b["n"])
    for i in range(n):
        for f in SCALARS:
            if a[f][i] != b[f][i]:
                return i, f, a[f][i], b[f][i]
        for f in PER_NODE:
            for k in range(nodes):
                j = i * nodes + k
                if a[f][j] != b[f][j]:
                    return i, f"{f}[{k}]", a[f][j], b[f][j]
    return None


def maxima(a, b, nodes):
    n = min(a["n"], b["n"])
    out = {}
    for f in SCALARS:
        out[f] = max((abs(a[f][i] - b[f][i]) for i in range(n)), default=0.0)
    for f in PER_NODE:
        out[f] = max((abs(a[f][j] - b[f][j]) for j in range(n * nodes)), default=0.0)
    return out


def profile(a, b, nodes):
    n = min(a["n"], b["n"])
    rows = []
    for i in range(n):
        if not (i < 12 or i % 10 == 0 or i >= n - 3):
            continue
        dz = abs(a["zO"][i] - b["zO"][i])
        dp = math.degrees(abs(a["pitch"][i] - b["pitch"][i]))
        dr = math.degrees(abs(a["roll"][i] - b["roll"][i]))
        dv = [abs(a["vol"][i * nodes + k] - b["vol"][i * nodes + k]) for k in range(nodes)]
        mv = max(dv)
        rows.append((i, a["t"][i] / 60, dz, dp, dr, sum(dv), mv, dv.index(mv)))
    return rows


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    A, B = load(argv[1]), load(argv[2])
    nodes = A["nodes"]
    if B["nodes"] != nodes:
        raise SystemExit("the traces have different node counts")
    print(f"A: {argv[1]}  ({A.get('meta', {}).get('producer', '?')}, {A.get('meta', {}).get('engine', '?')})")
    print(f"B: {argv[2]}  ({B.get('meta', {}).get('producer', '?')}, {B.get('meta', {}).get('engine', '?')})")
    print(f"dt {A['dt']} vs {B['dt']}; steps {A['steps']} vs {B['steps']}; founder {A['founderT']} vs {B['founderT']} s")
    identical = True
    for name in ("dense", "samples"):
        a, b = A[name], B[name]
        fd = first_difference(a, b, nodes)
        print(f"\n== {name}: {a['n']} vs {b['n']} records ==")
        if fd is None:
            print("  identical over the common length")
        else:
            identical = False
            i, f, x, y = fd
            print(f"  first difference at record {i} (t = {a['t'][i]:.2f} s), field {f}: {x!r} vs {y!r}")
            m = maxima(a, b, nodes)
            print("  max |diff|: " + "  ".join(f"{k} {v:.3e}" for k, v in m.items() if v > 0))
    print("\n== events ==")
    ea, eb = A["events"], B["events"]
    for x, y in zip(ea, eb):
        flag = "" if x["t"] == y["t"] and x["id"] == y["id"] else f"   DIFF {y['t'] - x['t']:+.2f} s" + ("" if x["id"] == y["id"] else f" (id {x['id']} vs {y['id']})")
        print(f"  {x['id']:9s} {x['t']:9.2f} {y['t']:9.2f}{flag}")
    if len(ea) != len(eb):
        print(f"  event counts differ: {len(ea)} vs {len(eb)}")
        identical = False
    if not identical:
        print("\n== divergence profile (samples): record, t_min, |d zO| m, |d pitch| deg, |d roll| deg, sum|d vol| m3, max|d vol| m3 (node) ==")
        for r in profile(A["samples"], B["samples"], nodes):
            print(f"  {r[0]:5d} {r[1]:7.1f}  {r[2]:10.3e}  {r[3]:12.3e}  {r[4]:12.3e}  {r[5]:13.3e}  {r[6]:13.3e} ({r[7]})")
    print("\nRESULT:", "IDENTICAL" if identical else "DIFFERENT")
    return 0 if identical else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
