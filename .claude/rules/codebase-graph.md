---
paths:
  - "Weasel**/*.cpp"
  - "Weasel**/*.h"
  - "include/**/*.h"
  - "test/**/*.cpp"
  - "RimeWithWeasel/**/*"
---

# Codebase graph

This project is indexed under `codebase-memory-mcp`
(`F-soft-00selfmade-rime_claude`, 12,679 nodes / 38,137 边).

## Lookup order (must)

`search_graph` → `trace_path` → `get_code_snippet` → `query_graph` → `get_architecture`。

## Downgrade to Grep / Read (and **state why** in the response)

- `search_graph` 返回 0 命中且符号理应在 scope
- 需要 `git blame` / commit history (图谱无时间轴)
- 二进制 / 非代码 (图、NSIS、exe)
- MCP 不可用 / 超时 / `expected_nodes=0`
- 会话中途源码改动 (先 `index_repository` 再继续)

## Defaults (saves 3 hops otherwise)

- `path_filter="^(Weasel|RimeWithWeasel|include|tools/win-shims|test)/"`
  drops `librime/` `thirdparty/` `plum/` leakage.
- `trace_path(function, direction="inbound", mode="calls")` 加
  `mode="data_flow"` 看参数传播。
- `min_degree: 5` 在 `HotkeyBinding` / `PipeChannel` 这种 hotspot 上过滤掉一过性函数。

## Anti-patterns (踩过的坑)

- `entry_points` 仍含 `librime/build_Win32/.../main` 即使该 dir 标记 excluded;需要时设 `exclude_entry_points=true`。
- `Macro` label 占 ~25% (SAL/ATL 由 `<windows.h>` 带入);搜代码时排除它。
- `get_code_snippet` 用 short name 不一定返回 source;传 `search_graph` 返回的完整 `qualified_name`。
