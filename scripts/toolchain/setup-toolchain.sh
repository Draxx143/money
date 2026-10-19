#!/usr/bin/env bash
#
# setup-toolchain.sh -- provision the C++/Qt 6 toolchain of this project.
#
# What it does, and why
# ---------------------
# The application is a native C++/Qt 6 program.  Building it needs five things:
#
#   1. the Qt 6 shared libraries and plugins   -> runtime
#   2. the Qt 6 public headers                 -> generated from the sources
#   3. moc                                     -> built from the sources
#   4. uic / rcc                               -> shipped by the runtime wheel
#   5. host libraries the Qt runtime links to  -> stubbed, see make-stub-libs.sh
#
# On a workstation with a package manager all of this comes from
# `apt install qt6-base-dev qt6-tools-dev` and the application is configured
# with -DUSE_SYSTEM_QT6=ON.  This sandbox has no usable package manager (every
# distribution mirror is blocked), so the script assembles the very same
# toolchain from sources that are reachable:
#
#   * the *runtime* half of the official Qt 6 binaries that the PySide6 wheels
#     on PyPI carry (the wheels contain no headers, no CMake package files and
#     no moc, but they do contain libQt6*.so, the platform plugins, the SQLite
#     driver, uic and rcc),
#   * the matching qtbase *sources* from the Qt GitHub mirror, which provide the
#     public headers and the moc sources.
#
# Both halves are pinned to the same Qt version and the script refuses to
# continue if they do not match, because headers and libraries must come from
# the same build.
#
# Usage: setup-toolchain.sh [--force]
#
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)

QT_VERSION=6.12.0
PYSIDE_VERSION=6.12.0
QT_BASE_URL=https://codeload.github.com/qt/qtbase/tar.gz/refs/tags/v${QT_VERSION}

CMAKE_VERSION=3.31.6
NINJA_VERSION=1.13.2

EXTERNAL="$ROOT/external"
DOWNLOADS="$EXTERNAL/downloads"
QTSRC="$EXTERNAL/qtbase"
QTINC="$EXTERNAL/qt-headers"
QTRT="$EXTERNAL/qt-runtime"
QTTOOLS="$EXTERNAL/qt-tools"
STUBS="$EXTERNAL/stub-libs"

FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

mkdir -p "$EXTERNAL" "$DOWNLOADS"

# --------------------------------------------------------------------------
# 0. build tools (CMake and Ninja come from PyPI; the sandbox has neither and
#    has no package manager)
# --------------------------------------------------------------------------
if [ ! -x "$EXTERNAL/venv/bin/cmake" ]; then
    echo "provisioning CMake ${CMAKE_VERSION} and Ninja ${NINJA_VERSION}"
    python3 -m venv "$EXTERNAL/venv"
fi
"$EXTERNAL/venv/bin/pip" install -q "cmake==${CMAKE_VERSION}" "ninja==${NINJA_VERSION}"
echo "cmake: $("$EXTERNAL/venv/bin/cmake" --version | head -1)"

# --------------------------------------------------------------------------
# 1. downloads
# --------------------------------------------------------------------------
# pip normalises the file name of a wheel differently from one release to the
# next ("PySide6_Essentials-..." once, "pyside6_essentials-..." later), so the
# wheel is looked up by pattern instead of by exact name.
wheel_path() {
    ls "$DOWNLOADS"/*.whl 2>/dev/null | grep -i "pyside6[_\-]essentials" | head -1
}

fetch_wheel() {
    [ "$FORCE" = 0 ] && [ -n "$(wheel_path)" ] && return 0
    echo "downloading the PySide6 runtime wheel"
    ( cd "$DOWNLOADS" && python3 -m pip download "PySide6-Essentials==${PYSIDE_VERSION}" \
          --no-deps -d . -q )
}

fetch_qtbase() {
    local tarball="qtbase-${QT_VERSION}.tar.gz"
    if [ ! -f "$DOWNLOADS/$tarball" ] || [ "$FORCE" = 1 ]; then
        echo "downloading qtbase ${QT_VERSION}"
        curl -sSL -o "$DOWNLOADS/$tarball" "$QT_BASE_URL"
    fi
    if [ "$FORCE" = 1 ] || [ ! -f "$QTSRC/.cmake.conf" ]; then
        rm -rf "$QTSRC"
        mkdir -p "$QTSRC"
        tar xzf "$DOWNLOADS/$tarball" -C "$QTSRC" --strip-components=1
    fi
}

fetch_wheel
WHEEL=$(wheel_path)
[ -n "$WHEEL" ] || { echo "failed to download the PySide6 runtime wheel" >&2; exit 1; }
echo "runtime wheel: $(basename "$WHEEL")"

fetch_qtbase
echo "qtbase sources: $QTSRC"

# --------------------------------------------------------------------------
# 2. unpack the runtime (libraries, plugins, uic, rcc)
# --------------------------------------------------------------------------
if [ "$FORCE" = 1 ] || [ ! -d "$QTRT/Qt/lib" ]; then
    rm -rf "$QTRT"
    echo "unpacking the Qt runtime"
    python3 - "$WHEEL" "$QTRT" <<'EOF'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
EOF
    # PySide6 keeps the Qt install under PySide6/Qt; lift it up one level.
    if [ -d "$QTRT/PySide6/Qt" ]; then
        mv "$QTRT/PySide6/Qt" "$QTRT/Qt.tmp"
        rm -rf "$QTRT/PySide6"
        mv "$QTRT/Qt.tmp" "$QTRT/Qt"
        rm -rf "$QTRT/PySide6_Essentials.json"
    fi
    # The wheels only carry the versioned .so files, the linker wants the
    # unversioned ones as well.
    for f in "$QTRT"/Qt/lib/*.so.*; do
        short=${f%.[0-9]}
        [ -e "$short" ] || ln -s "$(basename "$f")" "$short"
    done
    chmod +x "$QTRT"/Qt/libexec/* 2>/dev/null || true
fi

# --------------------------------------------------------------------------
# 3. version check -- headers and libraries must come from the same Qt build
# --------------------------------------------------------------------------
libver=$(strings "$QTRT/Qt/lib/libQt6Core.so.6" | grep -E "^6\.[0-9]+\.[0-9]+$" | sort -u | head -1)
srcver=$(sed -n 's/^set(QT_REPO_MODULE_VERSION "\{0,1\}\([0-9.]*\)".*/\1/p' "$QTSRC/.cmake.conf" | head -1)
echo "Qt runtime version: $libver / qtbase source version: $srcver"
if [ "$libver" != "$srcver" ]; then
    echo "ERROR: the Qt runtime ($libver) and the qtbase sources ($srcver) differ." >&2
    echo "       Pin both to the same version in $0." >&2
    exit 1
fi

# --------------------------------------------------------------------------
# 4. headers, moc, stubs
# --------------------------------------------------------------------------
bash "$HERE/generate-qt-headers.sh" "$QTSRC" "$QTINC"
# The stubs come first: linking moc against the prebuilt QtCore needs them, in
# the same way linking the application does.
bash "$HERE/make-stub-libs.sh" "$QTRT/Qt/lib" "$STUBS"
bash "$HERE/build-moc.sh" "$QTSRC" "$QTINC" "$QTRT/Qt/lib" "$QTTOOLS" "$STUBS"

cat > "$EXTERNAL/toolchain.env" <<EOF
# Generated by scripts/toolchain/setup-toolchain.sh -- sourced by scripts/run.sh
export QT_VERSION=$libver
export QT_ROOT=$QTRT/Qt
export QT_INCLUDE_ROOT=$QTINC
export QT_TOOLS_ROOT=$QTTOOLS
export QT_STUB_LIBS=$STUBS
export LD_LIBRARY_PATH="$QTRT/Qt/lib:$STUBS\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$QTRT/Qt/plugins"
export QT_QPA_PLATFORM=\${QT_QPA_PLATFORM:-offscreen}
EOF

echo
echo "Qt $libver toolchain ready:"
echo "  runtime  $QTRT/Qt"
echo "  headers  $QTINC"
echo "  tools    $QTTOOLS"
echo "  stubs    $STUBS"
echo
echo "Source $EXTERNAL/toolchain.env before running the application."
