# Phase L Bugfix 3 v0.19.0.66 — 新会话第一句话 prompt

承接 `handoff-after-phase-L-bugfix-3-pre-subagent-v0.19.0.66-2026-07-25.md`(本会话已读完)。session-health 强制 <2h,本会话承接 subagent-phase 主进程接力。

**当前阶段**: systematic-debugging Phase 1 完成(主进程已派 3 subagent background,Alt+, 真路径独立 grep 已得),下一步 Phase 2 综合 + **Phase 4.5 架构讨论 (已触发,3+ ship 失败)**。

**必交付清单**:
1. 收 3 subagent 报告 (background notification → 读 JSONL 末尾 / 用 TaskOutput / 重 TaskCreate named agents)
2. 综合分析 + **验证 Hypothesis C5** (Show(null x 6) 是否真破坏 QuickPanelDialog 单例 state)
3. **Phase 4.5 架构讨论** — 4 选项请 user 决策(改 Show 调用 / 重做 Bug 2 / out-of-process 架构 / 4 路径独立 state)
4. 仅在 user 同意后才改源码,严格走 spec-init 流程
5. ship v0.19.0.66 前必跑 sandbox-verify + `grep -RIn "if (false)" WeaselServer RimeWithWeasel WeaselTSF` (Trigger L3 gate)
6. `_check_install_v2.ps1` 期望 md5 同步更新

**严格禁止**:
- ❌ 改 install.nsi (L09/L17/L54/L66 雷区)
- ❌ 改 QuickPanelDialog.cpp 内部 (除非 4 路径全不 work + spec-init 走通)
- ❌ Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点
- ❌ `if (false)` 残留 (Trigger L3 gate)
- ❌ L97 stale binary ship (verify installer extract md5 = source build md5)

**加速模式**: bash >=60 → 切 Write/Edit;单回合 diff >500 → 拆回合;并行 subagent ≤2 (本会话特例是 user 显式授权)。

**新会话 KEEP 自检**:
- 本会话时长 < 2h (按 session-health.md 强制)
- 不读主会话已读的 handoff / handoff §1-§4 / memory 5 条 / spec 038/045 (本会话已读)
- 但**必须重读** 3 subagent 报告 (主会话只 cat .jsonl 不读内容)
