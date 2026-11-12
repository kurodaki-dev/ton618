// Runs every tests/*.ton through the browser playground's engine
// (exemples/ton618-lite.js) and compares the output with tests/*.expected,
// so the playground can't silently drift from the real interpreter.
// Usage: node tests/run-lite.js
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
require(path.join(root, "exemples", "ton618-lite.js"));
let pass = 0, fail = 0;
for (const f of fs.readdirSync(__dirname).filter((f) => f.endsWith(".ton")).sort()) {
  const exp = path.join(__dirname, f.replace(/\.ton$/, ".expected"));
  if (!fs.existsSync(exp)) continue;
  const out = globalThis.Ton618Lite.runTon618(fs.readFileSync(path.join(__dirname, f), "utf8")).output;
  if (out.trim() === fs.readFileSync(exp, "utf8").trim()) { pass++; console.log("ok    " + f); }
  else { fail++; console.log("FAIL  " + f + "\n" + out); }
}
console.log(`${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);
