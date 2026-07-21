//! Safe wrappers for the remaining GTCaca widgets: form controls, charts, the
//! mind map, and the analytics-oriented pie/scatter plots. They follow the same
//! conventions as the core widgets in [`crate`]: each is a lightweight handle
//! tied to its parent's lifetime, built with `new`, and (for the chart/model
//! widgets) painted with an explicit `draw` when you run your own event loop.
//!
//! Every widget also gets [`Widget::send_key`] for feeding it keys and
//! [`Widget::set_focus`] for the focus highlight.

use std::ffi::{c_char, c_int, CStr, CString};
use std::marker::PhantomData;

use gtcaca_sys as sys;

use crate::{cstring, Widget};

// Build a NUL-terminated C string vector plus a borrowed pointer array. The
// returned `Vec<CString>` must be kept alive while the pointers are used.
fn strv(items: &[&str]) -> (Vec<CString>, Vec<*const c_char>) {
    let owned: Vec<CString> = items.iter().map(|s| cstring(s).unwrap_or_default()).collect();
    let ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
    (owned, ptrs)
}

macro_rules! impl_widget {
    ($t:ident) => {
        impl Widget for $t<'_> {
            fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
                self.ptr as *mut sys::gtcaca_widget_t
            }
        }
    };
}

// ── Radio button ────────────────────────────────────────────────────────────

/// A radio button. Buttons sharing a `group_id` are mutually exclusive.
pub struct Radiobutton<'a> {
    ptr: *mut sys::gtcaca_radiobutton_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Radiobutton<'a> {
    pub fn new(parent: &'a impl Widget, label: &str, group_id: i32, x: i32, y: i32) -> Radiobutton<'a> {
        let c = cstring(label).unwrap_or_default();
        let ptr = unsafe {
            sys::gtcaca_radiobutton_new(parent.as_widget_ptr(), c.as_ptr(), group_id, x, y)
        };
        assert!(!ptr.is_null(), "gtcaca_radiobutton_new returned NULL");
        Radiobutton { ptr, _p: PhantomData }
    }
    /// Make this the active button in its group.
    pub fn set_active(&self) {
        unsafe { sys::gtcaca_radiobutton_set_active(self.ptr) };
    }
    pub fn is_active(&self) -> bool {
        unsafe { sys::gtcaca_radiobutton_get_active(self.ptr) != 0 }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_radiobutton_draw(self.ptr) };
    }
}
impl_widget!(Radiobutton);

// ── Combo box ───────────────────────────────────────────────────────────────

/// A drop-down of text choices (Up/Down/Enter to pick).
pub struct Combobox<'a> {
    ptr: *mut sys::gtcaca_combobox_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Combobox<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32) -> Combobox<'a> {
        let ptr = unsafe { sys::gtcaca_combobox_new(parent.as_widget_ptr(), x, y, width) };
        assert!(!ptr.is_null(), "gtcaca_combobox_new returned NULL");
        Combobox { ptr, _p: PhantomData }
    }
    pub fn append(&self, item: &str) {
        let c = cstring(item).unwrap_or_default();
        unsafe { sys::gtcaca_combobox_append(self.ptr, c.as_ptr()) };
    }
    pub fn selected(&self) -> Option<String> {
        let p = unsafe { sys::gtcaca_combobox_get_selected(self.ptr) };
        if p.is_null() { None } else { Some(unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()) }
    }
    pub fn selected_index(&self) -> i32 {
        unsafe { sys::gtcaca_combobox_get_selected_index(self.ptr) }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_combobox_draw(self.ptr) };
    }
}
impl_widget!(Combobox);

// ── Switch ──────────────────────────────────────────────────────────────────

/// An on/off toggle (Space/Enter flips it).
pub struct Switch<'a> {
    ptr: *mut sys::gtcaca_switch_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Switch<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32) -> Switch<'a> {
        let ptr = unsafe { sys::gtcaca_switch_new(parent.as_widget_ptr(), x, y) };
        assert!(!ptr.is_null(), "gtcaca_switch_new returned NULL");
        Switch { ptr, _p: PhantomData }
    }
    pub fn set_active(&self, active: bool) {
        unsafe { sys::gtcaca_switch_set_active(self.ptr, active as c_int) };
    }
    pub fn is_active(&self) -> bool {
        unsafe { sys::gtcaca_switch_get_active(self.ptr) != 0 }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_switch_draw(self.ptr) };
    }
}
impl_widget!(Switch);

// ── Scale (slider) ──────────────────────────────────────────────────────────

/// A horizontal slider over `[min, max]` (Left/Right adjust by `step`).
pub struct Scale<'a> {
    ptr: *mut sys::gtcaca_scale_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Scale<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, min: f64, max: f64, step: f64) -> Scale<'a> {
        let ptr = unsafe { sys::gtcaca_scale_new(parent.as_widget_ptr(), x, y, width, min, max, step) };
        assert!(!ptr.is_null(), "gtcaca_scale_new returned NULL");
        Scale { ptr, _p: PhantomData }
    }
    pub fn set_value(&self, value: f64) {
        unsafe { sys::gtcaca_scale_set_value(self.ptr, value) };
    }
    pub fn value(&self) -> f64 {
        unsafe { sys::gtcaca_scale_get_value(self.ptr) }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_scale_draw(self.ptr) };
    }
}
impl_widget!(Scale);

// ── Spin button ─────────────────────────────────────────────────────────────

/// A numeric spinner over `[min, max]` (Left/Right adjust by `step`).
pub struct Spinbutton<'a> {
    ptr: *mut sys::gtcaca_spinbutton_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Spinbutton<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, min: f64, max: f64, step: f64) -> Spinbutton<'a> {
        let ptr = unsafe { sys::gtcaca_spinbutton_new(parent.as_widget_ptr(), x, y, min, max, step) };
        assert!(!ptr.is_null(), "gtcaca_spinbutton_new returned NULL");
        Spinbutton { ptr, _p: PhantomData }
    }
    pub fn set_value(&self, value: f64) {
        unsafe { sys::gtcaca_spinbutton_set_value(self.ptr, value) };
    }
    pub fn value(&self) -> f64 {
        unsafe { sys::gtcaca_spinbutton_get_value(self.ptr) }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_spinbutton_draw(self.ptr) };
    }
}
impl_widget!(Spinbutton);

// ── Progress bar ────────────────────────────────────────────────────────────

/// A determinate progress bar; `value` is a fraction in `[0.0, 1.0]`.
pub struct Progressbar<'a> {
    ptr: *mut sys::gtcaca_progressbar_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Progressbar<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32) -> Progressbar<'a> {
        let ptr = unsafe { sys::gtcaca_progressbar_new(parent.as_widget_ptr(), x, y, width) };
        assert!(!ptr.is_null(), "gtcaca_progressbar_new returned NULL");
        Progressbar { ptr, _p: PhantomData }
    }
    pub fn set_value(&self, value: f32) {
        unsafe { sys::gtcaca_progressbar_set_value(self.ptr, value) };
    }
    pub fn value(&self) -> f32 {
        unsafe { sys::gtcaca_progressbar_get_value(self.ptr) }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_progressbar_draw(self.ptr) };
    }
}
impl_widget!(Progressbar);

// ── Text view ───────────────────────────────────────────────────────────────

/// A scrollable, append-only multi-line text area.
pub struct Textview<'a> {
    ptr: *mut sys::gtcaca_textview_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Textview<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Textview<'a> {
        let ptr = unsafe { sys::gtcaca_textview_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_textview_new returned NULL");
        Textview { ptr, _p: PhantomData }
    }
    pub fn append(&self, line: &str) {
        let c = cstring(line).unwrap_or_default();
        unsafe { sys::gtcaca_textview_append(self.ptr, c.as_ptr()) };
    }
    pub fn clear(&self) {
        unsafe { sys::gtcaca_textview_clear(self.ptr) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_textview_draw(self.ptr) };
    }
}
impl_widget!(Textview);

// ── Frame ───────────────────────────────────────────────────────────────────

/// A titled box that visually groups the widgets placed inside it.
pub struct Frame<'a> {
    ptr: *mut sys::gtcaca_frame_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Frame<'a> {
    pub fn new(parent: &'a impl Widget, label: &str, x: i32, y: i32, width: i32, height: i32) -> Frame<'a> {
        let c = cstring(label).unwrap_or_default();
        let ptr = unsafe { sys::gtcaca_frame_new(parent.as_widget_ptr(), c.as_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_frame_new returned NULL");
        Frame { ptr, _p: PhantomData }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_frame_draw(self.ptr) };
    }
}
impl_widget!(Frame);

// ── Separator ───────────────────────────────────────────────────────────────

/// A horizontal or vertical divider rule.
pub struct Separator<'a> {
    ptr: *mut sys::gtcaca_separator_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Separator<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, length: i32, vertical: bool) -> Separator<'a> {
        let ptr = unsafe {
            sys::gtcaca_separator_new(parent.as_widget_ptr(), x, y, length, vertical as c_int)
        };
        assert!(!ptr.is_null(), "gtcaca_separator_new returned NULL");
        Separator { ptr, _p: PhantomData }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_separator_draw(self.ptr) };
    }
}
impl_widget!(Separator);

// ── Expander ────────────────────────────────────────────────────────────────

/// A disclosure header that shows or hides its managed widgets (Space toggles).
pub struct Expander<'a> {
    ptr: *mut sys::gtcaca_expander_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Expander<'a> {
    pub fn new(parent: &'a impl Widget, label: &str, x: i32, y: i32, width: i32) -> Expander<'a> {
        let c = cstring(label).unwrap_or_default();
        let ptr = unsafe { sys::gtcaca_expander_new(parent.as_widget_ptr(), c.as_ptr(), x, y, width) };
        assert!(!ptr.is_null(), "gtcaca_expander_new returned NULL");
        Expander { ptr, _p: PhantomData }
    }
    /// Reveal/hide `widget` together with the expander.
    pub fn add_managed(&self, widget: &impl Widget) {
        unsafe { sys::gtcaca_expander_add_managed(self.ptr, widget.as_widget_ptr()) };
    }
    pub fn set_expanded(&self, expanded: bool) {
        unsafe { sys::gtcaca_expander_set_expanded(self.ptr, expanded as c_int) };
    }
    pub fn is_expanded(&self) -> bool {
        unsafe { sys::gtcaca_expander_get_expanded(self.ptr) != 0 }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_expander_draw(self.ptr) };
    }
}
impl_widget!(Expander);

// ── Tabs ────────────────────────────────────────────────────────────────────

/// A tab bar (Left/Right/Tab switch pages).
pub struct Tabs<'a> {
    ptr: *mut sys::gtcaca_tabs_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Tabs<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Tabs<'a> {
        let ptr = unsafe { sys::gtcaca_tabs_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_tabs_new returned NULL");
        Tabs { ptr, _p: PhantomData }
    }
    pub fn set_titles(&self, titles: &[&str]) {
        let (_owned, mut ptrs) = strv(titles);
        unsafe { sys::gtcaca_tabs_set_titles(self.ptr, ptrs.as_mut_ptr(), ptrs.len() as c_int) };
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_tabs_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn selected(&self) -> i32 {
        unsafe { sys::gtcaca_tabs_selected(self.ptr) }
    }
    pub fn set_selected(&self, idx: i32) {
        unsafe { sys::gtcaca_tabs_set_selected(self.ptr, idx) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_tabs_draw(self.ptr) };
    }
}
impl_widget!(Tabs);

// ── Sparkline ───────────────────────────────────────────────────────────────

/// A compact inline trend chart (a row of mini-bars).
pub struct Sparkline<'a> {
    ptr: *mut sys::gtcaca_sparkline_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Sparkline<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Sparkline<'a> {
        let ptr = unsafe { sys::gtcaca_sparkline_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_sparkline_new returned NULL");
        Sparkline { ptr, _p: PhantomData }
    }
    pub fn set_data(&self, values: &[f32]) {
        unsafe { sys::gtcaca_sparkline_set_data(self.ptr, values.as_ptr(), values.len() as c_int) };
    }
    /// Append one sample (scrolls the window).
    pub fn push(&self, value: f32) {
        unsafe { sys::gtcaca_sparkline_push(self.ptr, value) };
    }
    pub fn set_color(&self, colour: u8) {
        unsafe { sys::gtcaca_sparkline_set_color(self.ptr, colour) };
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_sparkline_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_sparkline_draw(self.ptr) };
    }
}
impl_widget!(Sparkline);

// ── Gauge ───────────────────────────────────────────────────────────────────

/// A single-value percentage bar with an optional label.
pub struct Gauge<'a> {
    ptr: *mut sys::gtcaca_gauge_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Gauge<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32) -> Gauge<'a> {
        let ptr = unsafe { sys::gtcaca_gauge_new(parent.as_widget_ptr(), x, y, width) };
        assert!(!ptr.is_null(), "gtcaca_gauge_new returned NULL");
        Gauge { ptr, _p: PhantomData }
    }
    pub fn set_percent(&self, percent: i32) {
        unsafe { sys::gtcaca_gauge_set_percent(self.ptr, percent) };
    }
    pub fn set_value(&self, value: f32) {
        unsafe { sys::gtcaca_gauge_set_value(self.ptr, value) };
    }
    pub fn value(&self) -> f32 {
        unsafe { sys::gtcaca_gauge_get_value(self.ptr) }
    }
    pub fn set_label(&self, label: &str) {
        let c = cstring(label).unwrap_or_default();
        unsafe { sys::gtcaca_gauge_set_label(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_gauge_draw(self.ptr) };
    }
}
impl_widget!(Gauge);

// ── Bar chart ───────────────────────────────────────────────────────────────

/// A vertical bar chart — one labelled bar per value (histograms, counts).
pub struct Barchart<'a> {
    ptr: *mut sys::gtcaca_barchart_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Barchart<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Barchart<'a> {
        let ptr = unsafe { sys::gtcaca_barchart_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_barchart_new returned NULL");
        Barchart { ptr, _p: PhantomData }
    }
    /// Set the bar values and (optionally) their labels. Pass an empty `labels`
    /// slice for none; otherwise it should be the same length as `values`.
    pub fn set_data(&self, values: &[f32], labels: &[&str]) {
        let (_owned, ptrs) = strv(labels);
        let lp = if labels.is_empty() { std::ptr::null() } else { ptrs.as_ptr() };
        unsafe { sys::gtcaca_barchart_set_data(self.ptr, values.as_ptr(), lp, values.len() as c_int) };
    }
    /// Fix the scale maximum (0.0 = auto-scale to the data).
    pub fn set_max(&self, maxval: f32) {
        unsafe { sys::gtcaca_barchart_set_max(self.ptr, maxval) };
    }
    pub fn set_color(&self, colour: u8) {
        unsafe { sys::gtcaca_barchart_set_color(self.ptr, colour) };
    }
    pub fn set_bar_width(&self, bar_width: i32, gap: i32) {
        unsafe { sys::gtcaca_barchart_set_bar_width(self.ptr, bar_width, gap) };
    }
    pub fn set_show_values(&self, on: bool) {
        unsafe { sys::gtcaca_barchart_set_show_values(self.ptr, on as c_int) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_barchart_draw(self.ptr) };
    }
}
impl_widget!(Barchart);

// ── Seven-segment display ───────────────────────────────────────────────────

/// A retro seven-segment LED readout (digits, colon, a few letters).
pub struct Segdisplay<'a> {
    ptr: *mut sys::gtcaca_segdisplay_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Segdisplay<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Segdisplay<'a> {
        let ptr = unsafe { sys::gtcaca_segdisplay_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_segdisplay_new returned NULL");
        Segdisplay { ptr, _p: PhantomData }
    }
    pub fn set_text(&self, text: &str) {
        let c = cstring(text).unwrap_or_default();
        unsafe { sys::gtcaca_segdisplay_set_text(self.ptr, c.as_ptr()) };
    }
    pub fn set_colour(&self, colour: u8) {
        unsafe { sys::gtcaca_segdisplay_set_colour(self.ptr, colour) };
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_segdisplay_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_segdisplay_draw(self.ptr) };
    }
}
impl_widget!(Segdisplay);

// ── Map ─────────────────────────────────────────────────────────────────────

/// A world map with plotted cities/points (arrows hop between them).
pub struct Map<'a> {
    ptr: *mut sys::gtcaca_map_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Map<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Map<'a> {
        let ptr = unsafe { sys::gtcaca_map_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_map_new returned NULL");
        Map { ptr, _p: PhantomData }
    }
    /// Draw the built-in world coastline in `colour`.
    pub fn add_world(&self, colour: u8) {
        unsafe { sys::gtcaca_map_add_world(self.ptr, colour) };
    }
    /// Plot a named city (looked up in the built-in gazetteer). Returns whether
    /// the city was found.
    pub fn add_city(&self, name: &str, glyph: char, colour: u8) -> bool {
        let c = cstring(name).unwrap_or_default();
        unsafe { sys::gtcaca_map_add_city(self.ptr, c.as_ptr(), glyph as u32, colour) != 0 }
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_map_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_map_draw(self.ptr) };
    }
}
impl_widget!(Map);

// ── Calendar ────────────────────────────────────────────────────────────────

/// A month calendar (arrows by day/week, PgUp/PgDn by month).
pub struct Calendar<'a> {
    ptr: *mut sys::gtcaca_calendar_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Calendar<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Calendar<'a> {
        let ptr = unsafe { sys::gtcaca_calendar_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_calendar_new returned NULL");
        Calendar { ptr, _p: PhantomData }
    }
    pub fn set_date(&self, year: i32, month: i32, day: i32) {
        unsafe { sys::gtcaca_calendar_set_date(self.ptr, year, month, day) };
    }
    /// The currently-selected `(year, month, day)`.
    pub fn date(&self) -> (i32, i32, i32) {
        let (mut y, mut m, mut d) = (0, 0, 0);
        unsafe { sys::gtcaca_calendar_get_date(self.ptr, &mut y, &mut m, &mut d) };
        (y, m, d)
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_calendar_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_calendar_draw(self.ptr) };
    }
}
impl_widget!(Calendar);

// ── Mind map ────────────────────────────────────────────────────────────────

/// A handle to one node in a [`Mindmap`]. Cheap to copy; valid while its map is.
#[derive(Copy, Clone)]
pub struct MmNode(*mut sys::gtcaca_mm_node_t);

impl MmNode {
    /// The raw node pointer (escape hatch).
    pub fn as_ptr(self) -> *mut sys::gtcaca_mm_node_t {
        self.0
    }
    pub fn is_null(self) -> bool {
        self.0.is_null()
    }
    /// This node's text.
    pub fn text(self) -> String {
        if self.0.is_null() {
            return String::new();
        }
        let p = unsafe { (*self.0).text };
        if p.is_null() { String::new() } else { unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned() }
    }
    pub fn is_folded(self) -> bool {
        !self.0.is_null() && unsafe { (*self.0).folded != 0 }
    }
    pub fn set_folded(self, folded: bool) {
        if !self.0.is_null() {
            unsafe { (*self.0).folded = folded as c_int };
        }
    }
    /// This node's child nodes, left to right.
    pub fn children(self) -> Vec<MmNode> {
        if self.0.is_null() {
            return Vec::new();
        }
        unsafe {
            let n = (*self.0).nkids.max(0) as usize;
            let kids = (*self.0).kids;
            (0..n).map(|i| MmNode(*kids.add(i))).collect()
        }
    }
}

/// A FreeMind-style mind map: a tree of text nodes laid out left to right.
pub struct Mindmap<'a> {
    ptr: *mut sys::gtcaca_mindmap_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Mindmap<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Mindmap<'a> {
        let ptr = unsafe { sys::gtcaca_mindmap_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_mindmap_new returned NULL");
        Mindmap { ptr, _p: PhantomData }
    }
    /// The root node (created automatically; set its text with [`Mindmap::set_text`]).
    pub fn root(&self) -> MmNode {
        MmNode(unsafe { sys::gtcaca_mindmap_root(self.ptr) })
    }
    /// Append a child under `parent` and return it.
    pub fn add_child(&self, parent: MmNode, text: &str) -> MmNode {
        let c = cstring(text).unwrap_or_default();
        MmNode(unsafe { sys::gtcaca_mindmap_add_child(self.ptr, parent.0, c.as_ptr()) })
    }
    /// Insert a sibling after `node` and return it (no-op on the root).
    pub fn add_sibling(&self, node: MmNode, text: &str) -> MmNode {
        let c = cstring(text).unwrap_or_default();
        MmNode(unsafe { sys::gtcaca_mindmap_add_sibling(self.ptr, node.0, c.as_ptr()) })
    }
    /// Remove `node` and its subtree (no-op on the root).
    pub fn delete(&self, node: MmNode) {
        unsafe { sys::gtcaca_mindmap_delete(self.ptr, node.0) };
    }
    pub fn set_text(&self, node: MmNode, text: &str) {
        let c = cstring(text).unwrap_or_default();
        unsafe { sys::gtcaca_mindmap_set_text(node.0, c.as_ptr()) };
    }
    pub fn selected(&self) -> MmNode {
        MmNode(unsafe { sys::gtcaca_mindmap_selected(self.ptr) })
    }
    pub fn select(&self, node: MmNode) {
        unsafe { sys::gtcaca_mindmap_select(self.ptr, node.0) };
    }
    /// Free every node and start over with a single root holding `root_text`.
    pub fn clear(&self, root_text: &str) {
        let c = cstring(root_text).unwrap_or_default();
        unsafe { sys::gtcaca_mindmap_clear(self.ptr, c.as_ptr()) };
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_mindmap_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_mindmap_draw(self.ptr) };
    }
}
impl_widget!(Mindmap);

// ── Editor ──────────────────────────────────────────────────────────────────

/// A multi-line text editor (Emacs-ish keys, optional syntax highlighting).
pub struct Editor<'a> {
    ptr: *mut sys::gtcaca_editor_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Editor<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Editor<'a> {
        let ptr = unsafe { sys::gtcaca_editor_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_editor_new returned NULL");
        Editor { ptr, _p: PhantomData }
    }
    pub fn set_text(&self, text: &str) {
        let c = cstring(text).unwrap_or_default();
        unsafe { sys::gtcaca_editor_set_text(self.ptr, c.as_ptr()) };
    }
    /// The full buffer contents.
    pub fn text(&self) -> String {
        let len = unsafe { sys::gtcaca_editor_get_length(self.ptr) };
        if len <= 0 {
            return String::new();
        }
        let mut buf = vec![0u8; len as usize + 1];
        let n = unsafe {
            sys::gtcaca_editor_get_text(self.ptr, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int)
        };
        if n < 0 {
            return String::new();
        }
        let end = buf.iter().position(|&b| b == 0).unwrap_or(buf.len());
        String::from_utf8_lossy(&buf[..end]).into_owned()
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_editor_draw(self.ptr) };
    }
}
impl_widget!(Editor);

// ── Dialog ──────────────────────────────────────────────────────────────────

/// A modal message box with a row of buttons (Left/Right + Enter to choose).
pub struct Dialog<'a> {
    ptr: *mut sys::gtcaca_dialog_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Dialog<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Dialog<'a> {
        let ptr = unsafe { sys::gtcaca_dialog_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_dialog_new returned NULL");
        Dialog { ptr, _p: PhantomData }
    }
    /// Configure the title, message and button labels.
    pub fn set(&self, title: &str, message: &str, buttons: &[&str]) {
        let tc = cstring(title).unwrap_or_default();
        let mc = cstring(message).unwrap_or_default();
        let (_owned, mut ptrs) = strv(buttons);
        unsafe {
            sys::gtcaca_dialog_set(
                self.ptr,
                tc.as_ptr(),
                mc.as_ptr(),
                ptrs.as_mut_ptr(),
                ptrs.len() as c_int,
            )
        };
    }
    /// The chosen button index once decided, or a negative "ongoing/cancelled"
    /// sentinel (see the C `GTCACA_DIALOG_*` constants).
    pub fn result(&self) -> i32 {
        unsafe { sys::gtcaca_dialog_result(self.ptr) }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_dialog_draw(self.ptr) };
    }
}
impl_widget!(Dialog);

// ── File chooser ────────────────────────────────────────────────────────────

/// A directory browser dialog.
pub struct Filechooser<'a> {
    ptr: *mut sys::gtcaca_filechooser_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Filechooser<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Filechooser<'a> {
        let ptr = unsafe { sys::gtcaca_filechooser_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_filechooser_new returned NULL");
        Filechooser { ptr, _p: PhantomData }
    }
    /// Browse starting at `dir`.
    pub fn set_dir(&self, dir: &str) {
        let c = cstring(dir).unwrap_or_default();
        unsafe { sys::gtcaca_filechooser_set_dir(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_filechooser_draw(self.ptr) };
    }
}
impl_widget!(Filechooser);

// ── Pie chart ───────────────────────────────────────────────────────────────

/// A pie / donut chart of categorical proportions, with an optional legend.
pub struct Piechart<'a> {
    ptr: *mut sys::gtcaca_piechart_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Piechart<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Piechart<'a> {
        let ptr = unsafe { sys::gtcaca_piechart_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_piechart_new returned NULL");
        Piechart { ptr, _p: PhantomData }
    }
    /// Set the slice values and (optionally) their labels. Values need not sum
    /// to any particular total; each slice is drawn proportionally.
    pub fn set_data(&self, values: &[f32], labels: &[&str]) {
        let (_owned, ptrs) = strv(labels);
        let lp = if labels.is_empty() { std::ptr::null() } else { ptrs.as_ptr() };
        unsafe { sys::gtcaca_piechart_set_data(self.ptr, values.as_ptr(), lp, values.len() as c_int) };
    }
    /// Override the slice colours (otherwise a built-in palette is cycled).
    pub fn set_colors(&self, colours: &[u8]) {
        unsafe { sys::gtcaca_piechart_set_colors(self.ptr, colours.as_ptr(), colours.len() as c_int) };
    }
    /// Draw a hollow centre (ring chart).
    pub fn set_donut(&self, on: bool) {
        unsafe { sys::gtcaca_piechart_set_donut(self.ptr, on as c_int) };
    }
    pub fn set_show_legend(&self, on: bool) {
        unsafe { sys::gtcaca_piechart_set_show_legend(self.ptr, on as c_int) };
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_piechart_set_title(self.ptr, c.as_ptr()) };
    }
    /// The palette colour slice `i` gets when no explicit colours are set.
    pub fn palette(i: i32) -> u8 {
        unsafe { sys::gtcaca_piechart_palette(i) }
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_piechart_draw(self.ptr) };
    }
}
impl_widget!(Piechart);

// ── Scatter plot ────────────────────────────────────────────────────────────

/// An (x, y) scatter plot with framed, auto-fitting axes.
pub struct Scatter<'a> {
    ptr: *mut sys::gtcaca_scatter_widget_t,
    _p: PhantomData<&'a ()>,
}
impl<'a> Scatter<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Scatter<'a> {
        let ptr = unsafe { sys::gtcaca_scatter_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_scatter_new returned NULL");
        Scatter { ptr, _p: PhantomData }
    }
    /// Append one point in `colour`.
    pub fn add_point(&self, x: f64, y: f64, colour: u8) {
        unsafe { sys::gtcaca_scatter_add_point(self.ptr, x, y, colour) };
    }
    /// Append a whole series (parallel `xs`/`ys`) in one `colour`.
    pub fn add_series(&self, xs: &[f64], ys: &[f64], colour: u8) {
        let n = xs.len().min(ys.len());
        unsafe { sys::gtcaca_scatter_add_series(self.ptr, xs.as_ptr(), ys.as_ptr(), n as c_int, colour) };
    }
    /// Replace all points with a single series.
    pub fn set_data(&self, xs: &[f64], ys: &[f64], colour: u8) {
        let n = xs.len().min(ys.len());
        unsafe { sys::gtcaca_scatter_set_data(self.ptr, xs.as_ptr(), ys.as_ptr(), n as c_int, colour) };
    }
    pub fn clear(&self) {
        unsafe { sys::gtcaca_scatter_clear(self.ptr) };
    }
    /// Pin the axis range (disables autoscaling).
    pub fn set_bounds(&self, xmin: f64, xmax: f64, ymin: f64, ymax: f64) {
        unsafe { sys::gtcaca_scatter_set_bounds(self.ptr, xmin, xmax, ymin, ymax) };
    }
    pub fn set_glyph(&self, glyph: char) {
        unsafe { sys::gtcaca_scatter_set_glyph(self.ptr, glyph as u32) };
    }
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_scatter_set_title(self.ptr, c.as_ptr()) };
    }
    pub fn draw(&self) {
        unsafe { sys::gtcaca_scatter_draw(self.ptr) };
    }
}
impl_widget!(Scatter);
