#!/usr/bin/env bash
#
# Publish one workspace crate to crates.io, idempotently.
#
# Usage: publish-crate.sh <crate-name> [index-wait-seconds]
#
# Run from the cargo workspace root (src/bindings/rust).
set -euo pipefail

CRATE=${1:?usage: publish-crate.sh <crate-name> [index-wait-seconds]}
INDEX_WAIT=${2:-0}

# crates.io rejects requests with a generic/absent User-Agent (403), so the
# probe must identify itself or it looks "unpublished" and we try to re-publish.
UA="gtcaca-ci (https://github.com/${GITHUB_REPOSITORY:-gtcaca})"

VER=$(cargo metadata --no-deps --format-version 1 \
      | python3 -c "import sys,json;print(next(p['version'] for p in json.load(sys.stdin)['packages'] if p['name']=='$CRATE'))")

echo "$CRATE version is $VER"

code=$(curl -sS -o /dev/null -w '%{http_code}' -A "$UA" \
       "https://crates.io/api/v1/crates/$CRATE/$VER" || echo 000)

case "$code" in
  200)
    echo "$CRATE $VER already published — skipping."
    exit 0
    ;;
  404)
    echo "$CRATE $VER not on crates.io — publishing."
    ;;
  *)
    echo "crates.io probe returned HTTP $code; attempting publish anyway."
    ;;
esac

# Belt and braces: if the probe was wrong (rate limit, outage, stale index),
# an "already exists" rejection still means the desired end state holds.
if ! out=$(cargo publish -p "$CRATE" --allow-dirty 2>&1); then
  echo "$out"
  if grep -qi 'already exists on crates.io index' <<<"$out"; then
    echo "$CRATE $VER was already on the index — treating as success."
    exit 0
  fi
  exit 1
fi
echo "$out"

if [ "$INDEX_WAIT" -gt 0 ]; then
  echo "Waiting ${INDEX_WAIT}s for crates.io to index $CRATE $VER…"
  sleep "$INDEX_WAIT"
fi
