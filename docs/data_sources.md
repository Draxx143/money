# Data sources and provenance

Every number, document extract and suggestion in a project records **where it
came from** and **whether anybody checked it**. This file defines those levels.

## Verification status

| Status            | Meaning |
| ----------------- | ------- |
| Unknown           | The origin is not recorded. Treated as unverified. |
| ExtractedFromSource | Read out of a file the user imported (BOM, PDF, board file). |
| OfficialSourceVerified | Comes from the manufacturer's documentation and a human confirmed it. |
| UserVerified      | A human checked it against the physical board or an official document. |
| AiSuggested       | Proposed by a language model. **Never** treated as a fact. |
| Disputed          | Two sources disagree; the conflict is shown, not resolved silently. |

## Value source

`measured`, `extracted`, `datasheet`, `user`, `ai-suggested`, `derived`,
`unknown`.

## Rules the code follows

1. A value suggested by a model is always labelled `ai-suggested` and can never
   be promoted to `measured`.
2. When two sources disagree the software keeps both and marks the value
   `Disputed`. It never picks one silently.
3. A measurement that has no expected value is reported as "not tested", never
   as a pass or a fail.
4. Reports state, for every number, which of the levels above applies.

## Local knowledge database

`~/.local/share/pcb-workbench/knowledge.sqlite3` holds what the application
learns across projects: component records (part number, manufacturer, category,
datasheet) and facts about them — a pinout, an absolute maximum, a measured
value, a typical failure — each with the source it came from. It is a local
file, it is not uploaded, and deleting it only loses those notes: projects keep
working.

Search uses SQLite's FTS5 index over the statement, the topic and the source.
The database reports which search it actually ran (`fts5` for the index, `text
scan` for the fallback), so a result list never pretends to be something it is
not.

What goes into the database, and what does not:

| Written | When | Provenance |
| ------- | ---- | ---------- |
| A component record | a board file names a part number | `ExtractedFromSource`, with the board file and the footprint as the location |
| A component record completed by hand | you edit a part number or datasheet in the Component panel | `NeedsReview`, `ManualEntry` |
| A fact you add | `Add...` in the Knowledge tab | what you enter: with a source it keeps the status you choose, without one it is stored as `Unverified` |
| A fact from a datasheet or from a model | not yet — there is no importer or assistant that writes facts automatically | — |

A component that the board file does not name (a bare "100n", a connector
without a part number) is deliberately **not** recorded: a part number guessed
from a value would be invented data.

## What is not a source

* A filename.
* A reference designator similarity ("this looks like the same part").
* Anything the software would have to invent to fill a field.
