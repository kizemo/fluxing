# Handoff · spec 099 快捷设置栏 · P0-6 真机 DPI 收口 + 门禁全绿（2026-11-05）

> 上一棒：`handoff-2026-11-05-fluxing-spec099-quickpanel-p0-4-p0-5-p0-6.md`
> 本棒新增的唯一实质结论：**P0-6 的 150% DPI 项已用真机实测收口，且实测纠正了上一棒的一处环境误判。**

---

## 1. 背景与目标（不变）

spec 099「参考逆向搜狗输入法优化快捷设置栏」7 个 P0 片中：
P0-3 / P0-4 / P0-5 / P0-7 的代码部分与门禁已完成并推送（`3f35083` + 收尾提交 `bfb65eb`）。
本棒目标：把 **P0-6 真机 DPI 复验** 推到可验收；把 **P0-1 / P0-2** 收敛成「用户在场即可执行的最小步骤」；
并复核基线（单测 + 门禁）确实绿。

---

## 2. 本棒实际完成

### 2.1 基线复核（已验证）

| 项 | 结果 | 证据 |
|---|---|---|
| 单测 `release\TestQuickPanelDialog.exe` | **190 / 190 通过，exit=0** | 上棒实跑；本棒未改动生产代码，未重跑 |
| 门禁 `run-all.ps1`（L0/L1/L3/回归，跳过 L5） | **steps=5 passed=4 failed=0 skipped=1** | `docs/archive/evidence/spec099-gate-run-2026-11-05.txt` |
| ├ L0 | checks=13 pass=13 fail=0 | 同上 |
| ├ L1 快捷栏静态门禁 | checks=15 pass=15 fail=0 | 同上 |
| ├ L3 变异自检 | mutations=7 **killed=7** survived=0 invalid=0 | 同上 |
| ├ 回归 fast | cases=1 pass=1 | 同上 |
| └ L5 sandbox-gate | **SKIP**（按 `-SkipSandboxGate`） | 沙箱缺 `D:` 盘 / 外网，断言固定红，属环境能力项，**不得改 SKIP 刷绿** |

### 2.2 P0-6 真机 DPI 实测（已验证 · 本棒核心产出）

做法：**不装包**。直接从源码构建 `output\Win32\WeaselServer.exe` 起进程（启动前已确认无任何
`WeaselServer` 在跑），向 `WeaselIPCWindow_1.0` 窗口 `PostMessage(WM_COMMAND, wParam=40020)`
（= `ID_WEASELTRAY_QUICK_PANEL_SHOW`）唤出面板，再 `GetDpiForWindow` + `GetWindowRect` 读数，
截面板区域图，`PostMessage(40021)` 隐藏，`Stop-Process` 收尾。全程只读，未改任何持久状态。

实测结果：

```
WeaselIPCWindow_1.0   hwnd 0x48040A   PostMessage(WM_COMMAND, 40020) => ok=True
FluxingQuickPanel_v3  hwnd 0x2E510C8
GetDpiForWindow       = 144
GetWindowRect         = (1450, 852, 1818, 924)
物理尺寸              = 368 x 72
```

与 `include\FluxingPanelDpi.h` 的 `(logical*dpi+48)/96` 就近舍入断言 **368x72 逐像素一致**
⇒ P0-6 主项收口，**不需要**登记 `DEFECTS.yaml` 新缺陷。

防「窗口在但没画」的假绿：面板截图 368x72 / 6807 B，隔像素采样去重 **362 种颜色** ⇒ 确有绘制。

### 2.3 纠正上一棒的一处环境误判（重要）

上一棒 handoff 写「当前 100% DPI、虚拟屏 2560x1440」——**是错的**。

- 真实：屏幕 **3840x2160**，系统缩放 **150%**。
- `2560x1440` 是 **DPI 未感知进程看到的虚拟化逻辑分辨率**（3840/1.5=2560，2160/1.5=1440），
  不是真实缩放。
- 三条独立来源互证：`GetDeviceCaps(LOGPIXELSX/LOGPIXELSY)=144`、
  `GetDpiForMonitor(MDT_EFFECTIVE_DPI)=144x144 hr=0`、
  `HKCU:\Control Panel\Desktop` 的 `LogPixels=144` + `Win8DpiScaling=1`。

> 教训：判系统 DPI 不能读 `SystemInformation.VirtualScreen`（那受调用进程 DPI 感知级别
> 影响，会被虚拟化成逻辑坐标）。要读 `GetDeviceCaps(desktop DC, LOGPIXELSX)` 或
> `GetDpiForMonitor`。已同步进 `.specify\memory\lessons-learned.md`（见 §6）。

### 2.4 已归档文件

```
docs/archive/evidence/
  spec099-p0-6-real-machine-dpi.txt        ← 结论 + 交叉验证 + 未覆盖项（主文档）
  spec099-p0-6-measure-raw.txt             ← 探针原始输出
  spec099-p0-6-panel-150dpi-368x72.png     ← 面板区域截图（368x72）
  spec099-p0-6-measure-panel.ps1           ← 可复跑探针（被归档的副本）
  spec099-gate-run-2026-11-05.txt          ← 本棒门禁完整日志
tmp/spec099-probe/
  measure-panel.ps1                        ← 探针工作副本
  probe-windows.ps1                        ← 顶层窗口/UIA 枚举探针（上一棒建）
  desktop-with-panel.png                   ← 全屏上下文截图
  windows.json / measure-panel.txt         ← 原始数据
```

---

## 3. 仍未完成（都需要用户在场，不是我能替的）

### 3.1 P0-1 搜狗快捷设置栏黑盒行为采集

现状：搜狗进程在跑（`SGTool` / `SogouCloud` / `SogouImeBroker`），但**状态栏窗口不存在**——
顶层窗口枚举里搜狗相关窗口全部 `Visible=False`，说明搜狗当前**不是活动输入法**。
要把搜狗切成活动输入法才能看到状态栏与属性面板，**这会打断用户正在用的输入法**，不能替用户做。

用户在场时的最小步骤：
1. 切到搜狗输入法（Win+Space 或点任务栏），让状态栏出现在屏幕边缘；
2. 我（新会话）跑 `tmp\spec099-probe\probe-windows.ps1 -DumpUia` 枚举状态栏窗口 + UIA 树；
3. 右键状态栏开属性面板，我截屏 + 逐项登记名称/图标含义；
4. 切中/英、全/半角，各截一次，记录回显变化（这直接决定 P0-4 的状态区规格是否还要调）。

合规红线（spec 099）：**只做只读静态证据 + 黑盒观察**；不提取/复用搜狗二进制、资源、词库、
`.cupf` 皮肤与专有文案；**永不连接** `docs\research\sogou-reverse-2026-11\evidence\urls-hosts.txt`
内的 185 个主机；每条借鉴须写「来源证据 + 差异化实现」。

### 3.2 P0-2 差距矩阵

依赖 P0-1 的采集结果。三列表：搜狗行为 × 我方现状（源码位置） × 设计意图
（`docs\design\quickpanel-v{3,4,5}`）。P0-1 一完成即可写，不需要再等任何人。

### 3.3 P0-6 剩余两档

- 100% ⇒ 期望 245x48；200% ⇒ 期望 490x96。改系统缩放**必须注销重登**，会话内无法完成。
- 多屏 / 拔插后落点：**本机单屏**，无设备可测。

### 3.4 P0-3 剩余一条：装机验收

`scripts\install\` 已有 `verify-installed.ps1` / `silent-install.ps1` / `acceptance-check.ps1`，
**本轮未跑**。挡点：已安装版 `D:\Program Files\fluxing\weasel\WeaselServer.exe`
FileVersion `1.1.9.0`，mtime 2026-11-03，而 `env.bat` 的版本也是 `1.1.9.0` ⇒ **同版本不同内容**，
安装器升级路径会挡（lessons **L107**）。要装机验必须先 **bump 版本**（如 1.2.0）再打包安装器。

### 3.5 P0-5 真机键盘行为

`CaptureKeyboardFocus()` 跨线程 `AttachThreadInput`、`WS_EX_NOACTIVATE` 下面板收不到按键需先点一下——
必须在装机产物上人工确认，无头断言只能钉住逻辑。

---

## 4. 下阶段计划（按优先级）

1. **P0-1 采集 → P0-2 矩阵**：需要用户先切搜狗。一次会话内可做完两片。
2. **bump 版本 1.2.0 → 打包安装器 → 静默安装 → `verify-installed.ps1`**：
   这一步同时收掉 P0-3 最后一条，并为 P0-5 人工点按、P0-6 剩余两档提供装机产物。
   注意 `AGENTS.md` §2.5 / lessons **L169**：静默安装必须 `/D=<path>` 显式、先清
   `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing`；**装机是部署动作不是测试**。
3. **P0-5 / P0-6 人工点按**：用户重登切 100% / 200% 各量一次。
4. 收尾：`CHANGELOG.md` 未发布段（P5）+ `lessons-learned.md`（BOM + CRLF）。

---

## 5. 环境事实与工具坑（本棒新踩到的）

1. **pwsh 7 路径变了**：上一棒记的 `C:\Program Files\PowerShell\7\pwsh.exe` **不存在**。
   实机可用路径：`$env:LOCALAPPDATA\Microsoft\WindowsApps\Microsoft.PowerShell_8wekyb3d8bbwe\pwsh.exe`
   （版本 **7.6.9**）。门禁宿主必须 pwsh 7（PS 5.1 会因 UTF-8 无 BOM + 中文报
   `ParserError: Missing closing '}'`，lessons L181）。
2. **`run-all.ps1` 没有 `-Level` 参数**。真实参数：`-Root` / `-SkipL0` / `-SkipL1` / `-SkipL3` /
   `-SkipCases` / `-SkipSandboxGate` / `-CaseTimeoutSec`。传错参数时脚本 exit=1 但会被
   后台任务包装成 `succeeded`（见下条）——**别把「后台任务 succeeded」当成「门禁通过」**。
3. **后台任务 `succeeded` 只代表 shell 退出码 0，不代表逻辑通过**。本棒第一次跑门禁时
   pwsh 路径不存在，日志只有一行 `不会被识别为 cmdlet`，任务状态却是 succeeded。
   判门禁结果只看 `GATE RESULT` / `RESULT` 行。
4. **`Remove-Item` 被硬策略拦**（无回收站的 PowerShell 删除）。清日志改用
   `Tee-Object -FilePath` 直接覆盖，或走 `mavis-trash`。
5. **判系统 DPI 别读 `VirtualScreen`**（见 §2.3）。
6. **起 server 不用先停计划任务**：本棒直接 `Start-Process` 就是在无 server 在跑的情况下成功的；
   但**先 `Get-Process WeaselServer` 确认空**再起，否则会连到旧进程（旧构建）上量错对象。
7. `output\Win32\` 里已有 `rime.dll` / `WeaselDeployer.exe` / `WinSparkle.dll` 等依赖，
   `WeaselServer.exe` 可直接在该目录起，无需安装。
8. 本会话 `bash` 工具实为 PowerShell；`cd` 到工作区后写相对路径会被安全策略整条拦掉
   （`write/delete targets cannot be verified safely after a command working directory change`）
   ⇒ **所有写/删目标一律用绝对路径**。

---

## 6. 已验证 / 推断 / 未验证（严格三分）

**已验证**
- 门禁 L0 13/13、L1 15/15、L3 变异 7/7 全杀、回归 fast 1/1；L5 按开关跳过（有完整日志）。
- 系统缩放 150%（三条独立来源互证）；屏幕 3840x2160。
- 150% 下面板真机物理尺寸 **368x72**，与代码断言逐像素一致；面板确有绘制（362 色）。
- 触发链路 `WM_COMMAND 40020 → 面板显示` 在真机可用。
- 已安装版仍是 2026-11-03 的 `1.1.9.0` 旧构建，不含 P0-3~P0-7。

**推断（未直接验证）**
- 100% / 200% 档真机像素应分别为 245x48 / 490x96（仅代码断言 + 本棒 150% 档佐证，未直测）。
- `verify-installed.ps1` 在当前同版本情况下大概率会因版本相同而无法确认升级成功（未跑，未证）。

**未验证**
- P0-1 搜狗状态栏/属性面板的任何行为（窗口不存在，需用户切输入法）。
- P0-2 差距矩阵（依赖 P0-1）。
- 多屏 / 拔插后落点（本机单屏）。
- P0-5 真机键盘可达性（跨线程焦点、`WS_EX_NOACTIVATE` 首次点击）。
- 装机产物上的任何验收（未装、未跑 `verify-installed.ps1`）。
- L5 sandbox-gate（环境缺能力，未跑）。

---

## 7. 纪律提醒（下一棒别踩）

- **不要**在 P0-1 采集时联网、不要抓取搜狗资源文件；只做黑盒观察 + 截图。
- **不要**在同一份 `env.bat` 版本号下重装（L107）。
- **不要**为了刷绿改 L5 的 SKIP。
- 单测改生产代码后必须重跑 `release\TestQuickPanelDialog.exe`（当前基线 190/190）。
- 提交前 `git status -sb` + `git diff --stat`；`git add` 逐路径；`m librime` 脏标记不得进提交。
- Conventional Commits + 模块前缀。