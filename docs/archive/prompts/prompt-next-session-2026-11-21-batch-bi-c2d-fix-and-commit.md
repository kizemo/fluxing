# 接力 prompt · 2026-11-21 批次 BI 收工 → 下一会话

> 规则见 [`AGENTS.md` §9](../../../AGENTS.md)：本 prompt **只给全路径指针**，不复述 handoff 内容。
> 本批真源 = [`docs/archive/handoffs/handoff-2026-11-21-batch-bi-c2d-fix-and-commit.md`](../handoffs/handoff-2026-11-21-batch-bi-c2d-fix-and-commit.md)

## 一句话 prompt（可直接复制）

先读 `CLAUDE.md` 的进站 5 份与「本批交接指针（BI）」，再读本批真源
`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bi-c2d-fix-and-commit.md`
（重点 §7 并发还原事故、§8 提交实况与错误登记更正、§10 未闭环待办、§11 证据路径）；
背景不足再查同目录 `handoff-2026-11-21-batch-bh-cloud-account-ui-3req.md`（三项需求本体）；
权威判据原文在 `F:\soft\00selfmade\rime_claude\reports\fluxing-bh-ui\run-uicheck9.log`。
下一会话第 1 项 = 该 handoff §10 的第 1 项，其余按序执行；
已闭环项（c2d / c3、A-1~A-5、B-1~B-5、`/verify-client-pid`、175% DPI 像素证据）**勿重跑**。

## 动手前必须自检的三件事（本批踩过）

1. **并发还原**：同工作区有另一会话会删改/还原正本文件 ⇒ 动手前先 `git status -sb` + 目标文件 mtime；
   产出正本文件**必须同轮** `Test-Path` + 字节/行数回读，`write` 工具的自述不算证据。
2. **判据入库优先**：判据只存在于工作区时可以被外部还原悄悄抹掉 ⇒ 取证结论尽快 commit（push 仍需用户授权）。
3. **工具调用预算**：重活（构建 / 门禁 / 出包 / 装机 / 真机探针）一律下沉 `worker` 子会话，`effort` 不传。
