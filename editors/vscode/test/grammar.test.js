'use strict';
// Tokenizes adaC code with the same engine VS Code uses and checks the scopes.
// Run:  NODE_PATH=/path/to/node_modules node test/grammar.test.js
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const vsctm = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm')).buffer;
const lib = oniguruma.loadWASM(wasm).then(() => ({
  createOnigScanner: (p) => new oniguruma.OnigScanner(p),
  createOnigString: (s) => new oniguruma.OnigString(s),
}));
const registry = new vsctm.Registry({
  onigLib: lib,
  loadGrammar: async (scope) =>
    scope === 'source.adac'
      ? vsctm.parseRawGrammar(fs.readFileSync(path.join(__dirname, '..', 'syntaxes', 'adac.tmLanguage.json'), 'utf8'), 'adac.tmLanguage.json')
      : null,
});

function tokenizeAll(grammar, text) {
  let ruleStack = vsctm.INITIAL;
  const lines = text.split('\n');
  return lines.map((line) => {
    const r = grammar.tokenizeLine(line, ruleStack);
    ruleStack = r.ruleStack;
    return r.tokens.map((t) => ({ text: line.substring(t.startIndex, t.endIndex), scopes: t.scopes }));
  });
}

(async () => {
  const grammar = await registry.loadGrammar('source.adac');
  const src = [
    '#NOT_GARBAGE',                                        // 0
    'use std::collections::Stack;',                        // 1
    'class Pair<T> extends Base implements Shape {',       // 2
    '    private float ratio = 3.14e2 + 0xFF + 0b101;',    // 3
    '    String s = "a\\n%5d";  // comment',                // 4
    '    for (int i = 0 to 10; i++) println(i);',          // 5
    '    obj.method(1).field = Math.PI;',                  // 6
    '    List<int> xs = new List<int>(); /* block',         // 7
    '       still comment */ int after;',                  // 8
    "    char c = 'x'; bool ok = true && !false;",         // 9
    '    int* p = (int*) setMen(8); p = p >> 2;',          // 10
  ].join('\n');
  const lines = tokenizeAll(grammar, src);
  const has = (li, text, scope) => {
    const tok = lines[li].find((t) => t.text === text && t.scopes.some((s) => s.startsWith(scope)));
    assert.ok(tok, `line ${li}: expected "${text}" to have scope ${scope}\n  got: ${JSON.stringify(lines[li].filter((t) => t.text.trim()))}`);
  };

  has(0, 'NOT_GARBAGE', 'keyword.control.directive');
  has(1, 'use', 'keyword.control.import');
  has(1, 'std', 'entity.name.namespace');
  has(1, '::', 'punctuation.separator.namespace');
  has(1, 'Stack', 'entity.name.namespace');
  has(2, 'class', 'storage.type.class');
  has(2, 'Pair', 'entity.name.type.class');
  has(2, 'T', 'entity.name.type.parameter');
  has(2, 'extends', 'storage.modifier');
  has(2, 'Base', 'entity.name.type');
  has(3, 'private', 'storage.modifier');
  has(3, 'float', 'storage.type.primitive');
  has(3, '3.14e2', 'constant.numeric.float');
  has(3, '0xFF', 'constant.numeric.hex');
  has(3, '0b101', 'constant.numeric.binary');
  has(4, 'String', 'storage.type.primitive');
  has(4, '\\n', 'constant.character.escape');
  has(4, '%5d', 'constant.other.placeholder');
  has(4, '//', 'punctuation.definition.comment');
  has(4, ' comment', 'comment.line');
  has(5, 'for', 'keyword.control.loop');
  has(5, 'to', 'keyword.control.loop.range');
  has(5, 'println', 'support.function.builtin');
  has(6, 'method', 'entity.name.function.member');
  has(6, 'field', 'variable.other.property');
  has(6, 'Math', 'support.class');
  has(6, 'PI', 'variable.other.property');
  has(7, 'List', 'support.class');
  has(7, 'new', 'keyword.operator.new');
  has(7, '/*', 'punctuation.definition.comment.begin');
  has(7, ' block', 'comment.block');
  has(8, '*/', 'punctuation.definition.comment.end');
  has(8, 'int', 'storage.type.primitive');
  has(9, "'x'", 'string.quoted.single');
  has(9, 'true', 'constant.language.boolean');
  has(9, '&&', 'keyword.operator.logical');
  has(10, 'setMen', 'support.function.builtin');
  has(10, '>>', 'keyword.operator.bitwise');
  console.log('grammar: scope assertions passed');

  // every example and library file must tokenize without throwing, and comments/strings must close
  const bases = [path.join(__dirname, '..', '..', '..'), path.join(__dirname, '..', '..', 'adac')];   // repo layout / standalone layout
  const base = bases.find((b) => fs.existsSync(path.join(b, 'examples'))) || bases[0];
  const roots = [path.join(base, 'examples'), path.join(base, 'library')];
  let files = 0, tokens = 0;
  const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).forEach((e) => {
    const p = path.join(d, e.name);
    if (e.isDirectory()) { if (e.name !== 'bin') walk(p); }
    else if (e.name.endsWith('.adac')) {
      const out = tokenizeAll(grammar, fs.readFileSync(p, 'utf8'));
      files++; tokens += out.reduce((n, l) => n + l.length, 0);
      const last = out[out.length - 1] || [];
      assert.ok(!last.some((t) => t.scopes.some((s) => s.startsWith('comment.block'))) || fs.readFileSync(p, 'utf8').trim() === '', `unterminated block comment scope at end of ${p}`);
    }
  });
  roots.filter(fs.existsSync).forEach(walk);
  console.log(`grammar: tokenized ${files} real .adac files (${tokens} tokens) without errors`);
})().catch((e) => { console.error(e.message); process.exit(1); });
