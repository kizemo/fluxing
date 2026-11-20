# ADR 0068 — 账号页 DPI 感知布局：整表等比缩放（修缺陷 C：>150% 缩放裁切「换钥提示」）

- **Status**: Accepted（缺陷 C 根因已坐实、失败方向单一（只可能「偏大」不会「偏小」）、代价可逆）
- **Date**: 2026-11-20
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（沿用 spec 100 的账号页；本 ADR 只动**布局几何**，不动协议、不动文案语义）
- **Revises**: ADR 0063（账号页信息架构）中的**几何字面量口径** —— 坐标表由「固定像素」改为「96 dpi 基准 + 按 DPI 等比缩放」；ADR 0063 的**可见性规则、控件 ID、文案、窗口角色**全部不变
- **Lessons**: `.specify/memory/lessons-learned.md`（本批条目待写）
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 缺陷 C 的现场（批次 BA 真机实测，非推断）

批次 BA 在本机（144 dpi = 150% 缩放，主屏 2560×1440）用探针
`reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1` 实测账号页：

| 项 | 实测值 |
|---|---|
| 账号窗客户区 | `480 × 290` **物理像素** |
| 换钥提示控件 `kIdKeyRotationNote = 1035` | `STATIC`，`456 × 36` **物理像素**，字体 = 宋体 144 dpi |
| 文案 | 37 个全角字（`重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端数据。`） |
| `DT_CALCRECT` 单行 | 宽 `666 px`（= 37 × 18）、高 `18 px` |
| 装箱（456 宽） | 折 **2 行** ⇒ 需 `36 px` |
| 控件高度 | `36 px` ⇒ **余量 0 px** |
| 像素墨迹带 | `0–16` / `18–34`（第 35 行无墨）⇒ A-1/A-2/A-3 PASS（当前版本**恰好**没裁） |

**推论（算术，非实测）**：175% 需 42 px、200% 需 48 px ⇒ **>150% 缩放的机器必然裁掉末行**。

### 事实 2 — 缺陷的根因是「盒子没跟着字体放大」

UI 字体是 `DEFAULT_GUI_FONT`（9pt message 字体）。它**随 DPI 放大**，而布局表里的
字面量是**固定物理像素**（`CloudAccountUi.h` 里 `{kIdKeyRotationNote, 12, 156, 456, 36}`）。
两者不同步 ⇒ 文字需要的盒子随 DPI 变大，盒子不变 ⇒ 余量被吃光。

这不是单个控件的问题：**所有**静态文案行、所有输入框、所有按钮都有同一个形态
（例：200% 下 26 px 高的输入框配 24 px 字高，同样危险）。

### 事实 3 — 本进程**不做**系统级 DPI 缩放（实测）

- 清单 `PerMonitorHighDPIAware.manifest` = **V1**（`<dpiAware>True/PM</dpiAware>`），
  **不是** PerMonitorV2。⚠️ 批次 BA 的 handoff 把这条写成 PerMonitorV2 —— **本 ADR 更正之**。
- `WeaselServer.vcxproj` 5 处 `EnableDpiAwareness=PerMonitorHighDPIAware`。
- 实测：`CreateWindowExW` 传入的尺寸 = **物理像素**（客户区实测 480×290 物理、
  子控件实测 456×36 物理 ⇒ 系统没有替我们缩放）。

⇒ **布局必须由应用自己按 DPI 缩放**，这与 manifest 声明什么无关。

### 事实 4 — 仓库既有惯用法

`.specify/memory/project-knowledge.md:697-699` 已有 `Scale(v,dpi)=MulDiv(v,dpi,96)`；
WeaselUI 的快捷面板早就按 DPI 缩放（ADR 0049/0050）。**账号页是唯一漏网的模块**。

---

## 决策

### 决策 1 — 整表按 DPI **等比**缩放，而不是「只把 1035 的高度按 DPI 算」

等比缩放让「文字 : 盒子」的比值**与 DPI 无关** ⇒ 任何缩放下都不会裁切；
96 dpi 下逐像素等于旧布局 ⇒ **零行为变化**。

只调高度（候选 B′ / A-lite）都不成立：

- **B′（运行时量高度 + 重排 y）**：行数仍随 DPI 增长（盒子没变宽 ⇒ 折行数变多），
  且不修兄弟控件（200% 下 26 px 输入框配 24 px 字高）。
- **A-lite（`MulDiv(36,dpi,96)` 只缩高度不缩宽）**：盒子变高但不变宽 ⇒ 文字更宽地
  折行 ⇒ ≥288 dpi 仍会 3 行溢出。

### 决策 2 — 缩放入口**唯一化**，且**不保留「无 dpi」的兼容重载**

- 布局表统一走 `AccountScaleLayout(base, dpi)`；标量走 `AccountScale(v, dpi)`。
  客户区尺寸、控件几何、窗口尺寸**同一把尺子**。
- 全部对外函数**显式带 dpi 参数**：`AccountRoleLayout` / `AccountVisibleLayout` /
  `AccountVisibleIds` / `AccountVisibleControlIds` / `AccountWidgets` /
  `AccountClientSize`。
- **不保留**无 dpi 的重载：那个形态**就是**本缺陷的形态，留着就是复发入口。
  单测里的旧字面量断言改为显式传 `kAccountBaseDpi`（96）保留原语义。
- 基准表改名 `…LayoutBase()`，语义 = 「96 dpi 基准表」。

### 决策 3 — 缩放系数取自**系统 DPI**（与 stock 字体光栅化同源）

生产侧 `AccountUiDpi()`：

1. **主判据** = `GetDpiForSystem()`（user32，Win10 1607+）。**动态 `GetProcAddress` 取**
   —— 静态导入会让 Win8.1 及更早系统整个进程加载失败；取不到就回退 ②。
2. **回退** = gdi32 `GetDeviceCaps(GetDC(nullptr), LOGPIXELSX)`（Win95+ ⇒ 永远可用）。
3. **交叉校验**（**不参与布局**）：把 `DEFAULT_GUI_FONT` 选进 DC → `GetTextMetrics` →
   行高 `tmHeight + tmExternalLeading` → `AccountEffectiveDpiFromFontPx` 反推
   （12→96、15→120、18→144、21→168、24→192、30→240、36→288）；与系统 DPI 偏差
   > 12.5% ⇒ 记一行 `[FluxingAccountDlg]` 诊断日志（`acctdlg`）。

**为什么不用 `GetDpiForWindow`**：stock 字体是**进程级**资源，按**系统** DPI 光栅化；
`GetDpiForWindow` 给的是**窗口所在屏**的 DPI。多屏混合 DPI 时两者不同 ⇒ 按窗口 DPI
缩放 ⇒ 与实际渲染出来的字体失配 ⇒ **反而裁字**。

**为什么不用「量字体行高反推 DPI」**（⚠️ 本 ADR **实施期推翻**的早期口径）：本机实测
（系统 144 dpi）`DEFAULT_GUI_FONT` 真实行高 = **21 px**（宋体 9pt），而 144 dpi 下
**控件**单行高才是 18 px ⇒ 用 tm 行高反推得到 **168 dpi**（+17%）；96 dpi 机器上同理
得到 112 dpi ⇒ **违反「96 dpi 逐像素不变」不变量**。根因 = 「字体 tm 行高」与「控件
单行高」是**两个量**，批次 BA 记录的 18 px 属后者 ⇒ 把「控件行高模型」误当成「字体
度量」了。改法 = 直接问系统要 DPI，字体度量只留作告警。

**为什么禁用 `GetDpiForMonitor`**：它会引入 `api-ms-win-shcore-scaling-l1-1-1.dll`
依赖，Win10 24H2 上该 DLL 缺失 ⇒ `regsvr32` 退出码 3（教训 L122）。本 ADR 只用
user32 / gdi32 直出的 API（`GetDpiForSystem` / `GetDeviceCaps`），**不引入新 DLL 依赖**。

### 决策 4 — 客户区尺寸同样缩放；`WM_DPICHANGED` **显式吞掉**

- `AccountClientSize(role, dpi, &w, &h)` = 基准 × dpi/96（否则窗口边框会裁掉控件）。
- `WM_DPICHANGED` **不跟随**屏幕 DPI 缩放（跟随 ⇒ 与进程级字体失配 ⇒ 裁字），
  同时**不让 `DefWindowProc` 默认处理**（它会按系统建议矩形缩放窗口，而子控件仍按
  创建时的缩放摆位 ⇒ 客户区与控件对不上 ⇒ 控件被窗口边缘裁掉）。
  ⇒ 显式 `return 0`：窗口尺寸与控件布局始终来自**同一份**缩放。

### 决策 5 — 判据落进真相源（R7），余量口径 = **至少 1 行**

- 文案本体迁入真相源 `AccountKeyRotationNoteText()`（生产与单测共用一份，R7）。
- 判据函数：`AccountTextWidthEm` / `AccountTextLineCount` / `AccountLineHeightPx` /
  `AccountRequiredHeightForText` / `AccountMinHeightForText`。
- 「余量 ≥ 1 行」= 缺陷 C 的判据：150% 现场余量 0 ⇒ 必然判红。
- 单测里另有一条**反向判据**：用缺陷 C 的现场几何（144 dpi 未缩放的 456×36）算余量
  必须 = 0 ⇒ 证明判据本身不是恒真。

### 决策 6 — 100% 缩放下逐像素不变（零行为变化）

布局表字面量**一个都没改**，只是多了一层缩放；`AccountScale(v, 96) == v` 恒成立
⇒ 96 dpi 下控件坐标、尺寸、客户区尺寸**逐像素等于**改动前。ADR 0063 冻结合同
（换钥提示 `(12,156,456,36)`、状态行 `(12,198,456,40)`、关闭钮 `(368,250,100,28)`、
客户区 `480×290`）由单测逐条守住。

---

## 落码状态

| 文件 | 内容 |
|---|---|
| `WeaselServer/Cloud/CloudAccountUi.h` | 真相源：新增 `kAccountBaseDpi` / `AccountScale` / `AccountScaleLayout` / `kAccountUiEmPx96` / `kAccountUiFontPx96` / `AccountEffectiveDpiFromFontPx` / `AccountKeyRotationNoteText` / `AccountWideCharCount` / `AccountTextWidthEm` / `AccountTextLineCount` / `AccountLineHeightPx` / `AccountRequiredHeightForText` / `AccountMinHeightForText`；五张基准表改名 `…Base()`；全部派生函数带 `dpi` |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | 新增 `AccountUiDpi()`（**系统 DPI**：`GetDpiForSystem` **动态取** → 回退 gdi32 `GetDeviceCaps(LOGPIXELSX)`；字体度量降级为**交叉校验告警**）；`CreateWidgets` / `ApplyLayout` / `AccountClientSize` 透传 dpi；`ApplyLayout` 内接**余量校验**（`AccountMinHeightForText` 告警，R7 同源）；文案改用 `AccountKeyRotationNoteText()`；`WM_DPICHANGED` 显式吞掉；`WM_DPICHANGED` 兜底 `#define` |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 旧调用点补显式 `kAccountBaseDpi`；新增 `TestAccountPageUiDpiScale()`（8 条判据，含反向有牙判据） |

**不改**：`include/WeaselIPCData.h`、任何 IPC 消息类型、任何**用户可见文案**、
`librime/`、控件 ID 分配（`kIdKeyRotationNote = 1035` 不变）。

---

## 残余风险（未闭环）

1. **混合 DPI 多屏**：stock 字体是**进程级**（系统 DPI 口径），本 ADR 按**系统 DPI**
   缩放 ⇒ 窗口挪到更高 DPI 的屏时会显得偏小，但**不会裁字**。彻底修好需要 per-window
   font（`CreateFontIndirectW` + `SystemParametersInfoForDpi`）+ 按窗口所在屏缩放，
   本批**不做**（改动面大、收益是「好看」不是「不裁」）。
2. **175% / 200% 真机未验**：本批的实证只覆盖 150%（批次 BA 现场）。更高缩放档位
   由单测的缩放律覆盖（几何等价），但**没有真机像素证据**。
3. **A-4 / A-5 未测**：未登录态下不出现换钥提示（需登出，属用户账号状态变更，
   探针不自行发起）。
4. **字体度量与系统 DPI 的偏差**：stock 字体的 tm 行高与「控件单行高」不是同一个量
   （本机 21 px vs 18 px）⇒ 交叉校验在正常 Windows 上就会触发 >12.5% 偏差告警。
   **该告警只记日志、不影响布局**（布局只认系统 DPI）；它记录的是「字体度量模型与
   实际渲染不同」，**不表示布局算错**。
5. **未新建 `.specify/specs/`**：本批碰 3 个文件（constitution Principle III 通常要求
   spec），但本仓 AX/AY/AZ/BA 批次的既定实践是「ADR + handoff」。按 constitution
   「Justified deviations MUST be recorded」，**此处显式记录该偏离**。

---

## 已驳回的备选方案

1. **只把 `kIdKeyRotationNote` 的高度按 DPI 算，其余不动** —— 驳回：兄弟控件同病，
   且行数随 DPI 增长（盒子不变宽）。见决策 1。
2. **只缩高度不缩宽（A-lite）** —— 驳回：≥288 dpi 仍会 3 行溢出。见决策 1。
3. **按 `GetDpiForWindow`（窗口所在屏）缩放** —— 驳回：与进程级 stock 字体失配 ⇒
   多屏时反而裁字。见决策 3。
4. **把 DPI 硬编码成 144（现场值）** —— 驳回：那是「猜」，换台机器就错；且违背
   批次 BA 的判据口径（只认实测）。
5. **用 `GetDpiForMonitor`** —— 驳回：引入 `api-ms-win-shcore-scaling` 依赖，
   Win10 24H2 上 `regsvr32` 退出码 3（教训 L122）。见决策 3。
6. **走 `WM_DPICHANGED` 按屏幕 DPI 实时重排** —— 驳回：窗口与控件会各自按不同口径
   缩放（字体是进程级）⇒ 失配。见决策 4。
7. **做 per-window font（按窗口所在屏建字体 + `SystemParametersInfoForDpi`）** ——
   暂缓：能根治混合 DPI 多屏，但要引入字体生命周期（创建 / 销毁 / 重建），
   改动面与风险都远超本缺陷；本批先保证「永不裁切」。见残余风险第 1 条。
