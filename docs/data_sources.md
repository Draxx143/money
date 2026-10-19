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

`~/.local/share/pcb-workbench/knowledge.sqlite` holds what the application learns
across projects: component descriptions, notes, and the full text index used to
search imported documents (SQLite FTS5). It is a local file, it is not uploaded,
and deleting it only loses those notes — projects keep working.

## What is not a source

* A filename.
* A reference designator similarity ("this looks like the same part").
* Anything the software would have to invent to fill a field.
