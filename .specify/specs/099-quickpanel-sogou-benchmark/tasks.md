# 099 · Tasks · 快捷设置栏对标搜狗

> 配套 `spec.md`。**一次只做一片**，每片做完贴测试输出再进下一片。
> 状态标记：`[ ]` 待办 / `[~]` 进行中 / `[x]` 完成（完成时必须把验收输出写进本文件对应行下）。

## P0-1 · 搜狗快捷设置栏黑盒行为采集 【需用户在场配合】

- [ ] 记录搜狗 16.8.0.5415 状态栏右键菜单全项（截图 + 逐项名称/图标含义）
- [ ] 打开其「快捷设置/属性配置」面板，截图首屏与每个分页
- [ ] 逐项点击：记录「点了之后改变了什么」——是否即时生效、是否需要重启、是否有开关态
- [ ] 中/英、全/半角切换时，面板与状态栏的回显变化（这决定 P0-4 的规格）
- [ ] DPI 100% / 150% / 200% 下面板尺寸与清晰度；主/副屏拖拽后的表现
- [ ] 常驻 vs 临时出现的行为差异（对照我们的 `kAlwaysShow`）
- [ ] 产出 `docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`：每条观察 → 截图相对路径
- [ ] 未观察到的项写「未验证」，**禁止用推测填充**

## P0-2 · 差距矩阵与取舍

- [ ] 三列表：搜狗行为 × 我方现状（源码位置） × 设计意图（`docs\design\quickpanel-v{3,4,5}`）
- [ ] 每行判定「借鉴 / 改造 / 不抄」+ 理由
- [ ] 每行填「借鉴来源（证据文件全路径）」与「差异化实现」；填不出的行删除
- [ ] 产出 `.specify\specs\099-quickpanel-sogou-benchmark\gap-matrix.md`

## P0-3 · Account 占位槽处置 【可立即开工，不依赖 P0-1】

- [ ] 先写判别性测试：断言「每个可见槽位点击必产生确定的用户可见效果」——当前必红（hit==3 为 no-op）
- [ ] 决策：实装 or 整槽摘除（UserDict 整槽删除的先例见 `QuickPanelDialog.h:132` 注释）
- [ ] 实现（`WeaselServer\QuickPanelDialog.cpp` `kButtonCount`/`HitTest`/`OnLButtonUp` + `WeaselServerApp.cpp:439-447`）
- [ ] 若改动 `kButtonCount` ⇒ 同步 `L1-quickpanel-icon.ps1` 的槽位数量期望（改期望值前先看它测的是什么）
- [ ] `scripts\gate\run-all.ps1 -Level L0` 全绿 + 贴输出

## P0-4 · 当前输入状态回显（中/英 + 全/半角）

- [ ] 确认状态来源：面板 `Show()` 现有 `currentFullwidth` 入参；中文态需从 server 侧取（**禁 TSF 阻塞 I/O，P2**）
- [ ] 先写失败测试：状态切换后面板回显跟随
- [ ] 实现绘制 + 命中区不变；不得改动 `quickpanel.state` 语义
- [ ] 贴 e2e 输出

## P0-5 · 键盘可达性 + 槽位 tooltip

- [ ] 失败测试先行：Tab / Shift+Tab / ← → 移焦、Enter|Space 激活、Esc 收起
- [ ] Esc 行为**必守 D0032**（只收面板、不得连带收起候选窗；用例 `test\regression\cases\D0032-esc-no-cascade.ps1`）
- [ ] tooltip：每槽「名称 + 当前态」，品牌 tooltip 现有实现可扩展
- [ ] 扩展 `scripts\gate\L1-quickpanel-hotkey.ps1` 并**造坏数据证明新断言会红**

## P0-6 · 装机版 DPI 复验（T5 收口）【可立即开工】

- [ ] 探测 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 版本（应为 1.1.9.0，本轮已证）
- [ ] 起 server（**先停计划任务 `FluxingDeployerMaintenance` 再 `Start-Process`**），在 150% DPI 下量面板物理尺寸
- [ ] 期望 368×72（修复后），若仍 245×48 ⇒ 登记 `test\regression\DEFECTS.yaml` 新 `D00xx` 并写 heavy 复现用例
- [ ] 100% / 200% 各测一次；多屏各测一次

## P0-7 · 面板位置持久化

- [ ] 决策落 ADR：位置存在哪（**不得污染 `quickpanel.state` 的单一真相源语义，R7**）
- [ ] 失败测试：跨重启保留位置；屏幕分辨率/ DPI 变化后钳制回可视区
- [ ] 实现 + 多屏负坐标用例

## P1 · 声明式槽位表（不排进 1.1.x）

- [ ] ADR 草案：槽位表驱动（对齐 `UIPlugin.dll` 的 `AddUIFunction` 思路，自有实现，不抄代码）
- [ ] 迁移方案 + 兼容性测试

## 收尾（每片都要做）

- [ ] 用户可见改动写 `CHANGELOG.md` 未发布段（P5）
- [ ] 事故/坑追加 `.specify\memory\lessons-learned.md`（**BOM + CRLF**）
- [ ] 架构决策进 `docs\adr\`（下一个编号从 0034 起，先 `ls` 确认）
- [ ] push 前 `scripts\gate\run-all.ps1 -Level L0`；发版前 `-Level L0,L3,L4,L5 -Build`
- [ ] 提交用 Conventional Commits + 单一 scope；`git status -sb` 确认 ` m librime` 未被带进去
