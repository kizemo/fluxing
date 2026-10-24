# Handoff — v0.74.2 修复被拒绝,真因未找到 (2026-07-31)

> **承接**: `handoff-after-c2-v0.74.2-silentfail-fix-2026-07-31.md` (v0.74.2 build → user 测试 → 仍 fail)
> **状态**: ❌ v0.74.2 没修复无响应 bug。user 反馈 1 周工作失败,要求彻底排查
> **下一步**: 新会话从这张 handoff 接手,**不要**继承我之前的"System32 残留 DLL 是真因"假设,**重新**系统性排查

---

## 0. user 反馈 (2026-07-31)

> **"非常令人失望,一周的工作,你还是没修复无响应的 bug,你始终找不到正确的原因。"**
> **"问题依旧,使用管理员权限安装输入法,重启后,可以显示输入法设置栏,但只要开始输入,就会进入无响应状态,然后宿主软件关闭,explorer 关闭。"**
> **"好几个不相关的程序(Claude Code / dopus / HipsMain / aboboo)也都崩在同一处:都是在使用火流猩输入的时候,所以这个锅肯定就是在火流猩。"**
> **"请彻底排查 bug 原因,一次性修复所有可能,而不是一点点去尝试,浪费大量时间和 Token。"**
> **"请把反馈的上述问题,写入 handoff,另开新会话,为我撰写一句 Prompt,用于新会话。"**

---

## 1. user 给出的关键新事实(我之前没掌握)

### 1.1 输入触发,不是切换触发
- v0.74.1 装机后:**只切换输入法 → 设置栏正常显示,不崩**
- **只要开始输入中文 → 进入无响应 → 宿主软件关闭 + explorer 关闭**
- → 真因在"输入处理路径"上,不在"启动 / 初始化 / 注册"路径上

### 1.2 崩溃窗口 = 正在用火流猩输入
- Claude Code / dopus / HipsMain / aboboo 等不相关程序也是**在使用火流猩输入时**崩
- → 输入法进程/线程在输入瞬间做了某件事,**直接影响所有挂 TSF 的进程**
- → 不是 explorer 自身问题,不是 ntdll.dll 问题,是输入法进程主动触发了某种全局破坏

### 1.3 admin 权限装机已确认
- user 已用管理员权限装机 + 重启
- → 不是 admin token 缺失 → 不是 NSIS silent fail → 不是 cleanup 没跑
- → v0.74.2 的 IfErrors 修复方向错了:即使 cleanup 100% 跑通,bug 仍然在

---

## 2. 我之前 C1/C2 排查的盲点

我之前聚焦在 **"装机流程"** 上,假设"残留 DLL 导致 TSF 框架加载不兼容 shim"是真因。但 user 反馈揭示:

| 我之前的假设 | user 反馈证伪 |
|---|---|
| "System32 残留 DLL 让 TSF 加载错 shim" | input 触发,不是 switch 触发 |
| "explorer crash 是输入法副作用" | "都是在使用火流猩输入的时候崩" → 输入法是直接触发者 |
| "admin 权限问题" | user 已确认 admin 装机 |

**正确方向**: 排查**输入瞬间**做的事 → 找出能让所有 TSF 客户端进程崩的代码路径。

---

## 3. 新会话必须做的事(我自己之前的认知盲区)

### 3.1 不要继承"残留 DLL"假设
- v0.74.2 已 build,user 已测,**问题依旧**
- 不要再花时间改 install.nsi cleanup 逻辑

### 3.2 真因排查清单(按 user 反馈"输入瞬间"重新定位)

#### A. WeaselServer 在 input callback 里做了什么
- 查看 `WeaselServer/WeaselServerApp.cpp` / `WeaselServerImpl.cpp` 里所有 input 处理回调(`OnKeyEvent` / `OnComposition*` / `OnCandidatePage*`)
- 看里面有没有 `CreateFile` / `WriteFile` / `SendMessage(...)` / `TerminateProcess` / `system(...)` / `taskkill` / `cmd /c` 等危险调用
- L04 + L07 + L17 红线: TSF 回调里禁止阻塞 I/O / 同步注册表 / `BeginWaitForSingleObject` / `TerminateProcess` / `taskkill`
- 看是否有任何代码路径能在 input callback 里把整个系统弄挂

#### B. WeaselServer 是否 fork 出子进程 / 启动其它进程
- `WeaselServer/PhrasesDialogIPC.cpp` 创建了 `FluxingPhrasesDialog.exe`
- `WeaselServer/WeaselServerApp.cpp` 可能调 `WeaselDeployer.exe` / `start_service.bat` / `stop_service.bat`
- 这些子进程如果在 input callback 里同步启动,会卡 input 线程 → 影响所有 TSF 进程

#### C. PhrasesDialog 进程对 input callback 的影响
- v0.51+ 之后 PhrasesDialog 已经 out-of-process(独立 FluxingPhrasesDialog.exe + named pipe)
- 但 v0.51 之前的某些残留调用 / 同步 IPC 是否还在 input callback 里
- 是否有任何 path 让 `OnKeyEvent` 等同步等 PhrasesDialog response → TSF input pipeline 卡死 → 所有 TSF 进程(包括 explorer)假死

#### D. explorer.exe 关闭的真相
- 之前我看到 explorer crash `0xc0000374` HEAP_CORRUPTION
- 但**所有 crash 都发生在输入瞬间**,意味着是输入法进程主动把 explorer 弄崩的
- 看 WeaselServer 是否有 `EnumWindows` / `SendMessage(WM_CLOSE)` / `TerminateProcess(explorer)` / `ExitWindowsEx`
- 看是否有任何全局 hook / global message broadcast

#### E. rime.weasel.*.log 在 input 瞬间的报错
- 让 user 跑 `_diag_v0741_logs.ps1` 看 `rime.weasel.INFO.WARN.*.log` 在输入瞬间有没有 `ERROR` / `WARN` / `EXCEPTION` / `STACK`
- 看是否有 librime engine 异常、OpenCC 异常、leveldb 异常

#### F. dump 文件分析
- `%LOCALAPPDATA%\CrashDumps\` / `C:\Users\Duanyi\AppData\Local\CrashDumps\` 是否有 WeaselServer.exe / FluxingPhrasesDialog.exe 的 dump
- 让 user 跑 `_diag_v0741_dumps.ps1`(需新写)枚举最近 24h 的 CrashDumps
- dump 是真因的唯一直接证据

#### G. Process Monitor / Process Explorer 现场抓 trace
- 让 user 跑 ProcMon(微软官方),过滤 WeaselServer.exe + FluxingPhrasesDialog.exe + explorer.exe + explorer 子进程
- 输入瞬间的 file / registry / network / process 操作能看到真凶

### 3.3 不要做的事

- ❌ 不要继续改 `output/install.nsi` cleanup 逻辑(已证明不是这条)
- ❌ 不要假设 System32 残留 / admin token / 静默模式是真因
- ❌ 不要继续花时间在 stage2 + schtasks + LogonCommand 路径
- ❌ 不要"先做小的修复看 user 反馈"——这是 user 明确禁止的反模式
- ❌ 不要 commit / push / SFTP 任何东西——这次只诊断不修

---

## 4. 现状(2026-07-31)

- 工作区未 commit 改动:
  - `output/install.nsi` +24/-3 行(CleanupLegacySystem32WeaselImpl macro IfErrors + weasel.old.dll exact-match + completion log)
  - `.specify/memory/lessons-learned.md` +L##-PhaseL-9.10
  - `test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1` 新建
  - `release/fluxing-0.19.0.74.2-installer.exe` 新 build(md5 `6ee983a365648d923162c0c3121aee68`)
- 这些 v0.74.2 改动**无效**(user 已验证),不 commit 不 push,**留给下个真因修复版本一起回滚或重做**
- 5 binary md5 未变(weasel.dll / weaselx64.dll / WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe)

---

## 5. 新会话 Prompt(给 user 直接用)

参见同目录 `prompt-next-session-input-crash-2026-07-31.md`。

---

## 6. user 已确认的行为约束

- "请彻底排查 bug 原因,一次性修复所有可能,而不是一点点去尝试,浪费大量时间和 Token。"
- "请把反馈的上述问题,写入 handoff"
- "另开新会话,为我撰写一句 Prompt,用于新会话"

新会话必须遵守:
- **不要试错**:一次拿全 evidence(从源头读 WeaselServer 输入路径所有 callback + 抓 dump + 抓 ProcMon),直接定位真因,再写修复
- **不要 commit 任何东西**——只诊断,等 user 看完 handoff + 真因报告再决定
- **不要碰 install.nsi cleanup**——证明不是这条
- **不要碰 stage2 / schtasks / LogonCommand**——证明不是这条
- **可以新建 `_diag_*.ps1` 取证脚本**(gitignored 命名,_check_*.ps1 同样 gitignored),但不要 commit

---

*End of handoff. v0.74.2 修复被拒绝,真因 = 输入瞬间代码路径。下个会话从"input callback 排查"开始,不再假设残留 DLL。*