// KiCad board importer.
//
// Reads a .kicad_pcb file (the s-expression board format of KiCad 6, 7, 8 and 9)
// into the unified project model, and nothing else: it does not guess, it does
// not fill in, and it does not touch the user interface.
//
// Three rules the code follows, because a repair decision is made on top of this
// data:
//
//   1. Everything that came out of the file is marked as
//      VerificationStatus::ExtractedFromSource with a Provenance that names the
//      file. Nothing in a board file is a measurement.
//   2. Anything the file does not say stays unknown: a component without a value
//      keeps an empty value, a pad without a net keeps an empty net, a board
//      without an outline keeps an empty bounding box.
//   3. Everything that was understood but not imported is reported in
//      <warnings>, verbatim and counted, so the user knows what is missing
//      instead of finding out later.
#pragma once

#include "ProjectModel.h"

#include <QString>
#include <QStringList>

namespace eda {

struct KicadImportResult
{
    bool ok = false;
    Board board;
    QStringList warnings;
    QString formatVersion;   // "(version 20240101)" of the file, empty if absent
    QString generator;       // tool that wrote the file, empty if absent
    int componentCount = 0;
    int padCount = 0;
    int trackCount = 0;
    int viaCount = 0;
};

// Reads a board from a file path.
KicadImportResult importKicadPcb(const QString &path, QString *error = nullptr);

// Reads a board from text that is already in memory (used by the tests and by
// any future drag and drop). <sourcePath> is only recorded as the origin of the
// data; it is not opened.
KicadImportResult importKicadPcbText(const QString &text, const QString &sourcePath,
                                     QString *error = nullptr);

// File dialog filter for the boards this importer understands.
QString kicadBoardFileFilter();

} // namespace eda
