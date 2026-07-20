# gtcaca

Safe, idiomatic Rust bindings for [GTCaca], a libcaca-based terminal UI
toolkit written in C.

This is the high-level crate. It wraps the raw FFI in [`gtcaca-sys`] with widget
handles, builder-style constructors, and closure-based key callbacks. The whole
toolkit is single-threaded (one global context, one blocking event loop), so
the context and every widget are `!Send`/`!Sync`.

## Requirements

libcaca development files, found at build time via `pkg-config`:

| Platform | Install |
| --- | --- |
| Debian/Ubuntu | `apt install libcaca-dev pkg-config` |
| Fedora/RHEL | `dnf install libcaca-devel pkgconf-pkg-config` |
| macOS | `brew install libcaca pkg-config` |

## Example

```rust,no_run
use gtcaca::{key, Application, Button, Entry, Gtcaca, Label, Window};

fn main() -> Result<(), gtcaca::Error> {
    let ctx = Gtcaca::init()?;                 // opens the terminal display

    let app = Application::new(&ctx, "Demo");
    let win = Window::fullscreen(&app, None);

    Label::new(&win, "Name:", 5, 2);
    let entry = Entry::new(&win, 12, 2, 20);
    let quit = Button::new(&win, "Quit", 5, 4);

    quit.on_key(|k| {
        if k == key::RETURN { gtcaca::quit(); true } else { false }
    });

    ctx.run();                                 // blocking loop
    println!("you typed: {}", entry.text());
    Ok(())
}
```

Run the bundled demo in a real terminal:

```sh
cargo run --example hello
```

## Coverage

The safe API currently wraps the core loop and the common widgets
(application, window, label, button, entry, checkbox), and is growing. The
**entire** C API — all ~490 functions — is available today, unsafe, via the
re-exported [`gtcaca_sys`](https://crates.io/crates/gtcaca-sys) FFI:

```rust
use gtcaca::ffi;
unsafe { /* call any gtcaca_* function directly */ }
```

## License

Public Domain (Unlicense), same as GTCaca.

[GTCaca]: https://github.com/stricaud/gtcaca
[`gtcaca-sys`]: https://crates.io/crates/gtcaca-sys
