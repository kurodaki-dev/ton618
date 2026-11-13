// Runs TON618 code with the built-in engine (ton618-lite.js) off the UI
// thread, so a long or infinite loop never freezes the editor: the IDE's
// Stop button simply terminates this worker.
importScripts("ton618-lite.js");

self.onmessage = (e) => {
  const { source, files } = e.data;
  const started = Date.now();
  let printed = 0;
  const r = self.Ton618Lite.runTon618(source, {
    files,
    onPrint: (text) => { printed++; self.postMessage({ type: "out", text }); },
  });
  let error = null;
  if (!r.ok) {
    const lines = r.output.split("\n");
    error = lines.slice(printed).join("\n") || lines[lines.length - 1];
  }
  self.postMessage({ type: "done", ok: r.ok, error, ms: Date.now() - started });
};
