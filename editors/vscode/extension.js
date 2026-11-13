// TON618 for VS Code: run the current file, open the REPL, and show syntax
// errors (from `ton618 --check`) when a .ton file is opened or saved.
// No dependency besides the VS Code API.
"use strict";

const vscode = require("vscode");
const { execFile } = require("child_process");
const path = require("path");

let terminal = null;
let diagnostics = null;
let warnedMissing = false;

function config() {
  return vscode.workspace.getConfiguration("ton618");
}

function interpreter() {
  return config().get("interpreterPath") || "ton618";
}

function quote(arg) {
  // Quoting understood by bash, zsh, PowerShell and cmd alike for ordinary paths.
  return /[\s"'$`&|;()]/.test(arg) ? `"${arg.replace(/"/g, '\\"')}"` : arg;
}

function getTerminal() {
  if (!terminal || terminal.exitStatus !== undefined) {
    terminal = vscode.window.createTerminal({ name: "TON618", env: { TON618_UNBUFFERED: "1" } });
  }
  return terminal;
}

function missingInterpreter() {
  if (warnedMissing) return;
  warnedMissing = true;
  vscode.window
    .showWarningMessage(
      `The ton618 interpreter was not found ("${interpreter()}"). Install it, or set "ton618.interpreterPath".`,
      "How to install"
    )
    .then((choice) => {
      if (choice) vscode.env.openExternal(vscode.Uri.parse("https://c-ton618.duckdns.org/docs/installing-building-the-interpreter/"));
    });
}

async function runFile(uri) {
  const doc = uri ? await vscode.workspace.openTextDocument(uri) : vscode.window.activeTextEditor?.document;
  if (!doc || doc.languageId !== "ton618") {
    vscode.window.showInformationMessage("Open a .ton file to run it.");
    return;
  }
  if (doc.isUntitled) {
    vscode.window.showInformationMessage("Save the file first, so ton618 can run it.");
    return;
  }
  if (doc.isDirty && config().get("saveBeforeRun")) await doc.save();
  const t = getTerminal();
  t.show(true);
  const file = doc.uri.fsPath;
  const cd = `cd ${quote(path.dirname(file))}`;
  t.sendText(`${cd} && ${quote(interpreter())} ${quote(path.basename(file))}`);
}

function openRepl() {
  const t = vscode.window.createTerminal({ name: "TON618 REPL", shellPath: interpreter() });
  t.show();
}

function check(doc) {
  if (!doc || doc.languageId !== "ton618" || doc.isUntitled || doc.uri.scheme !== "file") return;
  execFile(interpreter(), ["--check", doc.uri.fsPath], { timeout: 10000 }, (err, stdout, stderr) => {
    if (err && err.code === "ENOENT") {
      missingInterpreter();
      return;
    }
    const list = [];
    // "<file>:<line>: <message>"
    for (const line of String(stderr).split(/\r?\n/)) {
      const m = /:(\d+): (.*)$/.exec(line);
      if (!m) continue;
      const n = Math.max(0, Math.min(Number(m[1]) - 1, doc.lineCount - 1));
      const range = doc.lineAt(n).range;
      const d = new vscode.Diagnostic(range, m[2], vscode.DiagnosticSeverity.Error);
      d.source = "ton618";
      list.push(d);
    }
    diagnostics.set(doc.uri, list);
  });
}

function activate(context) {
  diagnostics = vscode.languages.createDiagnosticCollection("ton618");
  context.subscriptions.push(
    diagnostics,
    vscode.commands.registerCommand("ton618.run", runFile),
    vscode.commands.registerCommand("ton618.repl", openRepl),
    vscode.commands.registerCommand("ton618.check", () => check(vscode.window.activeTextEditor?.document)),
    vscode.workspace.onDidSaveTextDocument((doc) => config().get("checkOnSave") && check(doc)),
    vscode.workspace.onDidOpenTextDocument((doc) => config().get("checkOnSave") && check(doc)),
    vscode.workspace.onDidCloseTextDocument((doc) => diagnostics.delete(doc.uri)),
    vscode.window.onDidCloseTerminal((t) => { if (t === terminal) terminal = null; })
  );
  for (const doc of vscode.workspace.textDocuments) if (config().get("checkOnSave")) check(doc);
}

function deactivate() {}

module.exports = { activate, deactivate };
