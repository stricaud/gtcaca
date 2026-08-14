#!/usr/bin/env bash
#
# Set the Rust workspace version, mirroring what setuptools-scm does for the
# Python wheels: the git tag is the single source of truth for a release.
#
# Usage:
#   set-version.sh              # derive from the current/most recent tag
#   set-version.sh 0.1.28       # set explicitly
#
# Rewrites both version strings in the workspace Cargo.toml:
#   [workspace.package]      version         -> applies to gtcaca-sys + gtcaca
#   [workspace.dependencies] gtcaca-sys      -> the gtcaca -> gtcaca-sys req
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
manifest="$here/../Cargo.toml"

if [ $# -ge 1 ]; then
  raw=$1
else
  # GITHUB_REF_NAME is the tag on a tag push; otherwise use the latest tag so a
  # manual workflow_dispatch or local run still gets a sensible number.
  raw=${GITHUB_REF_NAME:-}
  case "${GITHUB_REF:-}" in
    refs/tags/*) raw=${GITHUB_REF#refs/tags/} ;;
  esac
  if [ -z "$raw" ] || ! git -C "$here" describe --tags --exact-match >/dev/null 2>&1; then
    raw=$(git -C "$here" describe --tags --abbrev=0)
  fi
fi

# Accept v0.1.28 and rust-v0.1.28 alike; crates.io wants the bare semver.
ver=${raw#rust-}
ver=${ver#v}

if ! [[ $ver =~ ^[0-9]+\.[0-9]+\.[0-9]+([-+].*)?$ ]]; then
  echo "set-version.sh: '$raw' does not yield a semver version (got '$ver')" >&2
  exit 1
fi

python3 - "$manifest" "$ver" <<'PY'
import re, sys

path, ver = sys.argv[1], sys.argv[2]
src = open(path).read()

src, n1 = re.subn(r'(?m)^version = "[^"]*"$', f'version = "{ver}"', src, count=1)
src, n2 = re.subn(r'(?m)^(gtcaca-sys = \{ version = )"[^"]*"', rf'\1"{ver}"', src, count=1)

if n1 != 1 or n2 != 1:
    sys.exit(f"set-version.sh: expected 1 match each, got {n1} and {n2} in {path}")

open(path, "w").write(src)
print(f"Rust workspace version set to {ver}")
PY
