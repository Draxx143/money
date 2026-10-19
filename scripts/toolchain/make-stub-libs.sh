#!/usr/bin/env bash
#
# make-stub-libs.sh -- create placeholder shared libraries for host libraries
# that the prebuilt Qt runtime needs but that are not installed here.
#
# Why this exists
# ---------------
# The Qt 6 runtime used by this project is the one shipped inside the PySide6
# manylinux wheels.  manylinux wheels are not allowed to bundle a number of
# system libraries (libGL, libEGL, libX11, libxcb*, libxkbcommon, libdbus-1
# ...), they rely on the distribution to provide them.  A minimal container -
# such as this build sandbox - has none of them installed and has no working
# package manager, so neither the linker nor the dynamic loader can work with
# libQt6Gui.so (it has DT_NEEDED entries for libGL.so.1, libEGL.so.1,
# libxkbcommon.so.0 and libdbus-1.so.3).
#
# This script generates stub shared libraries that export the symbols the Qt
# libraries actually reference, with the same symbol versions.  They make it
# possible to *link* and to *run the application head less*
# (QT_QPA_PLATFORM=offscreen/minimal), which is what the automated test suite
# does.  They are a build-environment shim only:
#
#   * they are never packaged, never installed and never shipped in the .deb
#   * on a normal desktop the real libraries come from the distribution and the
#     .deb declares them as dependencies
#   * no stub function is ever called: the offscreen platform plugin does not
#     use OpenGL, EGL, X11 keyboard handling or D-Bus
#
# Usage: make-stub-libs.sh <qt-runtime-lib-dir> <output-dir>
#
set -euo pipefail

LIBDIR=${1:?usage: $0 <qt-runtime-lib-dir> <output-dir>}
OUT=${2:?usage: $0 <qt-runtime-lib-dir> <output-dir>}

command -v nm >/dev/null || { echo "binutils (nm) is required" >&2; exit 1; }

mkdir -p "$OUT"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# --------------------------------------------------------------------------
# 1. which libraries are missing?
# --------------------------------------------------------------------------
# Only the libraries the Qt *core* libraries themselves need are stubbed.
# Optional plugin dependencies (CUPS, the SQL client libraries, GTK, the XCB
# extension libraries used by the xcb platform plugin, ...) are not: the
# automated tests run with the offscreen platform plugin and never load them.
WANTED="libGL.so.1 libEGL.so.1 libxkbcommon.so.0 libdbus-1.so.3 libxkbcommon-x11.so.0 libX11-xcb.so.1"

missing=$(for f in "$LIBDIR"/*.so*; do
              [ -f "$f" ] || continue
              LD_LIBRARY_PATH="$LIBDIR" ldd "$f" 2>/dev/null || true
          done | grep 'not found' | sed -e 's/^[[:space:]]*//' -e 's/ =>.*//' | sort -u)
missing=$(echo "$missing" | tr ' ' '\n' | grep -xF -f <(echo "$WANTED" | tr ' ' '\n') || true)

if [ -z "$missing" ]; then
    echo "no missing host libraries, nothing to do"
    exit 0
fi
echo "missing host libraries:" $missing

# --------------------------------------------------------------------------
# 2. symbol prefix and soname for every stub we know how to build
# --------------------------------------------------------------------------
prefix_for() {
    case "$1" in
        libGL.so.1)            echo '^(gl[A-Z]|glX|GL_)' ;;
        libEGL.so.1)           echo '^(egl[A-Z]|EGL_)' ;;
        libxkbcommon.so.0)     echo '^xkb_' ;;
        libxkbcommon-x11.so.0) echo '^xkb_x11_' ;;
        libdbus-1.so.3)        echo '^dbus_' ;;
        libX11-xcb.so.1)       echo '^XGetXCBConnection$' ;;
        libxcb-cursor.so.0)    echo '^xcb_cursor_' ;;
        libxcb-icccm.so.4)     echo '^xcb_icccm_' ;;
        libxcb-image.so.0)     echo '^xcb_image_' ;;
        libxcb-keysyms.so.1)   echo '^xcb_key_symbols_' ;;
        libxcb-randr.so.0)     echo '^xcb_randr_' ;;
        libxcb-render-util.so.0) echo '^xcb_render_util_' ;;
        libxcb-shm.so.0)       echo '^xcb_shm_' ;;
        libxcb-sync.so.1)      echo '^xcb_sync_' ;;
        libxcb-util.so.1)      echo '^xcb_aux_' ;;
        libxcb-xfixes.so.0)    echo '^xcb_xfixes_' ;;
        libxcb-xinerama.so.0)  echo '^xcb_xinerama_' ;;
        libxcb-xkb.so.1)       echo '^xcb_xkb_' ;;
        *)                     echo '' ;;
    esac
}

# --------------------------------------------------------------------------
# 3. every undefined symbol of the Qt runtime, so that the stubs can provide
#    exactly those
# --------------------------------------------------------------------------
: > "$WORK/undefined.syms"
for f in "$LIBDIR"/*.so* "$LIBDIR"/../plugins/*/*.so; do
    [ -f "$f" ] || continue
    nm -D --undefined-only "$f" 2>/dev/null | awk '{print $2}' >> "$WORK/undefined.syms" || true
done
sort -u "$WORK/undefined.syms" -o "$WORK/undefined.syms"
echo "undefined symbols in the Qt runtime: $(wc -l < "$WORK/undefined.syms")"

for lib in $missing; do
    prefix=$(prefix_for "$lib")
    if [ -z "$prefix" ]; then
        echo "  SKIP $lib (no symbol prefix known -- add one to $0)"
        continue
    fi
    syms=$(grep -E "$prefix" "$WORK/undefined.syms" || true)
    if [ -z "$syms" ]; then
        echo "  SKIP $lib (no referenced symbols found)"
        continue
    fi

    # symbol name -> version map
    : > "$WORK/versions.txt"
    : > "$WORK/stub.c"
    echo "$syms" | while IFS= read -r s; do
        case "$s" in
            *@*) name=${s%@*}; ver=${s#*@} ;;
            *)   name=$s;      ver= ;;
        esac
        printf 'void %s(void) {}\n' "$name" >> "$WORK/stub.c"
        if [ -n "$ver" ]; then
            echo "$ver" >> "$WORK/versions.txt"
        fi
    done

    {
        echo "{"
        echo "  global:"
        # unversioned references need a matching unversioned definition; the
        # wildcard covers every symbol of this stub
        echo "    *;"
        echo "  local: *;"
        echo "};"
    } > "$WORK/$lib.map"

    # one version node per symbol version the runtime asks for
    if [ -s "$WORK/versions.txt" ]; then
        : > "$WORK/$lib.map"
        for v in $(sort -u "$WORK/versions.txt"); do
            printf '%s {\n  global: *;\n};\n' "$v" >> "$WORK/$lib.map"
        done
    fi

    gcc -shared -fPIC -O0 -o "$OUT/$lib" "$WORK/stub.c" \
        -Wl,-soname,"$lib" -Wl,--version-script,"$WORK/$lib.map"
    echo "  built stub $lib ($(grep -c 'void ' "$WORK/stub.c") symbols)"
done

echo "stub libraries written to $OUT"
