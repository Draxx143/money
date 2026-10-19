#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUTPUT="$ROOT/dist"
STAGING=$(mktemp -d)
trap 'rm -rf "$STAGING"' EXIT HUP INT TERM
chmod 755 "$STAGING"
mkdir -p "$OUTPUT" "$STAGING/DEBIAN" "$STAGING/usr/bin" "$STAGING/usr/share/applications"
cat > "$STAGING/DEBIAN/control" <<'EOF'
Package: indigo-screen
Version: 1.0.1
Section: graphics
Priority: optional
Architecture: all
Maintainer: Indigo Screen Maintainers <maintainers@example.invalid>
Depends: python3, python3-gi, gir1.2-gtk-3.0
Description: A plain fullscreen indigo display
 Displays only an indigo background, with no text or controls.
 Press Escape to close the application.
EOF
install -m 755 "$ROOT/src/indigo-screen" "$STAGING/usr/bin/indigo-screen"
install -m 644 "$ROOT/packaging/indigo-screen.desktop" "$STAGING/usr/share/applications/indigo-screen.desktop"
dpkg-deb --root-owner-group --build "$STAGING" "$OUTPUT/indigo-screen_1.0.1_all.deb"
