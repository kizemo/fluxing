# 批次 BN · 同步轮次提示在成功态可见 —— 结构化 notices 通道（ADR 0072）

> 一句话结论：本批把「本轮同步的提示」从**中文 message blob**里拆出**结构化 `notices` 通道**，让成功态 1017 与手动同步 1006 **真正显示**「云端有 N 条格式非法的条目已跳过」「访问令牌已自动续期」——BM §7/§10.2 登记的**观测面缺口**已由真机复验端到端闭合。

---

## 1. 本批性质

**产品修复 + 门禁 + 出包装机 + 装机实证 + 真机复验 + 提交**。

- **未改** `librime/`；**未改** IPC 协议消息类型；**未新增**第三方依赖；**未 bump 版本号**（装机版仍 1.3.8.0 ⇒ 判定一律用 **sha16 + 字节数**，版本号无判别力）。
- **不动**控件 ID / 布局表 / 不加控件。
- **规格不改**：`.specify/specs/100-fluxing-cloud-account-sync/spec.md` 的 FR-016 只要求界面显示「最近一次同步时间与结果」；本批补的是**实现缺口**，不动 FR 编号与措辞。

---

## 2. 背景与根因

### 2.1 缺口是怎么来的（**verified**）

| 批次 | 做了什么 | 留下的缺口 |
|---|---|---|
| BK（ADR 0069） | 把「最近一次同步时间与结果」写进 `kIdSyncStatus`(1017)，文案收进共享真相源 `AccountSyncStatusText*` | 成功态走 `AccountSyncStatusTextSuccess(when, pushed, pulled)` **独立拼装**，**不消费** `out->message` ⇒ 只吃时间 + 两个计数 |
| BL（ADR 0070） | 同步路径补 401→续期→重试 | 续期这件事只在**日志**里可见 |
| BM（ADR 0071） | 云端形态非法条目跳过并单独计数；message 追加两条提示 | 两条提示**只进了 message**，而成功态**不读 message** ⇒ **成功态看不见** |

- **verified**：成功态分支在 `WeaselServer/Cloud/CloudAccountDialog.cpp`（`CloudAccountController::Impl::RunSyncRound` 成功分支），只调 `AccountSyncStatusTextSuccess(when, out->items_pushed, out->items_pulled)`；`out->message` 被丢弃。
- **verified**：失败态走 `AccountSyncStatusTextFailed(when, reason)`，reason 取自 `err` / `out->message` ⇒ **失败态可见、成功态不可见**（这正是 BM 真机两轮都能看到「失败（访问令牌已过期…）」却看不到两条新提示的原因）。
- **verified**：根因在 BK 批次 ADR 0069 的「成功态独立拼装」设计，**不是 BM 引入**；BM 只是让 message 多出了两条提示。
- **verified（BM 轮实测）**：BM 轮 1017 = 「最近同步：2026-11-22 09:19:54 · 成功（推送 0 条，拉取 1 条）」——**没有**任何提示。

### 2.2 为什么不能靠「解析中文 message」补上（**verified**）

message 是**中文 blob**。成功态若要显示它，只能靠**解析中文**（脆弱、与 message 文案强耦合，改一个字就断）。ADR 0069 当时正是因此选了「成功态独立拼装」。

⇒ 本批的正解是**再加一条结构化通道**，而不是反解 blob。

---

## 3. 修法（ADR 0072 · 9 条决策）

决策全文：[`docs/adr/0072-sync-round-notices-visible-in-success-status.md`](../../adr/0072-sync-round-notices-visible-in-success-status.md)。

1. **结构化 notices 通道**：`CloudSyncRoundResult` 新增 `std::vector<std::string> notices;`（窄字符 UTF-8 中文短句）。
2. **notices 与 message 同源**：在 `CloudSyncCoordinator::SyncOnce` 既有的 message 拼装处，每段可选信息**用同一个字符串变量**既拼进 message、又推进 notices（`count` 之类的数字也共用同一个 `std::string`）⇒ 单测可逐字断言「**每条 notice 都逐字出现在 message 中**」。
3. **message 逐字不变**：既有 message 的每一个字节保持原样（BM / ADR 0071 的期望值、诊断日志、既有探针期望值全部零影响）。
4. **条件追加（保底不变量）**：`AccountSyncStatusTextSuccessWithNotices(when, pushed, pulled, notices)` —— notices 为空 ⇒ 与既有 `AccountSyncStatusTextSuccess` **逐字相同**；非空 ⇒ 在「成功（…）」括号内追加。
5. **分隔符用全角分号「；」**：各条提示是**独立子句**，用逗号会与「推送 N 条，拉取 M 条」里的逗号混淆。
6. **手动同步 1006 一并消费**：`AccountSyncManualDoneTextWithNotices(pushed, pulled, notices)`（基础串「同步完成：推送 N 条，拉取 M 条。」**逐字不变**，追加时句末「。」保留）。理由：同一轮同步的同一批提示，只在一处显示 = 口径漂移的入口。
7. **既有七个文案函数逐字不动**：本 ADR 只**新增**两个函数，不改动 `CloudAccountUi.h` 中既有七个文案函数的任何字节。
8. **不动 IPC 协议 / 不动控件 ID / 不动布局表 / 不加控件 / 不改 `librime/` / 不新增第三方依赖 / 不 bump 版本号**。
9. **单测护栏（批次 BN 组）**：保底不变量、单条提示、多条提示、既有 BK 断言零改动、协调器级同源断言。

### 3.1 notices 覆盖的 6 类片段（**verified**，实现见 `CloudSyncService.cpp` 成功态拼装处）

| 触发条件 | notice 文本 |
|---|---|
| 换口令（`key_rotated_`） | 加密密钥已变化，已重置本地同步状态并全量重推 |
| `items_skipped > 0` | 云端已有更新版本跳过 N 条 |
| `undecryptable > 0` | 旧密钥加密的行无法解密跳过 N 条 |
| `malformed > 0`（BM / ADR 0071） | 云端有 N 条格式非法的条目已跳过 |
| `token_refreshed`（BL / ADR 0070） | 访问令牌已自动续期 |
| `has_more` | 云端还有更多条目，下次同步继续 |

> ⚠️ notice 文本**不带句末「。」**（换口令片段在 message 里才有句点）⇒ 追加到界面文案时标点结构保持一致。

### 3.2 不触发 IPC 四边同步红线（**verified**）

`CloudSyncRoundResult` 定义在 `WeaselServer/Cloud/CloudSyncEngine.h`（**模块内部头**），**不在** `include/WeaselIPCData.h` ⇒ 新增字段不构成 IPC 协议变更，**无需** server / TSF / deployer 三端同步改动 + 协议 ADR + `test/TestWeaselIPC/*` 测试。

---

## 4. 改动文件清单（**verified**，`git diff --stat`）

| 文件 | 改动 | 性质 |
|---|---|---|
| `WeaselServer/Cloud/CloudSyncEngine.h` | +5 | `CloudSyncRoundResult` 新增 `notices` 字段 |
| `WeaselServer/Cloud/CloudSyncService.cpp` | +30/−10 | `SyncOnce` 重置块清 `notices`；成功态 message/notices **同源**装配（6 类片段） |
| `WeaselServer/Cloud/CloudAccountUi.h` | +56 | 新增 `AccountSyncStatusTextSuccessWithNotices` / `AccountSyncManualDoneText` / `AccountSyncManualDoneTextWithNotices`（**既有七个文案函数逐字未动**） |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | +26/−10 | 新增 `ToWideNotices` helper（用生产 `widgets::ToWide`）；成功态 1017 与手动同步 1006 改消费 notices |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | +200/−0 | BN 组：`BnHasNotice()` + `TestSyncStatusNotices()`（18 条断言 / 7 段）+ `main()` 注册 + include |
| `docs/adr/0072-sync-round-notices-visible-in-success-status.md` | 新增 | ADR 0072（9 条决策） |
| `docs/adr/README.md` | +1 | ADR 索引补 0072 行（位于 0071 之后） |
| `CHANGELOG.md` | +N | BN 条目（CRLF-only 字节级写入） |
| `docs/archive/handoffs/handoff-2026-11-22-batch-bn-sync-round-notices-visible.md` | 新增 | 本文档 |
| `docs/archive/prompts/prompt-next-session-2026-11-22-batch-bn-sync-round-notices-visible.md` | 新增 | 接力 prompt（一句 + 全路径指针） |

> 单测改动为**纯新增**（200 insertions / 0 deletions）⇒ 不可能改动既有 BK / BM 断言。

---

## 5. 门禁（**verified**，全部实测）

| 门禁 | 结果 |
|---|---|
| 构建 | `cmd /c "build.bat release weasel installer"` ⇒ **`BUILD_EXIT=0`**；weasel.sln Release\|Win32 **0 个警告 0 个错误**（`msbuild2.log`：「已成功生成。/ 0 个警告 / 0 个错误」）；makensis 仅 8 条**既有** `warning 7010`（ARM/ARM64/ARM64X 可选产物 + `data\*.gram` + `data\lua\*.txt`） |
| 云单测 | 显式 msbuild（带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`）⇒ `Release\TestFluxingCloud.exe` **在仓库根运行** ⇒ **`RESULT: PASS=704 FAIL=0 / VERDICT: ALL PASS / TEST_EXIT=0`**（BM 基线 686 ⇒ **+18**） |
| 单测非陈旧 | `exe.mtime 10:25:41 ≥ src.mtime 10:24:40` ⇒ **NONSTALE=True**；且 4 个生产文件 mtime 均 ≤ 10:25:41 ⇒ exe 含格式化后的代码 |
| L0 静态门禁 | `scripts/gate/L0-static.ps1 -Mode all` ⇒ **15/15**（`checks=15 pass=15 fail=0 skip=0`） |
| clang-format | 定点 `clang-format 18.1.8 --style=file --lines=…` 4 个改动文件：首轮 **1 处真实漂移**（`CloudAccountDialog.cpp` 的 `ToWideNotices` helper）⇒ `-i --lines=73:83` 就地修 ⇒ **重算区间复检 drift=0** |
| 编码 | 4 个生产文件 + 2 个 docs 文件 **U+FFFD=0**；`CHANGELOG.md` **CRLF-only**（`bareLF=0`）；`docs/adr|handoffs|prompts` = LF-only + 无 BOM |

> ⚠️ **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 须显式 msbuild **且带 `/p:SolutionDir=`**（否则 `FluxingLocalChangeHook.h` 找不到），且 exe **须在仓库根运行**（在 `release\` 下跑会 `FAIL: cannot open output/data/default.yaml` ⇒ 假红）。该工程有 3 条**既有** `warning C4996: 'fopen'`（src 行 3265/3276/3316，非本批代码），因不在 sln 故不计入「0 警告」门禁。

---

## 6. 出包装机（**verified**）

| 项 | 值 |
|---|---|
| 安装包 | `output\archives\fluxing-1.3.8-installer.exe` = **42 994 250 B** / sha16 **`BA42507D75A8157D`** |
| 内嵌载荷 | `output\Win32\WeaselServer.exe` = **2 801 664 B** / sha16 **`9F4C6E7C9806D302`** |
| BM 基线对照 | 安装包 42 989 923 B / `24B47CF1A6CBCD25`；载荷 2 797 568 B / `D9D4DB32962F9252` |

> ⚠️ **顺序铁律**：格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha。本批**格式化发生在首次构建之后**（首轮产物 sha16 `81CF28635EA7F384` / `866CED06EFF143BA` **已作废**），故已重启重建后才记本节指纹。

---

## 7. 装机实证 + 真机复验（**verified**）

### 7.1 装机

- `/S /D=D:\Program Files\fluxing` 静默装机 ⇒ 装机件 `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **2 801 664 B / sha16 `9F4C6E7C9806D302`** ⇒ **`SHA_MATCH_BUILD_VS_INSTALLED=True`**。
- 服务进程换为 **pid 193484**（2026-11-22 10:28:35 起，`D:\Program Files\fluxing\weasel\WeaselServer.exe`，ver 1.3.8.0）—— 由**安装器自己换掉**，属**预期**。
- `HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` **未动**；**未登出、未登录、未改注册表、未 `/quit`**。
- ⚠️ NSIS `InstallDirRegKey` 指向 **32 位视图** `HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir` ⇒ `/D=` 生效 ⇒ **无需清注册表**（清了反而丢 `RimeUserDir`）。

### 7.2 真机复验 —— 观测面缺口端到端闭合

复用 `reports/fluxing-bk-syncstatus\probe-sync-status.ps1`（**未改探针一行**；按**控件 ID 1017** 定位账号窗，不按中文标题）：

```
VERDICT=SYNC_STATUS_OBSERVABLE   EXIT=0
SYNC_STATUS_BEFORE=最近同步：尚未同步。
SYNC_STATUS_AFTER=最近同步：2026-11-22 10:29:14 · 成功（推送 0 条，拉取 1 条；云端有 1 条格式非法的条目已跳过）
SYNC_STATUS_CHANGED=True
SYNC_STATUS_HAS_TIMESTAMP=True
SYNC_STATUS_HAS_RESULT=True
SYNC_STATUS_AFTER_IS_PLACEHOLDER=False
SYNC_STATUS_RESULT_KIND=SUCCESS
STATUS_LINE_1006_CHANGED=True
REASON=OK
```

- **前后对照（本批最硬的证据）**：

| | 1017 文案 |
|---|---|
| BM 轮（09:19） | 最近同步：… · 成功（推送 0 条，拉取 1 条） |
| **BN 轮（10:29）** | 最近同步：2026-11-22 10:29:14 · 成功（推送 0 条，拉取 1 条；**云端有 1 条格式非法的条目已跳过**） |

⇒ BM 造的「云端有 N 条格式非法的条目已跳过」这条提示，**第一次在成功态界面可见** ⇒ BM §7 / §10.2 登记的观测面缺口**闭合**。

- `STATUS_LINE_1006_CHANGED=True` ⇒ 手动同步 1006 状态行**同源**消费 notices（ADR 0072 决策 6）。
- 诊断文件（`%TEMP%\fluxing-profile-notify.log`）同轮命中直证行：
  `2026-11-22 10:28:39.012][cloudsync] skipped malformed cloud userdb entry (payload shape invalid)`
  （同一份日志里另有 `[cloudsync] access token refreshed after 401; retry with new token` 多条 ⇒ 「访问令牌已自动续期」提示的另一条来源也在实战中反复触发）。
- 探针报告：`reports\fluxing-bk-syncstatus\sync-status-20261122-102915.txt`。

> **口径维持**：1017 仍是**人眼观测面**，**不升级为闸门判据**（沿用 ADR 0069 / 0070 / 0071 口径）；「维护闸门是否放行 = 导出文件 mtime 是否前进」的机读口径继续有效。

---

## 8. 单测（BN 组，**verified**）

- `test/TestFluxingCloud/TestFluxingCloud.cpp`：新增 `BnHasNotice()`（:4305）+ `TestSyncStatusNotices()`（:4314，**18 条断言 = 16 Check + 2 CheckMany，7 段**）+ `main()` 注册（:4558）+ `#include "CloudAccountWidgets.h"`（:80）。
- **判据全来自生产实现**：复用 BM 批假后端 `BmGateBackend`（:4082）；宽字符转换用生产 `fc::widgets::ToWide`（**未自造第二份**，宪法 R7）。
- 关键断言：
  - 「notices 里逐字有『云端有 1 条格式非法的条目已跳过』」；
  - 「notices 里逐字有『访问令牌已自动续期』」；
  - 「**notices 的每一条都逐字出现在 message 里**」（`CheckMany`，协调器级，**续期轮**再验一次）⇒ 同源不漂移。
  - 保底不变量：无 notices 时输出与既有 `AccountSyncStatusTextSuccess` **逐字相同**。
- 结果：`PASS=704 FAIL=0 / VERDICT: ALL PASS`（BM 基线 686 ⇒ **+18**）。

---

## 9. 本轮工具坑（**verified**）

1. **`cmd //c "build.bat …"` 在本机会被 MSYS/PowerShell 吞参数** ⇒ 只输出 cmd 横幅、exit 0 的**假绿**。正确写法：`cmd /c "build.bat release weasel installer"`。**判据**：看产物 mtime/sha 是否真的前进。
2. **`build.bat` 的 `msbuild1.log`/`msbuild2.log` 是 GBK/UTF-16 混杂** ⇒ 直接 `Get-Content` 搜 `Warning(s)` **一个都搜不到**（误判「没有汇总行」）。须按 BOM 判 `Encoding.Unicode`，否则 `Encoding.GetEncoding(936)`，再搜「警告 / 错误 / 已成功生成」。汇总行在 `msbuild2.log`。
3. **整文件 clang-format 有既有漂移** ⇒ **必须** `--lines=起:止` 定点查；`-i` 修完后**必须重算区间**再复检（区间会随改动漂移）。
4. **改完格式 ⇒ 产物 sha 会变** ⇒ 顺序必须是「格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha」。本批首轮构建产物已作废。
5. **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 须显式 msbuild **且带 `/p:SolutionDir=$repo\`**（尾反斜杠用**单引号**，双引号会解析出错），且 exe **须在仓库根跑**。
6. **协调器级用例的假后端必须导出可解析快照文本**（`ParseUserDictSnapshots("")` 返 **false** ⇒ 空快照 = 失败 ≠ 无本地条目）。
7. **NSIS `InstallDirRegKey` 指向 32 位视图** ⇒ **无需清注册表**（清了反而丢 `RimeUserDir`）。
8. **装机后服务 pid 必变**（NSIS 按映像名杀旧进程），属**预期**不是宿主被杀。
9. **`/D=` 必须是最后一个参数且不加引号**；`Start-Process -Wait` 对 NSIS **挂死不返回** ⇒ 用 `Wait-Process -Timeout 300`。
10. **`CHANGELOG.md` 是 CRLF-only** vs handoff / prompt / ADR **LF-only** ⇒ 别用同一套行尾策略；CHANGELOG 插条目必须**字节级**写入并把 LF 转 CRLF，否则 L0 的 `bareLF` 检查红。
11. **图谱降级**：`codebase-memory-mcp` 的 `search_graph` 两次 **0 命中** ⇒ 按 `.claude/rules/codebase-graph.md` 降级 Read/Grep。
12. **派「只读取证 + 落盘报告」的子会话必须派 `worker`**（`explore`/`verifier` 严格只读，会秒返 `ROLE_MISMATCH`）。

---

## 10. 未闭环（**not verified**，需另一台机器 + 用户在场）

1. **双机 AC 7 条**（BL §8 原计划）。
2. **restore 端到端**。
3. **输入延迟两组对照**。
4. **缺陷 A 反向判据 G-3 / G-5 真跑**。
5. **「令牌过期 → 续期 → 同步成功」端到端**：BL §13 遗留。本批已在真机上看到 `access token refreshed after 401` 日志与「访问令牌已自动续期」提示的**装配**，但**没有**在「已过期令牌」前提下人为复现完整链路（需要制造已过期访问令牌 + 用户在场点「立即同步」）。
6. **「云端还有更多条目，下次同步继续」提示的真机可见性**：`has_more` 分支在真机这轮未触发（单轮拉取即完成）⇒ 其界面呈现**未经真机验证**。
7. **1017 单行 STATIC 控件在多提示极长场景下可能显示不全**（ADR 0072 后果段已登记；本决策**刻意不做**自动换行/截断，避免引入新视觉口径）。

---

## 11. 待用户拍板（沿自 BM §11，不阻塞本批）

1. 工作区 **18 份 `run-*.log`** 是否 `git add -f` 入库（被 `.gitignore` 挡住）。
2. `tmp\bh-c2d-fmt-check.cpp`（83 767 B）本机策略删不掉 ⇒ 需用户手动处置。
3. 上面 §10 的双机项是否安排。

---

## 12. 提交实况（**verified**）

- 分支：`feat/100-cloud-sync` → 远端 `kizemo/feat/100-cloud-sync`；本批提交后 **`[ahead 7]`**，**未 push**（push 未获用户授权）。
- 按 BM 的分提交惯例落 **3 个提交**：

| commit | 类型 | 内容 | 规模 |
|---|---|---|---|
| `cab485a` | `fix(WeaselServer)` | 成功态「最近同步」吃下本轮提示 —— 同步轮次新增结构化 notices 通道（ADR 0072）：4 个生产文件 + 单测 + ADR 0072 + ADR 索引 + CHANGELOG | 8 files, +421/−10 |
| `78726e7` | `docs(fluxing)` | 批次 BN 交接文档、接力 prompt 与交接指针（本文档 + prompt + `CLAUDE.md` §2） | 3 files, +256/−2 |
| `f16a314` | `docs(fluxing)` | 批次 BN 真机复验原始探针输出入库（`reports/fluxing-bk-syncstatus/sync-status-20261122-102915.txt`） | 1 file, +31 |

- 提交前必查（AGENTS.md §3）：`git status -sb` + `git diff --stat` + `git diff --cached --stat` ⇒ 待提交集合 **12 files / +708 / −12**，**未夹带** `weasel.props` / `env.bat` / `*.obj`。
- 提交后工作区只剩**既有污染**（本批**未**夹带）：`librime`（子模块指针 dirty）、`release/`、`docs/design/quickpanel-v6/design-demos/{a-benchmark,b-craft,c-roulette}.html`、`reports/fluxing-bh-ui/probe-bh.ps1.bak-pass8`、`scripts/verify/{diag-host-window-enum,orphan-probe-host,verify-orphan-session-reclaim}.ps1`。
- 提交信息一律经**消息文件**（`git commit -F <file>`）落盘写入 ⇒ 规避「命令行传中文」的编码风险（本轮**未**使用 `git commit -m` 传中文）。

---

## 13. 证据路径清单（**verified**）

| 证据 | 路径 |
|---|---|
| 本批决策依据 | `F:\soft\00selfmade\rime_claude\docs\adr\0072-sync-round-notices-visible-in-success-status.md` |
| 上一批真源（BM） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bm-malformed-cloud-entry-skip.md` |
| 再上一批真源（BL） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bl-sync-token-refresh-retry.md` |
| 既有 ADR（0069 / 0070 / 0071） | `F:\soft\00selfmade\rime_claude\docs\adr\` |
| 可复用探针 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\probe-sync-status.ps1` |
| 本轮探针报告 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\sync-status-20261122-102915.txt` |
| 规格（本批**未改**） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` |
| 生产代码 | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\{CloudSyncEngine.h,CloudSyncService.cpp,CloudAccountUi.h,CloudAccountDialog.cpp}` |
| 单测 | `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp` |
| 安装包 | `F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.3.8-installer.exe` |
| 诊断日志 | `%TEMP%\fluxing-profile-notify.log` |
