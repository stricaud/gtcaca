//! # A tiny data-analytics spreadsheet, block by block
//!
//! This example is a guided tour: read it top to bottom to learn how to build a
//! real gtcaca app. It loads a CSV, shows it in a [`Table`], and summarises a
//! numeric column two ways — a **histogram** ([`Barchart`]) and a **pie chart**
//! ([`Piechart`]) of the share each category contributes.
//!
//! ```text
//! cargo run --example spreadsheet            # built-in sample data
//! cargo run --example spreadsheet sales.csv  # your own CSV (label,num,num,…)
//! ```
//!
//! Keys:  ↑/↓ move the row · ←/→ move the column · c: chart the next column ·
//! q: quit.
//!
//! The pattern here — *model → widgets → recompute on change → custom draw loop*
//! — is the same one carscal (the packet analyzer) uses at scale.

use std::cell::RefCell;
use std::rc::Rc;

use gtcaca::{
    color, key, Application, Barchart, Entry, Gtcaca, Piechart, Statusbar, Table, TableModel, Widget,
    Window,
};

// ─────────────────────────────────────────────────────────────────────────────
// STEP 1 — The data model.
//
// A spreadsheet is just a grid of strings plus a header row. We keep it in a
// plain struct behind `Rc<RefCell<…>>` so both the Table's model and our loop
// can see it. The first column is treated as a *label* (category); the rest are
// numbers we can chart.
// ─────────────────────────────────────────────────────────────────────────────

struct Sheet {
    headers: Vec<String>,
    rows: Vec<Vec<String>>,
}

impl Sheet {
    /// Parse a very small CSV (no quoted-comma handling — a real app would reuse
    /// its CSV crate; the point here is the widgets, not the parser).
    fn from_csv(text: &str) -> Sheet {
        let mut lines = text.lines().filter(|l| !l.trim().is_empty());
        let headers = lines
            .next()
            .unwrap_or("label,value")
            .split(',')
            .map(|s| s.trim().to_string())
            .collect();
        let rows = lines
            .map(|l| l.split(',').map(|s| s.trim().to_string()).collect())
            .collect();
        Sheet { headers, rows }
    }

    fn cols(&self) -> usize {
        self.headers.len()
    }

    /// The numbers in column `col`, one per row (non-numeric cells read as 0).
    fn column_values(&self, col: usize) -> Vec<f32> {
        self.rows
            .iter()
            .map(|r| r.get(col).and_then(|c| c.parse::<f32>().ok()).unwrap_or(0.0))
            .collect()
    }

    /// The row labels (column 0), for chart axes and pie slices.
    fn labels(&self) -> Vec<String> {
        self.rows.iter().map(|r| r.first().cloned().unwrap_or_default()).collect()
    }

    /// The text of one cell (empty if out of range).
    fn get_cell(&self, row: usize, col: usize) -> String {
        self.rows.get(row).and_then(|r| r.get(col)).cloned().unwrap_or_default()
    }

    /// Overwrite one cell (used by the in-place editor).
    fn set_cell(&mut self, row: usize, col: usize, value: &str) {
        if let Some(r) = self.rows.get_mut(row) {
            if let Some(c) = r.get_mut(col) {
                *c = value.to_string();
            }
        }
    }
}

// Recompute both charts from column `col`. A free function (not a closure) so
// the setup and the editor can both call it. Values/labels are copied into the
// widgets, so there are no lifetimes to track.
fn refresh_charts(sheet: &RefCell<Sheet>, bars: &Barchart, pie: &Piechart, col: usize) {
    let s = sheet.borrow();
    let values = s.column_values(col);
    let labels = s.labels();
    let label_refs: Vec<&str> = labels.iter().map(|s| s.as_str()).collect();

    bars.set_data(&values, &label_refs); // histogram: magnitude per category
    pie.set_data(&values, &label_refs); // pie: the same numbers as proportions
    pie.set_title(&format!("{} share", s.headers.get(col).map(String::as_str).unwrap_or("")));
}

// A built-in dataset so the example runs with no arguments.
const SAMPLE: &str = "\
Region,Q1,Q2,Q3,Q4
North,42,55,61,70
South,30,28,35,40
East,58,63,59,66
West,25,33,44,52
Central,48,50,47,53";

// ─────────────────────────────────────────────────────────────────────────────
// STEP 2 — Bridge the data to the Table widget.
//
// `Table` renders through a `TableModel`: it asks how many rows/cols there are
// and for the text of each cell, on demand. We implement that trait over a
// shared handle to the `Sheet`. This "virtual" model means the Table never
// copies the data and scales to huge sheets.
// ─────────────────────────────────────────────────────────────────────────────

struct SheetModel(Rc<RefCell<Sheet>>);

impl TableModel for SheetModel {
    fn row_count(&self) -> i64 {
        self.0.borrow().rows.len() as i64
    }
    fn headers(&self) -> Vec<String> {
        self.0.borrow().headers.clone()
    }
    fn cell(&self, row: i64, col: i32) -> String {
        self.0
            .borrow()
            .rows
            .get(row as usize)
            .and_then(|r| r.get(col as usize))
            .cloned()
            .unwrap_or_default()
    }
}

fn main() -> Result<(), gtcaca::Error> {
    // ─────────────────────────────────────────────────────────────────────────
    // STEP 3 — Load the data (CLI argument or the built-in sample).
    // ─────────────────────────────────────────────────────────────────────────
    let text = std::env::args()
        .nth(1)
        .and_then(|p| std::fs::read_to_string(p).ok())
        .unwrap_or_else(|| SAMPLE.to_string());
    let sheet = Rc::new(RefCell::new(Sheet::from_csv(&text)));

    // Which numeric column we currently chart (start at column 1, the first
    // number after the label column). `c` cycles it.
    let mut chart_col = 1usize.min(sheet.borrow().cols().saturating_sub(1));

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 4 — Open the toolkit and lay out the window.
    //
    // `Window::content(margin)` hands back the inner rectangle so we never
    // hand-compute `x + 1` / `width - 2`. We split it: the table on top, the two
    // charts side by side underneath.
    // ─────────────────────────────────────────────────────────────────────────
    let ctx = Gtcaca::init()?;
    let app = Application::new(&ctx, "gtcaca spreadsheet");
    let win = Window::fullscreen(&app, Some("Data analytics"));
    let area = win.content(1);

    let table_h = (area.height / 2).max(6);
    let charts_y = area.y + table_h + 1;
    let charts_h = (area.y + area.height - charts_y).max(6);
    let half_w = area.width / 2;

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 5 — Create the widgets.
    //
    //   • Table    — the raw grid, backed by our model.
    //   • Barchart — a histogram: one bar per row for the chosen column.
    //   • Piechart — the share each row contributes to that column's total.
    // ─────────────────────────────────────────────────────────────────────────
    let model = Box::new(SheetModel(Rc::clone(&sheet)));
    let table = Table::new(&win, area.x, area.y, area.width, table_h, model);
    table.set_title("data");
    table.set_current(0, 0);

    let bars = Barchart::new(&win, area.x, charts_y, half_w, charts_h);
    bars.set_color(color::CYAN);

    let pie = Piechart::new(&win, area.x + half_w + 1, charts_y, area.width - half_w - 1, charts_h);

    let status = Statusbar::new("");

    // A one-line editor built from an Entry, on the bottom row. We only feed it
    // keys while editing a cell (STEP 8).
    let prompt = Entry::new(&win, area.x, area.y + area.height - 1, area.width);

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 6 — Draw the initial charts for the starting column.
    // ─────────────────────────────────────────────────────────────────────────
    refresh_charts(&sheet, &bars, &pie, chart_col);

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 7 — The render + input loop.
    //
    // We paint exactly the widgets we want (clear → paint → flush) rather than
    // `redraw()`, then block for a key. The Table consumes arrow keys via
    // `send_key`; we handle the rest.
    // ─────────────────────────────────────────────────────────────────────────
    table.set_focus(true);
    loop {
        ctx.clear();
        win.paint();
        table.paint();
        bars.paint();
        pie.paint();
        status.set_text(&format!(
            " row {}  ·  charting '{}'  —  arrows move · Enter: edit cell · c: next column · q: quit",
            table.current_row() + 1,
            sheet.borrow().headers.get(chart_col).map(String::as_str).unwrap_or("")
        ));
        status.draw();
        ctx.flush();

        let Some(k) = gtcaca::poll_key(-1) else { continue };
        const Q: i32 = b'q' as i32;
        const QU: i32 = b'Q' as i32;
        const C: i32 = b'c' as i32;
        match k {
            Q | QU => break,
            C => {
                // Cycle to the next numeric column (skip the label column 0).
                let cols = sheet.borrow().cols();
                if cols > 1 {
                    chart_col = chart_col % (cols - 1) + 1;
                    refresh_charts(&sheet, &bars, &pie, chart_col);
                }
            }
            // ─────────────────────────────────────────────────────────────────
            // STEP 8 — Edit the current cell in place, then recompute the charts.
            //
            // Editing a value the charts are drawn from and pressing Enter
            // immediately re-draws the histogram and pie from the new number —
            // the whole point of a live analytics view.
            // ─────────────────────────────────────────────────────────────────
            key::RETURN => {
                let (row, col) = (table.current_row() as usize, table.current_col() as usize);
                prompt.set_text(&sheet.borrow().get_cell(row, col));
                prompt.set_focus(true);
                loop {
                    ctx.clear();
                    win.paint();
                    table.paint();
                    bars.paint();
                    pie.paint();
                    prompt.paint();
                    ctx.flush();
                    let Some(k) = gtcaca::poll_key(-1) else { continue };
                    match k {
                        key::RETURN => {
                            sheet.borrow_mut().set_cell(row, col, &prompt.text());
                            break;
                        }
                        key::ESCAPE => break,
                        _ => {
                            prompt.send_key(k); // the Entry edits the text for us
                        }
                    }
                }
                prompt.set_focus(false);
                refresh_charts(&sheet, &bars, &pie, chart_col); // reflect the change
            }
            key::UP | key::DOWN | key::LEFT | key::RIGHT => {
                table.send_key(k);
            }
            _ => {}
        }
    }
    Ok(())
}
