#!/bin/sh
#
# Run a gtcaca demo against the freshly-built LOCAL library, not the (possibly
# stale) system one in /usr/local/lib — the same fix as runccm.sh.
#
# Usage:  ./demo.sh <name> [args…]     e.g.  ./demo.sh gallery   ./demo.sh tabs
#         BUILD=/path/to/build ./demo.sh …   to point at a different build dir
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BUILD:-$HERE/build}"

list() {
  echo "available demos:" >&2
  ls "$BUILD/demo" 2>/dev/null | grep -E '^demo_' | sed 's/^demo_/  /' >&2
}

name="$1"
if [ -z "$name" ]; then
  echo "usage: ./demo.sh <name> [args…]   (runs build/demo/demo_<name> with the fresh lib)" >&2
  list; exit 1
fi
shift

bin="$BUILD/demo/demo_$name"
[ -x "$bin" ] || bin="$BUILD/demo/$name"      # also accept the full "demo_xxx" name
if [ ! -x "$bin" ]; then
  echo "demo.sh: no demo '$name' in $BUILD/demo (build it first?)" >&2
  list; exit 1
fi

# build/src first so the fresh libgtcaca wins; keep any existing path after it
exec env DYLD_LIBRARY_PATH="$BUILD/src${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" "$bin" "$@"
