'use strict';
const vscode = require('vscode');
const cp = require('child_process');
const path = require('path');
const { parseCompilerOutput } = require('./lib/diagnostics');

let collection;          // vscode.DiagnosticCollection
let output;              // vscode.OutputChannel
const timers = new Map();    // document uri -> debounce timer
const running = new Map();   // document uri -> child process of the current check
let warnedMissing = false;

function config(doc) {
  return vscode.workspace.getConfiguration('adac', doc ? doc.uri : undefined);
}

/**
 * The compiler setting may be a bare command ("adac", found through PATH), an absolute path, or a relative path
 * such as "./adac.exe" or "build/adac" (resolved against the workspace folder that contains the file).
 */
function compilerFor(doc) {
  const setting = config(doc).get('compilerPath') || 'adac';
  if (path.isAbsolute(setting) || !/[\\/]/.test(setting)) return setting;
  const folder = vscode.workspace.getWorkspaceFolder ? vscode.workspace.getWorkspaceFolder(doc.uri) : undefined;
  return path.resolve(folder ? folder.uri.fsPath : path.dirname(doc.uri.fsPath), setting);
}

function samePath(a, b) {
  const na = path.normalize(a), nb = path.normalize(b);
  return process.platform === 'win32' ? na.toLowerCase() === nb.toLowerCase() : na === nb;
}

function missingCompiler(compiler) {
  if (warnedMissing) return;
  warnedMissing = true;
  vscode.window
    .showErrorMessage(
      `adaC: could not run the compiler '${compiler}'. Build it (make / build.bat) and set "adac.compilerPath" to its full path.`,
      'Open Settings')
    .then((choice) => {
      if (choice === 'Open Settings') vscode.commands.executeCommand('workbench.action.openSettings', 'adac.compilerPath');
    });
}

/** Turns the compiler's problems into VS Code diagnostics for `doc`. */
function toDiagnostics(doc, items, showWarnings) {
  const result = [];
  for (const it of items) {
    if (it.severity === 'warning' && !showWarnings) continue;
    let line = it.line - 1, col = it.col - 1, message = it.message;
    if (it.file === null || !samePath(it.file, doc.uri.fsPath)) {
      // problem in another file (an imported module or the library): show it at the top of this file
      message = it.file === null ? message : `[${path.basename(it.file)}:${it.line}] ${message}`;
      line = 0;
      col = 0;
    }
    line = Math.max(0, Math.min(line, doc.lineCount - 1));
    const text = doc.lineAt(line).text;
    col = Math.max(0, Math.min(col, text.length));
    const pos = new vscode.Position(line, col);
    let range = doc.getWordRangeAtPosition(pos);
    if (!range || !range.start.isEqual(pos)) {
      range = new vscode.Range(pos, new vscode.Position(line, Math.min(text.length, col + 1)));
    }
    const d = new vscode.Diagnostic(
      range, message,
      it.severity === 'warning' ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error);
    d.source = 'adac';
    result.push(d);
  }
  return result;
}

/** Returns extra CLI arguments from the user's settings. */
function extraArgs(doc) {
  const args = [];
  const cfg = config(doc);
  if (cfg.get('strict')) args.push('--strict');
  const libPath = cfg.get('libPath');
  if (libPath && typeof libPath === 'string' && libPath.trim().length > 0) {
    args.push('-L', libPath.trim());
  }
  return args;
}

/** Runs `adac --check` on the text of the document (no need to save). */
function check(doc, done) {
  if (!doc || doc.languageId !== 'adac' || doc.uri.scheme !== 'file') return;
  const compiler = compilerFor(doc);
  const file = doc.uri.fsPath;
  const args = ['--check', '--no-color', '--stdin-name', file, ...extraArgs(doc)];

  const key = doc.uri.toString();
  const previous = running.get(key);
  if (previous) previous.kill();

  let child;
  try {
    child = cp.spawn(compiler, args, { cwd: path.dirname(file) });
  } catch (e) {
    missingCompiler(compiler);
    return;
  }
  running.set(key, child);
  let stderr = '';
  child.stdout.on('data', () => {});
  child.stderr.on('data', (d) => { stderr += d.toString(); });
  child.stdin.on('error', () => {});
  child.on('error', (e) => {
    running.delete(key);
    if (e && e.code === 'ENOENT') missingCompiler(compiler);
  });
  child.on('close', () => {
    if (running.get(key) !== child) return;          // a newer check replaced this one
    running.delete(key);
    const items = parseCompilerOutput(stderr);
    const cfg = config(doc);
    collection.set(doc.uri, toDiagnostics(doc, items, cfg.get('showWarnings') !== false));
    if (done) done(items.filter((i) => i.severity === 'error').length);
  });
  child.stdin.end(doc.getText());
}

function schedule(doc) {
  if (doc.languageId !== 'adac') return;
  const key = doc.uri.toString();
  clearTimeout(timers.get(key));
  const delay = config(doc).get('checkDelay');
  timers.set(key, setTimeout(() => check(doc), typeof delay === 'number' ? delay : 350));
}

/** Builds the active file; resolves to the .exe path or null on failure. */
async function build() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== 'adac') {
    vscode.window.showWarningMessage('adaC: open an .adac file first.');
    return null;
  }
  const doc = editor.document;
  await doc.save();
  const compiler = compilerFor(doc);
  const exe = doc.uri.fsPath.replace(/\.adac$/i, '') + '.exe';
  const args = [doc.uri.fsPath, '-o', exe, '--no-color', ...extraArgs(doc)];
  output.clear();
  output.show(true);
  output.appendLine(`> ${compiler} ${args.map((a) => (a.includes(' ') ? `"${a}"` : a)).join(' ')}`);
  return new Promise((resolve) => {
    let child;
    try {
      child = cp.spawn(compiler, args, { cwd: path.dirname(doc.uri.fsPath) });
    } catch (e) {
      missingCompiler(compiler);
      return resolve(null);
    }
    child.stdout.on('data', (d) => output.append(d.toString()));
    child.stderr.on('data', (d) => output.append(d.toString()));
    child.on('error', (e) => {
      if (e && e.code === 'ENOENT') missingCompiler(compiler);
      resolve(null);
    });
    child.on('close', (code) => resolve(code === 0 ? exe : null));
  });
}

async function run() {
  const exe = await build();
  if (!exe) {
    vscode.window.showErrorMessage('adaC: compilation failed (see the "adaC" output).');
    return;
  }
  const term = vscode.window.createTerminal({ name: 'adaC', cwd: path.dirname(exe) });
  term.show();
  const q = `"${exe}"`;
  if (process.platform === 'win32') {
    const ps = /pwsh|powershell/i.test(vscode.env.shell || '');
    term.sendText((ps ? '& ' : '') + q);
  } else {
    term.sendText('wine ' + q);        // the generated program is a Windows executable
  }
}

function activate(context) {
  collection = vscode.languages.createDiagnosticCollection('adac');
  output = vscode.window.createOutputChannel('adaC');
  context.subscriptions.push(collection, output);

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument((d) => check(d)),
    vscode.workspace.onDidSaveTextDocument((d) => check(d)),
    vscode.workspace.onDidChangeTextDocument((e) => {
      if (config(e.document).get('checkOnChange')) schedule(e.document);
    }),
    vscode.workspace.onDidCloseTextDocument((d) => {
      collection.delete(d.uri);
      const key = d.uri.toString();
      clearTimeout(timers.get(key));
      timers.delete(key);
    }),
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration('adac')) vscode.workspace.textDocuments.forEach((d) => check(d));
    }),
    vscode.commands.registerCommand('adac.check', () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) return;
      check(editor.document, (errors) => {
        if (errors === 0) vscode.window.setStatusBarMessage('adaC: no errors', 3000);
      });
    }),
    vscode.commands.registerCommand('adac.build', async () => {
      const exe = await build();
      if (exe) vscode.window.showInformationMessage(`adaC: built ${path.basename(exe)}`);
    }),
    vscode.commands.registerCommand('adac.run', run)
  );

  vscode.workspace.textDocuments.forEach((d) => check(d));
}

function deactivate() {
  for (const c of running.values()) c.kill();
  running.clear();
}

module.exports = { activate, deactivate };
