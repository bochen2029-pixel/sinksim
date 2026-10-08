"""Drive sinksim sweeps from Python: write a sweep file, run it on the CPU or the GPU, read the results.

    from sweep import Sweep, run
    sw = Sweep(ship="ships/titanic/titanic64.ship.json", sim="ships/titanic/sims/titanic.sim.json", tmax=260*60, every=30)
    sw.add("base")
    sw.add("wider", scale={"over": 1.2}, connections=[{"kind": "door", "idx": 3, "enabled": True, "tOn": 600}])
    results = run(sw, engine="gpu")        # or "cpu"
    results["instances"][1]["founderT"]

Standard library only. The engines are the command-line tools built by CMake; set SINKSIM_BIN or pass bin_dir.
"""
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def bin_dir(engine: str, explicit: str | None = None) -> str:
    if explicit:
        return explicit
    env = os.environ.get("SINKSIM_BIN")
    if env:
        return env
    preset = "msvc-cuda-release" if engine == "gpu" else "msvc-release"
    for p in (preset, "msvc-cuda-release", "msvc-release", "gcc-release"):
        d = os.path.join(ROOT, "build", p, "bin")
        if os.path.isdir(d):
            return d
    raise FileNotFoundError("no build directory found; build with tools/build/msvc.cmd or set SINKSIM_BIN")


def exe(bin_dir_: str, name: str) -> str:
    p = os.path.join(bin_dir_, name + (".exe" if os.name == "nt" else ""))
    if not os.path.exists(p):
        raise FileNotFoundError(p)
    return p


class Sweep:
    def __init__(self, ship: str, sim: str, tmax: float = 5 * 3600, every: float = 30, stop_when_stable: bool = False):
        self.ship, self.sim, self.tmax, self.every, self.stop_when_stable = ship, sim, tmax, every, stop_when_stable
        self.instances: list[dict] = []

    def add(self, id_: str, scale: dict | None = None, connections: list[dict] | None = None) -> None:
        inst: dict = {"id": id_}
        if scale:
            inst["scale"] = dict(scale)
        if connections:
            inst["connections"] = list(connections)
        self.instances.append(inst)

    def document(self) -> dict:
        return {
            "format": "sinksim.sweep", "formatVersion": 1,
            "ship": os.path.basename(self.ship), "sim": os.path.basename(self.sim),
            "tMax": self.tmax, "every": self.every, "stopWhenStable": self.stop_when_stable,
            "instances": self.instances,
        }

    def write(self, path: str) -> str:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(self.document(), f, indent=1)
        return path


def run(sw: Sweep, engine: str = "gpu", numerics: str | None = None, precision: str = "mixed", threads: int = 0,
        bin_dir_: str | None = None, keep: str | None = None, quiet: bool = True) -> dict:
    """Run the sweep and return the parsed results document. engine: "gpu" or "cpu"."""
    d = bin_dir(engine, bin_dir_)
    work = keep or tempfile.mkdtemp(prefix="sinksim-sweep-")
    sweep_path = sw.write(os.path.join(work, "sweep.json"))
    results_path = os.path.join(work, "results.json")
    if engine == "gpu":
        cmd = [exe(d, "sinksim_cuda_run"), sw.ship, sw.sim, "--sweep", sweep_path, "--results", results_path, "--precision", precision]
    else:
        cmd = [exe(d, "sinksim_sweep"), sw.ship, sw.sim, sweep_path, "--results", results_path]
        if numerics:
            cmd += ["--numerics", numerics]
        if threads:
            cmd += ["--threads", str(threads)]
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if not quiet:
        sys.stdout.write(proc.stdout)
    if proc.returncode != 0:
        raise RuntimeError(f"{cmd[0]} failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}")
    with open(results_path, "r", encoding="utf-8") as f:
        return json.load(f)


def interpolate(curve: dict, key: str, t: float, after: float | None = None) -> float:
    """Linear interpolation of a curve column at time t (seconds), as the oracle's calibration does."""
    ts = curve["t"]
    ys = curve[key]
    if t >= ts[-1]:
        return ys[-1] if after is None else after
    for i in range(1, len(ts)):
        if ts[i] >= t:
            u = (t - ts[i - 1]) / (ts[i] - ts[i - 1])
            return ys[i - 1] + u * (ys[i] - ys[i - 1])
    return ys[0]
