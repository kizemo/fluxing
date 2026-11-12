# ADR 0061 — 用户词典同步的 rime 生命周期口径：Start/End 必须配对，levers 调用一律加 SEH 护栏，生命周期操作必须互斥

- **Status**: Accepted
- **Date**: 2026-11-12
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（阶段 D · T305 用户词典后端）
- **Revises**: `CLAUDE.md` §2「必须 `StartMaintenance` / `EndMaintenance` 包裹 `RimeLeversApi::{export,import}_user_dict`」的 **rationale**（字面要求仍满足；理由由「finalize 释放锁」改为「Start+End 配对恢复 rime 后再独占用户库」）
- **Lessons**: `.specify/memory/lessons-learned.md` L216–L218
- **Supersedes / superseded by**: —（新增）

---

## 背景

批次 AP 出包 `fluxing-1.3.0-installer.exe` 后发现一个**会杀死常驻进程**的生产缺陷：含云构建启动约 **30.2 s** 后 `WeaselServer.exe` 自行退出。

### 定位过程（结论先于决策）

1. **2×2 缺格实验**（`tmp/AQ-selfexit.ps1`）：

   | 单元格 | 运行位置 | 二进制 | 结果 |
   |---|---|---|---|
   | CELL-1 | 装机目录 | 含云 | 30.9 s 退出 `0xC0000005` |
   | CELL-2 | 工作区副本 | 含云 | 30.6 s 退出 `0xC0000005` |
   | CELL-3 | 工作区副本 | pre-cloud（`WeaselServer.exe.bakL`） | 存活 ≥ 60 s |

   ⇒ 崩溃**只与「含云二进制」相关**，与安装目录 / PPL / 注册表 / 工作区副本无关；且**不是优雅退出**，是访问冲突。

2. **崩溃点切分**（`tmp/AQ-crashprobe.ps1`）：把 `FLUXING_CLOUD_BASE_URL` 指向 `http://127.0.0.1:1`（GRID-A）与真实地址（GRID-B）**同样崩** ⇒ 崩溃发生在首轮同步拿到网络响应**之前**。

3. **崩溃时刻 = 云引擎 30 s 定时器**：`CloudAccountDialog.cpp:278` `options.startup_delay_ms = 30 * 1000`；`CloudSyncEngine.cpp:62` `startup_at_ms_ = SteadyNowMs() + startup_delay_ms`；凭据 `Target: LegacyGeneric:target=Fluxing:CloudAccount` 存在 ⇒ 构造期 `RestoreFromStore()` 成功 ⇒ 构造期 `StartEngine()` ⇒ **30 s 后跑首轮同步**。

4. **崩溃安全网自身有缺陷**（已修）：`WeaselServer/WeaselServer.cpp::WriteMinidumpOnCrash` 原来只建**一级**目录 ⇒ 父目录 `%LOCALAPPDATA%\fluxing` 不存在 ⇒ dump **从未落盘**，此前所有「查不到 dump」的困惑都源于此。

5. **trace 实证**（决定性）：临时埋点写入 `%TEMP%\fluxing-cloud-trace.log`（行级原子 ⇒ 崩溃前的最后一行就是崩溃点）：

   ```
   19:42:12.188 begin_maintenance_enter
   19:42:12.188 begin_maintenance_done ok=1
   19:42:12.190 export_enter
   19:42:12.191 export_levers acquired=1
   19:42:12.191 export_iter_init_call      ← 崩在这里（无 export_iter_init_ok）
   ```

   进程存活 **31.9 s** 后 `ExitCode = -1073741819 (0xC0000005)`；修好建目录后 minidump 成功落盘：`%LOCALAPPDATA%\fluxing\crash\fluxing-20261112-194212-c0000005.dmp`（15 242 255 B）。

### 根因链（源码逐行锁定，与 trace 完全吻合）

| # | 位置 | 事实 |
|---|---|---|
| 1 | `WeaselServer/Cloud/CloudSyncService.cpp:900` | `SyncOnce` 调 `backend_->BeginMaintenance()` |
| 2 | `WeaselServer/Cloud/CloudUserDictIo.cpp:312-338` | `CallCloudBeginMaintenance` → 维护态钩子 |
| 3 | `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp`（`RegisterCloudMaintenanceHostImpl` 的 lambda） | `g_maintenance_host->StartMaintenance()` |
| 4 | `RimeWithWeasel/RimeWithWeasel.cpp:1041-1045` | `StartMaintenance()` = `m_session_status_map.clear(); Finalize(); _UpdateUI(0);` |
| 5 | `RimeWithWeasel/RimeWithWeasel.cpp:258-264` | `Finalize()` = `m_disabled = true; rime_api->finalize();` |
| 6 | `librime/src/rime_api_impl.h:58-63` | `RimeFinalize()` → `JoinMaintenanceThread()` → `Service::StopService()` → **`Registry::instance().Clear()`** → `ModuleManager::UnloadModules()` |
| 7 | `librime/src/rime/dict_module.cc:31` | `"userdb"` 组件在此注册 ⇒ Registry 清空即被销毁 |
| 8 | `librime/src/rime/lever/levers_api_impl.h:193-204` | `rime_levers_user_dict_iterator_init` → 构造 `UserDictManager` |
| 9 | `librime/src/rime/lever/user_dict_manager.cc:23-28` | `user_db_component_(UserDb::Require("userdb"))` ⇒ **nullptr** |
| 10 | `librime/src/rime/lever/user_dict_manager.cc:44` | `component->extension()` ⇒ **空指针解引用 = 0xC0000005** |

补充事实（解释「为什么 `GetLevers()` 还非空」）：`ModuleManager::UnloadModules` 只清 `loaded_`，`modules_` 保留 ⇒ `RimeFindModule("levers")` 仍返回非空 ⇒ 拿到的是**已卸载模块的空壳**，levers 本身不是崩溃源，崩溃源是它内部对已销毁 `userdb` 组件的解引用。

## 决策

### 决策 1 — 备份/还原必须发生在「rime 可用且用户库无人占用」的窗口，而不是「rime 已 finalize」的窗口

`BeginMaintenance()` 的语义改为 **Start 之后立刻 End**：

```
BeginMaintenance():
  取 rime 生命周期锁（2 s 超时）
  StartMaintenance()   // = finalize()，释放 leveldb 用户库
  EndMaintenance()     // = m_disabled 为真 ⇒ Initialize() ⇒ 组件重新注册、Registry 恢复
  释放前不返回
```

于是导出/回写发生在 **rime 已恢复**、且**没有 TSF 会话**的时刻：

- levers 拿得到**有效**的 `userdb` 组件（不再 nullptr）；
- 用户库处于「无人占用」的独占态 ⇒ leveldb LOCK 不会与我们自己打开的实例冲突
  （依据：`librime/src/rime/dict/level_db.cc:54-58` 的 `LevelDbWrapper::Open` **没有 read_only 模式**，
  `librime/deps/leveldb/db/db_impl.cc:300` 的 `Recover` 无条件 `LockFile` ⇒ 只要有人占着库，备份必失败）。

与旧口径的关系：`CLAUDE.md` 的「必须 `StartMaintenance`/`EndMaintenance` 包裹 export/import」**字面仍满足**（仍有 Begin/End 包裹），但 rationale 变了——不再依赖「finalize 之后 levers 仍能用」这个**已被证伪**的假设。

### 决策 2 — rime 生命周期操作必须互斥（新增 `include/FluxingRimeLifecycle.h`）

采用 Win32 **命名信号量** `Local\Fluxing.RimeLifecycle`：

- **为什么是命名对象**：由内核按名字唯一化 ⇒ 跨模块（`WeaselServer/Cloud` 与 `WeaselIPCServer`）指向**同一把锁**；C++ `inline static` 会被各模块各持一份 ⇒ 互斥形同虚设。
- **为什么是信号量而不是互斥体**：互斥体只能由**持有它的同一线程**释放；本锁的获取方（云同步后台线程）与释放方（IPC 处理线程）不保证同线程 ⇒ 只能由**任意线程**释放的信号量才适用。
- **不会永久锁死**：进程退出时内核销毁命名对象，占用计数随之复位。
- ⚠️ **配对释放必须走 `ReleaseSemaphoreHandle()`**（先 `ReleaseSemaphore` 再 `CloseHandle`）：信号量的占用计数**不随 `CloseHandle` 复位**，直接 Close 会把锁永久留在「已占用」状态。
- 取锁等待上限 **2 s**；拿不到就**跳过本轮**并给出中文提示（「输入法引擎正忙（可能正在部署或上一轮同步尚未结束），本轮跳过用户词典同步。」）⇒ 绝不阻塞用户输入。

### 决策 3 — 所有 levers 调用一律加 SEH 护栏，访问冲突降级为「本轮失败」

新增 5 个 **POD-only** 护栏：

| 护栏 | 覆盖的调用 | 返回语义 |
|---|---|---|
| `SafeUserDictIteratorInit` | `user_dict_iterator_init` | 1 = 成功 / 0 = 无词典 / -1 = 访问冲突 |
| `SafeNextUserDict` | `next_user_dict` | 1 = 取到 / 0 = 枚举结束 / -1 = 访问冲突 |
| `SafeUserDictIteratorDestroy` | `user_dict_iterator_destroy` | 忽略异常 |
| `SafeBackupUserDict` | `backup_user_dict` | true / false（含访问冲突） |
| `SafeRestoreUserDict` | `restore_user_dict` | true / false（含访问冲突） |

访问冲突 ⇒ **放弃本轮** + 中文错误（「输入法引擎当前不可用（可能正在部署），本轮跳过用户词典同步。」），**进程永不崩**。

护栏函数刻意只收指针/整数、不放任何 C++ 对象：MSVC 规定含 `__try` 的函数不能有需要对象展开的局部变量（C2712）。

### 决策 4 — 崩溃取证必须先修安全网

`WeaselServer/WeaselServer.cpp::WriteMinidumpOnCrash` 改为**两级建目录**（先 `%LOCALAPPDATA%\fluxing`，再 `...\crash`）。

理由：**没有落盘的 dump 等于没有取证**。本轮正是靠这条修复才第一次拿到 15.2 MB minidump，也才确认「退出码 0xC0000005」这一事实。

## 后果

- ✅ 「含云构建 30 s 自退」缺陷消除：从「整进程崩溃」降级为「最坏一轮同步失败 + 中文提示」。
- ✅ 用户词典同步从「可能整进程崩溃」变成「可降级、可观测、可重试」的失败模式。
- ✅ 崩溃取证能力补齐（minidump 现在真的落盘），后续同类缺陷有现成通道。
- ⚠️ **残留缺陷（本 ADR 明确不修）**：**每轮同步仍会 finalize + Initialize rime** ⇒ 当前 TSF 会话被清空（用户进行中的输入可能被中断）。根因是「独占 leveldb 用户库」在 rime 架构下必须先 finalize。彻底解法需改为「复制 userdb 目录后另开 leveldb」或「独立进程执行备份」（改动面更大，需另立 ADR）。
- ⚠️ `WeaselServer/Cloud/CloudMaintenanceHost.h` 里关于维护态语义的注释描述的是「释放 rime 给部署器」的旧语义 ⇒ 已在本批同批更新。
- ⚠️ 部署器维护（`WeaselIPCServer/WeaselServerImpl.cpp::OnStartMaintenance`）**本轮不改**：它与云同步并发时，决策 2 的锁 + 决策 3 的 SEH 护栏已把「崩进程」降级为「本轮失败」；改它会触发 `CLAUDE.md` 的 **IPC 四边同步**约束（协议变更需 server / TSF / deployer 三端 + ADR + 测试），收益与代价不匹配。

## 被否决的替代方案

1. **只在调用点判空 / 只改 `ImportUserDict`**：崩溃发生在 levers **内部**（`UserDb::Require` 返回 nullptr 后被解引用），调用点拿不到任何可判别的信号 ⇒ 否决。
2. **让 `RimeWithWeasel::StartMaintenance` 不 finalize**：那样 leveldb 用户库仍被占用 ⇒ 备份/还原必然 LOCK 失败 ⇒ 这正是当初引入 finalize 的原始理由 ⇒ 否决。
3. **改 `librime` 的 `UserDictManager` 让它对 nullptr 容错**：改的是 submodule（`librime` 本身是脏的 upstream fork），长期维护成本高；且 `nullptr` 只应掩盖「rime 不可用」的真实现象，不如在调用侧显式降级并给出中文原因 ⇒ 否决。
4. **本批同时改 `WeaselIPCServer` 侧接同一把锁**：收益是「部署器与云同步严格串行」，但触发 IPC 四边同步约束，且 SEH 护栏已把最坏情况从「崩进程」压到「本轮失败」⇒ 本批不做（见后果第 4 条）。
5. **把生命周期互斥做成 C++ `inline static` 互斥量**：跨模块各持一份 ⇒ 形同虚设 ⇒ 否决（见决策 2）。

## 验证

- **构建**：`build.bat weasel` ⇒ **0 警告 0 错误**（两次：带 trace 的诊断版、去 trace 的发布版）；`CloudRimeUserDictBackend.cpp` 确认重编、`WeaselServer.exe` 重新链接。
- **发布件**：`output\Win32\WeaselServer.exe` = **2 700 800 B** / 2026-11-12 19:54:28 / sha256 前 16 = `AA932E223440BDD4`；`env.bat` 的 `RELEASE_BUILD=1`。
- **二进制自证**：新中文提示（「输入法引擎正忙…」「输入法引擎当前不可用…」）与 `Local\Fluxing.RimeLifecycle` 均以 **UTF-8 字节**命中 —— ⚠️ `/utf-8` + 窄字符串字面量 ⇒ 产物里是 **UTF-8** 不是 UTF-16，用 UTF-16 搜索会**假红**；临时 trace 标记 `fluxing-cloud-trace.log` 在发布件中 **0 命中**（通道已移除）。
- **修复验证（带 trace 的诊断版，`tmp/AQ-trace-probe.txt`）**：进程**存活 ≥ 60 s 未退出**（修复前 30.6–30.9 s 必崩 `0xC0000005`）；trace 走完整链路：

  ```
  begin_maintenance_enter → begin_maintenance_done ok=1 → export_enter
  → export_levers acquired=1 → export_iter_init_ok → export_dicts n=1
  → export_backup rime_ice ok=1 → export_done exported=1
  → end_maintenance_enter → end_maintenance_done ok=1
  ```

  ⇒ **备份路径首次真实跑通**（导出 1 个词典 `rime_ice`）；crash 目录**无新增 dump**。
- **发布件复跑（去 trace 版，同一探针脚本第二次）**：**存活 ≥ 60 s 未退出**，无 trace 文件（通道已移除），crash 目录**无新增 dump**。
- **单测**：`test\TestFluxingCloud` ⇒ **`PASS=453 FAIL=0`** / `VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0`（与批次 AO / AP 基线逐字一致 ⇒ 无回归）。
- **反向旁证**：探针收尾「恢复装机版常驻实例」拉起的仍是**批次 AP 的 1.3.0（含缺陷）** ⇒ 它在 19:54:02 再次崩溃并落 dump `fluxing-20261112-195402-c0000005.dmp` ⇒ 既印证「崩溃只由含缺陷的二进制产生」，也说明**装机版必须被新包覆盖**（且 PPL ⇒ 须注销 / 重启）。

### 未闭环

- **restore（回写）路径未在真实云数据上端到端验证** —— 需第二台设备产生「云端版本更新」的拉取场景。代码侧已由 `SafeRestoreUserDict` 护栏覆盖，且与已验证的 export 路径共享同一前提（rime 已恢复 + 用户库无人占用 ⇒ leveldb LOCK 可得）。
- **每轮同步仍会 finalize + Initialize rime** ⇒ 当前 TSF 会话被清空（见「后果」）。
