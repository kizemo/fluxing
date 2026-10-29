# spec 087 取证记录（2026-10-29 真机复测反馈）

> 本文件是 spec 087 的**证据底稿**：先说事实与实测数据，再谈结论。
> 所有结论都必须能回指到本文件里的一条实测或一条源码行号。
> 状态：取证阶段收口 ⇒ 进入修复设计。

---

## A. 裸 Shift 左右上屏方向（用户反馈：左=第 3 个、右=第 2 个）

### A.1 用户实测（2026-10-29，安装并重启 1.1.0 之后）

- 左 Shift 上屏**第 3** 个候选、右 Shift 上屏**第 2** 个候选 —— 与期望（左=第 2、右=第 3）相反。
- **关键观察**：该症状与 spec 086 修复**之前**（2026-10-28 用户首报）**完全一致**。
  服务端解码已被改过（`a2fa205`），症状却一动不动 ⇒ 解码方向不是唯一变量。

### A.2 源码链路（逐行读实，2026-10-29）

| 层 | 位置 | 内容 |
|---|---|---|
| 前端判定 | `WeaselTSF/KeyEventSink.cpp:44-58` | `ke.keycode == ibus::Shift_L \|\| == ibus::Shift_R` → `isLeft = (ke.keycode == ibus::Shift_L)`；Shift 事件被完全 eat |
| 前端映射 | `WeaselTSF/KeyEvent.cpp:54` | `fluxing::ResolveCharacter(vkey, kinfo, keyState, NULL)` |
| 映射表 | `include/KeyNameMapper.h:377-396` | `case VK_SHIFT:` → `kinfo.scanCode == 0x36 ? ibus::Shift_R : ibus::Shift_L` |
| 映射表 | `include/KeyNameMapper.h:551-554` | `VK_LSHIFT → ibus::Shift_L`；`VK_RSHIFT → ibus::Shift_R` |
| 映射表 | `include/KeyNameMapper.h:578-584` | `ResolveCharacter` → `TranslateKeycode(vkey, kinfo)`（唯一一张表，无第二套映射） |
| 位域 | `include/KeyEvent.h:3-15` | `KeyInfo`：`repeatCount:16 / scanCode:8 / isExtended:1 / … / isKeyUp:1`，与 Windows lParam 布局一致；`KeyInfo(LPARAM)` 直接重解释 |
| 客户端编码 | `WeaselIPC/WeaselClientImpl.cpp:96-110` | `EncodeShiftIsLeft(is_left)` = `is_left ? 0u : 1u`（`include/ShiftStateMachine.h:23-25`） |
| 服务端解码 | `WeaselIPCServer/WeaselServerImpl.cpp:406-437` | `DecodeShiftIsLeft(wParam)` = `(wParam & 1u) == 0u`（`include/ShiftStateMachine.h:27-29`） |
| 消息路由 | `WeaselIPCServer/WeaselServerImpl.cpp:463-494` | `MAP_PIPE_MSG_HANDLE(pipe_msg.Msg, pipe_msg.wParam, pipe_msg.lParam)` —— 参数顺序正确 |
| 状态机 | `include/ShiftStateMachine.h:59-69` | `r.index = lastIsLeft ? 1u : 2u`（0-based ⇒ 左=第 2、右=第 3） |
| 选词 | `RimeWithWeasel/RimeWithWeasel.cpp:1859-1876` | `size_t idx = s.lastIsLeft ? 1 : 2;` → `select_candidate(idx)` → `commit_composition` → `_Respond` |
| librime | `librime/src/rime_api_impl.h:970-980,1036-1039` | `RimeSelectCandidate` → `do_with_candidate(session_id, index, &Context::Select)` |

**修复前（`a2fa205` 之前）服务端解码是 `(wParam & 1u) != 0`** —— 与客户端的 `is_left ? 0u : 1u` 正好相反。
这正好能解释 2026-10-28 的首报症状：左（发 0）被服务端读成「右」→ idx 2 → 第 3 个；右（发 1）被读成「左」→ idx 1 → 第 2 个。
**`a2fa205` 修的就是这一处，且修对了。**

### A.3 已排除的假设（逐条实测）

1. **wire 极性反**：客户端 `is_left ? 0 : 1` 自 08-15 `c529f1f` 起从未变过；`git log -- WeaselIPC/WeaselClientImpl.cpp` 只有 3 笔。排除。
2. **客户端左右判反**：`KeyEventSink.cpp` 自 08-16 `1bbb4e5` 起未改；判定式本身自洽。排除（代码层面）。
3. **服务端解码没部署**：`include/ShiftStateMachine.h` mtime 22:31:57 → `WeaselServerImpl.obj` 22:32:15 → `WeaselIPCServer.lib` 22:32:17 → `output\Win32\WeaselServer.exe` 22:32:56。排除。
4. **旧进程抢占管道**：运行中的 `WeaselServer.exe` pid 32304，创建于 2026/10/29 07:37:11，路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe`。排除。
5. **管道参数错位**：`MAP_PIPE_MSG_HANDLE` 顺序正确。排除。
6. **旧前端映射不同**：10-27 HEAD `a023ee1` 的 `WeaselTSF/KeyEvent.cpp` 与 HEAD **逐字相同**；64 位旧件 `weaselx64.dll`（10-27 23:50）里的客户端编码同样是 `is_left ? 0u : 1u`。排除。
7. **TSF 存在第二套映射表**：`ResolveCharacter` → `TranslateKeycode` 唯一入口。排除。
8. **OS 级扫描码重映射**：`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layout` **无** `Scancode Map` 值。排除。

### A.4 运行时探针（决定性证据，2026-10-29）

用仓库自带 `WeaselIPC.lib` 写客户端探针 `tmp/shiftprobe/shiftprobe.cpp`，直连**运行中的**服务器管道：

- 参考轮：`Client::SelectCandidate(1)` / `SelectCandidate(2)`（与服务端 Shift 路径**同一个** `select_candidate` API，索引基准自动抵消）+ `CommitComposition()`。
- 实验轮：`Client::ShiftDown(is_left)` / `ShiftUp(is_left)`。

结果（schema = `double_pinyin_sogou`，输入 `nihao`）：

| 轮次 | 上屏文本 |
|---|---|
| `SelectCandidate(1)` | 你哈哦 |
| `SelectCandidate(2)` | 拟哈哦 |
| **裸 Shift bit0=0（左）** | **你哈哦**（= index 1） |
| **裸 Shift bit0=1（右）** | **拟哈哦**（= index 2） |

⇒ **运行中的服务端解码完全正确**：bit0=0 → idx 1（第 2 个候选），bit0=1 → idx 2（第 3 个候选）。
⇒ 部署件核对：`D:\Program Files\fluxing\weasel\WeaselServer.exe` SHA256 =
`830E485E66150C81B581CD9116FA6963A631E812A59531BA99F0E54FCC0EAECE`，
与 `output\Win32\WeaselServer.exe` **逐字节相同**（2231808 B，mtime 2026/10/28 22:32:56）。

### A.5 结论（根因闭合）

1. 服务端**无错**，客户端**无错**（按源码看），但**运行结果仍反** ⇒ 必然有一端在运行时拿到的「左右」与物理按键相反。
2. 由于用户症状在服务端解码修正前后**完全不变**，唯一能同时满足的模型是：
   **客户端一直把物理左 Shift 报成了右**（即 `ke.keycode` 在运行期等于 `ibus::Shift_R`），
   于是发出去的是 bit0=1，服务端按契约解成「右」→ idx 2 → 第 3 个候选；
   物理右 Shift 被报成左 → bit0=0 → idx 1 → 第 2 个候选。**与用户实测逐条吻合。**
3. 客户端判定左右**只有一个输入**：`TranslateKeycode(vkey, kinfo)` 的结果，
   而它只依赖 **vkey** 与 **扫描码**。⇒ 二者之一在运行期与物理键不符。

### A.6 尚未闭合的一点（下一阶段要取的事实）

**OS 到底怎么认这两个键？** 需要一次「用户按一下键」的实测，二选一：

- 若 `GetAsyncKeyState(VK_LSHIFT)` 在按物理左 Shift 时**亮** ⇒ OS 认知正常 ⇒ 是 Weasel/TSF 这一层的
  vkey/扫描码翻译出错 ⇒ 改用**物理扫描码**（0x2A=左 / 0x36=右）判定即可根治。
- 若 `GetAsyncKeyState(VK_RSHIFT)` 在按物理左 Shift 时**亮** ⇒ **OS/驱动层就报反了**
  ⇒ 软件无法自行分辨物理左右，**只能提供「左右互换」开关**让用户显式翻转。

两种情况的修复形态不同，因此必须在改码前把这条事实取到手（constitution R1）。

### A.7 闭合：链路上每一环都对 ⇒ 反转来自宿主交给 TSF 的 (vkey, 扫描码) 这一对输入本身

**2026-10-29 本轮把 A.6 的那两个输入本身也查完了。** 结论：**链路上每一环都是对的。**

| 环节 | 位置 | 结论 |
|---|---|---|
| 位域布局 | `include/KeyEvent.h:3-15` | 全部位域基类型都是 `UINT` ⇒ 布局 = Windows lParam（bit16-23 = 扫描码、bit31 = key up）；旧件与新件**逐字相同** |
| 键名映射 | `include/KeyNameMapper.h:391-396`、`:551-554` | `VK_SHIFT` → 扫描码 0x36 为右、其余为左；`VK_LSHIFT` → 左；`VK_RSHIFT` → 右。**2012 年至今每个历史版本都没有写反** |
| 前端判定 | `WeaselTSF/KeyEventSink.cpp:44-58` | `isLeft = (ke.keycode == ibus::Shift_L)`；旧 64 位件的源码 `d77db65` 与 HEAD **逐字相同** ⇒ 旧件被排除 |
| 客户端编码 | `WeaselIPC/WeaselClientImpl.cpp:96-110` | `EncodeShiftIsLeft(is_left) = is_left ? 0 : 1`，自 08-15 起未变 |
| 服务端解码 | `WeaselIPCServer/WeaselServerImpl.cpp:406-437` | `DecodeShiftIsLeft(wParam) = (wParam & 1) == 0`；部署件与 `output\Win32\WeaselServer.exe` **逐字节相同** |
| 状态机 → 选词 | `RimeWithWeasel/RimeWithWeasel.cpp:1812-1822`、`:1869-1872` | `s.lastIsLeft = is_left`；`idx = s.lastIsLeft ? 1 : 2` → `select_candidate(idx)`（0-based ⇒ 左 = 第 2、右 = 第 3） |

**运行时探针**（`tmp/shiftprobe/shiftprobe.cpp`，直连运行中的服务器管道）证明接收侧**完全正确**：
`ShiftDown(bit0=0)` → 第 2 个候选、`ShiftDown(bit0=1)` → 第 3 个候选 ⇒ **发送侧送反**。

**已排除的假设**（逐条有据）：

- 旧 64 位前端被占用 —— `git diff d77db65 HEAD -- WeaselTSF/KeyEventSink.cpp` 的 Shift 相关 hunk **为空**；
- 旧头文件 —— `git diff d77db65 HEAD -- include/KeyEvent.h` 的差异**仅是注释**；
- 位域布局错位 —— 见上表；
- 映射表写反 —— 见上表（逐个历史 revision 核对过，从 2012 年 `cae1471` 起）；
- 服务端解码未部署 —— 部署件与 `output\Win32\WeaselServer.exe` 逐字节相同；
- 旧进程抢占 / 管道参数错位 —— 运行时探针直连管道复现，接收侧正确；
- OS Scancode Map —— `HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layout` **无** `Scancode Map`；
- 注入实验 —— 注入扫描码 `0x2A` 时低层钩子看到 `vk = VK_LSHIFT`（**未反**）；扫描码 `0x36` + `KEYEVENTF_EXTENDEDKEY` **未翻译出虚拟键**（`vk=0x00`）⇒ 右 Shift 的注入路径**未补齐**（不影响结论，只影响探针完整性）。

**运行日志取不到值**：`DLOG(INFO)` 在 Release 构建里被编译掉，
`%TEMP%\rime.weasel\*.INFO.*.log`（43,154 B，PID 32304 = 用户 10-29 复测会话）里**没有任何 Shift 行**
（全文 2,417 行）⇒ 无法从日志回放运行期的左右值。

⇒ **结论**：反转只能来自**宿主交给 TSF 的 (vkey, 扫描码) 这一对输入本身**
（应用层交换，或更下面的驱动层交换）。软件**无法分辨驱动层的交换** ⇒ 必须同时留两条杠杆：
**扫描码优先**（修应用层交换）+ **用户可配置的左右互换开关**（修驱动层交换）。

**A.6 的事实仍未取得**（用户尚未跑 `shiftside.exe`），但它**不再阻塞落地**：
两种形态**合并**实施（ADR 0017 D1 + D2），缺省行为不变。

---

## B. 快速面板「常驻」勾选后仍消失

- 已知：`WeaselServer/QuickPanelDialog.cpp:1438 Hide()` **没有** `AlwaysShowMode()` 守卫；
  而 `:1313 OnTimer(1)`、`:1324 OnKillFocus` 都有守卫。
- 已知：`WeaselServer/WeaselServerApp.cpp:371` 菜单处理器 → `ToggleAlwaysShowMode()`；
  `:383 ToggleQuickPanel` → `Hide()`；`:395 ToggleAlwaysShowMode`（关闭长显时 `Hide()`）；
  `:405 ShowQuickPanel` 的 `onPhrases` 分支 `:419-422` 再 `Hide()`。
- 待补：输入过程中究竟是哪一条路径把面板收走（子代理取证中，结论另附）。

---

## C. CapsLock 语义（用户已裁定：**这是正确规则，不作为缺陷**）

- librime `ascii_composer.cc:126-166` + `engine.cc:99-122`：`good_old_caps_lock: true` 时，
  英文模式下按 CapsLock 走 `kRejected`，**中断处理器链** ⇒ 不切回中文，而是留在英文态；
  字母按 CapsLock 的锁存态输出大写。
- 用户裁定：「英文模式下按 CapsLock 应当进入大写英文输入状态，不切回中文是正确的」。
- ⇒ 处置：**不改码**，只做两件事：
  1. 在说明书/交接文档里把这条语义**写清楚**（caps 打开 ⇒ 大写英文、无候选窗）；
  2. **修正上一轮复测单的口径**：选候选 / 翻页 / 切中英 / 切标点 / 切简繁 **必须在 CapsLock 关闭时**测，
     caps 打开时测这些项一律无意义。

---

## D. 64 位前端换装链路未闭环

| 缺陷 | 位置 | 事实 |
|---|---|---|
| 版本化 shim 复制缺 `/Y` | `output/install.nsi:689` | `CopyFiles /SILENT "$TEMP\fluxing-staged\weaselx64.dll" "$INSTDIR\weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"` —— 目标已存在即失败（用户机上有 10-28 20:50 的 `weaselx64.1.1.0.0.dll`）⇒ 直接掉进 stage2 分支 |
| `schtasks /TR` 引号断裂 | `output/install.nsi:1055` / `:1080` | `'... /TR "cmd /c $\"$TEMP\...\stage2-install.bat$\"" ...'` —— NSIS 的 `$\"` 展开成**裸引号**（无反斜杠），进程命令行变成 `/TR "cmd /c "C:\...\x.bat""` ⇒ schtasks 解析失败 ⇒ **两个 Stage2 任务都没被创建**（用户机上 `FluxingStage2Install` / `FluxingStage2InstallX64` 均不存在） |
| stage2 批处理用 `%TEMP%` | `output/install.nsi:1048-1052` / `:1074-1078` | 批处理在 SYSTEM 上下文运行时 `%TEMP%` = `C:\Windows\Temp`，而暂存文件在**安装时的用户临时目录** ⇒ 路径指错 |
| 批处理用 xcopy 且目标带 `*` | 同上 | `xcopy /Y /Q "src" "dst*"` 对**文件**目标是非法用法（`*` 只在目录通配时成立） |

用户机现状：`weaselx64.dll` = 10-27 23:50 **旧件**；`weaselx64.1.1.0.0.dll` = 10-28 20:50 旧新件（shim 从未注册成功）；
`weaselx64.dll.stage2` marker 残留 ⇒ 64 位 CLSID 仍指向旧件。
