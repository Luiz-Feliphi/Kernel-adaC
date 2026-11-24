# adaC Language Support

Editor support for the **adaC** programming language (`.adac` files) in Visual Studio Code.

* **Syntax highlighting** — keywords, types, the standard library classes (including `Pc`, `Db`, `Web`, `Ui`, `Utf8`…),
  directives (`#NOT_GARBAGE`, `#STRICT`…), `use` / `import` paths with `::` or `.` separators,
  `for (… to …)`, `printf` placeholders, Unicode escape sequences (`\uXXXX`, `\UXXXXXXXX`),
  numbers, strings and comments.
* **Live error checking** — syntax and semantic errors (and warnings) are underlined while you type, with the
  exact messages of the compiler. The text of the editor is checked, so you do not have to save.
* **Snippets** — `main`, `forto`, `class`, `import`, `webserver`, `uicss`, `dbsql`, `pcrun` … (type a prefix and press Tab).
* **Commands** — *adaC: Check Current File*, *adaC: Build Current File (.exe)*, *adaC: Build and Run Current File*
  (also a ▶ button in the editor title bar).

## What's New in 0.2.0

* **Module import syntax** — `import std::pc;`, `import std::db;`, `import std.web;`, `import mylib;`
  (in addition to the existing `import "file.adac";` path syntax).
* **New standard libraries** highlighted and with snippets:
  * `std::pc` — Run commands, file ops, system info, clipboard, mouse control (`Pc`)
  * `std::db` — Embedded key-value & SQL database with file persistence, SQLite FFI (`Db`, `Database`, `Sqlite`, `DbTable`)
  * `std::web` — HTTP/1.1 server with routing, JSON, HTTP client (`Web`, `WebApp`, `HttpRequest`, `Json`)
  * `std::ui` — Native Win32 GUI with CSS and fluent styling modes (`Ui`, `UiApp`, `UiElement`)
  * `std::unicode` — UTF-8/UTF-16 conversion, codepoint ops, case mapping (`Utf8`)
* **Unicode escape sequences** — `\uXXXX` and `\UXXXXXXXX` in strings are highlighted as escape characters.
* **Library search path** — New `adac.libPath` setting passes `-L <path>` to the compiler.

## Requirements

The adaC compiler (`adac` / `adac.exe`). Build it from the adaC repository (`make` or `build.bat`), then either
put its folder in your `PATH` or set the full path in the settings:

```json
{ "adac.compilerPath": "C:\\dev\\adac\\adac.exe" }
```

Error checking needs a compiler that supports `--stdin-name` (adaC compiler of this repository or newer).

## Install

* From a `.vsix`: *Extensions* view → `…` menu → **Install from VSIX…** → pick `adac-language-0.2.0.vsix`.
* Or from a terminal: `code --install-extension adac-language-0.2.0.vsix`.
* To try it from source: open this folder in VS Code and press **F5**.

## Settings

| Setting | Default | |
|---|---|---|
| `adac.compilerPath` | `adac` | The compiler: a command in your `PATH`, an absolute path, or a path relative to the workspace folder (`./adac.exe`) |
| `adac.libPath` | *(empty)* | Extra library search path (`-L`). The compiler already searches `libs/` and `lib/` relative to the source file |
| `adac.checkOnChange` | `true` | Check while typing (otherwise on open and save) |
| `adac.checkDelay` | `350` | Milliseconds to wait after the last keystroke |
| `adac.strict` | `false` | Pass `--strict` (assignment to an undeclared variable is an error) |
| `adac.showWarnings` | `true` | Show compiler warnings |

## Snippets

| Prefix | Inserts |
|---|---|
| `main` | `void main() { }` |
| `pl`, `pr`, `printf` | `println`, `print`, `printf` |
| `readint`, `readstr` | read from the console |
| `var`, `const` | variable / constant |
| `if`, `ifelse`, `switch` | conditionals |
| `forto`, `for`, `forint`, `foreach`, `while`, `dowhile` | loops (including the `to` and `int _` forms) |
| `fn`, `method`, `static` | functions and methods |
| `class`, `classext`, `abstract`, `interface`, `impl`, `generic`, `enum` | type declarations |
| `try`, `tryf`, `throw`, `exception` | exceptions |
| `list`, `map`, `array`, `arrlit`, `matrix` | collections and arrays |
| `use`, `usec`, `import`, `importf` | library / file imports |
| `importpc`, `importdb`, `importweb`, `importui`, `importutf` | import specific std modules |
| `pcrun`, `pcfile`, `pcsys` | PC control: run command, file ops, system info |
| `dbkv`, `dbsql`, `sqlite` | database: key-value, SQL tables, SQLite |
| `webserver`, `webjson`, `webclient` | web: HTTP server, JSON, HTTP client |
| `uicss`, `uifluent` | UI windows: CSS mode, fluent mode |
| `utf8`, `uescape` | Unicode / UTF-8 operations |
| `manual`, `strict`, `package`, `setmen` | directives and manual memory |
| `option`, `result`, `sb`, `tern`, `region` | library types and helpers |

## Standard Library Modules

| Module | Import | Description |
|---|---|---|
| `std::pc` | `import std::pc;` | System commands, file operations, environment, clipboard, mouse |
| `std::db` | `import std::db;` | Key-value store, relational tables with SQL, file persistence, SQLite FFI |
| `std::web` | `import std::web;` | HTTP/1.1 server with routing, JSON parser, HTTP client |
| `std::ui` | `import std::ui;` | Native Win32 GUI — CSS mode and fluent inline styling mode |
| `std::unicode` | `import std::unicode;` | UTF-8/UTF-16 conversion, codepoint operations, case mapping |

## How the checking works

Each check runs `adac --check --no-color --stdin-name <file>` and feeds the editor text through standard input
(about 3 ms per run). Problems in other files (imported modules) appear at the top of the file as
`[other.adac:LINE] message`.

## Development

```
NODE_PATH=<dir with vscode-textmate and vscode-oniguruma> node test/grammar.test.js
node test/diagnostics.test.js /path/to/adac
node test/extension.test.js   /path/to/adac
```
