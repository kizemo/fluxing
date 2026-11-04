# 0035. 快捷设置栏的位置持久化到独立文件 `quickpanel.position`；启动期读一次并缓存，恢复时钳制回可见工作区

- **状态**: Accepted
- **日期**: 2026-11-04
- **决策者**: Fluxing maintainers
- **相关**: spec 099（参考逆向搜狗输入法优化快捷设置栏）P0-7；ADR 0019 / 0020 / 0026（快捷面板状态持久化与落点）；constitution P2 / R7；lessons-learned L179

## Context

快捷设置栏（`QuickPanelDialog`）自 ADR 0022 起是**常驻面板**，显示时固定落在
主屏右下角（`PlacePanelAtBottomRight()`，见 `WeaselServer/QuickPanelDialog.cpp`）。
用户无法把它放到自己习惯的位置（副屏、常用手边），每次显示都回到右下角。

同类的第三方面板提供「拖到哪记住到哪」的行为。这属于**黑盒可观察的用户
可见行为**，本 fork 可以自行实现；但按合规红线，**只观察行为，不提取、不复用
第三方的二进制、资源、词库或专有文案**，实现必须自有。

本 fork 已经有一条**位置相近但语义不同**的持久化：`quickpanel.state`
（`include/FluxingAlwaysShow.h`），存的是「常驻开关」。关键约束是它的读盘判据
`always_show::ResolveAlwaysShowFromFiles` 依赖**第一行内容精确等于
`always_show=1` / `always_show=0`**。

另有三条硬约束同时生效：

1. **R7（单一真相源）**：同一份状态不得有两个来源。把 `pos_x` / `pos_y` 塞进
   `quickpanel.state` 会同时破坏上述精确匹配判据，并制造「一个文件两个键」的
   混合真相源 ⇒ 位置必须另立文件。
2. **P2 红线**：TSF 侧与面板点击回调内禁止阻塞 I/O。位置若在面板显示或点击
   回调里现读盘，就是阻塞 I/O。
3. **spec 055**：`RestoreAlwaysShowMode()` 绝不 auto-show 面板 —— 恢复位置
   **只回填缓存**，不得显示面板。

多屏场景还有两个环境事实：左侧副屏的工作区坐标**可以为负**（`left` / `top`
为负）；显示器拔插与分辨率 / DPI 变化会让已存位置落到可视区之外。

## Decision

**一、位置落独立文件，不污染状态文件**

位置写入 `<用户数据目录>\quickpanel.position`，与 `quickpanel.state`
**同目录、不同文件**。位置与常驻状态各自是单一真相源，互不污染（R7）。
用户数据目录沿用 `FluxingUserPaths.h::ResolveFluxingUserDataPath()`
（ADR 0020 已把落点收敛到 fork 用户数据目录，与上游 `rime/weasel` 互不冲突）。

**二、落盘格式**

`pos_x=<int>\npos_y=<int>\n` —— 两行 ASCII，十进制整数，**允许负坐标**。
解析判据：两个键必须同时存在；重复键以**第一次出现**为准；坐标绝对值超过
100000 视为损坏（防野值与 `x + w` 溢出）；容忍 CRLF；任何不满足的输入一律
判为「无有效位置」，绝不猜。

**三、共享纯函数下沉**

新增 `include/FluxingPanelPosition.h`（header-only，namespace
`fluxing::panel_position`）：格式 / 解析 / 读写 / 钳制 / 工作区枚举全部在此，
生产代码与单元测试**同源**（R7 精神）。写盘采用「截断写 + 回读自证」，
失败即删除坏文件 —— 与 `always_show::WriteStateFile` 同款，避免半截文件
被当成有效状态。

**四、读写时机**

- **写**：**拖动结束**时（`WM_LBUTTONUP` 且判定为真拖拽）写**原始位置**，
  不在保存时钳制，避免松手瞬间面板跳动。纯点击不写盘。写盘发生在
  WeaselServer 进程内、非 TSF 回调 ⇒ 不违反 P2。
- **读**：**服务端构造函数**内 `QuickPanelDialog::RestorePosition()` 读一次并
  缓存（紧随 `AboutDialog::Initialize()` 之后）。`Show()` 建窗只读内存缓存
  ⇒ **显示路径零 I/O**（P2）。恢复**不显示**面板（spec 055）。

**五、钳制规则（三段）**

只在**恢复时**（`Show()` 建窗）与 **`WM_DPICHANGED`** 时执行：

1. 面板矩形**完整落在某个工作区内** ⇒ 原样保留（这一条承载多屏负坐标用例）。
2. 否则 ⇒ 选「面板中心点最近、且尺寸容得下面板」的工作区，在其中钳制
   `x ∈ [wa.left, wa.right - w]`、`y ∈ [wa.top, wa.bottom - h]`。
3. 一个工作区都容不下面板 ⇒ `(0, 0)`。

工作区来自 `EnumDisplayMonitors` + `GetMonitorInfoW` 的 `rcWork`（排除任务栏）。

**六、DPI 变化语义**

`WM_DPICHANGED` 时：有持久化位置 ⇒ 保留用户位置并按新工作区钳回可见区；
无持久化位置 ⇒ 沿用右下角默认锚点（**未拖过面板的用户行为完全不变**）。

## 被否决的替代方案

| 替代方案 | 否决理由 |
| --- | --- |
| 把 `pos_x` / `pos_y` 塞进 `quickpanel.state` | 破坏 `always_show=0\|1` 的精确匹配判据 ⇒ 一个文件两个真相源，直接违 R7 |
| 存注册表 | 同步注册表 I/O 发生在拖拽回调 ⇒ 被 P2 明令禁止；且用户数据目录才是本 fork 既有落点口径（ADR 0020） |
| 在 `Show()` 里现读盘 | 显示路径上的阻塞 I/O，违 P2 |
| 恢复位置时顺带 auto-show 面板 | 违 spec 055（常驻恢复不得 auto-show 面板） |
| 用 `WINDOWPLACEMENT` / `SetWindowPlacement` | 记录的是窗口位置 + 状态 + 顺序，跨屏语义由 Windows 自行解释、不可控，且会把「未拖过」的默认态也写进去；本 fork 只需要 (x, y) 两个自由度 |
| 存「显示器序号 + 相对偏移」 | 显示器拔插后序号失效，需要额外的显示器身份映射，复杂度不换收益 |

## Consequences

- ✅ 面板位置跨重启保留；多屏（含负坐标副屏）可用。
- ✅ `quickpanel.state` 的语义与格式**零改动** ⇒ R7 守住，
  `ResolveAlwaysShowFromFiles` 的判据不受影响。
- ✅ 显示路径零 I/O、恢复不 auto-show ⇒ P2 与 spec 055 同时守住。
- ✅ 格式 / 解析 / 钳制 / 跨重启均有单元测试覆盖（`test/TestQuickPanelDialog`
  T24 系列），生产与测试同源纯函数。
- ✅ `WeaselIPCData.h` **未改动** ⇒ 不触发 IPC 四边同步。
- ⚠️ 跨屏拖拽产生的「悬空半屏」位置在重启后会被吸附进某一个屏（已知取舍）。
- ⚠️ 钳制只在恢复时与 `WM_DPICHANGED` 时执行；运行中拔掉副屏（不触发 DPI
  变化）时位置不会立刻拉回，直到下次恢复。
- ⚠️ 该文件首次出现之前，面板仍落在右下角默认锚点（行为不变）。
- ⚠️ 单元测试里的「合成点击」依赖**真实全局光标**在按下与抬起之间不漂移，
  而真实环境里外部鼠标移动会让生产侧（正确地）判成拖拽 ⇒ 用例可能偶发红。
  该抖动已在 lessons-learned **L179** 记录并在 harness 侧消除。
- ↩️ 可逆：删除 `quickpanel.position` 即回到默认锚点；不涉及线格式与协议，
  回退不需要同步改前端或部署端。
