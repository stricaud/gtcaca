# GTCaca Rust bindings

Rust bindings for [GTCaca](https://github.com/stricaud/gtcaca), a libcaca-based
terminal UI toolkit. Two crates, the usual `-sys` split:

| Crate | What it is |
| --- | --- |
| [`gtcaca-sys`](gtcaca-sys/) | Raw FFI (bindgen) + builds & links the C library. `unsafe`. |
| [`gtcaca`](gtcaca/) | Safe, idiomatic wrappers on top. Start here. |

The full C API (~490 functions) is available immediately through `gtcaca-sys`;
the safe `gtcaca` crate wraps the core loop + common widgets and grows from
there.

## Build requirements

libcaca dev files, found via `pkg-config`:
`apt install libcaca-dev` · `dnf install libcaca-devel` · `brew install libcaca`.

## Develop

```sh
cd src/bindings/rust
cargo build                 # builds against the in-repo C sources (../../../src)
cargo run --example hello   # in a real terminal
cargo run --example smoke   # non-interactive round-trip check
```

`build.rs` compiles the C sources straight from the repo tree during
development. The C source list it compiles mirrors `GTCACA_C_SOURCES` in
`src/bindings/python/CMakeLists.txt` and must be kept in sync when a widget is
added (also add the new header to `gtcaca-sys/wrapper.h`).

## Publish to crates.io

The C sources live outside the crate dirs, so they are vendored into
`gtcaca-sys/vendor` first (crates.io tarballs can't reference outside files).
`build.rs` prefers `vendor/` when present, so this only affects packaging.

```sh
cd src/bindings/rust
bash scripts/sync-sources.sh            # copy src/*.c + headers into gtcaca-sys/vendor
cargo package -p gtcaca-sys --allow-dirty   # sanity: builds in isolation
cargo publish  -p gtcaca-sys            # publish the -sys crate first
cargo publish  -p gtcaca                # then the safe crate
```

Publish `gtcaca-sys` before `gtcaca` (the latter depends on the published
version). Bump versions in the workspace `Cargo.toml` (`[workspace.package]`)
before each release.
