# 075 - v0.74.3 input-crash rollback (Phase L Bugfix 9.11)

## 0. Background

v0.74.2 ship 后用户反馈: 装机 + admin 重启 + **输入中文** → 4 个不相关进程 (Claude
Code / dopus / HipsMain / aboboo + explorer.exe) **同时** 0xc0000374 HEAP_CORRUPTION
in `ntdll.dll`. 真机 forensic (`_diag_v0741_evidence.ps1`) 显示:
- v0.74.2 cleanup 留下的 System32 残留: `weasel.dll` (204 KB) + `weasel.old.dll` (1.16 MB)
  + `weaselx64.dll` (1.2 MB) 全在 `C:\WINDOWS\System32\`。
- 5 binary md5 跟 v0.74.1 一致 (装机包完整)。
- Application Error event count ≥ 12, all 0xc0000374, all in `ntdll.dll` at offset
  `0x0000000000112165` (跟 L69 signature 完全相同).
- `fluxing\crash` directory 不存在 (L67 SetUnhandledExceptionFilter 没把 dump 写出).
- Stage2 schtasks / pending.json 全 0 (per L##-PhaseL-9.10 预测, Win11 24H2 Sandbox
  LogonCommand flaky 模式).

**Root cause (high-confidence)**:
- **Path A** — System32 残留 old shim DLL (`weaselx64.dll` 是 v0.66 era, 1.2 MB,
  Rime 官方 2.0.0.9 旧 build). ctfmon.exe + TextInputHost.exe + explorer.exe 加载它,
  其 IPC 协议跟 v0.74.x 不兼容, 触发 COM heap 写越界 → 累加 → 0xc0000374.
  **真因非-flush 部分**. v0.74.2 cleanup silent fail (admin token missing or
  ctfmon 文件锁), v0.74.2 `IfErrors` DetailPrint 给运行安装的用户可见,但
  silent `/S` 模式下 deploy 自动化 catch 不到退出码.
- **Path B** — `RimeWithWeasel::FocusIn` 在 v0.19.0.65 (commit `505c398`) 重新启用
  `QuickPanelDialog::Show(...)`, 但 commit 同时引入双份 Show (line 434-438 + 487-491)
  + 第一份跨 `m_disabled` gate (line 434). GDI 资源反复 churn + `WeaselPanel` 还
  用 `GdiplusStartup` (line 107 of `WeaselUI/WeaselPanel.cpp`) 是 L67-L69 雷区
  真正残留路径 (spec 074 只重写 QuickPanel, 没重写 WeaselPanel).

## 1. Goals
G1. 装机 v0.74.3 + admin reboot 后, 连续输入中文 ≥10 次不发生任何 Application
    Error event (counter 0).
G2. 装机 v0.74.3 后 `HKLM\Software\Fluxing\Weasel\AdminRequired` registry 仅在
    silent + non-admin 模式下设 1 (deploy script 可 query).
G3. 装机 v0.74.3 后 `Application Error` event log 0xc0000374 count (24h 内) ≤ 5
    (任意进程), 装机前 24h 该 count ≥ 12.

## 2. User stories

- **US075-A** [P1]: user 切到 Fluxing IME 不再弹浮动栏 (暂时禁用 QuickPanel
  until v0.75 真修复). 输入中文正常候选 panel 出现 (WeaselPanel 不动).
- **US075-B** [P1]: silent deploy 自动化脚本能 catch silent + non-admin 安装失败
  via `$LASTEXITCODE -eq 6002` (continuation of L##-PhaseL-9.5-H silent Abort 6001).
- **US075-C** [P2]: hot-path GetProcAddress 不再在 input event 上做. spec 075 真机
  stress test 时 input event rate ≥ 50Hz 不观察到 user32.dll jitter.

## 3. Functional requirements
- **FR-001**: `RimeWithWeasel::FocusIn` 撤销 v0.19.0.65 re-enable 的双份 Show 调用,
  加 `if (false)` gate 与 L69-fix 风格对齐.
- **FR-002**: `WeaselServerImpl::OnUpdateInputPosition` 把 `GetProcAddress` 缓存
  到构造期, HWND 用 `m_hWndMsg` 替代 `NULL`.
- **FR-003**: `install.nsi` `.onInit` 加 admin token check. silent + non-admin
  时 SetErrorLevel 6002 + Quit; GUI + non-admin 时 continue + DetailPrint.

## 4. Non-goals
- 不重写 WeaselPanel GDI+ path (放 v0.75 大改).
- 不升级 WeaselServer.exe → spec 070 D2D 完全重写 (out-of-process rendering, 放 v0.76).
- 不修 stage2 atomic (L##-PhaseL-9.5 Schtasks /ORC root cause 仍是 Win11 24H2
  Sandbox LogonCommand flake, 放 v0.75 spec 076 调研).

## 5. Acceptance criteria

| ID | Criterion |
|---|---|
| SC-001-1 | `_diag_v0741_evidence.ps1` happy path: 5 binary md5 == v0.74.2 (install-only ship;install.nsi changed → installer bytes change) |
| SC-001-2 | silent install + non-admin user → exit code 6002 via `$LASTEXITCODE` 截获 |
| SC-001-3 | GUI install + admin token = true → install completes normally + `fluxing-install.log` 没 "current user not admin" 行 |
| SC-002-1 | `_diag_v0741_dumps.ps1` post-install + 24h input crash trigger: 0xc0000374 event count = 0 (was ≥ 12 in v0.74.1/2) |
| SC-002-2 | WeaselPanel 显示正常 (input panel + candidate list 不破 visual) |
| SC-003-1 | `_check_install_v3.ps1` happy path: `Fluxing v0.74.3 install OK` (vs v0.74.2 admin-required silent `FluxingAdminRequired=1` if non-admin) |

## 6. Risks
- **R-001**: QuickPanel 重新禁用会让部分 user 失去 "切 Fluxing 弹浮动栏" 习惯. 文档
  在 CHANGELOG + release notes + commit message 标注.
- **R-002**: admin token check `UserInfo` NSIS plugin 在 ARM64 install path
  可能返回非-2/3 user-info-string, SetErrorLevel 6002 误命中. Mitigation:
  R8 value check in `${If} $R8 != "2" ${AndIf} $R8 != "3"` 已经覆盖.

## 7. Dependencies
- NSIS `UserInfo` plugin (built-in to NSIS 3.x, ships with `makensis`).
- Win32 `GetModuleHandle + GetProcAddress` for PhysicalToLogicalPointForPerMonitorDPI
  caching.

## 8. Out of scope (defer to v0.75+)
- WeaselPanel GDI+ → GDI/D2D rewrite (spec 076 候选, 真机 heap-corruption stress test
  之后才能做).
- emoji.ocd ship 化 (L##-PhaseL-9.8 真因尚未真机验证).
- spec 070 QuickPanelDialog out-of-process 化 (大范围架构变更).
- explorer.exe crash 跨多进程 (5 binary good + IPC healthy 但 System32 still has old
  DLL — 这件事清理 System32 后会自然消退, 不需要 write more code).
