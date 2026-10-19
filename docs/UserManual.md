# User Manual

This manual describes what the application can do **today**. Features that are
planned but not implemented yet are listed in
[supported_formats.md](supported_formats.md) and are never advertised as working
on screen: a menu entry for an unfinished feature does not exist.

## 1. Starting the application

```
pcb-workbench                 # start with an empty workspace
pcb-workbench --project a.pwb # open a project straight away
pcb-workbench --version       # print the version and exit
```

Everything the application writes lives in user writable directories:

| What            | Linux (XDG)                                        |
| --------------- | -------------------------------------------------- |
| Configuration   | `~/.config/pcb-workbench/settings.conf`            |
| Projects        | `~/.local/share/pcb-workbench/projects/`           |
| Database        | `~/.local/share/pcb-workbench/knowledge.sqlite`    |
| Autosave        | `~/.cache/pcb-workbench/recovery.pwb`              |
| Logs            | `~/.local/state/pcb-workbench/logs/`               |

Setting the environment variable `PCB_WORKBENCH_HOME` moves the whole tree. This
is what the automated test suite uses so that it never touches a real home
directory. The installation directory is never written to, so the packaged
application runs as a normal user.

## 2. The workspace

The window is a classic workbench with dockable panels:

* **Components** (left) – every component of the imported board. The search box
  filters reference, value, package and manufacturer. The two drop-downs filter
  by category and by board side. The row count under the list always tells you
  how many components are currently visible, so a filtered view can never be
  mistaken for the whole board.
* **Board** (centre) – the board view. Dragging pans, the mouse wheel scrolls,
  `Ctrl` + wheel zooms at the cursor, `View → Zoom In/Out`, `Fit Board to View`
  (`F`) and `Reset View` (`0`) control the zoom. `View → Show Board Back` (`B`)
  mirrors the board the way it looks when you turn it over; the designators stay
  readable and the board data is not modified. The status bar shows the cursor
  position in millimetres, the current zoom factor and which side is shown.
* **Component details** (right) – everything about the component you selected:
  what the board file said, what you completed by hand, its pads with their
  nets, and what is stored about it in the knowledge database.
* **Assistant** (right) – the status of the optional AI features. Until a
  provider is configured it says so plainly and lists which diagnostic features
  work completely offline.
* **Log** (bottom) – every message the application produced, with timestamps and
  severity. Use the toolbar of that panel to copy or save the log when reporting
  a problem.

The layout (panel positions, sizes, window geometry) is remembered between runs
in `workspace.ini`. `View → Reset Workspace Layout` restores the default.

## 3. Projects

`File → New Project`, `Open Project…`, `Save`, `Save As…` and `Close` behave the
way you expect. The project file uses the extension `.pwb` (JSON, UTF-8).

Autosave: whenever a project has unsaved changes, a copy is kept next to the
cache directory. If the application is closed with unsaved changes you are asked
whether to keep them; on the next start the application offers to restore the
recovered data. A restored project is always marked as modified, so `Save` can
never silently overwrite the file on disk.

## 4. Importing a board

`File → Import Board File…` (`Ctrl+I`) reads a KiCad board file (`.kicad_pcb`)
into the open project. A board always belongs to a project: the notes, test
points and measurements you add are stored next to it, and the import is
recorded in the project so that you can see where the data came from.

What is read, and what is deliberately left out, is listed in
[supported_formats.md](supported_formats.md#kicad-board-files). The short
version: layers, nets, footprints, pads, tracks, vias and the board outline are
imported; zones, silkscreen and fabrication drawings, text and 3D models are
not, and every one of them is named in a warning.

Importing a second board replaces the board of the project. The components of
the new board are a fresh set, so notes that were attached to the previous board
are not silently carried over.

After an import:

* the **Components** panel lists every footprint, with its reference, value and
  side as the file gave them (a component the file does not name has no name
  here either);
* the **Board** panel draws the board outline, the copper tracks, the vias and
  every pad in its real shape, at its real position;
* the **status bar** reports the board: `project name | 34 components, 32 nets |
  board file`;
* the **Log** panel lists everything that was skipped, one line per kind.

If the file cannot be read — it is not a KiCad board, it is truncated, or it is
empty — nothing is imported at all. The error message names the file and the
line where parsing stopped.

## 5. Component details and knowledge

Select a component in the list, or click it on the board, and the **Component
details** panel shows it in two tabs.

**Component** – value, description, category, manufacturer, part number,
package, datasheet link and notes. The fields are filled with what the board
file said; you can complete them. As soon as you do, the component is marked as
*partly hand entered* (`Needs review`), because it is now file data and your
data mixed — it is never presented as confirmed. Below the fields, the panel
names the source of the data and lists the pads of the component with their pin
name, net and position.

**Knowledge** – the facts stored about this component in the local database.
Empty search box: this component's facts. Type something: every fact in the
database is searched, and the panel says whether the full text index or a plain
text scan answered. `Add...` asks for the statement, an optional value with its
unit and conditions, and — as prominently — where it came from. A fact stored
without a source is kept, but marked `Unverified`: it is a note, not evidence.
Click a fact to see it in full, with its source and its status.

The knowledge database is a single SQLite file in your data directory
(`knowledge.sqlite3`). It is shared by all projects, it never leaves the
computer, and `Settings → Database` shows where it is, how large it is, how many
records and facts it holds, and lets you check and compact it.

## 6. Settings

`Settings → Configure…` opens a nine page dialog:

1. **Identity** – the application name, subtitle, organisation, icon and accent
   colours. The name you enter here is used in the window title, the About dialog
   and in report headers; it is never hardcoded.
2. **Appearance** – theme and view defaults.
3. **Language** – English only at the moment; the page exists so that a
   translation can be added without changing the code.
4. **Directories** – where projects, logs, backups and the database live.
5. **AI Provider** – the optional remote assistant. Disabled until you configure
   it.
6. **Privacy & Network** – whether the application is allowed to send anything
   off this computer. Off by default.
7. **Database** – location of the local knowledge database.
8. **Logging** – log level, how many days of log files are kept.
9. **Import Options** – defaults applied when a board file is imported.

## 7. Privacy

Nothing leaves the computer unless you explicitly configure an AI provider and
enable network access on the *Privacy & Network* page. Both are off by default,
and the Assistant panel always shows the current state instead of a green tick
that might mean anything.
