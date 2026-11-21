# 批次 BI · c2d「点他处不收起」根因修复 + 提交 BH 全套 + 并发还原取证

> 日期：2026-11-21（接 BF / BE / BD / BC / BH 之后）
> 本批性质：**代码修复 + 出包装机 + 真机复验 + 提交**（未改 `librime/`、未改 IPC 协议、未动版本号、未 push）
> 触发：用户对「本批改动现在提交吗？」拍板 **「先修 c2d 再提交」**（否掉「现在就提交，暂不推送」与「提交并推送」两个选项）。
> ⚠️ **本 handoff 是本批唯一的文档真源**：上一轮我写的 `handoff-2026-11-21-batch-bg-verify-g35-delegated.md` 与其 prompt **已在工作区消失**、`CLAUDE.md` 的 BG 指针行与 BH handoff 的 §4.5–4.7 追加**被外部还原抹掉**（详见 §7）。故 BG 的结论在本文件 §2 内做一次压缩重述。

---

## 1. 背景与目标链

- **BH 批次**做了账号页三条需求（① 登录态改为勾选「保存账号」② 已保存账号改为聚焦邮箱框自动下拉、复用输入框自身、不另加模块 ③ 填写框文字垂直居中并与标题垂直对齐）。BH 第 7 轮真机复验暴露 **`c2d_hide_on_other=False`**（下拉列表可见时点击他处不收起）。
- 用户拍板：**先修 c2d，再提交**。⇒ 本批目标 = ① 定根因并修好 ② 重跑全套门禁 + 出包装机 + 真机复验 ③ 把 BH 三需求 + c2d 修复提交入库（先不 push）。
- 本批还顺带闭环了 BF/BE 留下的一个探针缺陷（`c3_setup` 恒 False）。

## 2. BG 批次结论压缩重述（原 handoff 已丢失）

BG 的反向判据 **G-3 / G-5 判定 = `BLOCKED_HARNESS_PRECONDITION`（环境前置不可得，非缺陷、非 PASS/FAIL）**：

- 前提不可得：本机是**单机**（第二台设备离线）且**服务端 1.3.4.0→1.3.8.0 升级链从未真实发生**。G-3/G-5 要的态是「1.3.4 及更早客户端 + 导出 mtime 已老化」⇒ 只能真造。
- 可行性 = **CONDITIONAL YES**，两条硬前置：(a) 旧端须用**未含 R3 守卫的 1.3.5 构建**（非 1.3.4 装机包）；(b) 须**手工老化** `fluxing-rime-backup\profile.tar.gz` 的 mtime。配方已写死在 `maintenance\BG-G35-conditional-feasibility.md`。
- **BG 遗留（需两台机器 + 用户在场）**：双机 AC 7 条、restore 端到端、输入延迟两组对照、G-3/G-5 真跑、以及 **`kIdSyncStatus`(1017) 不可观测待拍板**（见 `docs/archive/handoffs/NOTES-sync-status-not-observable.md`，该文件已在 `e85259c` 内）。
- BG 的取证脚本与报告（`scripts/verify/orphan-gate-g35-export-mtime.ps1` 等）**已入库** = `e85259c test(ci)`。

## 3. c2d 根因（代码 + 日志两侧收敛，实测坐实）

**收起逻辑原先只挂在父窗 `WM_PARENTNOTIFY` 上**，而该通知**只在鼠标消息被投递给某个子控件时**由系统补发给父窗：

- 点在 **EDIT / BUTTON / LISTBOX** 等吃鼠标的子控件 ⇒ 有 `WM_PARENTNOTIFY`（`wParam=513`）⇒ 收起 ✅
- 点在 **STATIC 标签**（如 `1006 数据同步状态`，`WM_NCHITTEST` 返回 `HTTRANSPARENT`）或**客户区空白** ⇒ 消息直接落到父窗，只是普通 **`WM_LBUTTONDOWN`**，**没有** `WM_PARENTNOTIFY`；且 STATIC **不吃焦点** ⇒ 邮箱 EDIT 也无 `EN_KILLFOCUS` ⇒ **两条收起路径全不触发** ❌

**否证的假设（勿再走）**：
- 「下拉列表遮挡判靶」—— 列表 rect `y=58..81`，判靶点击在 `520,434`，`FG_OK=True`（前台断言通过）⇒ 几何上根本不遮挡。
- 「合成点击没送达」—— 同一轮 `c2a/c2b/c2c` 全 True 且 `all_clicks_delivered=True`，送达无问题。
- 另：**第 5 轮的 `c2d=True` 属空测**（重弹已失败、列表早已隐藏，判据无判别力）⇒ 当时登记的「已修」不成立；权威基线是第 7 轮 FAIL。

## 4. 修法（保守、可逆）

`WeaselServer/Cloud/CloudAccountDialog.cpp`（CF 后行号）：

1. **新增 `case WM_LBUTTONDOWN`**（整块 **L1753–1771**）：父窗收到直接左键按下时，若命中的是邮箱框角色（`RoleOf(hwnd)==Role::kAccount`）之外的情形，记 `LogAccountDlg(L"c2d parent lbuttondown hide")`（**L1766**）后 `HideSuggestList(hwnd)` 并 `return 0`。与既有 `WM_PARENTNOTIFY` 分支**并联**，两条路径都收起。
2. `WM_PARENTNOTIFY`（**L1772** 起）三条分支各加**判别性留证日志**：down 分支 `c2d parentnotify down evt=<mouse_event> id=<id>`（**L1786**）、else 收起分支 `c2d parentnotify hide id=<id>`（**L1798**）——用 `std::to_wstring`（`<string>` 已在 include 列）。
3. **不动**：布局表 `CloudAccountUi.h`、IPC 协议、`librime/`、版本号、依赖；`HideSuggestList`（定义 **L1246**）对「未创建 / 已隐藏」列表是安全 no-op（仅清 `g_list_mouse_down` + `SW_HIDE`）。

## 5. 门禁与交付链（子会话 `bg_8b4a85c1` 实测，T1–T5 全过）

| 步 | 判据 | 实测 |
|---|---|---|
| T1 构建 | `build.bat release weasel installer` | `BUILD_EXIT=0`、`error C=0 warning C=0` |
| T2 云单测 | `TestFluxingCloud.exe`（**不在 sln**，须显式 msbuild + `/p:SolutionDir=`） | **`RESULT: PASS=614 FAIL=0` / `VERDICT: ALL PASS`**（回环 `PASS=11 FAIL=0`；`exe.mtime 19:22:25 ≥ src 19:21:30` ⇒ `FRESH=True`） |
| T2 L0 | `scripts\gate\L0-static.ps1`（真身，**不是 `scripts\verify\`**） | `checks=15 pass=15 fail=0 skip=0` |
| T2 格式 | clang-format 18.1.8 `--style=file --lines=1753:1771 --lines=1784:1801` | `REPLACEMENTS=0 DRIFT=False` |
| T3 出包 | 载荷 + 包 | 载荷 `output\Win32\WeaselServer.exe` `2790400 B / 20:52:43 / sha16 65565EF293F91236`；包 `output\archives\fluxing-1.3.8-installer.exe` `42989665 B / 20:56:01 / sha16 3532922B872B3C7C`；**UTF-16LE** 令牌 `TOK_LBD/TOK_PN/TOK_PN_HIDE=True` |
| T4 装机 | `INSTALL_EXIT=0`（60 s） | 装机件 `2788864 B/20:21:06/1A60B28CD714F31F` → `2790400 B/20:52:42/65565EF293F91236` ⇒ **`SHA_MATCH_BUILD_VS_INSTALLED=True`** |
| T5 真机 | 两轮复验 + 重登安全网 | 见 §6 |

- 服务端唯一 **pid 479804**（start 20:57:58，ver 1.3.8.0）、`SRV_COUNT=1`；本机自然 DPI **168（175%）**；未 `/quit`、未杀宿主、未改注册表；账号态已复原为**已登录**（`RELOGIN_OK_AFTER_SEC=4 / 5`、`A4_RESTORE_VERDICT=PASS`、`SET_MATCH=True`）。

## 6. 真机复验（权威判据 = 第 8 / 9 轮）

**第 8 轮** `reports/fluxing-bh-ui/run-uicheck8.log`（**未修改探针**，c2d 权威复验）：
`C2_VERDICT=PASS c2a_open=True c2b_pick=True c2c_reopen=True c2d_hide_on_other=True c2d_judged=True all_clicks_delivered=True` ⇒ **c2d 已修**
同轮 `C3_VERDICT=NOT_JUDGED c3_setup=False`、`BH_VERDICT=FAIL EXIT=4`。

**第 9 轮** `run-uicheck9.log`（仅改探针取样判据，见 §7）：`C2_VERDICT=PASS`、`C3_VERDICT=PASS c3_setup=True`、**`BH_VERDICT=PASS EXIT=0`**。

**需求 ③ 实质量值（两轮逐字一致）**：邮箱 `CENTER_DELTA=1.5`、密码 `CENTER_DELTA=1.5`、正控制 1003 `C3_POSCTL_OK=True`、`c3a_has_ink / c3b_centered / c3c_not_clipped = True`。
**需求 ①**：`NO_LEGACY_HISTORY_WIDGET=True`、`C1_SAVE_ACCOUNT check_state=1`、文案 sha16 `6A48109401C0EB6D`。
**需求 ②**：`c2a_open/c2b_pick/c2c_reopen/c2d_hide_on_other` 全 True。

**因果证据到手**（`%TEMP%\fluxing-profile-notify.log`，切片 319772→320372→320886 B）—— 两条收起分支各被**独立命中**：
- 子控件点击：`c2d parentnotify down evt=513 id=0` + `c2d parentnotify hide id=0`
- **c2d 判靶那一击（点 STATIC `1006`）走新分支**：`[20:58:41.385][acctdlg] c2d parent lbuttondown hide`（第 9 轮另有 21:03:20.020 / 21:03:21.454 两条）

## 7. 🔴 并发还原事故（本批第二大认知）

- 我在本会话早先写的 **`handoff-2026-11-21-batch-bg-verify-g35-delegated.md` + 其 prompt 文件已从盘上消失**；`CLAUDE.md` 里我加的 BG 指针行、以及我给 **BH handoff 追加的 §4.5–4.7 与更正段**均被**外部还原抹掉**（BH handoff 现为 20:33 版本 / 12291 字符，含第 7 轮 FAIL 但不含第 8/9 轮）；`git status` 里 AM 批次曾 staged 的 `.specify/specs/100-cloud-sync/spec.md` 与 `docs/adr/0063-*.md` 也回到了未改动状态。
- 证据链 = 「同轮 `Test-Path=True` + 字节回读」当时为真 ⇒ 之后被替换；BH handoff 的 mtime **早于**我的编辑时刻 ⇒ 典型的 **`Copy-Item` 还原保留源 mtime** 形态（本项目既有坑）。
- **存活的部分**：代码补丁（`c2d` 三处锚点在盘、1996 行）、`probe-bh.ps1`（37937 B / 21:03:01）、`probe-bh.ps1.bak-pass8`（36679 B / 20:17:00）、`run-uicheck7/8/9.log`、全部出包/装机/门禁日志。
- **对策（已执行）**：本批文档改用**不与并行会话争抢的新文件名**（本文件），并**立刻把代码提交入库**（提交后外部还原不再能悄悄抹掉判据）。**不要**再回写 BH handoff / CLAUDE.md 的既有段落去和另一会话抢同一文件；BH handoff 里「已提交 `e85259c`」的错误由本文件 §8 更正承担。

## 8. 🔴 提交实况与一条错误登记的更正

- **更正**：BH handoff（及本会话早先的转述）登记的「**BH 三条需求代码已提交 `e85259c`**」**为假**。`e85259c` = `test(ci): 批次 BG · G-3/G-5 维护闸门造态能力与取证脚本`，**不含 `WeaselServer/` 任何文件**。硬证据：`git log -S 'ShowSuggestIfAny' -- WeaselServer/Cloud/CloudAccountDialog.cpp` **全历史 0 命中**；`git show HEAD:WeaselServer/Cloud/CloudAccountDialog.cpp` 内 `ShowSuggestIfAny` / `c2d parent` **均不存在**；`git show HEAD:...h` 内 `kIdAccountSuggest` **不存在**。（上述三条判据取自**当时的** HEAD = `e85259c`；下方「实况」落地后已全部翻转，勿按旧判据复核。）
- ⇒ **BH 三需求 + 本批 c2d 修复此前整体只存在于工作区**，未提交 diffstat（`git diff --numstat HEAD`）= `.cpp +275/−32`、`.h +28/−12`、`test +43/−3`。

### 提交实况（2026-11-21 21:23 落地，同轮三条判据一起取；commit B 随后 amend 一次以并入本文件的更正段 ⇒ 文档提交的最终哈希 ≠ 下面写的 `7c70d68`，以 `git log -1 --format=%H` 为准）

- **commit A** = `3450472 feat(fluxing): 云端账号页三项 UI 需求 + 已保存账号下拉点他处收起` ⇒ **3 files changed, 346 insertions(+), 47 deletions(-)**：`WeaselServer/Cloud/CloudAccountDialog.cpp` +307、`WeaselServer/Cloud/CloudAccountUi.h` +40、`test/TestFluxingCloud/TestFluxingCloud.cpp` +46。BH 三需求与 c2d 补丁落在**同一份未提交 diff 里无法单独切出** ⇒ 合为一个 `feat(fluxing)`（提交范围属工程判断，按记忆 2026-10-14 自行裁定并汇报，不再发问卷）。
- **commit B** = `7c70d68 docs(fluxing): 批次 BH/BI 交接文档、接力 prompt 与真机证据` ⇒ **25 files changed, 1440 insertions(+), 1 deletion(-)**：`CLAUDE.md` 指针 + BH/BI 两份 handoff + BH/BI 两份接力 prompt + `reports/fluxing-bh-ui/`（10 张 PNG + `probe-bh.ps1` 643 行 + 9 份 `ui-*-UiCheck/SelfTest.txt`）。
- **三条判据实测**：`git cat-file -t` 两者均 `commit`；`git show --stat` 如上；`git grep -F 'c2d parent' HEAD` = **3 命中**；HEAD 内 `case WM_LBUTTONDOWN:` True、`c2d parentnotify hide id=` True、`ShowSuggestIfAny` 7 处、`保存账号（下次启动自动登录）` 在 `:932`、`kIdEmailLabel` cpp 2 / .h 4。**未 push**：`git status -sb` = `## feat/100-cloud-sync...kizemo/feat/100-cloud-sync [ahead 3]`。
- **⚠️ 更正本节早先两处不精确（本轮实测）** = ① 需求③ 的实现符号**不是** `CenterV(...)`：HEAD 内 `CenterV` **0 命中**，`CloudAccountUi.h:118` 那段声明是**注释掉的** ⇒ 真实现 = `CloudAccountDialog.cpp:1202` 的 `const int line_px = tm.tmHeight + tm.tmExternalLeading;`；② `c1_save_account` / `save account=checked` **不是代码字面量**（HEAD 源码 0 命中）⇒ 它们是**探针输出字段名**；入库判据以代码里的中文文案 `保存账号（下次启动自动登录）` + 盘上探针字段 `C1_SAVE_ACCOUNT check_state=1` 为准。
- **`.gitignore` 挡住 18 份判据原文**：`reports/fluxing-bh-ui/run-uicheck.log … run-uicheck9.log`、`run-relogin.log … run-relogin9.log` 均未入库 ⇒ 第 9 轮权威原文 `run-uicheck9.log`（`BH_VERDICT=PASS / EXIT=0`）**只活在盘上**。已入库的同内容判据 = `ui-20261121-205842-UiCheck.txt`（55 行）与 `ui-20261121-210322-UiCheck.txt`（56 行，实测含 `c2d_hide_on_other=True` + `C3_VERDICT=PASS` + `BH_VERDICT=PASS`）。要让 `.log` 原文也入库需 `git add -f`，**待用户定**。
- **有意未入库**：`librime` 脏指针、`release/`、`docs/design/quickpanel-v6/design-demos/{a-benchmark,b-craft,c-roulette}.html`、`scripts/verify/{diag-host-window-enum,orphan-probe-host,verify-orphan-session-reclaim}.ps1`（疑并行会话产物）、`reports/fluxing-bh-ui/probe-bh.ps1.bak-pass8`（探针改前留档）。

## 9. 本批新工具坑（务必带进下一会话）

1. **`Select-String -SimpleMatch` 的 0 命中不得下阴性结论**：我用 `git diff HEAD -- file | Select-String 'c2d parent'` 得 0 命中，误推出「HEAD 已含 BH 代码、补丁不在 diff 里」——两个结论都错。判存在性用 **`git grep <串> HEAD`** / **`git log -S <串> -- <path>`** / **`(Get-Content -Raw -Encoding UTF8).Contains(...)`**。
2. **`ES_PASSWORD` 编辑框跨进程读不回文本**：`WM_GETTEXT` 对密码框恒返空串 ⇒ 「种子文本读回等长」这类判据对**任何构建永不成立**（第 1/3/5/6/8 轮 `pwd_after_hide_len=0` 由此而来，属**探针缺陷不是产品缺陷**）。修法 = `EM_GETTEXTLEN(0x00C1)` + **同帧客户区裁剪 `Crop-Ink` 墨迹行数**双通道，**两向都成立才放行**（保留判别性，不凑绿）。仅改探针 `probe-bh.ps1` L553–571（+13 行），产品码不动；改前留档 `probe-bh.ps1.bak-pass8`。
3. **`probe-a4-a5.ps1 -Mode Relogin` 尾部既有瑕疵**「不能为空数组建立索引」⇒ `RELOGIN*_EXIT=1`；**判恢复只看** `RELOGIN_OK_AFTER_SEC` / `A4_RESTORE_VERDICT=PASS` 两行，不看 exit code。**未越界改他批探针**，待单独修。
4. **会登出的探针必须与同一 bash 调用里的 `-Mode Relogin` 安全网无条件串联**（本批两轮各触发 1 次重登，均成功）。
5. **包内含码只认载荷 exe 的令牌，且必须按 UTF-16LE 搜**（源码是 `L"..."`，ASCII 搜必假红）；NSIS installer 是压缩容器，只能证真不能证伪。
6. **版本号 1.3.8.0 无判别性**（同版重装不改版本）⇒ 装机类判定一律 sha16 比对。
7. **并发写同一文件的工作区**：写文档前 `git status` + mtime 复核；写完的**同号文件**可能在几分钟后被另一会话用 `Copy-Item` 还原抹掉（mtime 反而变旧）⇒ 判据文档应尽快随代码入库。

## 10. 未闭环 / 待办（下一会话按序）

1. **push**：`feat/100-cloud-sync` 领先 kizemo，本批提交**未推送**；推送前需用户授权（代理/直连可用性会随网络反转 ⇒ 两条通道各发一次 `Invoke-WebRequest -Method Head` 再选）。
2. **BH handoff 的回写冲突**：待另一会话收手后，把 §4.5–4.7 与 §8 的更正合并进 `handoff-2026-11-21-batch-bh-cloud-account-ui-3req.md`（现盘上是 20:33 版、缺第 8/9 轮），并在 `CLAUDE.md` 补 BH/BG/BI 三条指针行（当前盘上只有 BF）。
3. **BG 遗留**（需两台机器 + 用户在场）：双机 AC 7 条、restore 端到端、输入延迟两组对照、G-3/G-5 真跑（配方 `maintenance\BG-G35-conditional-feasibility.md`）、`kIdSyncStatus`(1017) 不可观测待拍板。
4. **`probe-a4-a5.ps1` 尾部索引瑕疵**单独修（BF 批次资产，别混进本批）。
5. **遗留脏文件** `tmp\bh-c2d-fmt-check.cpp`（83767 B，clang-format 对照副本）**删不掉**（本机 `Remove-Item` 被硬策略整条拒、`rm --` 报 `requires the targets-only trash marker`）⇒ 需用户手动处置。
6. **勿重跑**：A-1~A-5、B-1~B-5、`/verify-client-pid`、175%/200% DPI 切档（已拍板）、c2d 复验（已 PASS）、c3 量值（已 PASS）。

## 11. 证据 / 产物路径

```
代码        WeaselServer/Cloud/CloudAccountDialog.cpp  (1996 行；c2d 锚点 L1766 / L1786 / L1798)
            WeaselServer/Cloud/CloudAccountUi.h        (kIdAccountSuggest 等)
            test/TestFluxingCloud/TestFluxingCloud.cpp
真机判据    reports/fluxing-bh-ui/{run-uicheck7.log,run-uicheck8.log,run-uicheck9.log,
              run-relogin8.log,run-relogin9.log,ui-20261121-205842-UiCheck.txt,ui-20261121-210322-UiCheck.txt}
截图        reports/fluxing-bh-ui/{evidence-ui-loggedout.png,evidence-c1-save-account.png,evidence-c3-seeded.png,
              evidence-c3-{email,pwd}-{label,edit}.png}
探针        reports/fluxing-bh-ui/probe-bh.ps1 (643 行/37937 B) + probe-bh.ps1.bak-pass8 (630 行/36679 B)
构建/门禁   tmp/{bh-c2d-build.log,bh-c2d-build2.log,bh-c2d-build2-exit.txt,bh-c2d-testbuild.log,
              bh-c2d-testrun.log,bh-c2d-l0.log,bh-c2d-install.cmd,bh-c2d-install.log,
              bh-c2d-log-before.txt,bh-c2d-log-before9.txt,bh-c2d-probe-patch.ps1,bh-c2d-fmt-check.cpp}
包          output/archives/fluxing-1.3.8-installer.exe (42989665 B / sha16 3532922B872B3C7C)
载荷        output/Win32/WeaselServer.exe (2790400 B / sha16 65565EF293F91236)
子会话全文  task_output bg_8b4a85c1-1dae-4c7e-9392-95426ed570c5
上一轮快照  E:\Users\Duanyi\.minimax\.minimax\v2\sessions\2026\11\21\09-12-35-135-session_bXZzXzE0ZmYyN2IyM2MwMzQzYzdhMDE4ZmU4ZGIzYWYxYTg3\snapshots
```

**装机版 1.3.8.0（未 bump）· 服务端 pid 479804 · 本机 DPI 168 · 已登录态已复原**
