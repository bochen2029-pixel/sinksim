# Sources

What the model rests on, what each source gives, and what is still to be fetched. From roadmap phase 3 on, every
number in a ship or scenario file cites one of these by key.

## In use (through the original model)

| Key | Source | Gives |
|---|---|---|
| `BWC1912` | British Wreck Commissioner's report, 1912 | compartment lengths, bulkhead positions and the decks they reach |
| `Wilding1912` | Edward Wilding's evidence at the British Inquiry | 12 sq ft aggregate damage, 16,000 tons in 40 minutes, the four- and five-compartment cases |
| `Halpern-stability` | S. Halpern, *A Matter of Stability and Trim* | displacement, KB, BM, GM, LCF, drafts on the night |
| `Halpern2023` | S. Halpern, *Lifeboats, Launch Times, List and Trim*, part II (2023) | eyewitness-derived trim and list against time, the calibration and validation targets |
| `HB1996` | D. Hackett and J. G. Bedford, *The Sinking of S.S. Titanic, Investigated by Modern Techniques*, RINA Transactions 1996 | flooded capacities per compartment, GM, and the flooding conditions C1 to C7 with drafts at times (the stronger calibration target still to be fully used) |
| `Chirnside` | M. Chirnside on Olympic, Titanic and Britannic | Britannic's open portholes and subdivision, Olympic and Hawke |
| `Halpern-list` | S. Halpern, *Titanic's Initial List to Starboard* (titanicology.com) | the mechanism of the 5° starboard list at 11:50 |

## To fetch for phases 3 and 4

| Key | Source | Needed for |
|---|---|---|
| `Shipbuilder1911` | *The Shipbuilder*, special number on Olympic and Titanic, 1911 (public domain) | general-arrangement plans for the deck-level space digitisation; machinery and bunker layout |
| `StettlerThomas2013` | J. W. Stettler and B. S. Thomas, *Flooding and structural forensic analysis of the sinking of the RMS Titanic*, Ships and Offshore Structures 8 (2013) | flooding analysis with modern tools, hull-girder loads, break-up conditions |
| `Garzke1997` | W. H. Garzke et al., *Titanic, the Anatomy of a Disaster*, SNAME 1997 | bending moments, hull strength estimate, break-up |
| `Reappraisal2011` | *Report into the Loss of the SS Titanic: A Centennial Reappraisal*, 2011 | flooding and break-up chapters, testimony synthesis |
| `SNAME-MFC-2022` | SNAME Marine Forensics Committee break-up papers, 2022 | break angle and sequence |
| `Ruponen2007` | P. Ruponen, *Progressive Flooding of a Damaged Passenger Ship*, Helsinki University of Technology, 2007 | the pressure-correction method, air compression, the method's validation cases: the peer group for the scheme |
| `ITTC-flooding` | ITTC benchmark studies on time-domain flooding | benchmark cases for the generic engine |
| `UCL2025` | the 2025 UCL flooding simulation from *Titanic: The Digital Resurrection*, if published | an independent modern comparison |
| `Testimony` | the US Senate and British Inquiry testimony (Barrett, Hendrickson, Dillon, Scott, Lightoller and others) | the door and pump schedule as a scenario, the port gangway door, boiler room 5 |

## Other ships (phase 7)

Lusitania (1915, 18 minutes, heavy list, speed), Empress of Ireland (1914, 14 minutes), Andrea Doria (1956, 11 hours
of asymmetric flooding, the best test of a list model), Costa Concordia (2012, grounding, modern investigation data),
Estonia (1994, car deck free surface). Each gets its own `ships/<name>/` with its own sources table.

## Convention for citations in data files

A ship or scenario file carries `"sources": { "<key>": "<full reference>" }` and, on each parameter object,
`"source": "<key>", "note": "<page or figure, what was read, how it was converted>"`. Unsourced numbers are allowed
only with `"source": "estimate"` and a note saying how the estimate was made and how uncertain it is.
