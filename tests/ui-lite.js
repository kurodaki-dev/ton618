// ton.ui on the JavaScript engine: render, events, timers, storage, escaping (no DOM needed).
const path = require("path");
require(path.join(__dirname, "..", "exemples", "ton618-lite.js"));
const lite = globalThis.Ton618Lite;
let failed = 0;
const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) failed++; };

const out = [];
const r = lite.runTon618(`IMPORT://ton.ui
ton.let n = 0
ton.function draw() {
  ton.let who = ui_storage_get("who", "nobody")
  ui_render(f"<b data-do=go>{n} {who}</b>")
}
ui_on("click", (e) => { n = n + 1
  ui_storage_set("who", e["id"])
  draw() })
ui_on("input", (e) => { ui_render("typed:" + e["value"]) })
ui_every(60000, () => { n = n + 100 })
draw()
print(ui_escape("<a href=\\"x\\">&'</a>"), ui_clock()["hour"] >= 0)`, { onUi: (m) => out.push(m), storage: { who: "alice" } });

ok(r.ok && !!r.session, "a program with ui handlers stays alive as a session" + (r.ok ? "" : ": " + r.output));
ok(out[0].type === "render" && out[0].html.includes("0 alice"), "the first render uses the host's saved storage: " + out[0].html);
ok(/&lt;a href=&quot;x&quot;&gt;&amp;&#39;&lt;\/a&gt; true/.test(r.output), "ui_escape escapes HTML and ui_clock answers");
r.session.dispatch({ type: "click", id: "go", win: "", value: "" });
r.session.dispatch({ type: "click", id: "again", win: "", value: "" });
const renders = out.filter((m) => m.type === "render");
ok(renders[renders.length - 1].html.includes("2 again"), "clicks run the handler and re-render: " + renders[renders.length - 1].html);
ok(out.some((m) => m.type === "storage" && m.key === "who" && m.value === "again"), "ui_storage_set tells the host to save");
r.session.dispatch({ type: "input", id: "t", value: "hello" });
ok(out[out.length - 1].html === "typed:hello", "input events carry the typed text");
const errs = [];
const r2 = lite.runTon618(`IMPORT://ton.ui
ui_on("click", (e) => { throw "boom" })
ui_render("x")`, { onUi: (m) => { if (m.type === "error") errs.push(m.text); } });
r2.session.dispatch({ type: "click", id: "a" });
ok(errs.length === 1 && /boom/.test(errs[0]), "an error in a handler is reported to the host, not thrown: " + errs[0]);
const plain = lite.runTon618('print("no ui here")');
ok(plain.ok && plain.session === null, "a program without ui has no session");
r.session.stop(); r2.session.stop();
process.exit(failed ? 1 : 0);
