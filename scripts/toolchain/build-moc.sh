#!/usr/bin/env bash
#
# build-moc.sh -- build Qt's Meta Object Compiler from the qtbase sources.
#
# moc is the only Qt build tool that the PySide6 runtime wheels do not ship
# (they only carry uic, rcc and the QML tooling), and no moc binary of a
# matching Qt version is available in this environment.  moc itself only needs
# the public and private QtCore headers plus a handful of helper headers from
# the qtbase source tree, so it can be compiled against the prebuilt QtCore.
#
# Usage: build-moc.sh <qtbase-source-dir> <qt-include-dir> <qt-lib-dir> <output-dir> [stub-lib-dir]
#
set -euo pipefail

QTSRC=${1:?usage: $0 <qtbase-src> <qt-include-dir> <qt-lib-dir> <output-dir>}
QTINC=${2:?usage: $0 <qtbase-src> <qt-include-dir> <qt-lib-dir> <output-dir>}
QTLIB=${3:?usage: $0 <qtbase-src> <qt-include-dir> <qt-lib-dir> <output-dir>}
OUT=${4:?usage: $0 <qtbase-src> <qt-include-dir> <qt-lib-dir> <output-dir> [stub-lib-dir]}
# The stub libraries are optional: they only exist in containers without the
# host libraries the prebuilt Qt runtime links to.
STUBS=${5:-}

CXX=${CXX:-g++}
command -v "$CXX" >/dev/null || { echo "no C++ compiler ($CXX)" >&2; exit 1; }

mkdir -p "$OUT"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# The sources and the definitions are exactly the ones of
# src/tools/moc/CMakeLists.txt plus QT_USE_QSTRINGBUILDER, which Qt adds
# globally to TOOL scope (cmake/QtInternalTargets.cmake).
SOURCES="collectjson.cpp generator.cpp main.cpp moc.cpp parser.cpp preprocessor.cpp token.cpp"

DEFS="-DQT_MOC -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_QPAIR
      -DQT_NO_URL_CAST_FROM_STRING -DQT_USE_NODISCARD_FILE_OPEN -DQT_USE_QSTRINGBUILDER"

INCS="-I$QTINC -I$QTINC/QtCore -I$QTINC/QtCore/private -I$QTINC/QtGui/private
      -I$QTSRC/src/tools/moc -I$QTSRC/src/tools/shared -I$QTSRC/src/3rdparty/tinycbor/src"

echo "building moc from $QTSRC"
for f in $SOURCES; do
    printf '  CC      %s\n' "$f"
    # shellcheck disable=SC2086
    ( cd "$QTSRC/src/tools/moc" && "$CXX" -std=c++20 -O2 -DQT_SHARED -DQT_NO_DEBUG \
        $DEFS $INCS -c "$f" -o "$WORK/${f%.cpp}.o" )
done

printf '  LINK    moc\n'
LINK_DIRS="-L$QTLIB"
RPATH="-Wl,-rpath,$QTLIB"
if [ -d "$STUBS" ]; then
    LINK_DIRS="$LINK_DIRS -L$STUBS"
    RPATH="$RPATH,-rpath,$STUBS"
fi
# shellcheck disable=SC2086
"$CXX" $WORK/*.o -o "$OUT/moc" $LINK_DIRS -lQt6Core $RPATH
chmod +x "$OUT/moc"

"$OUT/moc" --help >/dev/null 2>&1
echo "moc built: $OUT/moc"
