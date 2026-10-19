# Troubleshooting

## The application does not start

Run it from a terminal and read the output:

```
pcb-workbench
```

The most common causes:

* **No writable home.** The application needs to create
  `~/.config/pcb-workbench`. If `HOME` is unset or read-only, startup fails with
  a dialog that names the directory. The environment variable
  `PCB_WORKBENCH_HOME` redirects everything to another location — useful when
  running from a read-only account or in a container.
* **Missing Qt platform plugin.** On a headless machine set
  `QT_QPA_PLATFORM=offscreen`. On a normal desktop Qt picks the right plugin
  automatically; if `libxcb-cursor.so.0` is missing, install it with the
  distribution's package manager.
* **Mixed Qt versions.** `Help → About` shows the Qt version the application was
  compiled against and the one it is running with. They must match; if they do
  not, the application is linking against a different Qt than the one on the
  library path.

## Where are the logs?

`~/.local/state/pcb-workbench/logs/app-YYYY-MM-DD.log` (or
`$PCB_WORKBENCH_HOME/state/pcb-workbench/logs/…`). The **Log** panel in the
application shows the same entries live and can save them to a file.

## The workspace layout is broken

`View → Reset Workspace Layout` restores the default docking. The layout is
stored in `~/.config/pcb-workbench/workspace.ini`; deleting that file has the
same effect.

## A project will not open

* *"… is not a project file"* – the file was produced by another tool. Open it
  with the importer of its own format instead.
* *"… was written by a newer version"* – install a newer build; the file is not
  modified, and the application refuses to load it rather than losing data.
* *"… cannot be opened for reading"* – permissions or a broken path.
* *"invalid project file"* – the file is truncated or damaged. A copy of the last
  good state may still be in `~/.local/share/pcb-workbench/backups/`.

## Recovering unsaved work

Autosave writes to `~/.cache/pcb-workbench/recovery.pwb`. On startup, if that
file is newer than the project on disk, the application asks whether to restore
it. A restored project is always marked as modified.

## Reporting a problem

Include the version and Qt version from `Help → About`, the log file of the day,
and the exact steps that reproduce the problem. If a board file is involved,
mention its format and the tool that produced it.
