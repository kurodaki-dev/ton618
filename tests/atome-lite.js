// The atome module on the JavaScript engine (what Acode, the IDE and the playground run): the three styles,
// the Windows-style terminal and the background tick. No DOM needed: it reads the HTML the system draws.
const path = require("path");
const fs = require("fs");
require(path.join(__dirname, "..", "exemples", "ton618-lite.js"));
const lite = globalThis.Ton618Lite;
// IMPORT://atome comes from the registry package, as in Acode: here from the package's source file
const ATOME = fs.readFileSync(path.join(__dirname, "..", "registry", "sources", "atome", "atome.ton"), "utf8");
let failed = 0;
const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) failed++; };
function boot(setup) {
  const out = [];
  const r = lite.runTon618(`IMPORT://atome\natome.boot("WebOS")\n${setup}\natome.run()`, { files: { "modules/atome.ton": ATOME }, onUi: (m) => out.push(m) });
  if (!r.ok) throw new Error(r.output);
  const html = () => out.filter((m) => m.type === "render").pop().html;
  const click = (id, win = "", value = "") => r.session.dispatch({ type: "click", id, win, value });
  const winId = () => /data-win=.(w\d+)/.exec(html())[1];
  return { r, html, click, winId, out };
}

let o = boot('atome.open("terminal")');
ok(/class=os>/.test(o.html()) && o.html().includes("class=tbar"), "Windows style (default): a desktop with a taskbar");
const w = o.winId();
const type = (text) => { o.r.session.dispatch({ type: "input", id: "i:cmd", win: w, value: text }); o.r.session.dispatch({ type: "key", id: "i:cmd", win: w, key: "Enter", value: text }); };
ok(o.html().includes("C:\\home&gt;"), "the Terminal opens in the Windows (cmd) style");
type("dir"); ok(o.html().includes("Directory of C:\\home") && o.html().includes("&lt;DIR&gt;"), "cmd: dir");
type("cd docs"); type("type todo.txt"); ok(o.html().includes("write my own app"), "cmd: cd and type");
type("md fresh"); type("dir"); ok(o.html().includes("fresh"), "cmd: md then dir");
type("copy todo.txt fresh"); type("dir fresh"); ok(o.html().includes("todo.txt"), "cmd: copy into a folder");
type("tasklist"); ok(o.html().includes("Terminal"), "cmd: tasklist");
type("nope"); ok(o.html().includes("is not recognized"), "cmd: unknown command");
type("unix"); type("pwd"); ok(o.html().includes("/home/docs"), "the unix command switches style, the folder is kept");
o.r.session.stop();

o = boot('atome.style("android")\natome.open("notes")');
ok(o.html().includes("class=nav") && o.html().includes("class=sb"), "Android style: status bar and navigation bar");
o.click("os:home"); ok(o.html().includes("class=grid"), "Android style: the home screen is a grid of apps");
o.r.session.stop();

o = boot('ton.function v(win) { return [atome.title("My own shell")] }\natome.app({"id": "mine", "title": "Mine", "view": v})\natome.style("custom")\natome.open("mine")');
ok(o.html().includes("My own shell") && !o.html().includes("class=tbar") && !o.html().includes("class=nav"), "Custom style: only your app, no taskbar and no navigation bar");
o.r.session.stop();

o = boot('atome.style("auto")\natome.open("clock")');
o.r.session.dispatch({ type: "size", id: "", win: "", x: 400, y: 800, value: "" }); ok(o.html().includes("class=nav"), "auto: a narrow screen gets the Android style");
o.r.session.dispatch({ type: "size", id: "", win: "", x: 1200, y: 800, value: "" }); ok(o.html().includes("class=tbar"), "auto: a wide screen gets the Windows style");
o.r.session.stop();

o = boot('atome.open("clock")');
const cw = o.winId();
o.click("a:toggle", cw); ok(o.html().includes("Pause"), "Clock: the stopwatch starts");
o.click("os:launch", "", "notes"); ok(o.html().includes("Notes") && o.html().includes("Pause"), "other apps open while it keeps running (it is still in its window)");
o.r.session.stop();
// the engine no longer carries the module: without the package, IMPORT://atome says so
const none = lite.runTon618("IMPORT://atome");
ok(!none.ok && /atome/.test(none.output), "the engine has no built-in atome: " + none.output.split("\n").pop());
// fetchRegistryModules downloads the package (a fake registry here) and only what is missing
(async () => {
  const pkgText = fs.readFileSync(path.join(__dirname, "..", "registry", "packages", "atome-1.0.0.tonpkg"), "utf8");
  const sha = require("crypto").createHash("sha256").update(pkgText).digest("hex");
  const calls = [];
  globalThis.fetch = async (url) => {
    calls.push(String(url));
    if (String(url).includes("index.json")) return { ok: true, json: async () => ({ packages: { atome: { versions: { "1.0.0": { file: "packages/atome-1.0.0.tonpkg", sha256: sha } } } } }) };
    return { ok: true, text: async () => pkgText };
  };
  const files = await lite.fetchRegistryModules("IMPORT://atome\natome.boot(\"x\")");
  ok(files["modules/atome.ton"] === ATOME, "fetchRegistryModules installs atome from the registry");
  const again = await lite.fetchRegistryModules("IMPORT://atome", files);
  ok(calls.length === 2 && again["modules/atome.ton"] === ATOME, "nothing is fetched when the module is already there");
  globalThis.fetch = async () => ({ ok: true, text: async () => "tampered", json: async () => ({ packages: { atome: { versions: { "1.0.0": { file: "packages/atome-1.0.0.tonpkg", sha256: sha } } } } }) });
  const bad = await lite.fetchRegistryModules("IMPORT://atome");
  ok(!("modules/atome.ton" in bad), "a package with the wrong checksum is refused");
  const r = lite.runTon618('IMPORT://atome\natome.boot("T")\natome.run()', { files });
  ok(r.ok && r.session, "a program using the fetched module runs");
  console.log(failed ? failed + " failed" : "all passed");
  process.exit(failed ? 1 : 0);
})();
