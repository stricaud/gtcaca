#!/usr/bin/env sh
# Regenerate the committed FFI bindings that docs.rs (which has no libcaca) uses.
#
# gtcaca-sys/build.rs runs bindgen normally, but on docs.rs it copies
# gtcaca-sys/docsrs-bindings.rs instead. Re-run this after any change to the C
# API (headers / wrapper.h) so the two stay in sync. Needs libcaca installed.
set -e
cd "$(dirname "$0")/.."

# Force a fresh bindgen run and grab the freshest bindings.rs.
touch gtcaca-sys/build.rs
cargo build -p gtcaca-sys >/dev/null
newest=$(ls -t $(find target -name bindings.rs) | head -1)
cp "$newest" gtcaca-sys/docsrs-bindings.rs
echo "updated gtcaca-sys/docsrs-bindings.rs from $newest"

# Sanity: it must carry no platform-specific stdio types (they'd break the
# reused copy on docs.rs's Linux target).
if grep -qE '__darwin|__sFILE|_IO_FILE|fpos_t' gtcaca-sys/docsrs-bindings.rs; then
  echo "ERROR: platform-specific types leaked into docsrs-bindings.rs" >&2
  exit 1
fi
echo "ok: bindings are target-independent"
