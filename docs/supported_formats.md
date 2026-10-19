# Supported formats

Status is spelled out honestly. "Planned" means there is no code for it yet; the
user interface offers no entry for a planned feature.

## Board files

| Format                       | Extension              | Status  | Notes |
| ---------------------------- | ---------------------- | ------- | ----- |
| KiCad PCB (s-expression)     | `.kicad_pcb`           | Planned | Target for the first real importer, milestone M3. Geometry, layers, nets, footprints and pads are read from the file; nothing is inferred. |
| KiCad schematic              | `.kicad_sch`           | Planned | Later milestone, used for connectivity cross-checking only. |
| Gerber RS-274X               | `.gbr`, `.gbl`, …      | Planned | Copper layers, drill and outline. |
| Excellon drill               | `.drl`                 | Planned | Via and hole geometry. |
| ODB++                        | directory              | Not planned | Proprietary, needs a licence to test against. |
| Altium                       | `.PcbDoc`              | Not planned | Undocumented binary format. |

## Bill of materials and component data

| Format                | Extension     | Status  | Notes |
| --------------------- | ------------- | ------- | ----- |
| CSV                   | `.csv`        | Planned | Header detection, column mapping dialog, delimiter guessing. |
| Excel                 | `.xlsx`       | Planned | Requires a reader; no fabricated rows on failure. |
| JSON                  | `.json`       | Planned | Simple array-of-objects mapping. |

## Documents

| Format | Extension | Status  | Notes |
| ------ | --------- | ------- | ----- |
| PDF    | `.pdf`    | Planned | Text and tables are extracted, page numbers are kept as provenance. |
| Images | `.png`, `.jpg` | Planned | Stored as references for manual inspection. |

## Project files

| What            | Extension | Status      | Notes |
| --------------- | --------- | ----------- | ----- |
| Project         | `.pwb`    | Implemented | JSON, schema version 1. Forward compatible: a file written by a newer version of the application is refused with a clear message instead of being half-loaded. |

## Rule for every importer

1. Parse the file. On a parse error, report the file, the line if known, and
   stop — never import partial data silently.
2. Everything that could not be understood is collected in the board's warning
   list and shown to the user.
3. No value is ever invented. A component without a value stays without a value;
   a test point without an expected value stays without one. The distinction
   between "unknown" and "zero" is kept in the data model
   (`hasExpectedValue`, `hasTolerance`, `hasPosition`, `hasGeometry`).
