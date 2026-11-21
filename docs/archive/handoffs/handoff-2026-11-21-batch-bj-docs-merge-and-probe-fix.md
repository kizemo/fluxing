# 批次 BJ · BH/BI 回写合并 + BG 资产重建 + A-4/A-5 探针索引修复 + push

> 日期：2026-11-21 · 分支 `feat/100-cloud-sync` · 本批性质：**文档合并 + 探针修复 + 资产重建 + push**
> **产品代码零改动**（未改 `librime/`、未改 IPC 协议消息类型、未改 `WeaselServer/**`、未改 `include/**`、未改 `install.nsi` / `version.txt` / 任何 `.rc` / `.vcxproj`、未新增依赖、未 bump 版本号；装机版仍 **1.3.8.0**，判定一律用 sha16/字节数，版本号无判别性）。
> 上一批：[BI · c2d「点他处不收起」根因修复 + BH 三需求全套出库](handoff-2026-11-21-batch-bi-c2d-fix-and-commit.md)
> 触发：用户 2026-11-21 指令「读 `CLAUDE.md` 与 BI handoff，再按其 §10 的顺序推进」（**不要从零重做已闭环项**）。

---

## 1. §10 逐条对账

| §10 项 | 状态 | 本文件落点 |
|---|---|---|
| ① push 本批提交 | **✅ 已完成**（两轮） | §2 |
| ② BH handoff §4.5–4.7 + §8 更正合并进 BH handoff，并补 `CLAUDE.md` BH/BG/BI 三行指针 | **✅ 已完成**（`34ca274`） | §3 |
| ③ BG 遗留（需两台机器 + 用户在场） | **⛔ 阻塞**（单机会话不可执行） | §6 |
| ④ 单独修 `probe-a4-a5.ps1` 尾部索引瑕疵 | **✅ 已完成并提交**（`4ab2e1e`） | §4 |
| ⑤ 处置遗留脏文件 `tmp\bh-c2d-fmt-check.cpp` | **⛔ 阻塞**（本机策略删不掉） | §6 |
| ⑥ 勿重跑清单 | **✅ 全程遵守** | §6 |
| （追加）BG handoff 与 BG 配方重建 | **✅ 已完成**（原资产**从未入库且已从盘上消失**） | §5 |

**本批提交链**：`4ab2e1e`（探针修复） → `34ca274`（BH 回写 + 指针） → `c042260`（BG 资产重建） → BJ 文档提交（`git log -1 --format=%H` 为准）。

---

## 2. §10.1 push（已完成）

- **轮次 1**（上一轮）：`git push kizemo feat/100-cloud-sync` ⇒ `d7722e2..76b9e70`，`PUSH_EXIT=0`；`git ls-remote kizemo refs/heads/feat/100-cloud-sync` = `76b9e7016771bd6c20d181fae95154cbfb7c3032`。
- **轮次 2**（本批）：`76b9e70..c042260`，`PUSH_EXIT=0`；`git ls-remote` = `c042260880ffdf1484076715dacbee402c7945e7` ⇒ `git status -sb` 与 `kizemo` **完全同步**（无 ahead / behind）。
- **通道探测**（动手前两条各发一次，BI §10.1 明写）：本轮 `Invoke-WebRequest -Method Head https://github.com` ⇒ **直连 `-NoProxy` 200**、**代理 `http://127.0.0.1:7897` 200**（两条都通）；git 的 `http.proxy` / `https.proxy` 已指向该代理。
  ⚠️ **可用性会随网络状态反转**（BI 轮 = 直连 timeout、BE 轮 = 直连可通）⇒ **每轮都要重探两条通道**，勿照抄旧配方。直连兜底写法：`git -c http.proxy= -c https.proxy= push ...`。

---

## 3. §10.2 BH handoff 回写 + `CLAUDE.md` 三行指针（`34ca274`）

### 3.1 BH handoff 回写

文件：`docs/archive/handoffs/handoff-2026-11-21-batch-bh-cloud-account-ui-3req.md` —— **18198 B / 165 行 → 28244 B / 231 行**（`git diff --stat` = `+88 / −11`）。

| 动作 | 内容 |
|---|---|
| 新增 **§4.6** | 第 8 轮（`run-uicheck8.log`，20:58:42，**未改探针**）：`C2_VERDICT=PASS c2a_open=True c2b_pick=True c2c_reopen=True c2d_hide_on_other=True c2d_judged=True all_clicks_delivered=True` ⇒ **c2d 已修**；同轮 `C3_VERDICT=NOT_JUDGED c3_setup=False`、`BH_VERDICT=FAIL EXIT=4` ⇒ **是探针门未满足，不是需求失败** |
| 新增 **§4.7** | 第 9 轮（`run-uicheck9.log`，21:03:22）：`C3_VERDICT=PASS c3_setup=True`、**`BH_VERDICT=PASS EXIT=0`**、`C3_email_CENTER_DELTA=1.5` / `C3_pwd_CENTER_DELTA=1.5`、`C3_POSCTL_OK=True`；因果日志两分支各被独立命中（`c2d parentnotify hide id=` / `c2d parent lbuttondown hide`） |
| 新增 **§9** | 提交实况与更正：① **「BH 改动已提交 `e85259c`」为假**（`e85259c` = BG 的 `test(ci)`，**不含 `WeaselServer/`**）⇒ 真入库 = `3450472`（代码）+ `76b9e70`（文档与证据）② 三条判据同取 ③ **订正两处不精确**：`CenterV` 在 HEAD **0 命中**（真实现 = `CloudAccountDialog.cpp:1202` 的 `const int line_px = tm.tmHeight + tm.tmExternalLeading;`）；`c1_save_account` / `save account=checked` **是探针输出字段名、不是代码字面量** ④ `.gitignore` 挡住 18 份 `run-*.log`（**待用户定**） |
| 新增 **§10** | 本批遗留（BG 遗留 / 1017 不可观测 / 18 份 log / `tmp\bh-c2d-fmt-check.cpp` / 勿重跑清单） |
| 改写 **§7** 引言 | 标「原第 1、2 项均已闭环」，下方第 1–5 项**保留原样作历史记录**，现役项以 §8 表 + §9 + §10 为准 |
| 改写 **§8** 表 | 6 行全部按实测改写（① ② ②c ③ 提交 push 装机版），不再写「未提交 / 未 push」 |
| 改写 **标题行** | 原「HEAD `e85259c`（本批产品改动尚未提交）」→ 订正为「`e85259c` 实为 **BG 批次**提交；本批产品改动已入库 = `3450472` + `76b9e70`，已 push」 |

**判据来源是盘上原文，不是转述**：第 8/9 轮判据逐行取自 `reports/fluxing-bh-ui/{run-uicheck8.log, run-uicheck9.log, ui-20261121-205842-UiCheck.txt, ui-20261121-210322-UiCheck.txt}`。

### 3.2 `CLAUDE.md` 指针行

新增 **BH**（L43）与 **BG**（L44）两行，**BI 行保留在 L42** ⇒ 指针序 **BI → BH → BG → BF**（newest-first）。
⚠️ **BI §7 建议「另起新文件名以免与并行会话抢同一文件」** —— 本批未采纳，理由：BH handoff mtime（20:33:07）与 `CLAUDE.md` mtime（21:22:58）已长时间未变（21:47 复核），并行会话看起来已收手；且 BI §10.2 明确要求**合并进 BH handoff + 补三行指针**。风险用「**写前复核 mtime + 写后立即 commit**」兜底（实测写后同轮 `git status` 只剩已知项，mtime 未被回退）。

---

## 4. §10.4 `probe-a4-a5.ps1` 尾部索引瑕疵修复（`4ab2e1e`）

### 4.1 根因（**不是**「空数组越界」）

- **实证**：PS 7.7 下 `$a=@(); $a[0]` / `$a[-1]` **均不报错**。真正报错文本「不能为空数组建立索引。」= .NET `Cannot index into a null array`，**仅当对 `$null` 取下标**才产生。
- **精确根因**：`probe-a4-a5.ps1` 的 `$LOGIN_TABLE` / `$LOGOUT_TABLE` 用 `[ordered]@{...}`（`OrderedDictionary` **同时实现 `IList`**）⇒ PowerShell 对整数下标**按序号**解析 ⇒ `$table[1004]` **恒为 `$null`** ⇒ 下一行 `$b[0]` 抛 null 索引。
  - 实证：`$o[1004]` = `$null`；`$o.Item(1004)` 抛 `Index was out of range. Must be non-negative and less than the size of the collection`；`$o[0]` 返回**第一项**；普通 `@{ 1004 = @(12,70,140,28) }[1004]` **正常**返回 4 元素数组。
- **崩溃行**：`$_.ScriptStackTrace` → `probe-a4-a5.ps1: 行 409`（几何期望值计算行）；崩溃前 `G_*` 几何行**一条未产出** ⇒ 该探针的**几何与 A-5 段从未执行过**（BF 的 A-5 数据实际来自独立探针 `probe-a5.ps1`，其 `[ordered]@{` **无 `$table[...]` 键索引** ⇒ 不受影响，BF 已入库证据仍有效）。
- **第二个潜在缺陷**：第 409 行 `@(MulDiv96 $b[0] $dpiW, MulDiv96 $b[1] $dpiW, ...)` 的多参调用形态会把 `$dpi` **绑成数组** ⇒ `$v * $dpi` 抛 `op_Multiply`；加显式括号后实测正确（`21,123,245,49` = `floor((v*168+48)/96)`）。

### 4.2 修法（4 处，15 insertions / 6 deletions，探针 28126 B，无 BOM，新增注释全 ASCII）

1. `$LOGIN_TABLE = [ordered]@{` → `@{`
2. `$LOGOUT_TABLE = [ordered]@{` → `@{`
3. 几何循环：加 ASCII 注释说明 OrderedDictionary 陷阱 + 缺表项显式记 `G_<key>=TABLE_ENTRY_MISSING`（计入几何不符）+ `$ex = @((MulDiv96 $b[0] $dpiW), (MulDiv96 $b[1] $dpiW), (MulDiv96 $b[2] $dpiW), (MulDiv96 $b[3] $dpiW))`
4. 三处空数组守卫（L307 / L428 / L434）改用 `$inkFirst` / `$inkLast`（空则 -1）

**范围刻意不扩大**：不改产品码、不改探针的 VERDICT 语义（几何不符在原探针中本就不影响 `$fail`）⇒ **不使 BF 已登记结论失效**。

### 4.3 复验证据（独立进程，避免同会话 `Add-Type` 重名）

| 模式 | 结果 |
|---|---|
| `Baseline` | `EXIT=0`、`VERDICT_BASELINE=PASS`、`ROWS_WITH_INK=147 FIRST=26 LAST=485` |
| `Relogin` | `EXIT=0`、`A4_RESTORE_VERDICT=PASS`、**9 条 `G_*` 几何行首次全部产出且 `ok=True`**、`GEOMETRY_MISMATCH_COUNT=0`、`ROWS_TOTAL=508 FIRST_INK=26 LAST_INK=485`、`A5_gap1=21px A5_gap2=21px A5_trailing_blank_below_last_ink=22`、`VERDICT_RELOGIN=PASS` |

- **未登出验证**：在**账号已登录态**跑 `-Mode Relogin`，走 `GUARD=ALREADY_LOGGED_IN_skip` 分支 ⇒ **账号态零变化**；服务端仍 pid **479804** / 1.3.8.0。**不跑 `-Mode Logout`**（§10.6 勿重跑 + 避免登出风险）。
- 语法体检 `[Parser]::ParseFile` ⇒ `PARSE_ERRORS=0`。
- **提交**：`4ab2e1e fix(fluxing): 修 A-4/A-5 探针有序字典整数索引越界并补真机复验证据` ⇒ **9 files changed, 135 insertions(+), 6 deletions(-)**（`probe-a4-a5.ps1` + `reports/fluxing-bf-a4-a5/fixcheck-20261121/`）。

---

## 5. （追加）BG handoff 与 BG 配方重建（`c042260`）

### 5.1 为什么必须重建

- 原 BG handoff `handoff-2026-11-21-batch-bg-verify-g35-delegated.md` 与其配套配方 **已被同工作区另一会话用 `Copy-Item` 还原抹掉**（BI §7 记载），且**从未进入 git 历史**：
  `git log --all -- 'docs/archive/handoffs/*bg*'` **0 命中**、`git log --all -- 'maintenance/'` **0 命中**、`docs/archive/handoffs/` 下无任何 `*bg*` 文件 ⇒ **不能靠 git 恢复，只能重建**。
- 重建基座 = **已入库**的 `reports/fluxing-bg-g35/harness-report.md`（402 行）+ BI §2 的压缩重述 + `docs/release/restart-checklist-1.3.6.md` §4（G-3/G-5 原文）。

### 5.2 产出（两文件，**已入库**）

| 文件 | 字节 | 行 | 形态 |
|---|---|---|---|
| `docs/archive/handoffs/handoff-2026-11-21-batch-bg-verify-g35-delegated.md` | **25064** | **279** | 批次档案（目标/结论口径/闸门判据/造态四步/已验证·推断·未验证三档/交付物/相位编排/未闭环/工具坑/证据路径/下一批建议） |
| `maintenance/BG-G35-conditional-feasibility.md` | **25551** | **335** | **可执行配方**（前置核对表 P/R 两类 + 预检脚本 + 构建 + 造态 + 相位时序 T0–T5 + 每相位判据 + 退出码语义表 + 失败排查三分支 + INCONCLUSIVE/ABORT 判读规则 + 硬前置缺失时的登记话术） |

### 5.3 口径固定（写死在两份文件开头）

- **可行性 = CONDITIONAL YES**；**执行判定 = `BLOCKED_HARNESS_PRECONDITION`**（环境前置不可得 ⇒ **非缺陷、非 PASS、也非 FAIL**）。
- **两条硬前置**：(a) 旧端须用**未含 R3 守卫的 1.3.5 构建**（非 1.3.4 装机包）；(b) 须**手工老化** `fluxing-rime-backup\profile.tar.gz` 的 mtime。
- **BG 自身的实验从未真跑** ⇒ 首次照抄执行配方产出的是**第一份真机结果**，**不得引用任何「BG 已 PASS」的说法**。

### 5.4 重建时额外抓到的硬事实

- 装机版 `WeaselServer.exe` 含 `status.composing=` ⇒ 自证通道真机可用。
- `MSBuild.exe`（VS 18 BuildTools）路径存在 ⇒ 配方命令**可照抄**。
- ⚠️ **`Abort` 分支即使退出码 5 也打印 `VERDICT=GATE_G35_FAIL`** ⇒ **判读必须以退出码为准**（已作为坑写进两份文件）。
- ⚠️ **「R3 守卫」的权威定义在本仓库无源**（现存文档中「守卫」只出现在 `restart-checklist-1.3.7.md:62`，已登记为待定位）；`fluxing-rime-backup\profile.tar.gz` 本机 `D:\Program Files\fluxing` 下 **0 命中** ⇒ 配方写为「真实双机环境现场定位」，**未虚构路径**。

---

## 6. 未闭环 / 阻塞 / 遵守项

| 项 | 状态 |
|---|---|
| **BG 遗留**：双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + G-3/G-5 真跑 | **⛔ 需两台机器 + 用户在场**；配方已就绪（§5） |
| **`kIdSyncStatus`(1017) 不可观测** | **待用户拍板**：刷新只写 `kIdStatus`(1006)，从不写 1017；最小修法约 3 行产品码 ⇒ **独立 ADR 批次** |
| **`.gitignore` 挡住 18 份 `run-*.log`** | **待用户定**：入库需 `git add -f`（等价内容已入库为 `ui-20261121-205842-UiCheck.txt` / `ui-20261121-210322-UiCheck.txt`） |
| **`tmp\bh-c2d-fmt-check.cpp`（83767 B）** | **⛔ 需用户手动处置**：本机 `Remove-Item` 被硬策略整条拒、`rm --` 报 `requires the targets-only trash marker` |
| **勿重跑** | **✅ 全程遵守**：A-1~A-5、B-1~B-5、`/verify-client-pid`、175%/200% DPI 切档、c2d 复验、c3 量值 |
| **有意未入库** | `librime` 脏指针、`release/`、`docs/design/quickpanel-v6/design-demos/{a-benchmark,b-craft,c-roulette}.html`、`scripts/verify/{diag-host-window-enum,orphan-probe-host,verify-orphan-session-reclaim}.ps1`（疑并行会话产物）、`reports/fluxing-bh-ui/probe-bh.ps1.bak-pass8`（探针改前留档） |

---

## 7. 本批新增工具坑（务必带进下一会话）

1. **`[ordered]@{}` 的整数下标 = 序号，不是键** ⇒ 键查表恒 `$null`，随后 `$null[0]` 抛「不能为空数组建立索引。」。**判据表一律用普通 `@{}`**；若必须有序，用 `[ordered]` 但**只枚举、不按整数下标查键**。
2. **PowerShell 多参调用 `@(f $a $x, f $b $x, ...)` 会把后段绑成数组** ⇒ `$v * $dpi` 抛 `op_Multiply`。**逐项加显式括号**：`@((f $a $x), (f $b $x))`。
3. **本机单测 `release\TestDefaultHotkeys.exe` 必须在仓库根跑**：在 `release\` 下跑会 `FAIL: cannot open output/data/default.yaml` ⇒ **假红**。正确口径 = 仓库根执行 ⇒ `Passed: 91 / 91`、`HOTKEYS_EXIT=0`。
4. **`Abort` 分支即使退出码 5 也打印 `VERDICT=GATE_G35_FAIL`** ⇒ BG/G-3/G-5 类判读**一律以退出码为准**（0=OK / 4=FAIL / 5=ABORT / 6=INCONCLUSIVE）。
5. **单会话内重复调用含 `Add-Type` 的探针会因类型重名失败** ⇒ 用独立 `pwsh -NoProfile -File` 进程复跑。
6. **交接文档会被外部会话用 `Copy-Item` 还原抹掉（mtime 反而变旧）** ⇒ **写前复核 mtime + 写后同轮 commit**；本批 3 个新提交在写后 10 分钟内全部入库。
7. **工具自述不是证据**（第 4 次实证）：`write` 报「Successfully wrote N bytes」≠ 盘上存在 ⇒ 一律「写 → 同轮 `Test-Path` + 字节/行数回读」。
8. **markdown 文件的字节体检口径**：本仓库 handoff / `CLAUDE.md` 在 git 里**本身就是 LF**（`HEAD~1` 也是 `CR=0 LF=164`）⇒ AGENTS.md §5 第 1 项的「CRLF-only」断言对这些文件**天然不成立**，判据应是**「与既有约定一致」**而非「必须 CRLF」；`overlong_utf8=0` 与「无 BOM（`install.nsi` 除外）」两条才是真判据。

---

## 8. 本批门禁（AGENTS.md §5）

| 步 | 结果 |
|---|---|
| ① 字节体检 | `CLAUDE.md` / BH handoff / BG handoff / BG 配方 **overlong_utf8=0**，行尾与既有约定一致（LF），**无 BOM**（`install.nsi` 除外，本批未碰） |
| ② 单测 | `release\TestDefaultHotkeys.exe`（**仓库根执行**）⇒ `Passed: 91 / 91`、`HOTKEYS_EXIT=0` |
| ③ 构建卫生 | `git status` 无新增 `*.log` / `*.build.log`；`weasel.props` / `env.bat` 未被触碰 |
| ④ 格式 | **不适用**（本批零 C++ 改动） |
| ⑤ 范围 | 三个提交均为单一 scope（`fix(fluxing)` / `docs(fluxing)`）⇒ 合 P4 |

**提交判据三取**（同轮一起）：`git cat-file -t HEAD` = `commit`；`git show --stat --oneline HEAD` 如上；`git grep -F '<新符号>' HEAD` 有命中。
`commit_exit=0` **不算**判据；`Select-String -SimpleMatch` 0 命中**不可**作阴性结论。

---

## 9. 证据 / 产物路径

```
BH 回写    docs/archive/handoffs/handoff-2026-11-21-batch-bh-cloud-account-ui-3req.md  (28244 B / 231 行)
BG 重建    docs/archive/handoffs/handoff-2026-11-21-batch-bg-verify-g35-delegated.md  (25064 B / 279 行)
           maintenance/BG-G35-conditional-feasibility.md                            (25551 B / 335 行)
BG 基座    reports/fluxing-bg-g35/harness-report.md                                  (已入库 / 402 行)
探针修复    reports/fluxing-bf-a4-a5/probe-a4-a5.ps1                                 (28126 B)
           reports/fluxing-bf-a4-a5/fixcheck-20261121/{run-Baseline.txt,run-Relogin.txt,
             baseline-20261121-214640.txt,relogin-20261121-214645.txt,evidence-*.png}
第 8/9 轮   reports/fluxing-bh-ui/{run-uicheck8.log,run-uicheck9.log,
             ui-20261121-205842-UiCheck.txt,ui-20261121-210322-UiCheck.txt}
本文件     docs/archive/handoffs/handoff-2026-11-21-batch-bj-docs-merge-and-probe-fix.md
```

**远端真值**：`git ls-remote kizemo refs/heads/feat/100-cloud-sync` = `c042260880ffdf1484076715dacbee402c7945e7`（BJ 文档提交后再推一次，以 `git ls-remote` 实测为准）。
**真机**：服务端唯一 pid **479804**（装机 1.3.8.0），本机 DPI 168（175%），账号态 = **已登录**（本批未改）。
