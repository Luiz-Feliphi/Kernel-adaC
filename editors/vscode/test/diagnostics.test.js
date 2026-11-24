'use strict';
// Checks the compiler-output parser against the REAL compiler.   Usage: node test/diagnostics.test.js /path/to/adac
const assert = require('assert');
const fs = require('fs');
const cp = require('child_process');
const path = require('path');
const { parseCompilerOutput } = require('../lib/diagnostics');

// 1) fixed text (also covers Windows paths with a drive letter)
const win = 'C:\\dev\\prog.adac:12:7: error: variable \'x\' is not declared\n    12 |     println(x);\n       |             ^\n' +
            'C:\\dev\\prog.adac:3:1: warning: variable \'t\' is assigned but never read\n' +
            'C:\\dev\\prog.adac:3:1: note: declared here\nadac: fatal error: could not open \'lib.adac\'\n2 error(s). Compilation aborted.\n';
const r = parseCompilerOutput(win);
assert.strictEqual(r.length, 3);
assert.deepStrictEqual([r[0].file, r[0].line, r[0].col, r[0].severity], ['C:\\dev\\prog.adac', 12, 7, 'error']);
assert.strictEqual(r[1].severity, 'warning');
assert.ok(r[1].message.includes('note: declared here'), 'notes are attached to the previous problem');
assert.strictEqual(r[2].file, null);
console.log('diagnostics: fixed-text assertions passed');

// 2) the real compiler on the original syntax catalogue (it must report errors) and on a clean program
const adac = process.argv[2];
if (adac) {
  const root = path.join(path.dirname(adac), 'examples');
  const bad = path.join(root, 'test_original_english.adac');
  const text = fs.readFileSync(bad, 'utf8');
  const res = cp.spawnSync(adac, ['--check', '--no-color', '--stdin-name', bad], { input: text, encoding: 'utf8' });
  const items = parseCompilerOutput(res.stderr);
  const errors = items.filter((i) => i.severity === 'error');
  assert.ok(errors.length >= 10, 'expected many errors, got ' + errors.length);
  assert.ok(errors.every((e) => e.file === bad && e.line > 0 && e.col > 0), 'every error has a location in the checked file');
  console.log(`diagnostics: real compiler reported ${errors.length} located errors for the syntax catalogue`);

  const good = path.join(root, 'all_gc.adac');
  const res2 = cp.spawnSync(adac, ['--check', '--no-color', '--stdin-name', good], { input: fs.readFileSync(good, 'utf8'), encoding: 'utf8' });
  assert.strictEqual(parseCompilerOutput(res2.stderr).filter((i) => i.severity === 'error').length, 0);
  assert.strictEqual(res2.status, 0);
  console.log('diagnostics: a valid program produces no errors');

  // unsaved text differs from the file on disk: the stdin text is what gets checked
  const res3 = cp.spawnSync(adac, ['--check', '--no-color', '--stdin-name', good], { input: 'void main() { int a = "x"; }\n', encoding: 'utf8' });
  assert.strictEqual(parseCompilerOutput(res3.stderr)[0].line, 1);
  console.log('diagnostics: unsaved buffer text is checked (not the file on disk)');
}
