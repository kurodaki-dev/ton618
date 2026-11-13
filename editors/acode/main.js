// TON618 plugin for Acode (https://acode.app): highlighting, run with an
// output panel, live syntax errors. Plain JavaScript, no build step: Acode
// loads this file as is, and ton618-lite.js (TON618's JavaScript engine)
// next to it.
(function () {
  "use strict";
  const PLUGIN_ID = "dev.kurodaki.ton618";
  if (!window.acode) return;

  // ---- Highlighting: an Ace mode -------------------------------------------
  function defineAceMode() {
    ace.define("ace/mode/ton618_highlight_rules", ["require", "exports", "module", "ace/lib/oop", "ace/mode/text_highlight_rules"], function (require, exports) {
      const oop = require("ace/lib/oop");
      const TextHighlightRules = require("ace/mode/text_highlight_rules").TextHighlightRules;
      function Rules() {
        const keywordMapper = this.createKeywordMapper({
          "keyword.control": "if|else|while|for|in|return|break|continue|try|catch|throw|finally|switch|case|default|and|or",
          "storage.type": "function|class|let|var|const|int|float|string|bool|array|dict|html|extends",
          "constant.language": "true|false|nil",
          "variable.language": "self|super",
          "support.function": "print",
        }, "identifier");
        this.$rules = {
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
            { token: keywordMapper, regex: "[A-Za-z_]\\w*\\b" },
            { token: "keyword.operator", regex: "=>|\\|>|\\?\\.|\\?\\?=?|\\.\\.=?|\\*\\*=?|[-+*/%=!<>&|^~]=?" },
            { token: "paren.lparen", regex: "[\\[({]" },
            { token: "paren.rparen", regex: "[\\])}]" },
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
        this.normalizeRules();
      }
      oop.inherits(Rules, TextHighlightRules);
      exports.Ton618HighlightRules = Rules;
    });

    ace.define("ace/mode/ton618", ["require", "exports", "module", "ace/lib/oop", "ace/mode/text", "ace/mode/ton618_highlight_rules", "ace/mode/folding/cstyle", "ace/mode/behaviour/cstyle"], function (require, exports) {
      const oop = require("ace/lib/oop");
      const TextMode = require("ace/mode/text").Mode;
      const Rules = require("ace/mode/ton618_highlight_rules").Ton618HighlightRules;
      const FoldMode = require("ace/mode/folding/cstyle").FoldMode;
      const Behaviour = require("ace/mode/behaviour/cstyle").CstyleBehaviour;
      function Mode() {
        this.HighlightRules = Rules;
        this.foldingRules = new FoldMode();
        this.$behaviour = new Behaviour();
      }
      oop.inherits(Mode, TextMode);
      (function () {
        this.lineCommentStart = "//";
        this.blockComment = { start: "/*", end: "*/" };
        this.getNextLineIndent = function (state, line, tab) {
          const indent = this.$getIndent(line);
          return /[{[(]\s*$/.test(line) ? indent + tab : indent;
        };
        this.checkOutdent = function (state, line, input) {
          return /^\s+$/.test(line) && /^\s*[}\])]/.test(input);
        };
        this.autoOutdent = function (state, doc, row) {
          const line = doc.getLine(row);
          const m = /^(\s*)[}\])]/.exec(line);
          if (!m || row === 0) return;
          const prev = doc.getLine(row - 1);
          const want = this.$getIndent(prev).replace(/( {1,4}|\t)$/, /[{[(]\s*$/.test(prev) ? "$1" : "");
          const target = /[{[(]\s*$/.test(prev) ? this.$getIndent(prev) : want;
          if (m[1] !== target) doc.replace({ start: { row, column: 0 }, end: { row, column: m[1].length } }, target);
        };
        this.$id = "ace/mode/ton618";
      }).call(Mode.prototype);
      exports.Mode = Mode;
    });
  }

  // ---- The plugin --------------------------------------------------------------
  class Ton618Plugin {
    async init(baseUrl) {
      this.baseUrl = baseUrl.endsWith("/") ? baseUrl : baseUrl + "/";
      this.engineSource = await fetch(this.baseUrl + "ton618-lite.js").then((r) => r.text());
      // A copy in the page too, for the instant syntax check.
      new Function(this.engineSource)();

      defineAceMode();
      try {
        const { addMode } = acode.require("aceModes");
        addMode("ton618", "ton", "TON618");
      } catch (e) { /* very old Acode: highlighting only for files opened after a restart */ }

      this.style = document.createElement("style");
      this.style.textContent = `
        .ton618-run { position: fixed; right: 16px; bottom: 76px; z-index: 50; width: 52px; height: 52px; border-radius: 50%;
          border: 2px solid #595959; background: #ffffff; color: #0b0b0b; font-size: 22px; box-shadow: 0 4px 14px #0008; }
        .ton618-panel { position: fixed; left: 0; right: 0; bottom: 0; z-index: 60; height: 42%; display: flex; flex-direction: column;
          background: #141414; color: #e8e8e8; border-top: 2px solid #595959; font: 13px/1.5 monospace; }
        .ton618-panel header { display: flex; gap: 8px; align-items: center; padding: 6px 10px; border-bottom: 1px solid #2b2b2b; font: 13px sans-serif; }
        .ton618-panel header b { color: #ffffff; } .ton618-panel header span { flex: 1; color: #8b8b97; }
        .ton618-panel header button { background: #1d1d1d; color: #e8e8e8; border: 1px solid #2b2b2b; border-radius: 6px; padding: 4px 10px; }
        .ton618-panel pre { flex: 1; margin: 0; padding: 8px 10px; overflow: auto; white-space: pre-wrap; word-break: break-word; }
        .ton618-panel .e { color: #ff6b6b; } .ton618-panel .e a { color: #ff6b6b; text-decoration: underline; }`;
      document.head.appendChild(this.style);

      this.button = document.createElement("button");
      this.button.className = "ton618-run";
      this.button.textContent = "▶";
      this.button.title = "Run TON618";
      this.button.onclick = () => this.run();
      document.body.appendChild(this.button);

      this.commands = [
        { name: "ton618:run", description: "TON618: Run", bindKey: { win: "Ctrl-Enter", mac: "Command-Enter" }, exec: () => this.run() },
        { name: "ton618:stop", description: "TON618: Stop", exec: () => this.stop() },
      ];
      for (const c of this.commands) editorManager.editor.commands.addCommand(c);

      this.onSwitch = () => this.refresh();
      this.onChange = () => { clearTimeout(this.timer); this.timer = setTimeout(() => this.check(), 400); };
      editorManager.on("switch-file", this.onSwitch);
      editorManager.editor.on("change", this.onChange);
      this.refresh();
    }

    get file() { return editorManager.activeFile; }
    isTon() { return !!(this.file && /\.ton$/i.test(this.file.filename || "")); }

    refresh() {
      const ton = this.isTon();
      this.button.style.display = ton ? "" : "none";
      if (ton && this.file.session.getMode().$id !== "ace/mode/ton618") this.file.session.setMode("ace/mode/ton618");
      this.check();
    }

    check() {
      if (!this.isTon()) return;
      const session = this.file.session;
      const r = window.Ton618Lite.checkTon618(session.getValue());
      session.setAnnotations(r.ok ? [] : [{ row: Math.max(0, r.line - 1), column: 0, text: r.message, type: "error" }]);
    }

    // Loads the modules a file imports (IMPORT://name, not ton.*) from its
    // folder, recursively, so the engine can find them.
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

    panel() {
      if (!this.panelEl) {
        const el = document.createElement("div");
        el.className = "ton618-panel";
        el.innerHTML = "<header><b>TON618</b><span></span><button data-a=stop>■ Stop</button><button data-a=close>✕</button></header><pre></pre>";
        el.querySelector("[data-a=stop]").onclick = () => this.stop();
        el.querySelector("[data-a=close]").onclick = () => { this.stop(); el.remove(); };
        this.panelEl = el;
      }
      if (!this.panelEl.isConnected) document.body.appendChild(this.panelEl);
      return this.panelEl;
    }

    print(text, isError) {
      const pre = this.panel().querySelector("pre");
      const span = document.createElement("span");
      if (isError) {
        span.className = "e";
        for (const part of text.split(/(line \d+)/i)) {
          const m = /line (\d+)/i.exec(part);
          if (m) {
            const a = document.createElement("a");
            a.textContent = part;
            a.onclick = () => { editorManager.editor.gotoLine(Number(m[1]), 0, true); editorManager.editor.focus(); };
            span.appendChild(a);
          } else span.append(part);
        }
      } else span.textContent = text;
      pre.append(span, "\n");
      pre.scrollTop = pre.scrollHeight;
    }

    async run() {
      if (!this.isTon()) { window.toast ? toast("Open a .ton file to run it") : alert("Open a .ton file to run it"); return; }
      this.stop();
      const source = this.file.session.getValue();
      const panel = this.panel();
      panel.querySelector("pre").textContent = "";
      const status = panel.querySelector("header span");
      status.textContent = "running…";

      const files = {};
      let dirUrl = null;
      try { dirUrl = this.file.uri ? acode.require("url").dirname(this.file.uri) : null; } catch (e) { dirUrl = null; }
      await this.collectModules(source, dirUrl, files);

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
        status.textContent = e.data.ok ? `finished in ${Date.now() - started} ms` : "stopped with an error";
        this.worker.terminate();
        this.worker = null;
      };
      this.worker.postMessage({ source, files });
    }

    stop() {
      if (this.worker) {
        this.worker.terminate();
        this.worker = null;
        this.print("■ stopped");
        if (this.panelEl) this.panelEl.querySelector("header span").textContent = "stopped";
      }
    }

    destroy() {
      this.stop();
      editorManager.off("switch-file", this.onSwitch);
      editorManager.editor.off("change", this.onChange);
      for (const c of this.commands) editorManager.editor.commands.removeCommand(c.name);
      try { acode.require("aceModes").removeMode("ton618"); } catch (e) { /* ignore */ }
      this.button && this.button.remove();
      this.panelEl && this.panelEl.remove();
      this.style && this.style.remove();
    }
  }

  const plugin = new Ton618Plugin();
  acode.setPluginInit(PLUGIN_ID, (baseUrl) => plugin.init(baseUrl));
  acode.setPluginUnmount(PLUGIN_ID, () => plugin.destroy());
})();
