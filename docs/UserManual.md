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
* **Board** (centre) – the board view. Mouse wheel zooms, dragging pans,
  `Fit` / `1:1` restore the view. The status bar shows the cursor position in
  millimetres and the current zoom factor.
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

## 4. Settings

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

## 5. Privacy

Nothing leaves the computer unless you explicitly configure an AI provider and
enable network access on the *Privacy & Network* page. Both are off by default,
and the Assistant panel always shows the current state instead of a green tick
that might mean anything.
