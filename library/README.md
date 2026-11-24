# library/

The adaC standard library, written in adaC and embedded into the compiler.
Layers: `core` (no heap, no OS), `alloc` (heap), `std` (OS). See `docs/LIBRARY.md` for the full list and
`MODULES` for the registry (`always` / `gc` / `manual` / `opt`).
