//! Non-interactive smoke test: init, build widgets, read state, exit without
//! entering the blocking loop. Prints a line proving the FFI round-trips.
use gtcaca::{Application, Button, Checkbox, Entry, Gtcaca, Label, Widget, Window};

fn main() {
    let ctx = match Gtcaca::init() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("init failed: {e}");
            std::process::exit(2);
        }
    };
    let app = Application::new(&ctx, "smoke");
    let win = Window::fullscreen(&app, Some("Smoke"));
    let _l = Label::new(&win, "hi", 1, 1);
    let e = Entry::new(&win, 1, 2, 10);
    e.set_text("abc");
    let cb = Checkbox::new(&win, "x", 1, 3);
    cb.set_checked(true);
    let b = Button::new(&win, "ok", 1, 4);
    b.on_key(|_k| true);
    println!(
        "SMOKE_OK app={:?} entry={:?} checked={}",
        app.geometry(),
        e.text(),
        cb.is_checked()
    );
}
