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
(`F-soft-00selfmade-rime-claude`, 12,715 nodes / 38,206 边).

## Lookup order (must)

`search_graph` → `trace_path` → `get_code_snippet` → `query_graph` → `get_architecture`。

## 本机 MCP 现状（2026-11-01 实测）

- **唯一权威图谱 = `codebase-memory-mcp`**（原生二进制 `codebase-memory-mcp.exe` v0.9.0，stdio，
  已注册进 MiniMax Code 且 `enabled=true`）。
  索引项目名 = `F-soft-00selfmade-rime-claude`（**连字符**，不是下划线）；
  `auto_watch=true` ⇒ 索引跟随 HEAD 自动更新；`auto_index=false` ⇒ 不做全量自动索引。
- **`graphify` MCP 保持 `enabled=false`，不要用它取证**：它只能读 `graphify-out/` 里的
  **静态快照**（387 个文件 / `graph.json` 7.3 MB，生成于 2026-10-22，内容停在 spec 028 时代），
  没有增量更新能力，已明显过期。`graphify-out/` 由 `.gitignore:151` 隔离。
- **不想付工具 schema 的常驻 token 时**：`mavis mcp update codebase-memory-mcp --enabled false`
  一键关停；或完全绕开 MCP，走 CLI
  `codebase-memory-mcp cli <tool> --project <name> ...`（0.9.0 的 raw JSON 传参已 deprecated）。
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
