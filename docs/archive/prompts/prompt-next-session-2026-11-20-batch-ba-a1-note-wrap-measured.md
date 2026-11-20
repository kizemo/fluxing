# 接力 prompt · 批次 BA 之后（2026-11-20）

> 本文件**只给全路径指针**，不复述 handoff 内容。背景 / 目标 / 进度 / 下阶段计划请自行从下列文档读取。

## 必读（按序）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md` — 进站指引与最新交接指针
2. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-20-batch-ba-a1-note-wrap-measured.md` — **本批真源**（换钥提示折行的实测数值、像素取证、新缺陷 C、判据口径、本批工具坑）
3. `F:\soft\00selfmade\rime_claude\docs\release\restart-checklist-1.3.6.md` — 验收清单（§1 换钥提示已部分闭环 / §2 孤儿回收未动）
4. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-20-batch-az-install-1-3-6.md` — 上一批（1.3.6 装机结果、TSF 注册真值、生效判据口径）
5. `F:\soft\00selfmade\rime_claude\reports\fluxing-ba-a1-note-wrap\report.json` — 本批实测数据原件（同目录另有 `probe-a1-note-wrap.ps1` 探针、`note-text.txt` 文案逐字、`note-1035.png` 提示行取帧、`account-dialog.png` 整窗取帧）

## 按需再读

- `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountUi.h` — 布局字面量唯一真相源（`kIdKeyRotationNote = 1035` 在 `:159`，`AccountClientSize()` 在 `:347`）
- `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountDialog.cpp` — 控件文案 `TextFor()`（`:953`）· 窗口类 `WinClassFor()`（`:986`）· 样式 `StyleFor()`（`:994`）· 建控件 `_CreateControls()`（`:1106`）
- `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountWidgets.h` — `DefaultUiFont()` / `ApplyFont()` / `AddControl()`（`:62-92`）
- `F:\soft\00selfmade\rime_claude\PerMonitorHighDPIAware.manifest` 与 `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServer.vcxproj` — 服务端 DPI 归属（`PerMonitorHighDPIAware`）
- `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（L13 / L09 / L20 / L112 / L120 / L126-I）
- `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§10 NSIS 陷阱；绝对禁止清单；PPL；重装配方）
- `F:\soft\00selfmade\rime_claude\output\install.nsi`（silent 分支、shim 降级路径、Stage2 marker、注册段）
- `F:\soft\00selfmade\rime_claude\docs\adr\0067-*.md`（孤儿会话回收）· `0062`（维护准入闸门）· `0063`（换钥提示）
- `F:\soft\00selfmade\rime_claude\docs\release\regression-checklist-cloud-sync.md`（G-3 / G-5 反向判据）

## 下阶段起点

BA 未闭环清单在 handoff §6：**第 1 项 = AZ handoff §6 第 2 项**（`restart-checklist-1.3.6.md` §2 B-1/B-2/B-3 孤儿会话回收端到端）。
另需注意：`§1` 的 A-4/A-5 需**未登录态**（不得自行登出）；**新缺陷 C（>150% 缩放必裁换钥提示末行）的修法属未决决策**，动代码前要先取得用户认可；
本批与 AZ 的文档改动**均未提交**，push 需用户授权。
