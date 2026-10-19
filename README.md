# PCB Workbench

A desktop workbench for **analysing printed circuit boards and documenting
diagnostics**, written in C++ with Qt 6 Widgets.

The program reads real board data (KiCad PCB files first), keeps every value
together with its provenance, and helps you work through a fault methodically:
components, nets, test points, measurements, hypotheses and the evidence for and
against each one.

It is an independent implementation. The feature set is intentionally in the same
category as commercial board-analysis tools, but no code, data or asset from such
a tool is used here.

## Status

Milestone 1 (desktop foundation):

* [x] CMake build, C++20, Qt 6 Widgets
* [x] Configurable identity — the product name is configuration, not a constant
* [x] XDG paths, project document with autosave and crash recovery
* [x] Workbench shell: menus, toolbar, dockable panels, board viewer, log viewer
* [x] Settings dialog, English UI, dark industrial palette
* [x] Automated tests (unit, integration, UI) with `ctest`
* [x] `.deb` package

Milestone 2 (real board import):

* [x] KiCad `.kicad_pcb` importer: layers, nets, footprints, pads (with shape,
      rotation and side), tracks, vias and the board outline
* [x] A strict s-expression reader that reports the line and column of a syntax
      error instead of importing a half-read file
* [x] Board outline built from `Edge.Cuts`: lines, rectangles, polygons, arcs and
      circles, joined into closed loops
* [x] Real geometry in the board view: substrate, outline, tracks, vias, pads in
      their own shape, and one clickable group per component
* [x] `File → Import Board File…`, `View → Show Board Back`
* [x] Every skipped element (zones, silkscreen, text, 3D models) is reported as a
      warning on the board and in the log; nothing is invented
* [x] Tests: s-expression reader, importer, and the whole path from a file to the
      explorer and the viewer

Milestone 3 (knowledge base):

* [x] Local SQLite database, schema versioned and migrated, never silently
      recreated when it is damaged
* [x] Component records, filed by part number and manufacturer, with provenance
* [x] Facts with topic, value, conditions and source, full text indexed (FTS5);
      a fact without a source is stored as unverified, nothing is invented
* [x] Component details panel: what the file said, what you completed, the pads
      with their nets, and the knowledge stored about the part
* [x] Importing a board records the part numbers it names, with the board file
      as their source
* [x] Database page in the settings: location, size, counts, integrity check and
      compact
* [x] Tests: 15 database cases (schema, FTS5 search, hostile input, damaged
      file, newer version, reopen) and 3 UI cases for the panel

Milestone 4 (test points and measurements):

* [x] Test points in the project: identifier, component and pad, net, category,
      expected value with a tolerance in absolute terms or in percent, the
      conditions, the instrument, where to put the probes and the source of the
      expectation
* [x] Measurements stored per test point, with instrument, conditions and notes;
      the whole history is kept, the newest one decides the current verdict
* [x] Honest verdicts: `Pass` and `Fail` only where an expected value exists;
      `Unknown` with a reason when there is none or when the units mean
      different things; `Not tested` when nothing was measured yet. Nothing is
      invented, and a missing expectation is never reported as a pass or a fail
* [x] Units are compared by meaning (mV and V are the same quantity, V and A are
      not) and values are scaled into the unit of the expectation before the
      tolerance is applied
* [x] Test points panel: list with the expected value and the newest reading,
      filter, history with the reason for each verdict, and a coverage line
      counting what has really been measured
* [x] Test points and measurements are saved with the project
* [x] Tests: 14 measurement cases (tolerances, edges, percent, units, coverage,
      coverage of what was not measured, round trip through the project) and 3
      UI cases for the panel

Milestone 5 (diagnostic engine):

* [x] Evidence as a first class record: what it is, which way it points, why it
      counts and how trustworthy it is, attached to one hypothesis
* [x] Every weight is explained. A repair counts 3.0, a measurement 2.0, a
      source document 1.0, an observation 0.5, prior knowledge 0.25 -- scaled
      down by how trustworthy it is and halved again when it was only suggested.
      A reading that could not be judged is kept, but weighs nothing
* [x] Honest scores: no evidence means a score of zero and the status
      "possible". One reading confirms nothing however clear it looks; a
      successful repair is the only single piece that confirms on its own
* [x] The status you recorded and the status the evidence suggests are shown
      side by side; the panel never overwrites the user's word
* [x] Candidates built only from data in the project: a reading that failed
      names its net and the parts on it, and every candidate carries the reading
      it came from. When nothing failed, the panel says so instead of inventing
      something
* [x] "Measure next": the test points that would tell us the most, ranked, with
      the reason for each; a reading that proves nothing yet is listed as a gap
      to close, not as a result
* [x] Sessions: title, symptoms, the readings filed under them, and an outcome
* [x] Tests: 19 diagnostic cases and 3 UI cases for the panel

The optional AI assistant follows in a later milestone. It is not advertised as
working today, and there is no menu entry for it yet.

## Building

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target package    # writes dist/*.deb
```

Requirements: CMake ≥ 3.20, a C++20 compiler and Qt 6 (Core, Gui, Widgets,
Network, Sql, Concurrent, Test, PrintSupport).

On a machine **without** Qt 6 development packages, configure with
`-DUSE_SYSTEM_QT6=OFF` (the default here) after provisioning the toolchain once:

```bash
scripts/toolchain/setup-toolchain.sh
```

That builds `moc` from the Qt sources and points the build at the Qt runtime
libraries that ship with a PySide6 wheel. It exists so that this repository can
be built in a container where the system packages are unavailable; on a normal
development machine use the system Qt and `USE_SYSTEM_QT6=ON`.

## Running the tests

```bash
ctest --test-dir build --output-on-failure
```

The suite redirects the entire application state into a temporary directory
(`PCB_WORKBENCH_HOME`), so it never touches your configuration or projects.

## Packages

```bash
scripts/package-deb.sh          # writes dist/pcb-workbench_<version>_amd64.deb
sudo apt install ./dist/pcb-workbench_0.1.0_amd64.deb
```

Two flavours:

* **Default**: the Qt libraries this build was linked against are installed
  privately under `/usr/lib/pcb-workbench` and are found through the RPATH of the
  binary and the small wrapper script `/usr/bin/pcb-workbench`. The package
  therefore runs on a distribution whose Qt 6 is a different build; it declares
  the host libraries it needs (X11, xcb, GL, ICU-less core, …) as dependencies.
* **`scripts/package-deb.sh --system-qt`**: the package depends on the Qt 6
  packages of the distribution (`libqt6widgets6`, `qt6-qpa-plugins`, …), which is
  what Debian and Ubuntu packaging policy expects. Use this when building *on*
  the target system; the resulting package is smaller and is updated together
  with the rest of the system.

On Ubuntu 24.04 the second variant is the right one, because there Qt 6 comes
from the archive.

## Documentation

* [User manual](docs/UserManual.md)
* [Supported formats](docs/supported_formats.md) — and what is not implemented
* [Architecture](docs/architecture.md)
* [Troubleshooting](docs/troubleshooting.md)
* [Safety](docs/safety.md) — read before probing a live board
* [Data sources and provenance](docs/data_sources.md)

## Licence

MIT — see [LICENSE](LICENSE).
