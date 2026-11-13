// Loads editors/acode/main.js in a headless Ace (jsdom) with a fake Acode, and
// checks: no ace.define needed, .ton highlighting, run, and the module
// library (install / installed state / remove) against a locally built registry.
// Needs `npm i ace-builds jsdom` (CI does it); skips itself when they are missing.
const fs = require("fs"), path = require("path"), cp = require("child_process"), os = require("os");
const root = path.join(__dirname, "..");
let JSDOM, aceSrc;
try {
  const base = process.env.ACE_MODULES || root;
  ({ JSDOM } = require(require.resolve("jsdom", { paths: [base, root] })));
  aceSrc = fs.readFileSync(require.resolve("ace-builds/src-noconflict/ace.js", { paths: [base, root] }), "utf8");
} catch (e) { console.log("acode test skipped (npm i ace-builds jsdom to run it)"); process.exit(0); }

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "acode-"));
cp.execFileSync("python3", [path.join(root, "scripts/registry.py"), "build", tmp]);
const regDir = path.join(tmp, "registry");

const dom = new JSDOM('<!doctype html><div id="ed"></div>', { runScripts: "outside-only", pretendToBeVisual: true });
const w = dom.window;
w.eval(aceSrc);
w.fetch = async (url) => {
  url = String(url).replace(/\?.*$/, "");
  let file = url.startsWith("https://c-ton618.duckdns.org/registry/") ? path.join(regDir, url.split("/registry/")[1])
    : url.endsWith("ton618-lite.js") ? path.join(root, "exemples/ton618-lite.js") : null;
  if (!file || !fs.existsSync(file)) return { ok: false, status: 404, text: async () => "", json: async () => ({}) };
  const text = fs.readFileSync(file, "utf8");
  return { ok: true, status: 200, text: async () => text, json: async () => JSON.parse(text) };
};
Object.defineProperty(w, "crypto", { value: { subtle: { digest: (_, b) => require("crypto").webcrypto.subtle.digest("SHA-256", b) } }, configurable: true });
w.TextEncoder = TextEncoder; w.Uint8Array = Uint8Array;

// an in-memory file system behind fsOperation
const disk = new Map(); // path -> string | null (directory)
disk.set("/proj", null);
const Url = { join: (...p) => p.join("/").replace(/\/+/g, "/"), dirname: (p) => p.replace(/\/[^/]*$/, "") };
const fsOp = (p) => ({
  exists: async () => disk.has(p),
  readFile: async () => { if (typeof disk.get(p) !== "string") throw new Error("ENOENT " + p); return disk.get(p); },
  writeFile: async (c) => { disk.set(p, c); },
  createFile: async (n, c) => { disk.set(Url.join(p, n), c); },
  createDirectory: async (n) => { disk.set(Url.join(p, n), null); },
  delete: async () => { for (const k of [...disk.keys()]) if (k === p || k.startsWith(p + "/")) disk.delete(k); },
});

const editor = w.ace.edit("ed");
const session = editor.getSession();
session.setValue('// hi\nlet x = 1\nprint(f"x={x}")\nton.sys\n');
const active = { filename: "a.ton", uri: "/proj/a.ton", session };
let sidebar, icons = [], init, modes = [];
w.editorManager = { editor, get activeFile() { return active; }, on() {}, off() {} };
w.acode = {
  setPluginInit: (id, f) => { init = f; }, setPluginUnmount() {}, addIcon: (n) => icons.push(n),
  require: (n) => ({
    aceModes: { addMode: (...a) => modes.push(a), removeMode() {} },
    sidebarApps: { add: (icon, id, title, cb) => { sidebar = w.document.createElement("div"); w.document.body.appendChild(sidebar); cb(sidebar); } },
    fsOperation: fsOp, url: Url,
  })[n],
};
w.toast = (t) => console.log("   toast:", t);
w.Worker = undefined;
w.URL.createObjectURL = () => "blob:x";
w.eval(fs.readFileSync(path.join(root, "editors/acode/main.js"), "utf8"));

const fail = [];
const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) fail.push(m); };
const tick = () => new Promise((r) => setTimeout(r, 30));

(async () => {
  await init("http://plugin/");
  await tick();
  ok(modes.length === 1 && modes[0][1] === "ton", "mode registered for the .ton extension");
  ok(icons.includes("file_type_ton618") && icons.includes("ton618-side"), "file and sidebar icons registered");
  ok(session.getMode().$id === "ace/mode/ton618", "the .ton file got the TON618 mode");
  const toks = session.getTokens(1).map((t) => t.type + ":" + t.value.trim()).join("|");
  ok(/storage\.type:let/.test(toks) && /constant\.numeric:1/.test(toks), "tokens for `let x = 1`: " + toks);
  ok(session.getTokens(2).some((t) => t.type === "string"), "f-string is a string token");
  ok(session.getTokens(0)[0].type === "comment", "comment token");
  ok(w.ace.config.$modes["ace/mode/ton618"] === session.getMode(), "mode cached for files opened later");
  const other = w.ace.createEditSession("let y = 2", "ace/mode/ton618");
  await tick();
  ok(other.getMode().$id === "ace/mode/ton618", "a session asking for ace/mode/ton618 gets it without module loading");

  ok(!!sidebar && !!sidebar.querySelector("[data-a=run]"), "sidebar has the Run button");
  await tick(); await tick();
  const rows = () => [...sidebar.querySelectorAll(".row")];
  ok(rows().length >= 1 && rows().some((r) => r.querySelector("b").textContent === "stringutils"), "library lists the registry's modules");
  const row = () => rows().find((r) => r.querySelector("b").textContent === "stringutils");
  ok(row().querySelector("button").textContent === "Install", "Install button shown");
  row().querySelector("button").click();
  await tick(); await tick(); await tick();
  ok(disk.has("/proj/modules/stringutils.ton") && disk.has("/proj/modules/.installed.json"), "files written into modules/");
  ok(row().querySelector("button").textContent === "Remove" && /installed/.test(row().textContent), "installed state shown after install");
  ok(JSON.parse(disk.get("/proj/modules/.installed.json")).packages.stringutils.version, ".installed.json records the version");
  row().querySelector("button").click();
  await tick(); await tick();
  ok(!disk.has("/proj/modules/stringutils.ton") && row().querySelector("button").textContent === "Install", "remove deletes the files");

  fs.rmSync(tmp, { recursive: true, force: true });
  console.log(fail.length ? `\n${fail.length} check(s) failed` : "\nacode: all checks passed");
  process.exit(fail.length ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(1); });
