# 批次 AQ · 含云构建「启动约 30.2 s 自退」根因闭环 + 崩溃修复 + 1.3.1 重新出包

> **一句话现状**：AP 批次列为「原因未查明」的「含云构建无会话态约 30.2 s 自退」已在本批**定位并闭环** —— 实为**访问冲突崩溃 `0xC0000005`**（不是优雅退出），根因是云同步 `BeginMaintenance` 触发 `rime_api->finalize()` 销毁 `userdb` 后，紧随其后的 levers `user_dict_iterator_init` 对空组件解引用；修复已落地（**Start/End 配对** + **rime 生命周期命名信号量互斥** + **levers 全量 SEH 护栏**）并实测通过（**存活 ≥ 60 s 未退出**、备份路径首次真实跑通、**crash 目录无新增 dump**）；门禁全绿（云单测 `PASS=453 FAIL=0`、生产构建 **0 警告 0 错误**）；**1.3.0 作废**（含崩溃缺陷），已重新出包 **1.3.1**。**未闭环**：装机验证（须注销/重启，PPL）、双机 AC 7 条、用户在场 AC 4 条、输入延迟两组对照、**restore 路径端到端**、以及**残留缺陷「每轮同步清空当前 TSF 会话」**。

## 1. 背景与目标

### 1.1 起因

批次 AP 的未闭环清单 §4.6 记：**含云构建无会话态约 30.2 s 自退**（v3/v4 两次一致，pre-cloud 同条件存活 ≥ 70 s，**原因未查明**）。当时只能判定「与云模块相关」，无法定责。

### 1.2 本批目标（按顺序）

1. **定性**：30.2 s 自退到底是优雅退出还是崩溃？⇒ 必须拿到退出码与转储证据。
2. **定根因**：源码级定位到具体空指针解引用点，并要求**运行时 trace 交叉验证**（不能只有源码推理）。
3. **修**：在**不触发 IPC 四边同步红线**的前提下修掉进程崩溃，并保留同步功能。
4. **验证**：修复后进程存活 ≥ 60 s，且**备份路径真实跑通**（导出成功），而非「不崩就算过」。
5. **出包**：修完必须重新出包；1.3.0 含缺陷 ⇒ 走 **1.3.1**。
6. **收尾**：ADR + 四处文档回写 + 提交 + handoff。

### 1.3 约束（来自仓库规则）

- **IPC 四边同步红线**：`include/WeaselIPCData.h` 与协议的任何变更 → server / TSF / deployer 三端 + 一个 ADR + 一个测试，**一次性提交**。⇒ 本批**决定不动 `WeaselIPCData.h`**，也不动 `WeaselIPCServer/WeaselServerImpl.cpp`。
- **`RimeWithWeasel/RimeWithWeasel.cpp` 有非本批脏改动**（spec 045，+118 行）⇒ **修复全在 Cloud 后端侧**，该文件一个字节都不碰。
- **MSVC C2712**：含 `__try` 的函数不得有需展开的局部变量 ⇒ **SEH 护栏函数必须 POD-only**。
- **命名信号量配对释放**：必须先 `ReleaseSemaphore` 再 `CloseHandle`（直接 Close 会把锁永久留在已占用状态）。
- **用户可见字符串中文**（constitution P2/P5）。

## 2. 本批完成

### 2.1 判据更正：不是「自退」，是访问冲突崩溃

| 证据 | 内容 |
|---|---|
| 退出码 | `0xC0000005`（STATUS_ACCESS_VIOLATION）⇒ **崩溃**，不是优雅退出 |
| minidump | `%LOCALAPPDATA%\fluxing\crash\fluxing-20261112-195402-c0000005.dmp`（15 242 077 B）—— 由本批新增的 minidump 两级建目录逻辑捕获 |
| 时间窗 | 修复前：含云构建 **30.6–30.9 s 必崩**（v3/v4/v5 多次一致）；pre-cloud 对照 **≥ 70 s 存活** |
| 归因范围 | **只由含云二进制产生** —— 与 PPL 签名、注册表、安装目录、工作区副本**均无关** |

> 取证通道本身有缺陷：最初的 minidump 建目录逻辑**只建一级目录**，真实路径 `%LOCALAPPDATA%\fluxing\crash` 是两级 ⇒ 前几轮 dump 落盘失败、错判为「无 dump」。**已修为两级建目录**（`WeaselServer/WeaselServer.cpp:36-67`）并保留。

### 2.2 根因链（源码 + trace 双证）

```
CloudSyncService::SyncOnce (CloudSyncService.cpp:900)
  → backend_->BeginMaintenance()
    → RimeWithWeaselHandler::StartMaintenance()      (RimeWithWeasel.cpp:1041)
      → Finalize()                                     (RimeWithWeasel.cpp:258)
        → rime_api->finalize()
          → Registry::Clear()  ⇒ 销毁 userdb 组件
  → levers user_dict_iterator_init
    → UserDb::Require("userdb") ⇒ nullptr
    → component->extension()   ⇒ 空指针解引用 ⇒ 0xC0000005
```

**为什么调用点判空挡不住**：`RimeFindModule("levers")` 在 finalize 之后**仍返回非空** —— 因为 `UnloadModules` 只清 `loaded_`，`modules_` 保留 ⇒ 拿到的是**已卸载模块的空壳** ⇒ `module != nullptr` 不代表内部组件还活着。这是本缺陷最关键的认知点。

**为什么 `Registry::Clear()` 是分水岭**：`finalize` 之前 `userdb` 组件在 Registry 里可用；`finalize` 之后用户库已释放（leveldb LOCK 已放），但**模块外壳还在** ⇒ 后续任何 levers 用户库操作都踩空。

### 2.3 修复方案（三条修法 + 一个新头文件）

| # | 改动 | 位置 | 作用 |
|---|---|---|---|
| 1 | **`BeginMaintenance` 改为 Start/End 配对**：取生命周期锁 → `StartMaintenance()`（finalize 释放用户库）→ **立刻 `EndMaintenance()`**（`m_disabled` 时 `Initialize()` 恢复注册表）→ 再做导出/回写 | `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp` | 让 rime 在**操作用户库之前**先回到可用态 ⇒ 不再拿着空壳调用 |
| 2 | **`EndMaintenance` 只释放锁**（不再承担恢复语义） | 同上 | 职责单一，避免与配对逻辑耦合 |
| 3 | **5 个 POD-only SEH 护栏**覆盖全部 levers 调用 | 同上 | 访问冲突 ⇒ **本轮同步失败 + 中文错误提示**，进程**永不崩**（兜底） |
| 4 | **新增 `include/FluxingRimeLifecycle.h`**：Win32 **命名信号量** `Local\Fluxing.RimeLifecycle` 做跨模块唯一互斥 | 新文件 | 云同步与部署器 `OnStartMaintenance` 并发时互斥；取锁上限 **2 s**，失败即**跳过本轮**（不阻塞输入） |

**新增的中文错误文案**（5 条，全部在 `CloudRimeUserDictBackend.cpp`）：`输入法引擎正忙` / `输入法引擎当前不可用` / `输入法引擎未能回到可用状态` / `用户词典维护态未持有` / `没有找到可同步的用户词典`。

> ⚠️ **MSVC C2712**：护栏函数内**只允许 POD 局部变量**（无析构、无需展开）。这是本批实现时的硬约束。

### 2.4 验证（决定性）

`tmp/AQ-trace-probe.ps1` 复跑，修复后：

- **进程存活 ≥ 60 s 未退出**（修复前 30.6–30.9 s 必崩）；trace 诊断版与**去 trace 发布件各跑一轮**，结果一致。
- trace 走完整链路：

```
begin_maintenance_enter → begin_maintenance_done ok=1 → export_enter
→ export_levers acquired=1 → export_iter_init_ok → export_dicts n=1
→ export_backup rime_ice ok=1 → export_done exported=1
→ end_maintenance_enter → end_maintenance_done ok=1
```

⇒ **备份路径首次真实跑通**（真实导出 1 个词典 `rime_ice`）。

- **修复件运行期间无新增 dump**；探针收尾于 19:57:05 拉起**旧装机版 1.2.1**（`WeaselServer.exe /quit` 停旧实例 → 拉起新实例），该实例于 **19:57:35** 崩溃并落第 4 枚 dump —— **minidump 内嵌模块路径判定**（`ModuleListStream` 的 UTF-16LE 路径）= `D:\Program Files\fluxing\weasel\WeaselServer.exe` ⇒ **该 dump 来自旧装机版，不是修复件**；且拉起→崩溃间隔 ≈ **30 s**，与修复前判据逐字吻合。**⇒ 同场次自然 A/B**：修复件存活 ≥ 60 s，旧装机版 30 s 必崩。
- **四枚 dump 的归属（按内嵌模块路径逐一判定）**：

  | dump | 内嵌 exe 路径 | 归属 |
  |---|---|---|
  | 19:42:12 | `F:\soft\00selfmade\rime_claude\tmp\AP-ctl-run4\WeaselServer.exe` | **修复前**的工作区副本（崩溃的那次） |
  | 19:42:44 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` | 旧装机版 1.2.1 |
  | 19:54:02 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` | 旧装机版 1.2.1 |
  | 19:57:35 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` | 旧装机版 1.2.1（探针收尾拉起的那次，**修复后仍崩**） |
- **二进制自证**：新中文提示与 `Local\Fluxing.RimeLifecycle` 以 **UTF-8 字节**在 `output\Win32\WeaselServer.exe` 中命中（`/utf-8` + 窄字符串字面量 ⇒ 产物里是 UTF-8 字节，**用 UTF-16 搜索会假红**）；临时 trace 标记 `fluxing-cloud-trace.log` 在发布件中 **0 命中**（trace 已移除）。

### 2.5 临时 trace 通道已移除

`tmp/AQ-remove-trace.ps1` 删除 `RimeUserDictBackend::Trace` 定义块（1 处）+ **18 处调用**。仓库内 `fluxing-cloud-trace` / `Trace(` 残留 **0**；文件 15 607 → 13 668 B。**trace 通道只在取证期存在，不进发布件。**

### 2.6 门禁

| 门禁 | 结果 | 证据 |
|---|---|---|
| 云单测 | **`PASS=453 FAIL=0`**（`VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0`）—— 与 AO/AP 基线**逐字一致**，无回归 | `tmp/AQ-cloudtest.log` |
| 生产构建 | `build.bat weasel` 两轮均 **0 警告 0 错误** | `tmp/AQ-build2.log`、`tmp/AQ-build3.log` |
| makensis | **8 条 `warning 7010`**（ARM/ARM64/ARM64X dll ×6 + `data\*.gram` + `data\lua\*.txt`）—— **既有可选产物告警**，非本批引入 | `tmp/AQ-installer.log` |

### 2.7 发版：1.3.0 作废 → 1.3.1

**决策**：1.3.0 安装包已产出且**含崩溃缺陷**；重出同名同版本 = **L107「同版本号 + 不同内容 = 发布事故」** ⇒ **1.3.0 作废**，发布 **1.3.1**。

**四处版本真相源同步**：`version.txt`（`1.3.1`，**入库**，5 字节无尾换行形态保住）/ `CHANGELOG.md`（顶部 `## [1.3.1]`，**入库**）/ `env.bat`（`FLUXING_VERSION=1.3.1`、`PRODUCT_VERSION=FILE_VERSION=1.3.1.0`、`VERSION_PATCH=1`、`WEASEL_BUILD=0`、`RELEASE_BUILD=1`，**被 gitignore**）/ `weasel.props`（`VERSION_PATCH=1`、`PRODUCT_VERSION=FILE_VERSION=1.3.1.0`，**被 gitignore**）。

`WEASEL_BUILD` 保持 **0** 的理由：shim 名 `weaselx64.1.3.1.0.dll` **从未存在** ⇒ 无 L107 风险。

**产物**：

| 项 | 值 |
|---|---|
| 路径 | `output/archives/fluxing-1.3.1-installer.exe` |
| 大小 | **42 963 252 B** |
| 时间 | 2026-11-12 20:00:50 |
| sha256 | `71286E7671052F3783E54F3338C83C1630254880533E264691FDD1F3C6EC5256` |
| sha16 | **`71286E7671052F37`** |
| 内嵌 `WeaselServer.exe` | 2 700 800 B / FileVersion & ProductVersion 均 **`1.3.1.0`** / sha16 `C538432FB2F45386` |

### 2.8 文档回写（本批）

| 文件 | 改动 |
|---|---|
| `docs/adr/0061-rime-lifecycle-paired-maintenance-and-seh-guards.md`（**新建**） | 11 394 B，Status Accepted；4 条决策（Start/End 配对 / 命名信号量互斥 / SEH 护栏 / 先修取证安全网）+ 5 条被否决方案 + 验证段 + 未闭环段 |
| `docs/adr/README.md` | 索引加 0061 行 |
| `docs/research/fluxing-cloud-2026-11/perf-baseline.md` | 遗留 6 划线改「已闭环」 |
| `docs/research/fluxing-cloud-2026-11/compliance-check.md` | 遗留 3 划线改「已闭环」 |
| `docs/research/fluxing-cloud-2026-11/acceptance-matrix.md` | 新增 **§2.1 批次 AQ 追加**（崩溃性质/归因/根因/修复/验证/门禁/发版/残留缺陷 8 行） |
| `.specify/specs/100-fluxing-cloud-account-sync/tasks.md` | T602 ③ 划线改「已闭环」；**T604 段追加批次 AQ**（1.3.0 作废 → 1.3.1 + 根因 + 修法 + 验证 + 残留缺陷） |
| `WeaselServer/Cloud/CloudMaintenanceHost.h` | 注释改写为 ADR 0061 新语义 |
| `.specify/memory/lessons-learned.md` | 追加 **L216 / L217 / L218**（自退先看退出码 + 取证安全网先自证落盘 / SEH 降级 + POD-only / 命名信号量与 Release-then-Close）；**BOM 保留、CRLF 全保留（裸 LF=0）** |
| `CHANGELOG.md` | 顶部新增 `## [1.3.1]`（缺陷、根因链、修法、实测、门禁、残留缺陷、「1.3.0 作废」、装机前置） |
| `CLAUDE.md` §2 | 「本批交接指针」更新为 AQ，AP 降为「上一批」 |

## 3. 已验证 / 推断 / 未验证

| 结论 | 标签 | 依据 |
|---|---|---|
| 30.2 s 自退 = `0xC0000005` 访问冲突崩溃 | **已验证** | 退出码 + minidump 文件名 + dump 大小 |
| 崩溃只由含云二进制产生 | **已验证** | 含云 30.2 s 崩 vs pre-cloud ≥ 70 s 存活，多次一致 |
| 根因 = finalize 销毁 userdb ⇒ levers 空指针解引用 | **已验证**（源码 + trace 双证） | trace 中 `export_iter_init` 前后与崩溃点吻合；`user_db.h:100-109`、`level_db.cc:54-58` |
| `RimeFindModule("levers")` 仍非空是因 `UnloadModules` 只清 `loaded_` | **已验证（源码）** | `rime_api_impl.h:51-63`、`service.cc:76-83` |
| 修复后进程存活 ≥ 60 s 且备份跑通 | **已验证** | `tmp/AQ-trace-probe.txt`（诊断版 + 发布件各一轮） |
| 云单测 `PASS=453 FAIL=0` / 构建 0 警告 0 错误 | **已验证** | `tmp/AQ-cloudtest.log`、`tmp/AQ-build3.log` |
| 1.3.1 产物与内嵌版本 | **已验证** | `tmp/AQ-installer.log` |
| 装机后行为正常（用户输入不被打断） | **未验证** | 装机未做；且**残留缺陷**：每轮同步仍 finalize + Initialize rime ⇒ **当前 TSF 会话被清空** |
| **restore（回写）路径在真实云数据上端到端可用** | **未验证** | 需第二台设备产生「云端版本更新」的拉取场景；代码侧已由 `SafeRestoreUserDict` 护栏覆盖，与**已验证的 export 路径共享同一前提**（rime 已恢复 + 用户库无人占用 ⇒ leveldb LOCK 可得） |
| 修复对**部署器** `OnStartMaintenance` 并发场景同样安全 | **推断** | 锁 + SEH 护栏把「崩进程」降级为「本轮失败」；未做并发压测 |
| 1.3.1 装机后 30 s 崩溃消失 | **未验证** | 装机未做 |

## 4. 未闭环清单

### 4.1 ⚠️ 装机 1.3.1（**最高优先 · 需用户在场**）

- 产物：`output/archives/fluxing-1.3.1-installer.exe`（42 963 252 B，sha16 `71286E7671052F37`）。
- **前置：`WeaselServer.exe` 是 PPL 签名进程 ⇒ 装机前必须注销 / 重启系统**，否则仍跑旧映像（磁盘哈希不能证明在跑的是新码）。
- **1.3.0 安装包作废，勿安装。**
- **当前装机版仍是 1.2.1.0（含云）⇒ 每启动约 30 s 崩溃一次** —— 这是用户当前机器上的真实状态。

### 4.2 双机 AC 7 条（AC-03 / 04 / 05 / 08 / 09 / 13 / 14）

需**第二台设备**或**第二个 Windows 账户**。`CloudCredentialStore` 是 **Windows 用户级全局单例** ⇒ **两套 Profile 目录不够**，必须真正独立的机器/账户。

### 4.3 用户在场 AC 4 条

| AC | 动作 |
|---|---|
| AC-01 | 用户**亲手**用原口令登录 `kizemo@163.com`（服务端已 `email_verified=1`，口令明文不存 ⇒ 无法代登） |
| AC-02 | 登录态下重启输入法服务与系统，观察**自动续期** |
| AC-06 | 按 **exe 路径**做防火墙出站阻断，验断网降级（规则须在 `.ps1` 的 `try/finally` 里，否则会残留阻断） |
| AC-10 | 点「清空云端数据」后回查 D1 `sync_items` **0 行** + R2 **0 对象** |

### 4.4 输入延迟两组对照（同步开 / 关）

需**登录态 + 用户在场 + 人工体感**（IME 自绘窗跨进程抓不到）。仅有结构性证据：引擎独立后台线程、`NotifyLocalChange` 只置标志、推拉不在 TSF 回调内发起。

### 4.5 ⚠️ 残留缺陷：每轮同步清空当前 TSF 会话

**现象**：每轮同步仍会 `finalize` + `Initialize` rime ⇒ **当前 TSF 会话被清空**，用户进行中的输入可能被中断。

**彻底解法（二选一，需另立 ADR）**：
1. **复制 userdb 目录后另开 leveldb** —— 备份在独立目录上跑，rime 本身不动；
2. **独立进程执行备份** —— 备份进程只读用户数据目录，主进程零副作用。

> 本批的修法（Start/End 配对）解决的是**崩溃**，**不解决**会话被打断。二者应分开验收。

### 4.6 严格同状态 A/B 复跑（AC-11 升级为实测，可选）

B 组须在 `tmp\AP-ctl-run4\data\build` 稳定后复跑，且**不得与构建并发**。

### 4.7 AC-07 补强项（不改结论）

真实客户端 HTTPS 抓包。

## 5. 证据路径

| 证据 | 路径 |
|---|---|
| 崩溃判据（退出码 / dump / 存活窗口 / trace 全链路） | `tmp/AQ-trace-probe.txt` |
| 探针运行日志（两轮） | `tmp/AQ-probe-run.log`、`tmp/AQ-probe-run2.log` |
| 生产构建（0 警告 0 错误） | `tmp/AQ-build2.log`、`tmp/AQ-build3.log` |
| 云单测（`PASS=453 FAIL=0`） | `tmp/AQ-cloudtest.log` |
| 出包日志（EXIT=0 + 8 条 warning 7010 + 产物清单） | `tmp/AQ-installer.log` |
| 移除 trace 的脚本 | `tmp/AQ-remove-trace.ps1` |
| 版本升级脚本 | `tmp/AQ-version-bump.ps1` |
| lessons 追加片段 | `tmp/AQ-lessons-append.md` |
| minidump | `%LOCALAPPDATA%\fluxing\crash\fluxing-20261112-195402-c0000005.dmp`（3 枚旧件：19:42:12 / 19:42:44 / 19:54:02） |
| ADR 0061 | `docs/adr/0061-rime-lifecycle-paired-maintenance-and-seh-guards.md` |
| 本批改动清单 | `git show --stat <本批提交>` |

## 6. 本轮工具坑（勿重犯）

1. **`version.txt` 在 HEAD 里是 5–6 字节、末尾无换行** ⇒ 改版本号**必须以 HEAD 字节为模板做等长替换**（`git cat-file blob HEAD:version.txt`），写成带尾换行 ⇒ L0 判红（必须 100% CRLF）。
2. **`env.bat` 用 `set "KEY=VALUE"` 带引号格式** ⇒ 正则 `^SET\s+KEY=` **整组漏匹配**。
3. **MSVC C2712**：含 `__try` 的函数不得有需展开的局部变量 ⇒ **SEH 护栏必须 POD-only**。
4. **命名信号量**：配对释放必须**先 `ReleaseSemaphore` 再 `CloseHandle`**（直接 Close 会把锁永久留在已占用状态）。
5. **命名信号量取锁要有上限**（本批 2 s）⇒ 失败即跳过本轮，**不得无限等待阻塞输入线程**。
6. **`/utf-8` + 窄字符串字面量** ⇒ 产物里中文是 **UTF-8 字节**；用 UTF-16 搜索会假红。
7. **`WeaselServer.exe` 是 PPL** ⇒ `taskkill` 禁用，只能 `WeaselServer.exe /quit`；**工作区副本**才可 `Stop-Process -Force`。
8. **minidump 目录要建两级**：`%LOCALAPPDATA%\fluxing\crash`；只建一级 ⇒ dump 落盘失败、错判「无 dump」。
9. **取证安全网先自证**：加的 trace/日志通道**本身要先验证落盘**，否则会得到「假无证据」。
10. **`git commit -F -` 不吃 stdin** ⇒ 消息写临时文件再 `git commit -F <file>`。
11. **提交前必须 `git status -sb` + `git diff --stat`**，排除非本批脏文件（本次排除 `RimeWithWeasel/RimeWithWeasel.cpp`、`librime` 子模块、3 个 `docs/design/quickpanel-v6/design-demos/*.html`）。
12. **`Copy-Item -LiteralPath "$inst\*"` 不展开通配符**；`$Pid` 是只读常量；`Set-Location` 后写文件须用**绝对路径**；外层 shell 是 **PowerShell**，复杂逻辑落 `.ps1` 再 `pwsh -File`。
13. **无调试器可用**：cdb / windbg / procdump / dumpchk / vsjitdebugger 全缺，已有 minidump 无法解析 ⇒ 根因定位靠**源码 + 自建行级 trace**，不要指望事后解栈。
14. **空闲 CPU 测量不得与构建并发**（AP 教训）。

## 7. 下一批建议顺序

1. **用户装机 1.3.1（注销/重启后）** ⇒ 验「不再 30 s 崩溃」+ 顺手完成 **AC-01 / AC-02 / AC-06 / AC-10** 四条用户在场 AC。**理由**：这是唯一能把「崩溃已修复」从代码层推到真机的路径，且用户本来就要装。
2. **双机 AC 7 条**（需第二台设备/账户）。**理由**：占矩阵 7 条，是全绿的最大缺口；且它同时是 **restore 路径端到端验证**（§4.5）的载体 —— 第二台设备登录后拉到云端版本更新的词库，正好验证 `SafeRestoreUserDict`。
3. **另立 ADR 处理「会话被打断」残留缺陷**（§4.6）。**理由**：这是用户可感知的体验缺陷，且方案分叉（复制 userdb 目录 vs 独立进程）需要 ADR 拍板，不能边改边定。
4. **输入延迟两组对照**（需登录态 + 人工体感）。**理由**：依赖第 1 步的登录态。
5. **AC-07 真实客户端 HTTPS 抓包**（补强项）。**理由**：不改变结论，可最后做。
6. **严格同状态 A/B 复跑**（可选）。**理由**：方向性结论已成立，仅为把 AC-11 从「推断」升级为「实测」。

---

## 附：必读文档（完整绝对路径）

| 用途 | 路径 |
|---|---|
| **本 handoff（唯一真源）** | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-12-batch-aq-rime-lifecycle-crash-fix.md` |
| 上一批 handoff（背景） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-12-batch-ap-ac11-attribution-and-release-1-3-0.md` |
| spec 任务书 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md`（T601–T604 段） |
| spec 正文 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` |
| **本批 ADR** | `F:\soft\00selfmade\rime_claude\docs\adr\0061-rime-lifecycle-paired-maintenance-and-seh-guards.md` |
| 上一批 ADR（CPU 口径） | `F:\soft\00selfmade\rime_claude\docs\adr\0060-idle-cpu-single-core-window-and-attribution-method.md` |
| 仓库法则 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` · `F:\soft\00selfmade\rime_claude\AGENTS.md` §9 |
| 宪法 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 验收矩阵 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\acceptance-matrix.md`（§2.1 为本批追加） |
| 合规核查 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\compliance-check.md` |
| 性能基线 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\perf-baseline.md` |
| 核心改动文件 | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudRimeUserDictBackend.cpp` · `F:\soft\00selfmade\rime_claude\include\FluxingRimeLifecycle.h` · `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServer.cpp` |
