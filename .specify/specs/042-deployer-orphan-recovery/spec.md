# 042 - Deployer 孤儿任务恢复（v0.18.30+）

> 元 spec 045+ 后续稳定性切片。`WeaselServer` 进程存在一个"永久卡死"缺陷：
> 当 `WeaselDeployer.exe` 被任何方式中途杀死（任务管理器 / 杀毒软件 / 异常崩溃），
> 维护模式的 `EndMaintenance()` 永远不会调用，librime 永远停留在 `finalize()` 后状态。
> 用户表现为"打字没反应 / 候选不弹 / 必须重启电脑才能恢复"。

## 0. 上下文

诊断报告：`C:\Users\Duanyi\Documents\Codex\2026-07-08\new-chat-2\outputs\rime-task-orphan-diagnosis.md`
（21 KB，含 6 条根因 + 6 个修复方案 + 验证步骤）

6 条已确认根因（按严重度）：

| 根因 | 位置 | 严重度 | 本 spec 处理 |
|---|---|---|---|
| R1 ShellExecuteW 无 PID/Job/心跳 | `WeaselServer/WeaselServerApp.h:13-17` | P1 | 推迟到 spec 043+ |
| R2 `DictManagement()` 漏 `join_maintenance_thread()` | `WeaselDeployer/Configurator.cpp:182-187` | P1 | **本 spec Fix 2** |
| R3 `StartMaintenance()` 无 refcount | `RimeWithWeasel/RimeWithWeasel.cpp:495-499` | P2 | 推迟到 spec 043+ |
| R4 Configurator 三段维护区间裸 Start→End | `WeaselDeployer/Configurator.cpp:138/153, 177/193, 217/234` | P1 | **本 spec Fix 1** |
| R5 `m_session_status_map.clear()` 不通知 TSF | `RimeWithWeasel/RimeWithWeasel.cpp:495-499` | P3 | 推迟到 spec 043+ |
| R6 `_IsDeployerRunning()` 只看 mutex | `RimeWithWeasel/RimeWithWeasel.cpp:574-576` | P3 | 推迟到 spec 043+ |

**本 spec 范围（最小可行修复）**：
- Fix 1 — `Configurator.cpp` 三段维护区间用 `MaintenanceGuard` RAII 包裹
- Fix 2 — `DictManagement()` 在 `run_task` 之后显式 `join_maintenance_thread()`

**不在本 spec 范围（明确推迟）**：
- 全部需要修改 WeaselServer 侧的项（R1/R3/R5/R6 + spec 043 范畴）
- Job Object + 进程心跳（spec 043）
- StartMaintenance refcount（spec 043）
- PPL 守护进程的二进制替换（spec 047 范畴）

## 1. 产品视角

### 1.1 目标

- 消除"用户右键托盘 → 重新部署/词典管理/同步 → deployer 中途死亡 → WeaselServer 永久卡死"路径
- 当 deployer 进程在维护区间内被 `taskkill /F` 杀死时，WeaselServer 端的维护模式状态能在下一次输入请求时自动恢复
- 不引入新的常驻进程、不破坏现有部署流程

### 1.2 用户故事

- US042-A [P1]: 用户触发"重新部署" / "词典管理" / "同步"后，WeaselDeployer.exe 任何时刻被 taskkill /F 杀死，WeaselServer 不会被永久卡死
- US042-B [P1]: WeaselDeployer.exe 异常退出（access violation）时，librime 状态自动恢复
- US042-C [P2]: 杀毒软件在 WeaselDeployer.exe 维护期间将其隔离（quarantine），WeaselServer 不会无限期等待
- US042-D [P2]: WeaselServer 端能在 5 秒内检测到 deployer 已死，自动 EndMaintenance + Initialize

### 1.3 验收

- Given `Configurator::DictManagement()` 正在执行 `run_task("installation_update")` 之后
- When 外部进程调用 `taskkill /F /IM WeaselDeployer.exe`
- Then WeaselServer 端 `m_disabled=true` 状态在 5 秒内被自动恢复（`m_disabled=false`）
- And 下一次 `ProcessKeyEvent` 调用能成功转发到 librime（不卡死）
- And 不需要用户重启电脑 / 重启 WeaselServer

- Given `Configurator::UpdateWorkspace()` 处于维护区间中
- When WeaselDeployer.exe 在 `dlg.DoModal()` 阻塞期间被 taskkill
- Then `~MaintenanceGuard()` 析构函数能正确触发 `client.EndMaintenance()`（即使原代码路径异常退出）
- And EndMaintenance 失败时不抛异常、不崩溃

## 2. 范围

### 2.1 改动文件

- **新建** `WeaselDeployer/MaintenanceGuard.h` - RAII 守卫头文件（~30 行）
- **修改** `WeaselDeployer/Configurator.cpp` - 三段维护区间用 guard 包裹（~20 行变化）
- **追加** `.specify/memory/lessons-learned.md` - L55 章节
- **新建** `test/TestOrphanRecovery/TestOrphanRecovery.{cpp,h,vcxproj,xmake.lua}` - 行为级测试
- **修改** `weasel.sln` - 注册新 test project
- **修改** `.github/workflows/ci.yml` - 把新 test 加入 test job（可选，不阻塞）

### 2.2 不在范围（明确推迟）

- R1/R3/R5/R6 全部（涉及 WeaselServer 侧，需要 spec 043）
- Job Object（spec 043 范畴，需要 CreateProcess 替换 ShellExecuteW）
- 进程心跳 / watchdog（spec 043 范畴，需要在 WeaselServer 端加定时器）
- 用户可见的提示信息（不做 UI 改动，避免引入新的 i18n 工作）

## 3. 风险

| 风险 | 缓解 |
|---|---|
| `MaintenanceGuard` 析构时 `client.EndMaintenance()` 抛出异常 | 析构函数 `noexcept`，内部 try/catch 吞掉 |
| RAII guard 改变了现有维护流程的时序 | 现有功能测试（TestWeaselIPC + AGENTS §2.5 smoke test）必须保持 PASS |
| `join_maintenance_thread()` 在 librime 版本 < 1.13 不存在 | 用 `RIME_API_AVAILABLE(rime, join_maintenance_thread)` 包裹 |
| Guard 对象本身被错误地复制 | 显式 delete 拷贝构造和拷贝赋值（single-owner） |
| 测试不能 spawn 真实 WeaselServer（PPL 进程） | 行为级 mock：抽 `MaintenanceGuard::EndMaintenance` 为可注入的回调 |