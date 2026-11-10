---
date: 2026-11-10
batch: V
slug: batch-v-panel-anchor-and-theme
status: 代码/单测/装机已完成，真机复验待用户
adr: [0051, 0052]
---

# 批次 V · 快捷设置栏「位置锚点（P0）」+「方向 A 主题重绘（P1）」

> 接力请先读本文件，再读 `docs/adr/0051-*.md` 与 `docs/adr/0052-*.md`。
> 上一批：批次 U（4 槽化 + 键帽符号/笔宽 DPI 严格同比，已上机并提交 `d38b2b5`），
> 见 `docs/archive/handoffs/handoff-2026-11-09-batch-u-panel-4slots-dpi-symbols.md`。

---

## 1. 背景与目标

用户两条原话：

1. 「实测更改分辨率，键槽图标相对大小不会变化了。但相对于屏幕的位置，在分辨率变化时会大幅度变化，
   比如分辨率增大后，设置栏的位置会向左上偏移。这个仍然需要修改。」
2. 「设置栏缺乏美感。请调用 /huashu-design ，再次对设置栏进行设计、美化」

⇒ 批次 V = **P0 修位置漂移** + **P1 视觉重绘**。

**P0 根因**：面板位置存的是**绝对像素**。分辨率放大后绝对像素不变 ⇒ 相对屏幕左上偏移
（分辨率越大偏移越明显）。放大器两个：① 位置只写不重算，`Show()` 复用旧绝对坐标；
② 显示器/工作区变化（`WM_DISPLAYCHANGE`、`SPI_SETWORKAREA`）完全没被监听。

**P1 方向**：huashu-design 三方向真实初稿（HTML + 截图）已展示给用户，
**用户选定方向 A（微软拼音 IME 工具栏 / Fluent Design 2 标杆迁移）**，
gate 文件 `docs/design/quickpanel-v6/direction-approved.md` 已落盘（6163 B）。
方向 A 只借**结构语法**（半透明层级 / 1px 发丝描边 / 分层软投影 / 分组轨道 / 光学均衡），
**不借配色**；品牌橙只用于中文侧状态与 hover 描边。

---

## 2. 进度

### 2.1 P0 位置锚点（代码 + 单测 + 装机 ✅）

| 产物 | 路径 | 状态 |
|---|---|---|
| 锚点纯函数 + v2 落盘 | `include/FluxingPanelPosition.h` | ✅ 已改 |
| 接线（`Show` / `WM_DPICHANGED` / `WM_DISPLAYCHANGE` / `WM_SETTINGCHANGE`） | `WeaselServer/QuickPanelDialog.cpp` / `.h` | ✅ 已改 |
| 单测 T40 锚点组（26 条断言） | `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` | ✅ 全绿 |
| ADR | `docs/adr/0051-quickpanel-position-anchored-to-workarea-ratio.md` | ✅ 已写 |
| 装机 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` | ✅ SHA256 双证通过 |

**语义**：位置存**相对工作区的比例锚点**，分母 = 工作区尺寸 − 面板可摆放空间
（保证锚点落在 [0,1] 且面板不溢出屏幕）。

**落盘 v2**：v2 的**前两行与 v1 逐字一致** ⇒ 旧文件按 v1 解析，向后兼容；
缺任一锚点字段一律按 v1 处理；首次拖动自动升级，**不需要迁移脚本**。

**四处重锚触发点**：

| 触发点 | 行为 |
|---|---|
| `Show()` | 三级优先级：锚点 → 旧绝对位置 → 右下角 |
| `WM_DPICHANGED` | 重映射 + 重锚 |
| `WM_DISPLAYCHANGE` | 无条件重锚 |
| `WM_SETTINGCHANGE` | 仅 `SPI_SETWORKAREA` 时重锚 |

**认显示器三级策略**：原点相同 → 交集面积最大 → 中心距离兜底。
原屏消失 ⇒ 移到最近屏，**不返回 nullptr**（面板不消失）。

### 2.2 P1 方向 A 主题（代码 + 单测 + 像素自验 + 装机 ✅）

| 产物 | 路径 | 状态 |
|---|---|---|
| 主题单一真相源 | `include/FluxingPanelTheme.h`（新建） | ✅ |
| 接线（`PaintThemeChrome` / `FinalizeThemeAlpha` / `ApplyPhysicalLayout`） | `WeaselServer/QuickPanelDialog.cpp` / `.h` | ✅ |
| 单测 T41 主题组（30 条断言） | `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` | ✅ 全绿 |
| ADR | `docs/adr/0052-quickpanel-direction-a-layered-translucent-theme.md` | ✅ 已写 |
| 设计稿 | `docs/design/quickpanel-v6/` | ✅ |

**交付形态固定**：**Win32 GDI 自绘** —— 无 CSS、无 D2D、无 GDI+、无 blur/backdrop-filter。
只做「半透明纯色 + 发丝描边 + 分层软投影」。

**alpha 模型**：**绝不使用 GDI `AlphaBlend`**（品牌 logo 黑框前科：GDI 把半透明源像素当
**反向权重**，实测数值 ≈ `255 − 源 alpha`）⇒ 自建 **src-over 预乘合成**，直写 DIB alpha 字节，
旧「按颜色阈值猜 alpha」路径整体下线。

**圆角语义**：改用 **SVG rx**（角圆心 = l+r, t+r）。旧 `CreateRoundRectRgn` 的角半径只有「值/2」，
实测旧面板比设计稿「方」。

**窗口尺寸**：加高 6 logical px **仅多一条透明阴影带** ⇒ 面板体语义仍是 **155×32**，
槽位几何 / 命中区 / 既有断言不变。投影带高度按 `dpr_x` 缩放（无循环依赖）；
三层投影偏移与带宽同源（偏移 ≤ 带宽）。

**hover**：只画 **1px 橙描边轮廓浮出（α .50）**，**不铺橙底**
（守用户指令「hover 时底色不变，只 icon 线条变橙」）。

**状态取色**：视觉位 1「中/英」中文态 `#FF5F31` / 英文态 `#241F1A`；
视觉位 2 全角环 `#FF5F31` / 半角实心点 `#241F1A`；active 白。

**R7 强化**：`ChromeSpec` 构造下沉到 `FluxingPanelTheme.h::BuildChromeSpec`（生产与单测同调），
DPI 换算复用 `include/FluxingPanelDpi.h::ScaleByFactor`。

---

## 3. 已验证 / 推断 / 未验证

### 3.1 已验证（有机器证据）

| 项 | 证据 |
|---|---|
| 单测 331/331 | `=== 331 / 331 assertions passed ===`，`TEST_EXIT=0` |
| 装机双证 | 装机 `WeaselServer.exe` SHA256 `23DFD12D4A6291F523A7EBAD3DFCC5120803F826E318483B8EFDC973E9627611` = 构建产物（2491392 B @2026-11-10 10:49:26）⇒ **已上机** |
| 数据一致性 | `data\default.yaml` 仓库与装机均 18448 B / `05CC2DDB21A1ABF35DF0D98294C946F8B528D6B1C31596FFEFD2D1B2625B22E0`，逐字节相同 |
| 面板像素自验 | `FLUXING_QP_DIAG_DUMP=1` ⇒ `qp-dump.bmp` / `qp-logo.bmp` / `qp-state.txt`（`hovered=-1 active=-1 panelW=155 panelH=32 dpr=1.000 radius_phys=14`）；合成到桌面底色放大 4× 目视：红猿 logo、品牌橙「中」、灰胶囊轨道、深墨点 + 逗号、暖白半透明面板体、圆角正确 |
| L0 静态门禁 | `scripts/gate/L0-static.ps1 -Mode source`（见 §6） |
| C1 判据已修 | `tmp/acceptance-check.cmd` 的 C1 由硬编码 18217 B / `917509f6f2460230` 改为**动态比对仓库 `output\data\default.yaml`**（size + sha256 双比） |

### 3.2 推断（合理但未直接验证）

- 位置锚点在**多显示器 + 不同缩放比例混搭**下的实际观感未做真机验证；纯函数层（T40）覆盖了
  边界，但 Windows 实际派发的 `WM_DPICHANGED` / `WM_DISPLAYCHANGE` 时序未在真机上抓到。
- 方向 A 的「分层软投影」在真实桌面背景（尤其浅色壁纸）上的观感未做真机验证；
  像素自验是在**深色底色**（RGB 96,110,128）上做的合成。
- 「内容不透明判定用 chrome 快照 RGB 差异容差 2/通道」这条阈值，在**极低对比内容**下
  （如白底白字）可能误判 ⇒ 未构造对抗用例。

### 3.3 未验证（**必须用户人工**）

面板是 **IME 激活瞬间才存在的自绘顶层窗**，跨进程 `FindWindow` 返回 0
（`EnumWindows` 能拿到句柄但下一进程即失）⇒ **最后一公里只能人工**。

1. **P0 真机复验**：真实改一次分辨率（不是只改缩放），看设置栏相对屏幕位置**不再向左上偏移**。
2. **P1 真机视觉复验**：方向 A 视觉是否满意 —— 半透明面板体 / 发丝描边 / 底部软投影 /
   状态轨道 / 品牌橙「中」/ hover 只描边不铺底。
3. 4 键帽顺序与托盘「快捷键设置」入口仍在（批次 U 遗留，本批未改结构，顺带复验）。

---

## 4. 下阶段建议

1. **等用户真机复验** P0 + P1 两条（见 §3.3）。若视觉不满意，改 `include/FluxingPanelTheme.h`
   的调色板 / alpha / 几何常量即可（单一真相源，改一处即可，不动 cpp）。
2. **若用户反馈「还是偏方」** ⇒ 调 `InsideRoundRect` 的 rx 语义参数；**若反馈「太透」** ⇒ 调
   `kPanelBodyAlpha`（当前 184）；**若反馈「阴影太重」** ⇒ 调 `kShadowH` / 投影偏移。
3. `docs/design/quickpanel-v6/design-demos/*.html`（含 base64，约 1.3 MB）**未提交**，
   仅提交了 `design-spec.md` / `direction-approved.md` / 三个 PNG 截图作为证据。
4. 本批**未触碰**：`include/WeaselIPCData.h`、IPC 协议、`output/install.nsi`、`librime/`、
   `output/data/**`、4 键帽结构与 `kVisualToLogical={0,4,5,1}`、托盘右键「快捷键设置」入口。

---

## 5. 证据路径

| 内容 | 路径 |
|---|---|
| 主题真相源 | `include/FluxingPanelTheme.h` |
| 位置锚点真相源 | `include/FluxingPanelPosition.h` |
| 面板实现 | `WeaselServer/QuickPanelDialog.cpp` / `.h` |
| 单测（T40 + T41） | `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` |
| ADR 0051 | `docs/adr/0051-quickpanel-position-anchored-to-workarea-ratio.md` |
| ADR 0052 | `docs/adr/0052-quickpanel-direction-a-layered-translucent-theme.md` |
| 设计 spec / gate / 截图 | `docs/design/quickpanel-v6/` |
| 变更记录 | `CHANGELOG.md`（`## [Unreleased]` 首条） |
| 装机包 | `output/archives/fluxing-1.2.1-installer.exe` |
| 装机位置 | `D:\Program Files\fluxing\weasel\` |
| 验收脚本 | `tmp/acceptance-check.cmd` |
| 构建脚本 | `tmp/build-test-panel.cmd` / `tmp/build-installer.cmd` |

---

## 6. 本轮工具坑（勿重试）

1. **`edit` 一次只能传一组 `old_string`/`new_string`** —— JSON 里重复键会被**静默丢弃**，
   返回 `1 block(s)` 只说明匹配到的那一处被替换。多处改动 = 多次调用 + 事后 grep 核对落盘。
2. **Windows PowerShell 5.1 读无 BOM UTF-8 `.ps1` 会按 ANSI 解码** ⇒ 脚本里的**中文注释**
   精确匹配全部失败（表现为「断言全部不命中」）。**必须用 `pwsh`** 跑补丁脚本。
   本轮 `tmp/patch-v6-theme.ps1` 就是这样失败的：`powershell` 跑不中、`pwsh` 一次 14 处全中。
3. **GDI+ 读取 `qp-dump.bmp` 会交换 R/B** ⇒ 预览图颜色反转（第一版把橙「中」显示成蓝、
   焦点框显示成橙）。**必须手工交换通道**再判色，生产取色本身没错。
4. **仓库策略拦截永久删除命令**（`Remove-Item` 被拒）⇒ 清理走可恢复删除
   （Windows 顶层 `rm -- "<target>"`）。
5. **`cmd` 括号块里的 `)` 会提前结束代码块** —— `echo` 文本里若含未转义的 `)`，
   整个 `if (...)` 结构会被截断。写 `acceptance-check.cmd` 这类脚本时，
   `echo` 文本里避免裸 `)`，或用 `^)` 转义。
6. **`certutil -hashfile` 的输出行要先用 `findstr` 正则筛纯 hex 行再 `tokens=1` 取值** ——
   本地化输出里还有「CertUtil: -hashfile 命令成功完成。」之类的杂行。
7. **面板是 IME 激活瞬间才存在的自绘顶层窗** ⇒ 跨进程 `FindWindow` 返回 0 是**常态**，
   不能读成「代码没生效」。自动化只能负责代码层 / 数据层 / 像素几何层证据。
8. **`xbuild.bat weasel` 按设计只编 x86**（librime 是 Win32-only）⇒ 落在 x64 TSF 前端的改动
   必须显式跑 `msbuild RimeWithWeasel\RimeWithWeasel.vcxproj /p:Platform=x64`
   → `msbuild WeaselTSF\WeaselTSF.vcxproj /p:Platform=x64`（**顺序不能反**）。
   本批改动全在 x86 `WeaselServer` 侧，故 `tmp/build-installer.cmd` 一条链足够。
