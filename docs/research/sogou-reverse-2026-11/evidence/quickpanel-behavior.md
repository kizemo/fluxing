# 搜狗输入法「快捷设置栏」黑盒行为观察（P0-1）

- 采集日期：2026-11-05
- 目标版本：搜狗输入法 **16.8.0.5415**（`D:\Program Files (x86)\SogouInput`）
- 采集机：3840×2160 @ **150%**（`GetDeviceCaps(LOGPIXELSX)=144`，与 P0-6 三源互证一致）
- IME 宿主：`FluxingIMEProbeHost`（自建 WinForms 编辑器，pid 75684），已加载 `SogouTSF.ime` + `SogouPY.ime`
  ⇒ **搜狗为活动输入法**（用户 2026-11-05 手动切换）
- 本文件证据目录：`docs\research\sogou-reverse-2026-11\evidence\quickpanel\`
- 逐条「借鉴 / 改造 / 不抄」判定见 `.specify\specs\099-quickpanel-sogou-benchmark\gap-matrix.md`

> **证据分级**（沿用 `REPORT.md`）：
> **【已验证】**= 本机实测产出（有日志或截图可复核）、
> **【推断】**= 由实测形态推导（如图标语义、尺寸比例）、
> **【未验证】**= 本轮工具链 / 会话条件下无法判定 —— **一律写「未验证」，不用推测填空**。

> **合规声明**：本文只记录**窗口类名 / 物理尺寸 / 槽位形态 / 触发路径**等结构级行为事实。
> 不提取、不复用搜狗二进制、资源、词库、`.cupf` 皮肤包与专有文案；截图为**行为证据**，
> 不进入产品构建。全程**未连接** `evidence\urls-hosts.txt` 内任何主机。

---

## 0. 采集方法与工具链限制

### 0.1 手段

| 手段 | 用途 | 备注 |
|---|---|---|
| `EnumWindows` + `GetWindowRect` | 窗口类名 / 可见性 / 物理尺寸 / 位置 | 主力 |
| `EnumChildWindows` | 判定是否自绘（子窗口计数） | 主力 |
| `PrintWindow` | 窗口位图 | 本窗口**渲染正确**（见 §0.3） |
| GDI 屏幕区域截图 | 真实画面 | 外观证据双保险 |
| `keybd_event` | 向活动输入法注入按键 | 已验证能触发搜狗组合 |
| `mouse_event` | 物理点击 | 已验证能命中自绘按钮 |

采集脚本（本目录，可复跑）：`probe-ime-host.ps1`（IME 宿主）、`probe-capture.ps1`（产出 `≡` 命中那次）。

### 0.2 REA 的边界（本轮工具链固有限制）

REA 6.8.0 已注册为 MCP（`rea mcp doctor` → `healthy=true`，229/229 tools，catalog digest 双向一致），
并在搜狗 `UIPlugin.dll` 上产出**静态 PE 证据**：`evidence\rea-uiplugin-resources.json`
（VERSION 资源 language=2052 zh-CN / 732 B；MANIFEST language=1033）。

但 **REA 的运行时原生 UI 家族在 Windows 上不可用**，实测原始报错见
`evidence\quickpanel\log-rea-native-ui-unavailable.txt`：

```
code: capability_unavailable
category: unsupported_provider
provider_id: "composite:native-macos+rea-artifact-graph+rea-dotnet-static"
reason: Native macOS utilities require macOS.
```

⇒ `observe-native-ui` / `capture-native-ui-scenario` / `capture-process` /
`trace-native-ui-action` / `observe-native-calls` 全部不可用。**不要再重复试探这条线。**

因此本文件的**运行时事实全部来自自建 Win32 观察链**，**静态侧事实来自 REA**，两者在文中分别标注。

### 0.3 两个会读出假数据的坑

1. **尺寸口径**：DPI 未感知进程读到的是**逻辑**尺寸（状态栏 235×44、候选条 475×81），
   DPI 感知进程读到的是**物理**尺寸（353×66、713×122），比值同为 **1.5**（@150%）。
   **本文一律记物理像素**（采集脚本已先 `SetProcessDpiAwarenessContext(-4)`）。
2. **`GetWindowRect` 不等于外观证据**：`SoPY_Status` 的 `PrintWindow` 输出**渲染正确**
   （353×66，见 `sogou-status-printwin.png`），但**屏幕截图仍是外观的唯一真像**
   （窗口带阴影与圆角，屏幕区域比窗口 rect 更大：`753×466` 区域图 vs `353×66` 窗口）。
   本文的槽位顺序由 `PrintWindow` 原生分辨率图与屏幕放大图**双向互证**。

---

## 1. 搜狗 UI 窗口清单【已验证】

来源：`evidence\quickpanel\log-window-enum.txt`、`log-capture.txt`、`log-menu-hit.txt`。

| 类名 | 可见 | 物理尺寸 | rect (L,T,R,B) | 子窗口数 | 角色 |
|---|---|---|---|---|---|
| `SoPY_Status` | **是** | **353×66** | `[3369,1964,3722,2030]` | **0** | **状态栏 = 搜狗的「快捷设置栏」主体** |
| `SoPY_Comp` | **仅组合时可见** | **713×122** | `[696,378,1409,500]` | **0** | 候选条（贴光标弹出） |
| `CherryIMEWindowClass` | 点击 `≡` 后 | **480×974** | `[1382,381,1862,1355]` | 0 | `≡` 展开面板（pid 与宿主**同一进程**） |
| `SoPY_Hint` ×2 | 否 | 0×0 | — | — | 每进程常驻占位 |
| `SoPY_UI` | 否 | 0×0 | — | — | 每进程常驻占位 |
| `Sogou_TSF_UI` | 否 | 0×0 | — | — | 每进程常驻占位 |

要点：

- **状态栏与候选条是两个独立顶层窗口**，且**子窗口数均为 0 ⇒ 完全自绘**（DirectUI 风格），
  没有 HWND 子控件可挂 ⇒ 无法用「枚举子控件读标签」这条路取文案。
- **状态栏在候选窗隐藏时依然可见** ⇒ 搜狗的「常驻工具条」与「候选窗」是两套独立生命周期。
- 状态栏落点：`3369..3722` ⇒ 距屏右 **118 px**；`1964..2030` ⇒ 距屏底 **130 px**（任务栏上方）。
- `CherryIMEWindowClass` 的 pid 与 IME 宿主相同 ⇒ **不是独立 exe**，是宿主进程内创建的顶层窗。

---

## 2. 状态栏（`SoPY_Status`）槽位清单【已验证】

证据：`sogou-status-printwin.png`（原生 353×66）、`sogou-status-zoom.png`（3× 放大，1059×198）、
`sogou-status-region.png`（桌面区域 753×466）。

横向单行 **7 个可点项 + 1 条竖分隔线**（左→右）：

| # | 形态 | 语义 | 分级 |
|---|---|---|---|
| 1 | 橙色 `S` 品牌标 | 搜狗品牌 / 应用入口 | 形态【已验证】，语义【推断】 |
| — | 竖分隔线 | 视觉分组，**非可点项** | 【已验证】 |
| 2 | `中` | 中/英输入状态（与面板「英 / 英文输入法」开关形态对应） | 形态【已验证】，语义【推断】 |
| 3 | `。,` | 标点 / 全半角（与面板「半 / 全半角切换」形态对应） | 形态【已验证】，语义【推断】 |
| 4 | 🎤 麦克风 | 语音输入 | 形态【已验证】，语义【推断】 |
| 5 | ⌨ 软键盘 | 软键盘 | 形态【已验证】，语义【推断】 |
| 6 | 彩色渐变块（粉蓝渐变 + 橙点） | 皮肤 / 主题 | 形态【已验证】，语义【推断】 |
| 7 | 2×2 四宫格 | 更多（展开 `≡` 面板） | 形态【已验证】，语义【推断】 |

外观：**白色圆角胶囊底**（pill），图标深灰；槽 6 是唯一彩色项。

- 物理 353×66 ⇒ 平均每槽 ≈ 50 px（**推算**，非逐槽实测）。
- 槽 2/3 语义为**形态对应推断**：搜狗 `≡` 面板确有「英 / 英文输入法」「半 / 全半角切换」两个开关
  （§4），与状态栏这两个字符图标一一对应。**这不等于**已实测「点击后状态回显」。

---

## 3. 候选条（`SoPY_Comp`）结构【已验证】

证据：`sogou-candidate-fullscreen.png`（真实编辑框上下文，2560×1440 全屏）、
`sogou-comp-region.png`（853×262 区域）、`sogou-comp-after-commit.png`（793×202）。

结构（左→右）：

1. **输入串行**：`ni'hao'o` —— 撇号 `'` 是搜狗自动分词时插入的分隔符（**【推断】**，未追分词实现）。
2. **候选行**（5 项）：
   - `1 你哈儿` —— **橙色 = 当前选中**
   - `2 你哈偶`
   - `3 你哈额👓` —— 第 3 项带 👓 角标（用户词频 / 用户词标记，**【推断】**）
   - `4 你哈`
   - `5 你`
3. **分隔线**
4. **`⌄`** —— 展开更多候选（**效果未验证**，见 §6-8）
5. **`≡`** —— 菜单（**已验证可点开**，见 §4 / §5）

- 候选条**贴光标弹出**（本次光标位于编辑框内，候选条左上角即 `[696,378]`）。
- **宽度随候选内容变化**：5 候选时 713 px；内容更多时实测 **842 px**
  ⇒ **内容驱动宽度**，非固定宽度（**【推断】**）。
- **子窗口数 0** ⇒ 候选条同样完全自绘。

---

## 4. `≡` 面板（`CherryIMEWindowClass`）内容清单【已验证】

证据：`sogou-menu-panel.png`（580×1074 区域截图，含窗口阴影）、`log-menu-hit.txt`。

**物理尺寸 480×974**，pid 与 IME 宿主相同。面板自上而下：

**① 账号区**：头像 + 用户名 `winerking`；右侧灰字 `今日输入277字`。

**② 快捷开关区**（4 个并列开关，**开关下各带一行小字说明**）：

| 图标 | 说明文字 |
|---|---|
| `简` | 简繁切换 |
| `半` | 全半角切换 |
| `英` | 英文输入法 |
| `👁` | 隐藏状态栏 |

**③ 功能列表**（10 项，组间有横分隔线）：

`语音输入` / `符号大全` / `软键盘` / `图片表情` / `更多输入 ›` / ─── / `皮肤商城` / ─── / `工具箱` / ─── / `定制状态栏` / `帮助 ›` / `全局设置`

（`›` 表示有二级页；**二级页内容未观察**，见 §6-9。）

**④ 结构判读**：面板是「**账号区 + 4 开关 + 10 项列表**」三段式；
**没有任何一项是纯文字标签** —— 开关区用「图标字（简 / 半 / 英 / 👁）+ 下方小字说明」的组合。
这是本轮**最值得借鉴的排版手法**（详见 gap-matrix 第 4 行）。

---

## 5. 交互路径证据【已验证】

| 动作 | 结果 | 证据 |
|---|---|---|
| 候选条 `≡` 点击（comp 右缘 −50 / 顶 +71 = 1359,449） | **弹出 `CherryIMEWindowClass` 480×974**（`new_count=1`） | `log-menu-hit.txt` |
| 候选条 `⌄` 位点点击（1276,449 等 6 个偏移扫描） | 无新窗口（**点位未命中**，**不等于**「无反应」） | `log-menu-sweep-miss.txt` |
| 缓冲区未清空时点击 | 候选条宽 842（≠713），固定偏移失效 | `log-menu-sweep-miss.txt` vs `log-menu-hit.txt` |
| `SoPY_Status` 右键 | **未观察到弹出菜单**（本轮未成功触发） | — |

> **方法论要点**：必须先 `Ctrl+A` + `Delete` 清空缓冲区，候选条宽度才稳定回 713 px，
> 此时 `R−50` 才是 `≡` 的稳定命中点。缓冲区脏 ⇒ 宽度变化 ⇒ 固定偏移失效。
> 合成点击**确实对搜狗候选条生效**（命中 `≡` 立即弹面板）。

---

## 6. 未验证项（明确列出，禁止推测填充）

| # | 未验证项 | 为什么没验成 |
|---|---|---|
| 1 | **状态栏右键菜单全项** | 本轮未成功触发右键菜单（无弹出窗口被记录） |
| 2 | **独立的「快捷设置 / 属性配置」分页面板** | 只观察到 `≡` 面板；是否另有分页面板未确认 |
| 3 | **各槽位点击后「是否即时生效 / 是否需重启 / 开关态是否回显」** | 逐项点击未执行 |
| 4 | **DPI 100% / 200% 下的尺寸与清晰度** | 改系统缩放需注销重登，会话内不可完成 |
| 5 | **多屏 / 拔插后的落点** | 本机单屏，无设备可测 |
| 6 | **状态栏「常驻 vs 临时出现」的完整判定** | 只观察到「候选窗隐藏时状态栏仍可见」这一条；拖拽落点持久化未验 |
| 7 | **状态栏是否可拖拽** | 未做拖拽实验 |
| 8 | **`⌄` 展开更多候选的实际效果** | 点击点位未命中 |
| 9 | **面板二级页内容**（更多输入 / 帮助 / 定制状态栏 / 全局设置） | 未点击进入 |
| 10 | **「今日输入277字」是本地统计还是云端统计** | 需断网对照或抓包；**本轮禁止联网核实** |
| 11 | **「👓」角标的准确语义**（用户词频 / 用户词标记 / 其它） | 仅有形态观察 |

---

## 7. 附：一条对「可访问性」的反面观察【已验证】

- `SoPY_Status` / `SoPY_Comp` / `CherryIMEWindowClass` **子窗口数均为 0**。
- UIA 枚举顶层窗口，**只拿到 DingTalk 的 `MSCTFIME Composition` 组合窗**，
  **搜狗的窗口一个 AX 节点都没暴露**（`log-uia-empty.txt`）。
  ⇒ 搜狗这条 UI **完全不可被 UI 自动化 / 读屏软件访问**。
  ⇒ 对我们的含义（**反向借鉴**：不抄这个缺陷）见 gap-matrix 第 8 行。

---

## 8. 证据文件索引

| 文件（相对 `evidence\`） | 内容 | 来源 |
|---|---|---|
| `quickpanel\sogou-status-printwin.png` | 状态栏原生分辨率 353×66（**槽位顺序以此图 + 放大图互证**） | PrintWindow |
| `quickpanel\sogou-status-zoom.png` | 状态栏 3× 放大 1059×198 | 屏幕区域截图 |
| `quickpanel\sogou-status-region.png` | 状态栏在桌面上的位置 753×466 | 屏幕区域截图 |
| `quickpanel\sogou-candidate-fullscreen.png` | 候选条在真实编辑框上的上下文 2560×1440 | 屏幕全屏截图 |
| `quickpanel\sogou-comp-region.png` | 候选条区域 853×262 | 屏幕区域截图 |
| `quickpanel\sogou-comp-after-commit.png` | 提交猜测后的候选条 793×202 | 屏幕区域截图 |
| `quickpanel\sogou-hamburger-region.png` | `≡` 按钮区域 340×150 | 屏幕区域截图 |
| `quickpanel\sogou-menu-panel.png` | **`≡` 面板全貌 580×1074** | 屏幕区域截图 |
| `quickpanel\log-window-enum.txt` | 窗口枚举基线（类名 / 尺寸 / 子窗口数） | probe4 |
| `quickpanel\log-capture.txt` | 采集 + 打字 + 状态栏/候选条测量 | probe6 |
| `quickpanel\log-menu-sweep-miss.txt` | `⌄` / `≡` 偏移扫描（未命中） | probe7 |
| `quickpanel\log-menu-hit.txt` | **`≡` 命中 → `CherryIMEWindowClass` 弹出** | probe8 |
| `quickpanel\log-rea-native-ui-unavailable.txt` | REA 原生 UI 在 Windows 不可用的原始报错 | REA CLI |
| `quickpanel\log-uia-empty.txt` | UIA 顶层枚举（搜狗窗口 0 节点） | UIA |
| `quickpanel\probe-ime-host.ps1` | IME 宿主（WinForms 编辑器） | 自建 |
| `quickpanel\probe-capture.ps1` | 采集 / 点击脚本（产出 `≡` 命中那次） | 自建 |
| `rea-uiplugin-resources.json` | REA 静态 PE 资源（`UIPlugin.dll` VERSION / MANIFEST） | REA CLI |

**复现要点**

```powershell
# 1) 起 IME 宿主（搜狗须为活动输入法）
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File evidence\quickpanel\probe-ime-host.ps1
# 2) 采集（物理点击 + 截图 + 轮询）
pwsh -NoProfile -File evidence\quickpanel\probe-capture.ps1
```

---

## 9. 与本项目的衔接

本文件只提供**行为规格参考**，不构成改造结论。
逐条「借鉴 / 改造 / 不抄」判定、每行的我方源码位置与差异化实现，
见 `.specify\specs\099-quickpanel-sogou-benchmark\gap-matrix.md`。
