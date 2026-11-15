// The atome module on the JavaScript engine (what Acode, the IDE and the playground run): the three styles,
// the Windows-style terminal and the background tick. No DOM needed: it reads the HTML the system draws.
const path = require("path");
require(path.join(__dirname, "..", "exemples", "ton618-lite.js"));
const lite = globalThis.Ton618Lite;
let failed = 0;
const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) failed++; };
function boot(setup) {
  const out = [];
  const r = lite.runTon618(`IMPORT://atome\natome.boot("WebOS")\n${setup}\natome.run()`, { onUi: (m) => out.push(m) });
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
process.exit(failed ? 1 : 0);
