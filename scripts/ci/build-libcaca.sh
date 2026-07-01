#!/usr/bin/env bash
#
# Build & install libcaca so cibuildwheel can compile + bundle the gtcaca
# extension against it. Runs once per platform in CIBW_BEFORE_ALL.
#
#   * Linux (manylinux_2_28 container, dnf): install ncurses-devel, build a
#     minimal shared libcaca into /usr/local. auditwheel then bundles
#     libcaca.so (+ libncursesw) into the wheel.
#   * macOS: install libcaca via Homebrew (pulls ncurses); delocate bundles it.
#
# gtcaca calls caca_create_display(NULL), which auto-selects a *terminal*
# driver, so libcaca MUST be built with ncurses (or slang) — a raw/null-only
# build would make caca_create_display() return NULL at runtime.
#
# Override the version with CACA_VERSION if needed.
set -euo pipefail

CACA_VERSION="${CACA_VERSION:-0.99.beta20}"

uname_s="$(uname -s)"

if [ "$uname_s" = "Darwin" ]; then
  # ---- macOS: Homebrew is simplest and gives us a driver + pkg-config ----
  brew install libcaca pkg-config
  exit 0
fi

# ---- Linux (manylinux container) ----------------------------------------
# manylinux_2_28 is AlmaLinux 8 (dnf). Install build deps + a terminal driver.
if command -v dnf >/dev/null 2>&1; then
  dnf install -y ncurses-devel gcc make curl tar gzip pkgconfig
elif command -v yum >/dev/null 2>&1; then
  yum install -y ncurses-devel gcc make curl tar gzip pkgconfig
elif command -v apk >/dev/null 2>&1; then
  # musllinux (Alpine) — only reached if you stop skipping musllinux builds.
  apk add --no-cache ncurses-dev gcc make musl-dev curl tar
fi

work="$(mktemp -d)"
cd "$work"

# Fetch the release tarball (ships a ./configure; no autotools needed).
urls=(
  "https://github.com/cacalabs/libcaca/releases/download/v${CACA_VERSION}/libcaca-${CACA_VERSION}.tar.gz"
  "http://libcaca.zoy.org/files/libcaca-${CACA_VERSION}.tar.gz"
  "http://caca.zoy.org/files/libcaca-${CACA_VERSION}.tar.gz"
)
ok=0
for u in "${urls[@]}"; do
  echo "Trying $u"
  if curl -fsSL -o libcaca.tar.gz "$u"; then ok=1; break; fi
done
[ "$ok" = 1 ] || { echo "ERROR: could not download libcaca ${CACA_VERSION}"; exit 1; }

mkdir -p libcaca && tar xzf libcaca.tar.gz -C libcaca --strip-components=1
cd libcaca

# Minimal shared build: keep the ncurses terminal driver, drop everything a
# TUI wheel doesn't need (GUI drivers, bindings, docs, tools).
./configure \
  --prefix=/usr/local \
  --enable-shared --disable-static \
  --enable-ncurses \
  --disable-slang --disable-x11 --disable-gl --disable-imlib2 \
  --disable-doc --disable-cxx --disable-python --disable-ruby \
  --disable-java --disable-csharp --disable-network \
  --disable-cocoa

make -j"$(nproc)"
make install
ldconfig 2>/dev/null || true

echo "libcaca ${CACA_VERSION} installed to /usr/local"
