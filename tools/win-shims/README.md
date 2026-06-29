# Windows Build Shims

Librime source on Windows references headers that the upstream
`rime/librime` project does not ship or install:

| Header | Source / License | Why needed |
|---|---|---|
| `X11/keysym.h` + `X11/keysymdef.h` | X.Org Foundation, MIT | librime uses `XK_*` keysyms in `key_table.{h,cc}` and ~10 other source files. CMake's `find_path(X11Keysym X11/keysym.h)` (librime/CMakeLists.txt:149) will auto-discover the shim once it is on the include path. |
| `utf8.h` + `utf8/{checked,core,cpp17,unchecked}.h` | Nemanja Trifunovic, MIT | librime includes `<utf8.h>` in `src/rime/algo/*.cc` for UTF-8 string slicing. |
| `darts.h` + `COPYING.darts-clone` | Daisuke Okanohara, BSD | librime includes `<darts.h>` for the `Darts::DoubleArrayTrie` class. |

The shims are placed in `librime/include/` at build time via
`setup-shims.ps1`. `librime/.gitignore` lists `include/*` as ignored,
so the shims cannot be committed inside the librime submodule; they
live in this tracked parent-repo directory instead.

Run once before any librime build:

```
powershell -ExecutionPolicy Bypass -File tools\win-shims\setup-shims.ps1
```

The script is **idempotent** — re-running overwrites the shim files.

## Provenance

The X11/keysym.h, utf8.h, and darts.h files were copied verbatim from a
known-working peer fork at
`F:\soft\02office\rime\weasel\librime\include\` (originally derived from
the upstream `rime/librime` Windows port). They have not been modified.