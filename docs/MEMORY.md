# Memory management: Rust compared with adaC

## What Rust does

Rust has **no garbage collector**. Memory safety is decided at compile time:

* **Ownership** — every value has exactly one owner; when the owner goes out of scope the value is dropped
  (`Drop`, i.e. RAII). Assigning or passing a value *moves* it.
* **Borrowing** — `&T` (shared) and `&mut T` (exclusive) references; the compiler (`rustc_borrowck`) rejects
  programs where a reference could outlive its value or alias a mutable one. **Lifetimes** describe the scopes.
* **Smart pointers** — `Box<T>` (single owner on the heap), `Rc<T>` / `Arc<T>` (reference counted sharing;
  cycles can leak). There is no `null`: absence is `Option<T>`.
* **Layers** — `core` (no heap), `alloc` (heap types), `std` (OS).

## What adaC does

adaC gives the programmer a choice **per program**:

| | default: garbage collector | `#NOT_GARBAGE`: manual |
|---|---|---|
| allocation | `new`, strings, arrays, `List`... | same, plus `setMen(n)` |
| release | automatic (conservative mark-and-sweep, `library/alloc/allocator_gc.adac`) | `freeMEM(x)` |
| pointers | not needed | `T*`, `*p`, `&x`, `p << n`, `p >> n` |
| safety | no dangling pointers | **your responsibility** (like C) |

Collector summary: every block has a 16-byte header `[next][size|atomic|mark]`; roots are the global
variables and the whole stack; any word equal to a block start keeps it alive; strings are "atomic" (not
scanned); the threshold adapts to the live size. See `docs/ARCHITECTURE.md` for where the code lives.

## Rust ideas that were brought over

| Rust | adaC (library) |
|---|---|
| `Option<T>` instead of null | `Option<T>` (`use std::option;`) |
| `Result<T, E>` | `Result<T>` with a message (`use std::result;`) |
| `Box<T>` | `Box<T>` (`use std::boxed::Box;`) |
| `Rc<T>`, `Rc::clone`, `strong_count`, drop | `Rc<T>`: `clone()`, `strongCount()`, `drop()` (frees the shared cell when the count reaches 0) |
| `Vec`, `VecDeque`, `HashMap`, `HashSet` | `List<T>`, `Queue<T>`, `Map<String,V>`, `Set` |
| `core` / `alloc` / `std` layering | `library/core`, `library/alloc`, `library/std` |

## What is *not* implemented (and why)

* **Ownership/move checking and the borrow checker.** In Rust this is a whole compiler phase built on a
  mid-level IR (MIR). adaC has no such IR yet; a faithful port is a multi-month project.
* **Automatic `Drop` at end of scope (RAII).** Needs ownership information to know who must drop.
* **`Arc` / threads.** The runtime is single-threaded.
* **Lifetimes.** Not needed while references do not exist as a separate concept.

### Possible roadmap (smallest steps first)

1. `defer expr;` — run a statement when the enclosing scope exits (cheap, covers most RAII uses).
2. Destructors: a `destroy()` method called by `freeMEM(obj)` and at scope exit for `Box`/`Rc`.
3. An optional `#OWNERSHIP` mode: a flow analysis that rejects use-after-`freeMEM` and double frees
   (a linear-types check on locals), reported like the other diagnostics.
4. An intermediate representation, then a real move/borrow analysis on it.

## Honest caveats about the current generics

`class Name<T>` is implemented by *type erasure*: `T` is stored as a 64-bit word. You can store, return and
compare T for identity, and the compiler checks the type arguments at the use site (`Pair<int>`,
`Stack<String>`), but code *inside* the class cannot call methods on `T`.
