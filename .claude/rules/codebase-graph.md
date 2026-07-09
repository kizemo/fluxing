---
paths:
  - "Weasel**/*.cpp"
  - "Weasel**/*.h"
  - "include/**/*.h"
  - "test/**/*.cpp"
  - "RimeWithWeasel/**/*"
---

# Codebase graph exploration rules

**Skill-mandated order** before any `Grep` / `Read` of source. If this
project is indexed under `codebase-memory-mcp`, the graph is the
authoritative navigation layer.

## Lookup order (do not skip)

1. `search_graph(name_pattern=...)` or `search_graph(query=...)`
2. `trace_path(function_name=..., mode=calls|data_flow)` for chains
3. `get_code_snippet(qualified_name=...)` for exact ranges
4. `query_graph(...)` for multi-hop patterns
5. `get_architecture(aspects=[...])` for the high-level map

## Fallback rules — when Grep / Read is the right move

Downgrade to `Grep` / `Read` (and **state why** in the response) only if:

1. `search_graph` returns 0 results *and* the symbol is plausibly in scope.
2. You need `git blame` / commit history — graph has no time axis.
3. You need a non-source artifact (binary, image, NSIS script).
4. The MCP server is unavailable, the tool call timed out, or `expected_nodes = 0`.
5. You are verifying a transient build-state change (e.g. the file was
   edited mid-session and the indexer hasn't re-run; in that case trigger
   `index_repository` first, then continue).

## Filter conventions

- Default `path_filter="^(Weasel|RimeWithWeasel|include|tools/win-shims|test)/"`
  to drop `librime/`, `thirdparty/`, `plum/` leakage from upstream hits.
- When searching for "the call that triggers this", use
  `trace_path(function_name, mode="calls", direction="inbound")` plus
  `mode="data_flow"` for parameter propagation.
- For hotspots inside the codebase layer (e.g. `HotkeyBinding`, `PipeChannel`),
  set `min_degree: 5` to skip one-off utility functions.

## Anti-patterns (do not do)

- **Do not** trust the entry-points list: it still surfaces
  `librime/build_Win32/.../main` symbols despite the directory being
  marked excluded — pass `exclude_entry_points=true` if fan-out matters.
- **Do not** search on the `Macro` label without filtering: ~25 % of nodes
  are SAL/ATL macros brought in by `<windows.h>`; they have no architectural
  value. Filter label to `Function|Method|Class` unless you really want macros.
- **Do not** assume `get_code_snippet(qualified_name=...)` returns
  line-numbered source on a short name — when in doubt pass the fully
  qualified name exactly as `search_graph` returned it.
