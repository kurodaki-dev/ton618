// Runs TON618 code with the built-in engine (ton618-lite.js) off the UI
// thread, so a long or infinite loop never freezes the editor: the IDE's
// Stop button simply terminates this worker.
importScripts("ton618-lite.js");

let session = null;   // a program that draws a screen (ton.ui) stays alive: the page sends it events

self.onmessage = (e) => {
  if (e.data.event) { if (session) session.dispatch(e.data.event); return; }
  const { source, files, storage } = e.data;
  const started = Date.now();
  let printed = 0;
  const r = self.Ton618Lite.runTon618(source, {
    files, storage,
    onPrint: (text) => { printed++; self.postMessage({ type: "out", text }); },
    onUi: (m) => self.postMessage({ type: "ui", ui: m }),
  });
  if (r.session) { session = r.session; session.start(); }
  let error = null;
  if (!r.ok) {
    const lines = r.output.split("\n");
    error = lines.slice(printed).join("\n") || lines[lines.length - 1];
  }
  self.postMessage({ type: "done", ok: r.ok, error, ms: Date.now() - started, interactive: !!r.session });
};
