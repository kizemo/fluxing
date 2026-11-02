# Handoff — spec 097 快捷设置栏「切走必隐」（2026-11-02）

> 交接对象：下一个接手 Fluxing spec 097 收尾的会话。
> 读完本文 + `CLAUDE.md` §1 的五份进站文档，即可无损接力。
> 本文只写「结论 + 证据指针 + 下阶段动作」，不复述源码全文。

---

## 0. 一句话现状

**根因已坐实（三个独立缺陷叠加），源码修复已全部落盘，ADR 0029 已写；
当前卡在「构建 → 测试 → 部署 → 真机复测」这后半段。**
真机复测需要用户亲手切一次输入法，Agent 无法自行完成。

---

## 1. 背景

用户报告：

> 快捷短语上屏问题已经解决。但还是存在快捷设置栏，在切换其他输入法时仍然存在的 bug，
> 应该随切入火流猩而显示，切到其他输入法则隐藏。

术语必须先对齐，否则会修错对象：

| 用户口语 | 代码实体 | 形态 |
|---|---|---|
| **快捷设置栏** | `WeaselServer/QuickPanelDialog.cpp` 的 `QuickPanelDialog` | WeaselServer 内的顶层小面板 |
| **快捷短语** | `FluxingPhrasesDialog.exe` | 独立进程，**与本次无关**（用户已确认该问题解决） |

本次要修的是**前者**。

服务端原本已有判定函数 `ShouldShowQuickPanelOnFocusIn()` /
`ShouldHideQuickPanelOnFocusOut()`（`include/WeaselIPC.h`），并且已有一支活体探针证明
**判定语义本身是正确的**：

```
FOCUS_IN(PROFILE_ACTIVATED)      => visible=True
FOCUS_OUT(param=0)               => visible=True   （尊重「常驻」开关）
FOCUS_OUT(PROFILE_DEACTIVATED)   => visible=False  ← 正是用户要的行为
```

结论：**问题不在「要不要隐藏」，而在「隐藏这个决定有没有被执行」。**

---

## 2. 目标

1. 切入火流猩 ⇒ 快捷设置栏显示。
2. 切到任何其他输入法 ⇒ 快捷设置栏必须隐藏，**即使打开了「常驻显示」**。
3. 不破坏既有路径：`Alt+,` 手动收起 / ESC / 点击按钮 / 托盘菜单 仍然可用。
4. 不违反宪法 P2：TSF 回调线程内不得出现阻塞 I/O。

---

## 3. 根因：三个独立缺陷叠成同一个症状

三个缺陷**互相掩盖**。只修其中任意一两个，症状都照旧 —— 这是本轮最重要的认知。

### 缺陷 1 — 通知线程把「退场通知」整条丢弃

`WeaselTSF/WeaselTSF.cpp` 的 `_ProfileNotifyLoop()` 旧写法：

```
先判 m_profileNotifyStop  →  再取 m_profileNotifyPending
```

`Deactivate()`（同文件）在析构竞态窗口里投递 `kProfileNotifyDeactivated`，
此时 `m_profileNotifyStop` 已被置位 ⇒ **整条消息被丢弃，连日志都不留**。
于是「用户切走了」这个事实从来没到达服务端。

### 缺陷 2 — 管道发送失败在调用点不可区分

`include/PipeChannel.h` 的 `_Send()`：catch 之后调用 `_Reconnect()`，
但**仍然用重连前的局部句柄去写**；二次失败被上层
`ClientImpl::_SendMessage` 的 `catch (DWORD) { return 0; }` 静默吞掉。

后果：调用点看到 `0`，无法判断是「消息没发出去」还是「发出去了对方没照做」。
服务端 `OnFocusOut` 的应答本来就恒为 `0`，两种语义彻底混在一起。

### 缺陷 3 — 显示与隐藏在**不同线程**上定序

- `FocusIn` 路径：`PostMessage(WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL_SHOW)`
  —— 异步，进 UI 线程队列。
- `FocusOut` 路径：**直接在 IPC 线程调用** `QuickPanelDialog::Hide()`
  —— 同步，立刻执行。

于是「排队中的 show」完全可能排在「已经在跑的 hide」之后执行 ⇒ **面板复活**。
这与缺陷 1/2 叠加后，表现为「切走后有时消失、有时不消失、有时闪一下又回来」。

---

## 4. 修复内容（F1–F6）

| 编号 | 位置 | 动作 |
|---|---|---|
| F1 | `WeaselTSF/WeaselTSF.cpp` `_ProfileNotifyLoop()` | 先取 pending，再判 stop；仅当 `kind == kProfileNotifyNone && m_profileNotifyStop` 才 return |
| F2 | `include/WeaselIPC.h`、`WeaselIPC/WeaselClientImpl.{h,cpp}` | `FocusOut` 返回值 `void → bool`，语义 = 「是否真的送达」；通知线程 deactivated 分支重试 3 次、间隔 120 ms，并写诊断日志 |
| F3 | `include/PipeChannel.h` `_Send()`、`WeaselIPC/PipeChannel.cpp` `_Connect()` | `_Send()` catch 后**重取** `_GetPipeHandle()` 再写；`_Connect()` 改为有界重试（3 s 预算，超预算抛 `ERROR_PIPE_BUSY`，`FILE_NOT_FOUND` 仍快速抛） |
| F4 | `include/WeaselIPC.h`、`include/RimeWithWeasel.h`、`RimeWithWeasel/RimeWithWeasel.cpp`、`WeaselServer/{resource.h,WeaselServerApp.cpp}` | 新增在场状态 `FluxingPresence{kUnknown,kPresent,kAbsent}` + 纯函数 `UpdateFluxingPresence()` / `ShouldShowQuickPanelForPresence()`；`FocusIn` 先置 present 再 show；`FocusOut` 先更新在场状态，再经 `m_onHideQuickPanel()`（= `PostMessage(HIDE)`）隐藏，与 show 共用 UI 线程队列；`ID_WEASELTRAY_QUICK_PANEL_HIDE 40021`；SHOW handler 增加在场闸门 |
| F5 | `RimeWithWeasel/RimeWithWeasel.cpp` `RemoveSession()` | `ipc_id == m_active_session` ⇒ presence = `kAbsent` + 请求隐藏。会话消失是不可伪造的事实，作为丢消息的兜底 |
| F6 | 新建 `include/FluxingDiagLog.h` | append-only 诊断日志，`FILE_APPEND_DATA` + `FILE_SHARE_READ\|WRITE`，单次 `WriteFile`，2 MB 上限，失败静默。落点 `%TEMP%\fluxing-profile-notify.log` |

### 设计约束（改动时必须继续遵守）

- **不改 `include/WeaselIPCData.h` 的线格式**，不改协议常量。
- **不动 `QuickPanelDialog::Hide()` 本体**（ADR 0013 D7 / ADR 0016 D1）——
  守卫只加在调用点。`Hide()` 还服务于需要用户明确手势的路径。
- **手动唤起路径（`Alt+,` / 托盘）不经过在场闸门** —— 那是用户显式意图。
- **`kUnknown` 允许显示** —— 服务端刚起时不能把面板锁死。
- **不在 `ActivateEx` 里主动 post「activated」** —— 会造成每次应用激活都弹面板。
- 诊断写文件只能发生在通知线程 / IPC 线程，**不能进 TSF 回调**（P2）。

---

## 5. 证据

### 5.1 活体探针（决定性）

`tmp/probe_quickpanel.py`（6,375 B），用法：

```powershell
& 'C:\Python314\python.exe' 'F:\soft\00selfmade\rime_claude\tmp\probe_quickpanel.py' full
```

输出（三态）：

```
FOCUS_IN(PROFILE_ACTIVATED)      => visible=True
FOCUS_OUT(param=0)               => visible=True
FOCUS_OUT(PROFILE_DEACTIVATED)   => visible=False
```

**意义**：服务端判定语义本身是对的。它把「判定逻辑写错了」这条最直觉的假设直接证伪，
把矛头指向投递链路与线程定序。

### 5.2 已证伪、不要再走的方向

- `ITfActiveLanguageProfileNotifySink` 签名错配 —— `msctf.h:8831` 已证伪。
- `clsid` 语义错配。
- `KillTimer` 跨线程失效。
- timer 让面板复活。
- 命名管道根本连不上。
- 服务端请求串行化。
- `Hide()` 本体有缺陷。
- wire 格式漂移。
- 服务端 `DLOG` 失效 —— 真因是 `include/no_logging.h` 把 `DLOG` 编译成空操作，
  故本轮改用 `include/FluxingDiagLog.h` 绕开。

### 5.3 单元级验证

`test/TestWeaselIPC/TestWeaselIPC.cpp` 新增：

```powershell
release\TestWeaselIPC.exe /verify-quickpanel-presence
```

覆盖 8 项断言（在场状态迁移矩阵 + SHOW 闸门 + 与 `ShouldHideQuickPanelOnFocusOut`
的一致性）。**本机跑这个用例需要先停掉装机版 `WeaselServer.exe`**，
否则它占着命名管道，mock server 绑不上。

### 5.4 构建

- 构建状态：**已通过**。`build.bat weasel` 返回 `BUILD_EXIT=0`，`weasel.sln`（`Platform=Win32`）
  全部项目成功。产物：`output\Win32\WeaselServer.exe` 2,267,648 B / sha16 `AFA6537E6A1951F1`；
  `output\weasel.dll` 1,672,192 B / sha16 `BE1C0378A6FE0F18`；
  `output\weaselx64.dll` 1,981,952 B / sha16 `4C6E130FE2215938`。
- 单元测试：**已通过**。`release\TestWeaselIPC.exe /verify-quickpanel-presence` 返回
  `TEST_EXIT=0`，末行 `verify-quickpanel-presence: PASS`（11 行断言全绿）。
- 日志：`tmp\build-weasel-2305.log`（历次：`build-weasel-2215.log` / `-2247.log` / `-2252.log`）
- 命令：`build.bat weasel`（内部走 MSBuild，`Platform=Win32` 给 `weasel.sln`，
  `Platform=x64` 给 `WeaselTSF.vcxproj`）。**发布禁用 xmake（A15/L127）。**

### 5.5 部署现状：**未部署（被本地安全策略阻塞）**

这是交接的**头号待办**，下一会话不要以为代码已上线。

- 本轮 shell 的硬安全策略禁止向受保护路径 `D:\Program Files\` 写入或删除
  ⇒ Agent **无法**把产物复制进装机目录，也没有绕过（不做「把路径塞进脚本再执行」这种规避）。
- 已产出可复现部署脚本：`tmp\deploy-20261102.cmd`（纯 ASCII，39 行）。动作序列：
  停 `WeaselServer` → 备份现有两个 DLL 到同目录 `.bak-20261102` → 覆盖
  `WeaselServer.exe` → 两个 TSF DLL 走「先 `ren` 旧文件、再 `move` 新文件」
  （被 `explorer.exe` 加载的镜像不能原地覆盖，但可以改名）→ 打印 sha256 → 重启服务端。
  脚本内置期望哈希前缀（server `AFA6537E…` / x64 `4C6E130F…` / x86 `BE1C0378…`）。
- **部署前实测到的异常（重要）**：`D:\Program Files\fluxing\weasel\WeaselServer.exe`
  **根本不存在**，且 `WeaselServer` 进程未运行；`uninstall.exe` 的 `DisplayIcon` 仍指向该路径，
  TSF CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` 仍指向 `weaselx64.dll`（存在）。
  另有一个**废弃嵌套目录** `D:\Program Files\fluxing\weasel\fluxing\weasel\WeaselServer.exe`
  （2,193,920 B / 2026-10-23），是早期 `/D=` 传错留下的残余，**不要**从那里拷。
  ⇒ 注册表只能证明「曾经装过」，不能证明「现在是完整安装」。部署脚本正是补上这个缺口。
- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 内**没有** `WeaselServer` 自启项，
  `schtasks` 也无同名任务 ⇒ 部署后服务端只在本次会话存活，**重启机器后不会自动起来**。
  恢复自启另需一步（`WeaselSetup.exe` 或手工补 Run 项），本轮未处理，见 §7。
- 备份落点：`tmp\deploy-backup-20261102\`（`weasel.dll` 1,670,144 B、`weaselx64.dll`
  1,978,368 B，均为部署前字节）。

---

## 6. 未提交改动清单（截至本文写作时）

```
## Fluxing...kizemo/Fluxing [ahead 3]
 M RimeWithWeasel/RimeWithWeasel.cpp     +48
 M WeaselIPC/PipeChannel.cpp             +11 -1
 M WeaselIPC/WeaselClientImpl.cpp        +18 -3
 M WeaselIPC/WeaselClientImpl.h           +5 -2
 M WeaselServer/WeaselServerApp.cpp      +25 (+1 include)
 M WeaselServer/resource.h                +5
 M WeaselTSF/WeaselTSF.cpp               +31 -2
 M docs/adr/README.md                     +1
 M include/PipeChannel.h                  +7 -1
 M include/RimeWithWeasel.h              +21
 M include/WeaselIPC.h                   +45
 m librime                                (脏子模块，永不提交)
 M test/TestWeaselIPC/TestWeaselIPC.cpp  +65
?? docs/adr/0029-quickpanel-hide-delivery-and-ordering.md
?? include/FluxingDiagLog.h
```

三个**未 push** 的旧 commit：

- `cc13c86` feat(fluxing) 短语注入
- `7c07862` docs(fluxing) CHANGELOG + lessons
- `7e20129` docs(agents) AGENTS.md

**push 刻意推迟**：commit 可逆、push 不可逆 ⇒ 等真机复测通过后再 `git push kizemo Fluxing`。

---

## 7. 下阶段计划（按顺序）

1. `build.bat weasel` 通过（当前动作）。
2. 停掉装机版 `WeaselServer.exe` → `release\TestWeaselIPC.exe /verify-quickpanel-presence` 全绿。
3. 部署到 `D:\Program Files\fluxing\weasel\`：
   `weasel.dll` / `weaselx64.dll` / `WeaselServer.exe`（**只覆盖程序目录，绝不碰
   `D:\Program Files\fluxing\user1\fluxing\`**）。
4. **请用户亲手切一次输入法**：切到火流猩 → 面板应出现；切到微软拼音 → 面板应消失。
   同时抓 `%TEMP%\fluxing-profile-notify.log` 与 `%LOCALAPPDATA%\Temp\rime.weasel\` 日志。
5. 通过后 → 写 `docs/adr/0029` 的 Status 从 `Proposed` 改 `Accepted`。
6. 逐文件显式 `git add`（**禁止 `git add .`**）→ `git status -sb` + `git diff --stat` → commit（不 push）。
7. 用户签字后再 `git push kizemo Fluxing`。

### 仍然挂起的旧诉求（与本 bug 无关，但同一会话遗留）

- **诉求① 真机 silent install**：`output\archives\fluxing-1.1.6-installer.exe /S /D=<path>`。
  验收 = 版本化 shim 指向新序号副本、无 `.stage2` marker、TIP 注册指向版本化 shim、
  装出的 `data\default.yaml` 为 18,217 B / sha `917509f6f2460230`。
  `/D=` 的值**不带引号**且**必须是最后一个参数**。
- **诉求③ 物理按键验收**：`Ctrl+.` / `Ctrl+/` / `Ctrl+Shift+3`。

---

## 8. 环境与命令（本机实测可用）

```powershell
# 构建
& 'F:\soft\00selfmade\rime_claude\build.bat' weasel > F:\soft\00selfmade\rime_claude\tmp\build.log 2>&1
$LASTEXITCODE

# 行尾 / BOM 核验（对任意文本文件）
$b=[System.IO.File]::ReadAllBytes($p)
$crlf=0;$lf=0
for($i=0;$i -lt $b.Length;$i++){ if($b[$i] -eq 10){ if($i -gt 0 -and $b[$i-1] -eq 13){$crlf++}else{$lf++} } }
$bom = ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
"size=$($b.Length) crlf=$crlf lf=$lf bom=$bom"
```

固定路径：

| 用途 | 路径 |
|---|---|
| 仓库根 | `F:\soft\00selfmade\rime_claude` |
| 装机根 | `D:\Program Files\fluxing\` |
| 部署目录 | `D:\Program Files\fluxing\weasel\` |
| 用户数据（**永不改**） | `D:\Program Files\fluxing\user1\fluxing\` |
| Python | `C:\Python314\python.exe` |
| MSBuild | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe` |
| makensis | `C:\Program Files (x86)\NSIS\Bin\makensis.exe`（**带 `Bin\`**） |
| BOOST_ROOT | `F:\b183` |

---

## 9. 踩过的坑（本轮新增，务必不要再踩）

- **`msbuild1.log` 会被残留 MSBuild 进程独占** ⇒ 新构建报 `MSB4104` 假错误。
  构建前先 `Get-Process MSBuild,cl` 清理，或换日志名。
- **补 setter 记得补成员**：给 `include/RimeWithWeasel.h` 加
  `SetOnHideQuickPanel()` 却漏了 `std::function<void()> m_onHideQuickPanel;`
  成员声明 ⇒ 4 处 `C2065`。成员区在 `m_active_session` 之后，
  同区已有 `m_onShowQuickPanel`。
- **用了 `fluxing::diag::AppendLine` 就必须 `#include "FluxingDiagLog.h"`**，
  否则报「`diag` 不是 `fluxing` 的成员」。
- **服务端 `DLOG` 是空操作**（`include/no_logging.h`），不要靠它取证。
- **glog 会轮转消失**，不能作为长期证据。
- **`Set-Location` 之后再 `Remove-Item`** 会被本地安全策略拦下；
  用绝对路径、不切目录、换新日志名而不是删旧文件。
- **`TsfProfileVerify.cpp` 的 `/verify-tsf-profile --allow-profile-switch` 会真实切换
  活动输入配置**，历史上曾导致 `dwm.exe` / `ImeService.exe` 崩溃 ⇒ 有三重守卫，
  **绝对不要在真实桌面跑**。

---

## 10. 已知文档漂移（未修，别当成本次引入的问题）

- `.specify/memory/project-knowledge.md:570` 的 `/D=` 带引号示例违反 L112/A14。
- `.claude/rules/commit-checklist.md:18` 路径形态错。
- `output/install.nsi` 46 处裸 LF。
- `FLUXING-CLAIMS-VERIFICATION.md` §3.11 四段式包名残留。
- `output\data` 缺 `*.gram` 与 `data\lua\*.txt`。
- `test\TestShortcutSettings\TestShortcutSettings.cpp:164` 写 cwd。
- `PhrasesDialog.cpp` 三处 `C4002`。

---

## 11. 相关文档

- `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md`（本改动，Status: Proposed）
- `docs/adr/0024-*.md`（spec 091，FocusOut 双语义的前置决策）
- `docs/adr/0027-*.md`（spec 096，FocusIn 判定共享纯函数）
- `.specify/memory/lessons-learned.md` L168（本轮沉淀）
- `include/WeaselIPC.h` 的 `ShouldShowQuickPanelOnFocusIn` /
  `ShouldHideQuickPanelOnFocusOut` / `UpdateFluxingPresence` /
  `ShouldShowQuickPanelForPresence`