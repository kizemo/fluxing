# 批次 BB · 账号页「换钥提示」DPI 感知布局（修缺陷 C：>150% 缩放裁切）

- **批次**：BB
- **日期**：2026-11-20
- **分支**：`feat/100-cloud-sync`（基线 HEAD `3dc4eaf`）
- **上游交接**：[批次 BA · 真机实测账号页换钥提示折行](handoff-2026-11-20-batch-ba-a1-note-wrap-measured.md) §6 未闭环清单**第 1 项**
- **本批性质**：**代码实现 + 出包装机批次**（未改 `librime/`、未改 IPC 协议消息类型、未新增第三方依赖）
- **ADR**：[0068 账号页 UI 的 DPI 等比缩放](../adr/0068-account-ui-dpi-proportional-scaling.md)（Status Accepted）

---

## 1. 目标

闭环 BA §6 第 1 项登记的**缺陷 C**：账号页「换钥提示」（控件 `kIdKeyRotationNote = 1035`）在
**>150% 系统缩放**的机器上**必然裁掉末行**。

BA 只把「折行宽度未实测」从推断变实测，并给出**算术推论**（未做像素验证）：
175% 需 42 px、200% 需 48 px，而控件高度字面量是 36 px ⇒ 必裁。本批把这条推论变成**结构性不可能**。

---

## 2. 缺陷现场（BA 真机实测，本批沿用）

| 项 | 实测值 |
|---|---|
| 环境 | 本机 144 dpi = 150% 缩放，主屏 2560×1440 |
| 账号窗客户区 | `480 × 290` **物理像素** |
| 控件 1035 | `STATIC`，`456 × 36` **物理像素**，字体 = 宋体 144 dpi |
| 文案 | 37 个全角字（`重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端数据。`） |
| `DT_CALCRECT` 单行 | 宽 `666 px`（= 37 × 18）、高 `18 px` |
| 装箱（456 宽） | 折 **2 行** ⇒ 需 `36 px` ⇒ **余量 0 px** |

**根因**：UI 字体是 `DEFAULT_GUI_FONT`（进程级 stock 对象，按**系统 DPI** 光栅化），随缩放放大；
而 `CloudAccountUi.h` 的布局字面量是**固定物理像素** ⇒ 盒子没跟着放大 ⇒ 余量被吃光。
**不是单个控件的问题**：所有静态文案行、输入框、按钮同一形态（200% 下 26 px 输入框配 24 px 字高）。

**本进程不做系统级缩放**（实测）：清单 `PerMonitorHighDPIAware.manifest` = **V1**
（`<dpiAware>True/PM</dpiAware>`，⚠️ **更正 BA handoff 写成 PerMonitorV2 的口径**）；
`WeaselServer.vcxproj` 5 处 `EnableDpiAwareness=PerMonitorHighDPIAware`；
`CreateWindowExW` 传入尺寸 = 物理像素 ⇒ **布局必须由应用自己缩放**。

---

## 3. 修法（ADR 0068）

**整表按 DPI 等比缩放**，不是「只调 1035 的高度」，也不是「只缩高不缩宽」：

- 布局表以 **96 dpi 为基准**，坐标/尺寸一律 `AccountScale(v, dpi) = MulDiv(v, dpi, 96)`；
- 缩放入口**唯一化**（`AccountScaleLayout`）⇒ 结构上不可能有控件漏缩；
- **客户区尺寸**同表缩放 ⇒ 窗口边框不会裁掉控件；
- `WM_DPICHANGED` **显式吞掉**（`return 0`）⇒ 窗口与控件永远来自同一份缩放；
- **不保留「无 dpi」兼容重载** —— 那个形态**就是**本缺陷的形态，留着就是复发入口；
- 文案迁入真相源 `AccountKeyRotationNoteText()`（生产与单测同源，R7）。

**缩放系数来源 = 系统 DPI**（⚠️ 本批实施期**推翻**了早期「量真实字体反推 DPI」的口径，详见 §6.1）：
`GetDpiForSystem()`（user32，**动态 `GetProcAddress` 取** ⇒ 老系统不炸）→ 回退 gdi32
`GetDeviceCaps(LOGPIXELSX)`。

**修后推算**（144 dpi）：控件 `456×36` ⇒ `684×54`；文字 `666 px` ⇒ **单行**（18 px）⇒ 余量 36 px ⇒ 不裁。

---

## 4. 落码清单

| 文件 | 内容 |
|---|---|
| `WeaselServer/Cloud/CloudAccountUi.h` | 真相源：`kAccountBaseDpi` / `AccountScale` / `AccountScaleLayout` / `kAccountUiEmPx96` / `kAccountUiFontPx96` / `AccountEffectiveDpiFromFontPx`（**降级为交叉校验**）/ `AccountKeyRotationNoteText` / `AccountWideCharCount` / `AccountTextWidthEm` / `AccountTextLineCount` / `AccountLineHeightPx` / `AccountRequiredHeightForText` / `AccountMinHeightForText`；五张基准表改名 `…Base()`；全部派生函数**显式带 `dpi`** |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | `AccountUiDpi()`（系统 DPI + 字体度量交叉校验告警）；`CreateWidgets` / `ApplyLayout` / `AccountClientSize` 透传 dpi；`ApplyLayout` 内接**余量校验**（`AccountMinHeightForText` 不足 ⇒ `LogAccountDlg` 告警，R7 同源）；文案改用 `AccountKeyRotationNoteText()`；`WM_DPICHANGED` 显式吞掉 + 兜底 `#define` |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 旧调用点补显式 `kAccountBaseDpi`；新增 `TestAccountPageUiDpiScale()`（含**反向有牙判据**：用缺陷 C 现场几何 144 dpi/456×36 算余量必须 = 0） |
| `docs/adr/0068-account-ui-dpi-proportional-scaling.md` | 新增 ADR（背景事实 1–4、决策 1–6、残余风险 5 条、已驳回备选 7 条） |
| `docs/adr/README.md` | 索引追加 0068 行 |

**不改**：`include/WeaselIPCData.h`、任何 IPC 消息类型、任何**用户可见文案**（37 字逐字不动）、
`librime/`、控件 ID 分配（`kIdKeyRotationNote = 1035` 不变）。

---

## 5. 门禁与产物

- **云单测**：`RESULT: PASS=585 FAIL=0 / VERDICT: ALL PASS`（基线 577 ⇒ **+8**）；新增 `TestAccountPageUiDpiScale` 8 条判据（96dpi 逐像素不变 / 144dpi 等比缩放 / 换钥文案装得下 / 余量 ≥ 1 行 …）。`TEST_EXE_MTIME 20:27:14 > SRC_MTIME 20:21:15` ⇒ 判据确实跑在新二进制上，**非假绿**。IPC 回环 `PASS=11 FAIL=0`。
- **编译**：`build.bat weasel installer` ⇒ `BUILD_EXIT=0` / `BUILD_ERROR_LINES=0`；8 条 `warning 7010` 全部是既有的可选产物告警（`weaselARM*.dll` ×6、`data\*.gram`、`data\lua\*.txt`）。
- **格式**：定点 `clang-format`（`--lines=` 模式，仅本次改动区间）三文件 `exit=0` ⇒ **0 漂移**。
- **静态门禁**：`L0-static.ps1` **15/15** —— `version-consistency version=1.3.7 一致`、`artifact-version-resource 5 个产物 FileVersion 均为 1.3.7.0`、`nsis-compile` 通过。
  - ⚠️ 头一轮曾红在这一条：`weasel.sln` **只构建 Win32 平台**，`output\Win32\weaselx64.dll` 根本不存在 ⇒ x64 TSF 产物由 `build.bat` 权威路径产出（`output\weaselx64.dll`，现为 1.3.7.0）。**改用 `build.bat` 出包后复跑即恢复 15/15**。
- **其它单测**：`TestDefaultHotkeys` / `TestWeaselIPC` / `TestOrphanRecovery` 均 `EXIT=0`。（`TestResponseParser.exe` 本仓库无此产物，**缺失不等于失败**。）

### 5.1 产物与装机实证

- **安装包**：`output/archives/fluxing-1.3.7-installer.exe` = **42 975 921 B** · sha16 `F91A7A108252CF91`。
- **服务映像**：`output\Win32\WeaselServer.exe` = **2 785 280 B** · `FileVersion 1.3.7.0` · sha16 `38316DCEFC8C502E`（装机版 1.3.6 为 2 780 672 B / sha16 `3CF15A582B2453F1` ⇒ 尺寸与哈希均已变化）。
- **令牌差集**（本批诊断串是 `std::wstring` ⇒ **UTF-16LE** 宽串）：`dpi-mismatch system=` / `note-margin-short dpi=` = 新构建 **True** / 装机版 1.3.6 **False** ⇒ 本批代码确在产物内。
- **覆盖装机**：`INSTALL_EXIT=0`（`/S /D=D:\Program Files\fluxing`，`/D` 为最后一个参数）⇒ 运行中服务映像换成 **1.3.7.0**（pid 77332 / start 20:37:56 / sha16 与构建产物**逐字一致**），**无 `*.stage2` 残留**（无待开机换件），`HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing` 未被删改。
- **BA 探针复跑**（`reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1`，判据是动态计算，不硬编码几何）：
  - 桌面守卫 `[OK] desktop-guard :: thread desktop = Default`（非影刀虚拟桌面）；
  - 前置 `[OK] precondition :: pid=77332 started=2026-11-20 20:37:56 ver=1.3.7.0`；
  - 控件 `1035` = `STATIC` / **`684×54`** / 37 字（**由 `456×36` 精确放大 ×1.5**），客户区 `480×290` → `720×435`；
  - 度量：单行 `w=666 h=18`；`wrapped-into-real-box(684) w=666 h=18` ⇒ **单行排下，不再折 2 行**；
  - 判定 `[PASS] judge-A1 :: TEXT NEEDS 18px, BOX GIVES 54px -> HEADROOM 36px -> NOT CLIPPED`；
  - 像素复核 `bands=0-16 bandCount=1 lastBandTouchesBottom=False` ⇒ 墨迹未触底；
  - **`VERDICT=A1_NOT_CLIPPED` ⇒ 缺陷 C 关闭**（BA 时期同一判据为 `A1_CLIPPED`）。
- **诊断日志实测**：`%TEMP%\fluxing-profile-notify.log` 出现 `[acctdlg] dpi-mismatch system=144 font_px=21 derived=168` ⇒ 字体度量交叉校验**按设计告警**（反推 168dpi 会 +17% 超缩 ⇒ 正是本批放弃「量字体反推 DPI」的直接原因）；全程**无** `note-margin-short` ⇒ 余量充足。

---

## 6. ⚠️ 本批推翻 / 踩到的坑（必读）

### 6.1 「量真实字体反推 DPI」是错的 —— `tm` 行高 ≠ 控件单行高

ADR 0068 早期版本写「缩放系数取自真实字体度量（`GetTextMetrics` 行高 → 反推 DPI）」。
**本批实施期实测推翻**：

| 量 | 本机实测（系统 144 dpi） |
|---|---|
| `GetDpiForSystem()` / `GetDeviceCaps(LOGPIXELSX)` / `Graphics.DpiX` | **144**（三者一致） |
| `DEFAULT_GUI_FONT` 真实行高（`tmHeight + tmExternalLeading`，宋体 9pt） | **21 px** |
| 144 dpi 下**控件**单行高（BA 的 `DT_CALCRECT` / 墨迹带） | **18 px** |

⇒ 用 21 px 反推得 **168 dpi（+17%）**；96 dpi 机器同理得 112 dpi ⇒ **违反「96 dpi 逐像素不变」不变量**。
根因 = 把「控件行高模型」误当成「字体 tm 度量」——**这是 BA 记录的 18 px 属后者**。
⇒ 改法：**直接问系统要 DPI**；字体度量降级为**交叉校验告警**（偏差 > 12.5% ⇒ 记 `acctdlg` 日志，不影响布局）。

### 6.2 `TestFluxingCloud` **不在** `weasel.sln` 里 ⇒ sln 构建完 exe 还是旧的

第一次门禁跑出 `PASS=577 FAIL=0`，看着像「基线没变」——实为 **假绿**：
`TEST_EXE_MTIME=14:14` 而源码 mtime `20:21` ⇒ **本批新判据一条都没跑**。
⇒ 必须**显式**构建该工程：
`msbuild test\TestFluxingCloud\TestFluxingCloud.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=<repo>\`
⇒ **判据**：`exe.mtime >= src.mtime`，否则一律判「未执行」。

### 6.3 源文件编码：改中文前先核 BOM / UTF-8 与工程 `/utf-8`

本批重写 `CloudAccountUi.h`（含 37 字中文常量）前，先核了三条：
① 工作区文件与 HEAD 原件**同为 UTF-8 无 BOM**（严格 UTF-8 解码通过；「云端」按 UTF-8 字节 `E4 BA 91 E7 AB AF` 命中、按 GBK `D4 C6 B6 CB` 不命中）；
② `WeaselServer.vcxproj` 与 `TestFluxingCloud.vcxproj` **均带 `/utf-8`** ⇒ 中文字面量按 UTF-8 解释；
③ `git diff --numstat` 变更量正常（非全文件重写 ⇒ 没把别的内容改坏）。
⇒ **若源文件是 GBK 而工程无 `/utf-8`**，中文会按 GBK 解释 ⇒ 界面乱码，且单测「同错相等」会**掩盖**它。

### 6.4 其它

- `GetDpiForSystem` 是 Win10 1607+ 才有 ⇒ **静态导入会让 Win8.1 及更早系统整个进程加载失败** ⇒ 必须动态取 + 回退。
- 禁用 `GetDpiForMonitor`：引入 `api-ms-win-shcore-scaling-l1-1-1.dll` 依赖（教训 L122）。

---

## 7. 未闭环清单（承接上一批，逐条可追）

| # | 项 | 来源 | 状态 |
|---|---|---|---|
| 1 | **175% / 200% 真机像素证据** | BA §6 第 3 项 | 本批仍**未验**（本机只有 150% 一档；更高档位由单测缩放律覆盖几何等价，但无像素证据） |
| 2 | **A-4 / A-5 未登录态实测** | BA §6 | **未测**（需登出 = 用户账号状态变更，探针不自行发起） |
| 3 | **AZ §6 第 2 项 B-1/B-2/B-3 孤儿会话回收端到端** | AZ | **未做**（建议作为下一批第 1 项） |
| 4 | **AZ §6 第 3 项：已运行 TSF 宿主逐 pid 枚举模块验证** | AZ | 未做 |
| 5 | **AZ §6 第 4 项：`TestWeaselIPC.exe /verify-client-pid`** | AZ | 未跑（需先 `WeaselServer.exe /quit` 抢管道，否则必然假红） |
| 6 | 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 | 多批 | 未做 |
| 7 | 缺陷 A 反向判据 G-3 / G-5 | AX | 未做 |
| 8 | 本批 + AZ + BA 的文档改动**未提交 / 未 push** | — | 见 §9 |

---

## 8. 证据路径

- 本批脚本与日志：`reports/fluxing-bb-dpi-scale/`
  - `patch-dialog-dpi.ps1`（8/8 补丁）、`patch-test-dpi.ps1`（10/10 补丁）
  - `bump-version-137.ps1`（版本 bump）
  - `gate-01.ps1` / `gate-01.log`（门禁全过程）
  - `bump-version-137.log`
- 上一批真机探针：`reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1`
- ADR：`docs/adr/0068-account-ui-dpi-proportional-scaling.md`
- 上一批 handoff：`docs/archive/handoffs/handoff-2026-11-20-batch-ba-a1-note-wrap-measured.md`

---

## 9. 提交状态

本批按 Conventional Commits + 单一 scope 分三次提交（**未 push**，push 需用户授权）：

| # | 提交信息 | scope | 内容 |
|---|---|---|---|
| 1 | `docs(fluxing): 补齐 AZ/BA 批次交接文档与 1.3.6 重启后测试清单` | `docs` | AZ / BA 两份 handoff + 两份接力 prompt + `docs/release/restart-checklist-1.3.6.md`（上一批遗留、此前未提交） |
| 2 | `fix(fluxing): 账号页换钥提示 DPI 感知布局 —— 修缺陷 C（>150% 缩放裁切）` | `fluxing` | `WeaselServer/Cloud/CloudAccountUi.h`、`WeaselServer/Cloud/CloudAccountDialog.cpp`、`test/TestFluxingCloud/TestFluxingCloud.cpp`、`docs/adr/0068-account-ui-dpi-proportional-scaling.md`、`docs/adr/README.md`、`CHANGELOG.md`、`CLAUDE.md`、`version.txt`、批次 BB handoff + 接力 prompt |
| 3 | `ci(fluxing): 账号页换钥提示折行与 DPI 取证探针` | `ci` | `reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1`、`reports/fluxing-bb-dpi-scale/*.ps1` 及门禁日志 |

- **哈希**：见文末「提交哈希补记」小节（提交落库后由收口提交补入）。
- **禁提交项**：`librime/` 子模块工作区脏（`m librime`）⇒ 全程**按路径显式 `git add`**，**未** `git add .`，子模块改动未入库；`docs/design/quickpanel-v6/design-demos/*.html`（他任务遗留）与 `reports/` 下的中间产物未入库。
- **本批未改**：`librime/`、IPC 协议（`include/WeaselIPCData.h` 未动 ⇒ 不触发四边同步红线）、未新增第三方依赖。

---

## 10. 下一批建议

1. **AZ §6 第 2 项：B-1 / B-2 / B-3 孤儿会话回收端到端**（ADR 0067）—— 宿主崩溃后服务端会话表里的孤儿 `ipc_id` 应被惰性清扫；需真机强杀宿主进程后取证 `live_sessions` 归零与闸门放行。
2. **A-4 / A-5 未登录态判据** —— 需用户在场并明确授权**登出**（改账号状态，探针不自行发起），再复跑 `probe-a1-note-wrap.ps1` 的未登录分支。
3. **175% / 200% 缩放真机复验** —— 改系统缩放比例后复跑同一探针，判据仍是 `VERDICT=A1_NOT_CLIPPED`。
4. **AZ §6 第 3 / 4 项** —— 逐 pid 枚举已运行 TSF 宿主的模块以验证实际加载的 shim 版本；`TestWeaselIPC.exe /verify-client-pid` 需先 `WeaselServer.exe /quit` 让出命名管道（真服务端在跑时 mock 必假红）。
5. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5** —— 用户在场时执行。
6. **提交与 push** —— 本批只做了本地提交；`feat/100-cloud-sync` → `kizemo/feat/100-cloud-sync` 的 push 需用户授权后执行。

---

## 11. 提交哈希补记

| # | 哈希 | 提交信息 |
|---|---|---|
| 1 | `b1c9c7b` | `docs(fluxing): 补齐 AZ/BA 批次交接文档与 1.3.6 重启后测试清单` |
| 2 | `255af0b` | `fix(fluxing): 账号页换钥提示 DPI 感知布局 —— 修缺陷 C（>150% 缩放裁切）` |
| 3 | `77dc265` | `ci(fluxing): 账号页换钥提示折行与 DPI 取证探针` |

- 分支 `feat/100-cloud-sync` 相对 `kizemo/feat/100-cloud-sync` **ahead 3** ⇒ **未 push**（push 需用户授权）。
- 本小节随收口提交（`docs(fluxing): 补记批次 BB 提交哈希与真机复验结论`）落库，其自身哈希见 `git log -1`。
- `librime/` 子模块工作区仍脏（`m librime`）⇒ **未入库**（全程按路径显式 `git add`，未用 `git add .`）。
- `docs/design/quickpanel-v6/design-demos/*.html`（他任务遗留）**未入库**。
