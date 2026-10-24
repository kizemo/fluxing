承接 `handoff-after-phase-L-bugfix-7.6-v0.70-blocked-2026-07-29.md`。
v0.70 装机后系统级卡死, Phase 1 evidence collected, root cause 未确认 (工作假设 = FocusIn 重入风暴 + TSF 线程阻塞 I/O, latent bug 暴露 by v0.69 Fix A)。
本会话任务: 跑 user A/B test (v0.69 vs v0.70) 决定 Fix B 是否根因 + 写 FocusIn 重入 guard 防御性修复 + rebuild + sandbox verify + 装机回 user 端再验证。如果 A/B 都不 freeze, 升级到 Phase 3 架构层面讨论 (FocusIn→Show→HWND_TOPMOST z-order 是否走 PostMessage 异步)。

## 必交付
1. **A/B test**: user 装 v0.69 (`release\fluxing-0.19.0.69-installer.exe`), 跑同样操作, 比对是否也 freeze
2. **写 FocusIn 重入 guard** in `WeaselServer/WeaselServerImpl.cpp::OnFocusIn`: 1-line recursive guard (atomic CAS 或 std::atomic<bool>), 防止 storm 中同一 thread 多次进入
3. **配套 test**: `test/TestWeaselIPC/` 加一个 mock test 模拟 concurrent FocusIn storm
4. **Rebuild + sandbox verify** (sandbox-verify script): 装机后 6 项 user flow + §5.1 reg query
5. **如 A/B test 显示 v0.69 也 freeze**: 升级到 Phase 3 架构 spec, 跟 user 决定是否走 spec-init 重新设计 FocusIn→Show 路径

## 禁止
- ❌ 不改 QuickPanelDialog::Show 内部 (spec 070 路径, 任何改动需 spec-init)
- ❌ 不改 install.nsi unless §5.1 reg query 真 FAIL
- ❌ 不 commit handoff-* / _prompt_* / diag-* 脚本
- ❌ 不 git push (K7 仍 500)
- ❌ 不把 diag-freeze-capture.ps1 改名为 ship-able 文件

## 加速模式 (session-health <2h)
- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit
- parallel subagent ≤ 2

## 完成时机
- PASS (6 项 user flow + §5.1 全绿) → ship-gate 通过, 写 handoff v0.71-validated, 关闭 task
- FAIL → 写新 handoff `handoff-after-phase-L-bugfix-8-v0.71-blocked-<date>.md` 升级到 Phase 3 架构讨论
