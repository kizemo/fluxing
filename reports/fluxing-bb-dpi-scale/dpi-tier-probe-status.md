# 批次 BB 后续 · 175%/200% 真机取证：现状与阻塞（状态外置）

- 记录时间：2026-11-20（本会话，root = `mvs_eda52139b4de4dc79444f38168f11bdd`）
- 目标：BB handoff §7 未闭环第 1 项 —— 账号页「换钥提示」`kIdKeyRotationNote = 1035` 在 **175%（168 dpi）/ 200%（192 dpi）** 下的真机像素证据（本机只有 150% 一档）
- 判据：沿用 BA 探针，每档须 `VERDICT=A1_NOT_CLIPPED`

## 1. 本轮实测（三处交叉，全部指向「未生效」）

| 读数 | 值 | 含义 |
|---|---|---|
| `GetDpiForMonitor(MDT_EFFECTIVE_DPI)` | **144** | 主显示器有效 DPI 仍是 150% |
| `GetDpiForWindow(Shell_TrayWnd)` | **144** | Explorer 侧也是 150%（非单进程假象） |
| `GetDpiForSystem()`（新起进程） | **144** | 系统 DPI 仍是 150% |
| `GetDeviceCaps(LOGPIXELSX)` | **144** | 同上 |
| `HKCU\Control Panel\Desktop\LogPixels` | **168**（本轮早先读数 **144**） | 175% 已被**写入** |
| `Win8DpiScaling` | **1**（本轮早先读数 **2**） | 同键被改写过 ⇒ 确有 DPI 设置动作发生 |
| `HKCU\...\WindowMetrics\AppliedDPI` | **144** | 当前会话**生效**的仍是 150% |
| `HKLM\...\Hardware Profiles\Current\Software\Fonts\LogPixels` | **144** | 登录期值未变 |
| `PerMonitorSettings\<mon>\DpiValue`（两条目） | **0 / 0** | 未写入任何 per-monitor 覆盖 ⇒ 按显示器的立即生效路径**没被走过** |

**结论：本机 175% 已被登记但未生效。** 因此「预设缩放即时生效、不用注销」这一判断在**本机被实测推翻**（该说法来自 `windows11.io` / `onewebcare` / `Windows Central`，描述的是「大多数机器」的一般行为，本机不属于）。

### 两条候选成因（未区分，需看设置页 UI 才能定）

1. **走的是「自定义缩放」**：自定义路径写 `LogPixels` 且要求注销；这与观测到的「`LogPixels` 变了、`DpiValue` 没变、`AppliedDPI` 没变」完全吻合。
2. **预设路径在本机也要求注销**：这台机器（Win11 26300，`Win8DpiScaling=1`）把预设值当成登录期应用。

判别方法（不用猜）：把「设置 → 系统 → 显示 → 缩放」下拉里的**预设 175%** 选项点一次（不要在下拉下方的「自定义缩放」百分比框里输入），然后看 `AppliedDPI` 是否变 168。

## 2. 已就绪资产

| 文件 | 作用 |
|---|---|
| `reports\fluxing-bb-dpi-scale\run-dpi-tier.ps1` | 单档一键跑完：桌面守卫 → **错档就拒绝**（`ABORT_WRONG_DPI`，退出码 3）→ 同版本覆盖安装以重启 `WeaselServer`（清旧版进程级 DPI 值缓存，实测 41 s）→ 校验新 pid + 版本 → 跑 BA 像素探针 → 落 `tier-summary.txt` / `probe-console.txt` / `report.json` |
| `reports\fluxing-bb-dpi-scale\selftest-guard\` | 守卫自检产物（`-ExpectedDpi 168` 在 150% 环境下必须 exit 3，已验） |
| `reports\fluxing-bb-dpi-scale\dpi-scale.ps1` | **作废留档**：程序化切档（DisplayConfig / `SPI_SETLOGICALDPIOVERRIDE` / 注册表直改）三条路全部走不通，见 §3 |
| `reports\fluxing-bb-dpi-scale\retest-spi-override.ps1` + `spi-override-retest.txt` | `SPI_SETLOGICALDPIOVERRIDE` 正确常量（`0x009F`）四档复测，`EXP1/EXP2_RESULT=NO_EFFECT`、还原后与 BEFORE 逐字相同 |

用法：
```powershell
pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-dpi-scale\run-dpi-tier.ps1' `
  -ExpectedDpi 168 -OutDir 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-dpi-scale\evidence-dpi-175'
```
退出码：0 探针已跑（看 `PROBE_EXIT` / `VERDICT` / `CLIENT_MATCHES_TIER` 行）/ 3 前置不成立（非 Default 桌面，或注册表 `AppliedDPI` 不等于 `ExpectedDpi`）/ 4 安装器缺失或 `INSTALL_EXIT != 0` / 5 服务没回来或版本不符 / 6 探针缺失或 `report.json` 无可用几何 / 7 **测量不属于本档**（几何读回 96 dpi 基准值 ⇒ DPI 虚拟化；或读回另一档尺寸 ⇒ 运行中服务里 dpi 缓存过期）。

**重启 `WeaselServer` 这一步只对 1.3.6 及更早版本必需**：这些版本的 `AccountUiDpi()` 把**值**缓存进 `static int cached`（缺陷 D，**本轮已修，见 §7**），且 `WM_DPICHANGED` 被显式吞掉（ADR 0068）⇒ 旧版不重启必然还观测到 150% 的尺寸。`WeaselServer.exe` 是 PPL，不能 kill；已证实的重启手段**只有同版本覆盖安装**。**含缺陷 D 修复的版本（1.3.7+）不再需要这一步**：账号窗每次打开都会重新取系统 DPI。

> ⚠️ **本条已作废（2026-11-20 21:5x 磁盘/进程实测更正）**：早先登记「缺陷 D 修复只在 `output\Win32\WeaselServer.exe`、未出包未装机」**不成立** —— `output\archives\fluxing-1.3.7-installer.exe`（42 975 921 B）**含**该修复且**已装机**（装机件 20:30:28 写入、服务进程 pid 77332 于 20:37:56 启动、`FileVersion=1.3.7.0`）。因此用 1.3.7 取证**不再需要**为缺陷 D 走覆盖安装重启；默认重装只在担心「已运行服务不感知新 DPI」时才必要。详见 §10。

## 3. 已排除的取证路径（别重试）

1. **DisplayConfig 私有 `DPI_SCALE`（`-3` / `-4`）**：本机返 `87 = ERROR_INVALID_PARAMETER`；SDK `10.0.26100.0\um\wingdi.h:3046-3052` 的 `DISPLAYCONFIG_DEVICE_INFO_TYPE` 只到 `SET_WCG_STATE = 17` + `FORCE_UINT32`，**无任何 DPI_SCALE 项**；两版 SDK 的 `um`/`shared` 均无 `displayconfig.h` ⇒ 第三方反推私有值，本机不支持。
2. **`SPI_SETLOGICALDPIOVERRIDE`（正确常量 `0x009F`）**：`WinUser.h:12633-12637` 的 `MAX/MIN_LOGICALDPIOVERRIDE = ±2` 档；四档全部实测 `ok=True` 但 `EFF` 恒 144（`v=+2` 另返 `err=1460 ERROR_TIMEOUT`）。
3. **注册表直改 `PerMonitorSettings\DpiValue`（-1 / -2 / +1）+ `WM_SETTINGCHANGE("Display")` 广播**：`EFF` 恒 144。
4. **注册表直改 `HKCU\Control Panel\Desktop\LogPixels`**：只注销/重登后生效。
5. **`WeaselServer.exe /quit`**：未验证；且停服后不会自动回来，不采用。

## 4. 未闭环（继承 BB §7 第 1 项）

- 175%（168 dpi）真机像素证据：**未闭环**（用户 2026-11-20 拍板「先不切，按未闭环收口」，不再要求任何物理动作 —— 口径见 §10.4）
- 200%（192 dpi）真机像素证据：**未闭环**（同上）
- 还原 150% 回归复跑：**不适用** —— 全程未离开 144 档；1.3.7 装机后 144 档已两次实测（§10.2），`PROBE_EXIT=0` / `clipped=False`
- **缺陷 D** —— **已修、已随 1.3.7 装机生效、已验证**（见 §7 与 §10），不是未闭环项
- 含缺陷 D 修复的安装包：**已出包并已装机 = 1.3.7**（`output\archives\fluxing-1.3.7-installer.exe`，42 975 921 B；pid 77332 / `FileVersion=1.3.7.0`）；**不需要 bump 1.3.8**
- BA 遗留探针瑕疵：`DT_WORDBREAK` / `DT_WORDBREAK|DT_EDITCONTROL` 在该探针返 `0×0`，**不得**读成「不需要高度」

## 5. 等价判据（若真机两档长期不可得）

`AccountScale(v, dpi) = MulDiv(v, dpi, 96)` 是纯函数，已把 168 / 192 加进 `test\TestFluxingCloud\TestFluxingCloud.cpp::TestAccountPageUiDpiScale`（144 已有反向齿：余量必须为 0）。预期值（基准 96dpi 表推导）：

| dpi | 客户区 | 1035 控件 | 单行高 | 两行需要 | 余量 |
|---|---|---|---|---|---|
| 144（已验） | 720×435 | 684×54 | 27 | 54 | 0 |
| 168（175%） | 840×508 | 798×63 | 27\* | 54 | 9 |
| 192（200%） | 960×580 | 912×72 | 36 | 72 | 0 |

\* 行高按 `AccountLineHeightPx` 的 96dpi 基准等比缩放推；**真机须以探针实测为准，不得用本表替代**。

## 6. 接力一句话（供下一会话）

读 `F:\soft\00selfmade\rime_claude\CLAUDE.md` 的交接指针 → BB handoff（`docs\archive\handoffs\handoff-2026-11-20-batch-bb-account-ui-dpi-scale.md`）→ **本文件**；工作入口是 `reports\fluxing-bb-dpi-scale\run-dpi-tier.ps1`（`-ExpectedDpi 168|192`），前置条件是 `AppliedDPI` 真的变成 168/192，否则脚本会以退出码 3 拒绝出证据。

## 7. 缺陷 D（本轮已修并验证）· `AccountUiDpi()` 缓存系统 DPI 值

**定性**：`CloudAccountDialog.cpp` 的 `AccountUiDpi()` 用 `static int cached = 0; if (cached > 0) return cached;` **永久缓存系统 DPI 值**，违反官方文档 `GetDpiForSystem` 页原文：「You should not cache the system DPI, but should use `GetDpiForSystem` whenever you need the system DPI value.」（Microsoft Learn，2024-02-22 版）。后果：用户改缩放后账号页仍按旧 DPI 排版，直到服务进程重启（而 `WeaselServer.exe` 是 PPL，不能 kill）。

**修法**（结构上不可能再缓存值）：

| 位置 | 改动 |
|---|---|
| `WeaselServer\Cloud\CloudAccountUi.h` | 新增纯函数 `inline int AccountNormalizeSystemDpi(long raw)`：合法区间 `[48, 960]`，越界回退 `kAccountBaseDpi = 96` |
| `WeaselServer\Cloud\CloudAccountDialog.cpp` → `AccountUiDpi()` | 删除值缓存；`static GetDpiForSystemFn fn` + `static bool fn_resolved` **只缓存函数指针**；探测用 `long raw`，`raw < 48` 回退 `GetDeviceCaps(LOGPIXELSX)`；末行 `const int dpi = AccountNormalizeSystemDpi(raw);` |
| 同函数交叉校验 | 字体度量反推 DPI 的告警加 `static bool mismatch_logged` **只记一次**（保留既有 `delta * 8 > dpi` 判据） |

**新增判据 9**（`test\TestFluxingCloud\TestFluxingCloud.cpp::TestAccountPageUiDpiScale` 末尾）：`AccountNormalizeSystemDpi(0/-1/47/961/4000) == 96`；`(48/120/144/168/192/240/288/480/960)` 原样透传。注释里已写明「**不缓存值**这一属性无法用单测直接证明（同一进程内系统 DPI 恒定）⇒ 生产侧改成结构上不存在值缓存，单测只覆盖可测分支」，避免假覆盖。

**门禁实测**：

| 项 | 结果 | 证据 |
|---|---|---|
| 云单测 | **`PASS=586 FAIL=0` / `VERDICT: ALL PASS`**，回环回归 11-0 | `reports\fluxing-bb-dpi-scale\cloudtest-defect-d.txt` |
| 计数自洽 | AX/AY 基线 577 + BB 判据 1–8 = 585 + 本轮判据 9 = **586** | 同上 |
| 产品构建 | `build.bat weasel` **exit 0 / 0 警告 0 错误** / 25.79 s | `reports\fluxing-bb-dpi-scale\build-weasel-defect-d.log` |
| 防假绿 | `msbuild\Release\Win32\WeaselServer\CloudAccountDialog.obj` mtime 晚于源码 mtime ⇒ 确实重编译 | — |
| clang-format 定点 | `CloudAccountUi.h:151-162` exit 0、`CloudAccountDialog.cpp:1150-1196` exit 0、判据 9 区间格式化一次后 exit 0 | 本文件 §7 记录 |

**既有噪声（非本轮引入）**：`TestFluxingCloud.cpp` 有 3 条 `warning C4996 'fopen'`（第 3164/3175/3215 行），距本轮改动的 2966-2988 行很远，属历史遗留。

## 8. 新工具坑（本轮收尾实测）：Agent shell 侧读不到真实 DPI

**现象**：同一台机器，此前探针读到 `GetDpiForSystem=144 / GetDeviceCaps(LOGPIXELSX)=144 / GetDpiForWindow(Shell_TrayWnd)=144`；本次在 shell 里同法读取却**一律得到 96**，`SM_CXSCREEN=2560`（= 物理宽，看着像 100%），一度疑似缩放被自己的实验改坏。

**根因**：`SetProcessDpiAwarenessContext(-4)` 在 Agent shell 进程里返回 **`False`** —— 该进程的 DPI awareness 在启动时已被继承固定（access denied），事后无法更改 ⇒ 无感知进程的 `GetDpiForSystem` / `GetDpiForWindow` / `GetDeviceCaps(LOGPIXELSX)` 全部被**虚拟化成 96**（固定回退值），`GetSystemMetrics` 也返回虚拟化尺寸。`EnumDisplaySettingsW` 在本 shell 里另返 `0x0`（DEVMODE 结构体字符串字段布局未对齐），同样不可用作物理分辨率判据。

**判据口径（固化）**：
- **禁止**用 Agent shell 侧的实时 DPI 读数判定「当前系统缩放是多少」。它只在**该进程确实感知**时才有意义，而 shell 无法自证感知（须同时打印 `SetProcessDpiAwarenessContext` 返回值）。
- 可用的两条权威来源：① `HKCU\Control Panel\Desktop\WindowMetrics\AppliedDPI`（**登录时写入**，本机 = 144 ⇒ 会话起于 150%，注册表后续改动不影响它）；② 由**自身带 PerMonitorV2 manifest 的进程**（`WeaselServer.exe`）在真机探针里报出的 dpi 字段。
- `HKCU\Control Panel\Desktop\LogPixels=168` 与 `AppliedDPI=144` 的差异属「**已登记、未生效**」，**不是**「已经切到 175%」。同理 `PerMonitorSettings\*\DpiValue=0` 两条均在。
- 本机当前结论：会话仍为 **150%（144 dpi）**，未被本轮实验改动；175% 需注销/登录才生效。

## 9. 测量侧自证（本轮）：rect 不是虚拟化值 + 两个 guard 已修并实测

### 9.1 「非感知 ⇒ rect 被虚拟化 ⇒ 探针失去判别力」这条假设**被实测推翻**

探针 `probe-dpi-awareness.ps1`（9 228 B，纯 ASCII）在**同一台机器同一时刻**跑两个宿主，产物
`awareness-pwsh7.txt` / `awareness-ps51.txt`，两宿主均 `EXIT=0`：

| 读数 | pwsh 7 | powershell 5.1 |
|---|---|---|
| `SetProcessDpiAwarenessContext(-4)` | **False**（进程启动时已固定） | **True** |
| `THREAD_AWARENESS`（0=unaware／1=system／2=per-monitor） | **1** | **2** |
| `GetDpiForSystem` / `LOGPIXELSX` | 96 / 96 | 96 / 96 |
| `ACCT_CLIENT`（账号窗客户区） | **720×435** | **720×435** |
| `id=1022` / `id=1035` / `id=1006` / `id=1010` | 684×27 / 684×54 / 684×60 / 150×42 | **逐字相同** |
| `ACCT_RECT` size | 742×491 | 742×491（原点 38,38 vs 76,76） |

**结论（判据口径）**：awareness 1 与 2 给出**逐字相同**的几何，且该组数值等于 **144 dpi 设计表**
（客户区 720×435、1035 控件 684×54、单行 27），**不是** 96 空间的虚拟化值（480×290、456×36）
⇒ **`GetClientRect`/`GetWindowRect` 读的是真物理像素**，现有宿主可直接做 175%/200% 取证。
`GetDpiForSystem` 恒 96 只说明**该宿主自己不感知**，与 rect 是否真实无关 —— 这正是 guard 2 不能拿它判档的原因。

**未定性观察（不是缺陷，勿据此改码）**：`GetDpiForWindow` 对 `FluxingCloudAccountDialog_v1`、
`FluxingQuickPanel_v3` 返 **96**，对 `WeaselIPCWindow_1.0`／`ATL:xxxx`／`TrayIconClass`／GDI+ 返 **144**，两宿主一致；
但该账号窗客户区确实按 144 dpi 缩放 ⇒ 该 leaf 不可用作档位判据（`report.json` 的 `dpi` 叶子同理，BA 已登记）。

### 9.2 `run-dpi-tier.ps1` 修了两处守卫（原版在 168/192 一定假拒）

- **guard 2 判源改为注册表 `AppliedDPI`**（原实现拿 `GetDpiForSystem() -ne ExpectedDpi` 判 ⇒ 该值恒 96 ⇒ **必然** `ABORT_WRONG_DPI` 退出码 3，任何人切到 175% 也会被拒）。`GetDpiForSystem`/`LOGPIXELSX`/`LogPixels` 降级为信息行；`AppliedDPI` 与宿主读数不一致时打 `WARN_HOST_DPI`（宿主不感知，非档位问题）。
- **新增 guard 3：几何本档校验**。以 96 dpi 基准 480×290 用 `MidpointRounding::AwayFromZero` 缩放到目标档（96⇒480×290、144⇒720×435、168⇒840×508、192⇒960×580），与 `report.json` 的 `dialog_client` 实测比对：等于基准宽 ⇒ `ABORT_DPI_VIRTUALIZED`、等于别档宽 ⇒ `ABORT_STALE_DPI`（点名匹配到的档），两者均 **退出码 7**；取不到几何 ⇒ 退出码 6。头部 exit-code 文档已同步 6/7 含义。

### 9.3 守卫实测（四个方向都跑了，不是纸面推断）

| 用例 | 命令要点 | 结果 | 证据目录 |
|---|---|---|---|
| 假拒已修（拒绝路径） | `-ExpectedDpi 168`（AppliedDPI=144） | `ABORT_WRONG_DPI applieddpi=144 expected=168` / **EXIT=3** | `selfcheck-reject-168\` |
| 整链接受路径（144 档） | `-ExpectedDpi 144 -SkipRestart` | `CLIENT=720x435 EXPECTED=720x435` → `CLIENT_MATCHES_TIER` / **EXIT=0**；探针 `note.chars=37`、`box_h=54`、`line_h=18`、`clipped=False` | `selfcheck-144\` |
| 有牙 1：虚拟化 rect | stub 探针喂 `0,0,480,290` | `ABORT_DPI_VIRTUALIZED` / **EXIT=7** | `selfcheck-teeth-virt\` |
| 有牙 2：stale 别档 rect | stub 探针喂 `0,0,840,508` | `ABORT_STALE_DPI ... matches 168dpi, not 144dpi` / **EXIT=7** | `selfcheck-teeth-stale\` |

stub 探针 `selfcheck-stub-probe.ps1` 只写合成 `report.json`（几何经 `STUB_CLIENT` 环境变量传入，因 `run-dpi-tier.ps1` 只转发 `-OutDir`/`-ControlIdOfKnownSingleLine`），**不用于取证**。

### 9.4 仍未闭环

> **后记（收口）**：2026-11-20 用户拍板**不切档** ⇒ 175%/200% 两档像素证据**登记未闭环**，最终口径以 §10.4 为准；本节下面的命令只在将来重新发起补证时使用。

`AppliedDPI` 仍 = **144**（本节写作时实测）⇒ 175%/200% 两档真机像素证据**仍不可得**，须用户点**预设 175%**
（不要用自定义输入框）；改完若 `AppliedDPI` 不变 ⇒ 该机需注销/重登（会中断本会话）。脚本侧已就绪，切档后直接：

```powershell
pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-dpi-scale\run-dpi-tier.ps1' `
  -ExpectedDpi 168 -OutDir 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-dpi-scale\evidence-dpi-175'
```
## 10. 收口核实（2026-11-20 21:5x）：1.3.7 已出包、已装机、144 档真机证据已闭环

本节纠正一条**过期认知**：此前的登记说「缺陷 D 修复只在 `output\Win32\WeaselServer.exe`，未出包未装机」。
按磁盘与进程实测，**不成立** —— 1.3.7 早已出包并装到位，而且装机后立刻取过一次真机证据。

### 10.1 时间线（全部为磁盘/进程实测，非推断）

| 时刻 | 事实 | 证据 |
|---|---|---|
| 20:30:28 | 装机件写入 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` mtime |
| 20:37:17 | 安装器产出 | `output\archives\fluxing-1.3.7-installer.exe` mtime，42,975,921 B |
| 20:37:56 | 服务进程启动 | pid 77332，`FileVersion=1.3.7.0` |
| 20:38:04 | 装机后真机探针 | `evidence-after-1.3.7\probe-report-after-137.json` |

### 10.2 144 档（当前档）真机证据：**两份独立**

1. `evidence-after-1.3.7\probe-report-after-137.json` —— 装机后首次真实取证：
   `dpi=144` / `dialog_client=0,0,720,435` / `id=1035 w=684 h=54` / `verdict=A1_NOT_CLIPPED` / `logged_in=True`
2. `selfcheck-144`（本轮守卫接受路径整链）：`CLIENT=720x435 EXPECTED=720x435 tier=144dpi` + `CLIENT_MATCHES_TIER`，
   探针 `note.chars=37 / box_w=684 / box_h=54 / line_h=18 / lines=1 / deficit_px=-36 / clipped=False`

**等比自证**（这是缺陷 C/D 修好的算术铁证，与实现细节解耦）：

- 客户区 `480×290 × 1.5 = 720×435`
- 提示控件 `456×36 × 1.5 = 684×54`（BA 在 1.3.6 实测的未缩放字面量正是 456×36）
- 若仍是未缩放字面量，144 档应读出 `456×36` 与 `480×290`；实测全部乘 1.5 ⇒ **整表等比缩放已落码并已装机**

### 10.3 收口判据口径（固化，防下一轮误判）

- `output\Win32\WeaselServer.exe`（21:30:35，sha16 `0F0C0D46C4AAF3B5`）与装机件（20:30:28，sha16 `38316DCEFC8C502E`）
  **尺寸完全相同**（2,785,280 B）、**版本相同**（1.3.7.0）、**哈希不同** ⇒ 这是 MSVC 重建带来的 PE 头/调试信息差异，
  **不得**据此判「装机件缺修复」。判「装的是哪份代码」只认两条：① 进程 `FileVersion`；② 功能读数（客户区/控件几何）。
- 安装布局是 `D:\Program Files\fluxing\weasel\*.exe`（**根目录 exe 数为 0**），NSIS `/D=D:\Program Files\fluxing` 依旧正确，
  `run-dpi-tier.ps1` 默认 `-InstallDir` 不需要改；`HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir` 与之逐字相同。

### 10.4 175% / 200% 两档：**用户拍板不切档 ⇒ 登记未闭环**

2026-11-20 用户明确指令「**先不切，按未闭环收口**」⇒ 本批**不**做 168/192 两档真机取证。
该两档从此状态为**未闭环**（不是「待办」、不是「阻塞」），也不再要求任何物理动作。

- **收口时实测档位**：`AppliedDPI=144`、`LogPixels=168`、`Win8DpiScaling=1`、`PerMonitorSettings` 两条目 `DpiValue=0`
  ⇒ 仍是 **150%**；175% 属**已登记未生效**，**不得**当作已切档（这是本批反复踩的坑）。
- **已具备的替代判据**：整表等比缩放律由云单测覆盖（**586-0**，判据 1–9）+ **144 档真机像素实测两份独立**（§10.2）
  + 等比自证（`480×290×1.5=720×435`、`456×36×1.5=684×54`）⇒ 几何等价性已有结构性证据，
  缺的仅为另两档的**像素级**确认。
- **若将来重新发起**：须先让 `AppliedDPI` 真变成 168/192（点**预设**缩放档位，不要用「自定义缩放」输入框；
  改完不变 ⇒ 该机需注销/重登），再照 §9.4 命令跑。脚本侧 guard 2/3 已就绪且四向实测通过（`3 / 0 / 7 / 7`，§9.3），
  因装机件已是 1.3.7.0，可加 `-SkipRestart` 免去 41 s 覆盖安装；若担心已运行服务不感知新 DPI，则用默认重装
  （重装会重启 PPL 服务，必定读到新档）。
