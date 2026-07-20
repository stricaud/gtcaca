#!/usr/bin/env bash
#
# Vendor the GTCaca C sources into gtcaca-sys/vendor so the -sys crate is
# self-contained for `cargo publish` (crates.io tarballs cannot reference files
# outside the crate directory). build.rs prefers vendor/ when present and falls
# back to the in-repo ../../../src for local development.
#
# Run this before `cargo publish -p gtcaca-sys`.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
rust_root="$(cd "$here/.." && pwd)"
repo_root="$(cd "$rust_root/../../.." && pwd)"
vendor="$rust_root/gtcaca-sys/vendor"

echo "repo root : $repo_root"
echo "vendor    : $vendor"

rm -rf "$vendor"
mkdir -p "$vendor/src"

# Copy the C translation units + all public headers. We copy the whole src/*.c
# set and src/include tree; build.rs selects the exact TU list it compiles.
cp "$repo_root"/src/*.c "$vendor/src/"
cp -R "$repo_root/src/include" "$vendor/src/include"

n_c=$(find "$vendor/src" -maxdepth 1 -name '*.c' | wc -l | tr -d ' ')
n_h=$(find "$vendor/src/include" -name '*.h' | wc -l | tr -d ' ')
echo "vendored $n_c .c files and $n_h headers"
echo "done. now: cargo publish -p gtcaca-sys   (from $rust_root)"
