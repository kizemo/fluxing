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
| 1 | **175% / 200% 真机像素证据** | BA §6 第 3 项 | **未闭环（用户 2026-11-20 拍板「先不切，按未闭环收口」）** —— 本机 `AppliedDPI` 恒 144，免注销切档五条路径全排除（§12.2）；已闭环的是 **144 档真机像素证据**（§12.4 两份独立）+ 整表等比缩放律（云单测 586-0）。缺的只是 168/192 两档像素级确认；重新发起须先真切档再跑 `run-dpi-tier.ps1 -ExpectedDpi 168/192`（脚本侧已就绪，见状态正本 §10.4） |
| 2 | **A-4 / A-5 未登录态实测** | BA §6 | **未测**（需登出 = 用户账号状态变更，探针不自行发起） |
| 3 | **AZ §6 第 2 项 B-1/B-2/B-3 孤儿会话回收端到端** | AZ | **未判定**（探针三轮 run-01/02/03 的全部阻塞点已逐个坐实并修掉，run-03 已跑通「宿主窗可见 + 账号窗打开 + 合成键入 + 两轮手动同步」全链；**差最后一条硬前置：探针宿主上 Fluxing 不是当前文本服务 ⇒ 键入没进 Fluxing ⇒ 闸门正当放行**。取证零成功，三条判据仍零证据；根因、修法与拍板项见 §13。建议作为下一批第 1 项） |
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

---

## 12. 收口追加（BB 出包后 · 缺陷 D + 175%/200% 取证阻塞）

本节由 BB 出包装机后的续接回合补记，**未经用户授权提交**。

### 12.1 缺陷 D 已修（`AccountUiDpi()` 缓存系统 DPI 值）

- **定性**：`CloudAccountDialog.cpp` 的 `AccountUiDpi()` 用 `static int cached = 0; if (cached > 0) return cached;` **永久缓存系统 DPI 值**，违反官方 `GetDpiForSystem` 页原文「You should not cache the system DPI, but should use `GetDpiForSystem` whenever you need the system DPI value.」（Microsoft Learn，2024-02-22 版）。后果：用户改缩放后账号页仍按旧 DPI 排版，直到服务进程重启；而 `WeaselServer.exe` 是 **PPL，不能 kill** ⇒ 实际表现为「改了缩放却看不出变化」。
- **修法**（结构上不再存在值缓存）：
  - `WeaselServer/Cloud/CloudAccountUi.h` 新增纯函数 `inline int AccountNormalizeSystemDpi(long raw)`：合法区间 `[48, 960]`，越界回退 `kAccountBaseDpi = 96`。
  - `WeaselServer/Cloud/CloudAccountDialog.cpp::AccountUiDpi()` 删除值缓存；`static GetDpiForSystemFn fn` + `static bool fn_resolved` **只缓存函数指针**；探测用 `long raw`，`raw < 48` 回退 `GetDeviceCaps(LOGPIXELSX)`；末行 `const int dpi = AccountNormalizeSystemDpi(raw);`。
  - 交叉校验日志加 `static bool mismatch_logged` **只记一次**（保留既有 `delta * 8 > dpi` 判据）。
- **新增判据 9**（`TestAccountPageUiDpiScale()` 末尾）：`AccountNormalizeSystemDpi(0/-1/47/961/4000) == 96`；`(48/120/144/168/192/240/288/480/960)` 原样透传。测试注释已写明「**不缓存值**无法用单测直接证明（同一进程内系统 DPI 恒定）⇒ 生产侧改成结构上不存在值缓存，单测只覆盖可测分支」，避免留下假覆盖。
- **门禁**（实测）：云单测 **`PASS=586 FAIL=0` / `VERDICT: ALL PASS`**（AX/AY 基线 577 + BB 判据 1–8 = 585 + 本轮判据 9 = **586**，计数自洽），回环回归 11-0；`build.bat weasel` **exit 0 / 0 警告 0 错误 / 25.79 s**；`CloudAccountDialog.obj` mtime 晚于源码 ⇒ 确实重编译；clang-format 定点 `CloudAccountUi.h:151-162` 与 `CloudAccountDialog.cpp:1150-1196` 均 exit 0，判据 9 区间格式化一次后 exit 0。
- **既有噪声（非本轮引入）**：`TestFluxingCloud.cpp` 3 条 `warning C4996 'fopen'`（第 3164/3175/3215 行），距本轮改动的 2966-2988 行很远。
- ~~**未出包**：缺陷 D 修复目前只在 `output/Win32/WeaselServer.exe`（未出包、未装机）；`fluxing-1.3.7-installer.exe` **不含**该修复；须 bump 1.3.8 + 重新出包 + 覆盖安装才能真机生效。~~ **本条已作废（2026-11-20 21:5x 磁盘/进程实测更正）**：`output/archives/fluxing-1.3.7-installer.exe`（42 975 921 B）**含**该修复且**已装机** —— 装机件 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 20:30:28 写入、服务进程 pid 77332 于 20:37:56 启动且 `FileVersion=1.3.7.0`，装机后真机探针读出的正是**缩放后**几何（客户区 720×435、`id=1035` 684×54 = 96 dpi 基准 480×290 / 456×36 的 1.5 倍）⇒ **缺陷 D 已在真机生效，不需要 bump 1.3.8**。完整实测链见状态正本 §10.1–§10.3。

### 12.2 §10 第 3 项（175%/200% 真机复验）**已登记未闭环**（用户拍板不切档）

**免注销切档五条路径全部实测排除**（别再试）：

1. DisplayConfig 私有 `GET/SET_DPI_SCALE`（`-3`/`-4`）⇒ 本机返 `87 = ERROR_INVALID_PARAMETER`；SDK `10.0.26100.0\um\wingdi.h:3046-3052` 的 `DISPLAYCONFIG_DEVICE_INFO_TYPE` 只到 `SET_WCG_STATE = 17`，两版 SDK 的 `um`/`shared` 均无 `displayconfig.h`。
2. `SPI_SETLOGICALDPIOVERRIDE`（正确常量 **`0x009F`**，首测因正则把 `0x009F` 截成 `0` 而作废重跑）⇒ 四档全 `ok=True` 但 `EFF` 恒 144（`v=+2` 另返 `err=1460 ERROR_TIMEOUT`），还原后与 BEFORE 逐字相同。
3. 注册表 `PerMonitorSettings\<mon>\DpiValue`（-1/-2/+1）+ `WM_SETTINGCHANGE("Display")` 广播 ⇒ `EFF` 恒 144。
4. 注册表直改 `HKCU\Control Panel\Desktop\LogPixels` ⇒ 只注销/重登后生效。
5. `SPI_SETWORKAREA` 类广播 ⇒ 无效。

**当前本机状态**：`EFF/SYSTEM/LOGPIXELSX/AppliedDPI` 全部 **144**，但 `HKCU\Control Panel\Desktop\LogPixels=168`、`Win8DpiScaling=1` ⇒ **175% 已登记未生效**（与「走了自定义缩放路径」吻合；`PerMonitorSettings` 两条 `DpiValue` 已还原为 0）。

**已拍板（2026-11-20）**：用户选 **②** —— 原话「**先不切，按未闭环收口**」。168/192 两档真机像素证据**登记为未闭环**（不是「待办」、不是「阻塞」），本批不再要求任何物理动作，也不再重试上面五条切档路径。替代判据见状态正本 §5（等价判据）与 §10.2（144 档两份独立像素证据 + 等比自证 `480×290×1.5=720×435`、`456×36×1.5=684×54`）。将来若要补证，仍走「预设 175%/200%」路线，脚本侧无需再改。

### 12.3 状态正本与新增资产

- 状态正本：`reports/fluxing-bb-dpi-scale/dpi-tier-probe-status.md`（§1 实测表、§2 资产与用法、§3 已排除路径、§4 未闭环、§5 等价判据、§7 缺陷 D 全记录）。
- 新增：`run-dpi-tier.ps1`（单档一键，桌面守卫 + **错档拒绝退出码 3**，守卫自检已验）、`retest-spi-override.ps1` + `spi-override-retest.txt`、`cloudtest-defect-d.txt`、`build-weasel-defect-d.log`；`dpi-scale.ps1` **作废留档**。
- `run-dpi-tier.ps1` 内 installer 路径**写死 1.3.7**，换包须同步改。

### 12.4 收口追加：测量侧两处守卫已修 + 144 档证据闭环 + 未闭环清单定稿

**测量侧（本轮新增，属探针代码、非产品代码；未改 `librime/`、未改 IPC 协议、未新增依赖）**

- `run-dpi-tier.ps1` **guard 2 判源改为注册表 `AppliedDPI`**。原实现拿 `GetDpiForSystem() -ne ExpectedDpi` 判档，而该值在本机任何宿主恒返 **96** ⇒ 任何人切到 175% 也会被 `ABORT_WRONG_DPI`（退出码 3）**假拒**。`GetDpiForSystem`/`LOGPIXELSX`/`LogPixels` 降级为信息行，与宿主读数不一致时打 `WARN_HOST_DPI`（宿主不感知，非档位问题）。
- **新增 guard 3：几何本档校验**。以 96 dpi 基准 480×290 按 `MidpointRounding::AwayFromZero` 缩放到目标档（96⇒480×290 / 144⇒720×435 / 168⇒840×508 / 192⇒960×580），与 `report.json` 的 `dialog_client` 实测比对：等于基准宽 ⇒ `ABORT_DPI_VIRTUALIZED`，等于别档宽 ⇒ `ABORT_STALE_DPI`（点名匹配到的档），两者均**退出码 7**；取不到几何 ⇒ 退出码 6。头部 exit-code 文档已同步。
- **四方向实测（不是纸面推断）**：拒绝路径 `EXIT=3`、144 档整链 `EXIT=0`（`CLIENT=720x435 EXPECTED=720x435` + `CLIENT_MATCHES_TIER`）、虚拟化有牙 `EXIT=7`、stale 别档有牙 `EXIT=7`；证据目录 `selfcheck-reject-168\` `selfcheck-144\` `selfcheck-teeth-virt\` `selfcheck-teeth-stale\`，合成数据由 `selfcheck-stub-probe.ps1` 经 `STUB_CLIENT` 注入（**只写合成 `report.json`，不用于取证**）。
- **rect 未虚拟化的实测自证**：`probe-dpi-awareness.ps1` 在同一机器同一时刻跑 pwsh 7（`THREAD_AWARENESS=1`）与 powershell 5.1（`=2`），账号窗客户区均 **720×435**、`id=1035` 均 **684×54**、`id=1006` 均 684×60，**逐字相同**且等于 144 dpi 设计表、不等于 96 空间（480×290 / 456×36）⇒ 现有宿主可直接做高 dpi 取证，无需先解决「宿主不感知」。

**144 档真机证据（已闭环，两份独立来源）**：`evidence-after-1.3.7\probe-report-after-137.json`（`dpi=144` / `dialog_client=0,0,720,435` / `id=1035` 684×54 / `verdict=A1_NOT_CLIPPED` / `logged_in=True`）与 `selfcheck-144`（探针 `note.chars=37 / box_w=684 / box_h=54 / line_h=18 / lines=1 / deficit_px=-36 / clipped=False`）。

**未闭环定稿**：175%/200% 两档像素证据按用户拍板登记未闭环，口径与「将来重跑」命令见状态正本 §10.4。状态正本 `dpi-tier-probe-status.md` 现为 §1–§10（§9 测量侧自证、§10 收口核实），§9.4 已加「后记」指向 §10.4。

---

## 13. 收口追加（续轮 · §7 第 3 项 B-1/B-2/B-3 孤儿会话回收端到端 = **未判定**，三轮探针根因全部坐实）

**本段性质** = 纯取证轮（**未改任何产品代码、未改 `librime/`、未改 IPC 协议、未新增依赖、未出包、未装机、未提交**）。装机版仍是 1.3.7.0。

### 13.1 三轮探针的阻塞点逐个坐实并修掉（改的都是 `scripts/verify/` 探针码）

| run | 断点 | 根因（实测，非推断） | 修法（已落码） |
|---|---|---|---|
| 01 | `ABORT_HOST_WINDOW_MISSING`（exit 7） | 宿主窗**确实存在**（`128480\|vis=0\|sz=502x276\|WindowsForms10.Window…`），marker 的 `host_pid` 与 `Start-Process .Id` **逐字相同 ⇒ pid 归属口径没错**；父侧 `Top()` **硬要求 `IsWindowVisible`** ⇒ 枚举恒空。`Start-Process -WindowStyle Hidden` 起的子进程里裸 `Form.Show()` 不产生可见窗 | 宿主侧强制可见（`WindowState=Normal` + `ShowWindow(SW_SHOWNORMAL)` + `SetWindowPos(0x43 含 SWP_SHOWWINDOW)` + `SetForegroundWindow` + marker 记 `hwnd/host_class/visible/ws/ex`）；父侧新增**不要求可见**的 `TopAny(cls)`（保留 `Top()` 原语义供账号窗查找） |
| 02 | `ERROR tray window not found on server pid` | `Open-Dlg` 把 `WM_COMMAND(40024)` 投给**托盘壳窗**且用 `Top()` 查（该窗常不可见）⇒ 必假红。同族已证实形态在 `scripts/verify/verify-cloud-account-window.ps1:311-320`：40024 只由**服务端自身主窗**处理，`CSystemTray` 的 WndProc 不管它 | 改为向**服务端 pid 的全部顶层窗**逐个 post 40024（只有 40024 有注册处理器，其余 post 无害），并把 `open_dlg_targets` 落 `report.json` 作证据 |
| 03 | `INCONCLUSIVE_WINDOW`，Leg A/B 均 `blocked=False` | 见 §13.2 —— **键入没进 Fluxing**，闸门正当放行；不是产品回归 | 未修（需用户拍板，§13.4） |

### 13.2 run-03 实测：取证链已跑通到最后一步，但**宿主没有 Fluxing 会话**

- 运行态（当场回读）：`server pid=77332 ver=1.3.7.0`；`host1 pid=333452 hwnd=0x1e0cd6 rect=502x276 visible=True marker.visible=1`；`dlg_opened_before_typing=true`；`open_dlg_targets` 5 个窗；账号窗关闭钮 `BM_CLICK(1016)` 生效（状态文本回读到「手动同步已提交…」）。
- Leg A `ms_since_last_key=192` ⇒ `blocked=False`；Leg B `ms_since_last_key=7770` ⇒ `blocked=False`，且 7770 < `IdleGraceMs=10000` ⇒ Leg B **落在无判别力窗口**（「最近输入」还没过期，与 Leg A 无法区分）⇒ 脚本按设计判 `INCONCLUSIVE_WINDOW` 作废，**不记 PASS/FAIL**。
- 🔑 **决定性证据 = 服务端诊断日志整轮几乎没长**：`295893 B → 295985 B`（**+92 B**），新增的**唯一一行**是 `23:33:47.894][acctdlg] WM_DESTROY: cleared slot for FluxingCloudAccountDialog_v1`（= 本探针 `finally` 关账号窗）。⇒ 全轮**没有** `focus_in profile presence=present`、**没有** `own_ui_session`、**没有** `remove_session`、**没有**任何闸门/skip 行。两条日志切片 `log-slice-A/B_*.txt` 各 **3 B（空）**。
- 与诊断枚举吻合：宿主进程上挂着 **`Sogou_TSF_UI` / `SoPY_Status` / `SoPY_Hint` / `SoPY_UI`（vis=1）** ⇒ 本机当前文本服务是**搜狗**，`SendKeys 'zhongguo'` 被搜狗吃掉，**Fluxing 侧连会话都没建** ⇒ `live_sessions=0` ⇒ `MaintenanceGateAllowsWithIdle(live<=0)` 放行是**正确行为**。
- **闸门通过率仍未知**（本批不能否证 ADR 0065 的实现，只能说这条真机路径没测到）。

### 13.3 本轮新增的固化判据（下次直接用，别再踩）

1. ✅ **`reclaim orphan session ipc_id=` 是文件可见的**：`RimeWithWeasel.cpp:1281` 用 `fluxing::diag::AppendLine("server", …)`；`:1279` 那条 `DLOG(INFO) << "reclaim orphan session: ipc_id = "` 无调试器不可见。**B-2 判据通道成立**，全日志当前命中 **0 次**（历史上从未回收过，符合 ADR 0067「崩溃后若无新会话 ⇒ 残留永不清」）。
2. ⚠️ **B-2 判据文本按清单字面必假红**（源码级坐实）：清单要 `… client_pid=<pid> live=<m>`，实现只写 `reclaim orphan session ipc_id=<n>` ⇒ 客户端 pid **全链路不落日志**；ipc_id ↔ 探针宿主也无直接通道 ⇒ 只能靠「sweep 窗口内恰好 N 条 reclaim 行」间接归因。**待用户拍板**：降级取证 or 补日志字段后重新出包 + 覆盖装机（代价 ≈ 一次出包 + 41 s 装机）。
3. 🚨 **`db mtime 前进` 单独不成牙齿**：run-03 里 Leg A 点击于 `23:33:39`，而 `db_mtime_after=23:33:33` **早于点击** ⇒ 该前进来自 librime 自身的 userdb 落盘，与「立即同步」无关。**判据必须收紧为 `mtime_after >= 点击时刻`**（并优先以日志行定性），否则任何一次后台导出都会伪造「放行」。`advanced` 的语义因此作废，`report.json` 里的 `advanced=True` 本轮**不得引用**。
4. ⚠️ `own_ui_session ipc_id=`（`:474-479`）**只对自家 UI 客户端打印** ⇒ 不能用来证明「探针宿主建了 Fluxing 会话」；`Add session: created session_id=` 走 `DLOG` ⇒ **宿主是否建会话没有直接日志通道**，只能靠「日志增长量 / focus_in presence=present」反证。本轮用的就是这个反证。
5. Leg B 的合法窗口比 = 必须让 `IdleGraceMs < ms_since_last_key < CompGraceMs`（本机 10 s < idle < 60 s），且脚本边沿 8 s 内作废；本轮 `7770 ms` 是**等待时长算错**（超 `$CompGraceMs` 前太早就点）⇒ 下一轮把 Leg B 的等待改成**显式等到 idle ≥ 15 s**。

### 13.4 唯一剩下的硬前置 + 待拍板（下一批第 1 项的门槛）

**硬前置** = 探针宿主进程上 **Fluxing 必须是当前文本服务**（否则永远测不到闸门，也永远造不出孤儿会话）。三条路：

- **甲（最省，需用户在场，推荐先试这条）**：探针把宿主窗置前后，**由用户手动把输入法切到火流猩**（一次常规切换），脚本再键入。副作用 = 改动用户当前输入法选择，须用户同意并当场在场；测完用户自己切回。
- **乙（无需用户在场，探针侧写 TSF 绑定）**：在宿主子进程里用 TSF API 显式激活 Fluxing 的 TIP（CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` / profile `{3D02CAB6-…}`，见 `install.nsi:1154/1163` 与 `docs/release/restart-checklist-1.3.6.md`）。代价 = 新增探针码 + 需查证正确的接口序列（**本项目尚无已证实形态，不得凭记忆猜 API**）。
- **丙（放弃真机端到端）**：B-1/B-2/B-3 长期登记「单测覆盖 + 真机未判定」，把 §7 第 3 项按「受环境限制、不追」收口。

**探针已就绪、无需重写**：`scripts/verify/orphan-probe-host.ps1`（7,569 B，实测 `visible=1`）+ `scripts/verify/verify-orphan-session-reclaim.ps1`（4 处修复全落码、`PARSE_OK`）。下一轮**只需**：加 Leg B 等待修正（§13.3 第 5 条）+ 收紧 `advanced`（第 3 条）+ 走 §13.4 甲或乙，然后跑 run-04。
另需知：跑之前**确认用户不在打字**（Leg C 必须在宿主最后一次键入后 60 s 内点击，期间用户任何真实键入都会把闸门正当关掉 ⇒ 假 FAIL）。

### 13.5 证据路径（本轮全部产物）

`reports/fluxing-bb-orphan-reclaim/` = `evidence-log-vocab.txt`（判据字面量真源）、`diag-enum-01.md`（run-01 一次取齐诊断：marker 全文 + 全桌面窗口清单 + 宿主 stdout/stderr + 残留核对）、`report-run01.json` / `report.json`（= run-03 快照，已另存 `report-run03.json`）/ `notes.txt`、`run-02-console.txt` / `run-03-console.txt`、`log-slice-A_recent_input.txt`（3 B）/ `log-slice-B_orphan_composing.txt`（3 B）。

**本轮工具坑（新增）**：① 判「日志本轮有没有长」要**同时**看字节增量与新增行的**时间戳**，只看命中数会把「历史累计 483 条 `remove_session`」误读成本轮事件；② `& pwsh -File` 里脚本用 `exit` 会杀掉整个 pwsh ⇒ 探针必须起子进程跑；③ 三腿同处一个 60 s 组合宽限窗口，**任何一次 Leg 的等待时长要按 `ms_since_last_key` 现算而非按 `Start-Sleep` 秒数推算**（本轮 Leg B 就是这么落进无判别力窗口的）；④ **写取证/诊断脚本时 P/Invoke 的 DLL 归属要逐个核**（`GetWindowRect`/`ShowWindow`/`SetWindowPos` 都在 **`user32`** 不在 `kernel32`；`GetCurrentThreadId` 在 `kernel32`）⇒ 写错是**运行期** `EntryPointNotFoundException`，不是编译期错误，本轮踩过；⑤ `Start-Process -RedirectStandardOutput/Error` 的落地文件被子进程持有 ⇒ 读取必须用共享模式 `File.Open(p,'Open','Read','ReadWrite')`，`ReadAllText` 会抛 `IOException`。

**推送口径更正（本轮实测）**：§11 收口状态登记的「分支 ahead 3 未 push」**已不成立**。本轮 `git rev-list --left-right --count HEAD...kizemo/feat/100-cloud-sync` = **`0	0`**，`git log kizemo/feat/100-cloud-sync..HEAD` **空** ⇒ `feat/100-cloud-sync` 与上游**完全同步**（此前某轮已完成 push）。**本轮新增未提交改动**（均未 `git add`）：`scripts/verify/orphan-probe-host.ps1`、`scripts/verify/verify-orphan-session-reclaim.ps1`、`scripts/verify/diag-host-window-enum.ps1`（三个均 untracked 探针）+ 本 handoff + `docs/archive/prompts/prompt-next-session-2026-11-20-batch-bb-account-ui-dpi-scale.md`（接力 prompt 已就地改写为本轮口径）+ `reports/fluxing-bb-orphan-reclaim/*` 与 `reports/fluxing-bb-dpi-scale/dpi-tier-probe-status.md`。**push 仍需用户授权**。

## 14. 收口追加（批次 BC · AZ §6 第 2 项 B-1/B-2/B-3 孤儿会话回收端到端 = **实测闭环 PASS**）

### 14.1 本批性质 = 代码实现（**仅日志字段**）+ 取证批次

未改 `librime/`、未改 IPC 协议消息类型、未新增第三方依赖、用户可见中文文案零改动。
产品码改动只有一处：`include/FluxingSessionReclaim.h` 新增纯函数 `FormatReclaimLogLine(ipc_id, client_pid, live_after)`（**全 ASCII**——`RimeWithWeasel.vcxproj` 无 `/utf-8`；R7 生产与单测同源），服务端 `RimeWithWeasel/RimeWithWeasel.cpp::_ReclaimOrphanSessions` 在 `_RemoveSessionInternal(ipc_id, true)` **之后**写该行 ⇒ `live=` = **回收后**存活会话数、`client_pid=` = 被回收会话上报的宿主 pid。
⇒ **未走「加同步轮次诊断日志 + bump 1.3.9 重出包装机」那条路**（见 §14.3：不需要）。

### 14.2 B-2 / B-3 端到端 PASS

探针 `scripts/verify/orphan-ipc-chain.ps1`，打真机 **1.3.8.0** 服务端（pid 304416 / `D:\Program Files\fluxing\weasel\WeaselServer.exe`）：
孤儿会话 `ipc_id=2493775873` / `client_pid=207096` → 强杀宿主 → `--trigger` 客户端触发 `AddSession` 惰性清扫 ⇒
`reclaim orphan session ipc_id=2493775873 client_pid=207096 live=1`（**B-2 字面量命中**）+
`remove_session active orphan_reclaim presence=kept`（**B-3**）+ `presence=absent` **0 次**（**B-3 反向判据**）⇒ `VERDICT=ORPHAN_RECLAIM_OK` / `EXIT=0`。
`WEASEL_IPC_ECHO` 探活：存活时 `ECHO=2493775873`、回收后 `ECHO=0`。

### 14.3 ⚠️ 本轮最大认知 = B-1 的「账号页观测面」在产品侧**根本不存在**（不是判据写错）

- 账号页「最近同步」控件 `kIdSyncStatus = 1017` 的文案取自 `WidgetText()` 的**静态字面量**「最近同步：尚未同步。」；`RefreshUi()`（`WeaselServer/Cloud/CloudAccountDialog.cpp:1299-1352`）**只**刷新 1006 状态行 / 账号标签 / 历史下拉 / 设备列表，**从不写 1017**。
- 轮次结果只存在 `Impl::sync_status_text` 成员里（`SetSyncStatus` → `impl_->sync_status_text`，:232-235），**没有任何可见控件承载** ⇒ 「读 1017 看是否被跳过」这条路**在结构上不可能成立**。
- 前两轮探针（`scripts/verify/orphan-gate-sync-e2e.ps1` v1/v2）据此判 `FAIL=B1A_FAILED` 属**判据无效**，**不是产品缺陷**。
- ⇒ 改用**项目自己的官方口径**：`docs/release/restart-checklist-1.3.6.md` §4 明写「闸门是否放行 = **维护导出文件 mtime 是否前进**（状态文案不可作判据）」，观测面 = `<RimeUserDir>\sync\<账号>\rime_ice.userdb.txt`（本机 = `D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-d5b9-4dfc-9929-85eac8509db6\rime_ice.userdb.txt`，4 138 942 B）。
- ⇒ **因此不需要改产品代码、不需要 bump 1.3.9、不需要重新出包装机**（原计划的「加同步轮次诊断日志」路线作废）。

### 14.4 闸门语义与 B-1 的判别力来源（`include/FluxingMaintenanceGate.h`，ADR 0065，R7 真相源）

`allows = live<=0 || !has_input_history || idle>60s(有未上屏组合串) || idle>10s(无组合串)`；
`idle` 取**全局** `m_last_input_tick`（`ProcessKeyEvent` 每次按键都写，:713-717）。
⇒ **回收前后 `live_sessions` 数量其实不变**（回收方客户端自己占一个槽）⇒ B-1 的判别力**只能来自 `composing_sessions` 归零**（`_RemoveSessionInternal` :642-643 同时清 live 与 composing 两个闸门输入）。
⇒ 合成客户端必须真的持有「未上屏组合串」（`_SyncSessionComposing` → `status.is_composing`）才造得出「残留会话把闸门顶住」的状态；本轮把 `--hold` 改成**每 1.5 s 按一键**且支持 `<marker>.pause` 暂停打字才造出来。
⇒ **判别窗口**：`idle` 必须落在 **(10 s, 60 s]**——>10 s 才排除「最近有输入」判据，≤60 s 才排除「组合串宽限已过期」判据。本批 A2 用 idle≈13 s、B 用 idle=28.8 s ⇒ 两侧都在窗口内。

### 14.5 B-1 端到端 PASS（三段式对照）

探针 `scripts/verify/orphan-gate-export-mtime.ps1`（21 762 B），跑批 `reports/fluxing-bb-orphan-reclaim/gate-export-20261121-004132/`：

| 段 | 状态 | 结果 |
|---|---|---|
| **C 对照** | 无合成会话 | 触发一轮 ⇒ 导出文件 **0.4 s 推进**（证明触发链路 + 判据都有效） |
| **A2 残留** | 合成宿主存活、停打、idle > 10 s | 触发一轮 ⇒ **12 s 内零推进** ⇒ 闸门被残留会话顶住（B-1 的「之前」） |
| **B 回收后** | 强杀宿主 → `--trigger` 触发惰性清扫 | `ECHO_AFTER=0`、`IDLE_BEFORE_B_SEC=28.8` ⇒ 再触发一轮 ⇒ 导出文件 **0.4 s 推进** ⇒ **不再被跳过** |

⇒ `CRITERION_B1_CONTROL_ADVANCED=1` / `CRITERION_B1_RESIDUAL_BLOCKED=1` / `CRITERION_B1_RECLAIMED_ADVANCED=1` ⇒ `VERDICT=GATE_EXPORT_E2E_OK` / `EXIT=0`。
同轮 B-2/B-3 复核：`reclaim orphan session ipc_id=2493775873 client_pid=244760 live=1` ×1、`presence=kept` ×1、`presence=absent` ×0。

### 14.6 覆盖边界（不夸大）

- 被验对象是**合成 IPC 客户端**（与 TSF 宿主共用服务端同一条 `ProcessKeyEvent` → 会话/组合串记账 → 闸门输入 → 惰性回收路径）⇒ 证明的是**服务端 ADR 0067 路径确实释放了维护闸门**，**不等于**「真实 TSF 宿主崩溃现场原貌」已被复现。
- **B-4**（正常切窗 / 用户主动切走 ⇒ 表现与批次 AW 前一致）、**B-5**（旧客户端不写 `session.client_pid=` ⇒ 按「pid 未知」不回收、不得误杀）本批**未做**（B-4 需真实宿主、B-5 需旧版客户端）。

### 14.7 本轮工具坑（新增，务必带走）

1. **`RefreshUi` 不写 1017** ⇒ 「读账号页控件判跳过/不跳过」结构上不成立（两轮假 FAIL 的根因）。**先确认观测面真的被刷新，再信它。**
2. **导出文件 mtime 在轮次早期就推进**（对照轮 **0.4 s**）⇒ 判据极灵敏，「是否被跳过」1 s 内可判；**不必**等整轮网络往返。
3. **回收前后 `live_sessions` 不变** ⇒ 判别力只来自 `composing_sessions` ⇒ 合成客户端必须**持续按键**（一次性按键在 idle>10 s 后闸门即放行 ⇒ 探针无判别力）。
4. **idle 判别窗口 (10 s, 60 s]**：A2 与 B 的触发时刻都要落在窗口内，否则「B 轮推进」证明不了任何东西（组合串宽限过期后闸门本来就会放行）。
5. **合成客户端暂停打字** = 父脚本创建 `<marker>.pause` 文件，工具侧轮询文件是否存在。
6. PowerShell：`Log "fmt" -f a,b` 里的 `-f` 必须**加括号**或改插值；`-f` 参数跨行且行尾带逗号会抛「Index must be >= 0」。

### 14.8 证据路径（本批全部产物）

`reports/fluxing-bb-orphan-reclaim/gate-export-20261121-004132/`（**B-1 主证据**：`console.txt` / `account-children.txt` / `log-slice.txt` / `hold.*` / `trigger.*`）·
`reports/fluxing-bb-orphan-reclaim/ipc-chain-20261121-002839/`（**B-2/B-3**）·
`reports/fluxing-bb-orphan-reclaim/gate-sync-20261121-003351/` 与 `gate-sync-20261121-003540/`（**1017 文案判据的两次假 FAIL**，留作反例）·
`reports/bb-orphan-reclaim/`（1.3.8 构建/装机日志 + `install-138.cmd`）·
新脚本 `scripts/verify/orphan-gate-export-mtime.ps1`（B-1 新探针）/ `orphan-ipc-chain.ps1`（B-2/B-3）/ `orphan-gate-sync-e2e.ps1`（**旧 1017 判据探针，留作反例**）/ `test/TestOrphanReclaimE2E/`（合成客户端）。
产物：`output/archives/fluxing-1.3.8-installer.exe` = 42 967 778 B · sha16 `AB009C20F6304098`；`output\Win32\WeaselServer.exe` = 2 785 792 B · `FileVersion 1.3.8.0`。

### 14.9 未闭环清单（本批新增，沿用旧项不重复）

① **B-4 / B-5 未做**（B-4 需真实 TSF 宿主产生 FocusOut；B-5 需旧版客户端）；② 「换钥后旧行不可解是否在账号页提示」待用户拍板；③ 175% / 200% DPI 两档、A-4 / A-5（需用户在场登出）、双机 AC 7 条、restore 端到端、输入延迟两组对照、缺陷 A 反向判据 G-3/G-5、`/verify-client-pid`（需授权停服窗口）均**沿用未闭环**；④ **B-1/B-2/B-3 只覆盖服务端路径**，真实 TSF 宿主崩溃现场未复现；⑤ 本批 + BB + AZ 的文档与脚本改动**未 push**（需用户授权）。

### 14.10 提交哈希补记（本批已本地落库，push 需用户授权）

| # | 哈希 | 提交信息 |
|---|---|---|
| 1 | `5ef9a23` | `fix(fluxing): 孤儿会话回收日志补 client_pid/live 字段 —— 支撑 B-2 判据` |
| 2 | `86efdab` | `ci(fluxing): 合成 IPC 客户端探针 —— B-1/B-2/B-3 孤儿回收端到端取证` |
| 3 | 见 `git log -1` | `docs(fluxing): 1.3.8 孤儿回收端到端闭环收口（CHANGELOG + handoff §14 + 接力 prompt）` —— 本小节随其落库，故其自身哈希以 `git log -1` 为准 |

- 分支状态：`feat/100-cloud-sync` 相对 `kizemo/feat/100-cloud-sync` **ahead 3 / behind 0** ⇒ 本批**已本地提交、未 push**（push 需用户授权）。
- 提交纪律：`librime/` 子模块工作区脏（`m librime`）⇒ 全程**按路径显式 `git add`**，未用 `git add .`，子模块改动未入库；`docs/design/quickpanel-v6/design-demos/*.html` 与批次 BB 旧轮三个探针脚本保持 untracked，未入库。

