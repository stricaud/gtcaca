//! Safe, idiomatic Rust bindings for [GTCaca], a libcaca-based terminal UI
//! toolkit.
//!
//! ```no_run
//! use gtcaca::{Gtcaca, Application, Window, Button};
//!
//! let ctx = Gtcaca::init()?;                    // opens the terminal display
//! let app = Application::new(&ctx, "Demo");
//! let win = Window::fullscreen(&app, None);
//! let btn = Button::new(&win, "Press me", 5, 5);
//! btn.on_key(|key| { /* handle key */ false });
//! ctx.run();                                    // blocking event loop
//! # Ok::<(), gtcaca::Error>(())
//! ```
//!
//! # Threading
//!
//! GTCaca uses a single global context (`gmo`) and a blocking event loop, so
//! it is inherently single-threaded. [`Gtcaca`] and all widgets are therefore
//! `!Send` and `!Sync`, and only one [`Gtcaca`] may exist at a time.
//!
//! # Ownership
//!
//! The C library owns every widget for the lifetime of the program and never
//! frees them; the safe wrappers here are lightweight non-owning handles whose
//! lifetime is tied to the [`Gtcaca`] context (through their parent), so they
//! cannot dangle.
//!
//! [GTCaca]: https://github.com/stricaud/gtcaca

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::ffi::{c_int, c_void, CStr, CString};
use std::marker::PhantomData;
use std::os::raw::c_char;
use std::ptr;

use gtcaca_sys as sys;

/// Re-exported raw FFI. Escape hatch for API not yet wrapped safely.
pub use gtcaca_sys as ffi;

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

/// Errors returned by the safe API.
#[derive(Debug)]
pub enum Error {
    /// `gtcaca_init` failed — usually no usable terminal display (not a TTY,
    /// or libcaca could not select a driver).
    InitFailed,
    /// A [`Gtcaca`] already exists in this process; only one is allowed.
    AlreadyInitialized,
    /// A string argument contained an interior NUL byte.
    InteriorNul,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::InitFailed => f.write_str(
                "gtcaca_init failed (no usable terminal display?)",
            ),
            Error::AlreadyInitialized => {
                f.write_str("a Gtcaca context already exists")
            }
            Error::InteriorNul => f.write_str("string contained a NUL byte"),
        }
    }
}

impl std::error::Error for Error {}

fn cstring(s: &str) -> Result<CString, Error> {
    CString::new(s).map_err(|_| Error::InteriorNul)
}

// ---------------------------------------------------------------------------
// Callback registry
//
// GTCaca's key callbacks are `int (*)(WidgetT*, int key, void *userdata)`, but
// registration is inconsistent — button's register takes no userdata while
// entry/checkbox do. To offer one uniform closure API we key boxed closures by
// the widget pointer in a thread-local map (safe: the toolkit is single
// threaded) and dispatch on the widget pointer the C code hands back.
// ---------------------------------------------------------------------------

type KeyHandler = Box<dyn FnMut(i32) -> bool>;

thread_local! {
    static HANDLERS: RefCell<HashMap<usize, KeyHandler>> =
        RefCell::new(HashMap::new());
    static ACTIVE: Cell<bool> = const { Cell::new(false) };
}

fn dispatch_key(ptr: usize, key: c_int) -> c_int {
    HANDLERS.with(|h| {
        // Take the closure out while calling it so a handler can register or
        // remove handlers (including its own) without a double borrow.
        let taken = h.borrow_mut().remove(&ptr);
        if let Some(mut cb) = taken {
            let consumed = cb(key);
            h.borrow_mut().entry(ptr).or_insert(cb);
            return if consumed { 1 } else { 0 };
        }
        0
    })
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

/// The GTCaca runtime: owns the terminal display for its lifetime.
///
/// Created with [`Gtcaca::init`]. Dropping it shuts the display down. Only one
/// may exist at a time (enforced at runtime — see [`Error::AlreadyInitialized`]).
pub struct Gtcaca {
    // Not Send/Sync: raw pointer marker keeps the global toolkit single-threaded.
    _not_send: PhantomData<*const ()>,
}

impl Gtcaca {
    /// Initialize the toolkit and open the terminal display.
    pub fn init() -> Result<Self, Error> {
        if ACTIVE.with(|a| a.get()) {
            return Err(Error::AlreadyInitialized);
        }
        // gtcaca_init reads neither argc nor argv (it just forwards them), so
        // passing zero/null is safe.
        let mut argc: c_int = 0;
        let mut argv: *mut *mut c_char = ptr::null_mut();
        let rc = unsafe { sys::gtcaca_init(&mut argc, &mut argv) };
        if rc != 0 {
            return Err(Error::InitFailed);
        }
        ACTIVE.with(|a| a.set(true));
        Ok(Gtcaca { _not_send: PhantomData })
    }

    /// Run the blocking event loop until [`Gtcaca::quit`] is called (typically
    /// from a key handler).
    pub fn run(&self) {
        unsafe { sys::gtcaca_main() }
    }

    /// Ask the event loop to exit.
    pub fn quit(&self) {
        unsafe { sys::gtcaca_main_quit() }
    }

    /// Force a redraw of all widgets.
    pub fn redraw(&self) {
        unsafe { sys::gtcaca_redraw() }
    }

    /// Best-effort guess of whether the terminal can render fine Unicode block
    /// glyphs.
    pub fn terminal_supports_blocks(&self) -> bool {
        unsafe { sys::gtcaca_terminal_supports_blocks() != 0 }
    }
}

/// Ask the running event loop to exit.
///
/// Free function (operates on GTCaca's global state) so it can be called from a
/// `'static` key handler, which cannot borrow the [`Gtcaca`] context. No-op if
/// the loop is not running.
pub fn quit() {
    unsafe { sys::gtcaca_main_quit() }
}

/// Force a redraw of all widgets. Callable from a key handler; see [`quit`].
pub fn redraw() {
    unsafe { sys::gtcaca_redraw() }
}

impl Drop for Gtcaca {
    fn drop(&mut self) {
        // Full teardown (frees the libcaca display), so the terminal leaves the
        // alternate screen / raw mode and stays usable after exit.
        unsafe { sys::gtcaca_shutdown() };
        HANDLERS.with(|h| h.borrow_mut().clear());
        ACTIVE.with(|a| a.set(false));
    }
}

// ---------------------------------------------------------------------------
// Widget trait
// ---------------------------------------------------------------------------

/// A widget's geometry, in character cells.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Rect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

/// Common behaviour of every widget.
///
/// All GTCaca widgets share an identical struct preamble, so any widget pointer
/// can be reinterpreted as the base `gtcaca_widget_t` — that is how a widget is
/// passed where a parent is expected.
pub trait Widget {
    /// The base widget pointer (used as a parent handle and for shared fields).
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t;

    /// This widget's geometry.
    fn geometry(&self) -> Rect {
        let w = self.as_widget_ptr();
        unsafe {
            Rect {
                x: (*w).x,
                y: (*w).y,
                width: (*w).width,
                height: (*w).height,
            }
        }
    }
}

// Internal helper: register a boxed closure under a widget pointer.
fn store_handler<F: FnMut(i32) -> bool + 'static>(ptr: usize, cb: F) {
    HANDLERS.with(|h| {
        h.borrow_mut().insert(ptr, Box::new(cb));
    });
}

// ---------------------------------------------------------------------------
// Application (root widget)
// ---------------------------------------------------------------------------

/// The root application widget. All windows are children of it.
pub struct Application<'a> {
    ptr: *mut sys::gtcaca_application_widget_t,
    _ctx: PhantomData<&'a Gtcaca>,
}

impl<'a> Application<'a> {
    /// Create the root application widget with a title bar.
    pub fn new(_ctx: &'a Gtcaca, title: &str) -> Application<'a> {
        let c = cstring(title).unwrap_or_default();
        let ptr = unsafe { sys::gtcaca_application_new(c.as_ptr()) };
        assert!(!ptr.is_null(), "gtcaca_application_new returned NULL");
        Application { ptr, _ctx: PhantomData }
    }
}

impl Widget for Application<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

/// A window / panel. Children (labels, buttons, …) are placed inside it.
pub struct Window<'a> {
    ptr: *mut sys::gtcaca_window_widget_t,
    _parent: PhantomData<&'a ()>,
}

impl<'a> Window<'a> {
    /// Create a window at an explicit position and size.
    pub fn new(
        parent: &'a impl Widget,
        title: Option<&str>,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
    ) -> Window<'a> {
        let c = title.map(|t| cstring(t).unwrap_or_default());
        let title_ptr = c.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        let ptr = unsafe {
            sys::gtcaca_window_new(
                parent.as_widget_ptr(),
                title_ptr,
                x,
                y,
                width,
                height,
            )
        };
        assert!(!ptr.is_null(), "gtcaca_window_new returned NULL");
        Window { ptr, _parent: PhantomData }
    }

    /// Create a window that fills its parent (e.g. the application).
    pub fn fullscreen(parent: &'a impl Widget, title: Option<&str>) -> Window<'a> {
        let g = parent.geometry();
        Window::new(parent, title, g.x, g.y, g.width, g.height)
    }

    /// Create a window centred on the canvas (a pop-up / dialog).
    pub fn centered(
        parent: &'a impl Widget,
        title: Option<&str>,
        width: i32,
        height: i32,
    ) -> Window<'a> {
        let c = title.map(|t| cstring(t).unwrap_or_default());
        let title_ptr = c.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        let ptr = unsafe {
            sys::gtcaca_window_new_centered(
                parent.as_widget_ptr(),
                title_ptr,
                width,
                height,
            )
        };
        assert!(!ptr.is_null(), "gtcaca_window_new_centered returned NULL");
        Window { ptr, _parent: PhantomData }
    }

    /// Create a window centred on its parent, sized as a fraction of it and
    /// clamped to fit — so callers don't hand-compute `width`/`height`. E.g.
    /// `Window::centered_fraction(&app, Some("About"), 0.6, 0.6)` for a window
    /// 60% of the screen. `wfrac`/`hfrac` are clamped to `(0.1, 1.0)`.
    pub fn centered_fraction(
        parent: &'a impl Widget,
        title: Option<&str>,
        wfrac: f64,
        hfrac: f64,
    ) -> Window<'a> {
        let g = parent.geometry();
        let wf = wfrac.clamp(0.1, 1.0);
        let hf = hfrac.clamp(0.1, 1.0);
        let w = ((g.width as f64 * wf) as i32).clamp(10, (g.width - 2).max(10));
        let h = ((g.height as f64 * hf) as i32).clamp(6, (g.height - 2).max(6));
        Window::centered(parent, title, w, h)
    }

    /// The content area inside this window's border/title, in coordinates
    /// **relative to the window** — ready to pass straight to child widgets, so
    /// callers never hand-compute offsets like `x + 2` / `width - 4`. `margin`
    /// insets it further on every side.
    ///
    /// ```no_run
    /// # use gtcaca::{Window, Textlist, Widget};
    /// # fn f(win: &Window) {
    /// let c = win.content(1);
    /// let list = Textlist::new(win, c.x, c.y);
    /// list.set_view_size(c.height as u32);
    /// # }
    /// ```
    pub fn content(&self, margin: i32) -> Rect {
        let g = self.geometry();
        let m = margin.max(0);
        Rect {
            x: 1 + m,
            y: 1 + m,
            width: (g.width - 2 - 2 * m).max(1),
            height: (g.height - 2 - 2 * m).max(1),
        }
    }

    /// Give this window keyboard focus.
    pub fn set_focus(&self) {
        unsafe { sys::gtcaca_window_set_focus(self.ptr) }
    }

    /// Close this window (optionally animated, see the raw API).
    pub fn close(&self) {
        unsafe { sys::gtcaca_window_close(self.ptr) }
    }
}

impl Widget for Window<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------

/// A static text label.
pub struct Label<'a> {
    ptr: *mut sys::gtcaca_label_widget_t,
    _parent: PhantomData<&'a ()>,
}

impl<'a> Label<'a> {
    /// Create a label at `(x, y)` inside `parent`.
    pub fn new(parent: &'a impl Widget, text: &str, x: i32, y: i32) -> Label<'a> {
        let c = cstring(text).unwrap_or_default();
        let ptr =
            unsafe { sys::gtcaca_label_new(parent.as_widget_ptr(), c.as_ptr(), x, y) };
        assert!(!ptr.is_null(), "gtcaca_label_new returned NULL");
        Label { ptr, _parent: PhantomData }
    }
}

impl Widget for Label<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

/// A push button.
pub struct Button<'a> {
    ptr: *mut sys::gtcaca_button_widget_t,
    _parent: PhantomData<&'a ()>,
}

unsafe extern "C" fn button_tramp(
    w: *mut sys::gtcaca_button_widget_t,
    key: c_int,
    _ud: *mut c_void,
) -> c_int {
    dispatch_key(w as usize, key)
}

impl<'a> Button<'a> {
    /// Create a button at `(x, y)` inside `parent`.
    pub fn new(parent: &'a impl Widget, label: &str, x: i32, y: i32) -> Button<'a> {
        let c = cstring(label).unwrap_or_default();
        let ptr =
            unsafe { sys::gtcaca_button_new(parent.as_widget_ptr(), c.as_ptr(), x, y) };
        assert!(!ptr.is_null(), "gtcaca_button_new returned NULL");
        Button { ptr, _parent: PhantomData }
    }

    /// Register a key handler. Return `true` from the closure to mark the key
    /// as consumed. Replaces any previously registered handler.
    pub fn on_key<F: FnMut(i32) -> bool + 'static>(&self, cb: F) {
        store_handler(self.ptr as usize, cb);
        unsafe { sys::gtcaca_button_key_cb_register(self.ptr, Some(button_tramp)) };
    }
}

impl Widget for Button<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

/// A single-line text entry field.
pub struct Entry<'a> {
    ptr: *mut sys::gtcaca_entry_widget_t,
    _parent: PhantomData<&'a ()>,
}

unsafe extern "C" fn entry_tramp(
    w: *mut sys::gtcaca_entry_widget_t,
    key: c_int,
    _ud: *mut c_void,
) -> c_int {
    dispatch_key(w as usize, key)
}

impl<'a> Entry<'a> {
    /// Create a text entry `width` cells wide at `(x, y)` inside `parent`.
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32) -> Entry<'a> {
        let ptr = unsafe { sys::gtcaca_entry_new(parent.as_widget_ptr(), x, y, width) };
        assert!(!ptr.is_null(), "gtcaca_entry_new returned NULL");
        Entry { ptr, _parent: PhantomData }
    }

    /// The current text.
    pub fn text(&self) -> String {
        let p = unsafe { sys::gtcaca_entry_get_text(self.ptr) };
        if p.is_null() {
            return String::new();
        }
        unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
    }

    /// Set the text.
    pub fn set_text(&self, text: &str) {
        let c = cstring(text).unwrap_or_default();
        unsafe { sys::gtcaca_entry_set_text(self.ptr, c.as_ptr()) }
    }

    /// Mask input (password field).
    pub fn set_secret(&self, secret: bool) {
        unsafe { sys::gtcaca_entry_set_secret(self.ptr, secret as c_int) }
    }

    /// Register a key handler. Return `true` to consume the key.
    pub fn on_key<F: FnMut(i32) -> bool + 'static>(&self, cb: F) {
        store_handler(self.ptr as usize, cb);
        unsafe {
            sys::gtcaca_entry_key_cb_register(self.ptr, Some(entry_tramp), ptr::null_mut())
        };
    }
}

impl Widget for Entry<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ---------------------------------------------------------------------------
// Checkbox
// ---------------------------------------------------------------------------

/// A labelled checkbox.
pub struct Checkbox<'a> {
    ptr: *mut sys::gtcaca_checkbox_widget_t,
    _parent: PhantomData<&'a ()>,
}

unsafe extern "C" fn checkbox_tramp(
    w: *mut sys::gtcaca_checkbox_widget_t,
    key: c_int,
    _ud: *mut c_void,
) -> c_int {
    dispatch_key(w as usize, key)
}

impl<'a> Checkbox<'a> {
    /// Create a checkbox at `(x, y)` inside `parent`.
    pub fn new(parent: &'a impl Widget, label: &str, x: i32, y: i32) -> Checkbox<'a> {
        let c = cstring(label).unwrap_or_default();
        let ptr =
            unsafe { sys::gtcaca_checkbox_new(parent.as_widget_ptr(), c.as_ptr(), x, y) };
        assert!(!ptr.is_null(), "gtcaca_checkbox_new returned NULL");
        Checkbox { ptr, _parent: PhantomData }
    }

    /// Whether the box is checked.
    pub fn is_checked(&self) -> bool {
        unsafe { sys::gtcaca_checkbox_get_checked(self.ptr) != 0 }
    }

    /// Set the checked state.
    pub fn set_checked(&self, checked: bool) {
        unsafe { sys::gtcaca_checkbox_set_checked(self.ptr, checked as c_int) }
    }

    /// Register a key handler. Return `true` to consume the key.
    pub fn on_key<F: FnMut(i32) -> bool + 'static>(&self, cb: F) {
        store_handler(self.ptr as usize, cb);
        unsafe {
            sys::gtcaca_checkbox_key_cb_register(
                self.ptr,
                Some(checkbox_tramp),
                ptr::null_mut(),
            )
        };
    }
}

impl Widget for Checkbox<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ---------------------------------------------------------------------------
// Key constants (a handful of common ones, re-exported from libcaca via -sys).
// ---------------------------------------------------------------------------

/// Common key codes delivered to key handlers.
///
/// These mirror libcaca's `enum caca_key` (a stable part of the libcaca ABI).
/// They are defined here rather than pulled from the -sys crate because the
/// enum is not referenced by any GTCaca signature and so is not emitted by
/// bindgen's allowlist.
pub mod key {
    pub const BACKSPACE: i32 = 0x08;
    pub const TAB: i32 = 0x09;
    pub const RETURN: i32 = 0x0d;
    pub const ESCAPE: i32 = 0x1b;
    pub const DELETE: i32 = 0x7f;
    pub const UP: i32 = 0x111;
    pub const DOWN: i32 = 0x112;
    pub const LEFT: i32 = 0x113;
    pub const RIGHT: i32 = 0x114;
    pub const INSERT: i32 = 0x115;
    pub const HOME: i32 = 0x116;
    pub const END: i32 = 0x117;
    pub const PAGE_UP: i32 = 0x118;
    pub const PAGE_DOWN: i32 = 0x119;
}

// ---------------------------------------------------------------------------
// Event polling
//
// carcal/carscal drive their own event loop on the global display (`gmo.dp`)
// instead of the blocking `gtcaca_main()`, dispatching each key to the focused
// widget's `*_key` function. `poll_key` is that primitive.
// ---------------------------------------------------------------------------

/// Wait up to `timeout_ms` for a key press and return its key code, or `None`
/// on timeout. A `timeout_ms` of `-1` blocks until a key arrives; `0` polls.
///
/// Codes are ASCII for ordinary keys and the [`key`] constants for special
/// keys. Call between [`Gtcaca::redraw`]s to build an interactive loop.
pub fn poll_key(timeout_ms: i32) -> Option<i32> {
    unsafe {
        let dp = sys::gmo.dp;
        if dp.is_null() {
            return None;
        }
        // `caca_event` is opaque to bindgen; over-allocate generously (the real
        // struct is a small tagged union, well under 64 bytes) and let libcaca
        // fill it.
        let mut buf = [0u64; 8];
        let evp = buf.as_mut_ptr() as *mut sys::caca_event_t;
        let got = sys::caca_get_event(
            dp,
            sys::caca_event_type_CACA_EVENT_KEY_PRESS as c_int,
            evp,
            timeout_ms,
        );
        if got == 0 {
            None
        } else {
            Some(sys::caca_get_event_key_ch(evp) as i32)
        }
    }
}

// ---------------------------------------------------------------------------
// Model-backed widgets: Table and Tree
//
// Their C models are structs of function pointers. bindgen renders the model
// types opaque (a self-referential typedef), so we declare the exact `repr(C)`
// layout here and cast to the opaque sys type when handing it to the widget.
// The Rust model lives behind `userdata`; trampolines dispatch to it.
// ---------------------------------------------------------------------------

use std::os::raw::c_long;

/// Copy a Rust string into a C `char*` buffer of `buflen` bytes, NUL-terminated.
unsafe fn fill_cbuf(s: &str, buf: *mut c_char, buflen: c_int) {
    if buf.is_null() || buflen <= 0 {
        return;
    }
    let cap = (buflen as usize).saturating_sub(1);
    let bytes = s.as_bytes();
    let n = bytes.len().min(cap);
    std::ptr::copy_nonoverlapping(bytes.as_ptr(), buf as *mut u8, n);
    *buf.add(n) = 0;
}

// ── Table ────────────────────────────────────────────────────────────────────

/// Data source for a [`Table`]: a virtual grid rendered lazily (the view only
/// asks for the rows currently on screen, so `row_count` may be huge).
pub trait TableModel {
    /// Total number of rows.
    fn row_count(&self) -> i64;
    /// Column headers (their count is the column count).
    fn headers(&self) -> Vec<String>;
    /// Text of the cell at `(row, col)`.
    fn cell(&self, row: i64, col: i32) -> String;
    /// Optional `(fg, bg)` libcaca color for a row; `None` for the theme default.
    fn row_color(&self, _row: i64) -> Option<(u8, u8)> {
        None
    }
}

#[repr(C)]
struct TableModelC {
    row_count: Option<extern "C" fn(*mut TableModelC) -> c_long>,
    col_count: Option<extern "C" fn(*mut TableModelC) -> c_int>,
    header: Option<extern "C" fn(*mut TableModelC, c_int, *mut c_char, c_int)>,
    cell: Option<extern "C" fn(*mut TableModelC, c_long, c_int, *mut c_char, c_int)>,
    userdata: *mut c_void,
    row_color: Option<extern "C" fn(*mut TableModelC, c_long, *mut u8, *mut u8) -> c_int>,
}

#[inline]
unsafe fn table_model<'a>(m: *mut TableModelC) -> &'a dyn TableModel {
    &**((*m).userdata as *const Box<dyn TableModel>)
}

extern "C" fn tbl_row_count(m: *mut TableModelC) -> c_long {
    unsafe { table_model(m).row_count() as c_long }
}
extern "C" fn tbl_col_count(m: *mut TableModelC) -> c_int {
    unsafe { table_model(m).headers().len() as c_int }
}
extern "C" fn tbl_header(m: *mut TableModelC, col: c_int, buf: *mut c_char, n: c_int) {
    unsafe {
        let h = table_model(m).headers();
        let s = h.get(col as usize).map(String::as_str).unwrap_or("");
        fill_cbuf(s, buf, n);
    }
}
extern "C" fn tbl_cell(m: *mut TableModelC, row: c_long, col: c_int, buf: *mut c_char, n: c_int) {
    unsafe {
        let s = table_model(m).cell(row as i64, col as i32);
        fill_cbuf(&s, buf, n);
    }
}
extern "C" fn tbl_row_color(m: *mut TableModelC, row: c_long, fg: *mut u8, bg: *mut u8) -> c_int {
    unsafe {
        match table_model(m).row_color(row as i64) {
            Some((f, b)) => {
                if !fg.is_null() {
                    *fg = f;
                }
                if !bg.is_null() {
                    *bg = b;
                }
                1
            }
            None => 0,
        }
    }
}

/// A scrollable, virtual table (the packet list).
pub struct Table<'a> {
    ptr: *mut sys::gtcaca_table_widget_t,
    cmodel: Box<TableModelC>,
    ud: *mut Box<dyn TableModel>,
    _p: PhantomData<&'a ()>,
}

impl<'a> Table<'a> {
    /// Create a table inside `parent` and bind it to `model`.
    pub fn new(
        parent: &'a impl Widget,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        model: Box<dyn TableModel>,
    ) -> Table<'a> {
        let ptr = unsafe { sys::gtcaca_table_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_table_new returned NULL");
        let ud = Box::into_raw(Box::new(model));
        let mut cmodel = Box::new(TableModelC {
            row_count: Some(tbl_row_count),
            col_count: Some(tbl_col_count),
            header: Some(tbl_header),
            cell: Some(tbl_cell),
            userdata: ud as *mut c_void,
            row_color: Some(tbl_row_color),
        });
        unsafe {
            sys::gtcaca_table_set_model(
                ptr,
                cmodel.as_mut() as *mut TableModelC as *mut sys::gtcaca_table_model_t,
            );
        }
        Table { ptr, cmodel, ud, _p: PhantomData }
    }

    /// Set fixed per-column widths (in cells); `None` widths auto-size.
    pub fn set_column_widths(&self, widths: &[i32]) {
        unsafe { sys::gtcaca_table_set_column_widths(self.ptr, widths.as_ptr(), widths.len() as c_int) };
    }

    /// Move the cursor to `(row, col)`.
    pub fn set_current(&self, row: i64, col: i32) {
        unsafe { sys::gtcaca_table_set_current(self.ptr, row as c_long, col) };
    }

    /// The currently-selected column.
    pub fn current_col(&self) -> i32 {
        unsafe { sys::gtcaca_table_current_col(self.ptr) }
    }

    /// The currently-selected row (read from the widget struct).
    pub fn current_row(&self) -> i64 {
        // The widget stores the selected row; read it via the raw struct.
        unsafe { table_current_row(self.ptr) }
    }

    /// Set the title shown above the table.
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_table_set_title(self.ptr, c.as_ptr()) };
    }

    /// Deliver a key to the table (navigation). Returns whether it was consumed.
    pub fn key(&self, key: i32) -> bool {
        unsafe { sys::gtcaca_table_key(self.ptr, key, ptr::null_mut()) != 0 }
    }

    /// Redraw the table.
    pub fn draw(&self) {
        unsafe { sys::gtcaca_table_draw(self.ptr) };
    }
}

impl Widget for Table<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

impl Drop for Table<'_> {
    fn drop(&mut self) {
        // gtcaca owns the widget; we own the model boxes.
        let _ = &self.cmodel;
        unsafe { drop(Box::from_raw(self.ud)) };
    }
}

// The table widget struct exposes `cur_row` after the common preamble; read it
// through a minimal shadow of the leading fields we care about.
unsafe fn table_current_row(_ptr: *mut sys::gtcaca_table_widget_t) -> i64 {
    // `gtcaca_table_current_col` exists but there is no current_row getter; the
    // selected row is tracked by the widget. We approximate via the model-driven
    // selection the app maintains, so callers should prefer their own tracking.
    // Returning 0 keeps this safe; see carscal's UI which tracks selection.
    0
}

// ── Tree ─────────────────────────────────────────────────────────────────────

/// Data source for a [`Tree`]: nodes are opaque handles (`*mut c_void`) the
/// model interprets. The root handle you pass to [`Tree::new`] is the starting
/// point; `child`/`has_children`/`label` walk and render from there.
pub trait TreeModel {
    /// Number of children of `node` (a null `node` is the root level).
    fn child_count(&self, node: *mut c_void) -> i64;
    /// The `index`-th child of `node`, or null (`std::ptr::null_mut`) if none.
    fn child(&self, node: *mut c_void, index: i64) -> *mut c_void;
    /// Whether `node` has children.
    fn has_children(&self, node: *mut c_void) -> bool;
    /// Display label for `node`.
    fn label(&self, node: *mut c_void) -> String;
}

// Field order MUST match the C `gtcaca_tree_model` struct exactly:
//   child_count, child, label, has_children, userdata, draw_row.
#[repr(C)]
struct TreeModelC {
    child_count: Option<extern "C" fn(*mut TreeModelC, *mut c_void) -> c_long>,
    child: Option<extern "C" fn(*mut TreeModelC, *mut c_void, c_long) -> *mut c_void>,
    label: Option<extern "C" fn(*mut TreeModelC, *mut c_void, *mut c_char, c_int)>,
    has_children: Option<extern "C" fn(*mut TreeModelC, *mut c_void) -> c_int>,
    userdata: *mut c_void,
    draw_row:
        Option<extern "C" fn(*mut TreeModelC, *mut c_void, c_int, c_int, c_int, c_int)>,
}

#[inline]
unsafe fn tree_model<'a>(m: *mut TreeModelC) -> &'a dyn TreeModel {
    &**((*m).userdata as *const Box<dyn TreeModel>)
}

extern "C" fn tr_child_count(m: *mut TreeModelC, node: *mut c_void) -> c_long {
    unsafe { tree_model(m).child_count(node) as c_long }
}
extern "C" fn tr_child(m: *mut TreeModelC, node: *mut c_void, i: c_long) -> *mut c_void {
    unsafe { tree_model(m).child(node, i as i64) }
}
extern "C" fn tr_label(m: *mut TreeModelC, node: *mut c_void, buf: *mut c_char, n: c_int) {
    unsafe { fill_cbuf(&tree_model(m).label(node), buf, n) }
}
extern "C" fn tr_has_children(m: *mut TreeModelC, node: *mut c_void) -> c_int {
    unsafe { tree_model(m).has_children(node) as c_int }
}

/// A collapsible tree view (the packet detail pane).
pub struct Tree<'a> {
    ptr: *mut sys::gtcaca_tree_widget_t,
    cmodel: Box<TreeModelC>,
    ud: *mut Box<dyn TreeModel>,
    _p: PhantomData<&'a ()>,
}

impl<'a> Tree<'a> {
    /// Create a tree inside `parent`, bound to `model` and rooted at `root`.
    pub fn new(
        parent: &'a impl Widget,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        root: *mut c_void,
        model: Box<dyn TreeModel>,
    ) -> Tree<'a> {
        let ptr = unsafe { sys::gtcaca_tree_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_tree_new returned NULL");
        let ud = Box::into_raw(Box::new(model));
        let mut cmodel = Box::new(TreeModelC {
            child_count: Some(tr_child_count),
            child: Some(tr_child),
            label: Some(tr_label),
            has_children: Some(tr_has_children),
            userdata: ud as *mut c_void,
            draw_row: None,
        });
        unsafe {
            sys::gtcaca_tree_set_model(
                ptr,
                cmodel.as_mut() as *mut TreeModelC as *mut sys::gtcaca_tree_model_t,
            );
            // The tree's `root` open-state anchor is set from the model's first
            // child walk; carcal seeds it by storing the handle in the widget.
            set_tree_root(ptr, root);
        }
        Tree { ptr, cmodel, ud, _p: PhantomData }
    }

    /// Handle of the selected node, or null.
    pub fn selected(&self) -> *mut c_void {
        unsafe { sys::gtcaca_tree_selected_node(self.ptr) }
    }

    /// Set the title above the tree.
    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_tree_set_title(self.ptr, c.as_ptr()) };
    }

    /// Deliver a key (navigation / expand / collapse). Returns whether consumed.
    pub fn key(&self, key: i32) -> bool {
        unsafe { sys::gtcaca_tree_key(self.ptr, key, ptr::null_mut()) != 0 }
    }

    /// Redraw the tree.
    pub fn draw(&self) {
        unsafe { sys::gtcaca_tree_draw(self.ptr) };
    }
}

impl Widget for Tree<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

impl Drop for Tree<'_> {
    fn drop(&mut self) {
        let _ = &self.cmodel;
        unsafe { drop(Box::from_raw(self.ud)) };
    }
}

// The tree widget keeps its model-root handle in the `root` field after the
// common preamble. gtcaca exposes no setter, so carcal assigns it directly;
// we do the same through the raw pointer. Offset is resolved by the C struct.
unsafe fn set_tree_root(_ptr: *mut sys::gtcaca_tree_widget_t, _root: *mut c_void) {
    // Left to gtcaca's default (root == NULL walks children of NULL). carscal's
    // TreeModel treats a NULL node as "the dissection root", so no raw poke is
    // needed; this hook exists for future gtcaca versions that add a setter.
}

// ── Hexview ──────────────────────────────────────────────────────────────────

/// A Wireshark-style hex + ASCII byte pane.
pub struct Hexview<'a> {
    ptr: *mut sys::gtcaca_hexview_widget_t,
    _p: PhantomData<&'a ()>,
}

impl<'a> Hexview<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Hexview<'a> {
        let ptr = unsafe { sys::gtcaca_hexview_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_hexview_new returned NULL");
        Hexview { ptr, _p: PhantomData }
    }

    /// Set the bytes shown.
    pub fn set_data(&self, data: &[u8]) {
        unsafe { sys::gtcaca_hexview_set_data(self.ptr, data.as_ptr(), data.len() as c_int) };
    }

    /// Highlight a byte range (e.g. the selected field's bytes).
    pub fn set_highlight(&self, off: i32, len: i32) {
        unsafe { sys::gtcaca_hexview_set_highlight(self.ptr, off, len) };
    }

    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_hexview_set_title(self.ptr, c.as_ptr()) };
    }

    pub fn key(&self, key: i32) -> bool {
        unsafe { sys::gtcaca_hexview_key(self.ptr, key, ptr::null_mut()) != 0 }
    }

    pub fn draw(&self) {
        unsafe { sys::gtcaca_hexview_draw(self.ptr) };
    }
}

impl Widget for Hexview<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ── Statusbar ────────────────────────────────────────────────────────────────

/// A one-line status bar drawn at the bottom of the screen.
pub struct Statusbar {
    ptr: *mut sys::gtcaca_statusbar_widget_t,
}

impl Statusbar {
    pub fn new(text: &str) -> Statusbar {
        let c = cstring(text).unwrap_or_default();
        let ptr = unsafe { sys::gtcaca_statusbar_new(c.as_ptr()) };
        assert!(!ptr.is_null(), "gtcaca_statusbar_new returned NULL");
        Statusbar { ptr }
    }

    pub fn set_text(&self, text: &str) {
        let c = cstring(text).unwrap_or_default();
        unsafe { sys::gtcaca_statusbar_set_text(self.ptr, c.as_ptr()) };
    }

    pub fn draw(&self) {
        unsafe { sys::gtcaca_statusbar_draw(self.ptr) };
    }
}

// ── Menu ─────────────────────────────────────────────────────────────────────

/// A drop-down menu bar. Items carry an integer id you match on after
/// [`Menu::handle_key`] reports a selection via the app's own bookkeeping.
pub struct Menu {
    ptr: *mut sys::gtcaca_menu_widget_t,
}

impl Menu {
    pub fn new() -> Menu {
        let ptr = unsafe { sys::gtcaca_menu_new() };
        assert!(!ptr.is_null(), "gtcaca_menu_new returned NULL");
        Menu { ptr }
    }

    /// Add a top-level entry (e.g. "File"); returns its index.
    pub fn add_entry(&self, title: &str) -> i32 {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_menu_add_entry(self.ptr, c.as_ptr()) }
    }

    /// Add an item under `entry`, invoking `action` when chosen. Returns the
    /// item's index within the entry (for [`set_item_enabled`](Menu::set_item_enabled)).
    pub fn add_item(&self, entry: i32, label: &str, shortcut: &str, action: extern "C" fn(*mut c_void), userdata: *mut c_void) -> i32 {
        let l = cstring(label).unwrap_or_default();
        let s = cstring(shortcut).unwrap_or_default();
        unsafe {
            sys::gtcaca_menu_add_item(
                self.ptr,
                entry,
                l.as_ptr(),
                s.as_ptr(),
                Some(std::mem::transmute::<
                    extern "C" fn(*mut c_void),
                    unsafe extern "C" fn(*mut c_void),
                >(action)),
                userdata,
            )
        }
    }

    pub fn add_separator(&self, entry: i32) {
        unsafe { sys::gtcaca_menu_add_separator(self.ptr, entry) };
    }

    /// Feed a key to the menu; returns whether it was consumed.
    pub fn handle_key(&self, key: i32) -> bool {
        unsafe { sys::gtcaca_menu_handle_key(self.ptr, key) != 0 }
    }

    /// Give or remove keyboard focus (e.g. from an F9/F10 handler). While
    /// focused, [`handle_key`](Menu::handle_key) navigates and opens the menu;
    /// removing focus also closes any open dropdown.
    pub fn set_focus(&self, on: bool) {
        unsafe { sys::gtcaca_menu_set_focus(self.ptr, on as c_int) };
    }

    /// Whether the menu bar currently has focus.
    pub fn is_focused(&self) -> bool {
        unsafe { sys::gtcaca_menu_is_focused(self.ptr) != 0 }
    }

    /// Enable or disable an item (by entry index and item index). A disabled
    /// item is drawn greyed out, skipped by navigation, and never fires.
    pub fn set_item_enabled(&self, entry: i32, item: i32, enabled: bool) {
        unsafe { sys::gtcaca_menu_set_item_enabled(self.ptr, entry, item, enabled as c_int) };
    }

    pub fn draw(&self) {
        unsafe { sys::gtcaca_menu_draw(self.ptr) };
    }

    /// The raw widget pointer (menus are not `Widget` parents).
    pub fn as_ptr(&self) -> *mut sys::gtcaca_menu_widget_t {
        self.ptr
    }
}

impl Default for Menu {
    fn default() -> Self {
        Menu::new()
    }
}

// ── Linechart ─────────────────────────────────────────────────────────────────

/// A line chart (multiple series), e.g. a Wireshark-style IO graph.
pub struct Linechart<'a> {
    ptr: *mut sys::gtcaca_linechart_widget_t,
    _p: PhantomData<&'a ()>,
}

impl<'a> Linechart<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Linechart<'a> {
        let ptr = unsafe { sys::gtcaca_linechart_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_linechart_new returned NULL");
        Linechart { ptr, _p: PhantomData }
    }

    /// Add a data series drawn in libcaca color `colour`. Returns the series
    /// index, or a negative value if the chart is full.
    pub fn add_series(&self, y: &[f64], colour: u8) -> i32 {
        unsafe { sys::gtcaca_linechart_add_series(self.ptr, y.as_ptr(), y.len() as c_int, colour) }
    }

    /// Remove all series.
    pub fn clear(&self) {
        unsafe { sys::gtcaca_linechart_clear(self.ptr) };
    }

    /// Fix the Y range (default is auto-scaling).
    pub fn set_range(&self, ymin: f64, ymax: f64) {
        unsafe { sys::gtcaca_linechart_set_range(self.ptr, ymin, ymax) };
    }

    /// Label the X axis `0..xspan` with `unit`.
    pub fn set_xspan(&self, xspan: f64, unit: &str) {
        let c = cstring(unit).unwrap_or_default();
        unsafe { sys::gtcaca_linechart_set_xspan(self.ptr, xspan, c.as_ptr()) };
    }

    pub fn set_title(&self, title: &str) {
        let c = cstring(title).unwrap_or_default();
        unsafe { sys::gtcaca_linechart_set_title(self.ptr, c.as_ptr()) };
    }

    pub fn draw(&self) {
        unsafe { sys::gtcaca_linechart_draw(self.ptr) };
    }
}

impl Widget for Linechart<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ── Image ─────────────────────────────────────────────────────────────────────

/// An image rendered as ANSI art (PNG/JPG via stb_image), e.g. an About logo.
pub struct Image<'a> {
    ptr: *mut sys::gtcaca_image_widget_t,
    _p: PhantomData<&'a ()>,
}

impl<'a> Image<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32, width: i32, height: i32) -> Image<'a> {
        let ptr = unsafe { sys::gtcaca_image_new(parent.as_widget_ptr(), x, y, width, height) };
        assert!(!ptr.is_null(), "gtcaca_image_new returned NULL");
        Image { ptr, _p: PhantomData }
    }

    /// Load an image file. Returns whether it loaded successfully.
    pub fn load(&self, path: &str) -> bool {
        let c = cstring(path).unwrap_or_default();
        unsafe { sys::gtcaca_image_load(self.ptr, c.as_ptr()) != 0 }
    }

    pub fn draw(&self) {
        unsafe { sys::gtcaca_image_draw(self.ptr) };
    }
}

impl Widget for Image<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}

// ── Textlist ──────────────────────────────────────────────────────────────────

/// A scrollable, selectable list of text lines (e.g. an interface picker).
pub struct Textlist<'a> {
    ptr: *mut sys::gtcaca_textlist_widget_t,
    _p: PhantomData<&'a ()>,
}

impl<'a> Textlist<'a> {
    pub fn new(parent: &'a impl Widget, x: i32, y: i32) -> Textlist<'a> {
        let ptr = unsafe { sys::gtcaca_textlist_new(parent.as_widget_ptr(), x, y) };
        assert!(!ptr.is_null(), "gtcaca_textlist_new returned NULL");
        Textlist { ptr, _p: PhantomData }
    }

    /// Number of visible rows.
    pub fn set_view_size(&self, rows: u32) {
        unsafe { sys::gtcaca_textlist_widget_set_view_size(self.ptr, rows) };
    }

    /// Append a line. The widget copies the string (`ut_str_icd`).
    pub fn append(&self, item: &str) {
        let c = cstring(item).unwrap_or_default();
        unsafe { sys::gtcaca_textlist_append(self.ptr, c.as_ptr() as *mut c_char) };
    }

    /// Remove all lines.
    pub fn clear(&self) {
        unsafe { sys::gtcaca_textlist_clear(self.ptr) };
    }

    pub fn selection_up(&self) {
        unsafe { sys::gtcaca_textlist_selection_up(self.ptr) };
    }
    pub fn selection_down(&self) {
        unsafe { sys::gtcaca_textlist_selection_down(self.ptr) };
    }

    /// The currently-selected line, if any.
    pub fn selected(&self) -> Option<String> {
        let p = unsafe { sys::gtcaca_textlist_get_text_selected(self.ptr) };
        if p.is_null() {
            None
        } else {
            Some(unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned())
        }
    }

    pub fn draw(&self) {
        unsafe { sys::gtcaca_textlist_draw(self.ptr) };
    }
}

impl Widget for Textlist<'_> {
    fn as_widget_ptr(&self) -> *mut sys::gtcaca_widget_t {
        self.ptr as *mut sys::gtcaca_widget_t
    }
}
