// The plugin must survive an Acode whose editor is not Ace (CodeMirror: no .on(), no getSession()):
// this is the "editorManager.editor.on is not a function" error. Needs jsdom (skips without it).
const fs = require("fs"), path = require("path");
const root = path.join(__dirname, "..");
let JSDOM;
try { ({ JSDOM } = require(require.resolve("jsdom", { paths: [process.env.ACE_MODULES || root, root] }))); }
catch (e) { console.log("acode-cm test skipped (npm i jsdom to run it)"); process.exit(0); }

const w = new JSDOM("<!doctype html><body></body>", { runScripts: "outside-only" }).window;
w.fetch = async (url) => {
  const f = String(url).endsWith("ton618-lite.js") ? path.join(root, "exemples/ton618-lite.js") : null;
  if (!f) return { ok: false, status: 404, json: async () => ({}), text: async () => "" };
  const t = fs.readFileSync(f, "utf8");
  return { ok: true, status: 200, text: async () => t, json: async () => JSON.parse(t) };
};
const doc = "let x = 1\nprint(x";   // a syntax error on line 2
let init, sidebar;
// a CodeMirror-like editor: no on(), no getSession(), no commands
const view = { state: { doc: { toString: () => doc, lines: 2, line: () => ({ from: 0 }) } }, dispatch() {}, focus() {} };
w.editorManager = { editor: view, activeFile: { filename: "a.ton", uri: "/p/a.ton", session: { doc: { toString: () => doc } } }, on() {}, off() {} };
w.acode = {
  setPluginInit: (id, f) => { init = f; }, setPluginUnmount() {}, addIcon() {},
  require: (n) => ({ aceModes: { addMode() {} }, sidebarApps: { add: (i, id, t, cb) => { sidebar = w.document.createElement("div"); w.document.body.appendChild(sidebar); cb(sidebar); } },
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
  ok(!!sidebar && !!sidebar.querySelector("[data-a=run]"), "the sidebar is there");
  ok(/line 2/.test(sidebar.querySelector(".status").textContent), "the syntax error is shown in the sidebar: " + sidebar.querySelector(".status").textContent);
  process.exit(failed ? 1 : 0);
})();
