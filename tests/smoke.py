"""Run with /usr/bin/python3 under Xvfb, with both GTK 3 and 4 installed."""
import runpy
from pathlib import Path
from types import SimpleNamespace

app = runpy.run_path(str(Path(__file__).resolve().parents[1] / "src/indigo-screen"))
Gdk, Gtk = app["Gdk"], app["Gtk"]
assert Gdk._version == "3.0"
assert Gtk._version == "3.0"
window = app["IndigoScreen"]()
window.show_all()
while Gtk.events_pending():
    Gtk.main_iteration()
assert window.get_realized()
assert not window.get_decorated()
assert window.on_key_press(window, SimpleNamespace(keyval=Gdk.KEY_a)) is False
# Escape must close the window inside an active main loop.
from gi.repository import GLib
GLib.idle_add(window.on_key_press, window, SimpleNamespace(keyval=Gdk.KEY_Escape))
Gtk.main()
print("GTK 3 / Gdk 3 window creation and Escape smoke test passed")
