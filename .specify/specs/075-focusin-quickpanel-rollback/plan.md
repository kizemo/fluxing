# 075 - Plan

## Constitution Check

- **Principle I — code review × ship-gate = double verify** ✓ (L66 + L107 + L108 rules
  already enforced via AGENTS.md).
- **Principle II — Chinese UTF-8 + BOM + CRLF on install.nsi** ✓ (current patch keeps
  BOM + CRLF; byte-level verified after Edit).
- **Principle III — Conventional Commits + module prefix** ✓ (`fix(WeaselServerIPC):
  v0.19.0.74.3 input crash rollback`).
- **Principle IV — git status -sb + git diff --stat** ✓ (pre-commit self-check).
- **Principle V — reconnoitre before refactor + sub-agent verification** ✓
  (read source BEFORE write fix; subagent not used for trivial 5-line edits).

## Tech approach

最小可发货切片, 不动 D2D 或 GDI+ rewrite 大改。三个 surgical fix:

1. **`RimeWithWeasel/RimeWithWeasel.cpp` FocusIn** — 移除 v0.19.0.65 双份 Show,
   加 `if (false)` gate 对齐 L69-fix style.
2. **`WeaselIPCServer/WeaselServerImpl.{h,cpp}` OnUpdateInputPosition** — 把
   `GetProcAddress` 缓存到构造期 (member field `m_pfnPhysicalToLogical`), HWND
   用 `m_hWndMsg` 替换 `NULL`.
3. **`output/install.nsi` .onInit** — UserInfo + admin token check, silent
   non-admin SetErrorLevel 6002 + Quit, GUI non-admin 继续 + DetailPrint 警告.

无新增文件, 无 vcxproj 变更, xmake.lua 不变 (复用现有 .cpp 链接).
`env.bat` bump v0.74.1 → v0.74.3 (3 个 source files 改了,需要新 build).

## File-level changes

| Path | Type | Detail |
|---|---|---|
| `RimeWithWeasel/RimeWithWeasel.cpp:434-438` | Modify | 加 `if (false && ipc_id > 0)` gate |
| `RimeWithWeasel/RimeWithWeasel.cpp:487-492` | Modify | 删除 duplicate Show (line 487-491 + closing brace),留注释 |
| `WeaselIPCServer/WeaselServerImpl.h:103` | Modify | 加 `m_pfnPhysicalToLogical` field |
| `WeaselIPCServer/WeaselServerImpl.cpp:33-38` | Modify | 构造期 GetProcAddress 缓存 |
| `WeaselIPCServer/WeaselServerImpl.cpp:355-365` | Modify | Hot-path 改用 cached function pointer + `m_hWndMsg` |
| `output/install.nsi:.onInit` (after `Function .onInit`) | Modify | 加 admin token check block (~30 行) |
| `env.bat:24-25` | Modify | `WEASEL_PATCH=1` → `3`, `PRODUCT_VERSION=0.19.0.74.1` → `0.19.0.74.3` |
| `.specify/memory/lessons-learned.md` | Modify | 加 L##-PhaseL-9.11 entry (input crash rollback) |
| `.specify/specs/075-focusin-quickpanel-rollback/` | Create | spec.md (this), plan.md (this), tasks.md |

## Build / Test / Release impact

- Build: `xbuild.bat weasel installer` rebuild (per L107 双步 + L108 `_buildflow.cmd` rebuild)
  - WeaselServer.exe md5 必变 (FocusIn + admin check 都不影响, 但 OnUpdateInputPosition
    hot-path change 会改 binary md5)
  - 5 binary md5:
    - `weasel.dll` / `weaselx64.dll`: 不改 (无 TSF shim change)
    - `WeaselServer.exe`: 必改 (process_key_event IPC path 改)
    - `WeaselDeployer.exe`: 不改
    - `WeaselSetup.exe`: 不改
- Test:
  - `test/TestWeaselIPC` PASS
  - 新 test `test/TestFocusinQuickpanelDisabled`: 验证 FocusIn 不触发 QuickPanel
    Show (P2 - skip per spec if time budget insufficient)
  - `test/TestSystem32InlineCleanup` PASS (no NSIS change to cleanup macro itself)
- Installer: `_diag_v0741_evidence.ps1` Phase 2 SC-001-2 / SC-001-3 / SC-003-1
- Release: ship-gate reuses L##-PhaseL-9.10 list (5 binary md5 + sandbox-verify
  smoke 8 invariants + stage2 retained)

## Risk mitigation

- **R-001 (QuickPanel 重新禁用 UX)** — CHANGELOG + release notes + commit message
  文档化. v0.75 spec 076 dedicated to WeaselPanel GDI+ rewrite + re-enable QuickPanel
  after spec 074 stress test.
- **R-002 (admin check ARM64 误判)** — `${If} $R8 != "2" ${AndIf} $R8 != "3"`
  已经覆盖 standard + admin user types. 待 user 真机 test 验证.
