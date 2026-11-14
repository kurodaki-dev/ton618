// The `requests` package running on the JavaScript engine (Acode, playground), with a
// fake synchronous XMLHttpRequest: it needs ton.requests and ton.encoding there.
const fs = require("fs"), path = require("path");
const root = path.join(__dirname, "..");
const calls = [];
global.XMLHttpRequest = class {
  open(method, url) { this.method = method; this.url = url; this.h = {}; }
  setRequestHeader(k, v) { this.h[k] = v; }
  send(body) {
    calls.push({ method: this.method, url: this.url, headers: this.h, body });
    this.status = this.url.includes("/nope") ? 404 : 200;
    this.responseText = JSON.stringify({ method: this.method, url: this.url, auth: this.h.Authorization || null, body });
    this.responseURL = this.url;
  }
  getAllResponseHeaders() { return "Content-Type: application/json\r\nX-Test: yes\r\n"; }
};
require(path.join(root, "exemples/ton618-lite.js")); const lite = globalThis.Ton618Lite;
const files = { "requests.ton": fs.readFileSync(path.join(root, "registry/sources/requests/requests.ton"), "utf8") };
const out = lite.runTon618(`IMPORT://requests
ton.let r = requests.get("http://x.test/echo", {"params": {"q": "a b&c"}, "bearer": "tok"})
print(r.status, r.succeeded(), r.headers["x-test"], r.json().url, r.json().auth)
ton.let p = requests.post("http://x.test/p", {"json": {"name": "ton"}, "auth": ["u", "pw"]})
print(p.json().body, p.json().auth)
ton.let f = requests.post("http://x.test/f", {"data": {"a": "1 2", "b": "é"}})
print(f.json().body)
ton.let n = requests.get("http://x.test/nope")
try { n.raise_for_status() } catch (e) { print("caught:", e) }`, { files });
let failed = 0;
const ok = (c, m) => { console.log((c ? "ok   " : "FAIL ") + m); if (!c) failed++; };
ok(out.ok, "the script runs: " + (out.ok ? "" : out.output));
ok(/200 true yes http:\/\/x\.test\/echo\?q=a%20b%26c Bearer tok/.test(out.output), "GET with params, bearer token and headers");
ok(/\{"name":"ton"\} Basic dTpwdw==/.test(out.output), "POST JSON with basic auth");
ok(/a=1%202&b=%C3%A9/.test(out.output), "form body, URL-encoded");
ok(/caught: HTTP 404 for http:\/\/x\.test\/nope/.test(out.output), "raise_for_status throws on 404");
process.exit(failed ? 1 : 0);
