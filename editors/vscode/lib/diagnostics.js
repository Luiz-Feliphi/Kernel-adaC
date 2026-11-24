'use strict';
/**
 * Parses the text the adaC compiler prints to stderr into plain diagnostic objects.
 * No dependency on the vscode module, so it can be unit-tested with plain node.
 *
 * Compiler format:
 *   path/file.adac:12:7: error: variable 'x' is not declared
 *        12 |     println(x);
 *           |             ^
 */

const LOCATED = /^(.+?):(\d+):(\d+): (error|warning|note): (.*)$/;
const FATAL = /^adac: fatal error: (.*)$/;

/**
 * @param {string} text  compiler stderr
 * @returns {{file: (string|null), line: number, col: number, severity: string, message: string}[]}
 *          line / col are 1-based; a diagnostic without location has file === null and line === 0.
 */
function parseCompilerOutput(text) {
  const out = [];
  for (const raw of text.split(/\r?\n/)) {
    let m = LOCATED.exec(raw);
    if (m) {
      const item = { file: m[1], line: parseInt(m[2], 10), col: parseInt(m[3], 10), severity: m[4], message: m[5] };
      if (item.severity === 'note' && out.length > 0) {
        out[out.length - 1].message += '\n' + 'note: ' + item.message;   // attach notes to the previous problem
      } else {
        out.push(item);
      }
      continue;
    }
    m = FATAL.exec(raw);
    if (m) out.push({ file: null, line: 0, col: 0, severity: 'error', message: m[1] });
  }
  return out;
}

module.exports = { parseCompilerOutput };
