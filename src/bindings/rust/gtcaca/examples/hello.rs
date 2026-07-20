//! Minimal GTCaca app: a window with a label, entry, checkbox and button.
//!
//! Run in a real terminal: `cargo run --example hello`
//! Press Enter on "Quit" to exit.

use gtcaca::{key, Application, Button, Checkbox, Entry, Gtcaca, Label, Window};

fn main() -> Result<(), gtcaca::Error> {
    let ctx = Gtcaca::init()?;

    let app = Application::new(&ctx, "GTCaca Rust demo");
    let win = Window::fullscreen(&app, None);

    let _title = Label::new(&win, "Hello from Rust!", 5, 2);
    let _name = Label::new(&win, "Name:", 5, 4);
    let entry = Entry::new(&win, 11, 4, 20);
    let check = Checkbox::new(&win, "I agree", 5, 6);
    let quit = Button::new(&win, "Quit", 5, 8);

    quit.on_key(|k| {
        if k == key::RETURN {
            gtcaca::quit();
            return true;
        }
        false
    });

    ctx.run();

    println!("entry was: {:?}, checked: {}", entry.text(), check.is_checked());
    Ok(())
}
