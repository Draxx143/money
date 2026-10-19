# Supported formats

Status is spelled out honestly. "Planned" means there is no code for it yet; the
user interface offers no entry for a planned feature.

## Board files

| Format                       | Extension              | Status  | Notes |
| ---------------------------- | ---------------------- | ------- | ----- |
| KiCad PCB (s-expression)     | `.kicad_pcb`           | Implemented | KiCad 6, 7, 8 and 9 files. Layers, nets, footprints, pads, tracks, vias and the board outline are read from the file; nothing is inferred. What is read, and what is left out, is listed under "KiCad board files" below. |
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

## KiCad board files

Read from the file:

* the **format version** and the generator, so the file's origin is recorded;
* **layers** — canonical name, index and the side (`F.` / `B.`) taken from the name;
* the **net table** — every named net of the board, as confirmed connectivity;
* **footprints** — reference, value, description, manufacturer, part number and
  datasheet (from `property` in KiCad 9, from `fp_text` in KiCad 6 to 8),
  position, rotation and side, with the footprint's own `uuid`/`tstamp` as the
  stable identifier;
* **pads** — pin number, pin name (`pinfunction`), position, size, rotation,
  shape (`rect`, `circle`, `oval`, `roundrect`, `trapezoid`, `custom`), side
  (from the pad's `layers`) and net;
* **tracks** (`segment`) with width, layer and net, and **vias** with diameter,
  drill and net;
* the **board outline** (`Edge.Cuts`): lines, rectangles, polygons, arcs (both
  the three-point form of KiCad 7 and the older `angle` form) and circles. The
  pieces are joined end to end into closed loops, so a board drawn as four
  separate lines becomes one rectangle.

Not read, and reported as a warning on the imported board:

* **copper zones** (filled pours) — the data model does not carry them yet;
* **silkscreen, courtyard and fabrication drawings**, both on the board and
  inside footprints — a component is described by its pads, not by the drawings
  around them;
* **text** (`gr_text`, `fp_text user`) and **3D models**;
* anything else the parser meets: it is counted and named in the warning list
  instead of being ignored quietly.

Arcs and circles are drawn as polylines with 32 segments, which is reported as
well. Nothing in this list is guessed: a value the file does not contain stays
unknown, and a board without an `Edge.Cuts` outline keeps an empty bounding box
rather than a rectangle invented from the footprints.

The importer was checked against real boards, not only against the test
fixtures: the AVR demo boards (`at90s1200`, `at90s2323`, `at90s8535`, KiCad
format 20221018, up to 171 footprints and 595 tracks) import with every
reference and value intact, in well under a second each.
