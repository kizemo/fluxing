# 0056. 本地改动钩子：进程内直连（函数指针槽），不新增 IPC 消息

- 状态：已接受 (accepted)
- 日期：2026-11-11
- 关联：ADR 0055（同步引擎调度与收敛）、ADR 0039（云端账号与多端同步总体架构）、spec 100 `tasks.md` 阶段 F（T501 偏差项）与 T503 / T605 部署项
- 代码：`include/FluxingLocalChangeHook.h`（新建）、`RimeWithWeasel/FluxingLocalChangeHook.cpp`（新建）、`WeaselServer/Cloud/CloudSyncNotifier.{h,cpp}`（新建）、`RimeWithWeasel/RimeWithWeasel.cpp`（改）、`WeaselServer/Cloud/CloudAccountDialog.cpp`（改）、`WeaselServer/WeaselServerApp.cpp`（改）
- 测试：`test/TestFluxingCloud/TestFluxingCloud.cpp`（F5 组，+12 条断言）

## Context

ADR 0055 留了一条明确偏差：**本地改动的「即时推送」钩子未接** —— `CloudSyncEngine::NotifyLocalChange()` 无任何调用方。当时写下的判断是：

> 「rime 的提交事件发生在 TSF 客户端进程 ⇒ 接入它需要新增一条 IPC 消息 ⇒ 触发 CLAUDE.md 的『IPC 四边同步』红线（server / TSF / deployer + ADR + 测试）。当前由 60 分钟定时轮兜底传播，代价是本地改动最长延迟一小时。」

本轮开工前逐项核实工程事实，**该前提被推翻**：

| 核实项 | 事实 | 出处 |
|---|---|---|
| 谁链接 librime | `RimeWithWeasel.vcxproj` = StaticLibrary（链接 `rime.lib`）；`WeaselServer.vcxproj` 用 ProjectReference 引用它；**`WeaselTSF.vcxproj` = DynamicLibrary，不链接 librime** | 三个 `.vcxproj` |
| 提交发生在哪 | `RimeWithWeaselHandler::_Respond` 调 `rime_api->get_commit`；handler 是 `WeaselServerApp` 的成员（`m_handler`），**与 `CloudSyncEngine` 同在 `WeaselServer.exe`** | `WeaselServerApp.cpp:33,48` |
| 提交边界唯一性 | 全仓**唯一**一处 `get_commit` 调用（`RimeWithWeasel.cpp:1625`），覆盖自动上屏 / 选词 / 显式 commit 全部路径；`RimeGetCommit` 读后即 `ResetCommitText()` ⇒ 天然一次性事件探针 | `librime/src/rime_api_impl.h:305-321` |
| 依赖方向约束 | `test/TestWeaselIPC` **也链接 `RimeWithWeasel.lib`** ⇒ 若槽的实现放在库里却引用 Cloud 侧符号，该测试工程会 LNK2019 | `test/TestWeaselIPC/TestWeaselIPC.vcxproj` |

结论：**提交事件与同步引擎同进程 ⇒ 不需要 IPC**。原先的偏差记录属于「前提未核实即写进决策」，代价是白扛了 60 分钟的传播延迟。

## Decision

1. **用进程内函数指针槽，不新增 IPC 消息。** 槽声明在 `include/FluxingLocalChangeHook.h`（纯声明，**刻意不含任何标准库 / windows 头** —— `RimeWithWeasel.cpp` 走预编译头，会先注入 `windows.h` 的 min/max 宏），实现在 `RimeWithWeasel/FluxingLocalChangeHook.cpp`（`std::atomic<FluxingLocalChangeHookFn>`，注册 release / 通知 acquire 配对，免锁）。

2. **依赖方向固定为「Cloud → Rime」。** 槽的**实现**必须在 `RimeWithWeasel` 静态库内部；Cloud 侧只注册回调。理由见上表：`TestWeaselIPC` 链接同一个库，反向依赖会 LNK2019。

3. **落点是 `_Respond` 的提交分支。** `get_commit` 成功处即「本地词典可能已改动」的唯一事件源（读后即清 ⇒ 每笔提交恰好触发一次）；**不新增任何 `get_commit` 调用**（避免与 UI 抢提交文本）。该处覆盖 `ProcessKeyEvent` / `AddSession` / `SelectCandidateOnCurrentPage` / `HighlightCandidateOnCurrentPage` / `ChangePage` / `ShiftUp` 等全部提交路径。

4. **回调用 `shared_ptr` 持引擎。** 槽是无捕获函数指针，捕获 `this` 会在控制器析构后留下悬垂指针；`CloudSyncNotifier` 用 `std::shared_ptr<CloudSyncEngine>` 持有引擎 ⇒ 通知在飞行中时引擎对象不析构（无 use-after-free）。`StopEngine()` **先** `SetTarget(nullptr)` 再停引擎（先摘后停 ⇒ 不会再有新通知进来；已在飞行的通知由 shared_ptr 保活）。`SetTarget(nullptr)` 在引擎锁外、且不持任何引擎锁 ⇒ 无锁序反转。

5. **通知只做「置标志 + 唤醒」。** 引擎的 `NotifyLocalChange()` 只在短临界区写 `dirty_since_ms_`（**首次置位，重复通知幂等**）+ `wakeup_` + `cv_.notify_all()`；**网络 I/O 明确在引擎后台线程的锁外** ⇒ 输入响应线程零阻塞（宪法 P2）。

6. **启动期构造云账号控制器。** `WeaselServerApp` 构造期即 `fluxing::cloud::CloudAccountController::Instance()` ⇒ 云同步不再依赖「用户是否打开过账号页」；该构造只读本机凭据、不触网；**未登录时不挂引擎 ⇒ 零出向请求**（R9 / AC-12）。

## Consequences

- ✅ **不触发 IPC 四边同步红线**：协议与 `include/WeaselIPCData.h` 未变，server / TSF / deployer 三端无需同步改动。
- ✅ **本地改动的传播延迟从「最长 60 分钟」降到「防抖 5 分钟 + 推送全局间隔 15 分钟」**。
- ✅ **依赖方向单向**：`RimeWithWeasel` 库不引用 Cloud 符号 ⇒ `TestWeaselIPC` 不受影响。
- ✅ **可回退**：把 `RegisterFluxingLocalChangeHook(nullptr)` 一行去掉即回到「只有定时轮」的旧行为。
- ⚠️ **钩子语义是「可能改动」而非「确已改动」**：任何提交都会置脏（含未入用户词典的提交）⇒ 防抖窗口 + sidecar 差分共同兜底，不会造成写放大（推送集合按差分裁剪，没有本地改动时根本不发出推送请求）。
- ⚠️ **不是「用户词典已落盘」的精确信号**：真正的落盘由 librime 内部完成，本钩子只表示「刚有一笔上屏」。若将来要更精确的判据，需在 `RimeWithWeaselHandler` 之外另找落点。
- ⚠️ **只覆盖服务端进程**：`WeaselTSF` 不链接 librime ⇒ 只有 `WeaselServer.exe` 有提交事件。若将来出现第二个拥有会话的进程，需要在那个进程里再挂一份钩子。

## Verification

- 云单测 `tmp\build-fluxing-cloud-tests.cmd` ⇒ `RESULT: PASS=414 FAIL=0`（基线 402 ⇒ **+12**）。F5 组覆盖：未注册时通知为空操作 / 注册后每次通知回调一次 / 注销后不再回调；宿主无目标时通知为空操作 / 挂载后 `has_target` 为真 / 通知触发一轮同步且 `last_trigger == kLocalChange` / 该轮含推送方向 / 摘除目标后通知不再触发同步。**只 include 生产头、不复制第二份实现**（宪法 R7）。
- `build.bat weasel` ⇒ **0 错误 0 警告**。顺带修掉两处被本轮重编译暴露的**既有**警告：`WeaselServer/UserDictionary.h` 的 `class` / `struct` 不一致（C4099，前置声明改为 `struct UserDictEntry;`）、`WeaselServerApp.cpp` 的 `_snwprintf` 弃用（C4996 ⇒ `_snwprintf_s(..., _TRUNCATE, ...)`）。
- clang-format：12 个生产文件整文件逐行比对 `FORMAT_BAD_FILES=0`；两个既有大文件（`RimeWithWeasel.cpp` / `WeaselServerApp.cpp`）的 **HEAD 版本实测本就不干净** ⇒ 判据降级为「本轮新增行在 clang-format 输出里逐字不变」，`FORMAT_NEWLINE_BAD=0`。
- **未验证**：真机「打一个字 → 防抖窗口后云端可见」的端到端时序（需登录态 + 线上账号 + 用户在场）；`CloudCredentialStore` 是 Windows 用户级全局单例 ⇒ 双机验收需第二台设备或第二个 Windows 账户。
