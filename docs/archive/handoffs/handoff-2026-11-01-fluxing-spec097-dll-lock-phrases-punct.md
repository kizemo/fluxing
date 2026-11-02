# 交接文档 · Fluxing 1.1.6 · spec 094/095/096 已交付 + 三项新诉求取证

> ⚠️ **本文档已被取代，仅作历史记录保留。**
> 现行交接文档：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-02-fluxing-spec097-phrases-paste.md`
> 取代原因：诉求 2（快捷面板双击/回车上屏 + 上屏延时）已在 2026-11-02 完全修复并经用户实测通过（注入耗时 625/703 ms → 16/31/0 ms），且新增了「诊断探针待剥离 + 干净构建」这一硬收尾步骤。新文档才是接手入口。

- 生成时间：2026-11-01（GMT+8）
- 生成会话：`mvs_d8aee02cc7c048bb8d4fcb737f24b8e7`
- 仓库：`F:\soft\00selfmade\rime_claude`　分支：`Fluxing`　状态：`## Fluxing...kizemo/Fluxing [ahead 11]`，工作区干净（仅 ` m librime` 既有脏子模块），**未 push**
- 接手前置：本文件是**唯一入口**。不要从零重做已完成的事。

> ⚠️ 本文档区分三种置信度，接手时请按此对待：
> **【已验证】**= 本轮从磁盘/编译产物/git 实测读出；**【推断】**= 有代码依据但未跑判别实验；**【已证伪】**= 走过且确认是死路，**不要再走**。

---

## 0. 一句话现状

spec 094（两个装机 bug）+ spec 095（常驻开关进设置面板）+ spec 096（FocusIn/Out 位标志协议表）已实现、全量套件全绿、6 条提交入库、并已部署到装机目录（服务端已生效，TSF 侧待重启生效）。用户随后提出三项新诉求（DLL 占用免重启 / 双击短语不上屏 / 中文下英文标点），**本轮只完成取证，未改任何代码**。三项中只有第 1 项根因坐实，第 2、3 项根因未定，禁止凭假设动手。

---

## 1. 必读清单（按顺序，全部完整路径）

进站五份（`CLAUDE.md §1` 强制顺序）：

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`
2. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
3. `F:\soft\00selfmade\rime_claude\AGENTS.md`（首屏 §1 + 危险区 §4 + checklist §5）
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`（R1–R9 / P1–P8）
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§A 部署 + §11 NSIS 配方）
6. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md`

按需：

- 改 `output/data/**/*.yaml` 前**必读**：`F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`（重点 §5–§7、§9 本机现状含 §9.3 WeaselTSF `RELEASE_MASK` 坑、§10 常见坑）+ `F:\soft\00selfmade\rime_claude\.claude\rules\rime-settings.md`
  **本会话从头到尾没打开过它** —— 这是诉求 3 若真要改 yaml 的第一个欠账。
- 专项规则：`F:\soft\00selfmade\rime_claude\.claude\rules\{codebase-graph,ipc-boundary,build-toolchain,commit-checklist,hotkey-binding}.md`
- 事故史：`F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（本轮相关：L09 NSIS 编码、L72 `SetOverwrite try` 静默跳过、L113 四条从未生效的 Shift+标点绑定、L120 不杀 TSF 宿主、L161 spec 094、L162 MSVC 不重组宽字面量里的 `\xNN`）
- 决策史：`F:\soft\00selfmade\rime_claude\docs\adr\`（本轮新增 `0025`、`0026-*.md`、`0027-*.md`；索引在 `docs\adr\README.md`）
- 前序交接：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-01-fluxing-1.1.6-spec093-delivery.md`、`...\handoff-2026-10-31-fluxing-1.1.6-spec091-delivery.md`
- 本轮 spec 三件套：`F:\soft\00selfmade\rime_claude\.specify\specs\094-*\`、`095-quickpanel-always-show-in-settings\`、`096-focusin-focusout-flag-protocol-table\`

> `codebase-memory-mcp`（项目名 `F-soft-00selfmade-rime-claude`，12,715 节点 / 38,206 边）在本会话**父代理与 worker 子代理都实测未在 `tool_search` 命中**，只能降级 grep/read。接手时先试一次，命中就按 `.claude/rules/codebase-graph.md` 的顺序用。

---

## 2. 已完成（不要重做）

### 2.1 spec 094 · 两个装机实测 bug —— 5 条提交

| 现象 | 根因【已验证】 | 修法 |
|---|---|---|
| 切到别的输入法后快捷设置栏不消失 | `Deactivate()` 先 `EndSession()` ⇒ 通知必丢；Deactivated 分支有 `HasSession()` 门禁；`OnActivated()` 只认自身 CLSID | Deactivate 在 `EndSession` **之前** post Deactivated + 去掉门禁 + 新增共享纯函数 `DecideProfileNotify` 处理「非自身 CLSID + isActivated=TRUE ⇒ Deactivated」。**服务端不改，IPC 线格式零变化** |
| 右键删词无效 | `m_deleteCallback` 是值语义 `std::function`，注册后恒空 | 改引用绑定 |
| `Ctrl+Shift+数字` 删词无效 | `ConvertKeyEvent` 无法从字符反推数字键 | 改用 VK 码 + `GetKeyboardState` + `lParam` bit31，复用 `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE`；DELETE 回 body，客户端删词后立即 `GetResponseData` + `_UpdateUI`；`Ctrl+Shift` 的 Shift 不得原样转发，服务端 DELETE 处置把 `interveningKey` 置 true 兜底 |

提交：`3de27e6 fix(WeaselIPC)`、`28eb827 fix(WeaselUI)`、`83d3fd5 fix(WeaselTSF)`、`87b8078 test(fluxing)`、`492cc67 docs(fluxing)`

### 2.2 spec 095 / ADR 0026 · 「快捷面板常驻」开关收进统一设置面板

- **顺带修掉一个装机事故**：`WeaselServer/ShortcutSettings.cpp` 里 75 行中文是逐字节 `\xNN` 转义写的。MSVC **不会**重组宽字面量里的 `\xNN` 字节转义 ⇒ 装机后整张设置界面乱码。已还原成真 UTF-8，沉淀为 **L162**。手工修正 2 处有损字（`⟳ 恢多默` → `⟳ 恢复默认`）。
- 新共享头 `F:\soft\00selfmade\rime_claude\include\FluxingAlwaysShow.h`（UTF-8 BOM + LF，8980 B，namespace `fluxing::always_show`，header-only）：状态落盘/读盘/路径解析/观察者全部下沉到这里。关键 API：`AlwaysShowEnabled()`、`SetAlwaysShowFlag()`、`ResolveAlwaysShowFromFiles(primary, legacy, default_value)`、`ReadStateFile()`（三态 `StateReadStatus{kMissing,kEmpty,kRead}`）、`WriteStateFile()`（**写后回读自证**）、`StatePathOverrideRef()`（测试注入落点）、`SetModeChangedCallback()`/`NotifyModeChanged()`。
- `WeaselServer/QuickPanelDialog.h/.cpp`：删掉 `static Mode s_mode;`（消灭第二真源），`CurrentMode()`/`AlwaysShowMode()` 改读共享头；新增 `OnAlwaysShowModeChanged(bool)` + 文件作用域静态 registrar 注册观察者。legacy 落点（`%APPDATA%\Rime\quickpanel.state`，需 shell32）留在 QuickPanelDialog。
- `WeaselServer/ShortcutSettings.h/.cpp`：标题改「设置」（`L"⌨ " L"设置"`），副标题「快捷键与界面设置 · N 条快捷键」；新增 `kPrefsH=30`/`kPrefsY`，`kTableH_phys` 458→428，`kBtnY_phys`=606；`OnCreate` 在工具栏行后、状态行前插「界面」分组 + `BS_AUTOCHECKBOX`（`ID_CHK_ALWAYS_SHOW = 2130`）；`Hide()` 与 `OnDestroy()` 两处复位句柄；公开 `AlwaysShowChecked()` / `SetAlwaysShowChecked(bool)`（**不需建窗口**，供测试用）。
- **语义差异（已写进 CHANGELOG + ADR 0026）**：这个勾选框只改状态与持久化，**不主动 Show/Hide**；托盘菜单项是「开⇒显示／关⇒收起」。两者刻意不同。
- 运行期初值 = `false`（瞬时模式）；「文件缺失 ⇒ 默认常驻(true)」是**读盘兜底**（spec 089 / ADR 0022）。两者不可混。
- 测试：`test/TestShortcutSettings/TestShortcutSettings.cpp` 新增 5 组（T-A1 格式/解析、T-A2 三态读、T-A3 写盘回读自证、T-A4 主/旧落点优先级、T-B 生产 API 往返）；既有 `test/TestQuickPanelDialog` 断言**一行未改**仍全绿（等价性机器证据）。

提交：`ec438a4 feat(WeaselServer)`

### 2.3 spec 096 / ADR 0027 · FocusIn/FocusOut 对称位标志协议表

- `include/WeaselIPC.h`：新增 `WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED = 1u << 1`（:54）、`WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST = 1u << 1`（:76）。全局作用域协议表（插在 `DecideProfileNotify` 之后、`namespace weasel {` 之前）：`struct FocusFlagSlot{DWORD in_bit; DWORD out_bit; const char* dimension;}`、`FocusFlagProtocolTable(size_t&)`（函数内 static 数组，2 行：bit0=`ime_profile_activation`、bit1=`app_focus`）、`HasFocusInFlag`/`HasFocusOutFlag`、`enum class FocusInKind{kProfileActivated,kAppFocusGained,kUnknown}` / `FocusOutKind{...}`、`ClassifyFocusIn`/`ClassifyFocusOut`（**profile 位优先**）、`ShouldShowQuickPanelOnFocusIn`。
- `WeaselTSF/KeyEventSink.cpp:125` `OnSetFocus` 改发 bit1；`WeaselTSF/WeaselTSF.cpp` `_Reconnect()`（:484/:488）**刻意保持 0**（连接事件不是焦点事件）；`_ProfileNotifyLoop`（:408/:419）**未动**。
- `RimeWithWeasel/RimeWithWeasel.cpp:500` `FocusIn` 改用 `ClassifyFocusIn` + `ShouldShowQuickPanelOnFocusIn`；活动会话门禁与 early-return 不变；新增 `kAppFocusGained` 日志。`:544` `FocusOut` 仍用 `ShouldHideQuickPanelOnFocusOut(param, QuickPanelDialog::AlwaysShowMode())`。
- **双向安全降级**（必须写进任何汇报）：命令号与字段布局不变，只是 wParam 取值从 0 变 2 ⇒ 旧服务端只认 bit0，安全；旧客户端发 0 ⇒ `kUnknown` ⇒ 行为与今天一致。`ShouldHideQuickPanelOnFocusOut(APP_FOCUS_LOST, always_show)` 与加位前的 `(0, always_show)` 结果**完全相同**。
- `WeaselDeployer` 豁免：它没有 `RequestHandler` 实现，不消费 FocusIn/FocusOut。
- 测试：`test/TestWeaselIPC/TestWeaselIPC.cpp` 新增 `focusin_record_path()`、`TestRequestHandler::FocusIn` override（逐行落盘原始 wParam）、`verify_focusin_flags_main()`、`_tmain` 的 `/verify-focusin-flags` 分发。**踩过的坑**：`_tmain` 在 line 46 而定义在后部 ⇒ `error C3861: 找不到标识符`，已在声明区 line 18–26 补 `int verify_focusin_flags_main();`。
- `scripts/test-infra/run-test-suite.bat` 在 `/verify-focusout-flags` 之后、`/stop` 之前插入 `/verify-focusin-flags` 段。

提交：`0c9bad9 feat(WeaselIPC)`、`8bae8e9 feat(WeaselTSF)`、`d3f282c feat(RimeWithWeasel)`、`7e07072 test(fluxing)`、`44fa5b4 docs(fluxing)`

### 2.4 验证结果【已验证，`F:\soft\00selfmade\rime_claude\tmp\suite-096.log`】

```
BUILD_EXIT=0        0 个错误                        (tmp/build-096.log)
SUITE_EXIT=0        === ALL TESTS PASSED ===
TestQuickPanelRefactor: 32/32 assertions passed
Result: 49 PASS / 0 FAIL
Result: 10 PASS / 0 FAIL
Result: 12 passed, 0 failed
[TestShortcutSettings] PASS=52 FAIL=0
MAPPER-SUMMARY|failures=0
RECORDER-SUMMARY|failures=0
[FOCUSOUT] plain=[0] flagged=[1]   + PASS
[FOCUSIN]  app=[2] profile=[1]     + PASS
[DELETE]   got=1 body_status=1     + PASS
[SHIFT]    ...                     + PASS
```

### 2.5 部署现状【已验证】—— ⚠️ 接手第一件事要知道这个

用 `F:\soft\00selfmade\rime_claude\tmp\deploy_096.py` 做的**文件拷贝式部署**（`/quit` → 备份 → 覆盖 → 锁住的 DLL 用 `MoveFileExW(DELAY_UNTIL_REBOOT)` 排期 → 重启服务）。**没有跑 `build.bat installer` 走 NSIS 全流程。**

| `D:\Program Files\fluxing\weasel\` 文件 | 状态 |
|---|---|
| `WeaselServer.exe` | ✅ 已换 11/1 构建（2258944 B，11/1 11:00:29） |
| `WeaselDeployer.exe` | ✅ 已换（932352 B，11/1 11:00:37） |
| `weaselx64.dll` | ✅ 已换（1978368 B，11/1 11:00:02） |
| `weasel.dll` | ❌ **仍是旧的**（1670144 B，10/31 10:12:12）→ DEFERRED |
| `weaselx64.1.1.6.0.dll` | ❌ **仍是旧的**（1977344 B，10/31 10:12:06）→ DEFERRED |
| `FluxingPhrasesDialog.exe` | ⬜ 未部署（1095680 B / 10/31 10:06:22），但与 `output\Win32` 产物**字节一致** |

**后果**：COM `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` 的 `InprocServer32` 指向 `weaselx64.1.1.6.0.dll` ⇒ **各应用进程里加载的 TSF 仍是旧代码，重启前不生效**。spec 094 的 `Ctrl+Shift+数字` 删词、spec 096 的 bit1 都必须重启（或注销）后才能验收。这既是验收障碍，也正是诉求 1 要解决的问题。

两个被锁文件的替换已排入 `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\PendingFileRenameOperations`（实测 936 条中 weasel 相关 4 条 = 2 组源/目标对），源在 `F:\soft\00selfmade\rime_claude\tmp\deploy-stage-20261101\`（**重启前勿删**）。回滚备份在 `F:\soft\00selfmade\rime_claude\tmp\install-backup-20261101\`。

构建产物 → 装机文件映射【已验证】：
```
output\Win32\WeaselServer.exe         2258944  11/1 11:00:29  → weasel\WeaselServer.exe
output\Win32\WeaselDeployer.exe        932352  11/1 11:00:37  → weasel\WeaselDeployer.exe
output\weasel.dll                     1670144  11/1 11:00:11  → weasel\weasel.dll         (锁住 → DEFERRED)
output\weaselx64.dll                  1978368  11/1 11:00:02  → weasel\weaselx64.dll  和  weasel\weaselx64.1.1.6.0.dll
                                                                (前者 COPIED，后者锁住 → DEFERRED)
output\Win32\FluxingPhrasesDialog.exe 1095680  10/31 10:06:23 → weasel\FluxingPhrasesDialog.exe
output\FluxingPhrasesDialog.exe       1416192  10/24 22:34:31 → 另一个更旧产物，未装机（别拿它对比）
```
**产品在 `output/` 与 `output/Win32/`，不在 `Release/`**（`Release/` 只有测试 exe）。

---

## 3. 三项新诉求 · 取证结论

### 3.1 诉求 1 · 安装时能否去除 `weaselx64.1.1.6.0.dll` 占用，做到不重启生效

**根因【已验证】：versioned shim 的文件名只由产品版本决定，同一版本重复安装时必然撞上当前已被所有应用进程映射的那个 DLL。**

`F:\soft\00selfmade\rime_claude\output\install.nsi` 里**已经存在**一套免重启机制（spec 087 / ADR 0016），流程是：

1. `Rename` 旧 DLL 失败（= 被映射锁住）⇒ `DetailPrint "... is locked by a running app; installing a versioned shim instead (no reboot needed)"`（32 位在 `:632`，64 位在 `:703`）
2. `Delete "$INSTDIR\weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"`（`:709`）
3. `SetOverwrite on` + `File /nonfatal /oname=weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll "weaselx64.dll"`（`:711-712`）
4. 成功 ⇒ `StrCpy $FluxingWeaselX64Dll ...`（`:721`）+ `ExecWait '"$FluxingRegSvr32x64" /s "$FluxingWeaselX64Dll"' $0`（`:722`）**重注册 COM 指向新名字** ⇒ 免重启
5. 失败 / regsvr32 非 0 ⇒ 写 `$INSTDIR\weaselx64.dll.stage2` = `STAGE2_WEASELX64_NEEDED`（`:714-719` / `:725-727`）+ `$FluxingDegraded=1` ⇒ 走开机补换（Stage 2 段 `:800+` 注册 At-startup 启动项）⇒ **需要重启**

而 shim 名字的唯二分量在 `install.nsi:16-21`：
```
!ifndef FLUXING_VERSION
  !define FLUXING_VERSION 1.0.0
!endif
!ifndef WEASEL_BUILD
  !define WEASEL_BUILD 0
!endif
```
（`:53 VIProductVersion "${FLUXING_VERSION}.${WEASEL_BUILD}"`）**没有任何「每次安装运行唯一」的分量** —— 无时间戳、无自增序号。

⇒ 同版本重装时，第 2 步的 `Delete` 对**被映射文件静默失败**，第 3 步的 `File /oname=`（即使 `SetOverwrite on`）也失败并置错误位 ⇒ 直接落到第 5 步的 `.stage2` ⇒ 必须重启。这正是本次 11/1 遇到的情况：`weaselx64.dll` 与 `weaselx64.1.1.6.0.dll` 同为 1977344 B、同为 10/31 10:12:06，说明 10/31 那次 1.1.6 安装是成功的、COM 已指向 `1.1.6.0`；11/1 同版本重装时新 shim 名字仍算出 `1.1.6.0`，撞上本体。

装机目录的累积痕迹**反过来证明机制本身是好的**（每次版本号变化时新名字空闲 ⇒ 写入 ⇒ regsvr32 重指 ⇒ 免重启）：
```
weaselx64.1.1.6.0.dll  1977344  10/31 10:12:06   ← COM 当前指向它
weaselx64.dll          1977344  10/31 10:12:06
weaselx64.1.1.3.0.dll  1956352  10/29 21:36:36
weaselx64.1.1.2.0.dll  1956352  10/29 19:54:04
weaselx64.1.1.1.0.dll  1956352  10/29 15:04:10
weaselx64.1.1.0.0.dll  1956352  10/28 20:50:26
weaselx64.dll.old.tmp  1977344  10/30 21:37:54   ← 遗留未清理（Rename 成功过的痕迹）
```

**定性**：Windows 加载器锁下**无法真正「去除占用」** —— 既不能覆盖也不能删除已映射的 DLL；L120 已明确禁止杀 TSF 宿主（会拖垮整个 text-services 栈）。可达成的目标是**让安装对占用免疫**。

**修法（最小化，只动 `install.nsi` 两个对称分支）**：给 versioned shim 加一个「每次安装运行唯一」的序号分量 —— `Delete` 之后仍 `IfFileExists` 就递增探测 `.0/.1/.2…` 直到找到空闲名字，再用它 `File /oname=` + `regsvr32` 重注册。效果：新打开的应用立刻加载新代码；已运行的应用保持旧映射（**重开该应用即可，无需重启/注销**）—— 这正是 `:632`/`:703` 的 DetailPrint 已经承诺、但被同版本撞名打破的行为。

卸载侧**已有通配清理**，加序号不会造成残留：`install.nsi:1225 Delete "$INSTDIR\weasel.*.dll"`、`:1226 Delete "$INSTDIR\weaselx64.*.dll"`。

**改 `install.nsi` 的硬约束**（`CLAUDE.md §2` / L09 / L13 / L17 / L54）：
- 写盘必须 **UTF-8 + `EF BB BF` BOM + CRLF**
- silent-install 必须 `/D=<path>` 显式，且先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing`
- **任何 NSIS 改动先跑 `makensis` 当门禁**（NSIS `File /oname=` 才是改名语法；`File "src" "dst"` 会把两个路径都当源文件 ⇒ `no files found.` 并中止打包。`SetOverwrite try` 静默跳过且不置错误位，**不能**用它判断目标是否被占用；要判断用 `Rename` 成败，或 `Delete` + `SetOverwrite on` + `File /nonfatal`）
- `install.nsi` **是手工维护、直接提交的文件**（`git log -- output/install.nsi` → `8cbe9ee fix(installer): 装机更新不再需要重启`、`5556d31`、`ceff6b6`、`c55abfa`、`60ed0a4`）。仓库**不存在 `installer/` 目录**，也没有任何生成器脚本。前序会话把它列为禁改区是**错的**，它可以改。

其它相关锚点：`:597-604` L120 清 bookkeeping（`WeaselServer.exe.stage2` / `weaselx64.dll.stage2` / 三个 `.old.tmp`）；`:606-616` `weasel.dll` 的 Rename-then-File；`:618-632` spec 087 fix 2（含 `:624-631` L120 注释）；`:634-659` **32 位镜像分支**（`:642` Delete、`:644-645` File、`:647-649` degraded、`:651-656` regsvr32、`:658` Goto weasel_done）；`:660-672` Rename 成功分支；`:685-687` `weaselx64.dll` Rename；`:734-742` 64 位 Rename 成功分支 + `weaselx64_done:`；`:744-745` `${If} ${IsNativeARM64}` / `weaselARM.dll`；`:754-788` `WeaselServer.exe`（`:782` locked 则 scheduled delete on reboot）；`:816-831` `WeaselDeployer.exe`；`:844-859` `WeaselSetup.exe`；`:869-882` 末尾强删 `.old.tmp`（`:878`）。

**⚠️ 验收方式**：必须真的跑 `build.bat installer` + 执行 NSIS 产物，**不能用文件拷贝**。本轮正是因为用了拷贝式部署，才没暴露/没走到安装器的 shim 机制。

### 3.2 诉求 2 · 快捷设置栏 → 常用短语 → 双击目标短语不上屏（选中后回车可以）

**先排除部署陈旧【已验证】**：装机 `FluxingPhrasesDialog.exe` = 1095680 B @ 10/31 10:06:22，与 `output\Win32\FluxingPhrasesDialog.exe`（1095680 B @ 10/31 10:06:23）一致 ⇒ **bug 在代码里，不是装错了版本**。

**代码路径已完整定位【已验证】**（`F:\soft\00selfmade\rime_claude\FluxingPhrasesDialog\PhrasesDialog.cpp`，67615 B；`PhrasesDialog.h`，12634 B）：

`OnNotify` 是 **static**（`PhrasesDialog.h:261`），由 `WM_NOTIFY` 直接转发（`:489-490 case WM_NOTIFY: return OnNotify(hwnd, lp);`）。三条注入路径**主体逻辑字面相同**：都是 `HideWithoutDisconnect()` → `s_pipeClient->SendMessage(fluxing::BuildINJECT(idx))` → `s_pipeClient->ReadMessage()` → `Hide()`；非 pipe 模式则 `Hide()` + `InjectText(text)`。

| 路径 | 位置 | 差别 |
|---|---|---|
| `NM_DBLCLK` | `:1082-1108` | 索引来自 `pia->iItem` |
| `NM_RETURN` | `:1109-1127` | 索引来自 `pia->iItem` |
| `VK_RETURN`（**真正工作的那条**） | `:925-962` | 索引用 `m_selectedIndex`；额外要求 `GetFocus() == s_hList`（`:941`）；焦点在 `s_hInput` 时 Enter 改走 `ID_BTN_EDIT`/`ID_BTN_ADD_TOP`（`:932-939`） |

其它锚点：`HideWithoutDisconnect()` 在 `:348`（内部 `DestroyWindow(s_hwnd)` `:351`；`:362` 注释「不 Disconnect pipe — 让 NM_DBLCLK 等后续 INJECT 能 send + read ACK」）；`Hide()` 在 `:319`（`DestroyWindow` `:322`）；`InjectText` 在 `:458`；ListView 样式 `:678 LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS`、`:708 LVS_EX_FULLROWSELECT`，**无 `WS_EX_NOACTIVATE`**，`WS_EX_LAYERED` 已在 v0.19.0.33 删除（`:204-207` 注释）；`NM_CLICK`（`:1128-1138`）设 `m_selectedIndex = pia->iItem` 并把文本填进 `s_hInput`；`LVN_ITEMCHANGED`（`:1057-1081`）选中时同步、失选时置 `-1` 并清空 input；`MoveSelection` 尾部手工同步（`:1010-1017`，注释说明 `LVN_ITEMCHANGED` 异步可能晚于同帧 Enter）。

服务端 `F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialogIPC.cpp:245-272 case MT_INJECT`：用**服务端自己的 `s_phrases`** 做边界检查（`msg.id < (int)s_phrases.size()`）→ `fluxing::foreground_restore::GetHwnd()`（`:261`）→ `IsWindow` 则 best-effort `SetForegroundWindow(target)`（`:266`；v0.19.0.55 Phase K3 T019 Bug 1 起**不再 AttachThreadInput**，因 Win11 22H2+ foreground 限制极严、实测仍可能拒）→ `InjectText(text)`（`:268`）→ `PipeSend(hPipe, BuildACK())`（`:270`）。设计注释 `:248-260` 明写「client NM_DBLCLK / NM_RETURN / VK_RETURN 全部 **Hide 先于 INJECT**，dialog DestroyWindow 后 foreground 自动归位 user app」。

**领先假设【推断，未取证，禁止直接采信】**：双击路径执行 `DestroyWindow` 时 **LBUTTON 仍处于按下状态**，而键驱动的 Enter 路径没有 ⇒ 销毁窗口后的 foreground 归位、以及服务端 `SetForegroundWindow` + `SendInput` 的落点与 Enter 路径不同（`SetForegroundWindow` 在鼠标键按下 / 调用进程非前台时会被 Windows 拒绝）。

**判别实验（做这个，别改代码）**：
1. 在 `NM_DBLCLK` 分支入口与 `SendMessage(BuildINJECT)` 前后加落盘日志，与 Enter 路径逐行对比 —— 先确认到底是「客户端没发出 INJECT」还是「服务端收到但注入落空」。
2. 或临时在 `HideWithoutDisconnect()` 之后轮询 `GetAsyncKeyState(VK_LBUTTON)` 等到抬起再发 INJECT，看是否恢复。**这只是探针，不是修复方案**；确认后再决定正式修法。

可正常工作的对照样板：`WeaselServer\ShortcutSettings.cpp:1274 if (nmhdr->code == NM_CLICK || nmhdr->code == NM_DBLCLK) {`。
源码历史：`12b450e v0.19.0.58 (Phase K5 Bug 3+4 真修 hotfix) — input 子控件 Enter/Esc bubble via subclass`、`2356f71 v0.19.0.57`、`f518c42 v0.19.0.55 (NM_DBLCLK Hide 先于 INJECT + pipe handle 泄漏 + Option B 简化)`、`36d646b refactor out-of-process PhrasesDialog IPC`。

### 3.3 诉求 3 · 中文输入法下仍是英文标点，标点不随中英文切换（用户说是「新出现的 bug」）

#### 【已证伪】死路 1：`punctuator/half_shape` 映射被改坏

编译产物 `D:\Program Files\fluxing\user1\fluxing\build\double_pinyin_sogou.schema.yaml`（50972 B，**11/01 10:06:40 今天新编译**）第 **177–209** 行映射**完全正确、全是全角**：`",": "，"`、`.: "。"`、`"/": "、"`、`":": "："`、`";": "；"`、`"<": "《"`、`"?": "？"`、`"[": "【"`、`"\\": "、"`、`"]": "】"`、`"^": "……"`、`_: "——"`、`` "`": "·" ``、`"{": "「"`、`"}": "」"`、`"!": "！"`、`"(": "（"`、`")": "）"`、`"\"": {pair: ["“","”"]}`、`"$": "¥"`；`# % & * + - = / @ | ~` 保持半角。

⇒ `double_pinyin_sogou.custom.yaml` 里「重述 `__include: default:/punctuator/half_shape` + 8 条覆盖」的 patch **确实合并成功了**（librime `kPatch(2) > kInclude(1)` 陷阱已被正确规避）。**标点映射表不是根因，不要再查这条。**

编译产物锚点行号【已验证】：
```
build\double_pinyin_sogou.schema.yaml   punctuator:141  full_shape:143  half_shape:177  symbols:210  switches:762
build\rime_ice.schema.yaml              punctuator:148  full_shape:150  half_shape:184  symbols:217  switches:866
build\default.yaml                      punctuator:54   full_shape:56   half_shape:90
build\weasel.yaml                       inline_preedit: L8 和 L524     style: L509
```

#### 【已证伪】死路 2：`style/inline_preedit: true`

本轮一度认定 `output/data/weasel.yaml` 的 `style/inline_preedit: true` 是根因，理由是「WeaselTSF 没有实现 ITfCompositionSink」。**这条推理是错的，已撤回**：

- `F:\soft\00selfmade\rime_claude\WeaselTSF\Composition.cpp:192-223` 确实有 `_HandleComposition` 并对 range 设 `TF_IPP_FLAG_ACTIVE`，即 composition 路径是存在的；
- 更根本的是，**标点转换发生在 librime 的 punctuator 内部**（`RimeProcessKey` 链路），`inline_preedit` 只决定预编辑串**显示在哪里**（宿主内联 vs 独立候选窗），它不可能让 librime 吐出半角标点。

⇒ **不要改 `inline_preedit`**。补丁脚本 `tmp\fix_inline_preedit.py` 的自检守卫已拦下，**两个 yaml 都是原样未改**（`RESULT=ambiguous-anchor` / `missing`，日志 `tmp\fix-inline-preedit.log`）。

顺带纠正一个文件身份错误【已验证】：仓库根的 `weasel.default.yaml` **不存在**（`Test-Path` = MISSING，全仓 yaml 里也搜不到 `v0.22.0.8` 字样）。真正的配置源是 `F:\soft\00selfmade\rime_claude\output\data\weasel.yaml`（27269 B），里面 `inline_preedit` 有**两处**：
- `:22` 在 `app_options/firefox.exe/` 下 —— 是规避 <https://github.com/rime/weasel/issues/946> 的**刻意设置，绝对不要动**
- `:66` 在 `style/` 下 —— 全局默认

装机侧生效链路【已验证】：
```
repo  output\data\weasel.yaml                     27269 B   (源)
  →   D:\Program Files\fluxing\weasel\data\weasel.yaml      27269 B  10-29 11:07
  →   D:\Program Files\fluxing\user1\fluxing\build\weasel.yaml  17991 B  11-01 00:41  ← librime 编译产物，运行时真正读它
      (合并了 D:\Program Files\fluxing\user1\fluxing\weasel.custom.yaml，264 B，只 patch 了 style/color_scheme: flypy)
```
**`D:\Program Files\fluxing\user1\fluxing\weasel.yaml` 不存在** —— 用户目录只有 `weasel.custom.yaml`，所以没有用户级覆盖。

#### 【剩余两条待判】根因未定

**假设 A（更可能）**：`ascii_punct` 开关处于 ON。此时 librime 绕过 punctuator 直接吐 ASCII，表现恰好是「中文能出候选字、但标点是半角，且不随中英切换」。

支持证据【已验证】：
- `output\data\default.yaml:44` 把 `ascii_punct` 列进 `switcher/save_options`（同列表还有 `traditionalization`/`emoji`/`full_shape`/`search_single_char`，`:43-48`）⇒ 状态被记忆、跨会话跨方案保留
- 各 schema 里 `ascii_punct` 定义为 `- name: ascii_punct  # 中英标点` + `states: [ ¥, $ ]`，**且没有 `reset`**（`rime_ice.schema.yaml:27`、`double_pinyin*.schema.yaml:38`；同列表中只有 `emoji` 带 `reset: 1`）
- `output\data\default.yaml:265` = `- { when: always, toggle: ascii_punct, accept: Control+Shift+numbersign }  # 切换中英标点` ⇒ **Ctrl+Shift+3** 会切它，很容易误触
- `output\data\default.yaml:39` 注释明写「开关记忆…**需要记忆的开关不能设定 reset**」⇒ 「加 reset」这个直觉修法是**被项目自己禁止的**，不能照做

反证 / 缺口【已验证】：
- `D:\Program Files\fluxing\user1\fluxing\user.yaml`（146 B，11/01 10:06:41）**全文只有**：
  ```yaml
  var:
    last_build_time: 1793498801
    previously_selected_schema: double_pinyin_sogou
    schema_access_time:
      double_pinyin_sogou: 1785946576
  ```
  **没有任何 `ascii_punct` 持久化状态** ⇒ 若 `save_options` 的记忆生效，它应该出现在这里。它没出现，可能是 (a) 状态根本没被 ON 过，(b) ON 了但 WeaselServer 非正常退出没落盘，(c) 落盘位置不在 `user.yaml`。三者未区分。
- **`build\double_pinyin_sogou.schema.yaml:762+` 的 `switches` 块从未读过** —— 这是最大的证据缺口。必须先读它确认 `ascii_punct` 的实际 `reset` 与默认 state。

**决定性实验（一步定生死，先做这个）**：在中文模式下按 **Ctrl+Shift+3**（`Control+Shift+numbersign`），看标点是否立刻变全角。
- 变了 ⇒ 假设 A 成立，`ascii_punct` 被卡在 ON。修法是**找出谁把它打开的**（重点查 spec 077 的裸 Shift IPC 链路），而不是加 `reset`。
- 没变 ⇒ 假设 A 排除，转假设 B。

**假设 B**：spec 077 的「裸 Shift 走 IPC 切中英」链路让 TSF/服务端认定的中英状态与 librime 实际 `ascii_mode`/`ascii_punct` **失步**。相关提交【已验证】：`258e708 feat(fluxing): bare Shift toggles Chinese/English when no candidate menu is open`、`4b46315 fix(fluxing): 裸 Shift 左右判定改为扫描码优先并加可配置互换开关`。**这两条是「新出现」最可疑的时间点。**

`output/data` 近期提交全在 Shift / 标点方向【已验证】：`4b46315`、`258e708`、`ea0a84c fix(fluxing): rename the four never-firing Shift+punctuation bindings`、`c45167d fix(fluxing): delete the two never-firing duplicate hotkey chords`、`47f2452 fix(fluxing): drop the 4 never-effective shift-punctuation bindings from all three custom.yaml`、`4096df2 feat(rime_ice): remove Shift+Shift_L/R send 2/3 bindings from default.yaml:240-241 (spec 077 IPC path takes over)`。`output/data/user-custom` 近期提交：`ea0a84c`、`47f2452`、`559ab9e`。

**⚠️ 当前活动方案是 `double_pinyin_sogou`，不是 `rime_ice`**【已验证，见上面 user.yaml】。查任何配置都要对着 `double_pinyin_sogou` 的编译产物查。

#### 改 yaml 前的硬约束

- **只改 `<名字>.custom.yaml` 的 `patch:`**；直接改发行版 `.yaml` 会被升级覆盖
- 一个文件只能有**一个** `patch:` 键（重复写静默覆盖前一份）
- patch 路径是 DSL：`/` 分层、`@0/@last/@before N/@after N/@next` 选列表项、`/+` 合并、`/=` 替换；含特殊字符的路径用单引号
- patch 前先确认源节点是不是 `__include` 引入的（`kPatch > kInclude` ⇒ 不重述 `__include` 会**静默失败**）。本项目三份 custom.yaml 都已正确重述，编译产物已验证合并成功
- 改完**必须重新部署**（运行时读 `build/`，不读源文件）
- 校验器：`python F:\soft\00selfmade\rime_claude\scripts\validate_custom_yaml.py <file>` 必须 0 error
- **反模式**：「在 `punctuator` 里指望 Shift 生效」—— punctuator 用 unshifted ASCII 作映射键，对 Shift 不感知；Shift+标点必须 `key_binder` 拦截
- **危险信号**：换方案后繁简/中英状态莫名变了 ⇒ 开关缺 `reset`；部署日志 `invalid key binding #N, with invalid accept pattern` ⇒ 键名大小写/拼写错或 `bindings` 写成 list 而非 list of map；`WARNING` 刷屏但功能正常 ⇒ `accept` 模式永不命中
- 验证清单：改动落在 `.custom.yaml` 的 `patch:` 下且全文件只有一个 `patch:` → 校验器通过 → 已重新部署且日志无 `E` 开头行 → `build/<name>.schema.yaml` 里能看到预期合并结果 → 现场实际敲一遍复现 → 没误伤其他方案 → 改了发行版默认行为的已落文档
- patch 层文件：`output\data\user-custom\double_pinyin_sogou.custom.yaml`（3959 B，已读 1–60；`:10 patch:`、`:12-21 punctuator/half_shape`、`:27-57` ADR 0009 / L113 / librime 1.13 限制的长注释、`:60 key_binder/bindings/+`、**61-65 未读**）、`rime_ice.custom.yaml`（5696 B，**正文未读**）、`radical_pinyin.custom.yaml`（3368 B，`:4` 注明 radical_pinyin 无 punctuator 段）
- `output\data\default.yaml:290` 是**已被注释掉**的 `# - { when: always, accept: Control+period, toggle: ascii_punct }`

---

## 4. 下阶段计划（建议顺序）

1. **先做 3.3 的决定性实验**（中文模式按 Ctrl+Shift+3）+ 读 `build\double_pinyin_sogou.schema.yaml:762+` 的 `switches` 块 + 读 `double_pinyin_sogou.custom.yaml:60-65`。这三件事**零风险、一次取齐**，能直接判掉假设 A/B。改 yaml 前先读 `Rime-设置说明书-重点快捷键.md`。
2. **3.2 的落盘日志判别实验**（先定位「客户端没发」还是「服务端注入落空」，再动手改）。
3. **3.1 的 install.nsi shim 序号**（根因已坐实，可直接实现；改完先跑 `makensis` 门禁，再用 `build.bat installer` + 真跑安装器验收，**不要文件拷贝**）。
4. 每项按宪法走：改动 ≥3 文件 ⇒ `.specify/specs/NNN-<kebab>/{spec,plan,tasks}.md` 三件套（下一个编号是 **097**）；架构决策 ⇒ `docs/adr/NNNN-*.md`（下一个是 **0028**）+ `docs/adr/README.md` 补索引；事故/bugfix ⇒ `.specify/memory/lessons-learned.md` 的 `L##`（下一个是 **L163**）；用户可感知变化 ⇒ `CHANGELOG.md` 的 `[Unreleased]`。
5. 收尾：`build.bat weasel` → `scripts/test-infra/run-test-suite.bat` 全绿 → 分模块小提交 → 部署 → R6 汇报（贴测试输出 + 真机人工验证步骤）。

---

## 5. 环境与命令速查（都是本会话实测成功过的形态）

环境：Windows 10.0.26300 x64；PowerShell 7.6.9；Boost `F:\b183`；MSBuild `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`（套件脚本自有 `:discover_msbuild` 优先 2022）；NSIS `C:\Program Files (x86)\NSIS\`；xmake `F:\soft\08tools\xmake\xmake.exe`（**发布禁用**）；Python `C:\Python314\python.exe`；装机目录 `D:\Program Files\fluxing\weasel\`；Rime 用户数据 `D:\Program Files\fluxing\user1\fluxing\`。

```powershell
# 构建（bash 里不要 cd，工作目录已是仓库根；产物在 output/ 与 output/Win32/）
cmd /c "build.bat weasel > tmp\build.log 2>&1"
cmd /c "build.bat weasel installer > tmp\build-installer.log 2>&1"

# 全量测试套件（必须先停装机服务；BOOST 断言在 Release 返回负数 ⇒ 一律 if !errorlevel! NEQ 0）
& 'D:\Program Files\fluxing\weasel\WeaselServer.exe' /quit
cmd /c "scripts\test-infra\run-test-suite.bat > tmp\suite.log 2>&1"
Get-Content -LiteralPath 'F:\soft\00selfmade\rime_claude\tmp\suite.log' -Tail 55
Start-Process -FilePath 'D:\Program Files\fluxing\weasel\WeaselServer.exe' -WorkingDirectory 'D:\Program Files\fluxing\weasel'

# git 复核（提交标题核验必须走 Python ascii()，GBK 控制台会显示乱码）
git -C 'F:\soft\00selfmade\rime_claude' status -sb
git -C 'F:\soft\00selfmade\rime_claude' log --oneline -8
git -C 'F:\soft\00selfmade\rime_claude' diff --stat

# 可恢复删除：必须是「单条顶层 rm --」+ 字面量路径，不能与别的语句混在一条命令里
rm -- 'F:\soft\00selfmade\rime_claude\path\a' 'F:\soft\00selfmade\rime_claude\path\b'
```

### 坑（都踩过）

- **控制台是 GBK**：打印中文会乱码 / `UnicodeEncodeError` ⇒ 诊断脚本一律 `ascii()`/`repr`/ASCII-only 输出；要看中文就**落 UTF-8 文件再用 `read` 工具读**（`Get-Content` 打中文到管道同样会花）
- **改文本必须保留原行尾与 BOM**。`Path.write_text` 会把 LF 转成 CRLF 且可能写成 0 字节 ⇒ **写 bytes 或 `newline=""`**，写完**回读断言 `size > 0`**。`.rc` = UTF-16LE+BOM；`.cmd`/`.bat` = CRLF；新共享头 = UTF-8 BOM + LF
- **不要在 str 字面量里用 `\xNN` 拼中文**：`\xe5\x80\x99` 在 str 里是三个码点 U+00E5 U+0080 U+0099，再 `.encode('utf-8')` 就是 mojibake。这正是 L162 那类事故。要么全用真实中文字符，要么全程在 bytes 空间
- **`edit` 工具对同一文件多次替换必须串行**
- **bash 禁 `Push-Location`/`Pop-Location`，bash 里 `cd` 被拦截**
- **正则里别写 `\\(` 开头**：本地硬安全策略会把它误判成 UNC 网络路径并拒绝执行。用 `-notlike '*tmp*'` 这类通配代替
- **`WeaselServer.exe` 是 PPL，不可 `taskkill`**，必须 `WeaselServer.exe /quit`；跑 `TestWeaselIPC` 前必须停服务
- **`web_search` 在本会话连续失败，不可用**
- **`tmp/patch_095.py` / `tmp/patch_096.py` / `tmp/commits_096.py` / `tmp/deploy_096.py` 都只能跑一次**（锚点已消耗 / 提交会 STAGED SET MISMATCH / `PendingFileRenameOperations` 会重复写入）
- **编码还原的逐字面量字节比对有 17 处假阳性**，勿重试；用组级校验器 `tmp\verify95b.py`（输出 `VERIFY-PASS (selftest=ok)`）
- **子代理可能卡死**：本轮一个 worker 跑了 40+ 分钟只产出 2 行枚举常量、零输出，已 `task_stop` 取消、父会话自行接手。派活后要盯 `task_output`

### 行尾/BOM 实测基线（`ReadAllBytes` 核过）

`include/WeaselIPC.h`=BOM+CRLF；`WeaselTSF/KeyEventSink.cpp`=无BOM+LF；`WeaselTSF/WeaselTSF.cpp`=BOM+CRLF；`RimeWithWeasel/RimeWithWeasel.cpp`=无BOM+CRLF；`test/TestWeaselIPC/TestWeaselIPC.cpp`=BOM+CRLF；`scripts/test-infra/run-test-suite.bat`=无BOM+CRLF；`docs/adr/README.md`=无BOM+LF；`CHANGELOG.md`=无BOM+CRLF；`.specify/memory/lessons-learned.md`=BOM+CRLF；ADR 0024–0027 与 spec 095/096 的 spec.md=无BOM+LF；`output/data/weasel.yaml`=无BOM+CRLF（27269 B / 691 CRLF）。

### 临时文件（gitignored，勿提交）

`tmp\fix_inline_preedit.py`（**未生效，守卫拦下**）、`tmp\fix-inline-preedit.log`、`tmp\patch_095.py`、`tmp\095-backup\`、`tmp\verify95b.py`、`tmp\patch_096.py`、`tmp\commits_096.py`、`tmp\commit-095-1.txt`、`tmp\commit-096-1..5.txt`、`tmp\deploy_096.py`、`tmp\deploy-stage-20261101\`（**含 2 个待换名 DLL，重启前勿删**）、`tmp\install-backup-20261101\`（回滚备份）、`tmp\build-095.log`、`tmp\build-096.log`、`tmp\suite-096.log`。

另：装机目录存在嵌套残留 `D:\Program Files\fluxing\weasel\fluxing\weasel\WeaselServer.exe`（0.21.0.0）以及 `\weasel\fluxing\user1\fluxing\build\weasel.yaml`（17928 B，10-23）—— 前序决定**只记录不清理**。

### 压缩前完整历史

`E:\Users\Duanyi\.minimax\.minimax\v2\sessions\2026\11\01\01-14-03-043-session_bXZzX2Q4YWVlMDJjYzdjMDQ4YmI4ZDRmY2I3MzdmMjRiOGU3\snapshots`（取 `g` 最大者）；超大工具输出在同目录 `reports\tool-outputs`。

> `E:\Users\Duanyi\.minimax\.minimax\maintenance\task-state\LATEST.md` 是**自动生成卡，当前被另一个会话** `mvs_51b8f568125d4cdea361fc183847b75f`（winget PR #467381 自查）**占用，不是本任务的卡，勿覆盖、勿采信**。本任务的交接以本文件为准。
