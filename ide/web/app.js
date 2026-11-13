// app.js — the TON618 IDE: files, tabs, running code, output.
// Works as a plain web page too (open web/index.html): the desktop/Android
// features (folders on disk, the native ton618 interpreter) are used only
// when running inside the Tauri app (window.__TAURI__).
"use strict";

const $ = (id) => document.getElementById(id);
const TAURI = window.__TAURI__ || null;
const invoke = TAURI ? TAURI.core.invoke : null;
const STORE_KEY = "ton618-ide";

const EXAMPLES = {
  "hello.ton": `// Welcome to the TON618 IDE! Press Ctrl+Enter (or ▶ Run) to run this file.
ton.string name = "world"
print(f"Hello, {name}!")

ton.array squares = (1..=5).map(n => n ** 2)
print(squares, "sum:", squares.sum())
`,
  "classes.ton": `ton.class Animal {
    ton.string sound = "..."
    ton.function init(name) { self.name = name }
    ton.function speak() { return f"{self.name} says {self.sound}" }
}

ton.class Dog extends Animal {
    ton.function init(name) {
        super.init(name)
        self.sound = "woof"
    }
}

ton.Dog rex = Dog("Rex")
print(rex.speak())
print(isinstance(rex, Animal), type(rex))
`,
  "modules.ton": `// IMPORT:// loads another file of the project (here: utils.ton)
IMPORT://utils
print(shout("modules work"))

IMPORT://utils as u
print(u.shout("namespaces too"))
`,
  "utils.ton": `ton.function shout(text) {
    return text.upper() + "!"
}
`,
  "errors.ton": `ton.function divide(a, b) {
    if b == 0 { throw {code: 400, message: "b must not be 0"} }
    return a / b
}

try {
    print(divide(10, 4))
    print(divide(1, 0))
} catch (err) {
    print("caught:", err.code, err.message)
}
`,
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
const state = {
  mode: "workspace",        // "workspace" (saved in the app) or "folder" (a folder on disk)
  folder: null,             // absolute path in folder mode
  files: {},                // name -> content (workspace) / relative path -> content cache (folder)
  names: [],                // the file list shown in the sidebar
  tabs: [],                 // open file names
  active: null,
  dirty: new Set(),
  running: false,
  platform: "web",          // web | windows | linux | macos | android | ios
  sep: "/",
};

function saveWorkspace() {
  if (state.mode !== "workspace") return;
  try {
    localStorage.setItem(STORE_KEY, JSON.stringify({ files: state.files, tabs: state.tabs, active: state.active }));
  } catch (e) { /* storage full or blocked: keep working in memory */ }
}

function loadWorkspace() {
  let saved = null;
  try { saved = JSON.parse(localStorage.getItem(STORE_KEY) || "null"); } catch (e) { saved = null; }
  state.mode = "workspace";
  state.folder = null;
  state.files = saved && saved.files && Object.keys(saved.files).length ? saved.files : { ...EXAMPLES };
  state.names = Object.keys(state.files).sort();
  state.tabs = (saved && saved.tabs || ["hello.ton"]).filter((n) => n in state.files);
  state.active = saved && saved.active in state.files ? saved.active : state.tabs[0] || state.names[0];
  if (state.active && !state.tabs.includes(state.active)) state.tabs.push(state.active);
  state.dirty.clear();
}

// ---------------------------------------------------------------------------
// Editor, sidebar, tabs
// ---------------------------------------------------------------------------
const editor = new TonEditor($("editor"), {
  onChange: (text) => {
    if (!state.active) return;
    state.files[state.active] = text;
    if (state.mode === "workspace") saveWorkspace();
    else state.dirty.add(state.active);
    renderTabs();
    scheduleCheck();
  },
  onRun: () => run(),
  onSave: () => saveActive(),
});

let checkTimer = 0;
function scheduleCheck() {
  clearTimeout(checkTimer);
  checkTimer = setTimeout(() => {
    const r = Ton618Lite.checkTon618(editor.value);
    editor.setError(r.ok ? 0 : r.line);
    if (!state.running) setStatus(r.ok ? "" : r.message, r.ok ? "" : "err");
  }, 350);
}

function renderFiles() {
  const list = $("fileList");
  list.innerHTML = "";
  for (const name of state.names) {
    const li = document.createElement("li");
    if (name === state.active) li.className = "active";
    const a = document.createElement("a");
    a.textContent = name;
    a.title = name;
    a.onclick = () => { openFile(name); document.body.classList.remove("show-files"); };
    li.appendChild(a);
    if (state.mode === "workspace") {
      const ren = document.createElement("button");
      ren.className = "icon"; ren.textContent = "✎"; ren.title = "Rename";
      ren.onclick = () => renameFile(name);
      const del = document.createElement("button");
      del.className = "icon"; del.textContent = "✕"; del.title = "Delete";
      del.onclick = () => deleteFile(name);
      li.append(ren, del);
    }
    list.appendChild(li);
  }
  $("project").textContent = state.mode === "folder" ? state.folder : "Workspace";
  $("filesTitle").textContent = state.mode === "folder" ? state.folder.split(/[\\/]/).pop() : "Workspace";
}

function renderTabs() {
  const bar = $("tabs");
  bar.innerHTML = "";
  for (const name of state.tabs) {
    const tab = document.createElement("div");
    tab.className = "tab" + (name === state.active ? " active" : "");
    tab.innerHTML = "";
    tab.append(name.split("/").pop());
    if (state.dirty.has(name)) { const d = document.createElement("span"); d.className = "dot"; d.textContent = " ●"; tab.append(d); }
    const x = document.createElement("span");
    x.textContent = "  ×";
    x.onclick = (e) => { e.stopPropagation(); closeTab(name); };
    tab.append(x);
    tab.onclick = () => openFile(name);
    bar.appendChild(tab);
  }
}

async function openFile(name) {
  if (!(name in state.files) && state.mode === "folder") {
    try { state.files[name] = await invoke("read_text", { path: joinPath(state.folder, name) }); }
    catch (e) { return print(String(e), "e"); }
  }
  if (!state.tabs.includes(name)) state.tabs.push(name);
  state.active = name;
  editor.value = state.files[name] ?? "";
  renderFiles();
  renderTabs();
  scheduleCheck();
  saveWorkspace();
}

function closeTab(name) {
  if (state.dirty.has(name) && !confirm(`${name} has unsaved changes. Close it anyway?`)) return;
  state.dirty.delete(name);
  state.tabs = state.tabs.filter((t) => t !== name);
  if (state.active === name) {
    state.active = state.tabs[state.tabs.length - 1] || null;
    editor.value = state.active ? state.files[state.active] : "";
  }
  renderFiles();
  renderTabs();
  saveWorkspace();
}

async function newFile() {
  let name = prompt("Name of the new file:", "untitled.ton");
  if (!name) return;
  name = name.trim();
  if (!/\.\w+$/.test(name)) name += ".ton";
  if (name in state.files || state.names.includes(name)) return alert(`${name} already exists.`);
  state.files[name] = "";
  if (state.mode === "folder") {
    try { await invoke("write_text", { path: joinPath(state.folder, name), content: "" }); }
    catch (e) { return print(String(e), "e"); }
  }
  state.names = [...state.names, name].sort();
  openFile(name);
}

function renameFile(name) {
  let to = prompt("New name:", name);
  if (!to || to === name) return;
  if (!/\.\w+$/.test(to)) to += ".ton";
  if (to in state.files) return alert(`${to} already exists.`);
  state.files[to] = state.files[name];
  delete state.files[name];
  state.names = Object.keys(state.files).sort();
  state.tabs = state.tabs.map((t) => (t === name ? to : t));
  if (state.active === name) state.active = to;
  renderFiles(); renderTabs(); saveWorkspace();
}

function deleteFile(name) {
  if (!confirm(`Delete ${name}?`)) return;
  delete state.files[name];
  state.names = Object.keys(state.files).sort();
  closeTab(name);
  renderFiles();
  saveWorkspace();
}

async function saveActive() {
  if (state.mode !== "folder" || !state.active) return;
  await saveFile(state.active);
  renderTabs();
}

async function saveFile(name) {
  try {
    await invoke("write_text", { path: joinPath(state.folder, name), content: state.files[name] });
    state.dirty.delete(name);
  } catch (e) { print(String(e), "e"); }
}

function joinPath(dir, rel) {
  return dir.replace(/[\\/]+$/, "") + state.sep + rel.split("/").join(state.sep);
}

async function openFolder() {
  const dir = await TAURI.dialog.open({ directory: true, multiple: false, title: "Open a folder" });
  if (!dir) return;
  let names;
  try { names = await invoke("list_files", { dir }); }
  catch (e) { return print(String(e), "e"); }
  state.mode = "folder";
  state.folder = dir;
  state.files = {};
  state.names = names;
  state.tabs = [];
  state.dirty.clear();
  state.active = null;
  editor.value = "";
  renderFiles(); renderTabs();
  const first = names.find((n) => /(^|\/)main\.ton$/.test(n)) || names.find((n) => n.endsWith(".ton"));
  if (first) openFile(first);
  try { localStorage.setItem(STORE_KEY + "-folder", dir); } catch (e) { /* ignore */ }
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------
// raw: append exactly `text` (chunks streamed by the native interpreter),
// instead of one line per call (print() calls of the built-in engine).
function print(text, cls, raw = false) {
  const out = $("output");
  const span = document.createElement("span");
  if (cls) span.className = cls;
  // "line N" in an error becomes a link that jumps to that line.
  if (cls === "e") {
    const parts = text.split(/(line \d+)/i);
    for (const p of parts) {
      const m = /line (\d+)/i.exec(p);
      if (m) {
        const a = document.createElement("a");
        a.textContent = p;
        a.onclick = () => editor.gotoLine(Number(m[1]));
        span.appendChild(a);
      } else span.append(p);
    }
  } else span.textContent = text;
  if (raw) out.append(span); else out.append(span, "\n");
  out.scrollTop = out.scrollHeight;
}

function setStatus(text, cls) {
  const s = $("status");
  s.textContent = text;
  s.className = "status " + (cls || "");
}

function errorLine(text) {
  const m = /(?:Error|error) at line (\d+)/.exec(text) || /[Ll]ine (\d+)/.exec(text);
  return m ? Number(m[1]) : 0;
}

// ---------------------------------------------------------------------------
// Running code
// ---------------------------------------------------------------------------
let worker = null;
let unlisten = [];
let startedAt = 0;

function setRunning(on) {
  state.running = on;
  $("run").hidden = on;
  $("stop").hidden = !on;
  $("inputForm").hidden = !(on && $("engine").value === "native");
  if (on) $("outputPane").classList.remove("collapsed");
}

function finish(ok, error) {
  if (error) {
    print(error, "e");
    const line = errorLine(error);
    if (line) editor.setError(line);
  }
  const ms = Date.now() - startedAt;
  setStatus(ok ? `finished in ${ms} ms` : "stopped with an error", ok ? "ok" : "err");
  setRunning(false);
}

async function run() {
  if (state.running || !state.active) return;
  clearTimeout(checkTimer); // don't let a pending syntax check clear the run's error marker
  $("output").textContent = "";
  editor.setError(0);
  startedAt = Date.now();
  setStatus("running…");
  setRunning(true);
  if ($("engine").value === "native" && invoke) return runNative();

  // Built-in engine, in a worker. Every project file is passed along so
  // IMPORT://name finds name.ton (or modules/name.ton).
  const files = { ...state.files };
  if (state.mode === "folder") {
    for (const n of state.names) {
      if (n.endsWith(".ton") && !(n in files)) {
        try { files[n] = await invoke("read_text", { path: joinPath(state.folder, n) }); } catch (e) { /* skip */ }
      }
    }
  }
  worker = new Worker("worker.js");
  worker.onmessage = (e) => {
    const m = e.data;
    if (m.type === "out") print(m.text);
    else { worker.terminate(); worker = null; finish(m.ok, m.error); }
  };
  worker.onerror = (e) => { worker.terminate(); worker = null; finish(false, e.message || "The engine crashed."); };
  worker.postMessage({ source: editor.value, files });
}

async function runNative() {
  for (const off of unlisten) off();
  unlisten = [
    await TAURI.event.listen("run-output", (e) => print(e.payload.text, e.payload.stream === "stderr" ? "e" : "", true)),
    await TAURI.event.listen("run-exit", (e) => finish(e.payload.code === 0, null)),
  ];
  try {
    if (state.mode === "folder") {
      for (const n of [...state.dirty]) await saveFile(n);
      renderTabs();
      await invoke("run_native", { dir: state.folder, file: state.active });
    } else {
      await invoke("run_native_files", { files: state.files, file: state.active });
    }
  } catch (e) {
    finish(false, String(e));
  }
}

async function stop() {
  if (worker) { worker.terminate(); worker = null; print("■ stopped", "i"); finish(false, null); }
  else if (invoke) { await invoke("stop_run").catch(() => {}); print("■ stopped", "i"); }
}

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------
$("run").onclick = run;
$("stop").onclick = stop;
$("newFile").onclick = newFile;
$("clear").onclick = () => { $("output").textContent = ""; setStatus(""); };
$("toggleOutput").onclick = () => $("outputPane").classList.toggle("collapsed");
$("toggleFiles").onclick = () => document.body.classList.toggle("show-files");
$("inputForm").onsubmit = (e) => {
  e.preventDefault();
  const text = $("input").value;
  $("input").value = "";
  print(text, "i");
  invoke("send_input", { text }).catch((err) => print(String(err), "e"));
};
$("engine").onchange = () => { try { localStorage.setItem(STORE_KEY + "-engine", $("engine").value); } catch (e) { /* ignore */ } };

for (const name of Object.keys(EXAMPLES)) {
  const o = document.createElement("option");
  o.value = name; o.textContent = name;
  $("examples").appendChild(o);
}
$("examples").onchange = () => {
  const name = $("examples").value;
  $("examples").value = "";
  if (!name) return;
  let target = name;
  if (state.files[target] !== undefined && state.files[target] !== EXAMPLES[name]) {
    let i = 2;
    while ((target = name.replace(".ton", `-${i}.ton`)) in state.files) i++;
  }
  state.files[target] = EXAMPLES[name];
  if (!state.names.includes(target)) state.names = [...state.names, target].sort();
  if (state.mode === "folder") { state.dirty.add(target); }
  openFile(target);
};

document.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter" && e.target.tagName !== "TEXTAREA") { e.preventDefault(); run(); }
  if ((e.ctrlKey || e.metaKey) && (e.key === "s" || e.key === "S") && e.target.tagName !== "TEXTAREA") { e.preventDefault(); saveActive(); }
});

window.addEventListener("beforeunload", (e) => { if (state.dirty.size) { e.preventDefault(); e.returnValue = ""; } });

(async function start() {
  loadWorkspace();
  if (invoke) {
    try { state.platform = await invoke("platform"); } catch (e) { state.platform = "web"; }
  }
  state.sep = state.platform === "windows" ? "\\" : "/";
  const desktop = ["windows", "linux", "macos"].includes(state.platform);
  $("openFolder").hidden = !desktop;
  $("openFolder").onclick = openFolder;
  // The native interpreter only exists in the desktop app.
  if (!desktop) $("engine").querySelector('[value="native"]').remove();
  let engine = "builtin";
  try { engine = localStorage.getItem(STORE_KEY + "-engine") || "builtin"; } catch (e) { /* ignore */ }
  $("engine").value = desktop ? engine : "builtin";
  renderFiles();
  if (state.active) openFile(state.active);
})();
