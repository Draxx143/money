# Safety

This program helps you understand a printed circuit board. It does not protect
you from the board.

## Electricity

* Mains voltage and the DC bus of a switched-mode supply can kill. A primary-side
  capacitor can hold a lethal charge for minutes after the board is unplugged.
* Measure, do not assume. Verify with a meter that a node is discharged before
  touching it.
* Use an isolating transformer or a current-limited supply when working on a live
  board. Never work on mains-powered equipment alone.
* The application cannot know whether a node is safe. A net called `GND` in a
  file may be at mains potential in the hardware in front of you. **The name in
  the file is not a safety guarantee.**

## Heat and chemicals

* Components, heatsinks and solder joints stay hot after power is removed.
* Soldering produces fumes; work in a ventilated area and wash your hands.
* Lithium cells can catch fire when punctured or shorted.

## About the data this program shows

* Expected values taken from a document are **document values**, not a
  measurement of your board. They are marked as such everywhere.
* A value suggested by a language model is a **hypothesis**. It is labelled as
  one, it is never presented as a measurement, and it is never written into a
  report without that label.
* Nothing in this program decides whether a board is safe to use. If the result
  matters — medical, automotive, aviation, mains-connected equipment — have the
  work checked by a qualified person and by the applicable standards.

## Privacy

* The application sends nothing anywhere unless you configure an AI provider and
  enable network access. Both are off by default.
* When you do enable it, the Assistant panel states on every start what is
  configured and where requests go.
* Project files, board files and logs stay on this computer.
