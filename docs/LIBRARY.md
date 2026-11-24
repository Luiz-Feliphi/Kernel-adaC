# The adaC standard library

Source: `library/`. Registry: `library/MODULES`. Layers follow Rust: **core** (no heap, no OS),
**alloc** (needs the heap), **std** (needs the OS; `use std::...` also reaches `core` and `alloc`).

## Using it

Prelude modules are always available. Optional modules are compiled into the program only when imported:

```c
use std::math;                  // module core/math
use std::collections::Stack;    // item path: resolves to alloc/collections/stack
use std::collections;           // every optional module in alloc/collections
use alloc::rc::Rc;              // explicit layer
```
An unknown path is an error: `unresolved import 'std::foo' (no such library module)`.
There are no namespaces yet: imported classes are global, so a program must not define a class with the same
name as a module it imports.

## Prelude (always loaded)

| Module | Provides |
|---|---|
| `std/sys` | process start-up, handles, `__write`, the exception handler chain |
| `std/io` | `print`, `println`, `printf` back-ends and `read`, `readInt`, `readFloat`, `readLine` |
| `core/error` | `Exception`, `RuntimeException`, `instanceof` support |
| `core/panic` | fatal runtime errors (null, bounds, division by zero, unhandled exception) |
| `core/str` | `length`, `equals`, `indexOf`, `contains`, `startsWith`, `endsWith`, `isEmpty`, `toInt`, `toFloat` |
| `core/num` | `abs`, `min`, `max`, `pow(base, intExp)` |
| `core/fmt` | number to text (`%d`, `%f`, `%x`...) |
| `core/dispatch` | calls through interfaces |
| `alloc/string` | `+`, `substring`, `trim`, `toUpperCase`, `toLowerCase`, `replace`, `split`, `repeat`, `toString` |
| `alloc/array` | matrices `new T[n][m]` |
| `alloc/collections/list` | `List<T>` |
| `alloc/collections/map` | `Map<String, V>` |
| `alloc/allocator_gc` / `allocator_manual` | the allocator for the chosen memory mode |

## Optional modules

| `use` path | Types | Main API |
|---|---|---|
| `std::math` | `Math`, `Int` | `Math.PI E LN2 LN10`, `floor ceil round trunc sin cos tan exp ln log10 pow hypot toRadians toDegrees gcd lcm clamp clampf signum`, `Int.MAX`, `Int.MIN` |
| `std::char` | `Char` | `isDigit isAlpha isAlnum isUpper isLower isSpace toUpper toLower toDigit fromDigit` |
| `std::option` | `Option<T>` | `new Option<T>()` (None), `new Option<T>(v)` (Some); `isSome isNone unwrap unwrapOr set clear` |
| `std::result` | `Result<T>` | `ok(v)`, `fail(msg)`, `isOk isErr error unwrap unwrapOr` |
| `std::string_builder` | `StringBuilder` | `append appendChar appendInt appendFloat length isEmpty clear toString` |
| `std::collections::Stack` | `Stack<T>` | `push pop peek size isEmpty clear` |
| `std::collections::Queue` | `Queue<T>` | `enqueue dequeue peek size isEmpty` |
| `std::collections::Set` | `Set` (of String) | `add contains remove size isEmpty items clear` |
| `std::boxed::Box` | `Box<T>` | `get set` |
| `std::rc::Rc` | `Rc<T>` | `clone get set strongCount drop` |
| `std::time` | `Time` | `nowMillis()`, `sleep(ms)` |
| `std::fs` | `Fs` | `exists(path)`, `readToString(path)` (null on failure), `writeString(path, text)` |
| `std::pc` | `Pc` | `run(cmd)`, `exec(cmd)`, `open(target)`, `env`, `setEnv`, `cwd`, `cd`, `mkdir`, `rmdir`, `rm`, `mv`, `cp`, `ls`, `exists`, `isDir`, `user`, `hostname`, `cpuCores`, `ramTotalMB`, `ramFreeMB`, `uptimeMs`, `beep`, `alert`, `confirm`, `clipboardGet`, `clipboardSet`, `mouseX`, `mouseY`, `setMouse`, `windowTitle` |
| `std::db` | `Database`, `Db`, `Sqlite` | `Db.open(path)`, KV (`set`, `get`, `has`, `del`, `keys`), Tables (`createTable`, `insert`, `insertRow`, `selectAll`, `where`, `updateWhere`, `deleteWhere`, `count`, `tables`), simple SQL (`exec`, `query`), `save`/`commit`, plus optional `Sqlite` (`sqlite3.dll`) |
| `std::web` | `Web`, `WebApp`, `HttpRequest`, `Json` | `Web.listen(port)`, `app.next()`, `app.req()` (`is`, `startsWith`, `param`, `query`, `header`, `body`), `app.status()`, `app.header()`, `app.text()`, `app.html()`, `app.json()`, `app.sendFile()`, `app.redirect()`, `Web.get(ip, port, path)`, `Json.obj/fromMap/fromList/parse` |
| `std::ui` | `Ui`, `UiApp`, `UiElement` | Native Win32 + anti-aliased DIB + ClearType UI (no Chromium). Two styling modes: literal CSS (`app.css(...)`, `app.loadCss(...)` with tags, `.class`, `#id`, `:hover`, `:active`, `:focus`, `linear-gradient`, `box-shadow`, `border-radius`) and inline fluent styling (`.bg()`, `.gradient()`, `.hoverBg()`, `.radius()`, `.paddingXY()`, `.shadow()`, `.bold()`) |
| `std::unicode` | `Utf8` | `len`, `at`, `codepointAt`, `fromCodepoint`, `sub`, `popChar`, `chars`, `toUpper`, `toLower`, `isValid`, `toUtf16`, `fromUtf16` |

`List<T>`: `add push_back get set insert removeAt pop size isEmpty clear indexOf contains`, `l[i]`, `l[i] = x`.
`Map<String,V>`: `put get has remove size isEmpty clear keys`, `m["k"]`, `m["k"] = v`.

## Writing a library module

1. Add `library/<layer>/<name>.adac` (a class with static members, or a generic class `class Name<T>`).
2. Register it in `library/MODULES` (`opt` for optional modules).
3. Add a test in `tests/library/` and run `python3 tests/harness/compiletest.py --bless <name>`.
Library code may use the intrinsics (`__winapi`, `__peek64`, `__poke64`, `__memcpy`, ...) and the `__alloc` family.
