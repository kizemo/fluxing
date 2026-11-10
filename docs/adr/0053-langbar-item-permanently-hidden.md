# 0053 · 语言栏项永久隐藏：托盘「中英」图标用「不注册」去掉

- 状态：已接受 (accepted)
- 日期：2026-11-10
- 修订：ADR 0044（其 `TF_LBI_STYLE_SHOWNINTRAY` 前提被证伪）
- 修订记录（本 ADR 自身两次被判死，第三次才成立）：
  - **v1**：`TF_LBI_STYLE_HIDDENSTATUSCONTROL` + 恒 `TF_LBI_STATUS_HIDDEN` —— **真机证伪，作废**（§「两条被证伪的路径」）。
  - **v2（现行）**：`kLangBarRegisterItem = false`，`_InitLanguageBar()` 直接不注册语言栏项并 `return TRUE`。**真机通过**。
- 关联：ADR 0012（语言栏文字「英/中」）、ADR 0038（品牌图标右键菜单与左键面板）、ADR 0044
- 代码：`include/FluxingLangBarStyle.h`（新建，单一真相源）、`WeaselTSF/LanguageBar.cpp`（`_InitLanguageBar` / `_UninitLanguageBar` / `_UpdateLanguageBar`）、`test/TestFluxingComponents/TestFluxingLangBar.cpp`（新建，T1–T8）

## Context

用户 2026-11-10 反馈：「之前去除的托盘“中英”图标，又出现了，需要去除。」

UIA 枚举通知区域（`Shell_TrayWnd` 后代 Button，共 18 个）确认通知区域并存两个「任务栏输入指示」：

| 项 | name | 归属 |
| --- | --- | --- |
| ① | `任务栏输入指示 左键切换模式，右键打开菜单` | **本仓库的 TSF 语言栏项** —— tooltip 逐字等于 `WeaselTSF/LanguageBar.cpp:162` 的 `GetTooltipString`；图标 = `GetIcon()` 的 `IDI_ZH`/`IDI_EN`，即方框「中」/「英」 |
| ② | `任务栏输入指示 简体中文(中国大陆) 火流猩输入法…` | Windows 自带 IME 指示器（红猿头），由系统按输入配置文件创建，**本仓库代码改不掉** |

⇒ 用户说的「中英图标」= ①。② 不在本 ADR 范围（要关只能走 Windows 设置 / 组策略）。

### 两条被证伪的路径

| 路径 | 机制 | 判死依据 |
| --- | --- | --- |
| **ADR 0044** | 从 `dwStyle` 摘掉 `TF_LBI_STYLE_SHOWNINTRAY` | 微软文档对该标志写明「除语言栏外，项目还将显示在通知图标托盘中。**目前不支援此标志。**」（`TF_LBI_STYLE_SHOWNINTRAYONLY` 同样标注不支援）⇒ 该 flag 是 **no-op**。0044 的 `Verification` 段当时只记「改动尚未上机（文件锁）」，**从未验证机制本身是否成立**。 |
| **本 ADR v1** | `TF_LBI_STYLE_HIDDENSTATUSCONTROL` + `Show()` 恒置 `TF_LBI_STATUS_HIDDEN` | 2026-11-10 真机：装机副本 SHA256 与构建产物逐字节一致、注册表 `InprocServer32` 指向版本化 shim、explorer 已重启且实测跑新镜像、火流猩为活动 IME —— **图标依然在**。 |

「修复确实已在机上」已被五元组证据排除假修复可能：注册表指向的版本化 shim === 构建产物（SHA256）+ 宿主进程 `StartTime` 晚于 DLL 落地 + 模块**完整路径** + 前台窗口的宿主进程就是加载新镜像的那个进程 + 该宿主真的激活了本输入法（探针自证 `FOUND_FLUXING_ACTIVE=True`）。

⇒ 代码在机、图标仍在 ⇒ 必须换机制（**不注册**），而不是再排查「有没有上机」。

## Decision

**根本不向语言栏注册该项**：`_InitLanguageBar()` 在 `new CLangBarItemButton` / `AddItem` 之前早退并 `return TRUE`。系统没有托管项可渲染 ⇒ 通知区域不可能出现该图标。

1. **单一真相源** `include/FluxingLangBarStyle.h` 的 `kLangBarRegisterItem = false` 是 v14 总开关；生产与单测同调（constitution R7）。
2. `_InitLanguageBar()` 在 `AddItem` 之前 `if (!fluxing::kLangBarRegisterItem) return TRUE;`。
   **必须 `return TRUE`，绝不能 `return FALSE`** —— `WeaselTSF.cpp::ActivateEx()` 里 `if (!_InitLanguageBar()) goto ExitError;` 会走 `Deactivate()`，整个文本服务失活、**中文输入直接死掉**。
3. `_UninitLanguageBar()` 在 `_pLangBarButton == NULL` 时直接 return（不注册 ⇒ 无按钮 ⇒ 不得解引用）。
4. `_UpdateLanguageBar()` 的 compartment 读写（`GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION`，native / full shape）与语言栏项**解耦**：原来这段挂在 `if (!_pLangBarButton) return;` 之后，语言栏项不注册后系统就收不到中/英、全/半的状态声明。改为**无条件执行**，末尾再空判更新图标。
5. 原注册 / 隐藏代码（`kLangBarItemStyle` 仍含 `HIDDENSTATUSCONTROL`、`kLangBarItemForceHidden = true`、L459 的 `Show(FALSE)`）**整体保留为回退路径**：把 `kLangBarRegisterItem` 翻成 `true` 即回到 v1 行为，不需要改 `.cpp`。

明确否决：

- **`TF_LBI_STYLE_SHOWNINTRAY` 路线（0044）** —— 微软标注「不支援」，no-op。
- **`HIDDENSTATUSCONTROL` + 恒 `TF_LBI_STATUS_HIDDEN` 路线（本 ADR v1）** —— 真机双证无效。
- **`TF_LBI_STYLE_INVISIBLE`** —— 它会把语言栏里的入口一并藏掉，且同样不解决通知区域问题；`FluxingLangBarStyle.h` 文件头保留该禁令，防回退时误用。
- **重新打开 `WeaselServer` 托盘图标做补偿入口** —— `output/data/weasel.yaml:81 display_tray_icon: false` 是用户主动关的，不得擅自恢复。

## Consequences

- ✅ 通知区域里的方框「中」/「英」图标不再出现。真机实测：「任务栏输入指示」按钮数 **2 → 1**，本仓库那一项 tooltip **0 命中**。
- ✅ 不新增/移除任何 IPC 字段，不动 `include/WeaselIPCData.h`。
- ✅ 中/英、全/半对系统的状态声明**不丢**（compartment 解耦，见 Decision 4）。
- ⚠️ **代价：语言栏项整体不注册 ⇒ 该图标承载的全部交互入口一并消失。** 落实清单（不许含糊）：
  - 通知区域里「左键切换模式 / 右键打开菜单」这一整项没了；
  - 挂在它右键菜单上的「重启算法服务 / 程序文件夹 / 快捷键设置 / 用户登录」也**没有别的入口**（`display_tray_icon: false` ⇒ `WeaselServer` 托盘图标不常驻；批次 U 已按用户指示把「快捷键设置」「用户登录」两个键帽从快捷面板摘掉）。
  - **仍可用**：快捷设置栏 —— `Alt+,`（`WeaselServer/WeaselServerImpl.cpp:81` `RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA)`）。
  - **回退杠杆**：把 `kLangBarRegisterItem` 置 `true` 重新编译即可换回入口，同时换回图标。
- ⚠️ 生效需要**重启加载 TSF 的宿主进程**（explorer / ctfmon / 各应用）—— 旧进程仍持有旧 DLL。
- ⚠️ Windows 自带的红猿头指示器 ② 不受本 ADR 影响。

## Verification

单测 `test/TestFluxingComponents/TestFluxingLangBar.cpp`（**8 条**，`TestFluxingLangBar: 8/8 PASS`）：

- T1–T7 锁住回退路径的自洽性：样式位含 `HIDDENSTATUSCONTROL` / 保留 `BTN_BUTTON`+`BTN_MENU` / 不含 `SHOWNINTRAY` / 无未审样式位 / 隐藏状态位 == `TF_LBI_STATUS_HIDDEN` / 恒隐藏策略开 / 策略与样式位自洽；
- **T8 锁住 v14 生效态**：`kLangBarRegisterItem == false`（`langbar item is not registered (v14 force-absent)`）。

真机判据（UIA 枚举通知区域 + 五元组宿主自证，脚本 `tmp/probe-v14-explorer2.ps1`，日志 `tmp/probe-v14-explorer2.log`）：

| 判据 | 实测 |
| --- | --- |
| 前台宿主加载的镜像是构建产物 | `explorer.exe`（pid 61200，`CabinetWClass`）映射 `D:\Program Files\fluxing\weasel\weaselx64.1.2.1.3.0.dll`，SHA256 `415FE7C5…6121` ≡ `output\weaselx64.dll` |
| 本输入法确实是活动 IME | `FOUND_FLUXING_ACTIVE=True`（ROUND=2 切到「火流猩输入法」） |
| 「任务栏输入指示」按钮数回落 | `INPUT_INDICATOR_COUNT=1`（改前基线 2） |
| 本仓库项 tooltip 命中数 | `OUR_TOOLTIP_HIT=0` |
| 结论 | **`VERDICT=PASS_VALID_PRECONDITION`** |

> 前三次 `FAIL` / `INCONCLUSIVE` 均归因为**假阴性**（宿主进程持旧 DLL / `notepad` 是 Store 应用取不到 `MainWindowHandle` / `charmap.exe` 吞 `Win+Space`），不是代码无效。判「已生效」必须换宿主进程，不能只重启 explorer 就用「当前前台应用」验收。

源码侧封死替代来源：

```powershell
# 全仓唯一 tooltip 与唯一 AddItem 调用点，且 AddItem 在 v14 早退之后
Select-String -Path WeaselTSF\*.cpp -Pattern 'AddItem\s*\('     # 仅 LanguageBar.cpp:450
Select-String -Path WeaselTSF\*.cpp,include\*.h -Pattern '左键切换模式'  # 仅 LanguageBar.cpp:162 + 本头注释
Select-String -Path include\FluxingLangBarStyle.h -Pattern 'kLangBarRegisterItem|TF_LBI_STYLE|TF_LBI_STATUS'
```