//! Widget gallery — browse and drive every widget the Rust bindings wrap.
//!
//! Run in a real terminal:  `cargo run --example gallery`
//!
//!   Up/Down   pick a widget          Tab / Enter   use the selected widget
//!   /         search the list        Tab / Esc     back to the list
//!   q         quit (from the list)
//!
//! It mirrors the C `demo_gallery`, but shows the idiomatic Rust shape: every
//! widget is a `Box<dyn Widget>`, so one loop can `paint`, `send_key` and
//! `set_focus` them all uniformly — no per-type switching.

use std::ffi::c_void;

use gtcaca::{
    color, key, Application, Barchart, Button, Calendar, Checkbox, Combobox, Dialog, Editor, Entry,
    Expander, Filechooser, Frame, Gauge, Gtcaca, Hexview, Label, Linechart, Map, Mindmap, Piechart,
    Progressbar, Radiobutton, Scale, Scatter, Segdisplay, Separator, Sparkline, Spinbutton, Switch,
    Table, TableModel, Tabs, Textlist, Textview, Tree, TreeModel, Widget, Window,
};

// ── demo models for the Tree and Table ──────────────────────────────────────

/// A synthetic 3-level tree. Each node handle packs its depth and ordinal, so
/// the model is stateless (same trick as the C demo).
struct DemoTree;
fn depth(n: *mut c_void) -> usize {
    (n as usize) >> 56
}
fn ord(n: *mut c_void) -> usize {
    (n as usize) & 0x00FF_FFFF_FFFF_FFFF
}
fn node(d: usize, o: usize) -> *mut c_void {
    ((d << 56) | o) as *mut c_void
}
impl TreeModel for DemoTree {
    fn child_count(&self, n: *mut c_void) -> i64 {
        if depth(n) < 3 { 4 } else { 0 }
    }
    fn child(&self, n: *mut c_void, i: i64) -> *mut c_void {
        node(depth(n) + 1, ord(n) * 4 + i as usize)
    }
    fn has_children(&self, n: *mut c_void) -> bool {
        depth(n) < 3
    }
    fn label(&self, n: *mut c_void) -> String {
        let kind = ["Root", "Folder", "Subfolder", "File"][depth(n).min(3)];
        format!("{kind} {}", ord(n))
    }
}

struct DemoTable;
impl TableModel for DemoTable {
    fn row_count(&self) -> i64 {
        5
    }
    fn headers(&self) -> Vec<String> {
        vec!["Name".into(), "Role".into(), "Score".into()]
    }
    fn cell(&self, row: i64, col: i32) -> String {
        let names = ["Ada", "Linus", "Grace", "Ken", "Margaret"];
        let roles = ["Eng", "Kernel", "Navy", "Unix", "Apollo"];
        let scores = [98, 95, 99, 91, 97];
        let r = row as usize;
        match col {
            0 => names[r].into(),
            1 => roles[r].into(),
            _ => scores[r].to_string(),
        }
    }
}

/// One gallery page: a label, a one-line description, and the live widget.
struct Page<'a> {
    name: &'static str,
    desc: &'static str,
    widget: Box<dyn Widget + 'a>,
}

fn main() -> Result<(), gtcaca::Error> {
    let ctx = Gtcaca::init()?;
    let app = Application::new(&ctx, "gtcaca gallery");
    let win = Window::fullscreen(&app, None);

    // Layout: a menu column on the left, the live widget on the right.
    let g = win.geometry();
    let list_w = 20;
    let (rx, ry) = (list_w + 2, 2);
    let rw = (g.width - rx - 2).max(10);
    let rh = (g.height - ry - 2).max(6);
    let clamp = |w: i32, h: i32| (w.min(rw), h.min(rh));

    // Keep any borrowed buffers alive for the whole run (the hex view borrows).
    let hexbuf: Vec<u8> = (0u16..=255).map(|b| b as u8).collect();

    // Build every widget once, at the same spot on the right; we only paint the
    // selected one. Each is configured on its concrete type, then boxed as a
    // uniform `dyn Widget`.
    let mut pages: Vec<Page> = Vec::new();
    let mut add = |name, desc, w: Box<dyn Widget>| pages.push(Page { name, desc, widget: w });

    add("Label", "static text", Box::new(Label::new(&win, "Hello, gtcaca!", rx, ry)));
    add("Button", "push button", Box::new(Button::new(&win, "OK", rx, ry)));
    {
        let e = Entry::new(&win, rx, ry, 24.min(rw));
        e.set_text("type here");
        add("Entry", "text input — type!", Box::new(e));
    }
    {
        let c = Checkbox::new(&win, "Enable sound (Space)", rx, ry);
        add("Checkbox", "Space toggles", Box::new(c));
    }
    {
        let r = Radiobutton::new(&win, "Small (Space)", 1, rx, ry);
        r.set_active();
        add("Radio button", "Space selects", Box::new(r));
    }
    {
        let c = Combobox::new(&win, rx, ry, 22.min(rw));
        for s in ["C", "Rust", "Go"] {
            c.append(s);
        }
        add("Combo box", "Up/Down/Enter", Box::new(c));
    }
    {
        let s = Switch::new(&win, rx, ry);
        s.set_active(true);
        add("Switch", "Space/Enter toggles", Box::new(s));
    }
    {
        let s = Scale::new(&win, rx, ry, 34.min(rw), 0.0, 100.0, 5.0);
        s.set_value(60.0);
        add("Scale", "Left/Right adjust", Box::new(s));
    }
    {
        let s = Spinbutton::new(&win, rx, ry, 0.0, 100.0, 1.0);
        s.set_value(42.0);
        add("Spin button", "Left/Right adjust", Box::new(s));
    }
    {
        let p = Progressbar::new(&win, rx, ry, 34.min(rw));
        p.set_value(0.62);
        add("Progress bar", "determinate", Box::new(p));
    }
    {
        let (w, h) = clamp(34, 8);
        let t = Textview::new(&win, rx, ry, w, h);
        for i in 1..=12 {
            t.append(&format!("line {i} — scroll me"));
        }
        add("Text view", "Up/Down/PgDn scroll", Box::new(t));
    }
    {
        let (w, h) = clamp(30, 6);
        add("Frame", "grouping box", Box::new(Frame::new(&win, "Account", rx, ry, w, h)));
    }
    add("Separator", "divider rule", Box::new(Separator::new(&win, rx, ry, 30.min(rw), false)));
    {
        let e = Expander::new(&win, "Advanced options (Space)", rx, ry, 30.min(rw));
        e.set_expanded(false);
        add("Expander", "Space expand/collapse", Box::new(e));
    }
    {
        let (w, h) = clamp(44, 3.max(3));
        let t = Tabs::new(&win, rx, ry, w, h.max(3));
        t.set_titles(&["Files", "Edit", "Build", "Help"]);
        add("Tabs", "Left/Right/Tab switch", Box::new(t));
    }
    {
        let (w, h) = clamp(36, 6);
        let s = Sparkline::new(&win, rx, ry, w, h);
        let v: Vec<f32> = (0..28).map(|i| 10.0 + 9.0 * ((i * 7 % 13) as f32)).collect();
        s.set_data(&v);
        s.set_color(color::LIGHTGREEN);
        add("Sparkline", "trend chart", Box::new(s));
    }
    {
        let g2 = Gauge::new(&win, rx, ry, 34.min(rw));
        g2.set_percent(72);
        add("Gauge", "percentage bar", Box::new(g2));
    }
    {
        let (w, h) = clamp(36, 9);
        let b = Barchart::new(&win, rx, ry, w, h);
        b.set_data(&[30.0, 75.0, 45.0, 90.0, 60.0], &["Jan", "Feb", "Mar", "Apr", "May"]);
        add("Bar chart", "labelled bars", Box::new(b));
    }
    {
        let (w, h) = clamp(52, 12);
        let c = Linechart::new(&win, rx, ry, w, h);
        c.set_title("sin / cos");
        let a: Vec<f64> = (0..40).map(|i| (i as f64 * 0.3).sin()).collect();
        let b: Vec<f64> = (0..40).map(|i| 0.6 * (i as f64 * 0.3).cos()).collect();
        c.add_series(&a, color::LIGHTGREEN);
        c.add_series(&b, color::LIGHTRED);
        add("Line chart", "XY plot", Box::new(c));
    }
    {
        let (w, h) = clamp(44, 11);
        let p = Piechart::new(&win, rx, ry, w, h);
        p.set_data(&[45.0, 25.0, 20.0, 10.0], &["TCP", "UDP", "TLS", "ARP"]);
        p.set_title("protocols");
        add("Pie chart", "proportions + legend", Box::new(p));
    }
    {
        let (w, h) = clamp(52, 14);
        let s = Scatter::new(&win, rx, ry, w, h);
        s.set_title("x vs y");
        for i in 0..40 {
            let x = i as f64;
            let y = i as f64 * 0.5 + ((i * 7 % 11) as f64) - 5.0;
            s.add_point(x, y, color::LIGHTGREEN);
        }
        add("Scatter plot", "x/y points, auto axes", Box::new(s));
    }
    {
        let (w, h) = clamp(30, 9);
        let s = Segdisplay::new(&win, rx, ry, w, h);
        s.set_title("clock");
        s.set_text("12:45");
        add("Seven-segment", "LED digits", Box::new(s));
    }
    {
        let (w, h) = clamp(36, 12);
        let t = Tree::new(&win, rx, ry, w, h, std::ptr::null_mut(), Box::new(DemoTree));
        t.set_title("files");
        t.key(key::RIGHT); // unfold the root so there is something to see
        add("Tree", "Right/Left fold, arrows", Box::new(t));
    }
    {
        let (w, h) = clamp(36, 9);
        let t = Table::new(&win, rx, ry, w, h, Box::new(DemoTable));
        t.set_title("people");
        t.set_current(0, 0);
        add("Table", "arrows move the cell", Box::new(t));
    }
    {
        let (w, h) = clamp(60, 12);
        let hv = Hexview::new(&win, rx, ry, w, h);
        hv.set_title("bytes");
        hv.set_data(&hexbuf);
        hv.set_highlight(16, 8);
        add("Hex view", "arrows move the cursor", Box::new(hv));
    }
    {
        let (w, h) = clamp(62, 16);
        let m = Map::new(&win, rx, ry, w, h);
        m.set_title("world");
        m.add_world(color::GREEN);
        for c in ["London", "New York", "Tokyo", "Sao Paulo", "Sydney", "Cairo"] {
            m.add_city(c, 'o', color::LIGHTGREEN);
        }
        add("Map", "arrows hop cities", Box::new(m));
    }
    {
        let (w, h) = clamp(30, 11);
        let c = Calendar::new(&win, rx, ry, w, h);
        c.set_date(2026, 6, 25);
        add("Calendar", "arrows by day/week", Box::new(c));
    }
    {
        let (w, h) = clamp(52, 9);
        let m = Mindmap::new(&win, rx, ry, w, h);
        m.set_title("plan");
        let root = m.root();
        m.set_text(root, "Project");
        let design = m.add_child(root, "Design");
        m.add_child(design, "UI");
        m.add_child(design, "API");
        let build = m.add_child(root, "Build");
        m.add_child(build, "CI");
        m.add_child(root, "Ship");
        m.select(design);
        add("Mind map", "arrows + Space fold", Box::new(m));
    }
    {
        let (w, h) = clamp(42, 8);
        let e = Editor::new(&win, rx, ry, w, h);
        e.set_text("int main(void) {\n    printf(\"hi\\n\");\n    return 0;\n}\n");
        add("Editor", "type to edit", Box::new(e));
    }
    {
        let (w, h) = clamp(40, 8);
        let d = Dialog::new(&win, rx, ry, w, h.max(6));
        d.set("Confirm", "Save changes before quitting?", &["OK", "Cancel"]);
        add("Dialog", "Left/Right + Enter", Box::new(d));
    }
    {
        let (w, h) = clamp(46, 14);
        let f = Filechooser::new(&win, rx, ry, w, h);
        f.set_dir(".");
        add("File chooser", "browse a directory", Box::new(f));
    }

    // The menu list on the left. Its built-in `/` search filters it as you type.
    let menu = Textlist::new(&win, 1, ry);
    menu.set_view_size(rh as u32);
    for p in &pages {
        menu.append(p.name);
    }

    let status = gtcaca::Statusbar::new("");

    // ── the event loop ──────────────────────────────────────────────────────
    let mut on_widget = false; // false: list has focus; true: the widget does
    let mut sel = 0usize;
    loop {
        // Resolve the highlighted list row to a page index (search may filter).
        if let Some(name) = menu.selected() {
            if let Some(i) = pages.iter().position(|p| p.name == name) {
                sel = i;
            }
        }
        let page = &pages[sel];

        // Focus goes to exactly one place.
        menu.set_focus(!on_widget);
        page.widget.set_focus(on_widget);

        // Paint only what we want: the frame, the menu, the selected widget, the
        // status line. (No `redraw()` — that would paint every hidden widget.)
        ctx.clear();
        win.paint();
        menu.paint();
        page.widget.paint();
        status.set_text(&format!(
            " {}  —  {}     [{}]",
            page.name,
            page.desc,
            if on_widget { "keys go to the widget · Tab/Esc: back" } else { "Up/Down · Tab: use · /: search · q: quit" }
        ));
        status.draw();
        ctx.flush();

        let k = match gtcaca::poll_key(-1) {
            Some(k) => k,
            None => continue,
        };

        if on_widget {
            if k == key::ESCAPE || k == key::TAB {
                on_widget = false;
            } else {
                page.widget.send_key(k);
            }
        } else if menu.is_searching() {
            menu.send_key(k); // search owns every key until Enter/Esc
        } else {
            const Q: i32 = b'q' as i32;
            const QU: i32 = b'Q' as i32;
            match k {
                Q | QU => break,
                key::TAB | key::RETURN | key::RIGHT => on_widget = true,
                _ => {
                    menu.send_key(k); // Up/Down and '/'
                }
            }
        }
    }
    Ok(())
}

