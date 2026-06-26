#!/bin/bash
#
# Build a SELF-CONTAINED macOS installer package for cacamacs.
#
# The resulting cacamacs-<ver>.pkg installs, under /usr/local:
#   bin/ccm                       the editor
#   lib/libgtcaca.*.dylib         the toolkit
#   lib/libcaca.0.dylib           bundled (so Homebrew is NOT required)
#   share/gtcaca/themes/          the default toolkit theme
#   share/gtcaca/ccm-theme.example   the cacamacs colour-theme sample
#
# A postinstall script then seeds ~/.ccm/theme for the installing user from that
# sample (only if they don't already have one, so it never clobbers).
#
# libcaca's own dependencies (ncurses, zlib) live in /usr/lib on every macOS,
# so nothing else is needed. After installing, just run `ccm` in a terminal.
#
# Usage:  scripts/package-cacamacs-macos.sh [build-dir]
set -euo pipefail
export COPYFILE_DISABLE=1          # don't copy ._ AppleDouble metadata files

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
VERSION="$(awk -F'"' '/set\(GTCACA_VERSION/{print $2}' "$ROOT/CMakeLists.txt")"
PREFIX="/usr/local"
STAGE="$(mktemp -d)"
DEST="$STAGE$PREFIX"
OUT="$ROOT/cacamacs-$VERSION.pkg"

echo ">> building cacamacs + libgtcaca"
cmake --build "$BUILD" --target cacamacs gtcaca >/dev/null

echo ">> staging into $DEST"
mkdir -p "$DEST/bin" "$DEST/lib" "$DEST/share/gtcaca/themes"
cp "$BUILD/apps/ccm"                      "$DEST/bin/"
cp -a "$BUILD"/src/libgtcaca.*.dylib      "$DEST/lib/"
cp "$ROOT/themes/default"                 "$DEST/share/gtcaca/themes/"
cp "$ROOT/docs/ccm-theme.example"         "$DEST/share/gtcaca/ccm-theme.example"

# resolve and bundle libcaca (the only non-system dependency)
LIBCACA="$(otool -L "$BUILD/apps/ccm" | awk '/libcaca/{print $1; exit}')"
LIBCACA_REAL="$(cd "$(dirname "$LIBCACA")" && pwd)/$(basename "$LIBCACA")"
echo ">> bundling $LIBCACA_REAL"
cp "$LIBCACA_REAL" "$DEST/lib/libcaca.0.dylib"
chmod -R u+w "$DEST/lib"

# repoint everything at @rpath so it resolves inside the install tree
install_name_tool -id @rpath/libcaca.0.dylib "$DEST/lib/libcaca.0.dylib"
GLIB="$(ls "$DEST"/lib/libgtcaca.*.dylib | grep -E 'libgtcaca\.[0-9]+\.dylib' | head -1)"
GCACA="$(otool -L "$GLIB" | awk '/libcaca/{print $1; exit}')"
[ -n "${GCACA:-}" ] && install_name_tool -change "$GCACA" @rpath/libcaca.0.dylib "$GLIB" || true
install_name_tool -change "$LIBCACA" @rpath/libcaca.0.dylib "$DEST/bin/ccm"
# make sure ccm can find ../lib via rpath (it already does once installed;
# add it defensively in case the binary came straight from the build tree)
install_name_tool -add_rpath @executable_path/../lib "$DEST/bin/ccm" 2>/dev/null || true

echo ">> verifying the staged binary resolves only @rpath + system libs"
otool -L "$DEST/bin/ccm" | sed 's/^/   /'

# postinstall: seed ~/.ccm/theme for the installing user (never clobber)
SCRIPTS="$(mktemp -d)"
cat > "$SCRIPTS/postinstall" <<'POST'
#!/bin/sh
# Give the installing user a starter ~/.ccm/theme from the bundled sample, but
# only if they don't already have one. Runs as root, so target the real user.
set -e
EXAMPLE="/usr/local/share/gtcaca/ccm-theme.example"

user="$(stat -f%Su /dev/console 2>/dev/null || true)"
if [ -z "$user" ] || [ "$user" = "root" ]; then user="${SUDO_USER:-${USER:-}}"; fi
[ -n "$user" ] && [ "$user" != "root" ] || exit 0

home="$(dscl . -read "/Users/$user" NFSHomeDirectory 2>/dev/null | awk '{print $2}')"
[ -n "$home" ] || home="$(eval echo "~$user")"
[ -n "$home" ] && [ -d "$home" ] || exit 0

theme="$home/.ccm/theme"
if [ ! -e "$theme" ] && [ -f "$EXAMPLE" ]; then
  mkdir -p "$home/.ccm"
  cp "$EXAMPLE" "$theme"
  chown "$user" "$home/.ccm" "$theme" 2>/dev/null || true
fi
exit 0
POST
chmod +x "$SCRIPTS/postinstall"

echo ">> building $OUT"
pkgbuild --root "$STAGE" --install-location / --scripts "$SCRIPTS" \
         --identifier org.gtcaca.cacamacs --version "$VERSION" "$OUT" >/dev/null

rm -rf "$STAGE" "$SCRIPTS"
echo ">> done: $OUT"
echo "   install it (double-click or: sudo installer -pkg \"$OUT\" -target /), then run: ccm"
