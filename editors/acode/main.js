// TON618 plugin for Acode (https://acode.app): highlighting, a sidebar with a
// Run button and the module library, live syntax errors. Plain JavaScript, no
// build step: Acode loads this file as is, and ton618-lite.js (TON618's
// JavaScript engine) next to it.
(function () {
  "use strict";
  const PLUGIN_ID = "dev.kurodaki.ton618";
  const REGISTRY = "https://c-ton618.duckdns.org/registry";
  const ENGINE_VERSION = "1.0.0";
  if (!window.acode) return;

  // ---- Highlighting ---------------------------------------------------------
  // No ace.define here (Acode's `ace` global doesn't have it): the rules are
  // handed to Ace's own Tokenizer class, found through the text mode every
  // session already has.
  const KEYWORDS = {
    "keyword.control": "if|else|while|for|in|return|break|continue|try|catch|throw|finally|switch|case|default|and|or",
    "storage.type": "function|class|let|var|const|int|float|string|bool|array|dict|html|extends",
    "constant.language": "true|false|nil",
    "variable.language": "self|super",
    "support.function": "print",
  };
  const WORDS = {};
  for (const token of Object.keys(KEYWORDS)) for (const w of KEYWORDS[token].split("|")) WORDS[w] = token;

  const RULES = {
    start: [
      { token: "comment", regex: "//.*$" },
      { token: "comment", regex: "/\\*", next: "comment" },
      { token: "string", regex: 'f"', next: "fstring" },
      { token: "string", regex: '"(?:\\\\.|[^"\\\\])*"' },
      { token: "string", regex: "`", next: "raw" },
      { token: "keyword.other", regex: "IMPORT://[\\w./]+" },
      { token: "keyword.other", regex: "\\bton\\." },
      { token: "constant.numeric", regex: "\\b(?:0[xX][\\da-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|\\d[\\d_]*(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)\\b" },
      { token: "entity.name.type", regex: "\\b[A-Z]\\w*\\b" },
      { token: "entity.name.function", regex: "[a-z_]\\w*(?=\\s*\\()" },
      { token: (value) => WORDS[value] || "identifier", regex: "[A-Za-z_]\\w*\\b" },
      { token: "keyword.operator", regex: "=>|\\|>|\\?\\.|\\?\\?=?|\\.\\.=?|\\*\\*=?|[-+*/%=!<>&|^~]=?" },
      { token: "paren.lparen", regex: "[\\[({]" },
      { token: "paren.rparen", regex: "[\\])}]" },
      { defaultToken: "text" },
    ],
    comment: [{ token: "comment", regex: "\\*/", next: "start" }, { defaultToken: "comment" }],
    fstring: [
      { token: "constant.language.escape", regex: "\\\\.|\\{\\{|\\}\\}" },
      { token: "paren.lparen", regex: "\\{", next: "fexpr" },
      { token: "string", regex: '"', next: "start" },
      { defaultToken: "string" },
    ],
    fexpr: [
      { token: "paren.rparen", regex: "\\}", next: "fstring" },
      { token: "constant.numeric", regex: "\\b\\d[\\d_.]*\\b" },
      { token: "variable.language", regex: "\\b(?:self|super)\\b" },
      { defaultToken: "identifier" },
    ],
    raw: [{ token: "string", regex: "`", next: "start" }, { defaultToken: "string" }],
  };

  // Builds the mode object from the text mode of a session. Returns null when
  // this Acode's Ace doesn't expose what is needed (then: no colours, but
  // everything else still works).
  function buildMode(textMode) {
    const tokenizer = textMode && textMode.getTokenizer && textMode.getTokenizer();
    const Tokenizer = tokenizer && tokenizer.constructor;
    if (typeof Tokenizer !== "function") return null;
    const mode = Object.create(textMode);
    const own = new Tokenizer(RULES);
    mode.$id = "ace/mode/ton618";
    mode.getTokenizer = () => own;
    mode.lineCommentStart = "//";
    mode.blockComment = { start: "/*", end: "*/" };
    mode.getNextLineIndent = function (state, line, tab) {
      const indent = this.$getIndent ? this.$getIndent(line) : (/^\s*/.exec(line) || [""])[0];
      return /[{[(]\s*$/.test(line) ? indent + tab : indent;
    };
    mode.checkOutdent = (state, line, input) => /^\s+$/.test(line) && /^\s*[}\])]/.test(input);
    mode.autoOutdent = function (state, doc, row) {
      const line = doc.getLine(row);
      const m = /^(\s*)[}\])]/.exec(line);
      if (!m || row === 0) return;
      const prev = doc.getLine(row - 1);
      const prevIndent = (/^\s*/.exec(prev) || [""])[0];
      const target = /[{[(]\s*$/.test(prev) ? prevIndent : prevIndent.replace(/( {1,4}|\t)$/, "");
      if (m[1] !== target) doc.replace({ start: { row, column: 0 }, end: { row, column: m[1].length } }, target);
    };
    return mode;
  }

  // ---- Versions and the registry (a small JavaScript twin of the C++ installer) ----
  function parseVer(v) {
    const m = /^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$/.exec(String(v).trim());
    return m ? { n: [+m[1], +m[2], +m[3]], pre: m[4] || "" } : null;
  }
  function cmpVer(a, b) {
    for (let i = 0; i < 3; i++) if (a.n[i] !== b.n[i]) return a.n[i] < b.n[i] ? -1 : 1;
    if (a.pre === b.pre) return 0;
    if (!a.pre) return 1;
    if (!b.pre) return -1;
    return a.pre < b.pre ? -1 : 1;
  }
  // ranges: 1.2.3  ^1.2  ~1.2.3  >=1.0  1.x  *  (several separated by spaces)
  function satisfies(range, version) {
    const v = parseVer(version);
    if (!v) return false;
    for (const part of String(range || "*").trim().split(/\s+/)) {
      if (!part || part === "*") continue;
      let m;
      if ((m = /^(>=|<=|>|<|=)?v?(\d+)(?:\.(\d+|x|\*))?(?:\.(\d+|x|\*))?$/.exec(part)) && m[1]) {
        const w = parseVer(`${m[2]}.${m[3] || 0}.${m[4] || 0}`);
        const c = cmpVer(v, w);
        if (!(m[1] === ">=" ? c >= 0 : m[1] === ">" ? c > 0 : m[1] === "<=" ? c <= 0 : m[1] === "<" ? c < 0 : c === 0)) return false;
      } else if ((m = /^\^v?(\d+)(?:\.(\d+))?(?:\.(\d+))?$/.exec(part))) {
        const lo = parseVer(`${m[1]}.${m[2] || 0}.${m[3] || 0}`);
        const hi = +m[1] > 0 ? { n: [+m[1] + 1, 0, 0], pre: "" } : m[2] !== undefined && +m[2] > 0 ? { n: [0, +m[2] + 1, 0], pre: "" } : { n: [0, 0, +(m[3] || 0) + 1], pre: "" };
        if (cmpVer(v, lo) < 0 || cmpVer(v, hi) >= 0) return false;
      } else if ((m = /^~v?(\d+)\.(\d+)(?:\.(\d+))?$/.exec(part))) {
        const lo = parseVer(`${m[1]}.${m[2]}.${m[3] || 0}`);
        if (cmpVer(v, lo) < 0 || v.n[0] !== +m[1] || v.n[1] !== +m[2]) return false;
      } else if ((m = /^v?(\d+)(?:\.(\d+|x|\*))?(?:\.(\d+|x|\*))?$/.exec(part))) {
        if (v.n[0] !== +m[1]) return false;
        if (m[2] !== undefined && !/^[x*]$/.test(m[2]) && v.n[1] !== +m[2]) return false;
        if (m[3] !== undefined && !/^[x*]$/.test(m[3]) && v.n[2] !== +m[3]) return false;
      } else return false;
    }
    return true;
  }
  function newest(versions, range) {
    return Object.keys(versions || {}).filter((v) => parseVer(v) && satisfies(range, v)).sort((a, b) => cmpVer(parseVer(b), parseVer(a)))[0];
  }
  async function sha256(text) {
    if (!(window.crypto && crypto.subtle)) throw new Error("This WebView can't verify checksums (no crypto.subtle), so nothing is installed.");
    const bytes = new TextEncoder().encode(text);
    const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
    return Array.from(hash, (b) => b.toString(16).padStart(2, "0")).join("");
  }
  const safeName = (p) => p && !p.startsWith("/") && !p.includes("..") && !p.includes("\\") && /^[\w.\-/]+$/.test(p);

  // ---- The editor: Acode has used Ace, and newer versions use CodeMirror ----
  const safe = (fn, fallback) => { try { const v = fn(); return v === undefined ? fallback : v; } catch (e) { return fallback; } };
  const editorOf = () => safe(() => editorManager.editor, null);
  const isAce = () => { const e = editorOf(); return !!(e && typeof e.getSession === "function" && typeof e.on === "function"); };
  function readText(file) {
    if (!file) return "";
    return safe(() => file.session.getValue(), undefined) ??       // Ace
      safe(() => file.session.doc.toString(), undefined) ??       // CodeMirror state
      safe(() => editorOf().state.doc.toString(), undefined) ??   // CodeMirror view
      safe(() => editorOf().getValue(), "");
  }
  function gotoLine(line) {
    const e = editorOf();
    if (!e) return;
    if (typeof e.gotoLine === "function") return safe(() => { e.gotoLine(line, 0, true); e.focus(); });
    safe(() => {   // CodeMirror
      const l = e.state.doc.line(Math.min(Math.max(line, 1), e.state.doc.lines));
      e.dispatch({ selection: { anchor: l.from }, scrollIntoView: true });
      e.focus();
    });
  }

  // ---- The plugin ----------------------------------------------------------------
  class Ton618Plugin {
    async init(baseUrl) {
      this.baseUrl = baseUrl.endsWith("/") ? baseUrl : baseUrl + "/";
      this.engineSource = await fetch(this.baseUrl + "ton618-lite.js").then((r) => r.text());
      new Function(this.engineSource)(); // a copy in the page too, for the instant syntax check
      this.mode = null;

      // Highlighting. Registered for the extension, and cached under the name
      // Ace asks for, so files opened later get it without any module loading.
      try { acode.require("aceModes").addMode("ton618", "ton", "TON618"); } catch (e) { /* old Acode */ }
      this.ensureMode();

      // File icon for .ton, and the sidebar icon.
      try {
        acode.addIcon("file_type_ton618", this.baseUrl + "icon.png");
        acode.addIcon("ton618-side", this.baseUrl + "icon.png");
      } catch (e) { /* no addIcon in this version */ }

      this.style = document.createElement("style");
      this.style.textContent = `
        .file_type_ton618, .icon.file_type_ton618 { background-image: url(${this.baseUrl}icon.png) !important; background-size: contain !important; background-repeat: no-repeat !important; background-position: center !important; }
        .icon.ton618-app { background-image: url(${this.baseUrl}icon.png) !important; background-size: 70% !important; background-repeat: no-repeat !important; background-position: center !important; }
        .ton618-win { position: fixed; top: 0; right: 0; bottom: 0; left: 0; z-index: 99999; display: flex; flex-direction: column; background: #0b0b0b; color: #e8e8e8; font: 14px/1.5 sans-serif; }
        .ton618-win header { display: flex; align-items: center; gap: 8px; padding: 10px 12px; border-bottom: 1px solid #2b2b2b; padding-top: max(10px, env(safe-area-inset-top)); }
        .ton618-win header b { color: #fff; } .ton618-win header span { flex: 1; color: #8b8b97; font-size: 12px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
        .ton618-win header button { font: inherit; padding: 8px 14px; border-radius: 6px; border: 1px solid #595959; background: #1b1b1b; color: #fff; }
        .ton618-win pre { flex: 1; margin: 0; padding: 12px; overflow: auto; white-space: pre-wrap; word-break: break-word; font: 14px/1.55 monospace; user-select: text; }
        .ton618-win pre .e { color: #ff6b6b; } .ton618-win pre .e a { text-decoration: underline; }
        .ton618-app { height: 100%; display: flex; flex-direction: column; color: inherit; font: 14px/1.4 sans-serif; overflow: hidden; }
        .ton618-app h3 { margin: 0; padding: 10px 12px 6px; font-size: 13px; letter-spacing: .06em; text-transform: uppercase; opacity: .7; }
        .ton618-app .bar { display: flex; gap: 6px; padding: 4px 12px 8px; }
        .ton618-app button { font: inherit; padding: 7px 12px; border-radius: 6px; border: 1px solid #595959; background: #1b1b1b; color: #fff; }
        .ton618-app button.go { background: #fff; color: #0b0b0b; font-weight: 600; flex: 1; }
        .ton618-app button[disabled] { opacity: .45; }
        .ton618-app .status { padding: 0 12px 6px; font-size: 12px; opacity: .7; min-height: 1.4em; }
        .ton618-app pre.out { margin: 0 12px 10px; padding: 8px; max-height: 32%; min-height: 3.5em; overflow: auto; white-space: pre-wrap; word-break: break-word; background: #141414; color: #e8e8e8; border: 1px solid #2b2b2b; border-radius: 6px; font: 12px/1.5 monospace; user-select: text; }
        .ton618-app pre.out .e { color: #ff6b6b; } .ton618-app pre.out .e a { text-decoration: underline; }
        .ton618-app input { margin: 0 12px 8px; padding: 8px; border-radius: 6px; border: 1px solid #595959; background: #141414; color: #fff; font: inherit; }
        .ton618-app .lib { flex: 1; overflow: auto; padding: 0 12px 12px; }
        .ton618-app .row { display: flex; gap: 8px; align-items: center; padding: 9px 0; border-bottom: 1px solid #2b2b2b; }
        .ton618-app .row .info { flex: 1; min-width: 0; } .ton618-app .row b { font-family: monospace; }
        .ton618-app .row small { display: block; opacity: .65; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
        .ton618-app .row button { padding: 5px 10px; flex: none; } .ton618-app .row .ok { opacity: .8; font-size: 12px; }
        .ton618-app .note { padding: 8px 0; font-size: 12px; opacity: .7; }`;
      document.head.appendChild(this.style);

      this.state = { index: null, installed: {}, busy: {}, filter: "", message: "" };
      try {
        acode.require("sidebarApps").add("ton618-side", "ton618", "TON618", (el) => this.buildSidebar(el));
      } catch (e) {
        this.fallbackButton(); // very old Acode: a floating button instead of the sidebar
      }

      // Everything below talks to the editor: each step is guarded, so a different editor
      // (CodeMirror instead of Ace) costs a feature, never the whole plugin.
      this.commands = [
        { name: "ton618:run", description: "TON618: Run", bindKey: { win: "Ctrl-Enter", mac: "Command-Enter" }, exec: () => this.run() },
        { name: "ton618:stop", description: "TON618: Stop", exec: () => this.stop() },
      ];
      this.commandsAdded = [];
      if (isAce()) for (const c of this.commands) safe(() => { editorOf().commands.addCommand(c); this.commandsAdded.push(c.name); });

      this.onSwitch = () => this.refresh();
      this.onChange = () => { clearTimeout(this.timer); this.timer = setTimeout(() => this.check(), 400); };
      this.listening = [];
      const listen = (target, event, fn) => safe(() => {
        if (target && typeof target.on === "function") { target.on(event, fn); this.listening.push([target, event, fn]); }
      });
      listen(editorManager, "switch-file", this.onSwitch);
      listen(editorManager, "file-loaded", this.onSwitch);
      listen(editorManager, "file-content-changed", this.onChange);
      if (isAce()) listen(editorOf(), "change", this.onChange);
      safe(() => this.refresh());
    }

    // ---- highlighting ----
    ensureMode() {
      if (this.mode) return this.mode;
      if (!isAce()) return null;   // CodeMirror: no Ace mode to build
      try {
        const session = editorManager.editor.getSession();
        const mode = buildMode(session.getMode());
        if (!mode) return null;
        this.mode = mode;
        const cache = session.$modes || (window.ace && ace.config && ace.config.$modes);
        if (cache) cache["ace/mode/ton618"] = mode;
      } catch (e) { this.mode = null; }
      return this.mode;
    }

    get file() { return editorManager.activeFile; }
    isTon() { return !!(this.file && /\.ton$/i.test(this.file.filename || "")); }

    refresh() {
      if (this.isTon() && isAce()) {
        const mode = this.ensureMode();
        const session = this.file.session;
        if (mode) safe(() => { if (session.getMode().$id !== "ace/mode/ton618") session.setMode(mode); });
      }
      if (this.runButton) this.runButton.disabled = !this.isTon();
      this.check();
      this.loadInstalled();
    }

    check() {
      if (!this.isTon()) return;
      const r = window.Ton618Lite.checkTon618(readText(this.file));
      this.lastCheck = r;
      if (typeof this.file.session.setAnnotations === "function") {   // Ace: marks in the gutter
        safe(() => this.file.session.setAnnotations(r.ok ? [] : [{ row: Math.max(0, r.line - 1), column: 0, text: r.message, type: "error" }]));
      }
      this.setStatus(r.ok ? "" : `line ${r.line}: ${r.message}`);   // always shown in the sidebar
    }

    // ---- sidebar ----
    buildSidebar(el) {
      el.classList.add("ton618-app");
      el.innerHTML = `
        <h3>TON618</h3>
        <div class="bar"><button class="go" data-a="run">▶ Run</button><button data-a="stop">■</button></div>
        <div class="status"></div>
        <pre class="out"></pre>
        <h3>Modules</h3>
        <input type="search" placeholder="Search the library…" data-a="filter">
        <div class="lib"></div>`;
      this.sidebar = el;
      this.runButton = el.querySelector("[data-a=run]");
      this.statusEl = el.querySelector(".status");
      this.outEl = el.querySelector("pre.out");
      this.libEl = el.querySelector(".lib");
      this.runButton.onclick = () => this.run();
      el.querySelector("[data-a=stop]").onclick = () => this.stop();
      el.querySelector("[data-a=filter]").oninput = (e) => { this.state.filter = e.target.value.trim().toLowerCase(); this.renderLibrary(); };
      this.runButton.disabled = !this.isTon();
      this.renderLibrary();
      this.loadIndex();
      this.loadInstalled();
    }

    fallbackButton() {
      this.floating = document.createElement("button");
      this.floating.textContent = "▶";
      this.floating.style.cssText = "position:fixed;right:16px;bottom:76px;z-index:50;width:52px;height:52px;border-radius:50%;border:2px solid #595959;background:#fff;color:#0b0b0b;font-size:22px";
      this.floating.onclick = () => this.run();
      document.body.appendChild(this.floating);
    }

    setStatus(text) {
      if (this.statusEl) this.statusEl.textContent = text;
      if (this.winStatus && this.winEl && this.winEl.isConnected) this.winStatus.textContent = text;
    }

    // Run opens a full window over Acode: the program's output, a Stop and a Close button.
    openWindow() {
      this.closeWindow();
      const el = document.createElement("div");
      el.className = "ton618-win";
      el.innerHTML = "<header><b>TON618</b><span></span><button data-a=stop>■ Stop</button><button data-a=again>▶ Again</button><button data-a=close>✕ Close</button></header><pre></pre>";
      el.querySelector("[data-a=stop]").onclick = () => this.stop();
      el.querySelector("[data-a=again]").onclick = () => this.run();
      el.querySelector("[data-a=close]").onclick = () => this.closeWindow();
      document.body.appendChild(el);
      this.winEl = el;
      this.winStatus = el.querySelector("span");
      this.winOut = el.querySelector("pre");
      this.winStatus.textContent = (this.file && this.file.filename) || "";
    }

    closeWindow() {
      this.stop(true);
      if (this.winEl) this.winEl.remove();
      this.winEl = this.winOut = this.winStatus = null;
    }

    print(text, isError) {
      const target = this.winOut && this.winEl && this.winEl.isConnected ? this.winOut : this.outEl;
      if (!target) return;
      const span = document.createElement("span");
      if (isError) {
        span.className = "e";
        for (const part of text.split(/(line \d+)/i)) {
          const m = /line (\d+)/i.exec(part);
          if (m) {
            const a = document.createElement("a");
            a.textContent = part;
            a.onclick = () => gotoLine(Number(m[1]));
            span.appendChild(a);
          } else span.append(part);
        }
      } else span.textContent = text;
      target.append(span, "\n");
      target.scrollTop = target.scrollHeight;
    }

    toast(text) { try { window.toast ? window.toast(text, 3000) : acode.alert("TON618", text); } catch (e) { /* ignore */ } }

    // ---- running ----
    // Loads the modules a file imports (IMPORT://name, not ton.*), recursively.
    async collectModules(source, dirUrl, files, depth = 0) {
      if (depth > 8 || !dirUrl) return;
      let fs, Url;
      try { fs = acode.require("fsOperation"); Url = acode.require("url"); } catch (e) { return; }
      const names = [...source.matchAll(/IMPORT:\/\/(?!ton\.)([\w/]+)/g)].map((m) => m[1]);
      for (const name of names) {
        for (const rel of [name + ".ton", "modules/" + name + ".ton"]) {
          if (rel in files) break;
          try {
            const text = await fs(Url.join(dirUrl, rel)).readFile("utf8");
            files[rel] = text;
            await this.collectModules(text, dirUrl, files, depth + 1);
            break;
          } catch (e) { /* not there: try the next location */ }
        }
      }
    }

    fileDir() {
      try { return this.file && this.file.uri ? acode.require("url").dirname(this.file.uri) : null; } catch (e) { return null; }
    }

    async run() {
      if (!this.isTon()) { this.toast("Open a .ton file to run it"); return; }
      if (!this.outEl) { try { acode.require("sidebarApps").get && acode.require("sidebarApps").get("ton618"); } catch (e) { /* ignore */ } }
      this.stop(true);
      const source = readText(this.file);
      this.openWindow();
      if (this.outEl) this.outEl.textContent = "";
      this.setStatus("running…");

      const files = {};
      await this.collectModules(source, this.fileDir(), files);

      const workerCode = this.engineSource + `
        self.onmessage = (e) => {
          let n = 0;
          const r = self.Ton618Lite.runTon618(e.data.source, { files: e.data.files, onPrint: (t) => { n++; self.postMessage({ out: t }); } });
          const lines = r.output.split("\\n");
          self.postMessage({ done: true, ok: r.ok, error: r.ok ? null : lines.slice(n).join("\\n") });
        };`;
      const url = URL.createObjectURL(new Blob([workerCode], { type: "text/javascript" }));
      const started = Date.now();
      this.worker = new Worker(url);
      URL.revokeObjectURL(url);
      this.worker.onmessage = (e) => {
        if (!e.data.done) return this.print(e.data.out);
        if (e.data.error) this.print(e.data.error, true);
        this.setStatus(e.data.ok ? `finished in ${Date.now() - started} ms` : "stopped with an error");
        this.worker.terminate();
        this.worker = null;
      };
      this.worker.postMessage({ source, files });
    }

    stop(quiet) {
      if (!this.worker) return;
      this.worker.terminate();
      this.worker = null;
      if (!quiet) { this.print("■ stopped"); this.setStatus("stopped"); }
    }

    // ---- module library ----
    async loadIndex() {
      this.state.message = "Loading the library…";
      this.renderLibrary();
      try {
        const r = await fetch(`${REGISTRY}/index.json?t=${Date.now()}`, { cache: "no-store" });
        if (!r.ok) throw new Error("HTTP " + r.status);
        const index = await r.json();
        if (!index || !index.packages) throw new Error("not a registry index");
        this.state.index = index;
        this.state.message = "";
      } catch (e) {
        this.state.message = "Can't reach the registry (" + e.message + "). Tap to retry.";
      }
      this.renderLibrary();
    }

    modulesDir() {
      const dir = this.fileDir();
      return dir ? acode.require("url").join(dir, "modules") : null;
    }

    async loadInstalled() {
      this.state.installed = {};
      try {
        const dir = this.modulesDir();
        if (dir) {
          const text = await acode.require("fsOperation")(acode.require("url").join(dir, ".installed.json")).readFile("utf8");
          const db = JSON.parse(text);
          this.state.installed = (db && db.packages) || {};
        }
      } catch (e) { /* no modules folder yet */ }
      this.renderLibrary();
    }

    renderLibrary() {
      if (!this.libEl) return;
      const s = this.state;
      this.libEl.textContent = "";
      const note = (text, click) => {
        const d = document.createElement("div");
        d.className = "note";
        d.textContent = text;
        if (click) d.onclick = click;
        this.libEl.appendChild(d);
      };
      if (s.message) note(s.message, s.index ? null : () => this.loadIndex());
      if (!this.fileDir()) note("Open a .ton file saved in a folder: modules are installed in its modules/ folder.");
      if (!s.index) return;
      const names = Object.keys(s.index.packages).filter((n) => {
        const p = s.index.packages[n];
        return !s.filter || (n + " " + (p.description || "") + " " + (p.keywords || []).join(" ")).toLowerCase().includes(s.filter);
      }).sort();
      if (!names.length) note("No module matches.");
      for (const name of names) {
        const p = s.index.packages[name], have = s.installed[name], busy = s.busy[name];
        const row = document.createElement("div");
        row.className = "row";
        const info = document.createElement("div");
        info.className = "info";
        info.innerHTML = "<b></b> <span></span><small></small>";
        info.querySelector("b").textContent = name;
        info.querySelector("span").textContent = (have ? have.version : p.latest || "");
        info.querySelector("small").textContent = p.description || "";
        row.appendChild(info);
        const button = document.createElement("button");
        const outdated = have && p.latest && parseVer(p.latest) && parseVer(have.version) && cmpVer(parseVer(p.latest), parseVer(have.version)) > 0;
        if (busy) { button.textContent = busy; button.disabled = true; }
        else if (!have) { button.textContent = "Install"; button.onclick = () => this.install(name); }
        else if (outdated) { button.textContent = "Update"; button.onclick = () => this.install(name, true); }
        else { button.textContent = "Remove"; button.onclick = () => this.remove(name); }
        row.appendChild(button);
        if (have && !outdated && !busy) { const ok = document.createElement("span"); ok.className = "ok"; ok.textContent = "✓ installed"; row.insertBefore(ok, button); }
        this.libEl.appendChild(row);
      }
    }

    async install(name, upgrade) {
      const dir = this.modulesDir();
      if (!dir) { this.toast("Open a saved .ton file first: modules go next to it."); return; }
      const s = this.state;
      try {
        await this.installOne(name, "*", dir, new Set(), true);
        this.toast(`${name} installed. Use IMPORT://${name}`);
      } catch (e) {
        this.toast(e.message || String(e));
        this.setStatus(e.message || String(e));
      }
      delete s.busy[name];
      this.renderLibrary();
    }

    async installOne(name, range, dir, stack, explicit) {
      const s = this.state, fs = acode.require("fsOperation"), Url = acode.require("url");
      if (!/^[a-z][a-z0-9_-]{0,63}$/.test(name)) throw new Error(`'${name}' isn't a valid package name.`);
      const entry = s.index && s.index.packages[name];
      if (!entry) throw new Error(`No package called '${name}' in the registry.`);
      const have = s.installed[name];
      if (have && satisfies(range, have.version) && !explicit) return;
      if (stack.has(name)) return;
      stack.add(name);
      const version = newest(entry.versions, range);
      if (!version) throw new Error(`No version of ${name} matches '${range}'.`);
      const info = entry.versions[version];
      if (!safeName(info.file) || !/^[0-9a-f]{64}$/.test(info.sha256 || "")) throw new Error(`The registry entry for ${name} is incomplete.`);
      s.busy[name] = "…";
      this.renderLibrary();

      const r = await fetch(`${REGISTRY}/${info.file}`, { cache: "no-store" });
      if (!r.ok) throw new Error(`Download failed (HTTP ${r.status}).`);
      const text = await r.text();
      if ((await sha256(text)) !== info.sha256) throw new Error(`Checksum mismatch for ${name} ${version}: nothing was installed.`);
      const pkg = JSON.parse(text);
      if (pkg.format !== 1 || pkg.name !== name || pkg.version !== version || !pkg.files || !pkg.files[pkg.main]) throw new Error("The package's contents don't match the registry.");
      if (pkg.requires && !satisfies(pkg.requires, ENGINE_VERSION)) throw new Error(`${name} ${version} needs ton618 ${pkg.requires}; this plugin runs ${ENGINE_VERSION}.`);
      for (const rel of Object.keys(pkg.files)) if (!safeName(rel) || typeof pkg.files[rel] !== "string") throw new Error(`The package holds an unsafe file name ('${rel}').`);

      for (const [dep, depRange] of Object.entries(pkg.dependencies || {})) await this.installOne(dep, depRange || "*", dir, stack, false);

      const written = [];
      const write = async (rel, content) => {
        const parts = rel.split("/");
        let folder = dir;
        for (const part of parts.slice(0, -1)) {
          const next = Url.join(folder, part);
          if (!(await fs(next).exists())) await fs(folder).createDirectory(part);
          folder = next;
        }
        const target = Url.join(folder, parts[parts.length - 1]);
        if (await fs(target).exists()) await fs(target).writeFile(content);
        else await fs(folder).createFile(parts[parts.length - 1], content);
      };
      if (!(await fs(dir).exists())) await fs(Url.dirname(dir)).createDirectory("modules");
      for (const [rel, content] of Object.entries(pkg.files)) {
        const target = rel === pkg.main ? `${name}.ton` : `${name}/${rel}`;
        await write(target, content);
        written.push("modules/" + target);
      }
      s.installed[name] = { version, sha256: info.sha256, dependencies: pkg.dependencies || {}, files: written };
      await write(".installed.json", JSON.stringify({ format: 1, packages: s.installed }, null, 2));
    }

    async remove(name) {
      const dir = this.modulesDir(), s = this.state;
      if (!dir) return;
      for (const [other, rec] of Object.entries(s.installed)) {
        if (other !== name && rec.dependencies && name in rec.dependencies) { this.toast(`${other} needs ${name}: remove it first.`); return; }
      }
      try {
        const fs = acode.require("fsOperation"), Url = acode.require("url");
        for (const f of (s.installed[name] && s.installed[name].files) || []) {
          const rel = f.replace(/^modules\//, "");
          if (!safeName(rel)) continue;
          try { await fs(Url.join(dir, rel)).delete(); } catch (e) { /* already gone */ }
        }
        try { await fs(Url.join(dir, name)).delete(); } catch (e) { /* no folder */ }
        delete s.installed[name];
        const target = Url.join(dir, ".installed.json");
        if (await fs(target).exists()) await fs(target).writeFile(JSON.stringify({ format: 1, packages: s.installed }, null, 2));
        this.toast(`${name} removed.`);
      } catch (e) { this.toast(e.message || String(e)); }
      this.renderLibrary();
    }

    destroy() {
      this.closeWindow();
      clearTimeout(this.timer);
      for (const [target, event, fn] of this.listening || []) safe(() => (target.off || target.removeListener).call(target, event, fn));
      for (const name of this.commandsAdded || []) safe(() => editorOf().commands.removeCommand(name));
      try { acode.require("aceModes").removeMode("ton618"); } catch (e) { /* ignore */ }
      try { acode.require("sidebarApps").remove && acode.require("sidebarApps").remove("ton618"); } catch (e) { /* ignore */ }
      this.floating && this.floating.remove();
      this.style && this.style.remove();
    }
  }

  const plugin = new Ton618Plugin();
  acode.setPluginInit(PLUGIN_ID, (baseUrl) => plugin.init(baseUrl));
  acode.setPluginUnmount(PLUGIN_ID, () => plugin.destroy());
})();
