//! # A FreeMind mind-map viewer/editor, block by block
//!
//! A guided example: it loads a FreeMind `.mm` file (the XML format the desktop
//! app FreeMind / Freeplane save), shows it in a [`Mindmap`] widget, and lets
//! you navigate, edit and save it. Read it top to bottom to learn how to drive
//! a tree-shaped widget and build a minibuffer prompt out of an [`Entry`].
//!
//! ```text
//! cargo run --example mindmap            # a built-in sample map
//! cargo run --example mindmap plan.mm    # open a FreeMind file
//! ```
//!
//! Keys:  arrows navigate (← fold/up, → unfold/down) · Space fold ·
//! Tab add child · Enter add sibling · e edit · Delete remove · s save · q quit.

use gtcaca::{key, Application, Entry, Gtcaca, MmNode, Mindmap, Statusbar, Widget, Window};

// ─────────────────────────────────────────────────────────────────────────────
// STEP 1 — Reading FreeMind XML.
//
// A `.mm` file is nested `<node TEXT="...">…</node>` elements. We don't need a
// full XML parser: we scan for node tags, track nesting with a stack, and build
// the widget's tree with `add_child`. `<node …/>` is a leaf; `</node>` pops.
// ─────────────────────────────────────────────────────────────────────────────

/// Decode the XML entities FreeMind uses in attribute values.
fn xml_decode(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut rest = s;
    while let Some(i) = rest.find('&') {
        out.push_str(&rest[..i]);
        rest = &rest[i..];
        if let Some(semi) = rest.find(';').filter(|&j| j <= 10) {
            let ent = &rest[1..semi];
            match ent {
                "amp" => out.push('&'),
                "lt" => out.push('<'),
                "gt" => out.push('>'),
                "quot" => out.push('"'),
                "apos" => out.push('\''),
                _ if ent.starts_with("#x") || ent.starts_with("#X") => {
                    if let Ok(n) = u32::from_str_radix(&ent[2..], 16) {
                        if let Some(c) = char::from_u32(n) {
                            out.push(c);
                        }
                    }
                }
                _ if ent.starts_with('#') => {
                    if let Ok(n) = ent[1..].parse::<u32>() {
                        if let Some(c) = char::from_u32(n) {
                            out.push(c);
                        }
                    }
                }
                _ => out.push_str(&rest[..=semi]), // unknown: keep literal
            }
            rest = &rest[semi + 1..];
        } else {
            out.push('&');
            rest = &rest[1..];
        }
    }
    out.push_str(rest);
    out
}

/// Read the value of attribute `name` out of a `<node …>` tag body.
fn attr(tag: &str, name: &str) -> Option<String> {
    let needle = format!("{name}=\"");
    let start = tag.find(&needle)? + needle.len();
    let end = tag[start..].find('"')? + start;
    Some(tag[start..end].to_string())
}

/// Rebuild the mind map `m` from FreeMind XML `xml`.
fn load_mm(m: &Mindmap, xml: &str) {
    m.clear(""); // start from a single empty root
    let mut stack: Vec<MmNode> = Vec::new();
    let mut got_root = false;
    let mut rest = xml;

    while let Some(i) = rest.find('<') {
        rest = &rest[i..];
        if let Some(after) = rest.strip_prefix("<!--") {
            // skip a comment
            rest = after.find("-->").map(|e| &after[e + 3..]).unwrap_or("");
            continue;
        }
        if let Some(after) = rest.strip_prefix("</node>") {
            stack.pop();
            rest = after;
            continue;
        }
        let is_node = rest.starts_with("<node")
            && rest[5..].chars().next().is_some_and(|c| c.is_whitespace() || c == '>' || c == '/');
        if is_node {
            let Some(gt) = rest.find('>') else { break };
            let tag = &rest[..gt];
            let self_close = tag.ends_with('/');
            let text = attr(tag, "TEXT").map(|t| xml_decode(&t)).unwrap_or_default();

            // The first node becomes the (already-existing) root; the rest are
            // children of whatever node is on top of the stack.
            let node = if !got_root {
                got_root = true;
                let r = m.root();
                m.set_text(r, &text);
                r
            } else {
                let parent = stack.last().copied().unwrap_or_else(|| m.root());
                m.add_child(parent, &text)
            };
            if attr(tag, "FOLDED").as_deref() == Some("true") {
                node.set_folded(true);
            }
            if !self_close {
                stack.push(node);
            }
            rest = &rest[gt + 1..];
            continue;
        }
        rest = &rest[1..]; // some other tag — step past this '<'
    }
    m.select(m.root());
}

// ─────────────────────────────────────────────────────────────────────────────
// STEP 2 — Writing FreeMind XML (walking the widget's node tree).
//
// The widget owns the tree; we read it back through `MmNode::children` / `text`
// and emit nested `<node>` elements. This round-trips with `load_mm`.
// ─────────────────────────────────────────────────────────────────────────────

fn xml_escape(s: &str) -> String {
    s.chars()
        .map(|c| match c {
            '&' => "&amp;".into(),
            '<' => "&lt;".into(),
            '>' => "&gt;".into(),
            '"' => "&quot;".into(),
            '\n' => "&#xa;".into(),
            c => c.to_string(),
        })
        .collect()
}

fn save_node(out: &mut String, node: MmNode, depth: usize) {
    let pad = "  ".repeat(depth);
    let kids = node.children();
    out.push_str(&format!("{pad}<node TEXT=\"{}\"", xml_escape(&node.text())));
    if node.is_folded() && !kids.is_empty() {
        out.push_str(" FOLDED=\"true\"");
    }
    if kids.is_empty() {
        out.push_str("/>\n");
        return;
    }
    out.push_str(">\n");
    for k in kids {
        save_node(out, k, depth + 1);
    }
    out.push_str(&format!("{pad}</node>\n"));
}

fn save_mm(m: &Mindmap, path: &str) -> std::io::Result<()> {
    let mut out = String::from("<map version=\"1.0.1\">\n");
    save_node(&mut out, m.root(), 0);
    out.push_str("</map>\n");
    std::fs::write(path, out)
}

const SAMPLE: &str = r#"<map version="1.0.1">
<node TEXT="Project">
  <node TEXT="Design">
    <node TEXT="UI"/>
    <node TEXT="API"/>
  </node>
  <node TEXT="Build">
    <node TEXT="CI"/>
    <node TEXT="Release"/>
  </node>
  <node TEXT="Ship"/>
</node>
</map>"#;

fn main() -> Result<(), gtcaca::Error> {
    // ─────────────────────────────────────────────────────────────────────────
    // STEP 3 — Open the toolkit and create the mind-map widget.
    // ─────────────────────────────────────────────────────────────────────────
    let path = std::env::args().nth(1);
    let xml = path
        .as_ref()
        .and_then(|p| std::fs::read_to_string(p).ok())
        .unwrap_or_else(|| SAMPLE.to_string());

    let ctx = Gtcaca::init()?;
    let app = Application::new(&ctx, "gtcaca mindmap");
    let win = Window::fullscreen(&app, None);
    let a = win.content(0);

    // The map fills the window except the bottom row, which we reserve for the
    // edit prompt / status line.
    let mm = Mindmap::new(&win, a.x, a.y, a.width, a.height - 1);
    mm.set_title(path.as_deref().unwrap_or("mindmap"));
    load_mm(&mm, &xml);

    // STEP 4 — A one-line minibuffer built from an Entry, used to edit a node's
    // text. It lives on the bottom row; we only feed it keys while editing.
    let prompt = Entry::new(&win, a.x, a.y + a.height - 1, a.width);
    let status = Statusbar::new("");
    let mut dirty = false;

    // Edit `node`'s text with the minibuffer. Returns to the caller when the
    // user presses Enter (commit) or Esc (cancel).
    let edit = |node: MmNode| -> bool {
        prompt.set_text(&node.text());
        prompt.set_focus(true);
        loop {
            ctx.clear();
            win.paint();
            mm.paint();
            prompt.paint();
            ctx.flush();
            let Some(k) = gtcaca::poll_key(-1) else { continue };
            match k {
                key::RETURN => {
                    mm.set_text(node, &prompt.text());
                    prompt.set_focus(false);
                    return true;
                }
                key::ESCAPE => {
                    prompt.set_focus(false);
                    return false;
                }
                _ => {
                    prompt.send_key(k); // the Entry does the text editing for us
                }
            }
        }
    };

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 5 — The main loop. Navigation keys go straight to the widget via
    // `send_key`; structural edits (add/remove/edit/save) we handle ourselves.
    // ─────────────────────────────────────────────────────────────────────────
    mm.set_focus(true);
    loop {
        ctx.clear();
        win.paint();
        mm.paint();
        status.set_text(&format!(
            " {}{}   Tab:child Enter:sibling e:edit Space:fold Del:remove s:save q:quit",
            path.as_deref().unwrap_or("(sample)"),
            if dirty { " *" } else { "" },
        ));
        status.draw();
        ctx.flush();

        let Some(k) = gtcaca::poll_key(-1) else { continue };
        const Q: i32 = b'q' as i32;
        const QU: i32 = b'Q' as i32;
        const E: i32 = b'e' as i32;
        const EU: i32 = b'E' as i32;
        const S: i32 = b's' as i32;
        const SU: i32 = b'S' as i32;
        match k {
            Q | QU => break,
            key::TAB => {
                // Add a child under the selection, unfold it, then edit it.
                let sel = mm.selected();
                sel.set_folded(false);
                let child = mm.add_child(sel, "");
                mm.select(child);
                dirty |= edit(child);
            }
            key::RETURN => {
                let node = mm.add_sibling(mm.selected(), "");
                if !node.is_null() {
                    mm.select(node);
                    dirty |= edit(node);
                }
            }
            E | EU => {
                dirty |= edit(mm.selected());
            }
            key::DELETE | key::BACKSPACE => {
                let sel = mm.selected();
                if !sel.as_ptr().eq(&mm.root().as_ptr()) {
                    mm.delete(sel);
                    dirty = true;
                }
            }
            S | SU => {
                if let Some(p) = &path {
                    if save_mm(&mm, p).is_ok() {
                        dirty = false;
                    }
                }
            }
            _ => {
                mm.send_key(k); // arrows + Space (fold) handled by the widget
            }
        }
    }
    Ok(())
}
