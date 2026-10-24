# 新会话 Prompt — Phase K3 T019 Dblclick Bugfix

## 上下文

Fluxing IME Phase K3 已 ship (4 commits at f6200a6, v0.19.0.54 真机装机验).
User 装机反馈 2 个真机 bug:
1. 常用短语 dialog 输入 → Add 成功 (OK)
2. dblclick 短语 → 文本没上屏 + dialog 消失 + 之后快捷键/按钮 无法重开 (FAIL × 3)

## 任务

**先读 handoff**: `F:\soft\00selfmade\rime_claude\handoff-phase-k3-T019-bugfix-2026-07-22.md`
(完整 root cause 分析 + 修法方案 + 验法)

## 必做 (按顺序)

1. **systematic-debugging** (强制): 跑 5 阶段 root cause 调查, 确认 handoff §3
   3 bug 假设都对 (Bug 2 pipe handle 泄漏 + Bug 3 NM_DBLCLK 顺序错位 +
   Bug 1 Option B 简化)
2. **实施 handoff §4 修法**:
   - §4.1 Bug 2 修 (worker exit CloseHandle + s_hPipe = INVALID)
   - §4.2 Bug 3 修 (client Hide 先于 INJECT + HideWithoutDisconnect API)
   - §4.3 Bug 1 Option B 简化 (best-effort fallback, 不依赖 cached HWND)
3. **验证**:
   - WeaselServer.exe Release Win32 — 0 errors
   - FluxingPhrasesDialog.exe Release x64 — 0 errors
   - TestPipeProtocol — 45/45 PASS (无回归)
4. **重 build v0.19.0.55 installer**:
   - env.bat bump 0.19.0.54→55 (gitignored)
   - 复制 build_v054.ps1 → build_v055.ps1, 改内部 version
   - 跑 build_v055.ps1 → output/archives/fluxing-0.19.0.55-installer.exe
   - copy 到 `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.55-installer.exe`
5. **更新 _check_install_v2.ps1 期望 md5** (sync 到 v0.19.0.55 binaries)
6. **commit 计划** (handoff §9):
   - commit 1: `fix(WeaselServer): pipe handle 泄漏 + NM_DBLCLK Hide 先于 INJECT`
   - commit 2: `chore(fluxing): v0.19.0.55 T019 hotfix installer`
7. **加 L105 到 lessons-learned.md** (Bug 2 pipe handle 泄漏是新教训)

## 不做

- 不动 4 commits (refactor / fix / docs / chore) — 它们是 K3 ship record
- 不改 install.nsi (T014 已 ship FluxingPhrasesDialog.exe)
- 不动 test/TestPhrasesDialog/ (orphan 决策留给单独会话)
- 不 push (等真机验 PASS 后再 push)

## 真机验 (留给 user 跑)

4 场景端到端验, 见 handoff §6:
A. 输入 → Add → 列表有 → 关重开还在
B. Notepad 焦点 → Alt+. → dblclick → 进 Notepad (Bug 1/3 修验证)
C. Alt+/ 同样路径
D. QuickPanel button 同样路径
E. 反复 dblclick 5 次, 每次都能重开 (Bug 2 修验证)

## 不要

- 不要 "擅自进行修改" — handoff 是参考, 真要改超出范围前先 ask
- 不要 push (T019 ship 决策留给 user)
- 不要 commit lessons-learned 改到其他 session 的 L## (按递增顺序)

— END OF PROMPT —