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
        unsafe { sys::gtcaca_present_shutdown() };
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
