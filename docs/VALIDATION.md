# Validation

## Targets

Calibration (four parameters, `oracle/js/tools/calibrate.js`): Halpern's eyewitness-derived trim curve, the
foundering time, and three timeline events (water over bulkhead F, the well deck awash, the bridge under). The fitted
values: aggregate opening 0.7506 m², E-deck path width factor 1.182, openings down through E deck 0.277 m² per side
and compartment, 1.0 % of the top deck open once submerged.

Validation cases not used in the fit: Wilding's four- and five-compartment cases, Olympic and Hawke (1911), Britannic
(1916) with and without open portholes, the 1912 damage without the boiler-room-5 seam, stopping at hold 3, with every
bulkhead raised to D deck and to B deck, and with the watertight doors left open.

## Current table

Produced by `sinksim_validate ships/titanic/catalog.json --compare data/validation/titanic64/validation.oracle.json`;
the engine's results equal the oracle's on Node 24 at the printed precision for every case.

| Case | Record | Model |
|---|---|---|
| Titanic 1912, calibrated | foundered 2:20 (broke 2:17) | founders at 2 h 37 min after impact (2:17) |
| Four forward compartments | Wilding: floats | afloat, trim 2.04°, 8,979 t aboard |
| Five forward compartments | Wilding: sinks | founders at 2 h 14 min |
| Olympic and HMS Hawke 1911 | returned to port | afloat, trim −0.74° |
| Britannic 1916, portholes open | about 55 min | founders at 0 h 50 min |
| Britannic, portholes shut | designed to float with six flooded | afloat, trim 8.79° |
| 1912 damage without the BR5 seam | Wilding: still sinks, slower | founders at 3 h 08 min |
| 1912 damage stopping at hold 3 | floats | afloat, trim 2.04° |
| Bulkheads to D deck | Wilding: no help | founders at 2 h 50 min |
| Bulkheads to B deck | Wilding: still sinks | afloat, trim 8.09° (disagrees with Wilding; agrees with the later Britannic design claim) |
| Watertight doors left open | debated | founders at 3 h 02 min, later than with doors closed |

Timeline of the calibrated run: centre propeller clear at 48 min; water over bulkheads C, D, E at 61, 67, 68 min;
over F at 76 min (witnessed 12:55, model 12:56); over B and A at D deck at 85 and 86 min; forecastle ports under at
89 min; over G at 102 min; well deck awash at 124 min (witnessed about 130); forecastle head under at 135 min;
bridge under at 154 min (witnessed 155); founders at 157 min.

## Not validated, and known wrong

- The list. The model gives a port list of 2° to 6° from the pre-collision coal list and free surfaces, not the 5°
  starboard list at 11:50 nor the 10° to 15° port list from 1:50; the mechanisms need room-level topology (roadmap
  phase 3 and 4).
- The end: the run stops at the pitch runaway, not at the break-up; no hull-girder model yet.
- No pumps, no trapped air, no buoyancy above B deck, no forward speed (which matters for Britannic).
- The doors-open result depends on door areas and sills that were guesses; it is a hypothesis until phase 3 puts
  plan-derived values and intervals on it.

## Running it

`tools\build\msvc.cmd test` runs the whole suite. Individually: `sinksim_validate` prints the table and the comparison;
`sinksim_check` prints the golden comparison with a divergence profile; `npm run validate:oracle` regenerates the
oracle's table on the pinned Node.
