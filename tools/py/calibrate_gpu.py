"""Calibrate the four flow parameters of the Titanic model on the GPU, with the oracle's own objective.

The compiled scenario already carries the oracle's calibrated values, so the parameters here are scale factors on
four connection kinds: breach (the aggregate damage area), over (the E-deck overflow widths, the oracle's wMul),
down (the openings through E deck, aDown) and top (the open share of the top deck, fOpen). Scales of 1.0
reproduce the oracle's best fit, J = 10.73.

The search is batched: each round evaluates one batch of candidate points around the current best (log-uniform
in a shrinking box) in a single GPU run, keeps the best, and shrinks. It is a demonstration of the engine's batch
path, not a statistical calibration (roadmap phase 5 adds priors and bands).

    python tools/py/calibrate_gpu.py [--rounds 5] [--batch 264] [--span 0.3] [--engine gpu|cpu] [--numerics portable32]
"""
import argparse
import json
import math
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import ROOT, Sweep, interpolate, run  # noqa: E402

KINDS = ["breach", "over", "down", "top"]
SHIP = os.path.join("ships", "titanic", "titanic64.ship.json")
SIM = os.path.join("ships", "titanic", "sims", "titanic.sim.json")


def load_observations() -> dict:
    with open(os.path.join(ROOT, "data", "observations", "titanic-1912.json"), "r", encoding="utf-8") as f:
        return json.load(f)


def objective(inst: dict, obs: dict) -> tuple[float, list]:
    """The oracle's calibration objective (oracle/js/tools/calibrate.js) on one instance's results."""
    ob = obs["objective"]
    curve = inst["curve"]
    foundered = inst["foundered"]
    tf_min = inst["founderT"] / 60 if foundered else ob["afloatFounderMin"]
    founder_sec = inst["founderT"] if foundered else 1e9
    J = 0.0
    rows = []
    for p in obs["trim"]:
        m = p["min"]
        if m == 0:
            continue
        sigma = next(b["sigma"] for b in ob["trimSigmaByMinute"] if m <= b["upTo"])
        v = ob["trimAfterFounder"] if m * 60 > founder_sec else interpolate(curve, "trim", m * 60)
        J += ((v - p["deg"]) / sigma) ** 2
        rows.append(("trim", m, p["deg"], v))
    J += ((tf_min - obs["founderMin"]) / ob["founderSigmaMin"]) ** 2
    rows.append(("founder", "-", obs["founderMin"], tf_min))
    for p in obs["list"]:
        m = p["min"]
        v = ob["listAfterFounder"] if m * 60 > founder_sec else interpolate(curve, "list", m * 60)
        J += ob["listWeight"] * ((v - p["deg"]) / ob["listSigmaDeg"]) ** 2
        rows.append(("list", m, p["deg"], v))
    events = {e["id"]: e["t"] / 60 for e in inst["events"]}
    for e in ob["events"]:
        v = events.get(e["id"], tf_min)
        J += ((v - e["obsMin"]) / e["sigmaMin"]) ** 2
        rows.append(("event:" + e["id"], "-", e["obsMin"], v))
    return J, rows


def evaluate(path: str) -> int:
    """Score every instance of a results file with the oracle's objective."""
    obs = load_observations()
    with open(path, "r", encoding="utf-8") as f:
        res = json.load(f)
    print(f"{path}: {len(res['instances'])} instances, numerics {res.get('numerics')}, {res.get('engine')}")
    for inst in res["instances"]:
        J, rows = objective(inst, obs)
        print(f"  {inst['id']:28s} J = {J:8.3f}   " + ("founders at %.1f min" % (inst["founderT"] / 60) if inst["foundered"] else "afloat"))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--evaluate", default=None, help="score the instances of this results file and exit")
    ap.add_argument("--rounds", type=int, default=5)
    ap.add_argument("--batch", type=int, default=264)
    ap.add_argument("--span", type=float, default=0.3, help="initial half-width of the log-uniform box")
    ap.add_argument("--shrink", type=float, default=0.5)
    ap.add_argument("--engine", default="gpu", choices=["gpu", "cpu"])
    ap.add_argument("--numerics", default="portable32")
    ap.add_argument("--seed", type=int, default=20261008)
    ap.add_argument("--keep", default=None, help="directory to keep sweep and result files in")
    args = ap.parse_args()
    if args.evaluate:
        return evaluate(args.evaluate)
    obs = load_observations()
    rng = random.Random(args.seed)
    ob = obs["objective"]
    center = {k: 0.0 for k in KINDS}          # log scales
    span = args.span
    best = (math.inf, None, None)
    t_start = time.time()
    for r in range(args.rounds):
        sw = Sweep(SHIP, SIM, tmax=ob["tMaxMin"] * 60, every=ob["historyEverySec"])
        points = []
        if r == 0:
            points.append({k: 0.0 for k in KINDS})   # the oracle's own fit, as the reference point
        while len(points) < args.batch:
            points.append({k: center[k] + rng.uniform(-span, span) for k in KINDS})
        for i, p in enumerate(points):
            sw.add(f"r{r}p{i}", scale={k: math.exp(v) for k, v in p.items()})
        t0 = time.time()
        res = run(sw, engine=args.engine, numerics=args.numerics, keep=os.path.join(args.keep, f"round{r}") if args.keep else None)
        dt = time.time() - t0
        scored = []
        for p, inst in zip(points, res["instances"]):
            J, rows = objective(inst, obs)
            scored.append((J, p, rows, inst))
        scored.sort(key=lambda x: x[0])
        J0 = next((s for s in scored if all(v == 0.0 for v in s[1].values())), None)
        print(f"round {r}: {len(points)} points in {dt:.1f} s; best J {scored[0][0]:.3f} at " +
              " ".join(f"{k}={math.exp(v):.4f}" for k, v in scored[0][1].items()) +
              (f"; the oracle's fit scores J {J0[0]:.3f}" if J0 else ""))
        if scored[0][0] < best[0]:
            best = (scored[0][0], scored[0][1], scored[0][2])
        center = dict(scored[0][1])
        span *= args.shrink
    J, p, rows = best
    print(f"\nbest J {J:.3f} after {args.rounds} rounds and {args.rounds * args.batch} evaluations in {time.time() - t_start:.1f} s")
    print("scales: " + " ".join(f"{k}={math.exp(v):.4f}" for k, v in p.items()) + f"   (oracle's best known J {ob['bestKnown']['J']} at scales 1)")
    for kind, m, o, v in rows:
        print(f"  {kind:13s} {str(m):>4s} obs {o:>6} model {v:7.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
