#!/usr/bin/env bash
#
# package-deb.sh -- build the Debian package into dist/.
#
# The package installs to /usr (what a Debian package is expected to do), so it
# is configured in its own build directory; the prefix is the only difference to
# the normal development build.
#
# Usage: package-deb.sh [--system-qt] [--build-dir DIR]
#
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

BUILD_DIR="$ROOT/build-deb"
QT_OPTION=(-DUSE_SYSTEM_QT6=OFF)

while [ $# -gt 0 ]; do
    case "$1" in
        --system-qt) QT_OPTION=(-DUSE_SYSTEM_QT6=ON) ;;
        --build-dir) shift; BUILD_DIR="$1" ;;
        -h|--help)   sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

CMAKE=${CMAKE:-cmake}
command -v "$CMAKE" >/dev/null || CMAKE="$ROOT/external/venv/bin/cmake"
command -v "$CMAKE" >/dev/null || { echo "no cmake found (set \$CMAKE)" >&2; exit 1; }
export PATH="$(dirname "$CMAKE"):$PATH"

echo "configuring the package build in $BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$ROOT" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DBUILD_TESTING=ON \
      "${QT_OPTION[@]}"

echo "compiling"
cmake --build "$BUILD_DIR"

echo "running the test suite before packaging"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "building the package"
cmake --build "$BUILD_DIR" --target package

ls -lh "$ROOT"/dist/*.deb
