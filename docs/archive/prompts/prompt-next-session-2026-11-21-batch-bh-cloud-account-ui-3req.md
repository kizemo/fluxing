# 接力 prompt · 批次 BH（账号页三条需求）

> 本文件**只给全路径指针**，不复述 handoff 内容。背景 / 目标 / 进度 / 计划一律去读 handoff 本体。

## 入口（按顺序读）

1. `F:\soft\00selfmade\rime_claude\AGENTS.md`（首屏 §1 + 危险区 §4 + checklist §5 + handoff 规则 §9）
2. `F:\soft\00selfmade\rime_claude\CLAUDE.md`（进站指引与必须强制清单）
3. **本批 handoff 本体（权威进度与判据）**：
   `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bh-cloud-account-ui-3req.md`
   —— 其 **§4.5「第 7 轮实测（权威）」覆盖 §4.2 / §4.3 / §4.4**；§7 已改写明新的第 1 项。
4. 上一批：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bf-a4-a5-logout-measured.md`
5. 并行会话写的同题 handoff（**另一份，勿混为真源**，只作交叉参考）：
   `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bh-ui-fixes-3req.md`

## 证据与脚本（全绝对路径，不删）

- 探针：`F:\soft\00selfmade\rime_claude\reports\fluxing-bh-ui\probe-bh.ps1`
- 判据原始数据：`F:\soft\00selfmade\rime_claude\reports\fluxing-bh-ui\run-uicheck5.log`（旧件）/
  `run-uicheck6.log`（不作数）/ `run-uicheck7.log`（**新件、权威**）/
  `ui-20261121-202648-UiCheck.txt`
- 取帧：`F:\soft\00selfmade\rime_claude\reports\fluxing-bh-ui\evidence-*.png`
- 重登安全网（**勿改**）：`F:\soft\00selfmade\rime_claude\reports\fluxing-bf-a4-a5\probe-a4-a5.ps1 -Mode Relogin`
- 出包 / 装机链：`F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.3.8-installer.exe`、
  `F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe`、
  `F:\soft\00selfmade\rime_claude\tmp\bh-reinstall-probe.ps1`、`F:\soft\00selfmade\rime_claude\tmp\bh-install3.log`

## 产品码与判据靶点

- `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountDialog.cpp`
  （`:1246 HideSuggestList` / `:1295 ShowSuggestIfAny` / `:1334 EmailEditProc WM_LBUTTONUP` /
  `:1572-1585 EN_SETFOCUS·EN_KILLFOCUS` / `:1753-1782 WM_PARENTNOTIFY`）
- `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountUi.h`（控件 ID / 可见性 / 几何唯一真相源）
- `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp`（**不在 `weasel.sln`，须显式 msbuild 且带 `/p:SolutionDir=`**）

## 一句话接力 prompt（可直接复制给新会话）

```
请按 AGENTS.md §9 继续 Fluxing 批次 BH：先读
F:/soft/00selfmade/rime_claude/docs/archive/handoffs/handoff-2026-11-21-batch-bh-cloud-account-ui-3req.md
（以其中 §4.5 第 7 轮权威实测为准，它覆盖 §4.2/§4.3/§4.4），再读
F:/soft/00selfmade/rime_claude/AGENTS.md 与
F:/soft/00selfmade/rime_claude/CLAUDE.md，
然后做 §7 新第 1 项：给第 7 轮实测暴露的 c2d_hide_on_other=False 定根因
（换判靶 1010/1002 各测一次 + 在 CloudAccountDialog.cpp:1765 三条分支各加一行 acctdlg 诊断日志），
修好后装机复验需求 ② 端到端；判据、坑与未闭环项一律以 handoff 为准，不要重复已闭环项。
```
