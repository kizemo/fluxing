# spec 042 计划 - Deployer 孤儿任务恢复（v0.18.30+）

## 1. 技术上下文

> - **三层进程结构**：
>   - WeaselServer（librime host，PPL 守护，PID 在 session 内稳定）
>   - WeaselDeployer.exe（每次维护时 ShellExecuteW 拉起，临时进程）
>   - 命名管道 `\\.\pipe\<user>\WeaselNamedPipe`（IPC 通道）
> - **维护模式状态机**（在 WeaselServer 端）：
>   - `m_disabled = false`（正常）
>   - `StartMaintenance()` → `m_session_status_map.clear()` + `Finalize()` → `m_disabled = true`
>   - `EndMaintenance()` → `Initialize()` → `m_disabled = false`
> - **触发链**（R4 + R2 触发路径）：
>   - 用户右键托盘 → 菜单 handler → `WeaselServerApp::execute(WeaselDeployer.exe)`
>   - `WeaselDeployer.exe /dict` → `Configurator::DictManagement()` → `client.StartMaintenance()`
>   - `WeaselDeployer.exe` 调 `rime->run_task("installation_update")`（异步）
>   - 假设此时 `taskkill /F /IM WeaselDeployer.exe` → 进程死
>   - `~MaintenanceGuard()` 永不调用 → `client.EndMaintenance()` 永不调用
>   - `m_disabled=true` 永久保留 → 下次 ProcessKeyEvent 走 `if (m_disabled) { return 0; }` 早退

## 2. 技术方案

### 2.1 MaintenanceGuard RAII 包裹（Fix 1）

**新建** `WeaselDeployer/MaintenanceGuard.h`（~30 行）：

```cpp
#pragma once
#include <WeaselIPC.h>
#include <logging.h>

namespace weasel {
namespace deployer {

// RAII guard: ensures client.EndMaintenance() is called on scope exit,
// even if the function returns early (error path) or the process is
// terminated abnormally (taskkill /F).
//
// spec 042: closes R4 (Configurator three maintenance intervals are
// naked Start->End, no try/finally, no RAII). L55.
class MaintenanceGuard {
 public:
  explicit MaintenanceGuard(weasel::Client& client) : client_(client) {
    if (client_.Connect()) {
      LOG(INFO) << "MaintenanceGuard: entering maintenance mode.";
      client_.StartMaintenance();
      entered_ = true;
    }
  }
  ~MaintenanceGuard() noexcept {
    if (!entered_) return;
    try {
      if (client_.Connect()) {
        LOG(INFO) << "MaintenanceGuard: leaving maintenance mode.";
        client_.EndMaintenance();
      } else {
        LOG(WARNING) << "MaintenanceGuard: client.Connect() failed in dtor; "
                     << "WeaselServer may be down, leaving as-is.";
      }
    } catch (...) {
      // never throw from destructor (std::terminate otherwise)
      LOG(ERROR) << "MaintenanceGuard: exception in dtor, swallowing.";
    }
  }

  // Non-copyable, non-movable: single owner per maintenance interval.
  MaintenanceGuard(const MaintenanceGuard&) = delete;
  MaintenanceGuard& operator=(const MaintenanceGuard&) = delete;
  MaintenanceGuard(MaintenanceGuard&&) = delete;
  MaintenanceGuard& operator=(MaintenanceGuard&&) = delete;

  bool entered() const { return entered_; }

 private:
  weasel::Client& client_;
  bool entered_ = false;
};

}  // namespace deployer
}  // namespace weasel
```

### 2.2 Configurator.cpp 三段维护区间改造

**修改** `WeaselDeployer/Configurator.cpp` 三处：

- `UpdateWorkspace()` (~line 130-155) — 把 `client.StartMaintenance()` 替换为 `MaintenanceGuard guard(client);`
- `DictManagement()` (~line 158-196) — 同上
- `SyncUserData()` (~line 198-237) — 同上

每段维护区间的代码从：
```cpp
weasel::Client client;
if (client.Connect()) {
  LOG(INFO) << "Turning WeaselServer into maintenance mode.";
  client.StartMaintenance();
}
{
  // ... maintenance work ...
}
CloseHandle(hMutex);

if (client.Connect()) {
  LOG(INFO) << "Resuming service.";
  client.EndMaintenance();
}
```

改为：
```cpp
weasel::Client client;
MaintenanceGuard guard(client);  // enters on construction
{
  // ... maintenance work ...
}
CloseHandle(hMutex);
// guard destructor: EndMaintenance() called here
```

注意：`CloseHandle(hMutex)` 必须在 guard 析构**之前**调用（保留原顺序，因为原代码注释 "should be closed before resuming service"）。

### 2.3 DictManagement 异步任务 join（Fix 2）

**修改** `WeaselDeployer/Configurator.cpp:182-187`：

```cpp
RimeApi* rime = rime_get_api();
if (RIME_API_AVAILABLE(rime, run_task)) {
  rime->run_task("installation_update");  // async
}
// spec 042 Fix 2 (L55 R2): run_task is async; before opening the
// modal dialog, wait for the maintenance thread to finish so
// user dict operations happen on a quiesced state.
if (RIME_API_AVAILABLE(rime, join_maintenance_thread)) {
  rime->join_maintenance_thread();
}
DictManagementDialog dlg;
dlg.DoModal();
```

### 2.4 行为级测试（TestOrphanRecovery）

**新建** `test/TestOrphanRecovery/TestOrphanRecovery.cpp`：

测试策略：**不 spawn 真实 WeaselServer**（PPL 进程 + 需要 IPC 命名管道）。改为：
- 用 `MockClient` 替换 `weasel::Client` 接口
- 验证 `MaintenanceGuard` 在以下 4 种异常路径下都调用了 `MockClient::EndMaintenance()`：
  1. 正常析构（happy path）
  2. throw 异常后析构（dtor 必须 noexcept）
  3. `Connect()` 失败后析构（不调用 EndMaintenance 因为 entered_=false）
  4. EndMaintenance 本身抛异常（dtor 必须吞掉）

```cpp
struct MockClient {
  int start_count = 0;
  int end_count = 0;
  bool connect_returns = true;
  bool end_throws = false;
  bool Connect() { return connect_returns; }
  void StartMaintenance() { ++start_count; }
  void EndMaintenance() {
    ++end_count;
    if (end_throws) throw std::runtime_error("mock");
  }
};

void TestHappyPath() {
  MockClient mc;
  { weasel::deployer::MaintenanceGuard g(mc); }
  assert(mc.start_count == 1);
  assert(mc.end_count == 1);
}

void TestExceptionInScope() {
  MockClient mc;
  try {
    weasel::deployer::MaintenanceGuard g(mc);
    throw std::runtime_error("simulated deployer crash");
  } catch (...) {}
  assert(mc.start_count == 1);
  assert(mc.end_count == 1);
}

void TestConnectFailed() {
  MockClient mc; mc.connect_returns = false;
  { weasel::deployer::MaintenanceGuard g(mc); }
  assert(mc.start_count == 0);
  assert(mc.end_count == 0);
}

void TestEndThrows() {
  MockClient mc; mc.end_throws = true;
  // 必须不 std::terminate
  { weasel::deployer::MaintenanceGuard g(mc); }
  assert(mc.start_count == 1);
  assert(mc.end_count == 1);  // throw is caught internally
}
```

**测试集成**：
- 新建 `test/TestOrphanRecovery/{TestOrphanRecovery.cpp, TestOrphanRecovery.vcxproj, xmake.lua, stdafx.h, stdafx.cpp, targetver.h}`
- 仿 `TestDarkModeBridge` 模板（用 `assert()` 不引入 GoogleTest）
- 注册到 `weasel.sln` 和 `scripts/test-infra/run-test-suite.bat`

## 3. 任务粒度（满足 R5: < 4h, 1-3 files）

- T001-T003: spec 三件套（已完成）
- T004: MaintenanceGuard.h 新建（~30 min）
- T005: Configurator.cpp 三段 RAII 包裹（~20 min）
- T006: DictManagement join_maintenance_thread（~5 min）
- T007: L55 lessons-learned 追加（~10 min）
- T008-T012: TestOrphanRecovery 5 个 test case + 模板（~60 min）
- T013: 注册 test 到 sln + run-test-suite.bat（~10 min）
- T014: 本地构建 + 测试（~5 min）
- T015: commit（~5 min）
- **总计**：~2.5 小时

## 4. 依赖

- spec 045 v0.18.29.0 ship（已完成）
- weasel::Client::StartMaintenance / EndMaintenance 已存在（上游版）
- TestDarkModeBridge 项目模板（已存在，参照其 vcxproj）