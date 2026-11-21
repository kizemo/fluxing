# 批次 BL · 同步路径补 401→续期→重试（访问令牌过期不再让同步持续失败）

> 日期：2026-11-22 · 分支 `feat/100-cloud-sync` · 本批性质：**产品修复 + 门禁 + 出包装机 + 装机实证 + 提交**
> 上一批：[BK · 账号页「最近同步」状态行(1017)可见性](handoff-2026-11-21-batch-bk-sync-status-1017-visibility.md)
> 触发：BK §10 遗留清单第 1 项 ——「同步路径的 401→续期→重试缺口（独立缺陷）」。
> **未改 `librime/`、未改 IPC 协议消息类型、未新增依赖、未 bump 版本号**（装机版仍 **1.3.8.0**，判定一律用 sha16 / 字节数，版本号无判别力）。

---

## 1. 当前状态（一段话）

BK 批把账号页「最近同步」状态行(1017)变成真实同步结果后，**如实暴露**出一个此前不可见的独立缺陷：两轮真机同步都报「失败（访问令牌已过期，请用刷新令牌续期。）」——因为 `CloudAccount.cpp` 的 `EnsureFreshToken`（401 后用刷新令牌续期）**只覆盖设备管理三处**（`GET /v1/devices` / `DELETE` 设备 / `DELETE /v1/account/data`），**同步轮次的推 / 拉路径完全没有这层**，`CloudSyncService` 只带一份 `access_token_` 快照，`SendJson` 收到 401 直接失败 ⇒ **访问令牌一过期，自动同步与手动同步全部持续失败**。本批按 **ADR 0070** 修掉它：同步服务新增**可选**的令牌续期回调，401 时续期**一次**并用**新令牌**重发同一请求（最多重试一次，不循环）；协调器透传该回调；账号页在后台线程的 `RunSyncRound` 里注入回调（刷新令牌只在本机，同步客户端不碰凭据）。代码、ADR、云单测（29 条新断言）、L0 门禁、格式定点、主构建、NSIS 打包、**装机实证**全部完成并落盘；**本批唯一未闭环项**是「真机上令牌过期 → 续期 → 同步成功」的端到端观测（需要一个真实过期的访问令牌 + 用户在场），其余遗留仍是 BK 起就挂着的双机 / G-3/G-5 / 待拍板项。

---

## 2. 必读文档（全路径）

| 顺序 | 文档 | 全路径 |
|---|---|---|
| 1 | 本批 handoff（真源） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bl-sync-token-refresh-retry.md` |
| 2 | 上一批 handoff（缺陷现场） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bk-sync-status-1017-visibility.md` |
| 3 | 本批 ADR（决策与后果） | `F:\soft\00selfmade\rime_claude\docs\adr\0070-sync-path-token-refresh-retry.md` |
| 4 | spec（FR-016 等需求） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` |
| 5 | tasks（本批**未**勾选，见 §4） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` |
| 6 | 宪法（不变量 / R1–R9 / P1–P8） | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 7 | 操作手册（危险区 / checklist / §9 收尾规则） | `F:\soft\00selfmade\rime_claude\AGENTS.md` |
| 8 | ADR 索引 | `F:\soft\00selfmade\rime_claude\docs\adr\README.md` |
| 9 | 接力 prompt | `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-22-batch-bl-sync-token-refresh-retry.md` |

---

## 3. 根因（产品缺陷，不是探针判据问题）

1. `WeaselServer/Cloud/CloudAccount.cpp` 的 `EnsureFreshToken`（401 → 用刷新令牌续期 → 重试）**只有三个调用点**：`ListDevices`（`GET /v1/devices`）、`RevokeDevice`（`DELETE /v1/devices/<id>`）、`DeleteCloudData`（`DELETE /v1/account/data`）。**同步轮次的上传 / 下载路径没有**。
2. `WeaselServer/Cloud/CloudSyncService.cpp` 的 `SendJson` 只带一份构造时注入的 `access_token_` 快照，收到 401 直接走通用错误分支返回失败 ⇒ 令牌一过期，同步**每一轮都失败**，且不会自愈。
3. 结果：`CloudAccountDialog.cpp` 的 `RunSyncRound` 把「访问令牌已过期，请用刷新令牌续期。」写进 `sync_status_text` ⇒ 1017 显示失败。**BK 之前不可见**，因为 1017 一直停在静态占位「最近同步：尚未同步。」。

---

## 4. 修法（ADR 0070）与变更清单

**修法三条**：

1. **续期能力以回调注入**（`WeaselServer/Cloud/CloudSyncService.h`）：新增 `using AccessTokenRefresher = std::function<bool(std::string* out_token, std::string* err)>;` + `CloudSyncService::SetAccessTokenRefresher()`。**为什么不是把账号服务塞进同步客户端**：刷新令牌只在 `CloudAccountService` 手里（Credential Manager / DPAPI），同步客户端必须保持**可脱凭据单测**（宪法 R7）。
2. **`SendJson` 改为至多两次尝试**：首发 401 且**已注入回调** ⇒ 调回调取新令牌 ⇒ 用新令牌**重发同一个请求**；续期失败即失败（原因取自回调）；**无回调时 401 行为与修复前完全一致**；重试后仍 401 ⇒ 如实报服务端原因。**只重试一次，不循环、不刷配额。**
3. **协调器透传 + 账号页注入**：`CloudSyncCoordinator::SetAccessTokenRefresher()` 透传给内部创建的 2 个 `CloudSyncService`（推 / 拉各一个）⇒ 推、拉、分页、多批次推送**自动受益**，无需逐处改；`CloudAccountDialog.cpp` 的 `RunSyncRound`（**后台线程**，不阻塞 UI）注入 lambda = `service->Refresh(&refresh_err)` 成功后 `service->GetAccessToken(out_token)`，失败写中文原因（`CloudAccountService::Refresh` 已是 public，**未改 `CloudAccount.h` 的 API 面**）。

| 文件 | 改动 |
|---|---|
| `WeaselServer/Cloud/CloudSyncService.h` | +24 行：`#include <functional>`；`AccessTokenRefresher` 类型；`CloudSyncService::SetAccessTokenRefresher` + 成员 `refresher_`；`CloudSyncCoordinator::SetAccessTokenRefresher` + 成员 `refresher_` |
| `WeaselServer/Cloud/CloudSyncService.cpp` | `SendJson` 包成 `for (int attempt = 0; attempt < 2; ++attempt)`；`SyncOnce` 内 2 处 `service.SetAccessTokenRefresher(refresher_);`（推 / 拉） |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | +16 行：`RunSyncRound` 内 `coordinator.SetAccessTokenRefresher(lambda)` |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | +230 行：`BlAuthHeaderOf()` / `MakeBlItem()` / `struct BlGateBackend`（导出**可解析**的最小快照文本）/ `TestCloudSync401RefreshRetry()` 六组用例 |
| `docs/adr/0070-sync-path-token-refresh-retry.md` | 新建（ADR：背景事实 1–5 / 决策 1–6 / 后果 / BL 验证表） |
| `docs/adr/README.md` | +1 行索引（0070） |
| `CHANGELOG.md` | `## [1.3.8]` 顶部插入「批次 BL · ADR 0070」条目（**字节级写入 + CRLF**） |

**口径维持（不扩大）**：1017 仍是**人眼可见**观测面；「维护闸门是否放行 = **导出文件 mtime 是否前进**」的机读口径**继续有效**；1017 **不**升级为闸门判据。

**spec 改动**：**无**。本批补的是实现缺失（BK 已定性），不改 FR-016 任何字样。

**tasks.md 改动**：**无**。BL 是缺陷修复批次，**未勾选** spec 100 的任何阶段任务（AGENTS.md §9 的双产物义务仍照做：handoff + prompt + `CLAUDE.md` 指针）。

---

## 5. 门禁（全部 **verified**，本轮实测）

| 项 | 命令 | 实测 |
|---|---|---|
| 构建 | `build.bat release weasel installer` | **`BUILD_EXIT=0`**；weasel.sln Release/Win32 **0 个警告 0 个错误**；makensis 仅 8 条既有 `warning 7010`（ARM/ARM64 可选产物 + `data\*.gram` + `data\lua\*.txt`） |
| 云单测 | `Release\TestFluxingCloud.exe`（**不在 `weasel.sln`**） | **`RESULT: PASS=653 FAIL=0 / VERDICT: ALL PASS / EXIT=0`**（基线 624 ⇒ **+29**；BL 组 29 条断言全绿） |
| L0 | `scripts/gate/L0-static.ps1` | **`checks=15 pass=15 fail=0 skip=0`**（`L0_EXIT=0`） |
| 格式 | clang-format 18.1.8 `--style=file --lines=…` **4 个**改动文件（3 生产 + 1 单测） | 首轮抓到 **4 处真实漂移**（生产 3 + 单测 1 组）⇒ `clang-format -i --lines=` 就地修 ⇒ **复检 `EXIT=0` / 0 漂移**（整文件存在**既有**漂移，故必须按改动行区间定点查） |
| 编码 | 7 个改动文件字节扫描 | **U+FFFD = 0**；`CHANGELOG.md` `crlf=7630 bareLF=0`；handoff / prompt / ADR **LF-only** |

**BL 组 29 条断言分组**：推送 401→续期→重试带新令牌成功（8）· 拉取同款（6）· **无**续期回调 ⇒ 401 直接失败且只发一次请求（3）· 续期失败 ⇒ 原因取自回调、不再重试（4）· 连续 401 ⇒ 只续期一次、最多两次请求（4）· 协调器透传（4）。

---

## 6. 产物与装机实证（**verified**）

| 项 | 值 |
|---|---|
| 载荷 | `output\Win32\WeaselServer.exe` = **2 795 008 B** / 2026-11-22 00:20:17 / sha16 `70C965EF62F5B2C8` |
| 装机包 | `output\archives\fluxing-1.3.8-installer.exe` = **43 005 358 B** / 2026-11-22 00:21:46 / sha16 `22248B6581F1623E` |
| 装机命令 | `Start-Process -FilePath <installer> -ArgumentList '/S','/D=D:\Program Files\fluxing'`（`/D=` **必须显式且在最后**） |
| 装机结果 | **`INSTALL_EXIT=0`**（未超时） |
| 装机件 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **2 795 008 B** / sha16 `70C965EF62F5B2C8` ⇒ **`SHA_MATCH_BUILD_VS_INSTALLED=True`** |
| 服务进程 | 装机前 **pid 407040**（2026-11-21 22:24:25 起）→ 装机后 **pid 94448**（2026-11-22 00:23:53 起，ver 1.3.8.0，单实例，路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe`） |
| 未动项 | `HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` **未动**（`unchanged=True`）；**未登出、未登录、未改注册表、未 /quit、未杀宿主**（服务进程由安装器自己换掉） |

---

## 7. 已验证 / 推断 / 未验证（逐条标注）

**已验证（verified）**
- 同步服务 401→续期→重试的**生产实现逻辑**（推 / 拉 / 协调器透传 / 无回调 / 续期失败 / 连续 401 六种形态）—— 判据全部来自**生产实现**，单测内写独立期望值逐字比对（R7）。
- 续期回调**只被调用一次**、**最多两次请求**（不循环）。
- 无续期回调时 401 行为与修复前**逐字一致**（向后兼容）。
- 账号页注入点位于**后台线程**的 `RunSyncRound`（不阻塞 UI，符合宪法 P2）。
- 构建 / 单测 / L0 / 格式 / 编码 / 装机 sha 全部实测通过（数字见 §5、§6）。

**推断（inferred）**
- 修复后真机同步在令牌过期时**应当**自动续期并成功 —— 依据是 §5 的单测覆盖了生产实现的同一段逻辑；但**未在真机上观测到**（见下）。

**未验证（not verified）**
- **真机端到端「令牌过期 → 续期 → 同步成功」**：需要在真机上制造一个**已过期的访问令牌**（本次未制造；服务端令牌有效期由服务端下发）⇒ 1017 上「失败（访问令牌已过期…）」**是否转为成功文案，本轮没有真机证据**。
- 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照（**需两台机器 + 用户在场**）。
- 缺陷 A 反向判据 G-3/G-5 真跑（**需两台机器 + 用户在场**）。

---

## 8. 下一步计划（按顺序，含理由）

1. **真机端到端复验本批修复**（**需用户在场**）：让账号页访问令牌过期 → 点「立即同步」→ 1017 应显示**成功**而非「访问令牌已过期，请用刷新令牌续期。」。**排在最前**，因为这是本批唯一的功能未闭环项；判据可用 BK 的现成探针 `reports/fluxing-bk-syncstatus/probe-sync-status.ps1`（按**控件 ID 1017** 定位，不按中文标题）。
2. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照**（**需两台机器 + 用户在场**）—— 顺序在第 1 项之后，因为它需要同一台机器已处于「已登录 + 已装机」状态。
3. **缺陷 A 反向判据 G-3/G-5 真跑**（**需两台机器 + 用户在场**）—— 用 BG 批留下的 `scripts/verify/orphan-gate-g35-export-mtime.ps1` 配方，**唯一可用判据仍是导出文件 mtime 是否前进**。
4. **待用户拍板的两项小事**（不阻塞上面三条）：① 工作区里 **18 份 `run-*.log`** 被 `.gitignore` 挡住，是否 `git add -f` 入库；② `tmp\bh-c2d-fmt-check.cpp`（**83 767 B**）本机策略删不掉，**需用户手动处置**。

---

## 9. 证据路径

| 内容 | 路径 |
|---|---|
| 构建 + 单测全量输出 | `F:\soft\00selfmade\rime_claude\reports\bl-build-verify.log` |
| L0 门禁输出 | `F:\soft\00selfmade\rime_claude\reports\bl-l0-gate.log` |
| 装机实证输出 | `F:\soft\00selfmade\rime_claude\reports\bl-install-verify.log` |
| 本批生产码 diff（快照） | `F:\soft\00selfmade\rime_claude\reports\bl-code-diff.txt` |
| 安装包 | `F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.3.8-installer.exe` |
| 构建载荷 | `F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe` |
| ADR | `F:\soft\00selfmade\rime_claude\docs\adr\0070-sync-path-token-refresh-retry.md` |
| BK 真机探针（复用） | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\probe-sync-status.ps1` |

> ⚠️ 三个 `.log` 证据文件命中 `.gitignore` 的 `*.log` 规则 ⇒ **未入库**，只存于本机工作区（是否 `git add -f` 入库属「待用户拍板」项，见 §11）；`bl-code-diff.txt` 已随本批入库。

---

## 10. 本轮工具坑（勿重走）

1. **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 必须显式 msbuild **且带 `/p:SolutionDir="F:\soft\00selfmade\rime_claude\"`**，否则 `FluxingLocalChangeHook.h` 找不到（BG 批同坑）。且 `Release\TestFluxingCloud.exe` **必须在仓库根跑**（在 `release\` 下跑会假红）。
2. **首次跑云单测 `FAIL=4`，全部集中在「⑥ 协调器透传」** —— 根因是假后端导出**空**快照，而 `ParseUserDictSnapshots("")` 返回 **false**（空快照 = 失败，**不等于**「无本地条目」）⇒ 轮次在发请求前就退出。**修法（已落盘）**：新增 `BlGateBackend`，让假后端导出**可解析**的最小快照文本。**教训：协调器级用例的假后端必须导出可解析快照。**
3. **整文件 clang-format 有既有漂移**（本批 4 个改动文件整文件 `EXIT=1`）⇒ **必须** `--lines=起:止` 定点查（区间从 `git diff -U0` 的 hunk 取）。本批定点首轮抓到 **4 处真实漂移**（生产 3 + 单测 1 组）⇒ `clang-format -i --lines=` 就地修后**必须复检 `EXIT=0`**。
4. **改完源码格式 ⇒ 产物 sha 会变** ⇒ 本批因此**重跑了一次完整构建 + 重装**（第一版 installer `42989383 B` / sha16 `8AE9F005699E1C2F` **已作废**，最终值见 §6）。**顺序：格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha。**
5. **`build.bat` 的日志 `msbuild1.log` / `msbuild2.log` 是 GBK/UTF-16 混杂** ⇒ 用 `Get-Content` 直接读会得到乱码、`Warning(s)`/`Error(s)` 正则**一个都搜不到**（误以为「没有汇总行」）。**正确姿势**：按 BOM 判 `Encoding.Unicode`，否则 `Encoding.GetEncoding(936)`，再搜「警告 / 错误 / 已成功生成」。本批据此确证 **0 个警告 0 个错误**。
6. **`CHANGELOG.md` 是 CRLF-only**（本批复核 `crlf=7630 bareLF=0`）⇒ 插条目必须**字节级**写入并把 LF 转 CRLF；而 `docs/archive/handoffs/*.md`、`docs/archive/prompts/*.md`、`docs/adr/*.md` 相反是 **LF-only** ⇒ 两种文件别用同一套行尾策略。
7. **NSIS `InstallDirRegKey` 指向 32 位注册表视图**（真实位置 `HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir`，本轮实测 = `D:\Program Files\fluxing`，与目标一致）⇒ `/D=` 生效 ⇒ **无需清注册表**；清 `HKCU\Software\Fluxing` 反而会**丢 `RimeUserDir`**（= 丢用户数据指针）。
8. **静默装机用 `Start-Process -ArgumentList '/S','/D=D:\Program Files\fluxing'` + `Wait-Process -Timeout 300`**（比 `Start-Process -Wait` 稳，不会挂死不返回）；`/D=` **必须是最后一个参数且不加引号**。
9. **NSIS 安装器会按映像名杀旧服务进程**（L0 检查项 `installer-ownui-kill`）⇒ 装机后服务 pid **必然变**（407040 → 94448），这是**预期**行为，不是宿主被杀。

---

## 11. 需要用户在场的事

- 真机端到端复验本批修复（§8 第 1 项）—— 需一个**已过期的访问令牌** + 用户点「立即同步」。
- 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照（§8 第 2 项）。
- 缺陷 A 反向判据 G-3/G-5 真跑（§8 第 3 项）。
- `tmp\bh-c2d-fmt-check.cpp`（83 767 B）本机策略删不掉，**需用户手动处置**。
- 18 份 `run-*.log` 是否 `git add -f` 入库，**待用户定**。

---

## 12. 提交实况（按实际 `git log` 回填）

| # | commit | 内容 |
|---|---|---|
| 1 | `d9d8a55` | `fix(WeaselServer): 同步路径补 401→续期→重试 —— 访问令牌过期不再让同步持续失败（ADR 0070）` —— 代码 3 文件 + 云单测 + ADR 0070 + `docs/adr/README.md` 索引 + `CHANGELOG.md` 条目 + `reports/bl-code-diff.txt` |
| 2 | `0639fd3` | `docs(fluxing): 批次 BL 交接文档与交接指针` —— 本 handoff + 接力 prompt + `CLAUDE.md` §2 指针 |

- 分支 `feat/100-cloud-sync`，基线 `ab4e350`（BK 批）。
- **提交范围刻意收窄**：只 `git add` 本批自己的文件 ⇒ `librime` 子模块的 ` m`（**非本批改动**）、`release/`（构建产物）、`docs/design/quickpanel-v6/design-demos/*.html`、`reports/fluxing-bh-ui/probe-bh.ps1.bak-pass8`、`scripts/verify/diag-host-window-enum.ps1`、`scripts/verify/orphan-probe-host.ps1`、`scripts/verify/verify-orphan-session-reclaim.ps1` **全部留在工作区未入库**（属其它批次/待用户定，**不夹带**）。
- **⚠️ 本批唯一的机读证据缺口**：三个 `.log`（`bl-build-verify.log` / `bl-l0-gate.log` / `bl-install-verify.log`）命中 `.gitignore` 的 `*.log` ⇒ **未入库**，只在本机工作区（`git add -f` 属待用户拍板项）。
- 本批**未 push 之前**先做 `git status -sb` + `git diff --stat` 复核（AGENTS.md §3），确认无 `weasel.props` / `env.bat` / `*.obj` 误入。
- **推送（已闭环）**：远端名是 **`kizemo`**（**没有 `origin`** ⇒ `git push origin` 必报 `'origin' does not appear to be a git repository`，别当成网络问题）。直连先试：`git -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync` ⇒ **失败** `fatal: unable to access ...: Recv failure: Connection was reset` ⇒ 改走**默认代理**再推 ⇒ **一次成功**（`ab4e350..168d0f1`，`PUSH_EXIT=0`）；`git ls-remote kizemo feat/100-cloud-sync` = `168d0f13a6abe8de233207db5926d7aaabb85350` = 本地 HEAD ⇒ **本地与远端一致**。
- 本小节的回填本身构成**第 4 个提交**（`docs(fluxing): 批次 BL 交接文档补记推送实况`），紧随 §12 其余内容一并推送；它自身不在上表内。
