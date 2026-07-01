#!/usr/bin/env bash
#
# Build libcaca for Windows x86_64 using the MSYS2 mingw-w64 toolchain, then
# produce an MSVC-linkable import library (caca.lib) plus the runtime DLLs.
# This lets cibuildwheel's MSVC build link the extension and lets delvewheel
# bundle the DLLs into the wheel.
#
# Run in the MSYS2 MINGW64 shell (shell: 'msys2 {0}'). Installs into
# $CACA_ROOT (default ./caca-dist under the repo). libcaca's own Windows recipe
# (build/build-win64) uses the same x86_64-w64-mingw32 toolchain.
#
# NOTE: unverified end-to-end — expect to iterate on CI logs. The likeliest
# friction points are the import-lib generation and which driver libcaca's
# caca_create_display() ends up using on Windows.
set -euo pipefail

CACA_VERSION="${CACA_VERSION:-0.99.beta20}"
DIST_WIN="${CACA_ROOT:-$(pwd)/caca-dist}"
DIST="$(cygpath -u "$DIST_WIN" 2>/dev/null || echo "$DIST_WIN")"
mkdir -p "$DIST"

work="$(mktemp -d)"; cd "$work"
urls=(
  "https://github.com/cacalabs/libcaca/releases/download/v${CACA_VERSION}/libcaca-${CACA_VERSION}.tar.gz"
  "http://libcaca.zoy.org/files/libcaca-${CACA_VERSION}.tar.gz"
)
ok=0; for u in "${urls[@]}"; do echo "Trying $u"; curl -fsSL -o c.tar.gz "$u" && { ok=1; break; }; done
[ "$ok" = 1 ] || { echo "ERROR: could not download libcaca ${CACA_VERSION}"; exit 1; }
mkdir srcdir && tar xzf c.tar.gz -C srcdir --strip-components=1
cd srcdir

# Native mingw build (in MINGW64 the compiler IS x86_64-w64-mingw32-gcc).
./configure --prefix="$DIST" \
  --enable-shared --disable-static \
  --disable-imlib2 --disable-java --disable-doc --disable-cxx \
  --disable-python --disable-ruby --disable-csharp \
  --disable-gl --disable-x11 --disable-network
make -j"$(nproc)"
make install

# Locate the installed DLL (libtool/mingw may name it libcaca-0.dll).
dll="$(ls "$DIST"/bin/*caca*.dll 2>/dev/null | head -n1)"
[ -n "$dll" ] || { echo "ERROR: no libcaca DLL produced"; ls -R "$DIST"; exit 1; }
echo "libcaca DLL: $dll"

# Create an MSVC-consumable import library: caca.lib (find_library(NAMES caca)).
mkdir -p "$DIST/lib"
( cd "$DIST/lib"
  gendef "$dll" >/dev/null 2>&1 || gendef - "$dll" || true
  def="$(ls ./*.def 2>/dev/null | head -n1)"
  [ -n "$def" ] || { echo "ERROR: gendef produced no .def"; exit 1; }
  dlltool --input-def "$def" --dllname "$(basename "$dll")" --output-lib caca.lib
  echo "created $(pwd)/caca.lib from $def" )

# Copy transitive mingw runtime DLLs next to caca.dll so delvewheel bundles them.
copydeps() {
  local f="$1" dep base
  ( ldd "$f" 2>/dev/null || true ) | awk '/[Mm]ingw64/ {print $3}' | while read -r dep; do
    [ -f "$dep" ] || continue
    base="$(basename "$dep")"
    if [ ! -f "$DIST/bin/$base" ]; then
      cp "$dep" "$DIST/bin/"
      copydeps "$DIST/bin/$base"
    fi
  done
}
copydeps "$dll"

echo "== $DIST/bin =="; ls "$DIST/bin"
echo "== $DIST/lib =="; ls "$DIST/lib"
