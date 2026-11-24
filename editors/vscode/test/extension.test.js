'use strict';
// Exercises extension.js against a mock of the vscode API and the REAL compiler.
// Usage: node test/extension.test.js /path/to/adac
const assert = require('assert');
const Module = require('module');
const fs = require('fs');
const os = require('os');
const path = require('path');

const adac = process.argv[2] ? path.resolve(process.argv[2]) : '';
assert.ok(adac && fs.existsSync(adac), 'pass the path of the adac compiler');

// ---------------- minimal vscode mock ----------------
class Position { constructor(l, c) { this.line = l; this.character = c; } isEqual(o) { return o.line === this.line && o.character === this.character; } }
class Range { constructor(a, b) { this.start = a; this.end = b; } }
class Diagnostic { constructor(range, message, severity) { this.range = range; this.message = message; this.severity = severity; } }
const store = new Map();
const commands = new Map();
const settings = { compilerPath: adac, checkOnChange: true, checkDelay: 10, strict: false, showWarnings: true };
const messages = [];
const sent = [];
let activeDoc = null;
const disposable = () => ({ dispose() {} });
const vscode = {
  Position, Range, Diagnostic,
  DiagnosticSeverity: { Error: 0, Warning: 1 },
  languages: { createDiagnosticCollection: () => ({ set: (u, d) => store.set(u.toString(), d), delete: (u) => store.delete(u.toString()), dispose() {} }) },
  window: {
    createOutputChannel: () => ({ appendLine() {}, append() {}, clear() {}, show() {}, dispose() {} }),
    showErrorMessage: (m) => { messages.push(m); return Promise.resolve(); },
    showWarningMessage: (m) => { messages.push(m); return Promise.resolve(); },
    showInformationMessage: (m) => { messages.push(m); return Promise.resolve(); },
    setStatusBarMessage: (m) => messages.push(m),
    createTerminal: () => ({ show() {}, sendText: (t) => sent.push(t) }),
    get activeTextEditor() { return activeDoc ? { document: activeDoc } : undefined; },
  },
  workspace: {
    getConfiguration: () => ({ get: (k) => settings[k] }),
    getWorkspaceFolder: () => ({ uri: { fsPath: path.dirname(adac) } }),
    onDidOpenTextDocument: disposable, onDidSaveTextDocument: disposable, onDidChangeTextDocument: disposable,
    onDidCloseTextDocument: disposable, onDidChangeConfiguration: disposable, textDocuments: [],
  },
  commands: { registerCommand: (id, fn) => { commands.set(id, fn); return disposable(); }, executeCommand() {} },
  env: { shell: '/bin/bash' },
};
const origLoad = Module._load;
Module._load = function (request) { return request === 'vscode' ? vscode : origLoad.apply(this, arguments); };
const ext = require('../extension.js');

function fakeDoc(file, text) {
  const lines = text.split('\n');
  return {
    languageId: 'adac', uri: { scheme: 'file', fsPath: file, toString: () => 'file://' + file },
    lineCount: lines.length, getText: () => text, lineAt: (i) => ({ text: lines[i] }),
    save: async () => { fs.writeFileSync(file, text); return true; },
    getWordRangeAtPosition: (p) => {
      const t = lines[p.line]; const re = /[A-Za-z_]\w*/g; let m;
      while ((m = re.exec(t))) if (p.character >= m.index && p.character <= m.index + m[0].length) return new Range(new Position(p.line, m.index), new Position(p.line, m.index + m[0].length));
      return undefined;
    },
  };
}
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'adac-ext-'));
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  ext.activate({ subscriptions: [] });
  assert.deepStrictEqual([...commands.keys()].sort(), ['adac.build', 'adac.check', 'adac.run']);

  // 1) an unsaved buffer with two errors and a warning-free body
  const file = path.join(tmp, 'broken.adac');
  const doc = fakeDoc(file, 'void main() {\n    int x = "hi";\n    println(y);\n}\n');
  activeDoc = doc;
  commands.get('adac.check')();
  await wait(600);
  const diags = store.get(doc.uri.toString());
  assert.ok(diags && diags.length === 2, 'expected 2 diagnostics, got ' + (diags && diags.length));
  assert.strictEqual(diags[0].range.start.line, 1);
  assert.ok(diags[0].message.includes("cannot convert 'String' to 'int'"));
  assert.strictEqual(diags[1].range.start.line, 2);
  assert.strictEqual(diags[1].range.end.character - diags[1].range.start.character, 1, 'range covers the word y');
  assert.ok(!fs.existsSync(file), 'checking must not require the file on disk');
  console.log('extension: live check of an unsaved buffer produced', diags.length, 'diagnostics with correct ranges');

  // 2) fix the text: diagnostics clear
  const good = fakeDoc(file, 'void main() {\n    int x = 1;\n    println(x);\n}\n');
  activeDoc = good;
  commands.get('adac.check')();
  await wait(600);
  assert.strictEqual(store.get(good.uri.toString()).length, 0);
  console.log('extension: diagnostics cleared after the fix');

  // 3) a warning (probable typo) is reported as a warning, and hidden when showWarnings is off
  const warn = fakeDoc(file, 'void main() {\n    total = 1;\n    println(total);\n    totl = 2;\n    println(totl);\n}\n');
  activeDoc = warn;
  commands.get('adac.check')();
  await wait(600);
  const wd = store.get(warn.uri.toString());
  assert.ok(wd.some((d) => d.severity === 1 && d.message.includes('did you mean')), 'typo warning expected');
  settings.showWarnings = false;
  commands.get('adac.check')();
  await wait(600);
  assert.strictEqual(store.get(warn.uri.toString()).length, 0);
  settings.showWarnings = true;
  console.log('extension: warnings shown / hidden by the setting');

  // 4) build and run
  const prog = path.join(tmp, 'hello.adac');
  activeDoc = fakeDoc(prog, 'void main() { println("hi from the extension"); }\n');
  await commands.get('adac.run')();
  assert.ok(fs.existsSync(path.join(tmp, 'hello.exe')), 'the .exe must be created');
  assert.ok(sent.length === 1 && sent[0].includes('hello.exe'), 'the program is launched in a terminal: ' + sent);
  console.log('extension: build + run created hello.exe and launched it');

  // 4b) a relative compiler path is resolved against the workspace folder
  settings.compilerPath = './' + path.basename(adac);
  store.clear();
  activeDoc = fakeDoc(file, 'void main() { int x = "hi"; }\n');
  commands.get('adac.check')();
  await wait(600);
  assert.ok((store.get(activeDoc.uri.toString()) || []).length === 1, 'relative compilerPath must work');
  settings.compilerPath = adac;
  console.log('extension: relative compilerPath resolved against the workspace folder');

  // 5) compiler not found: a single helpful error, no crash
  settings.compilerPath = path.join(tmp, 'no-such-compiler');
  commands.get('adac.check')();
  await wait(300);
  assert.ok(messages.some((m) => m.includes('could not run the compiler')));
  console.log('extension: missing compiler is reported once');
  ext.deactivate();
  fs.rmSync(tmp, { recursive: true, force: true });
})().catch((e) => { console.error(e); process.exit(1); });
