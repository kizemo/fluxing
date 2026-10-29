# 087 实施计划：快速面板「常驻」守卫 + 64 位前端换装闭环 + 裸 Shift 左右方向收尾

## 1. 技术上下文

### 1.1 现状（已核验）

**缺陷 A · 勾了「常驻」仍会消失**

面板的收起行为共有 9 个发起点，分两类：

| 类别 | 发起点 | 位置 | 是否已有 `AlwaysShowMode()` 守卫 |
|---|---|---|---|
| 自动 | 100 ms 轮询器 | `WeaselServer/QuickPanelDialog.cpp:740` | ✅ 有 |
| 自动 | `WM_ACTIVATEAPP`（应用整体切换） | `WeaselServer/QuickPanelDialog.cpp:682` | ✅ 有 |
| 自动 | `WM_KILLFOCUS`（面板失焦） | `WeaselServer/QuickPanelDialog.cpp:1326` | ✅ 有 |
| 自动 | `OnTimer(1)` 定时器回调 | `WeaselServer/QuickPanelDialog.cpp:1317` | ✅ 有 |
| **自动** | **输入法前端上报「输入焦点离开」** | `RimeWithWeasel/RimeWithWeasel.cpp:498`（改前裸调 `Hide()`） | ❌ **无** ← 缺陷点 |
| 手势 | ESC | `WeaselServer/QuickPanelDialog.cpp:1334` | 不需要（用户明确要求） |
| 手势 | `ToggleMode()` 内 | `WeaselServer/QuickPanelDialog.cpp:1453` | 不需要 |
| 手势 | `ToggleQuickPanel` | `WeaselServer/WeaselServerApp.cpp:387` | 不需要 |
| 手势 | 关闭「常驻」时 | `WeaselServer/WeaselServerApp.cpp:401` | 不需要 |
| 手势 | `onPhrases` 分支 | `WeaselServer/WeaselServerApp.cpp:421` | 不需要 |

⇒ 前 4 条自动路径在**面板内部**，守卫写在面板里；第 5 条**从外面进来**：

```
WeaselTSF/KeyEventSink.cpp:20        _EnsureServerConnected()
WeaselTSF/WeaselTSF.cpp:470-471      Echo 失败 ⇒ _Reconnect()
WeaselIPC/WeaselClientImpl.cpp:178   FocusOut()
WeaselIPCServer/WeaselServerImpl.cpp:272 / :277 / :474
RimeWithWeasel/RimeWithWeasel.cpp:498-499   ← 缺陷点（改后已加守卫）
WeaselServer/QuickPanelDialog.cpp:1438       Hide() → :1442 ShowWindow(SW_HIDE)
```

⇒ **修法**：守卫加在**调用点**（`RimeWithWeasel/RimeWithWeasel.cpp:498-499`），
**不动** `Hide()` 本体（`WeaselServer/QuickPanelDialog.cpp:1438`）。
理由：`Hide()` 服务于上表里 5 条**需要用户明确手势**的路径，把守卫放进 `Hide()` 内部
会让那些路径在长显态下失效（与 ADR 0013 D7 冲突）。

> 附带发现（**本条不修**）：勾选态只回填到托盘图标菜单
> （`WeaselServer/WeaselServerApp.cpp:29` → `WeaselServer/WeaselTrayIcon.cpp:32`
> `CheckMenuItem`）；语言栏弹出菜单 `WeaselTSF/LanguageBar.cpp:185` 裸调
> `TrackPopupMenuEx`，**无回填** ⇒ 用户在弹出菜单勾了看不到勾。

**缺陷 B · 64 位换装链路**

```
装包期  File/CopyFiles → $INSTDIR\weaselx64.<版本>.dll   ← 环节一
        失败则写 $INSTDIR\weaselx64.dll.stage2 标记        （install.nsi:701 / :710）
开机期  schtasks /Create /TR "cmd /c <bat>"                ← 环节二
        <bat> 内 copy /Y <源> $INSTDIR\weaselx64.dll        ← 环节三
```

| 环节 | 修复前 | 事实 |
|---|---|---|
| 一 | `output/install.nsi:689` 附近：`CopyFiles /SILENT "$TEMP\fluxing-staged\weaselx64.dll" "$INSTDIR\weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"` | 目标已存在即失败 ⇒ 版本化件**永远停在上一版**（用户机上有 10-28 20:50 的 `weaselx64.1.1.0.0.dll`）⇒ 直接掉进 stage2 分支 |
| 二 | `output/install.nsi:1055` / `:1080` 附近：`/TR "cmd /c $\"$TEMP\...\stage2-install.bat$\""` | NSIS 的 `$"` 展开成**裸引号**（丢反斜杠）⇒ 命令行变成 `/TR "cmd /c "C:\...\x.bat""` ⇒ schtasks 解析失败 ⇒ **两个任务都没被创建**（用户机上 `FluxingStage2Install` / `FluxingStage2InstallX64` **均不存在**） |
| 三 | `output/install.nsi:1048-1052` / `:1074-1078` 附近：`xcopy /Y /Q "src" "dst*"` | 目标带 `*` 对**文件**目标是非法用法（`*` 只在目录通配时成立） |

⇒ **三处叠在同一条链上**：任何一处坏掉 ⇒ 64 位程序继续跑旧件。

**缺陷 C · 裸 Shift 左右颠倒（事实链）**

| 环节 | 位置 | 事实 |
|---|---|---|
| 前端判定 | `WeaselTSF/KeyEventSink.cpp:44-52` | `isLeft = (ke.keycode == ibus::Shift_L)`，Shift 事件被完全 eat |
| 前端映射 | `WeaselTSF/KeyEvent.cpp:54` | `fluxing::ResolveCharacter(vkey, kinfo, keyState, NULL)` |
| 映射表 | `include/KeyNameMapper.h:391-396` | `case VK_SHIFT:` → `kinfo.scanCode == 0x36 ? ibus::Shift_R : ibus::Shift_L` |
| 映射表 | `include/KeyNameMapper.h:551-554` | `VK_LSHIFT → ibus::Shift_L`；`VK_RSHIFT → ibus::Shift_R` |
| 位域 | `include/KeyEvent.h:3-15` | `KeyInfo` 与 Windows `lParam` 布局一致，`KeyInfo(LPARAM)` 直接重解释 |
| 接收侧 | `WeaselIPCServer/WeaselServerImpl.cpp:406-437` | `DecodeShiftIsLeft(wParam)` = `(wParam & 1u) == 0u`（**正确**） |

⇒ 客户端判定左右**只有两个输入**：**虚拟键**与**物理扫描码**。
探针（`tmp/shiftprobe/shiftprobe.cpp`，直连运行中的管道）证明**接收侧正确**
（bit0=0 → 第 2 个候选、bit0=1 → 第 3 个候选）⇒ 只剩「这两个输入在运行期与物理键不符」一种可能。

**A.6 判据（缺的那一条事实）**

| 观测结果 | 含义 | 修复形态 |
|---|---|---|
| 按物理左 Shift 时，**操作系统认知的是左** | 翻译层错 | 改用**物理扫描码**判定（0x2A=左 / 0x36=右），虚拟键仅作兜底 |
| 按物理左 Shift 时，**操作系统认知的是右** | 操作系统/驱动层就报反了 | 软件**无法分辨物理左右** ⇒ 只能提供「左右互换」用户开关 |

观测工具：`tmp/shiftprobe/shiftside.cpp`（`WH_KEYBOARD_LL` 钩子 + `KBDLLHOOKSTRUCT::scanCode`
+ `vkCode`，再用 `fluxing::ResolveCharacter` 复算前端的键名），默认 30 秒窗口，
输出 `shiftside-out.txt`。

**缺陷 D · CapsLock（不是缺陷）**

`librime/src/rime/ascii_composer.cc:126-166` + `engine.cc:99-122`：
`good_old_caps_lock: true` 时，英文模式下按 CapsLock 走 `kRejected` ⇒ **中断处理器链**
⇒ 不切回中文，字母按锁存态输出大写。用户已裁定「这是正确规则」。
⇒ **不改码**，只写清语义 + 修正复测口径。

### 1.2 为什么不用另外几个方案

- **把守卫放进 `QuickPanelDialog::Hide()` 内部**：写法最短，但会让 5 条**需要用户明确手势**
  的路径在长显态下**全部失效**（ESC / 收起快捷键 / 点「词库」/ 关掉「常驻」/ `ToggleMode`）
  ⇒ **拒绝**。守卫放在**调用点**，只有「输入焦点离开」这一条被拦住。
- **给「常驻」再加一个全局抑制位**：改动面更大，且同样会误伤手势路径 ⇒ 不采纳。
- **版本化件改用「先删旧文件再拷」**：删掉用户机上的旧版本化件再拷，能解决「不覆盖」，
  但删旧件的动作本身有风险（删不掉就又回到原点）⇒ 改用带改名语义的 `File` + `SetOverwrite on`，
  **覆盖**而不是先删。
- **`schtasks /TR` 里手工拼 `\"` 转义**：NSIS 的 `$"` 展开成裸引号，**转义会被吃掉**；
  改成把批处理放到**无空格路径**（`$WINDIR\Temp`），命令行就**完全不需要内层引号** ⇒
  从根上绕开转义问题，而不是再拼一层反斜杠。
- **裸 Shift 方向：先按「客户端判反了」直接改**：接收侧已证伪该假设 ⇒ 改了也修不好。
  必须先取 A.6 的事实（宪法 R1）。
- **CapsLock：当成缺陷去改**：用户已裁定是正确规则 ⇒ 不改。

## 2. 技术方案

### 2.1 面板守卫（核心）

`RimeWithWeasel/RimeWithWeasel.cpp:498-499`，`FocusOut()` 内：

```cpp
  // spec 087 / ADR 0016: FocusOut 是面板的第四条自动隐藏路径。前三条
  // (轮询器 / WM_ACTIVATEAPP / WM_KILLFOCUS) 都在 QuickPanelDialog 内部
  // 带 AlwaysShowMode() 守卫,唯独这条从 WeaselTSF 过来的 IPC 直接调
  // Hide(),于是"勾了常驻"的面板在用户一切换输入焦点时仍然消失。
  //
  // 守卫加在调用点,不动 Hide() 本体: Hide() 还服务于需要用户明确手势
  // 的路径 (Alt+, 收起 / ESC / 点词库按钮),把守卫放进 Hide() 内部会让
  // 那些路径在长显态下失效 (ADR 0013 D7)。
  if (!QuickPanelDialog::AlwaysShowMode())
    QuickPanelDialog::Hide();
```

### 2.2 版本化安装件的覆盖（32 位 + 64 位两支同改）

`output/install.nsi:633-638`（32 位）与 `output/install.nsi:694-698`（64 位）：

```nsis
      SetOverwrite on
      Delete "$INSTDIR\weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"
      SetOverwrite on
      File /nonfatal /oname=weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll "weaselx64.dll"
      SetOverwrite try
```

⚠️ **实现陷阱（本轮打包实测订正）**：NSIS 的 `File "src" "dst"` **不是**改名语法 ——
`makensis` 会把第二个参数当成**待打包的输入文件**，直接报
`File: "weasel.1.1.0.0.dll" -> no files found.` 并中止。**唯一的改名语法是
`File /oname=<新名> <源文件>`**。另外：`SetOverwrite on` 要在 `File` 之前显式打开、
之后恢复 `try`，否则会把整段安装的文件覆盖策略一起改掉；`/nonfatal` 则把
「写不进去」变成错误位（降级到开机补换），而不是中止整个安装。

### 2.3 开机补换的三处修法

`output/install.nsi:1062` / `:1088`（批处理落点）与 `:1070` / `:1095`（任务登记）：

```nsis
    FileOpen $R9 "$WINDIR\Temp\fluxing-stage2-install.bat" w
    FileWrite $R9 "@echo off$\n"
    FileWrite $R9 'copy /Y "%TEMP%\fluxing-staged\WeaselServer.exe" "$INSTDIR\WeaselServer.exe" >NUL$\n'
    ...
    nsExec::ExecToStack 'schtasks /Create /SC ONSTART /TN FluxingStage2Install /TR "cmd /c $WINDIR\Temp\fluxing-stage2-install.bat" /RL HIGHEST /F'
```

三处分别对应：

1. `xcopy /Y /Q "src" "dst*"` → `copy /Y "src" "dst" >NUL`（`*` 对文件目标非法）。
2. 批处理落点 → `$WINDIR\Temp`（**无空格**）⇒ `/TR` 里**不需要内层引号** ⇒ 绕开 NSIS 丢反斜杠的转义问题。
3. **拷贝源（本轮已闭合）**：批处理里的拷贝源原先写成 `%TEMP%\fluxing-staged\...`，
   开机时按 **SYSTEM** 的 `%TEMP%`（= `C:\Windows\Temp`）解析，而文件是在**安装期**用 NSIS 的
   `$TEMP`（= 安装用户的临时目录，`output/install.nsi:585-586` / `:1050` / `:1083`）暂存的
   ⇒ **两者不是同一处**，开机补换会**找不到源文件**。
   本轮改成把 NSIS 的 `$TEMP` 在**安装期就展开**进批处理（`FileWrite` 里的 `$TEMP`
   是 NSIS 变量，不是 cmd 的 `%TEMP%`）⇒ 批处理里留下的是**绝对路径**。

### 2.4 裸 Shift 取证工具（只取证，不改判定）

`tmp/shiftprobe/shiftside.cpp`：`WH_KEYBOARD_LL` 低级键盘钩子 ⇒ 拿到
`KBDLLHOOKSTRUCT::scanCode`（**物理**位置：0x2A=左 / 0x36=右）与 `vkCode`
（**操作系统认知**的键）⇒ 再用 `fluxing::ResolveCharacter` 复算前端算出的键名 ⇒
输出 `PHYS-LEFT/PHYS-RIGHT`、`os_vk`、`weasel_keycode` 三列，一眼看懂谁在错。

配套：`tmp/shiftprobe/build-side.cmd`（A.6 诊断工具）、`tmp/shiftprobe/build.cmd`（服务端探针）。

## 3. 里程碑

### Q1 · 两处修复（核心）

- 面板守卫（`RimeWithWeasel/RimeWithWeasel.cpp`）+ 换装链路三处（`output/install.nsi`）。

### Q2 · 取证与交付

- `shiftside.exe` 交付 ⇒ 取现场按键数据 ⇒ 定 Shift 修复形态（**本条只到「形态判据」为止**）。
- 文档三件套（spec / plan / tasks）+ ADR 0016 + CHANGELOG + 任务总账 + 交接卡 §18。

## 4. Constitution Check

| 原则 | 落点 |
|---|---|
| **I · 先定意图** | R1 满足：动手前先写明意图与验收——意图＝面板守卫 + 换装闭环 + Shift 取证；验收＝SC-087-1…SC-087-9 写在 `spec.md` §5，每条可复算。**Shift 方向因事实未闭合而明确不写「已修」**，属 R1 的正面执行（先取证后改码），不是绕过。 |
| **II · 测试支撑 / R6 证据先行** | R6 满足：先取证再改码——面板 9 个发起点、换装 3 个环节、Shift 5 个环节逐环落到文件与行号（§1.1），探针数据贴进 `spec.md` §5.4。R2 满足：`spec.md` 不含技术栈词，文件名与库名只出现在 `plan.md`。 |
| **III · ≥3 文件需 spec/plan/tasks** | 满足：本条新建 `087-shift-side-and-quickpanel-persist/{spec,plan,tasks}.md`；改动文件为 2 个（`RimeWithWeasel/RimeWithWeasel.cpp`、`output/install.nsi`）+ 1 个诊断工具（`tmp/shiftprobe/shiftside.cpp`，**不入库**）+ 文档 6 个。 |
| **IV · 澄清** | 满足：三处歧义已在本条内消解——①「常驻」是不是全局生效 ⇒ 明确为**只挡自动隐藏、手势照旧**（`spec.md` FR-087-A/B）；②「Shift 先修还是先查」⇒ 明确为**先查后修**（FR-087-E/F）；③「CapsLock 算不算缺陷」⇒ 用户已裁定**不是缺陷**，只改口径（FR-087-G）。 |
| **V · 增量交付** | 满足：Q1（修复）与 Q2（取证/交付）分界清晰，可单独回退；本条**不扩协议、不动引擎本体、不碰用户数据路径**。 |

**必披露项**

| 披露项 | 结论 |
|---|---|
| 是否触及遗留用户数据路径 | **否**。只改前端一处调用点的判据与安装器的文件拷贝/任务登记，不读写用户词库/配置目录，不涉及 leveldb 与 `StartMaintenance`/`EndMaintenance` 包裹。P6 豁免不适用。 |
| 是否新增 IPC 消息类型 | **否**。面板守卫走既有「焦点变化」通路；换装链路是安装器内部行为。**不触碰 `include/WeaselIPCData.h`** ⇒ server / TSF / deployer 三端无需同步。 |
| 用户可见变更是否写 CHANGELOG | **是**。面板「常驻」不再被输入焦点切换打断 + 64 位换装闭环 + CapsLock 语义与复测口径，均写入 CHANGELOG `### 主要更新`。 |
| 是否修改 `librime/` | **否**（宪法 P3）。上游源码仅作只读取证依据（`ascii_composer.cc` / `engine.cc` 的 CapsLock 语义）。 |
| 是否新增第三方依赖 | **否**。取证工具用的是 Win32 `WH_KEYBOARD_LL` 低级键盘钩子与仓库自带的 `WeaselIPC.lib`，无新依赖。 |

## 5. 未决问题

- **Q-087-1（重要）**：开机补换脚本里的**拷贝源**路径未闭合——脚本在开机时（SYSTEM 上下文）
  解析出的 `%TEMP%` 与安装期暂存用的 `$TEMP` 可能不是同一处。本条**没有**把它写成已修好。
  下一阶段修法二选一：安装期把暂存目录**展开成绝对路径**写进脚本；或统一暂存到
  `$WINDIR\Temp\fluxing-staged`（与脚本同处）。
- **Q-087-2**：裸 Shift 方向的**事实未取得**（`shiftside.exe` 未跑）⇒ 修复形态二选一未定
  ⇒ 本条**不改判定逻辑**。
- **Q-087-3**：输入法弹出菜单**没有勾选标记**（`WeaselTSF/LanguageBar.cpp:185` 裸开菜单），
  补它需要新增「查询状态」的往返消息 ⇒ 协议扩张，留后续条目（沿用 ADR 0015 D7）。
- **Q-087-4**：沙箱权威门禁在本机**跑不起来**（`run-verify.cmd` exit 4、无 `boot.log`，
  清单 T19）⇒ 本条**没有门禁数字可贴**，如实记录，不编造。
- **Q-087-5**：64 位程序的注册项仍指向**未版本化**的文件名 ⇒ 版本化安装件装成后，
  仍需「注册」这一步成功改写它才生效（`output/install.nsi:698` 附近），失败会退回开机补齐路径。

## 6. 提交纪律

- Conventional Commits + 模块前缀；本条改动跨 `RimeWithWeasel` / `installer` ⇒ 拆两个提交：
  `fix(fluxing): …`（含代码与安装器）与 `docs(fluxing): …`（spec / ADR / 账本 / 交接卡）。
- 提交前：`git status -sb` + `git diff --stat`（防 `weasel.props` / `env.bat` / `*.obj` 误提交），
  格式检查、整解构建、全套件、CHANGELOG 四项门禁全绿。
- commit message 以 UTF-8 无 BOM 写文件后 `git commit -F <file>`。
- **本条不含** Shift 方向修复（Q-087-2 未闭合）⇒ 提交说明里不得出现「Shift 方向已修复」。
