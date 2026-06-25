#!/bin/sh
#
# Run the freshly-built ccm against the LOCAL build library, not the (possibly
# stale) system one in /usr/local/lib.
#
# dyld would otherwise pick up /usr/local/lib/libgtcaca.1.dylib (left there by a
# previous `make install` / .pkg) before the build's own rpath, which leads to
# missing-symbol crashes and old behaviour after you rebuild. Putting the build
# dir first on DYLD_LIBRARY_PATH forces the fresh library.
#
# Usage:  ./runccm.sh [args…]      e.g.  ./runccm.sh .    ./runccm.sh file.c    ./runccm.sh --test
#         BUILD=/path/to/build ./runccm.sh …    to point at a different build dir
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BUILD:-$HERE/build}"

if [ ! -x "$BUILD/apps/ccm" ]; then
  echo "runccm.sh: $BUILD/apps/ccm not found — build first:  cmake --build \"$BUILD\"" >&2
  exit 1
fi

# build/src first so the fresh libgtcaca wins; keep any existing path after it
exec env DYLD_LIBRARY_PATH="$BUILD/src${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" \
     "$BUILD/apps/ccm" "$@"
