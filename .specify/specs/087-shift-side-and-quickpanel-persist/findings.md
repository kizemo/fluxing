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

---

## A.8 常驻面板「有输入动作或点击后消失」+ 裸 Shift 部分选词连带上屏（2026-10-29 v1.1.0 真机反馈，ADR 0018）

### 缺陷 1 · 「点击」支（有代码证据 · 已修）

- **根因**：`WeaselServer/WeaselServerApp.cpp` 的 `onPhrases` 里 `WM_LBUTTONUP` 处理
  （v0.19.0.39 为让子 `PhrasesDialog` 拿到键盘事件做的 `Hide() → Show() → BringToTop()` workaround）
  是**全仓唯一未守卫的生产 `Hide()` 调用点**；点「词库」按钮正是那次「点击」。
- **取证方式**：全仓搜 `Hide();` 调用点 —— 其余四处（FocusOut / 100ms 轮询器 / `OnKillFocus` /
  `WM_ACTIVATEAPP`）已由 ADR 0016 加 `AlwaysShowMode()` 守卫；`QuickPanelDialog::Hide()` 本体
  （`WeaselServer/QuickPanelDialog.cpp:1438`）仍无条件 `SW_HIDE` 是**刻意的**（ADR 0013 D7：
  `Alt+,` / ESC 必须仍能收起）。
- **修法**：守卫加在**调用点**，非常驻态行为逐字节不变。

### 缺陷 1 · 「输入动作」支（代码取证 · 待装机复验）

- **根因**：不是 `Hide()`。面板用 `WS_EX_LAYERED` + `UpdateLayeredWindow` **逐像素 alpha**
  （`QuickPanelDialog.cpp:1415` 注明 v0.19.0.10 起弃用 `SetLayeredWindowAttributes(LWA_ALPHA)`），
  `ApplyAlphaGradient()` 先把逐像素 alpha 压到 140→82，再乘 `s_alphaFactor`；100ms 轮询器在
  **光标不在面板矩形内**时把常驻态 park 到 `fluxing::kAlphaFactorIdle = 51`（ADR 0013 D3 的 20%）
  ⇒ 有效 alpha ≈ 28→16（桌面约 6-11%）。打字时光标按定义在目标应用编辑框 ⇒ 面板淡到看不见，
  且 `Hide()` 复位 `s_alphaFactor = 255`（`:1445`）只在真走 Hide 时发生，常驻态不经 Hide 的复用
  带着旧值起步 ⇒ 用户读作「消失」。
- **修法**：`kAlphaFactorIdle` 51 → **153**（60%），`include/QuickPanelAlpha.h:41`；
  fade tick 注释同步（255→153 约 5 tick @ step 24）。修订 ADR 0013 D3 的设计值，由 ADR 0018 D2 背书。
- **诚实边界**：本支**没有 Hide() 调用点证据**，只有代码自洽性（alpha 机制是唯一能解释「有输入动作后
  看起来消失」的路径，全仓再无第 5 处生产 `Hide()`）⇒ **装机复验是完成条件**（R6）。

### 缺陷 2 · 裸 Shift 部分选词连带上屏（有机制链 + 判别性测试 · 已修）

- **症状**：两字以上双拼、第 1 候选是词、第 2/3 候选是单字时，Shift 上屏单字的**同时把第 1
  候选词的第二个字也带上屏**；数字键 2/3 无此现象（上屏单字后继续为剩余音节出候选）。
- **机制链**（librime 只读参照，P3）：
  - `RimeWithWeasel.cpp::ShiftUp()` 的 `kSelectCandidate` 分支：`select_candidate(idx)` 之后
    **无条件** `commit_composition()`（ADR 0012 遗留）。
  - `ConcreteEngine::OnSelect`（`librime/src/rime/engine.cc:259-282`）：整词（`seg.end ==
    input.length()`）→ `composition().Forward()`（`segmentation.cc:114-120`）追加**零长度无 menu
    尾段** ⇒ 不落屏 ⇒ 这正是 ADR 0012 需要 commit 的场景。
  - 部分选词（`cand.end < input.length()`）→ `Segment::Close`（`segmentation.cc:17-24`）截短 +
    `partial` tag → `Compose(ctx)`（`engine.cc:279`）为剩余音节重建 `kGuess` 段 ⇒ menu **非空**，
    全程不 commit。
  - `Context::Commit()`（`context.cc:18-26`）= `commit_notifier`(**整个** composition 含剩余段首选)
    + `Clear` ⇒ 部分选词时 commit 会把第 1 候选词的次字一并带出 ⇒ 与用户描述完全吻合。
- **修法**：`include/ShiftStateMachine.h:66` 新增纯函数
  `ShouldCommitAfterSelect(bool context_available, int remaining_candidates)`；
  `ShiftUp()` 在 `select_candidate` 后 `get_context` 读 `menu.num_candidates`，**仅当无剩余候选
  （整词）才 commit**；`get_context` 失败保守 flush（防 ADR 0012 整词场景回归）。镜像数字键路径
  `RimeWithWeasel.cpp:1911-1919 SelectCandidate()`（`select_candidate + _UpdateUI`，无 commit）。
- **排除的判据**：`get_input`/caret 对比（`OnSelect` 两条分支都不改 `input_`/`caret_pos`，量不出差别）；
  `get_commit` peek（会**消费**文本，等于吞字）。
- **回归断言**：`Test_ShouldCommitAfterSelect()`（`Test #22`，5 条）—— false-context→flush ×2、
  `(true,0)`→flush、`(true,1)`/`(true,5)`→不 flush。

### 门禁（2026-10-29，Win32 / Release，本机复跑）

```
SOLUTION_EXIT=0        # msbuild weasel.sln /p:Configuration=Release /p:Platform=Win32 /m
TESTPROJ_EXIT=0        # TestShiftIPCStateMachine.vcxproj
Result: 49 PASS / 0 FAIL   # 原 44 + 新增 Test #22 的 5 条
MAPPER-SUMMARY|failures=0  RESULT|PASS   # TestHotkeyBindings --selfcheck-mapper
```

### 未闭合

- 「输入动作」支需装机复验：勾「常驻」后**打字时**与**点「词库」后**面板均应仍在，
  idle 应是可辨的浅态（约 60%）而非近似消失；`Alt+,` / ESC 仍能收起。
- 缺陷 2 需真机复测：两字以上双拼 Shift 选第 2/3 单字应只上屏该单字，剩余音节继续出候选。
- 64 位换装仍未闭环（64 位 CLSID `{A3F4CDED-...}` 指向未版本化旧件，用户未回报核验结果）。
## A.9 v1.1.1 真机反馈：常驻态重启后丢失 + ESC 不应收起面板（2026-10-29，ADR 0019，v1.1.2 已修）

### 取证

- 生产 Hide() 路径（auto-hide 轮询 grace 2000ms + outsideMs>=1500ms、OnTimer id=1、OnKillFocus、WM_ACTIVATEAPP、FocusOut IPC）全部带 `!AlwaysShowMode()` 守卫，无一失效；「几秒后消失」时序 = **瞬时模式设计行为** => 用户机常驻态实际为 false。
- 根因：`s_mode` 纯内存态、全仓无持久化链路（spec 084 FR-007 原为刻意设计）；1.1.1 装包必须重启 WeaselServer => 勾选清零。
- 唯一无守卫的手势路径：`OnKeyDown VK_ESCAPE -> Hide()`。

### 修复（v1.1.2，ADR 0019）

- 常驻勾选持久化 `%APPDATA%\Rime\quickpanel.state`（单行 ASCII）；`SetAlwaysShowMode` 仅模式变化时落盘，写完回读自证（空则删文件）。`WeaselServerApp::Run()` 启动时 `RestoreAlwaysShowMode()`：只恢复 `s_mode`，**不 Show、不回写盘**。
- ESC 吞键（`return 0`）不再收起面板；显隐只由 `Alt+,` 控制。
- 推翻 spec 084 FR-007 与 spec 087 FR-087-B、修订 ADR 0013 D7 的 Esc 分支；历史 spec 正文不改写，以本 findings 收口。

### 门禁（2026-10-29，Win32/Release，1.1.2 升版后复跑）

- MSBuild 全量 exit 0；`WeaselServer.exe` ProductVersion=1.1.2.0。
- TestQuickPanelDialog **34/34 PASS**（新增 T13a-h / T14a-f；测试落点重定向 `%TEMP%\fluxing_qp_state_test.state`）。
- TestShiftIPCStateMachine 49 PASS / 0 FAIL；TestHotkeyBindings --selfcheck-mapper RESULT|PASS。

### 未闭合

- 装机复验（1.1.2）：勾选常驻后**重启机器/服务勾选仍在**；面板调出后不再几秒自动消失；**ESC 不收起**，`Alt+,` 仍可显隐。
- A.8 两项已由用户 2026-10-29 验收闭合：「Shift 问题已经解决」；「输入动作」支实际根因为本条 A.9（常驻态被重启清零），非守卫失效。
