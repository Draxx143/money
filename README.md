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

Board file import, the knowledge database, measurements, the diagnostic engine
and the optional AI assistant follow in later milestones. They are not advertised
as working today, and there is no menu entry for them yet.

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
