# gtcaca-sys

Raw FFI bindings to [GTCaca](https://github.com/stricaud/gtcaca), a
libcaca-based terminal UI toolkit written in C.

This is the low-level `-sys` crate: it links the GTCaca C sources and exposes
the whole C API verbatim via [bindgen]. Everything here is `unsafe`. For a
safe, idiomatic Rust interface, use the [`gtcaca`] crate instead.

## Requirements

libcaca development files must be installed; the build finds them via
`pkg-config`:

| Platform | Install |
| --- | --- |
| Debian/Ubuntu | `apt install libcaca-dev pkg-config` |
| Fedora/RHEL | `dnf install libcaca-devel pkgconf-pkg-config` |
| macOS | `brew install libcaca pkg-config` |

## License

Public Domain (Unlicense), same as GTCaca.

[bindgen]: https://github.com/rust-lang/rust-bindgen
[`gtcaca`]: https://crates.io/crates/gtcaca
