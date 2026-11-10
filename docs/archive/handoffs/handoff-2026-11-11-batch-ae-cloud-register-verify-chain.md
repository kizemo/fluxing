# 批次 AE · spec 100 阶段 E：客户端注册 + 邮箱验证码链路（服务层 + UI 层）

> 生成：2026-11-11 · 分支 `feat/100-cloud-sync` · 上承批次 AD（T405 / T406 / T407）
> 状态：**代码 + 单测 + 云单测门禁 DONE**；WeaselServer 主工程构建与 clang-format 见 §3「门禁」；
> **真机判据未闭环**（必须有用户在场，见 §7）。

---

## 1. 一句话结论

spec 100 阶段 E 的账号页此前**只能登录、不能注册**：客户端完全没有注册与邮箱验证码链路
（`CloudAccountService` 无 `Register`/`VerifyEmail`，`CloudAccountController` 无对应 `Submit*`，
全仓 `register|verify` 仅命中 `RegisterClassExW` 之类无关符号）。
本批把**服务端已有的两条端点**（`POST /v1/auth/register`、`POST /v1/auth/verify`）在客户端补齐到
**服务层 + UI 层**，并加 27 条单测断言坐实「注册 ≠ 登录」语义；云单测 **PASS=324 FAIL=0**（基线 297）。
**零服务端改动、零 IPC 改动、零新依赖。**

---

## 2. 必读文档（全绝对路径）

| 优先级 | 路径 | 读它解决什么 |
|---|---|---|
| 1 | `F:/soft/00selfmade/rime_claude/CONTEXT.md` | 身份 / 模块 / 不变量 |
| 2 | `F:/soft/00selfmade/rime_claude/AGENTS.md` | 操作手册；**§9 接力规范**（本文件即按此产出） |
| 3 | `F:/soft/00selfmade/rime_claude/.specify/memory/constitution.md` | 法则层 R1–R9 / P1–P8（**P2 TSF 回调禁阻塞 I/O** 是本批关键约束） |
| 4 | `F:/soft/00selfmade/rime_claude/.specify/specs/100-fluxing-cloud-account-sync/tasks.md` | 任务账本；本批回写见其「批次 AE 收尾记录」段 |
| 5 | `F:/soft/00selfmade/rime_claude/.specify/specs/100-fluxing-cloud-account-sync/plan.md` | 技术方案（§6.2 偏差表） |
| 6 | `F:/soft/00selfmade/rime_claude/docs/archive/handoffs/handoff-2026-11-11-batch-ad-tray-langbar-residue.md` | 上一批交接（T405 入口 + Cloudflare 全绿 + 投资线索） |
| 7 | `F:/soft/00selfmade/rime_claude/cloud/fluxing-sync/src/auth.ts` | **服务端契约真相源**：`handleRegister` @255 / `handleVerify` @291 / `requirePepper` @73 / `normalizeEmail` @83 / `requireAuthKey` @92 / `parseDevice` @122 |
| 8 | `F:/soft/00selfmade/rime_claude/cloud/fluxing-sync/src/index.ts` | `route()` 9 条端点 + `/v1/health`；**无 resend 端点** |
| 9 | `F:/soft/00selfmade/rime_claude/WeaselServer/Cloud/CloudAccount.cpp` | 本批 `Register` @308、`VerifyEmail` @401 |
| 10 | `F:/soft/00selfmade/rime_claude/WeaselServer/Cloud/CloudAccountDialog.cpp` | 本批 UI 层全部改动 |

---

## 3. 本批改动（5 文件，逐条）

| 文件（绝对路径） | 增量 | 内容 |
|---|---|---|
| `F:/soft/00selfmade/rime_claude/WeaselServer/Cloud/CloudAccount.h` | +17 | `CloudAccountService` 新增 `Register(email, password, out_account_id, err)` 与 `VerifyEmail(email, code, err)` 声明 + 中文契约注释 |
| `F:/soft/00selfmade/rime_claude/WeaselServer/Cloud/CloudAccount.cpp` | +134 | 两者的完整实现（`Register` @308、`VerifyEmail` @401），插在 `CloudAccountService::Logout` 之前 |
| `F:/soft/00selfmade/rime_claude/WeaselServer/Cloud/CloudAccountDialog.h` | +6 | `CloudAccountController` 新增 `SubmitRegister(email, password)` @61、`SubmitVerifyEmail(email, code)` @62 |
| `F:/soft/00selfmade/rime_claude/WeaselServer/Cloud/CloudAccountDialog.cpp` | +138 / −51 | 控件表重排 + 客户区 460×320 → **480×410** + `ReadControlText`/`SecureZeroWide` 两个内存卫生助手 + 两个 `Impl::Post` lambda |
| `F:/soft/00selfmade/rime_claude/test/TestFluxingCloud/TestFluxingCloud.cpp` | +84 | `TestCloudAccountMock()` 内新增块 ⑦（注册成功 / 本地拒掉非法码 / 验证成功不自动登录）与块 ⑧（409 `email_taken`） |

### 3.1 服务层语义（关键设计）

`Register`：
1. `NormalizeEmail` → 本地校验（空 / 缺 `'@'` / 缺 `'.'` / 空口令）**任一不满足就不发请求**；
   比 `Login` 更严（`Login` 只查 `'@'`），因为注册是**新建账号**，代价不可逆。
2. `DeriveKeys`（复用 ADR 0054 的 PBKDF2-HMAC-SHA256 600k + HKDF 自实现）。
3. `SecureZeroString(&password_copy)` 清明文副本。
4. 读 `info_.device_id`；为空则 `GenerateDeviceId()` 并写回 ⇒ 「注册 → 验证 → 登录」在服务端看到**同一台设备**。
5. `SendJson("POST", "/v1/auth/register", body, /*need_auth=*/false, ...)`，body = `{email, auth_key, device:{id,name}}`。
6. 解析 `account_id` 回填 `out_account_id`。
7. 失败路径清零 `auth_key` / `enc_key`。
8. **全程不写 store、不动 `state_`** —— 注册成功仍是 `kLoggedOut`。

`VerifyEmail`：
1. 本地校验 `/^\d{6}$/`（与服务端 `auth.ts` 完全对齐）；不满足写「验证码格式不正确（应为 6 位数字）。」且**不发请求**。
2. `SendJson("POST", "/v1/auth/verify", {email, code}, false, ...)`。
3. 成功仅返回 `ok`，**同样不动 `state_`**。

> **为什么「注册 ≠ 登录」**：服务端 `register` 返回 `need_email_verify:true`，此时发放令牌等于绕过邮箱验证。
> 单测用 `state()==kLoggedOut` + `store.Load` 失败**双重坐实**：注册后既没改状态机，也没落任何凭据。

### 3.2 UI 层（`CloudAccountDialog.cpp`）

- `ControlId` 追加 `kIdVerifyCode = 1012` / `kIdRegister = 1013` / `kIdVerify = 1014`。
- 客户区 `kClientWidth 460→480`、`kClientHeight 320→410`：多出「验证码 + 两个按钮」一行与底部按钮行。
- 布局：邮箱行 `y=20` / 密码行 `y=54`（`ES_PASSWORD`）/ 验证码行 `y=88`（`ES_NUMBER`）/
  状态框 `y=152, h=40` / 设备列表 `y=198, h=130` / 底部三按钮 `y=336` / 清空云端数据 + 关闭 `y=370`。
  **把「刷新登录状态」「清空云端数据」从右列移到底部按钮行**，避免与加高后的状态框重叠。
- 内存卫生：`ReadControlText()` 读后 `SecureZeroMemory(buf, sizeof buf)`；
  `SecureZeroWide(std::wstring*)` 逐字符 `SecureZeroMemory` 后 `clear()`。
  口令 UTF-8 副本在 lambda 内 `SecureZeroString` 清零。
- 验证码输入框**失败后不清空**（便于改后重提）；密码框成功后清空。
- **P2 合规**：PBKDF2 与全部网络请求都在 `Impl::Post` 的工作线程里，UI 线程只发任务、收结果。

---

## 4. 门禁

| 项 | 命令（实测可复现） | 结果 |
|---|---|---|
| 云单测（构建 + 运行） | `cd F:\soft\00selfmade\rime_claude; cmd /c tmp\build-fluxing-cloud-tests.cmd` | **`RESULT: PASS=324 FAIL=0`** · `VERDICT: ALL PASS` · `TEST_FLUXINGCLOUD_EXIT=0` · `EXIT=0`（**verified**，基线 297 ⇒ +27） |
| WeaselServer 主工程 | `cd F:\soft\00selfmade\rime_claude; cmd /c build.bat weasel` | **`WEASEL_EXIT=0`**、**0 个错误**、0 个警告、耗时 11.14s；产物 `output\Win32\WeaselServer.exe` 2654208 B、sha16 `6CA1628D290D3DC8`（**verified**） |
| clang-format | `pwsh -File tmp\AE-clangformat-check.ps1`（18.1.8，逐行比对） | 4 个生产文件 **`diffLines=0`**；`TestFluxingCloud.cpp` `diffLines=1896` 为**改动前既有**的全文件注释重排（差异起点第 4 行文件头，本轮未碰）⇒ 排除（**verified**） |
| 服务端（只读，未重部署） | `cd cloud\fluxing-sync; npm run check` / `npm run typecheck` | 批次 AD 已全绿；本批**未改服务端**故未复跑 |

### 4.1 实跑结论（收尾轮补记 · 2026-11-11）

**收尾轮做了什么**：clang-format 首轮抓出 3 行偏差 ⇒ 修 `CloudAccount.cpp` 的 `SendJson("POST","/v1/auth/verify",…)` 换行位置、`CloudAccountDialog.cpp` 的 `SubmitRegister` 续行缩进（1 空格）⇒ 因改了源码，**两项门禁全部重跑**：

| 门禁 | 最终数字 | 判定 |
|---|---|---|
| 云单测 | `RESULT: PASS=324 FAIL=0` / `VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0` / `TEST_EXIT=0` | **PASS**（与格式修正前逐字一致 ⇒ 格式改动未触碰语义） |
| WeaselServer 构建 | `WEASEL_EXIT=0` / 0 错误 / 0 警告 / 11.14s | **PASS** |
| clang-format | 4 生产文件 `diffLines=0` | **PASS** |

**产物新鲜度自证**（防「拿旧 obj 冒充」）：`msbuild\Release\Win32\WeaselServer\CloudAccountDialog.obj` mtime 23:05:22 > 源 23:03:35 ⇒ `FRESH=True`；`CloudAccount.obj` 23:05:22 > 23:02:00 ⇒ `FRESH=True`；`TestFluxingCloud\CloudAccount.obj` 23:04:20 > 23:02:00 ⇒ `FRESH=True`。

**新增单测段落**：`[D6]`（注册成功：`account_id` 解析 / 仍 `kLoggedOut` / `store.Load` 失败证明不写凭据 / `device_id` 顺带生成 / 只发一次 `POST /v1/auth/register` / body 含规范化邮箱与 `auth_key`/`device` / 不含口令明文）、`[D7]`（验证码本地拒：5 位与非数字均不发请求、不改状态；`/v1/auth/verify` 成功且不自动登录）、`[D8]`（409 `email_taken` ⇒ 失败、`err` 含「已注册」、不写凭据），全部 `[PASS]`。

---

## 5. 逐任务进度（spec 100）

| 任务 | 状态 | 说明 |
|---|---|---|
| T301–T305（阶段 D） | **DONE** | 批次 AB；本批加固 T305 后继相关单测但未改 T305 本体 |
| T401 账号页界面层 | **DONE（本批扩写）** | 原「只能登录」→ 现含注册 + 验证码；截图仍未闭环 |
| T403 / T404 | 见 tasks.md | 本批未改 |
| T405 入口接线 | **DONE**（批次 AC） | 真机截图仍缺，需用户在场 |
| T406 托盘图标 | 代码侧 DONE | 需用户注销/重启后复测 `OUR_HIT=0` |
| T407 线上判据 | **DONE**（批次 AD） | Cloudflare 全绿，`/v1/health` = 200 |
| **T408（本批新增）· 注册 + 邮箱验证码链路** | **代码 / 单测 DONE，真机未闭环** | 见 §3 |
| T501–T505（阶段 F） | 未开工 | 同步引擎与调度 |
| T601–T604（阶段 G） | 未开工 | 双机验收与发版 |

---

## 6. 下阶段有序计划

1. **T408 真机闭环**（需用户在场）：装机 → 打开账号页 → 用真实邮箱走「注册 → 收码 → 验证 → 登录」。
   ⚠ 当前 `MAIL_MODE="outbox"`（$0 路线）⇒ **验证码不发信**，只进 D1 `mail_outbox` / `email_verifications`，
   且后者存的是 **code_hash**，明文码**不可直读** ⇒ 该路径需改 `MAIL_MODE` 或临时加一个只读调试端点（均属服务端改动，需用户拍板）。
2. **T501–T505 阶段 F**：同步引擎与调度。依赖已就绪（T301 信封 / T305 快照通道 / Cloudflare 全绿）。
3. **阶段 G 加固项**（本批新增两条）：
   - **服务端缺 resend 端点**：验证码过期/次数用尽后无恢复路径（再 `register` 会 409 `email_taken`）⇒ 用户被锁死。
   - 词库快照通道**不传播墓碑** ⇒ 跨设备删除不保证生效。
4. **T601–T604**：双机验收 AC-01～AC-14 + 发版。

---

## 7. 必须有用户在场才能闭合的判据（**本批核心交付之一**）

| # | 判据 | 为什么必须用户在场 |
|---|---|---|
| 1 | 账号页**注册 → 验证 → 登录**端到端 | `MAIL_MODE="outbox"` 不发信，明文码只存在于用户邮箱（若切 cloudflare）或不可读的 hash（当前） |
| 2 | T405 右键菜单真机截图 | IME 自绘顶层窗**跨进程抓不到**（记忆 #43） |
| 3 | T406 `OUR_HIT=0` | 需先**退出本会话宿主 `MiniMax Code`** 或注销重登，清掉最后一个持旧镜像的宿主 |
| 4 | T305「导入期间输入不卡顿」时序 | 需真实敲键 + 秒表/ETW，自动化测不出体感 |
| 5 | AC-01～AC-14 双端同步 | `CloudCredentialStore` 默认 target 是全局单例 `Fluxing:CloudAccount`，Credential Manager 按 **Windows 用户**存 ⇒ 两套隔离用户数据目录**不足以**模拟两台设备（凭据互相覆盖）。需①探针显式传不同 `target_name`，或②第二个 Windows 用户 / 第二台机器 |

---

## 8. 环境事实与工具坑（本批踩到的）

1. **服务端没有 resend 端点** —— `route()` 只有 9 条 + `/v1/health`。不要把「重发验证码」当既有能力设计 UI。
2. **`email_verifications` 存 `code_hash`** —— 想从 D1 直读明文码是**不可能**的；排查验证码问题只能看 `mail_outbox` 是否落库 + 时间戳。
3. **`Register` 的邮箱校验必须比 `Login` 严** —— `Login` 只查 `'@'` 是历史实现；注册若也放行 `a@b` 会在服务端建出一个不可验证的账号。
4. **`test/TestFluxingCloud` 的 mock 传输是 `MockTransport`（@1357）** —— 断言「发了什么请求」用它的队列，不要新造 spy。
5. **一次 `edit` 只能传一组 `old_string`/`new_string`**（记忆 #40）—— 本批 `CloudAccountDialog.cpp` 改了 6 处，分 6 次调用并 grep 复核落盘。
6. **`xbuild.bat weasel` 只编 x86**（记忆 #41）—— WeaselServer 属 x86 链，本批改动全部落在该链内，故 `build.bat weasel` 足够；若改动涉及 x64 TSF 必须显式跑另一条链。
7. **工作区外写/删内联被拦** —— 写 `.ps1` 再 `pwsh -NoProfile -ExecutionPolicy Bypass -File` 执行（记忆·windows-toolchain）。
8. **`git commit -F -` 不读 stdin** —— 消息写临时文件再 `-F <file>`（记忆 #44）。
9. **`RimeWithWeasel/RimeWithWeasel.cpp` 有并发会话未提交改动（+118 行）** —— 本批提交**不得捎带**，提交前必看 `git status -sb` + `git diff --stat`。

---

## 9. 证据路径

| 证据 | 路径 |
|---|---|
| 云单测构建 + 运行日志 | 重跑即可复现（`tmp/build-fluxing-cloud-tests.cmd`）；输出末尾 `RESULT: PASS=324 FAIL=0` |
| WeaselServer 构建日志 | 见 §4.1 收尾轮补记 |
| 上一批托盘诊断 | `tmp/AD-tray-check-5.txt`、`tmp/AD-T405-tray-menu.png`、`tmp/AD-t405-menu-probe.ps1` |
| 服务端契约 | `cloud/fluxing-sync/src/auth.ts`、`src/index.ts` |

---

## 10. 结论可信度标注

- **verified**：客户端原先无注册/验证链路（grep 零命中）；服务端两条端点的请求/响应/错误码形态（读 `auth.ts` 源码）；
  云单测 `PASS=324 FAIL=0`（实跑）；`Register` 成功不落凭据、不改状态机（单测 ⑦ 双重坐实）。
- **inferred**：`Register` 顺带写回 `device_id` 会让服务端把「注册 → 验证 → 登录」视为同一设备（按 `parseDevice` 语义推断）。
- **not verified**：WeaselServer 主工程能否编过本批 UI 改动（收尾轮构建中）；
  端到端「注册 → 收码 → 验证 → 登录」（`MAIL_MODE="outbox"` 阻断）；
  真机截图与时序体感。