# Titanic flooding model: web proof of concept

This folder holds a headless flooding core in plain JavaScript and a three.js viewer that only reads the core's state. The core has no DOM, uses flat typed arrays and a fixed time step, so it ports line for line to C++/CUDA (see `PORTING.md`).

## Run it

- **Viewer:** open `out/titanic.html` in a browser. three.js and the fonts load from CDNs. To rebuild it, run `node tools/build.js`.
- **Headless:** you need Node 18 or later; nothing to install.
  - `node tools/titanic.js`: the 1912 run as a table, compared with the eyewitness trim and list.
  - `node tools/validate.js`: the validation cases; writes `out/validation.json`.
  - `node tools/cases.js`: the classic one- to six-compartment damage cases.
  - `node tools/calibrate.js 45`: refits the four flow parameters (takes about 4 minutes).
  - `node tools/fit_hull.js`: refits the hull form to the Harland & Wolff hydrostatics (takes about 2 minutes).

## What the core does

| Piece | How |
|---|---|
| Hull | Parametric form (parallel midbody, waterline exponents by height, bilge radius, raked stem, elliptical counter, sheer, tumblehome). It is fitted to the H&W hydrostatics at 32'3" and 34'7", and lands within about 1% on displacement, waterplane area, KB, BM, LCB and Cm. |
| Proxies | 4,408 vertical columns (3 m × 0.5 m) in the ship frame. Buoyancy and flood water are both integrated over the same columns at the actual pose, so there are no lookup tables. |
| Subdivision | 15 bulkheads at the British Inquiry positions, with tops at C/D/E deck as built, giving 16 compartments. Each compartment has 4 spaces: port/starboard × below/above E deck, 64 in all. Permeability is set per compartment. |
| Water paths | 290 connections. Hull openings, doors and deck openings use an orifice law; flow over bulkhead tops and the open deck uses a weir law (Villemonte for submerged flow). Port and starboard halves of an open space share one free surface. A full space is "pressed up" by the head above it. |
| Ship | Damped rigid body in heave, pitch and roll. Mass properties come from the drafts of 14 April (30'9" forward, 33'9" aft), with GM 2 ft 7.5 in (Hackett & Bedford). |
| Step | 0.25 s. About 11,000 steps/s single-threaded, so a full sinking takes about 3 s headless. |

## Calibration (4 parameters, `tools/calibrate.js`)

The fit targets are Halpern's eyewitness-derived trim curve (2023), the foundering time, and three timeline events.

| Parameter | Value | Meaning |
|---|---|---|
| Aggregate opening | 0.75 m² (8.1 sq ft) | Iceberg damage, spread over 6 compartments by Hackett & Bedford's flooded capacities |
| `wMul` | 1.18 | Width of the paths along E deck (Scotland Road / alleyway) |
| `aDown` | 0.28 m² | Openings down through E deck, per side per compartment |
| `fOpen` | 0.010 | Share of the top deck open once submerged |

| Check | Witnessed | Model |
|---|---|---|
| Trim at 12:10 / 12:55 / 1:20 / 2:15 | 1.8° / 3.2° / 4.0° / 10° | 1.65° / 3.5° / 4.15° / 9.6° |
| Water over bulkhead F (Wheat) | 12:55 | 12:56 |
| Bridge under | 2:15 | 2:14 |
| Foundered | 2:20 (broke at 2:17) | 2:17 |
| 16,000 tons aboard | 12:20 (Wilding's assumption) | 12:48 |

The eyewitness timeline fits about 8 sq ft. Wilding's 12 sq ft rested on 16,000 tons arriving in 40 minutes; the model takes 68 minutes to reach that much.

## Validation (not used in the fit)

| Case | Record | Model |
|---|---|---|
| Forepeak + holds 1–3 open | Wilding: floats | Floats, 2.0° by the head, 0.96 m below bulkhead D's top |
| Add boiler room 6 | Wilding: sinks | Founders |
| Olympic–Hawke 1911, two after compartments | Reached port | Floats, 0.7° by the stern |
| Britannic 1916 (approx.: B-deck bulkheads, failed doors, 25 open ports) | About 55 min | 50 min |
| Britannic, ports shut | Designed to float with 6 flooded | Floats, 8.8° |
| Titanic damage minus the BR5 seam | Wilding: still sinks, slower | 3 h 08 min |
| Titanic, bulkheads to D deck | Wilding: no help | 2 h 50 min |
| Titanic, bulkheads to B deck | Wilding: still sinks | Floats at 8.1°. This disagrees with Wilding; it agrees with the later Britannic design claim |
| Titanic, doors left open | Debated | 3 h 02 min, longer: water spreads aft low instead of tipping the bow |

## Known gaps

- **List.** The model gets a 2–6° port list (the pre-collision coal list amplified by free surfaces), but not the 5° starboard list at 11:50 or the 10–15° port list at 1:50–2:05. The mechanism the witnesses' list needs is not in the model yet. Candidates are asymmetric flooding of the boiler-room bunkers, water running aft along Scotland Road, and the port gangway door Lightoller ordered opened.
- **Structure.** There is no hull girder failure. Intact, the model's last minute is a rolling plunge rather than the break-up at about 2:17.
- **Other omissions.** No pumps, no trapped air, no buoyancy from the superstructure above B deck, no forward speed (which matters for Britannic).
- **Britannic.** Its hull is treated as Titanic's: same lines, slightly narrower beam. Which six bulkheads went to B deck, and where the open ports were, are assumptions.

## Sources

- British Wreck Commissioner's report: compartment lengths and bulkhead decks.
- Halpern, *A Matter of Stability and Trim*: displacement, KB, BM, GM, LCF, drafts.
- Halpern, *Lifeboats, Launch Times, List and Trim* Part II (2023): trim and list against time.
- Wilding's evidence: 12 sq ft, 16,000 tons, head of about 25 ft.
- Hackett & Bedford, RINA 1996.
- Chirnside on Britannic's open ports and Olympic/Britannic subdivision.
