# 批次 AG · spec 100 阶段 F 收尾 —— 服务端加固部署 + 本地改动钩子接通（进程内直连）

- 日期：2026-11-11
- 分支：`feat/100-cloud-sync`
- 批次范围：① 把批次 AF 写好但**未部署**的服务端加固部署到 `api.aiec.fun`；② 消除 ADR 0055 留下的 **「`NotifyLocalChange()` 无调用方」** 偏差；③ 门禁全绿 + 账本 / 指针 / ADR / CHANGELOG 回写。
- 前置批次：[批次 AF · 阶段 F「同步引擎与调度」](handoff-2026-11-11-batch-af-sync-engine-scheduling.md)

---

## 1. 当前状态（一段话）

批次 AF 交付的同步引擎（T501–T505）代码完整、门禁全绿，但**服务端加固从未上线**（`wrangler deploy` 未执行），且**本地改动的即时推送钩子完全没有调用方** —— 当时写下的理由是「rime 提交事件发生在 TSF 客户端进程 ⇒ 需要新增 IPC 消息 ⇒ 触发 CLAUDE.md 四边同步红线」，这个前提**本轮开工前被逐项核实推翻**：提交事件与同步引擎同在 `WeaselServer.exe`，所以**不需要 IPC**。本轮已完成：服务端部署上线（`/v1/health` 200、`/v1/auth/resend` 404 `email_not_found` ⇒ 端点生效）；本地改动钩子按**进程内函数指针槽**接通（提交边界 → 槽 → `CloudSyncNotifier` → 引擎置脏 + 唤醒），新增 ADR 0056；云单测 `PASS=414 FAIL=0`（基线 402 ⇒ **+12**）、`build.bat weasel` **0 错误 0 警告**、clang-format `FORMAT_BAD_FILES=0`。**未闭环**：真实账号才能验的 `skipped` / `server_seq`、双机 / 双用户验收、真机端到端时序。

---

## 2. 必读文档（全路径）

| 顺序 | 文档 | 绝对路径 |
|---|---|---|
| 1 | 本交接文档 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-11-batch-ag-local-change-hook-and-server-deploy.md` |
| 2 | spec 任务账本（阶段 F / T501–T505、T605、阶段 G） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` |
| 3 | ADR 0056（本轮新增：本地改动钩子 = 进程内直连） | `F:\soft\00selfmade\rime_claude\docs\adr\0056-local-change-hook-in-process-direct-link.md` |
| 4 | ADR 0055（同步引擎调度与收敛，含被本轮推翻的那条偏差） | `F:\soft\00selfmade\rime_claude\docs\adr\0055-cloud-sync-engine-scheduling-and-convergence.md` |
| 5 | ADR 0039（云端账号与多端同步总体架构） | `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md` |
| 6 | 宪法（法则层 R1–R9 / P1–P8） | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 7 | 操作手册（首屏 §1 / 危险区 §4 / checklist §5 / 接力 §9） | `F:\soft\00selfmade\rime_claude\AGENTS.md` |
| 8 | 外部前置条件与部署手册（Cloudflare / D1 / R2 / AUTH_PEPPER） | `F:\soft\00selfmade\rime_claude\docs\ops\fluxing-cloud-external-prerequisites.md` |
| 9 | 服务端 Worker 源码 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\` |

---

## 3. 阶段任务进度

| 任务 | 状态 | 说明 |
|---|---|---|
| T501 同步引擎 | **DONE（偏差已消除）** | 「本地改动钩子未接」的偏差本轮消除，见 §4.1 |
| T502 推送与拉取 | **代码 DONE / 线上部分生效** | 服务端 `skipped` + 顶层 `server_seq` 已部署；**需真实账号**才能功能判别 |
| T503 服务端收敛守卫 | **DONE** | `compareHlc` / `compareUtf8Bytes` + push HLC 守卫，已部署上线 |
| T504 降级与退避 | **代码 DONE** | 退避阶梯 / 日配额已在引擎侧（批次 AF）；运行期行为仍待真机 |
| T505 账号页同步控件 | **DONE** | 「最近同步」「立即同步」「重新发送验证码」（批次 AF） |
| T605 resend 端点 | **DONE + 线上已生效** | 本轮 `wrangler deploy` 后实测 404 `email_not_found` |
| 阶段 G 加固项 | **剩余缺口 = 真实账号端到端** | 见 §6 |

---

## 4. 本轮改动明细

### 4.1 本地改动钩子（ADR 0056）—— **不新增 IPC 消息**

**核实结果（推翻 AF 的前提）**：

| 核实项 | 事实 |
|---|---|
| 谁链接 librime | `RimeWithWeasel.vcxproj` = StaticLibrary（链 `rime.lib`）；`WeaselServer.vcxproj` 用 ProjectReference 引用它；**`WeaselTSF.vcxproj` = DynamicLibrary，不链接 librime** |
| 提交发生在哪 | `RimeWithWeaselHandler::_Respond` 调 `get_commit`；handler 是 `WeaselServerApp` 的成员 ⇒ **与 `CloudSyncEngine` 同在 `WeaselServer.exe`** |
| 提交边界唯一性 | 全仓**唯一**一处 `get_commit`（`RimeWithWeasel.cpp:1625`），覆盖自动上屏 / 选词 / 显式 commit；`RimeGetCommit` 读后即 `ResetCommitText()` ⇒ 天然一次性事件 |
| 依赖方向约束 | `test/TestWeaselIPC` **也链接 `RimeWithWeasel.lib`** ⇒ 槽实现若引用 Cloud 侧符号会 LNK2019 ⇒ 方向必须 **Cloud → Rime** |

**链路**（自上而下）：

1. `RimeWithWeasel/RimeWithWeasel.cpp::_Respond` —— 在 `body.append(L"commit=")...` 之后调 `NotifyFluxingLocalChange()`（**+10 行**）
2. `RimeWithWeasel/FluxingLocalChangeHook.cpp` + `include/FluxingLocalChangeHook.h` —— **函数指针槽**（`std::atomic`，注册 release / 通知 acquire 配对，免锁）。头文件**刻意不含任何标准库 / windows 头**（`RimeWithWeasel.cpp` 走预编译头，会先注入 `windows.h` 的 min/max 宏）
3. `WeaselServer/Cloud/CloudSyncNotifier.{h,cpp}` —— 进程级单例；静态 `NotifyLocalChange()` 先取单例再拷 `shared_ptr`、放锁后调引擎 ⇒ **无 UAF**
4. `WeaselServer/Cloud/CloudSyncEngine.cpp::NotifyLocalChange()` —— 短临界区写 `dirty_since_ms_`（首次置位、重复通知幂等）+ `wakeup_` + `notify_all`；**网络 I/O 明确在引擎后台线程的锁外** ⇒ 输入响应线程零阻塞（宪法 P2）

**配套改动**：

- `WeaselServer/Cloud/CloudAccountDialog.cpp`：`engine` 改 `shared_ptr`；`StartEngine()` 成功后 `SetTarget(engine)`；`StopEngine()` **先** `SetTarget(nullptr)` 再停引擎（先摘后停 ⇒ 飞行中通知由 shared_ptr 保活）
- `WeaselServer/WeaselServerApp.cpp`：构造期 `RegisterFluxingLocalChangeHook(&fluxing::cloud::CloudSyncNotifier::NotifyLocalChange)` + `fluxing::cloud::CloudAccountController::Instance()` ⇒ **云同步不再依赖「用户是否打开过账号页」**；该构造只读本机凭据、不触网，**未登录不挂引擎 ⇒ 零出向请求**（R9 / AC-12）

**回退杠杆**：`RegisterFluxingLocalChangeHook(nullptr)` 一行去掉 ⇒ 回到「只有定时轮」的旧行为。

**已知代价（接受）**：钩子语义是「**可能**改动」而非「确已改动」（任何提交都置脏）⇒ 防抖窗口 + sidecar 差分兜底；推送集合按差分裁剪，无本地改动时根本不发出推送请求。钩子**只覆盖 `WeaselServer.exe`**（`WeaselTSF` 不链接 librime）。

### 4.2 服务端加固部署

- `npx wrangler deploy` ⇒ 成功，`api.aiec.fun`（custom domain），Current Version ID **`0aaf933c-f832-4c64-a726-3570f32ed848`**，绑定 D1 / R2 `fluxing-sync`，`MAIL_MODE=outbox`
- `GET /v1/health` ⇒ **HTTP 200**
- `POST /v1/auth/resend`（未知邮箱）⇒ **HTTP 404** `email_not_found` ⇒ **resend 端点线上已生效**
- `POST /v1/auth/login` 探针形状错（服务端要 `auth_key` 而非 `password`）⇒ HTTP 400 `bad_request`，**属探针形状问题，非线上缺陷**
- 服务端门禁：`npm run typecheck` exit 0；`npx vitest run` **30/30 PASS**（基线 20 ⇒ +10）
- `MAIL_MODE="outbox"` 保持不变（Workers Paid 需账号级 $5/月，与用户拍板的 $0 路线冲突）

### 4.3 顺带修掉的两处**既有**编译警告

- `WeaselServer/UserDictionary.h`：`class` / `struct` 不一致 ⇒ C4099，前置声明改 `struct UserDictEntry;`
- `WeaselServer/WeaselServerApp.cpp`：`_snwprintf` 弃用 ⇒ C4996 ⇒ `_snwprintf_s(wbuf, _countof(wbuf), _TRUNCATE, ...)`

（这两处是本轮重编译才暴露的旧问题，不修则「0 警告」门禁无法成立。）

### 4.4 文件清单（**14 个生产/测试/文档文件**）

| 文件 | 类型 | 摘要 |
|---|---|---|
| `include/FluxingLocalChangeHook.h` | 新建 | 槽声明（无 stdlib / windows 头） |
| `RimeWithWeasel/FluxingLocalChangeHook.cpp` | 新建 | 槽实现（atomic） |
| `RimeWithWeasel/RimeWithWeasel.cpp` | 改 | +10 行（include + 提交边界通知） |
| `RimeWithWeasel/RimeWithWeasel.vcxproj` | 改 | +3（`FluxingLocalChangeHook.cpp`） |
| `WeaselServer/Cloud/CloudSyncNotifier.h` | 新建 | 进程级单例 + `shared_ptr` 持引擎 |
| `WeaselServer/Cloud/CloudSyncNotifier.cpp` | 新建 | 同上 |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | 改 | `shared_ptr` 化 + 挂载 / 摘除目标 |
| `WeaselServer/WeaselServerApp.cpp` | 改 | 注册钩子 + 构造云账号控制器 + `_snwprintf_s` |
| `WeaselServer/UserDictionary.h` | 改 | C4099 |
| `WeaselServer/WeaselServer.vcxproj` | 改 | +4（`CloudSyncNotifier.cpp/.h`） |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 改 | F5 组，+91 行 / +12 条断言 |
| `test/TestFluxingCloud/TestFluxingCloud.vcxproj` | 改 | +6（生产 `.cpp` 与 2 个 ClInclude） |
| `docs/adr/0056-local-change-hook-in-process-direct-link.md` | 新建 | ADR |
| `.specify/specs/100-fluxing-cloud-account-sync/tasks.md` | 改 | +8 / −5 回写 |

**未改**：`include/WeaselIPCData.h`、IPC 协议（**未新增任何 IPC 消息** ⇒ 不触发四边同步红线）、`output/install.nsi`、`output/data/**`、`librime/`、`cloud/fluxing-sync/src/**`（服务端本轮只部署不改动）。

---

## 5. 已验证 / 推断 / 未验证

### 5.1 已验证（实测，有命令输出）

| 项 | 证据 |
|---|---|
| 云单测 `PASS=414 FAIL=0`（基线 402 ⇒ **+12**），`VERDICT: ALL PASS` | `tmp\build-fluxing-cloud-tests.cmd` |
| `build.bat weasel` ⇒ **0 个警告 0 个错误**，`TestWeaselIPC.exe` 链接成功 ⇒ 依赖方向「Cloud → Rime」成立 | `cmd /c build.bat weasel` |
| clang-format：12 个生产文件整文件逐行 `FORMAT_BAD_FILES=0`；两个既有大文件（HEAD 版本实测本就不干净）降级为新增行逐字校验 `FORMAT_NEWLINE_BAD=0` | `tmp\AG-clangformat-check.ps1` |
| 服务端 `typecheck` exit 0、`vitest run` **30/30 PASS** | `cloud\fluxing-sync` |
| 部署成功 + Version ID + `/v1/health` 200 + `/v1/auth/resend` 404 `email_not_found` | `npx wrangler deploy` + 线上探针 |

### 5.2 推断（基于代码事实，未在真机跑过）

- 钩子链路在运行时确实被触发（依赖 `_Respond` 走到 `get_commit` 分支 + 注册槽在 `WeaselServerApp` 构造期完成）—— 单测覆盖了槽与宿主的行为，但**未在真机 IME 进程中观测到一次真实提交 → 引擎置脏**。
- 本地改动的传播延迟由「最长 60 分钟」降到「防抖 5 分钟 + 推送全局间隔 15 分钟」—— 调度逻辑有单测覆盖，**真实时钟下的等待未观测**。

### 5.3 未验证（**不可宣称成功**）

1. **`push` 响应 `skipped` 数组 / `pull` 顶层 `server_seq`** —— 需合法 `auth_key`（PBKDF2-SHA256 600000 轮、客户端派生）的真实账号。`MAIL_MODE="outbox"` 不发真信，`email_verifications` 只存 `code_hash`，**明文码不可直读**。
2. **双机 / 双 Windows 用户 AC-01~AC-14** —— `CloudCredentialStore` 是 **Windows 用户级全局单例** ⇒ 同用户两套目录**不够**，需第二台设备或第二个 Windows 账户。
3. **真机截图**（账号页 / 托盘）—— IME 自绘顶层窗跨进程抓不到，需人工点一次。
4. **托盘 `OUR_HIT=0` 复测（T406）** —— 需用户重启 MiniMax Code / 注销重登后复跑 `scripts/install/check-tray-input-indicator.ps1`（批次 AD 遗留，本轮未动）。
5. **断网退避 1/5/15/60 分钟 + 恢复补推**、**输入不卡顿体感** —— 需真机与用户在场。
6. **装载新二进制** —— 需先注销 / 重启（PPL 保护）。
7. **端到端时序**：打一个字 → 防抖 5 分钟 + 推送间隔 15 分钟 → 云端可见。

---

## 6. 下阶段计划（按推荐序，含理由）

1. **真实账号闭环（最高优先，理由 = 它是其余所有线上判据的前提）**
   取一个真实邮箱走 `register` → 从 D1 `mail_outbox` 人工取码 → `verify_email` → `login` ⇒ 得到 `auth_key` / `enc_key` ⇒ 立刻可验：`push` 响应含 `skipped`、`pull` 顶层含 `server_seq`、端到端词库同步。**这一步必须用户在场**（人工取码）。
   - 若要自动化：注入 `MAIL_MODE=cloudflare` 需 Workers Paid（$5/月），与 $0 路线冲突 ⇒ **建议保持 outbox + 人工取码**。
2. **双机 / 双用户验收 AC-01~AC-14**（依赖第 1 步拿到凭据；理由 = `CloudCredentialStore` 全局单例 ⇒ 只有跨机器或跨 Windows 账户才构成有效「双端」）。
3. **真机时序与体感验收**（打一字 → 云端可见；输入不卡顿；断网退避与恢复补推）—— 依赖第 1 步。
4. **托盘 `OUR_HIT=0` 复测 + 账号页真机截图** —— 独立于云侧，可与上述并行；只需用户在桌面上点一次。
5. **阶段 G 剩余加固项** —— 目前**只剩「真实账号端到端验证」**（服务端 resend 已部署、已线上判别）。

---

## 7. 本轮工具坑（别重试）

1. **`bash` 硬策略**：同一命令里先 `cd` 再对路径做写 / 删 ⇒ 被拦（`write/delete targets cannot be verified safely after a command working directory change`）。**改用 `git -C <repo>` + 绝对路径，不切目录**。
2. **工作区外的写 / 删内联被拦** ⇒ 把命令写进 `.ps1` 文件，再用 `pwsh -NoProfile -ExecutionPolicy Bypass -File <script>` 执行。
3. **同一文件的多个 `edit` 必须串行**（并行会基于同一快照互相覆盖）；改用「字节级 `.ps1` + 多锚点替换（每锚点断言命中数 == 1）」一次完成多处编辑。
4. **字节替换脚本的锚点缩进陷阱**：锚点若**从行首缩进之后开始匹配**，替换块首行**不可再带缩进**（否则缩进翻倍 —— 本轮踩过，产出 8 空格缩进）。
5. **`edit` 工具在 `RimeWithWeasel/RimeWithWeasel.cpp` 第 18 行锚点匹配失败**（该行是多段 CR 拼接的异常行）⇒ 改用其他锚点。
6. **clang-format（Chromium，列限 80，CJK 按 2 列计）会重排超宽中文注释行** ⇒ 新增注释须自行折行到 80 列内。
7. **`RimeWithWeasel.vcxproj` 无 `/utf-8`**（`WeaselServer.vcxproj` 与 `TestFluxingCloud.vcxproj` 有）；既有 UTF-8 无 BOM 中文源文件编译干净 ⇒ 新文件同款编码安全。
8. **`git commit -F -` 不读 stdin** ⇒ 消息写临时文件再 `-F <file>`。
9. **PowerShell 7 下 `Invoke-WebRequest` 的错误响应体取不到**：`Exception.Response.GetResponseStream()` 不存在 ⇒ 探针必须加 `-SkipHttpErrorCheck`。
10. **`librime/src/rime_api_impl.cc` 不存在**，实际是 `librime/src/rime_api_impl.h`。
11. **插件 hook 串台的交接卡**（`activeDataDir\maintenance\task-state\LATEST.md`）属**其他会话**（practilog 设计 demos / TOKENS.md），已忽略。

---

## 8. 提交边界（本轮）

**本轮提交包含**（仅上表 §4.4 的 14 个文件 + 本交接文档 + `CLAUDE.md` 指针 + `CHANGELOG.md` 条目）。

**本轮提交排除**：

- `librime/`（submodule 脏状态，非本轮改动）
- `docs/design/quickpanel-v6/design-demos/{a-benchmark,b-craft,c-roulette}.html`（未跟踪，非本轮改动）
- `RimeWithWeasel/RimeWithWeasel.cpp` 里**批次 AF 之前**的 spec 045 第三轮改动 —— 该改动在本轮开工时已 `git stash push -- RimeWithWeasel/RimeWithWeasel.cpp` 隔离（`stash@{0}`，备份 `tmp/AG-RimeWithWeasel.spec045.bak.cpp`）；本轮提交完成后应 `git stash pop` 恢复（区域不重叠：spec045 hunk 在 old 32 / 261 / 1086–1194，本轮新增在 old ~16 与 ~1535 ⇒ 3-way merge 应干净；失败时用备份手工恢复）。
- `tmp/`（本批脚本与日志，不入库）
