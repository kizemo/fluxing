# Fluxing × 搜狗逆向分析 — 整合路线与排期（ROADMAP）

- **Created**: 2026-11-03
- **Sources**: 本目录 `REPORT.md` + `evidence/`（REA 逆向）；`docs/adr/0030-*.md`（路线定案）；`docs/adr/0031-*.md`（会话隔离）；`.specify/specs/098-cloud-candidates/`（云候选三件套）；交接卡 `docs/archive/handoffs/handoff-2026-11-03-fluxing-quickpanel-session-and-alt-dot.md` §6
- **Status**: 待用户审核后转 Active

---

## 1. 路线（已定案，ADR 0030）

继续火流猩 fork，对齐搜狗能力但**不移植其实体与数据**：
不抽 `sysmodel.bin` / `.cupf` / 词库等搜狗资产，逆向所得仅作规格参考；
剔除搜狗内嵌广告、与搜狗其他软件的关联、与输入无关的功能（逆向实测这三类 ≈ **335.5 MB / 42%**【已验证·体积清单】）。

**路线 B（增量对齐）= 7.5–13.5 人月**；路线 A（逆向重写）23–45 人月，已被用户拍板否决。

## 2. 发布验收基线（ADR 0030 决策 2，每个能力 spec 必须引用）

| 指标 | 目标 | 判据 |
|---|---|---|
| 常驻进程数 | **1**（WeaselServer.exe；FluxingPhrasesDialog 仅唤起时存活） | `Get-CimInstance Win32_Process` 快照 |
| Server 内存 | Private Working Set **< 40 MB** | ≥8 s 静止采样 |
| 空闲 CPU | **< 0.1%** 单核 | 8 核折算 `(cpuB-cpuA)/8` |

对照：搜狗实测 6 进程 / private 183.4 MB / 空闲 0.78% 单核【已验证】。

网络边界（0030 决策 4）：默认不联网；联网能力一律用户自配端点 + 白名单 + 默认关；`evidence/urls-hosts.txt` 185 主机**永不连接**。

## 3. 本轮已落地（T1–T5，全部未 commit，待统一提交）

| # | 项 | 状态 | 关键证据 |
|---|---|---|---|
| ADR 0030 | 路线/基线/UIPlugin 切面/网络边界 | 【已验证】落盘 + README 索引 | `docs/adr/0030-*.md` |
| ADR 0031 | console 会话单 server + 自愈 | 【已验证】落盘 | `docs/adr/0031-*.md` |
| T1 | QuickPanel 150% 缩放**真缺陷坐实** | 【已验证】 | `tmp\quickpanel-evidence\dpi-aware-probe-1235.txt`（245×48 vs 应 368×72） |
| T5 | DPI 缩放修复（MulDiv dpi/96 + WM_DPICHANGED 重算 + 吞错改留痕） | 【已验证】**368×72 @ dpi=144**，装机版同路径对照 245×48 | `panel-crop-after-fix.png`；`tmp\t5-build.log`；改动 `QuickPanelDialog.{h,cpp}` 6 处 |
| T2 | Alt+. 端到端链路 | 【已验证】三轮注入全部唤起新对话框 pid + 顶层窗口（+216~441ms） | `tmp\alt-dot-evidence\end2end-1235.txt` |
| T2+ | **定性更正**：Alt+. show-only 是 042/051 设计行为（再触发=恢复显示，051 SC-003-4），**非缺陷**；交接卡「同一 toggle 逻辑」「spec 097 AC-4」均为错引（097 是删词键/shim spec） | 【已验证·原文核对】 | `tmp\alt-dot-evidence\spec-check-042-051-show-only-by-design.md` |
| T4 | ADR 0031 五项规定代码落地：`FluxingSessionPolicy.h` 共享判定头 + server Start 门禁/四类留痕 + TSF 节流/单飞/阻塞 I/O 全移出回调线程 + 判别性测试 | 【已验证】基线 PASS、**变异测试有牙**（`==`→`!=` 7 断言翻红后还原复跑 PASS）、生产 exe 真实留痕 `startup_allowed` | diff 5 文件 +325/−39；`tmp\t4-build-final.log`；L170 @ lessons 13540–13580 |
| T3 | spec 098 云候选三件套（25 任务、M0 前置核实 5 项） | 【已验证·文件存在+门禁自检】 | `.specify/specs/098-cloud-candidates/` |

## 4. 排期（P0 收尾 → 能力波次）

### P0 — 本批收尾（≤1 周，含用户签字项）
1. 统一 commit（Conventional Commits + 模块前缀；分 3 个逻辑 commit：`WeaselServer|WeaselTSF|docs` 可合并谈）+ push（按签字答复）。
2. 全量 release 构建 → **打 1.1.7 安装器**（含 NSIS 路径守卫 `e96af35` + DPI 修复 + 会话隔离；不替换已发布 1.1.6，签字项 2）。
3. 装包后真机验收三件事：① 面板 368×72@150%（`QpTrigger.exe` 触发，勿用 Alt+, 注入——已证唤不起）；② 会话隔离留痕出现在 `fluxing-profile-notify.log`；③ **三数字基线首测**（结果写入 0030 基线台账）。
4. 交接卡 §6.4 其余收尾：`tmp\cleanup-install-leftovers.cmd` 管理员清理；语言栏偶现未解释现象（需复现取证）；G-a/G-b/G-c 门禁接 CI。
5. 补 T2 遗留观察：PhrasesDialog 冷启动 rect `0,0 373x473` 异常，疑枚举时序——新装包后复查。

### P1 — 能力对齐波次（路线 B 主体，各波独立 spec，验收一律引用 0030 基线）
| 波次 | 能力 | 估时 | 前置/备注 |
|---|---|---|---|
| W1 | 9 键布局 + 表情/颜文字 | 0.5 + 0.5 | 纯本地，先行探路插件切面粒度 |
| W2 | 候选窗/皮肤插件化（照抄 UIPlugin 15 导出切面，**仓内模块边界**，不开放第三方 DLL） | 1–2 | ADR 0030 决策 3；0028 薄壳+IPC 只增 |
| W3 | 云候选（spec 098） | 1–2 | M0 五项核实先行；默认关+白名单；上屏通道=剪贴板借用须按 0030「本机借用 ≠ 跨设备上报」口径写文档 |
| W4 | AI 改错 + 看图识字 | 0.5–1 + 1–2 | 若涉云端，复用 098 网络边界 |
| W5 | 手写识别 | 2–3 | 路线 B 最重；可降级用 Windows Ink 现有能力评估【未验证】 |
| W6 | 词库/配置同步（spec 010 既有） | 1 | 独立于 098，勿混 |

每波完成即触发发布基线复测，**超标即发布阻塞**，改基线须修订 ADR 0030。

## 5. 本机现状与已知残余

- **运行态**【已验证 13:0x】：仅装机版 WeaselServer pid 21048 在跑（`D:\Program Files\fluxing\weasel`），构建产物 5948 已退出，无 dev 残留。装机版**尚不含** DPI 修复 → 面板现仍 245×48，待 1.1.7 生效。
- T4 风险项复核：其观察到的「319904 消失」即 T5 为运行验证主动中断+恢复的窗口期（12:50–12:57），归因已闭环，非异常。
- 96 DPI 零回归【推断，未实测】；跨屏/动态 DPI【未验证，本机单屏】；第二会话实测【未验证，无第二会话，已用同源纯函数+变异反证】。
- 工具坑备忘：`Alt+,` 键盘注入唤不起面板（Alt+. 可以）；用 `tmp\quickpanel-evidence\QpTrigger.exe <pid>`（PostMessage 40018/40020）。
- `librime/lib` 缺失系既有环境态（被 stash 成 `lib_Win32`），T4 已用副本修复，零改工程文件。

## 6. 文档义务（合规红线）

1. 火流猩上屏借用剪贴板（~2s、非文本格式不可恢复）与「不碰跨设备剪贴板上报」的卖点区分，须在任何联网能力文案中显式写明（0030 后果段）。
2. 搜狗逆向数据（词库/模型/皮肤文件）不得进入仓库与安装器；本文档与 REPORT.md 仅作规格参考引用。
3. Alt+. 文案口径统一为「唤起常用短语」（关闭走 Esc/✕），不再出现「toggle」误述。
