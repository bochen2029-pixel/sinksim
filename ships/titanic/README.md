# RMS Titanic, compiled model `titanic64`

`titanic64.ship.json` is the hull and subdivision of the original JavaScript model, compiled to the engine's format
(docs/FORMATS.md): a parametric hull fitted to the Harland and Wolff hydrostatics, integrated as 4,408 vertical columns,
divided by the 15 bulkheads of the British Inquiry into 16 compartments, each split into port and starboard halves below
and above E deck. That makes 64 nodes and 8,706 column segments.

`sims/*.sim.json` are the scenarios on that ship: the connection network (doors, bulkhead overflow paths, openings
through E deck, cross-flow, deck openings, breaches), the rigid-body constants and the initial state. `catalog.json`
lists them with the run options the validation uses. The ids are those of the original validation table so that
results stay comparable:

| id | scenario |
|---|---|
| `titanic` | 14 April 1912, calibrated iceberg damage, doors closed from the bridge |
| `four` | forepeak and holds 1 to 3 open to the sea (Wilding: floats) |
| `five` | the four plus boiler room 6 (Wilding: sinks) |
| `hawke` | Olympic and HMS Hawke, 1911, two after compartments |
| `britannic`, `britannicClosed` | Britannic 1916, approximate, with and without the open portholes |
| `titanicNoBR5`, `titanicFour` | the 1912 damage without the boiler room 5 seam; stopping at hold 3 |
| `titanicD`, `titanicB` | the 1912 damage with every bulkhead raised to D deck; to B deck |
| `titanicDoors` | the 1912 damage with the watertight doors left open |

Everything here is generated: `npm run export` rebuilds it from the oracle, and the engine refuses a file whose
arrays do not hash to the values written at export time. Parameters, sources and the fit are described in
`oracle/js/README.md`; the deck-level successor to this model is roadmap phase 3 (docs/ROADMAP.md).
