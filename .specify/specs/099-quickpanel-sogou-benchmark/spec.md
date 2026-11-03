# 099 · Spec · 快捷设置栏对标搜狗（逆向成果 → 改造计划）

> 立项时间：2026-11-03 深夜。前置：v1.1.9 已发布（`https://github.com/kizemo/fluxing/releases/tag/v1.1.9`），
> 门禁 L0/L3/L4 全绿、L5 仅两条环境能力断言红（`tmp/gate-final-rollback.log`）。
> **本 spec 不重做已有逆向分析**，只做「快捷设置栏」这一条波次的细化与可执行切片。

---

## 1. 任务目标（一句话）

以 `docs/research/sogou-reverse-2026-11/` 的搜狗逆向成果为**规格参考**，把我们的「快捷设置栏」
（`QuickPanelDialog`，用户口中的快捷短语/快捷设置面板）从「4 槽工具条」升级为
「看得懂当前输入状态、每个槽都有真语义、DPI 与多屏不塌」的常驻控制条。

## 2. 已有底座（必须复用，禁止从零重做）

| 资产 | 路径 | 用途 |
|---|---|---|
| 逆向主报告 | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\REPORT.md` | 803 MB 全量分组、`UIPlugin.dll` 15 个导出 |
| 改造路线图 | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\ROADMAP.md` | W1–W6 波次、发布验收三数字、网络边界 |
| REA 原始证据 | `...\evidence\`（21 文件：导入/导出表、`urls-hosts.txt` 185 主机、`component-split.csv`） | 每条结论的出处 |
| 路线决策 | `F:\soft\00selfmade\rime_claude\docs\adr\0030-*.md` | 路线 B（自有代码渐进改造）；路线 A 已被用户否决 |
| 常驻/状态真相源 | `docs\adr\0019 / 0020 / 0022`、`include\FluxingAlwaysShow.h` | 面板模式与落盘协议不可破坏 |
| UI 身份判别 | `docs\adr\0032-*.md` | 自家 UI 进程识别（安装器 kill-loop 同源） |
| 门禁 | `scripts\gate\{run-all,L0-static,L3-mutation,L4-adr}.ps1`、`test\regression\DEFECTS.yaml` | 每个切片的守门 |

## 3. 我方现状（本轮实测，非推测）

- 实现：`WeaselServer\QuickPanelDialog.{h,cpp}`（337 / 1687 行，头注 v3-rev3，GDI + per-pixel alpha + WIC PNG）；
  设计稿 `docs\design\quickpanel-v3\index.html`，另有 `quickpanel-v4`（macOS 风）、`quickpanel-v5`。
- 槽位：`kButtonCount = 4` = Schema / Phrase / Settings / **Account（hit==3 仍是 no-op 占位）**；
  `onSymbols`、`onLogin` 在 `WeaselServerApp.cpp:439-447` 传空 lambda；UserDict 槽已于 v0.19.0.62 整槽删除。
- 模式：`Mode::kHidden` / `kAlwaysShow`（`include\FluxingAlwaysShow.h` 为真相源），落盘
  `<WeaselUserDataPath()>\quickpanel.state`（`always_show=0|1`），缺失/损坏 ⇒ 默认常驻；`RestoreAlwaysShowMode()` 绝不 auto-show。
- 几何：logical 常量集中（`kPanelW=245 / kPanelH=48 / kBtnSize=35 / kIcoSize=19 / kBtnRadius=10 …`），
  运行时乘 `s_dpr_x/y` 得 `_phys`；DPI 变化走 `WM_DPICHANGED` 重算。
- 已知**缺失**（源码扫过，未见对应实现）：槽位级 tooltip（只有品牌 tooltip）、键盘可达性（Tab/方向键/Enter 激活）、
  面板位置持久化、右键菜单、**当前输入状态指示**（中/英、全/半角在面板上无回显）、账号槽真语义。
- 调用入口：托盘左键、`Alt+,` → `ToggleMode`、托盘菜单/TSF 语言栏 `ID_WEASELTRAY_QUICK_PANEL` → IPC → `Show()`。
- 测试资产：`test\TestQuickPanelDialog\`、`test\TestQuickPanelRefactor\`、`test\v0_19_0_{30,32}_e2e\`、
  `scripts\gate\L1-quickpanel-{state,icon,hotkey}.ps1`、`L2-rime-quickpanel-e2e.ps1`。
- 本机搜狗：`16.8.0.5415`，`D:\Program Files (x86)\SogouInput`；UI 相关大件 = `16.8.0.5415\UIPlugin.dll`
  （11,595,648 B）、`SGIGuideHelper.exe`；组件内 `soui-sys-resource.dll` ⇒ UI 框架是 **SOUI**（推断，非伪代码证实）。

## 4. 搜狗侧已知可借鉴点（只作**行为规格**参考）

来自 `REPORT.md` 的导出表证据，`UIPlugin.dll` 暴露的切面说明「状态栏 + 设置面板 + 皮肤预览」是**可插拔 UI 函数注册制**：

1. `RegisterImeClass` / `AddUIFunction` —— UI 能力注册，槽位由功能表驱动 ⇒ 我们的 4 硬编码槽可改为**声明式槽位表**。
2. `CreatePluginUIRes` / `SendUIResourceMessage` / `SendWndUIMessage` / `SendExtraUIMessage` —— UI 资源与消息分层 ⇒ 对应我们的 IPC 命令扩展点。
3. `SendSkinPreviewMessage` —— 皮肤预览是独立消息面（我们的皮肤预览当前无面板入口）。
4. `SetUiLayoutStub` —— 存在布局注入 stub ⇒ 提示「布局应可被配置改写」，与我们的硬编码几何常量形成对照。
5. 体积证据：`Components` 145.1 MB + 助手与运维 200+ MB ≈ 42% 不参与输入 ⇒ **我们的改造不得往面板加会带出组件下载的东西**。

> 【推断】标注原因：Hopper / Ghidra / IDA 在本机均不可用（`REPORT.md §0`），无伪代码，只到导出表与字符串粒度。

## 5. 合规红线（照抄 `ROADMAP.md §6`，不新发明）

- 只读静态证据 + 黑盒行为观察；**不反编译进产品、不提取/复用任何搜狗二进制、资源、词库、`.cupf` 皮肤包与专有文案**（本仓库 GPL 系，抄资产污染许可）。
- **永不连接** `evidence\urls-hosts.txt` 内 185 主机；快捷设置栏不得新增任何默认联网行为（P2 精神 + 宪法）。
- 每条借鉴必须在 spec/tasks 写「借鉴来源（证据文件）」+「差异化实现」，无证据不下结论。

## 6. 不可破坏的不变量

1. **P2**：TSF 侧不得为面板做阻塞 I/O（面板在 WeaselServer，TSF 只发 IPC）。
2. **R7**：常驻模式真相源唯一 = `quickpanel.state`，不得新增第二份状态。
3. **spec 055**：`RestoreAlwaysShowMode()` 只回填模式，**绝不 auto-show**。
4. **IPC 四边同步**：若为面板新增 IPC 命令 ⇒ `include/WeaselIPCData.h` + server/TSF/deployer 三端 + 一个 ADR + `test/TestWeaselIPC/*` 一次性提交。
5. **P3**：不改 `librime/` 子模块。
6. 图标唯一性：面板 PNG 不得出现重复 base64（`L1-quickpanel-icon.ps1` 已守）。

## 7. 切片计划（每片 = 可独立验收的代码 + 测试）

> Done 判据一律**贴测试输出**（R6）。顺序即优先级；P0-1 未出结论前不写 P1 代码。

| 片 | 内容 | 验收判据 |
|---|---|---|
| **P0-0** | 本 spec + `tasks.md` 立项（已完成） | 文件入库、路径与证据可追 |
| **P0-1** | 搜狗快捷设置栏**黑盒行为采集**：真实截图 + UIA 树 + 手工清单（有哪些槽、名称、开关语义、hover/点击效果、DPI 100/150/200%、多屏、隐藏态）→ `docs/research/sogou-reverse-2026-11/evidence/quickpanel-behavior.md` | 每条观察附截图路径；未观察到的写「未验证」，不许推测填空 |
| **P0-2** | 差距矩阵：搜狗行为 × 我方现状 × 设计意图（v3/v4/v5）⇒ 逐项判定「借鉴 / 改造 / 不抄」，产出 `...\099-quickpanel-sogou-benchmark\gap-matrix.md` | 每行含「借鉴来源证据文件 + 差异化实现」两栏，无证据行必须删 |
| **P0-3** | **Account 占位槽处置**（用户可见缺陷：画了图标、点下去没反应）——实装（跳设置/账号说明）或整槽摘除（对齐 UserDict 先例） | 新单测断言「每个可见槽位点击必产生确定的用户可见效果」；L0/L3 复跑全绿 |
| **P0-4** | **当前输入状态回显**：面板显示中/英、全/半角当前态（现仅 `currentFullwidth` 入参，无中文态） | e2e 断言状态切换后面板回显跟随；不得引入阻塞 I/O |
| **P0-5** | **键盘可达性 + 槽位 tooltip**：Tab/Shift+Tab/方向键移焦、Enter/Space 激活、Esc 收起（守 D0032）；每槽 tooltip = 名称 + 当前态 | `L1-quickpanel-hotkey.ps1` 扩展用例 + 造坏数据证明有牙 |
| **P0-6** | **装机版 DPI 复验（T5 收口）**：`ROADMAP.md` 记录 150% 装机版仍 245×48，而修复已在 1.1.7 落地、现已发布 1.1.9 ⇒ 必须在**装机产物**上重测 100/150/200% | 实测尺寸/位置进 `test/regression/cases/`，不符则登记 DEFECTS（id 沿用 `D00xx`） |
| **P0-7** | 面板位置持久化（现每按 `Alt+,` 按光标重定位）：多屏 + DPI 变更下的边界钳制 | 单测覆盖「屏外坐标钳制」；不改 `quickpanel.state` 语义（位置另落一份，需在 ADR 说明） |
| **P1** | 声明式槽位表（对齐 `AddUIFunction` 的思路，但自有实现）——改动面大，需 ADR | ADR + 迁移测试；本轮**不排进 1.1.x** |

## 8. 基线与门禁（发布不得比现状差）

- 常驻资源基线（`ROADMAP.md`，搜狗实测 6 进程 / 183.4 MB / 0.78%）：**常驻进程 1**、
  `WeaselServer.exe` **private < 40 MB**、**空闲 CPU < 0.1% 单核**（空闲 60s 取 ≥10 样本的 max）。
- 每片最小门禁 = 对应单测 + `run-all.ps1 -Level L0`；碰安装器/卸载则加 L1；
  **每次 push 前跑 L0**；改 ≥3 文件加 L4；改动大或发版前加 L3；**发版验收必跑 L5**（`-Build`，最长约 30 分钟）。
- 变异测试守门：新增检查脚本必须**造坏数据证明它会红**（DEFECTS.yaml 与 L3 同思路）。

## 9. 工具坑（新会话勿重踩，本轮实测）

- 键盘注入 `Alt+,` 在沙箱**唤不起面板**（`Alt+.` 可以）——触发用 `tmp\quickpanel-evidence\QpTrigger.exe <pid>`（PostMessage 40018/40020）。⚠ `tmp/` 被 git-ignore，换会话可能已不存在，需重建。
- `makensis.exe` 不在 PATH（`C:\Program Files (x86)\NSIS\Bin\`），且 `install.nsi` **必须** `/DFLUXING_OUTFILE=<绝对路径>`，`/O` 无效。
- `Select-String` **没有 `-Recurse` 参数**（本轮实测报错）；目录级内容搜索用 `grep` 工具。
- `Copy-Item` 目标为 `D:\Program Files\` 的 bash 命令会被运行时硬策略整条拦掉 ⇒ 恢复安装目录只能走安装器；反向读不受限。
- 读注册表卸载项要 `Set-RegistryView` 或用 `WOW6432Node` 路径（本轮实测 `HKLM:\Software\WOW6432Node\Fluxing\Weasel`）。
- CTIP `HKLM:\SOFTWARE\Microsoft\CTF\TIP\*\LanguageProfile\*` 的 `Description` 匹配「搜狗」返回空 ⇒ 该路径无效，勿重试。

## 10. 执行起点

P0-1 需要**交互式操作搜狗**（点击其状态栏打开快捷设置），无法在沙箱内自动化，必须由用户在场配合。
P0-3 / P0-6 / P0-7 不依赖搜狗观察，可立即开工。
