# 批次 W · 托盘「中英」图标（语言栏项）彻底不注册（v14）

- 日期：2026-11-10
- 分支：`Fluxing`（跟踪 `kizemo/Fluxing`）
- 上游批次：批次 V（commit `bac34e7`，面板位置锚点 ADR 0051 + 方向 A 主题 ADR 0052）
- 相关 ADR：**ADR 0053 v2/v14（现行方案）**；ADR 0044（已标 `Superseded by 0053`）
- 当前状态：**代码 / 单测 / x64 构建 / 安装包 / 静默装机 / 真机探针 全部完成并验证成立；文档已落盘；尚未 commit**

---

## 1. 用户诉求（原话）

> 之前去除的托盘 "中英"图标，又出现了，需要去除。

诉求只有一条：**通知区域里不要再出现本输入法的语言栏项**。

---

## 2. 根因：两条既往方案都被真机证伪

「此前已去除」这件事本身是真的 —— 批次 L 曾据此判定「已生效」，但那是**假阴性**：当时火流猩不是活动 IME，语言栏项根本没被创建，于是「找不到」被误读成「已摘掉」。换一个真激活火流猩的宿主后，图标一直在。

| # | 方案 | 出处 | 真机结果 | 判死理由 |
|---|---|---|---|---|
| 1 | 摘掉 `TF_LBI_STYLE_SHOWNINTRAY` | ADR 0044 | 图标仍在 | 微软 `TF_LBI_STYLE_*` 文档明写「目前不支援此标志」⇒ 摘掉是 **no-op**，从来没生效过 |
| 2 | `TF_LBI_STYLE_HIDDENSTATUSCONTROL` + 恒挂 `TF_LBI_STATUS_HIDDEN` | ADR 0053 v1 | 图标仍在 | 前提「新 DLL 已加载 + explorer 已重启 + 火流猩为活动 IME」三条全部满足的情况下，双证（UIA 明细 + DPI-aware 抓屏）仍能看到该项 |

补充判死：`Show(BOOL)` 里调 `SetLangbarStatus` —— `Show` 是**系统通知回调**，不是隐藏入口，调它不会隐藏任何东西。

---

## 3. 现行方案（v14）：不注册语言栏项

### 3.1 单一真相源

`include/FluxingLangBarStyle.h`（新增，55 行）：

- `kLangBarItemStyle`、`kLangBarStatusHidden`、`kLangBarItemForceHidden` 三者**保留**，作为回退路径的常量载体；
- 文件末尾 `const bool kLangBarRegisterItem = false;` 是 **v14 总开关**；
- 文件头注释记录了 ADR 0044 与 ADR 0053-v1 双双被证伪的事实，避免后人再走一遍。

### 3.2 改动点（`WeaselTSF/LanguageBar.cpp`）

| 位置 | 改动 |
|---|---|
| `_InitLanguageBar()` L437-438 | `if (!fluxing::kLangBarRegisterItem) return TRUE;` —— 位于 `new CLangBarItemButton`（L446）与 `AddItem`（L450）**之前** |
| 原注册代码 L440-462 | **原样保留**（含 L459 `Show(FALSE)`），仅在开关为 `true` 时执行 |
| `_UninitLanguageBar()` L468 | `_pLangBarButton == NULL` 直接 `return` |
| `_UpdateLanguageBar()` L478+ | compartment 读改写**解耦为无条件执行**，不再依赖 `_pLangBarButton` 是否为空 |

### 3.3 ★ 最容易踩的坑：`return TRUE` 不是 `return FALSE`

`_InitLanguageBar()` 的返回值直接决定 `ActivateEx` 的走向：

```text
_InitLanguageBar() 返回 FALSE
  → ActivateEx 走 goto ExitError
  → Deactivate()
  → 文本服务失活 ⇒ 中文输入整体死掉（输入法在，但打不出字）
```

所以「跳过注册」必须 **`return TRUE`**（= 初始化成功，只是没建项）。这一条写进了 ADR 0053 的 Decision 铁律与单测 T8 的意图注释里。

---

## 4. 验证（全部实测，非推断）

### 4.1 单测

- 构建：`tmp\build-test-langbar.cmd` → `MSBUILD_TEST_EXIT=0`
- 运行：`Release\TestFluxingComponents.exe` → `RUN_EXIT=0`
- 结果：`TestFluxingLangBar 8/8 PASS`（T8 = `langbar item is not registered (v14 force-absent)`）、`TestFluxingInstallPaths 19/19 PASS`、`TestFluxingComponents 6/6 PASS`
- 日志：`tmp\test-langbar-out.txt`

### 4.2 构建 + 装机

- `tmp\build-x64-langbar.cmd` → `X64_EXIT=0`
  - **注意**：`xbuild.bat weasel` 按设计只编 x86（librime 是 Win32-only），本次改动落在 **x64 TSF 前端**，必须显式跑 x64 链（教训 L210）。
- 产物：`output\weaselx64.dll`，mtime `2026-11-10 13:05:08`，SHA256 `415FE7C5C348D2D9C696F8CC826516D64489201DF9F22D134863895BAE066121`
- 安装包：后台任务 `bg_e19f5b01-176b-44b2-8308-60fcc63c0950` → succeeded
- 静默装机：`scripts\install\silent-install.ps1 'D:\Program Files\fluxing'` → `==== RESULT: ALL HARD CHECKS PASSED ====`、`INSTALL_EXIT = 0`
  - C1 data 一致、C2 无 stage2、C3 shim 28 个、C3b TIP 指向版本化 shim、C4 Run 项 1 个

### 4.3 真机判据（决定性探针）

`tmp\probe-v14-explorer2.ps1` → 日志 `tmp\probe-v14-explorer2.log`：

| 判据 | 实测 | 含义 |
|---|---|---|
| 前台窗口 | explorer 文件夹窗口（pid 61200，class `CabinetWClass`） | 是合法宿主 |
| 该进程映射的 `weaselx64*` | `D:\Program Files\fluxing\weasel\weaselx64.1.2.1.3.0.dll`，SHA256 = `415FE7C5…6121` | **等于构建产物** ⇒ 跑的是新镜像 |
| `FOUND_FLUXING_ACTIVE` | `True`（ROUND=2 才切到「火流猩输入法」） | 本输入法真的是活动 IME |
| `INPUT_INDICATOR_COUNT` | `1`（基线为 `2`） | 少了一个输入指示器项 |
| `OUR_TOOLTIP_HIT` | `0` | tooltip「左键切换模式，右键打开菜单」0 命中 |
| `VERDICT` | `PASS_VALID_PRECONDITION` | **成立** |

★ 探针本身必须自证「前提有效」才允许下结论：**新镜像 + 本输入法真激活**，缺一条就只能判 `INCONCLUSIVE`。

### 4.4 源码侧封死替代来源

- 全仓 `左键切换模式` 仅 2 处：`WeaselTSF/LanguageBar.cpp:162`（`SysAllocString(L"左键切换模式，右键打开菜单")`）与 `include/FluxingLangBarStyle.h:10`（注释）；
- 全仓 `AddItem(` 唯一调用点 `LanguageBar.cpp:450`，位于 v14 早退之后。

⇒ 图标不可能来自其它代码路径，Windows 自带的红猿头输入指示器不在本仓库范围内。

### 4.5 前三次 FAIL / INCONCLUSIVE 都是假阴性（勿重试）

| 日志 | 假阴性原因 |
|---|---|
| `tmp\install-and-probe-v14.log` | 宿主进程持的是**旧 DLL**（装机时被占用，覆盖失败） |
| `tmp\probe-v14-rerun.log` | `notepad` 是 Store 应用，`MainWindowHandle == 0` |
| `tmp\probe-v14-explorer.log` | `[Wu2.Native]` 类型解析失败 ⇒ 所有 P/Invoke 变空操作，`FOREGROUND_PID=0` |
| `tmp\probe-v14-freshhost.log` | `charmap.exe` 聚焦成功但**吞掉 `Win+Space`** ⇒ `INCONCLUSIVE_IME_NOT_ACTIVE` |

**宿主选择学**：`explorer` 的 `CabinetWClass` 文件夹窗口是唯一同时满足「能加载新镜像」+「`Win+Space` 有效」的宿主。

---

## 5. 代价（已落实，需用户知情）

v14 生效后，**语言栏项整体不存在**，因此以下入口全部消失：

| 入口 | 状态 |
|---|---|
| 左键点语言栏项 → 打开快捷设置栏 | ❌ 无（但 `Alt+,` 仍可打开，见下） |
| 右键菜单 → 重启算法服务 | ❌ 无 |
| 右键菜单 → 程序文件夹 | ❌ 无 |
| 右键菜单 → 快捷键设置 | ❌ 无（批次 U 已按用户指示从面板摘掉该键帽） |
| 右键菜单 → 用户登录（关于·账号说明） | ❌ 无（批次 U 同上已摘掉） |

**补偿入口（已核实存在）**：`WeaselServer/WeaselServerImpl.cpp:81`

```cpp
::RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA);
```

⇒ **`Alt+,` 仍能打开快捷设置栏**。

**回退杠杆**：把 `include/FluxingLangBarStyle.h` 末尾的 `kLangBarRegisterItem` 置 `true` 重新编译 x64 链，即可连带图标一起恢复全部入口。

**明确否决**：不重新打开 `WeaselServer` 托盘图标作为补偿（`output/data/weasel.yaml:81 display_tray_icon: false` 是用户主动关掉的，恢复等于违背用户意图）。

---

## 6. 本轮工具坑（复用价值）

1. **判定「修复已上机」的五元组**（缺一不可）：
   ① 注册表 `InprocServer32` 指向的**版本化 shim** 的 SHA256 == 构建产物；
   ② 宿主进程 `StartTime` 晚于该 DLL 落地时间；
   ③ 拿到的是**模块完整路径**（不是裸文件名）；
   ④ **前台窗口的宿主进程**必须正是加载新镜像的那个；
   ⑤ 该宿主真的激活了本输入法。
2. **同版本号不同内容**：本轮**没有**递增 `env.bat` 的 `WEASEL_BUILD`（仍为 `3`），于是产生了 `weaselx64.1.2.1.3.0.dll` 这个新版本化 shim 文件 —— 副作用反而是**规避了文件锁**（旧进程仍持旧文件，新宿主用新文件）。
3. **`Add-Type` 的 C# 类名必须唯一**（`Fg2` / `FgFocus`）。类名撞车会让「找不到类型」**静默**把全部 P/Invoke 变成空操作 → 探针输出一堆 `0`，看着像功能没生效。
4. **`charmap.exe` 会吞 `Win+Space`**；Store 版 `notepad` 取不到 `MainWindowHandle`。两者都不能当 TSF 宿主。
5. **否定证据不能反推「无改动」**：本轮第一轮把「装机 `weaselx64.dll` 未变」误读成「该模块无改动」，实际是**构建链没编到那份二进制**（教训 L210 同源）。

---

## 7. 改动的文件清单

### 未提交（本批次，待 commit）

| 文件 | 性质 |
|---|---|
| `include/FluxingLangBarStyle.h` | 新增（55 行，样式常量 + `kLangBarRegisterItem` 总开关） |
| `WeaselTSF/LanguageBar.cpp` | 修改（L437-438 早退 `return TRUE`；`_UninitLanguageBar` 空判；`_UpdateLanguageBar` 解耦） |
| `test/TestFluxingComponents/TestFluxingLangBar.cpp` | 新增（T1–T8） |
| `test/TestFluxingComponents/TestFluxingComponents.vcxproj` | 接线（含 219 行重排） |
| `test/TestFluxingComponents/TestFluxingMain.cpp` | 接线 |
| `docs/adr/0053-langbar-item-permanently-hidden.md` | 新增（v2/v14 整体重写版，8531 B） |
| `docs/adr/0044-*.md` | 标注 `Superseded by 0053` |
| `docs/adr/README.md` | 索引补 0053 |
| `docs/archive/handoffs/handoff-2026-11-10-batch-v-*.md` | 回填批次 V 真机复验结论 |
| `CLAUDE.md` + `CHANGELOG.md` | 本批指针与条目 |
| 本文件 | 新增 |

### 明确不提交

- `RimeWithWeasel/RimeWithWeasel.cpp`、` librime`（`git submodule` 假脏，并发会话所致）；
- `docs/design/quickpanel-v6/design-demos/{a-benchmark,b-craft,c-roulette}.html`（约 1.3 MB 设计稿）。

---

## 8. 已验证 / 推断 / 未验证

- **已验证**：单测 8/8；x64 构建 `X64_EXIT=0`；装机 `INSTALL_EXIT=0` 且 `ALL HARD CHECKS PASSED`；真机五元组 + `PASS_VALID_PRECONDITION`；`Alt+,` 快捷键登记点存在。
- **推断**（有一定依据但未直接观测）：语言栏项不注册后，TSF 的 compartment 与文本服务生命周期不受影响 —— 依据是 `_InitLanguageBar` 返回 `TRUE` 且真机中文输入可用（探针 ROUND=2 成功切到火流猩并保持活动）。
- **未验证**：右键菜单四个入口消失后是否有其它残留入口（README/文档类说明未逐条核对）。
- **待用户表态**：接受 §5 的代价，还是把 `kLangBarRegisterItem` 置回 `true`。

---

## 9. 下阶段

1. **用户资料云同步**（spec 100 已存在三件套，见 `.specify\specs\100-fluxing-cloud-account-sync\`）—— 本轮之后的主线。
2. 待用户复验：通知区域里本输入法的项是否真的不再出现（人眼确认一次）。

---

## 10. 接力 prompt

见本轮回答正文（按用户 2026-11-08 指示，接力 prompt 直接给在回答里，不另落文件）。