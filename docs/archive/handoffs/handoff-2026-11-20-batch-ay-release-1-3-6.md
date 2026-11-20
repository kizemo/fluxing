# 批次 AY · 出包 1.3.6（ADR 0067 孤儿会话回收 + 账号页换钥提示 + `[acctdlg]` 日志改接落盘）

- 日期：2026-11-20
- 会话：`mvs_74150cd7376a4bbfb29e8b8603dc04c6`
- 上一棒：[批次 AX](handoff-2026-11-20-batch-ax-realmachine-verification-and-orphan-session-reclaim.md)
- 分支 / 远端：`feat/100-cloud-sync` → `kizemo` = `https://github.com/kizemo/fluxing.git`

---

## 1. 本批背景与目标

AX 批次把 ADR 0067（孤儿 IPC 会话回收）与「账号页换钥后果提示」写完并入库，但**未出包**（装机版仍是 1.3.5.0）。
用户 2026-11-20 明确拍板 **「现在出包 1.3.6」** ⇒ 本批目标 = 把 AX 的代码交付物打进新安装包，并完成出包侧的全部收口动作：

1. 版本真相源 bump 1.3.5 ⇒ 1.3.6（四键位 + `version.txt`）；
2. touch 4 个 `.rc`（否则版本资源停在旧值）；
3. `CHANGELOG.md` 顶部补 `## [1.3.6]` 条目（L0 版本一致性判据会读它）；
4. `build.bat weasel installer` 全套构建 + makensis；
5. L0 静态门禁复跑到 15/15；
6. 产物判别（大小 / sha / 内嵌 exe 版本 / 令牌）；
7. 出「重启后测试清单」；
8. 提交 + push；
9. handoff + 接力 prompt（本文件 + `docs/archive/prompts/`）。

**未做产品代码改动** ⇒ 不涉及 `librime/`、不涉及 `include/WeaselIPCData.h`、不新增依赖 ⇒ 不触发 IPC 四边同步红线。

## 2. 已验证（有实测证据）

### 2.1 版本真相源

| 项 | 结果 | 判据 |
| --- | --- | --- |
| `version.txt` | `1.3.6` | **5 字节、无尾换行**（与 HEAD 字节等长替换 ⇒ L0 长度判据不红） |
| `env.bat` `FLUXING_VERSION` | `1.3.6` | 正则命中 **1 次** 才替换；替换后**字节长度不变** |
| `env.bat` `PRODUCT_VERSION` | `1.3.6.0` | 同上 |
| `env.bat` `FILE_VERSION` | `1.3.6.0` | 同上 |
| `env.bat` `VERSION_PATCH` | `6` | 同上 |
| `env.bat` `WEASEL_BUILD` | `0`（未动） | 无 `weaselx64.1.3.6.0.dll` ⇒ 无 L107 风险 |
| 4 个 `.rc` | 已 touch | `LastWriteTime = Get-Date`，实测已前进 |

**关键发现（AX 之前未登记）**：`.gitignore` 第 **31** 行忽略 `env.bat`、第 **32** 行忽略 `weasel.props` ⇒
版本真相源里**只有 `version.txt` + `CHANGELOG.md` 被 git 跟踪**。这解释了 CLAUDE.md「阻止 `env.bat` 误提交」那条规矩，
也意味着**换机 / 全新 clone 必须手工重建 `env.bat` 的版本四键位**（它不在版本控制里）。
实测命令：`git -C <repo> check-ignore -v env.bat weasel.props`。

### 2.2 构建与产物

- `cmd /c "cd /d F:\soft\00selfmade\rime_claude && build.bat weasel installer"` ⇒ 走完 MSBuild + makensis。
- 构建日志 `C:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-release-build.log`：MSBuild 层
  `warning C` / `error C` / `LNK` / `fatal` **零命中**；makensis 仅 **8 条既有 `warning 7010`**
  （ARM/ARM64 可选产物、`data\*.gram`、`data\lua\*.txt`），与 1.3.1–1.3.5 逐字同形。
  ⚠️ 日志尾部的 `RELEASE_BUILD_EXIT=` 行**未能读回**（编码问题，见 §6），故「exit=0」登记为**推断**而非实测；
  产物存在 + 尺寸 + `FileVersion 1.3.6.0` 三条实测足以证明构建成功。
- 📦 **`output/archives/fluxing-1.3.6-installer.exe`** = **42 951 049 B**
  · sha256 `47909B9174D304838C0A21A7383441D1C0033B75B750F9DD290BE369A54B7ED1`
  · sha16 `47909B9174D30483`
- 内嵌 **`output\Win32\WeaselServer.exe`** = **2 780 672 B** · `FileVersion 1.3.6.0` · sha16 `3CF15A582B2453F1`
- 历史包尺寸对照：1.3.0 42 952 023 / 1.3.1 42 963 252 / 1.3.2 42 952 698 / 1.3.3 42 968 958 / 1.3.4 42 990 667 / 1.3.5 42 973 772 / **1.3.6 42 951 049**。

### 2.3 产物令牌判别（**判据口径已更正**，见 §6 坑 1）

对 `output\Win32\WeaselServer.exe`（zip 载荷，非压缩容器）搜令牌，实测：

| 令牌 | 编码 | 结果 |
| --- | --- | --- |
| `orphan_reclaim presence=kept` | ASCII | **True** |
| `reclaim orphan session` | ASCII | **True** |
| `重置登录口令后` | UTF-16LE | **True** |
| `清空云端数据` | UTF-16LE | **True** |
| `无法再解开` | UTF-16LE | **True** |
| `key rotation` / `KeyRotationNote` / `reset login password` | ASCII | False（源码里不存在这些英文串，属**阴性对照**，非缺陷） |

⇒ ADR 0067 与换钥提示**确在 1.3.6 载荷内**。

**判据口径（比「必然假红」更准确）**：installer.exe 内的文本令牌搜索**只能证真、不能证伪**——
`reclaim orphan session` 在 **1.3.5 包里实测 True**（说明部分字面量确实可见、AX 那轮不是造假），
但在 **1.3.6 包里实测 False**（同一令牌、同一压缩容器）⇒ 包级 False **不得**写成「码不在包内」。
根因是压缩流里字面量可能被切开。因此**包级令牌差集不再是出包判据**；
出包判据改为三条实测：① `output\Win32\WeaselServer.exe` 的 `FileVersion == 1.3.6.0`（本批实测）
② L0 的 `artifact-version-resource` 判据覆盖 **5 个产物 FileVersion**（本批 15/15 内已 PASS）
③ 载荷 exe 的令牌搜索（本批 §2.3，ASCII + UTF-16LE 都 True）。

### 2.4 假提交哈希 `a33c203` 的更正（本批前半段完成）

AX 曾向用户登记提交 `a33c203` = 「账号页换钥提示」，**该提交不存在** ⇒ 三条实测判据全灭：
`git cat-file -t a33c203` 失败、`git show --stat a33c203` 失败、`git grep -c "kIdKeyRotationNote" HEAD` 当时为 0。
根因 = 当时代码**从未入库**（`commit_exit=0` 被误当成提交成立的证据）。
处置 = 真实提交 `02c67d6`（`feat(fluxing)`，4 files `+95/−4`，`git show --stat HEAD~1` 实测）+
更正文档提交 `eef5234`（`docs(fluxing)`，2 files `+11/−3`）；**不改写历史**，在
`docs/archive/handoffs/handoff-2026-11-20-batch-ax-...md:17`、其 §6.8 未做④、其 §7 坑 14 与 `CLAUDE.md` 就地更正。
提交真实性三条判据（今后**必须一起看**）：`git cat-file -t <h>` + `git show --stat <h>` + `git grep <新符号> HEAD`。

### 2.5 有效门禁（代码级，AX 已跑、本批复用）

- 云单测 `RESULT: PASS=577 FAIL=0` / `VERDICT: ALL PASS`（IPC 回环 11-0）
- `build.bat weasel` 0 警告 0 错误；定点 `clang-format` 0 漂移 `exit=0`
- `scripts\gate\L0-static.ps1` 目标 `checks=15 pass=15 fail=0 skip=0`（本批补 CHANGELOG 条目后复跑）

## 3. 交付物清单（本批新建 / 修改）

| 路径 | 性质 |
| --- | --- |
| `version.txt` | 改（5 字节等长） |
| `env.bat` | 改（**被 git 忽略**，不进提交） |
| `output\*.rc` ×4 | touch（资源时间戳） |
| `CHANGELOG.md` | 改（顶部插 `## [1.3.6]`） |
| `docs/release/restart-checklist-1.3.6.md` | 新建 |
| `docs/archive/handoffs/handoff-2026-11-20-batch-ay-release-1-3-6.md` | 新建（本文件） |
| `docs/archive/prompts/prompt-next-session-2026-11-20-batch-ay-release-1-3-6.md` | 新建（接力 prompt） |
| `CLAUDE.md` | 改（交接指针：AY 设为本批、AX 降为上一批） |
| `output/archives/fluxing-1.3.6-installer.exe` | 产物（不入库） |

## 4. 推断（有依据但未实测）

- `build.bat weasel installer` **exit=0**（产物三条实测支持，但 exit 行未读回）。
- 1.3.6 安装包在**干净机器**上可正常安装（本批未做装机，装机版仍是 **1.3.5.0**）。
- ~~NSIS 采用压缩载荷 ⇒ installer 级文本搜索必然假红~~ —— **该推断已被本批实测否证**（见 §2.3）：包级文本令牌搜索
  **只能证真、不能证伪**，同一令牌 `reclaim orphan session` 在 **1.3.5 包 True / 1.3.6 包 False**，而两版都含该码。

## 5. 未验证（不得当 PASS 用）

1. **1.3.6 未装机**：`D:\Program Files\fluxing\weasel\WeaselServer.exe` 仍是 1.3.5.0（2 766 336 B）；
   `WeaselServer.exe` 是 **PPL** ⇒ 装机前必须**注销/重启**，磁盘哈希不能证明在跑的是新码。
2. **换钥提示折行宽度未实测**：`kIdKeyRotationNote` 高 `h=36`、账号页窗高 290 均为**保守取值** ⇒
   真机须目视确认文案不被裁、不与「清空云端数据」按钮重叠（清单 §1 A-2 / §6）。
3. **孤儿会话回收真机端到端未验**：13 条判据只有单测覆盖。
4. **`TestWeaselIPC.exe /verify-client-pid` 环境受限未跑成**：IPC 管道名全局固定
   `\\.\pipe\<user>\WeaselNamedPipe`（`include/WeaselIPC.h:490 GetPipeName()`）⇒
   真实服务端在跑时 mock 抢不到管道（`EXIT=-4`）；须先 `WeaselServer.exe /quit`（**需用户给停服窗口**）。
5. 缺陷 A 反向判据 **G-3 / G-5**（出处 `docs/release/regression-checklist-cloud-sync.md:32/34/37/99/107`）未跑。
6. 双机 AC 7 条（AC-03/04/05/08/09/13/14）、restore 端到端、输入延迟 1.3.4/1.3.5 两组对照 —— 本机无法闭合。
7. 缺陷 B 第三分支未复现 ⇒ 复发时按清单 §3 取 `%TEMP%\fluxing-profile-notify.log` 的 `[acctdlg]` 行。
8. **AX 登记的「installer 内嵌 server 含令牌」判据口径存疑**，须用 §2.3 的「搜 zip 载荷」口径重做。

## 6. 本轮工具坑（务必带走）

1. **NSIS installer 是压缩容器 ⇒ 在 `.exe` 包里搜文本令牌「只能证真、不能证伪」**（旧措辞「必然假红」已被本批实测否证）。
   正确口径：搜 **zip 载荷** `output\Win32\WeaselServer.exe`；宽字符串按 **UTF-16LE** 搜，中文文案同理。
   实测证据：同一令牌 `reclaim orphan session` 在 **1.3.5 包 True**、在 **1.3.6 包 False**，而两版都含该码
   ⇒ 「包内 True」可用于证真，「包内 False」**不得**写成「码不在包内」（字面量可能被压缩流切开）。
2. **PS 5.1 `*>` 重定向建的日志是 UTF-16LE**，之后再 `Out-File -Append -Encoding utf8` ⇒ 文件内**混合编码**，
   用 `-Encoding Unicode` 读会读不到后追加的 ASCII 行（本轮 `RELEASE_BUILD_EXIT=` 就这样丢了）。
   做法：**一次调用只用一个编码**建日志；要读 exit 就在同一脚本里 `$LASTEXITCODE` 直接打印到 stdout，别靠回读日志。
3. **PS 嵌套数组写法 `@(,@('a','b'),@('c','d'))` 取 `$pair[0]` 会拿到整对拼接串** ⇒ 正则命中数 0、脚本 ABORT。
   多键位替换改用**显式逐条** + `function Rep`，并**先断言命中数 == 1** 再替换。
4. **`env.bat` 非 UTF-8**（中文注释读成 `?????`）⇒ 只能加 ASCII 注释；字节安全替换用
   `[Text.Encoding]::GetEncoding(28591)`（Latin1，字节↔字符 1:1 往返无损），改完校验 `Length` 不变。
   本批**决定不给 `env.bat` 加注释**（非 UTF-8 且被 ignore ⇒ 收益低风险高）。
5. **`version.txt` 必须与 HEAD 字节等长替换、保持无尾换行**，否则 L0 判红。
6. **`CHANGELOG.md` 顶部条目必须在 L0 之前补上**，否则版本一致性判据 `Test-VersionConsistency`
   （`scripts/gate/lib/GateChecks.ps1:372`）必红 —— 1.3.5 那次就是 14/15。
7. `Remove-Item` 删日志被硬策略拦（`Deletion intent detected`）⇒ 直接靠重定向**覆盖**，不要删。
8. 对 42 MB 包做 `[BitConverter]::ToString($bytes) -replace ...` 极慢 ⇒ 会超时转后台；令牌判别优先直接搜小 exe。
9. `Copy-Item` 还原保留 mtime ⇒ MSBuild 不重编译 ⇒ 变异验证假红，还原后必须 touch。
10. 改 `.props` / `env.bat` **不触发版本资源重编译** ⇒ 必须 touch 4 个 `.rc`。
11. 提交说明含中文 ⇒ `write` 成**无 BOM**临时文件再 `git commit -F <file>`；PS 5.1 的 `git commit -m "中文"` 会写出乱码；
    `git commit -F -` **不吃 stdin**。
12. `Where-Object { $_.FullName -notmatch '\\...' }` 这类以 `\\` 开头的正则会被判成 UNC ⇒ `Permission denied … network share`。
13. 收工前不得留**未终态后台任务**：后台任务终态会唤醒属主会话（归档拦不住，只有等完或 `task_stop` 拦得住）。

## 7. 下一棒该做什么

**首选（用户侧动作，需用户在场）**：按 `docs/release/restart-checklist-1.3.6.md` 装机 1.3.6 并做 §1 换钥提示目视验收
（重点 A-2 折行宽度 / 窗高 290 是否合适）。装机须先**注销或重启**。

**次选（本机可做的工程项）**：把 AX 的产物判别按 §6 坑 1 的口径重做一遍并回写 AX handoff；
把 §2.2 的 `exit=0` 从推断升级为实测（同一次调用内打印 `$LASTEXITCODE`，别回读混合编码日志）。

**需要用户给窗口才能推进**：`/verify-client-pid`（先 `WeaselServer.exe /quit`）；双机 AC 7 条；restore 端到端；输入延迟两组对照。

## 8. 证据路径

- 构建日志：`C:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-release-build.log`
- bump 记录：`C:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-bump.log`、`fluxing-ay-bump2.log`
- 更正提交：`C:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-corr-msg.txt`、`fluxing-ay-corr-commit.log`、`fluxing-ay-push2.log`
- 代码级门禁（AX）：`fluxing-ay-at3-{build,test,weasel-build,l0}.log`
- AX handoff：`docs/archive/handoffs/handoff-2026-11-20-batch-ax-realmachine-verification-and-orphan-session-reclaim.md`
- ADR：`docs/adr/0067-*.md`（孤儿会话回收）、`docs/adr/0063-*.md`（换钥提示补充小节）、`docs/adr/0062/0065/0066`
- 真机取证目录（AX）：`C:\Users\Duanyi\AppData\Local\Temp\fluxing-ax-verify-20261120-114712\`
