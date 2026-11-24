# compiler/

The adaC compiler. One directory per stage (see `docs/ARCHITECTURE.md`). Headers are included with their crate
path, e.g. `#include "adac_ast/ast.h"`; the include root is `compiler/`.
