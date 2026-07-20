//! Build script for gtcaca-sys.
//!
//! 1. Find libcaca via pkg-config (the one external C dependency).
//! 2. Compile the GTCaca C sources into a static lib with `cc`.
//! 3. Run bindgen over wrapper.h to emit the raw FFI.
//!
//! Source location: for a checkout, the C sources live at the repo root
//! (../../../src relative to this crate). For a published crate they are copied
//! into ./vendor by ../scripts/sync-sources.sh; we prefer that when present.

use std::env;
use std::path::PathBuf;

// The GTCaca C translation units compiled into the extension. MUST mirror
// GTCACA_C_SOURCES in src/bindings/python/CMakeLists.txt and the list in
// src/CMakeLists.txt — kept explicit (not globbed) so adding a source is a
// deliberate act. When you add a widget's .c here, add its header to wrapper.h.
const C_SOURCES: &[&str] = &[
    "main.c", "iniparse.c", "log.c",
    "application.c", "button.c", "checkbox.c", "combobox.c", "entry.c",
    "label.c", "menu.c", "progressbar.c", "radiobutton.c", "statusbar.c",
    "textlist.c", "textview.c", "theme.c", "widget.c", "window.c", "image.c",
    "spinner.c", "scale.c", "spinbutton.c", "switch.c", "frame.c",
    "separator.c", "expander.c", "box.c",
    "editor.c", "editor_lang.c", "editor_autoc.c", "editor_view.c",
    "editor_json.c", "editor_tm.c", "editor_search.c", "editor_ops.c",
    "json.c", "sparkline.c", "gauge.c", "barchart.c", "tree.c", "table.c",
    "map.c", "tabs.c", "mindmap.c", "segdisplay.c", "linechart.c", "hexview.c",
    "calendar.c", "dialog.c", "filechooser.c", "custom.c", "colordialog.c",
];

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());

    // Prefer vendored sources (published crate); fall back to the repo tree.
    let vendor = manifest_dir.join("vendor");
    let src_root = if vendor.join("src").exists() {
        vendor.join("src")
    } else {
        // src/bindings/rust/gtcaca-sys -> repo root is four levels up.
        manifest_dir
            .join("../../../..")
            .canonicalize()
            .expect("cannot resolve repo root")
            .join("src")
    };
    let include_dir = src_root.join("include");

    // --- 1. libcaca ---------------------------------------------------------
    let caca = pkg_config::Config::new()
        .probe("caca")
        .expect("libcaca not found via pkg-config. Install it: \
                 `apt install libcaca-dev`, `dnf install libcaca-devel`, \
                 or `brew install libcaca`.");

    // --- 2. compile the GTCaca C sources -----------------------------------
    let mut build = cc::Build::new();
    build
        .include(&include_dir)
        .std("gnu99") // POSIX APIs (strdup, usleep) need gnu99, not strict c99
        .warnings(false);
    for inc in &caca.include_paths {
        build.include(inc);
    }
    for src in C_SOURCES {
        build.file(src_root.join(src));
    }

    // Platform + endianness defines the C sources expect (see CMakeLists).
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "macos" => { build.define("MACOS", "1"); }
        "linux" => { build.define("LINUX", "1"); }
        "windows" => { build.define("WIN32", "1"); }
        _ => {}
    }
    if env::var("CARGO_CFG_TARGET_ENDIAN").as_deref() == Ok("big") {
        build.define("HAVE_LITTLE_ENDIAN", "0").define("HAVE_BIG_ENDIAN", "1");
    } else {
        build.define("HAVE_LITTLE_ENDIAN", "1").define("HAVE_BIG_ENDIAN", "0");
    }
    build.define("GTCACA_DATA_DIR", "\"/usr/local/share/gtcaca/\"");

    build.compile("gtcaca");

    // libcaca link flags are emitted by pkg-config's .probe() above. The C
    // sources also use pthread/math on Linux.
    if target_os == "linux" {
        println!("cargo:rustc-link-lib=pthread");
        println!("cargo:rustc-link-lib=m");
    }

    // --- 3. bindgen ---------------------------------------------------------
    let mut bindgen_builder = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg(format!("-I{}", include_dir.display()))
        // Some widget structs resolve as opaque; bindgen's size/align layout
        // assertions then overflow. They add nothing to FFI correctness.
        .layout_tests(false)
        // Emit only GTCaca's own API (+ the caca types its signatures need,
        // which bindgen pulls in transitively) and the CACA_KEY_* constants.
        .allowlist_function("gtcaca_.*")
        .allowlist_type("gtcaca_.*")
        .allowlist_type("_?g(mo|tcaca)_.*")
        .allowlist_var("GTCACA_.*")
        .allowlist_var("CACA_.*")
        .allowlist_var("gmo")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()));
    for inc in &caca.include_paths {
        bindgen_builder = bindgen_builder.clang_arg(format!("-I{}", inc.display()));
    }
    let bindings = bindgen_builder.generate().expect("bindgen failed");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("failed to write bindings.rs");

    // Rebuild triggers.
    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:include={}", include_dir.display());
}
