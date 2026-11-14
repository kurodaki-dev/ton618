// The plugin must survive an Acode whose editor is not Ace (CodeMirror: no .on(), no getSession()):
// this is the "editorManager.editor.on is not a function" error. Needs jsdom (skips without it).
const fs = require("fs"), path = require("path");
const root = path.join(__dirname, "..");
let JSDOM, CM = null, CMSTATE = null;
const base = [process.env.ACE_MODULES || root, root];
try { ({ JSDOM } = require(require.resolve("jsdom", { paths: base }))); }
catch (e) { console.log("acode-cm test skipped (npm i jsdom to run it)"); process.exit(0); }
try { CM = require(require.resolve("@codemirror/language", { paths: base })); CMSTATE = require(require.resolve("@codemirror/state", { paths: base })); } catch (e) { /* the highlighting part is skipped */ }

const w = new JSDOM("<!doctype html><body></body>", { runScripts: "outside-only" }).window;
w.fetch = async (url) => {
  const f = String(url).endsWith("ton618-lite.js") ? path.join(root, "exemples/ton618-lite.js") : null;
  if (!f) return { ok: false, status: 404, json: async () => ({}), text: async () => "" };
  const t = fs.readFileSync(f, "utf8");
  return { ok: true, status: 200, text: async () => t, json: async () => JSON.parse(t) };
};
const doc = "let x = 1\nprint(x";   // a syntax error on line 2
let init, sidebar, registered = null;
// a CodeMirror-like editor: no on(), no getSession(), no commands
const view = { state: { doc: { toString: () => doc, lines: 2, line: () => ({ from: 0 }) } }, dispatch() {}, focus() {} };
w.editorManager = { editor: view, activeFile: { filename: "a.ton", uri: "/p/a.ton", session: { doc: { toString: () => doc } } }, on() {}, off() {} };
w.acode = {
  setPluginInit: (id, f) => { init = f; }, setPluginUnmount() {}, addIcon() {},
  require: (n) => ({ "@codemirror/language": CM, editorLanguages: { register: (...a) => { registered = a; }, unregister() {} }, aceModes: { addMode() {} }, sidebarApps: { add: (i, id, t, cb) => { sidebar = w.document.createElement("div"); w.document.body.appendChild(sidebar); cb(sidebar); } },
    fsOperation: () => ({}), url: { join: (...p) => p.join("/"), dirname: (p) => p.replace(/\/[^/]*$/, "") } })[n],
};
w.toast = () => {};
w.eval(fs.readFileSync(path.join(root, "editors/acode/main.js"), "utf8"));
(async () => {
  let failed = 0;
  const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) failed++; };
  try { await init("http://plugin/"); ok(true, "init doesn't throw when the editor has no on()/getSession()"); }
  catch (e) { ok(false, "init threw: " + e.message); }
  await new Promise((r) => setTimeout(r, 30));
  ok(!!sidebar && !!sidebar.querySelector(".lib") && !sidebar.querySelector("button[data-a=run]"), "the sidebar is there: the module library only");
  if (CM) {
    ok(!!registered && registered[0] === "ton618" && registered[1] === "ton", "the language is registered with Acode's editorLanguages");
    const support = registered[3]();
    const state = CMSTATE.EditorState.create({ doc: '// hi\nif true { let n = 12 }\nprint(f"x={n}") /* c */\nton.dict d = {}', extensions: [support] });
    const tree = CM.ensureSyntaxTree(state, state.doc.length, 5000);
    const names = new Set(); tree.iterate({ enter: (node) => { names.add(node.name); } });
    for (const t of ["comment", "keyword", "number", "string", "variableName", "meta"]) ok(names.has(t), "highlighting produces '" + t + "' tokens");
  } else console.log("skip  highlighting checks (npm i @codemirror/language @codemirror/state @codemirror/view)");
  const css = (w.document.head.querySelector("style") || {}).textContent || "";
  ok(/\.file\.file_type_ton618::before/.test(css) && !/\.file_type_ton618 \{/.test(css), "the .ton file icon is drawn in ::before at 1em (not as a big background)");
  process.exit(failed ? 1 : 0);
})();
