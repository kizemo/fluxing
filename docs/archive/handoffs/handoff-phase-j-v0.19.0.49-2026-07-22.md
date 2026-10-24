# Handoff — Phase J v0.19.0.49 + 架构重设计 (2026-07-22)

> **承接会话**: `S-2026-07-22-02-phase-j-architecture-decision` (本次会话,健康)
> **承接 ship**: `1c85b47` (v0.19.0.49-fluxing installer) ← HEAD
> **承接状态**: **Phase J 已 ship 8 个版本都 fail 修 IME** (v0.19.0.35/36/43/45/46/47/48/49), **进入架构重设计阶段**
> **不变量**: PhrasesDialog 现在 in-process 跑在 WeaselServer.exe (TSF shim 进程), IMM32 路径被 bypass
> **真架构问题**: TSF shim thread 占用, s_hInput 不是 TSF 焦点, 中文候选词不出

---

## 1. 当前活跃工作线 (TL;DR)

**v0.19.0.49 ship 后装机 user 反馈** (2026-07-22):
1. **Bug A 仍 fail**: 顶部小蓝条**没有变高** (装机 v0.19.0.49 kTitleH 30→48 修改未生效)
2. **Bug B 仍 fail**: 仍无法输入中文 (IMM32 fallback 已 ship, TSF shim thread 抢 IME 焦点, 8 ship 版本 fail)
3. 用户决定: **另开新会话, 修改架构解决问题 (out-of-process PhrasesDialog)**

**v0.19.0.49 ship 详情**:
- installer: `release/fluxing-0.19.0.49-installer.exe` 43,159,632 bytes, md5 `d67cc67e67e668910928193cf3798470`
- WeaselServer.exe source build md5: `2468da35b987c31ade8474e57ae9ff55`
- L97 stale check 通过 (installer extract md5 = source build md5)
- TestPhrasesDialog 186/186 PASS / 0 FAIL (含 Test 36 v0.19.0.47 + Test 37 v0.19.0.48 + Test 38 v0.19.0.49)
- install.nsi: 10x taskkill WeaselServer + 5x ctfmon/TextInputHost + Stage 2 PPL fallback 仍在

**已 commit** (本会话):
- `fc983ed` fix(WeaselServer): v0.19.0.49 (Phase J 续修) — kTitleH 30→48 + IMM32 ImmAssociateContext fallback
- `1c85b47` chore(release): v0.19.0.49-fluxing installer (Phase J 续修 装机)

---

## 2. 8 ship 版本 IME 复发时间线 (Bug B 真因收敛)

| 版本 | commit | 改动 | 装机端 IME |
|---|---|---|---|
| v0.19.0.35 | fa196049 | 引入 `ImmCreateContext` + `ImmAssociateContext` + `ImmDestroyContext` | 死 |
| v0.19.0.36 | 79d522b6 | revert 改 `ImmReleaseContext(s_hInput, himc)` 2 参 | 死 |
| v0.19.0.43 | e658e42 | 改"不调 ImmReleaseContext" | **死** (TSF 进程不认) |
| v0.19.0.45 | 2d842fc | 没改 IME | 死 (carry-over) |
| v0.19.0.46 | b4f0626 | 删 isolated HIMC 5 行 | 死 (TSF shim 不配 default) |
| v0.19.0.47 | f40c15f | OnCreate SetFocus(s_hInput) 触发 TSF attach | 死 (TSF attach 失败) |
| v0.19.0.48 | d43bf3e | IMM32 fallback (ImmGetContext + ImmSetOpenStatus) | **死** (TSF TIP 抢 thread, IMM32 调用成功 ≠ 候选词出) |
| v0.19.0.49 | fc983ed | IMM32 ImmAssociateContext 强化 | **死** (TSF TIP 仍抢) |

**systematic-debugging 4.5 触发**: 8 fixes failed → 问架构。

---

## 3. 真架构问题 (Phase K 的 spec / 启动新 spec-init)

### 3.1 真因

`WeaselServer.exe` 是 **TSF shim 进程** (weaselx64.dll 主导), TSF 通过 `ITfThreadMgr` + `ITfInputProcessorProfileMgr` 强制 hwnd 走 system TSF-registered IME (e.g. 微软拼音)。

**问题**:
- `PhrasesDialog` 是 in-process 跑在 `WeaselServer.exe` 内的 modal dialog
- 它的 `s_hInput` 是普通 EDIT child control, **不是 TSF text service**
- TSF shim 把 thread 的 IME focus 给 TSF text service, in-process dialog 不自动接管
- IMM32 调用成功 ≠ TSF 候选词出 — 因为 TSF TIP 占用 thread, IMM32 状态不产生候选词
- 这是 **8 ship 版本本质 fail** 的原因

### 3.2 根治方案: out-of-process PhrasesDialog

把 `PhrasesDialog` 从 `WeaselServer.exe` **剥出**, 跑在独立 `FluxingPhrasesDialog.exe` GUI 进程:
- `FluxingPhrasesDialog.exe` 是普通 Win32 GUI 进程, **不是 TSF shim 进程**, **不注册 TSF textInputProcessor**
- 它的 hwnd 走 system IMM32 IME context (跟 notepad.exe 等普通 GUI 进程一致)
- 通过 **named pipe** 跟 `WeaselServer.exe` 通信:
  - 读 m_phrases (`\\.\pipe\FluxingPhrasesDialog\data`)
  - 写 m_phrases (AddTop 触发)
  - InjectText (双击 / Enter 触发, 由 FluxingPhrasesDialog 调 SendInput)
- TSF shim (weaselx64.dll) 还在 `WeaselServer.exe` 主编辑框正常工作
- `FluxingPhrasesDialog.exe` 独立 IME 上下文完整工作

### 3.3 实施路径 (Phase K spec 路线)

```
spec-init 创建 .specify/specs/051-out-of-process-phrases-dialog/
  ├── spec.md        — 用户故事 (中文简, UX 描述)
  ├── plan.md        — 技术方案 (FluxingPhrasesDialog.exe + named pipe IPC)
  └── tasks.md       — 实施步骤 (按 v0.19.0.50+ 分 ship)
```

**Phase K 阶段**:
1. **v0.19.0.50**: 新 exe 跑 `FluxingPhrasesDialog.cpp` (从现有 PhrasesDialog.cpp 抽出来, 改名 + 加 main + 加 Win32 启动逻辑) — sandbox 测试 + smoke test
2. **v0.19.0.51**: named pipe IPC server in WeaselServer.exe (`FluxingPhrasesDialogServer.cpp` 新增) + client in FluxingPhrasesDialog.exe — 双向通信: read phrases / write phrases / request inject
3. **v0.19.0.52**: `PhrasesDialog::Show()` 改为 `CreateProcess` FluxingPhrasesDialog.exe + hwnd-from-pipe; PhrasesDialog 代码从 WeaselServer.exe 删干净
4. **v0.19.0.53**: installer 注册 FluxingPhrasesDialog.exe + named pipe ACL + uninstall 时删除
5. **v0.19.0.54**: Phase J 装机 user flow 5+1 项 verify (打"ni"出候选词 / Ctrl+Shift 切 IME / 拖顶部小蓝条)

### 3.4 Bug A (蓝色条没变高) — 独立追查

新会话需先追查 Bug A: v0.19.0.49 source `kTitleH=48` 但用户装机端蓝色条**没变高**。可能根因:
- ModalChrome::PaintBackgroundAndBorder 用 hardcoded 30 像素 (不看 kTitleH)
- PhrasesDialog.cpp OnPaint (line ~795) `RECT titleRc = {0, 0, paintW, (int)(kTitleH * s_dpiScale)}` 用 kTitleH, 但 ModalChrome paint 用自己的高度
- installer 拉的 binary 是 source build 但 install 时路径错位

需要新会话拆解 ModalChrome + OnPaint 真画逻辑。

---

## 4. Working tree 状态 (`git status -sb` 关键行)

```
## Fluxing...kizemo/Fluxing [ahead 88]
 M .claude/skills/git-workflow/SKILL.md
 M .claude/skills/spec-init/SKILL.md
 M .claude/skills/systematic-debugging/SKILL.md
 M .claude/skills/tdd/SKILL.md
 M .claude/skills/verification-before-completion/SKILL.md
 M .specify/memory/.learnings/ERRORS.md
 M .specify/memory/lessons-learned.md
 M CLAUDE.md
 m librime
 D release/fluxing-0.19.0.44-installer.exe
 M task.md
?? $out/
?? .claude/agents/
... (其他 untracked, 不动)
```

**已 commit** (本会话):
- `fc983ed` fix(WeaselServer): v0.19.0.49 — kTitleH 30→48 + IMM32 ImmAssociateContext fallback
- `1c85b47` chore(release): v0.19.0.49-fluxing installer

**不动清单**:
- `M .claude/skills/*` — 工作流 skill 累积改动
- `M CLAUDE.md` — 进站指引累积改动
- `m librime` — 子模块 dirty
- `M task.md` — 主 task plan 累积改动
- `?? build_v046.*` `?? diag-quickpanel.ps1` `?? report-*` — 历史 build helper + 诊断报告, 临时
- `?? release/v0_19_0_32_e2e.exe/.pdb` — 历史 e2e binary, 不动

**新 handoff 文件**: 本文件 (不 commit, 会话级快照)

---

## 5. CHANGELOG.md 当前段 (按 commit 时间倒序)

```
## [0.19.0.49-fluxing] - 2026-07-22   ← 最新
## [0.19.0.48-fluxing] - 2026-07-22
## [0.19.0.47-fluxing] - 2026-07-22
## [0.19.0.46-fluxing] - 2026-07-22   ← Phase I Bug 4 真修 (删 isolated HIMC)
## [0.19.0.45-fluxing] - 2026-07-21
...
```

---

## 6. 不变量 (新会话维持)

- `env.bat` 是 gitignored, **绝不 commit** (当前是 `WEASEL_BUILD=49`, `FLUXING_VERSION=0.19.0`, `RELEASE_BUILD=1`)
- `weasel.props` 是 gitignored, **绝不 commit** (当前是 0.19.0.49)
- `_check_install_v2.ps1` expected md5 = `2468da35b987c31ade8474e57ae9ff55` (WeaselServer.exe)
- `release/` 目录 installer 是 git-tracked, 9 个版本已 commit (0.40-0.49)
- install.nsi UTF-8 BOM + CRLF 必须保持 (per L09)
- 装机命令必带 `taskkill + reg delete` (L13/L54/L66 强约束)
- working tree 累积改动不动

---

## 7. 装机命令模板 (L13/L54/L66 强约束, 双击 GUI mode 自动 Stage 2 fallback)

```powershell
# 1. 双击 GUI 装机 (auto taskkill + Stage 2 PPL fallback)
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.49-installer.exe"

# 2. verify md5 (排除 L97 stale binary)
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**期望**:
- D 盘 `WeaselServer.exe` md5 = `2468da35b987c31ade8474e57ae9ff55` (MATCH v0.19.0.49)
- L66 keys 全有
- `WeaselServer.exe.old.tmp` 不存在 (在线覆盖成功) 或存在 (PPL locked, 走 Stage 2)

---

## 8. 旧 handoff 归档标注

| 文件 | 状态 |
|---|---|
| `handoff-phase-f-bug3-vk_escape-2026-07-20.md` | ✓ RESOLVED (v0.19.0.40) |
| `handoff-phrases-dialog-2026-07-19.md` | ✓ Phase D 已 ship |
| `handoff-loop-engineering-2026-07-18.md` | Loop Engineering 框架 reference |
| `handoff-phase-g-v0.19.0.44-2026-07-21.md` | ✓ Phase G 已 ship |
| `handoff-phase-h-v0.19.0.45-2026-07-21.md` | ✓ Phase H 已 ship |
| `handoff-phase-i-v0.19.0.46-2026-07-22.md` | ✓ Phase I 已 ship (删 isolated HIMC, 6 ship 版本仍 fail) |
| `handoff-phase-j-v0.19.0.49-2026-07-22.md` | **(本文件)** — Phase J 已 ship, **进入架构重设计阶段** |

---

## 9. 新会话需立即做的事 (Phase K spec 启动)

1. **Bug A 追查** (1h): v0.19.0.49 blue strip 没变高 → 拆解 ModalChrome::PaintBackgroundAndBorder + PhrasesDialog.cpp OnPaint height 计算
2. **spec-init 启动** (4h): `.specify/specs/051-out-of-process-phrases-dialog/` 3 文件 (spec.md + plan.md + tasks.md) — 走 spec-check 后再 implement
3. **Phase K v0.19.0.50+ 分 ship** (per task.md): 新 exe + IPC + 集成 + installer + verify
4. **装机 user flow 5+1 verify** (核心 — Phase K 装机): 打"ni"出候选词 + Ctrl+Shift 切 IME + 拖标题条

---

## 10. 一句话总结

> Phase J v0.19.0.35/36/43/45/46/47/48/49 = **8 ship 版本都 fail 修 IME**, 真架构问题 = `PhrasesDialog` in-process 跑在 `WeaselServer.exe` (TSF shim 进程) 导致 TSF TIP 抢 thread, IMM32 路径被 bypass。**根治方案 = out-of-process 重设计**: 把 `PhrasesDialog` 剥出跑在独立 `FluxingPhrasesDialog.exe` 进程, 通过 named pipe IPC 跟 `WeaselServer.exe` 通信, 不受 TSF shim 限制。当前 HEAD `1c85b47` = v0.19.0.49 source fix 已 ship 但装机 user 仍 fail (蓝色条没变高 + 中文仍不能输), 装机 user 已要求新开会话走架构重设计。新会话通过 spec-init 启动 `.specify/specs/051-out-of-process-phrases-dialog/` 走完整 SDD 流程。

---

— END OF HANDOFF —
