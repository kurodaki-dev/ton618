// editor.js — a tiny code editor for TON618, with no dependency: a transparent
// <textarea> (which does all the typing, selection, undo and IME work the
// browser already knows) over a <pre> holding the syntax-highlighted copy.
"use strict";

const TON_KEYWORDS = new Set(("if else while for in function return break continue try catch throw finally " +
  "switch case default and or print class extends let var const as").split(" "));
const TON_TYPES = new Set("int float string bool array dict html".split(" "));
const TON_LITERALS = new Set(["true", "false", "nil"]);

function escapeHtml(s) {
  return s.replace(/[&<>]/g, (c) => (c === "&" ? "&amp;" : c === "<" ? "&lt;" : "&gt;"));
}

// One regex, one pass: every alternative is a token kind.
const TOKEN_RE = new RegExp([
  /(\/\/[^\n]*|\/\*[\s\S]*?(?:\*\/|$))/.source,                 // 1 comment
  /(f?"(?:\\.|[^"\\\n])*"?|`[^`]*`?)/.source,                    // 2 string, f-string, raw
  /(IMPORT:\/\/[\w./]+(?:\s+as\s+\w+)?)/.source,                 // 3 import
  /(\bton\.)/.source,                                            // 4 ton. prefix
  /(\b(?:0[xX][\da-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|\d[\d_]*(?:\.\d+)?(?:[eE][+-]?\d+)?)\b)/.source, // 5 number
  /([A-Za-z_]\w*)(?=\s*\()/.source,                              // 6 call
  /([A-Za-z_]\w*)/.source,                                       // 7 word
  /(=>|\|>|\?\.|\?\?=?|\.\.=?|\*\*|[-+*/%=!<>&|^~]=?)/.source,   // 8 operator
].join("|"), "g");

function highlightTon(code) {
  let out = "", last = 0, m;
  TOKEN_RE.lastIndex = 0;
  while ((m = TOKEN_RE.exec(code))) {
    if (m.index > last) out += escapeHtml(code.slice(last, m.index));
    const t = m[0];
    let cls = "";
    if (m[1]) cls = "t-com";
    else if (m[2]) {
      if (t[0] === "f") {
        // f"... {expr} ..." — highlight the braces' content as code
        out += '<span class="t-str">' + escapeHtml(t).replace(/\{([^{}]*)\}/g, (_, e) => '{<span class="t-sub">' + e + "</span>}") + "</span>";
        last = TOKEN_RE.lastIndex;
        continue;
      }
      cls = "t-str";
    } else if (m[3]) cls = "t-imp";
    else if (m[4]) cls = "t-ton";
    else if (m[5]) cls = "t-num";
    else if (m[6]) cls = TON_KEYWORDS.has(t) ? "t-kw" : /^[A-Z]/.test(t) ? "t-cls" : "t-fn";
    else if (m[7]) {
      if (TON_KEYWORDS.has(t)) cls = "t-kw";
      else if (TON_TYPES.has(t)) cls = "t-type";
      else if (TON_LITERALS.has(t)) cls = "t-lit";
      else if (t === "self" || t === "super") cls = "t-self";
      else if (/^[A-Z]/.test(t)) cls = "t-cls";
    } else if (m[8]) cls = "t-op";
    out += cls ? `<span class="${cls}">${escapeHtml(t)}</span>` : escapeHtml(t);
    last = TOKEN_RE.lastIndex;
  }
  return out + escapeHtml(code.slice(last));
}

const PAIRS = { "(": ")", "[": "]", "{": "}", '"': '"', "`": "`" };

class TonEditor {
  constructor(host, { onChange, onRun, onSave } = {}) {
    this.onChange = onChange || (() => {});
    host.innerHTML = '<div class="gutter"></div><div class="code"><pre aria-hidden="true"></pre>' +
      '<textarea spellcheck="false" autocapitalize="off" autocomplete="off" autocorrect="off" wrap="off"></textarea></div>';
    this.gutter = host.querySelector(".gutter");
    this.pre = host.querySelector("pre");
    this.ta = host.querySelector("textarea");
    this.errLine = 0;
    this.lineCount = 0;
    this.pending = false;

    this.ta.addEventListener("input", () => { this.render(); this.onChange(this.ta.value); });
    this.ta.addEventListener("scroll", () => this.sync());
    this.ta.addEventListener("keydown", (e) => this.key(e, onRun, onSave));
  }

  get value() { return this.ta.value; }
  set value(v) { this.ta.value = v; this.errLine = 0; this.render(); this.ta.scrollTop = 0; this.ta.scrollLeft = 0; this.sync(); }
  focus() { this.ta.focus(); }

  render() {
    if (this.pending) return;
    this.pending = true;
    requestAnimationFrame(() => {
      this.pending = false;
      // The trailing newline keeps the <pre> as tall as the textarea's last empty line.
      this.pre.innerHTML = highlightTon(this.ta.value) + "\n";
      const n = this.ta.value.split("\n").length;
      if (n !== this.lineCount || this.gutterErr !== this.errLine) {
        let g = "";
        for (let i = 1; i <= n; i++) g += i === this.errLine ? `<div class="err">${i}</div>` : `<div>${i}</div>`;
        this.gutter.innerHTML = g;
        this.lineCount = n;
        this.gutterErr = this.errLine;
      }
      this.sync();
    });
  }

  sync() {
    const x = this.ta.scrollLeft, y = this.ta.scrollTop;
    this.pre.style.transform = `translate(${-x}px, ${-y}px)`;
    this.gutter.style.transform = `translateY(${-y}px)`;
  }

  setError(line) { this.errLine = line || 0; this.lineCount = -1; this.render(); }

  gotoLine(line) {
    const lines = this.ta.value.split("\n");
    let pos = 0;
    for (let i = 0; i < Math.min(line - 1, lines.length); i++) pos += lines[i].length + 1;
    this.ta.focus();
    this.ta.setSelectionRange(pos, pos + (lines[line - 1] || "").length);
    const lh = parseFloat(getComputedStyle(this.ta).lineHeight) || 21;
    this.ta.scrollTop = Math.max(0, (line - 4) * lh);
  }

  // Replaces the selection with `text` through execCommand, so the browser's
  // own undo history keeps working (Ctrl+Z after an auto-indent).
  insert(text, selectFrom, selectTo) {
    const ta = this.ta;
    if (!document.execCommand || !document.execCommand("insertText", false, text)) {
      const s = ta.selectionStart;
      ta.setRangeText(text, s, ta.selectionEnd, "end");
      ta.dispatchEvent(new Event("input"));
    }
    if (selectFrom !== undefined) ta.setSelectionRange(selectFrom, selectTo ?? selectFrom);
  }

  key(e, onRun, onSave) {
    const ta = this.ta, v = ta.value, s = ta.selectionStart, end = ta.selectionEnd;
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key === "Enter") { e.preventDefault(); onRun && onRun(); return; }
    if (mod && (e.key === "s" || e.key === "S")) { e.preventDefault(); onSave && onSave(); return; }
    if (mod && e.key === "/") { e.preventDefault(); this.toggleComment(); return; }

    const lineStart = v.lastIndexOf("\n", s - 1) + 1;
    const indent = /^[ \t]*/.exec(v.slice(lineStart, s))[0];

    if (e.key === "Tab") {
      e.preventDefault();
      if (s === end && !e.shiftKey) return this.insert("    ");
      this.indentLines(e.shiftKey);
      return;
    }
    if (e.key === "Enter" && !e.isComposing) {
      e.preventDefault();
      const before = v[s - 1], after = v[s];
      const opens = before === "{" || before === "[" || before === "(";
      const inner = indent + (opens ? "    " : "");
      if (opens && after === PAIRS[before]) {
        this.insert("\n" + inner + "\n" + indent);
        ta.setSelectionRange(s + 1 + inner.length, s + 1 + inner.length);
      } else {
        this.insert("\n" + inner);
      }
      return;
    }
    if (e.key === "}" && s === end && /^[ \t]+$/.test(v.slice(lineStart, s)) && indent.length >= 4) {
      e.preventDefault();
      ta.setSelectionRange(s - 4, s);
      this.insert("}");
      return;
    }
    if (s === end && PAIRS[e.key] && !e.isComposing) {
      const next = v[s];
      if ((e.key === '"' || e.key === "`") && next === e.key) { e.preventDefault(); ta.setSelectionRange(s + 1, s + 1); return; }
      if (!next || /[\s)\]},;]/.test(next)) { e.preventDefault(); this.insert(e.key + PAIRS[e.key], s + 1); }
      return;
    }
    if (s === end && (e.key === ")" || e.key === "]" || e.key === "}") && v[s] === e.key) {
      e.preventDefault(); ta.setSelectionRange(s + 1, s + 1); return;
    }
    if (e.key === "Backspace" && s === end && s > 0 && PAIRS[v[s - 1]] === v[s] && v[s]) {
      e.preventDefault(); ta.setSelectionRange(s - 1, s + 1); this.insert("");
    }
  }

  selectedLines() {
    const v = this.ta.value;
    const from = v.lastIndexOf("\n", this.ta.selectionStart - 1) + 1;
    let to = v.indexOf("\n", Math.max(this.ta.selectionEnd - 1, this.ta.selectionStart));
    if (to < 0) to = v.length;
    return [from, to];
  }

  indentLines(outdent) {
    const [from, to] = this.selectedLines();
    const block = this.ta.value.slice(from, to);
    const changed = block.split("\n").map((l) => (outdent ? l.replace(/^( {1,4}|\t)/, "") : "    " + l)).join("\n");
    this.ta.setSelectionRange(from, to);
    this.insert(changed, from, from + changed.length);
  }

  toggleComment() {
    const [from, to] = this.selectedLines();
    const lines = this.ta.value.slice(from, to).split("\n");
    const allCommented = lines.every((l) => !l.trim() || /^\s*\/\//.test(l));
    const changed = lines.map((l) => (allCommented ? l.replace(/^(\s*)\/\/ ?/, "$1") : l.trim() ? l.replace(/^(\s*)/, "$1// ") : l)).join("\n");
    this.ta.setSelectionRange(from, to);
    this.insert(changed, from, from + changed.length);
  }
}

window.TonEditor = TonEditor;
window.highlightTon = highlightTon;
