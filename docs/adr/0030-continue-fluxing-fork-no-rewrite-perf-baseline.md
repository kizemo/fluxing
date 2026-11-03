# ADR 0030 — 路线定案：继续火流猩既有 fork，不做搜狗逆向重写；性能验收基线立标；UI 插件化以 UIPlugin.dll 导出切面为契约参考

- **Status**: Accepted
- **Date**: 2026-11-03
- **Spec**: [098-cloud-candidates](../../.specify/specs/098-cloud-candidates/spec.md)（本定案后的第一个能力 spec）；既有 P0 修复见交接卡 `docs/archive/handoffs/handoff-2026-11-03-fluxing-quickpanel-session-and-alt-dot.md` §6
- **Lessons**: —
- **Research**: `docs/research/sogou-reverse-2026-11/REPORT.md`（含 `evidence/` 24 件）
- **Supersedes / superseded by**: —（新增；与 ADR 0028「薄 TSF 壳 + IPC 只增不改」同向）

---

## 背景

火流猩的开发目标：对齐搜狗输入法的**输入能力**，剔除其内嵌广告、与搜狗其他软件的关联、以及与输入无关的功能，降低系统占用。

2026-11-03 用 REA（CLI 通道，`rea inspect-pe-imports` / `rea inspect-native-exports` + 运行时实测）对
`D:\Program Files (x86)\SogouInput` 16.8.0.5415 做了静态与运行时逆向研究，关键**已验证**事实：

1. **体积构成**：393 文件 / 803.1 MB。与输入无关的四组（云与联网 52.1 + 商业与助手 104.1 + Components 145.1 + 运维 34.2）= **335.5 MB ≈ 42%**，且不参与输入。
2. **架构解耦**：`SogouTSF.dll`（0.09 MB）是纯 COM 壳（只导出 `DllGetClassObject` 等 4 个标准入口）；真正引擎在 `ImeFunc.dll`（`DoInit`/`DoExit`/`GetImeFunc`）与 `UIPlugin.dll`（UI 插件契约）；联网能力集中在 `networkbase.dll`（89 导出，HTTP/WebSocket/信标/剪贴板同步）⇒「保留输入、砍掉联网」在库级成立。
3. **核心资产是数据不是代码**：`Data\sysmodel.bin` 单文件 353 MB（魔数 `DICT`），加 53 个 `sgim_*.bin` 共 382.6 MB 私有模型；无伪代码级分析能力（本机无 Ghidra/Hopper），其内部结构【未验证】⇒ 逆向复刻词义模型准确度**不可达成**。
4. **运行时占用（搜狗自身基线）**：6 进程、合计 private 183.4 MB、空闲 8s 采样 CPU 0.78%（单核）。

工程量结论（REPORT.md §6）：
- 路线 A「逆向重写搜狗级输入法」= **23–45 人月**，且准确度目标不可达；
- 路线 B「火流猩补齐可感知能力」= **7.5–13.5 人月**（云候选 1–2、皮肤 1–2、表情 0.5、AI 改错 0.5–1、看图识字 1–2、手写 2–3、账号同步 1、9 键 0.5）。

外部 AI 分析亦一致指向：重写难度远超继续推进火流猩，且要剔除的 42% 体积恰好是「不做那些东西」自动得到的。

## 决策

### 决策 1 — 继续火流猩既有 fork，不做逆向重写，不抽取搜狗数据

不采用任何反编译产物进入代码；不复制 `sysmodel.bin` / `sgim_*.bin` / `.cupf` 皮肤等任何搜狗数据。
逆向所得**只作为规格说明书与边界设计参考**。能力路线图按路线 B 逐项立项为 spec，第一个是云候选（098）。

### 决策 2 — 性能验收基线（每次发布必须回归）

| 指标 | 基线 | 判据 |
|---|---|---|
| 常驻进程数 | **1** | 仅 `WeaselServer.exe` 常驻；`WeaselTSF.dll` 注入宿主进程不计；`FluxingPhrasesDialog.exe` 仅用户唤起时计 |
| Server 内存 | **Private Working Set < 40 MB** | `Get-Process WeaselServer | Measure PrivateMemorySize64` |
| 空闲 CPU | **< 0.1%（占单核）** | 静止 ≥ 8s，两次 `CPU` 增量 / 窗口秒数 / 逻辑核折算，方法同 REPORT.md §5 |

对照物：搜狗 6 进程 / 183.4 MB / 0.78%。基线是**发布验收条款**：后续每个能力 spec（含 098 云候选）的验收段必须引用它；任一超标即发布阻塞，除非有 ADR 修订本基线。

### 决策 3 — 候选窗/皮肤插件化的契约切面照抄 UIPlugin.dll

`UIPlugin.dll` 的导出切面（13 个具名函数，完整 15 项清单见 `docs/research/sogou-reverse-2026-11/evidence/exports-UIPlugin*.json`）：
`InitDllEnv`、`RegisterImeClass`、`UnRegisterImeClass`、`CreatePluginUIRes`、`ReleasePluginUIRes`、
`SendUIResourceMessage`、`SendWndUIMessage`、`SendWndTopMessage`、`SendExtraUIMessage`、
`SendSkinPreviewMessage`、`SetUiLayoutStub`、`AddUIFunction`、`ShowAbout`。

这套切面把「UI 资源生命周期 / 消息路由 / 布局桩 / 注册钩子」分成了成熟边界，火流猩候选窗与皮肤子系统插件化时
**按同一切面粒度设计内部接口**（`WeaselUI`、`QuickPanelDialog`、皮肤资源加载）。

边界约束：切面是**仓内模块边界**的参考，不是第三方二进制插件通道 —— 火流猩不加载仓外 UI 插件 DLL
（攻击面、GPLv3 边界与 P8 约束）。

### 决策 4 — 网络边界：默认不联网，联网功能白名单化

搜狗侧抽出 185 个 host（埋点、设备指纹 `qimei`、推送长连、皮肤市场、智能助手等，清单见 REPORT.md §7），
在火流猩全部列为**永不连接黑名单**。任何联网能力（云候选、同步）只能连自建端点，且默认关闭、显式开启。

## 后果

- **正面**：不引入任何反编译代码 ⇒ GPLv3 洁净基线不受污染；42% 商业体积问题转化为「不做即达标」；
  性能基线给了可自动化的发布门禁；UI 插件化有了成熟切面，避免自造边界。
- **负面/代价**：词库与模型质量差距不会因路线图消失，只能靠 librime 数据生态追赶；皮肤市场、账号同步等
  体验对齐项要按 spec 排队，短期「不如搜狗」的体感仍存在。
- **既有实现的区分义务**：火流猩上屏借用剪贴板（CHANGELOG 已声明，已知限制约 2s）是**本机上屏机制**，
  与搜狗 `networkbase::ClipboardService` 的**跨设备上报**是两回事；文档表述必须区分，不得混同。

## 被否决的替代方案

1. **逆向重写搜狗级输入法（路线 A）**：23–45 人月起，353 MB 私有模型不可复刻 ⇒ 准确度目标不可达。否决。
2. **从搜狗包抽取词库/模型/皮肤**：法律（版权与许可）与质量（私有格式、结构未验证）双重不可行。否决。
3. **开放第三方 UI 插件 DLL**：攻击面、许可与维护成本不划算；只取其切面粒度做仓内边界。否决。
