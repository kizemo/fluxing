# 交接卡 · 火流猩 1.1.9 安装锁修复 + 版本资源修复 + 「功能丢失 / bug 复现」系统性预防机制

> 状态：**全部任务已闭环**（2026-11-03）。1.1.9 已发布、用户已装机验收、预防机制（回归资产 + 分层门禁 + ADR + CI）已落地并实跑全绿。
> 交棒目的：下一会话接手 §4 的剩余项，不要把已完成的再做一遍。

## 1. 背景与目标

用户两条原始指令：

1. **修复 1.1.8 安装报错**：安装时弹「无法打开要写入的文件：`D:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe`」，导致升级中断。
2. **系统性预防「功能丢失 / bug 复现」**：开发过程中反复出现"之前做好的功能丢失、修过的 bug 复现"，要求复盘根因并固化机制。

衍生指令（每条用户消息都重复）：

3. 更新完整 handoff 文档 + 一句话 prompt，prompt 只给全路径引导、**不复述 handoff 内容**。

## 2. 本轮已完成

### 2.1 安装报错根因坐实（运行时取证，非读码推测）

- 目标 exe 被**独占锁**（`File.Open(ReadWrite,None)` 抛 `being used by another process`），**不是**权限问题、不是路径问题。
- 占用源：**20 个** `FluxingPhrasesDialog.exe` 残留进程（隐藏孤儿，约 350 MB 工作集），命令行 `--pipe=\\.\pipe\FluxingPhrasesDialog\<serverpid>`；19 个的父进程已消失，仅 1 个父 `WeaselServer.exe` pid 84392 仍活。
- 产品侧成因：Esc 只 `Hide()` 不退出（`WeaselUI/PhrasesDialog.cpp:967`）；`PhrasesDialogIPC::TerminatePhrasesDialog()` 只在 server **优雅**退出路径被调用（`WeaselServer/PhrasesDialogIPC.cpp:112`）⇒ server 被强杀/崩溃 ⇒ 子进程成孤儿，永远不退出。
- 安装器侧成因：`output/install.nsi` 的清理只覆盖 `WeaselServer.exe`；`taskkill /T` 只沿**活着的**子进程树走 ⇒ 打不到父已死的孤儿。

### 2.2 根治修复（`output/install.nsi`，提交 `2eb8faa`）

- **4 处**按**映像名**杀自家 UI 进程（**不带 `/T`**）：实际行号 218 / 460 / 615 / 1327，字面量
  `nsExec::ExecToStack 'taskkill /F /IM FluxingPhrasesDialog.exe'`。
- 同源注释 4 处：`Keep this name in sync with fluxing::OwnUiExeClientName`（与 `include/WeaselIPC.h` 的 `OwnUiClientExeName()` 绑定）。
- 附带修 L09：该文件此前有 46 处裸 LF ⇒ 归一化为 UTF-8 BOM + 100% CRLF（68193 B，crlf=1393，bare_lf=0）。

### 2.3 PE 版本资源 `FileVersion` 恒空 → 已修（教训 **L175**，提交 `f101448`）

- 三层取证：PE 内 `VS_FIXEDFILEINFO` 的 `dwFileVersionMS/LS` = 1.1.9.0；**字符串表 `FileVersion` 是空串**；.NET `FileVersionInfo.FileVersion` 读的是字符串表。
- 根因：4 个 rc 文件写 `VALUE "FileVersion", FILE_VERSION`，宏展开成**无引号**的 `1.1.9.0` ⇒ 字符串表为空。
- 修法：**10 处**改为 `VALUE "FileVersion", STRZ(FILE_VERSION)`。行号：`WeaselServer.rc` :116/:230/:324；`WeaselTSF.rc` :159/:171/:183；`WeaselSetup.rc` :321；`WeaselDeployer.rc` :197/:330/:506。
- A/B 实证：同版本同大小（2,287,104 B）两二进制，修复前 `.VersionInfo.FileVersion=''`，修复后 `'1.1.9.0'`。
- 影响：`AGENTS.md §4.3` 里原本"恒空"的版本核验命令因此恢复了判别力。

### 2.4 1.1.9 构建 + 装机 A/B 验收 + 对外发布（**已闭环，用户已确认**）

- `env.bat`：`FLUXING_VERSION=1.1.9` :92 / `PRODUCT_VERSION=1.1.9.0` :95 / `FILE_VERSION=1.1.9.0` :96 / `VERSION_PATCH=9` :101。
- 产物 `output/archives/fluxing-1.1.9-installer.exe`：**42,797,082 B**，sha256 `8D5DD1A877E7F9FF40DFD87B4BDBD060070A80038184D1397D323FDA8AD6C18E`，md5 `556BB87230FBB9E3C767DDF4796A818B`。
- **A/B v2**（证据 `tmp/install119-ab-v2.txt`）：
  - `OLD: rewrote_target=False, reached_install_tail=True`（旧安装器静默失败，退出码却是 0）
  - `NEW: installed_hash_matches_build=True, orphans_cleared=True, DisplayVersion=1.1.9`
  - `VERDICT=AB PASS`
- **发布**（用户已签字）：推送至 `kizemo/Fluxing`；轻量 tag `v1.1.9` → `778ce35`；Release
  `https://github.com/kizemo/fluxing/releases/tag/v1.1.9` 为 **Latest**（非 draft / 非 prerelease）；
  远端资产 digest `sha256:8d5dd1a877e7f9ff40dfd87b4bdbd060070a80038184d1397d323fda8ad6c18e`、size=42797082 ⇒ 与本地产物逐字节一致。
- **用户装机验收原话**：「已安装，这次安装没有报错。在显示用户短语界面时，键入 esc，快捷设置栏不会消失了」。

### 2.5 「功能丢失 / bug 复现」复盘分析落盘

`docs/analysis/2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`：

- **8 条根因 RC1–RC8**（核心：验收脚本活在 git-ignored 的 `tmp/` 里 ⇒ 验收资产没被当产品资产管理）。
- **7 根支柱 P1–P7**（回归用例库 + 缺陷登记表 + 分层门禁 + 变异测试 + 安装后置校验 + CI 硬约束 + 产品侧加固）。
- 固定验收矩阵（每轮发布必须复现的维度：残留进程 / 孤儿进程 / 独占锁 / 产物哈希 / 版本资源 / DisplayVersion）。

### 2.6 预防机制落地（资产已入库，见 §8.1）

| 资产 | 作用 |
|---|---|
| `version.txt`（5 B，无结尾换行） | 版本号单一真相源（R7），供门禁一致性校验 |
| `test/regression/DEFECTS.yaml` | 缺陷登记表（3 条缺陷 ↔ 3 个用例，双向一致由门禁断言） |
| `test/regression/lib/CaseHarness.ps1` | 用例骨架（Pass/Fail/Skip、结构化输出） |
| `test/regression/lib/PeVersion.ps1` | 直接解析 PE + .NET `VersionInfo` 双口径取版本 |
| `test/regression/probes/ReproEsc.cs` | Esc 不退出短语窗口的复现探针 |
| `test/regression/cases/C0032-phrasesdialog-esc-panel.ps1` | 缺陷 3 的回归用例（`heavy`，需 `FLUXING_ALLOW_UI_AUTOMATION=1`） |
| `test/regression/cases/C0174-installer-orphan-lock.ps1` | 安装器孤儿锁回归（`heavy`，§A 静态段任何机器可跑有齿；§B A/B 段需 `FLUXING_ALLOW_INSTALL_AB=1`） |
| `test/regression/cases/C0175-version-resource.ps1` | 版本资源回归（`fast`） |
| `test/regression/run-cases.ps1` | 用例运行器（全 SKIP 退出码 2，被上层折成 0） |
| `scripts/gate/lib/GateChecks.ps1` | 全部检查函数（24983 B / 450 行） |
| `scripts/gate/L0-static.ps1` | L0 静态门禁（`-Mode source\|artifact\|all`） |
| `scripts/gate/L3-mutation.ps1` | L3 变异门禁（对临时副本做破坏，检查必须翻红） |
| `scripts/gate/run-all.ps1` / `run-all.cmd` | 串跑 L0→L6 |
| `scripts/install/verify-installed.ps1` | 装机后置校验（哈希 / 版本资源 / DisplayVersion / 残留进程 / 独占锁，11517 B / 267 行） |
| `docs/adr/0033-regression-asset-and-staged-gate.md` | 机制决策 ADR |
| `.github/workflows/gate.yml` | CI 硬约束（源码层 L0，不编译安装器） |

### 2.7 门禁实跑结果（全部通过）

```
L0 static gate  mode=all  checks=12 pass=12 fail=0 skip=0        exit=0   （tmp/gate-l0-fix4.log）
L3 RESULT mutations=7 killed=7 survived=0 invalid=0 skipped=0    exit=0
L4 REGRESSION RESULT level=fast cases=1 pass=1 fail=0 skip=0 error=0  exit=0
scripts/install/verify-installed.ps1  ok=True inconclusive=False problems=0  exit=0
```

L0 的 12 项：`version.txt` / `CHANGELOG.md` / `lessons-learned.md` / `install.nsi` / `rc-utf16le` / `installer-ownui-kill` / `version-consistency` / `script-syntax` / `defects-registry` / `nsis-compile` / `guard-unit-test` / `artifact-version-resource`。

L3 的 7 条变异：M01 / M02 / M03 / M04 / M05a / M05b / M06，**全部 KILLED**。

### 2.8 门禁自检自身的 8 个缺陷（**本轮最大收获，务必读完再改门禁**）

门禁第一版跑出 10 PASS / 2 FAIL，故障全在**检查代码自己**身上，逐个坐实并修：

| # | 缺陷 | 根因 | 修法 |
|---|---|---|---|
| 1 | `rc-utf16le` 误报裸 LF | rc 文件确实有裸 LF（既有缺陷，非本轮引入） | `tmp/normalize-rc-eol.py` 码元级补 CR + **语义等价证明**（去 CR 的码元流 == HEAD 同口径），三文件全 `semantic-equal-HEAD=True` |
| 2 | 同上，修完仍误判 | PowerShell `-shl` 把结果截断回左操作数类型 ⇒ `[byte]0x54 -shl 8 = 0`，码元 `0x6B0A` 被读成 `0x000A` | `Test-Utf16LeFileShape` 两处 `-shl` 前加显式 `[int]`；**造坏数据验证仍有牙**（把一处 CR 改成 `0x0052` ⇒ `mutant Ok=False 裸 LF=1 处`） |
| 3 | `run-all` 吞掉全部子进程输出 | `$r = & $Body` 把每行 stdout 收成 `Object[]` ⇒ `[int]$r` 抛 `Object[]→Int32`，输出同时被吞 | `$r = @(& $Body)` + Count 分支；四处调用点追加 `\| ForEach-Object { Write-Host $_ }`（**不用 `Out-Host`**，它绕过 `*>` 重定向） |
| 4 | `guard-unit-test` 抛 `StandardOutputEncoding is only supported when standard output is redirected` | `& $exe 2>&1` 在宿主 stdout 未重定向时非法 | 改 `System.Diagnostics.ProcessStartInfo`（`UseShellExecute=$false` + 重定向 + `ReadToEnd` + `WaitForExit` + `ExitCode`） |
| 5 | `artifact-version-resource` 报"产物不齐" | 构建产物**分两个目录**：`output\` 根放 `WeaselSetup.exe`/`weasel.dll`/`weaselx64.dll`/`FluxingPhrasesDialog.exe`，`output\Win32\` 放 `WeaselServer.exe`/`WeaselDeployer.exe` | `L0-static.ps1` 加双目录解析表（`$searchDirs` + `$resolved`）；`verify-installed.ps1` 加 `Resolve-BuildArtifact` 同口径 |
| 6 | `guard-unit-test` 抛"不是有效应用程序" | makensis `/O` **不是**该 nsi 的覆盖开关（契约是 `!ifdef GUARD_TEST_OUTFILE`）⇒ exe 路径被写成 882 B 的**编译文本**（首 4 字节 `50-72-6F-63` = `Proc`） | 改 `"/DGUARD_TEST_OUTFILE=$exe"`；加两条防御：路径含空白直接报清晰错误；产物必须 `MZ` 头，否则报"输出路径覆盖没生效" |
| 7 | `verify-installed` 哈希假阳性 | 只比普通名 `weaselx64.dll`，而普通名那份是删不掉的陈旧残留（1.1.7.0）；活的是版本化 shim `weaselx64.1.1.9.0.0.dll` | 新增 `Get-InstalledCandidates`（普通名 + `^{stem}\..+\.dll$` 版本化 shim），任一候选哈希等于构建哈希即通过；与版本资源段统一"活体判定"口径 |
| 8 | `guard-unit-test` 报"守卫测例没输出 TOTAL 行（rc=0）" | guard 测例**结果只写文件** `${GUARD_TEST_OUT}\guard_install_path_result.txt`（NSIS `Unicode true` ⇒ **UTF-16LE + BOM**），从不往 stdout 打结论 | 运行前删残留结果文件（防"没运行"被读成"跑通了"）；用 `[System.IO.File]::ReadAllText` 读文件解析 `TOTAL=`；"没产出结果文件"与"结果不符"分开报 |

> 结论（ADR 0033 已记录）：**门禁自己也要有判别性**。写完任何检查，必须造一份坏数据证明它会翻红（缺陷 2 的做法是标准形态）。

## 3. 仍未修的缺陷（本轮坐实，未动手；ADR 0033 已记为"未做项 + 触发条件"）

1. **短语窗口在 pipe 断开（server 死）时不自退**：`WeaselUI/PhrasesDialog.cpp:967` Esc 只 `Hide()`，属 L167 既定语义，改它属行为变更，**需新 ADR + 重建 + A/B**。
2. **server 启动时不回收上一代遗留的自家 UI 进程**：同理需 ADR。
3. **静默模式安装器的失败会被吞**：NSIS 的 `File` 失败后退出码仍是 0、旧二进制留存 ⇒ 已由 `scripts/install/verify-installed.ps1` 从外部兜住，但安装器内部仍无兜底。
4. **受保护路径下的安装残留（需用户手动）**：`D:\Program Files\fluxing\weasel\fluxing\`（223 文件 / 231,538,974 B）与 `weaselx64.dll.old.tmp`；已备份到 `D:\Program Files\fluxing\engine-nested-fluxing-bak-20261103`。
5. **`FluxingPhrasesDialog.exe` 完全没有版本资源**（独立 vcxproj，无 .rc）。已在 C0175 注释与 ADR 0033 中**显式排除**，它**不在** `$script:ProductFiles` 里。
6. **交互（非静默）安装器的报错文案与中止点未验证**：本轮只跑静默安装。

## 4. 下阶段计划（按优先级）

1. **产品侧加固（§3 的 1 / 2）**：先写 ADR（行为变更必须有决策记录），再改 `PhrasesDialog` 与 server 启动回收，再重建 + A/B。**这是"打掉孤儿锁的根"**，安装器侧现在只是兜底。
2. **交互安装验收**：手动跑一次带 UI 的安装器，确认报错文案与中止点符合预期，补进 `test/regression/cases/`。
3. **`FluxingPhrasesDialog.exe` 补版本资源**：新建 .rc + 加进 `$script:ProductFiles`，否则版本门禁对它永远是盲区。
4. **CI `gate.yml` 首次真跑观察**：注意 `weasel.props` / `env.bat` 是机器本地文件（git-ignored）⇒ `Test-VersionConsistency` 已容忍缺失；CI 里 `output\Win32\*` 不存在 ⇒ C0175 走 SKIP 属设计内。CI 用 `-Mode source -SkipNsisCompile -SkipGuardUnitTest`。
5. **把 `tmp/` 里的可复用补丁脚本产品化**：`tmp/normalize-gate-assets.py`、`tmp/normalize-rc-eol.py`、`tmp/patch-*.py` 目前都还躺在 git-ignored 的 `tmp/`——**这正是 RC1 的翻版**。可复用者应迁到 `scripts/maintenance/`。
6. **清理受保护路径残留（§3.4）**：需用户手动执行，脚本已备好思路但被硬策略拦。
7. **§3.3 安装器内部兜底**：在 `install.nsi` 的 `File` 后加"写后校验"（读回文件大小/哈希），失败即 `Abort`。

## 5. 已验证 / 推断 / 未验证

### 已验证（运行时取证）

- 20 个孤儿 `FluxingPhrasesDialog.exe` 主因独占锁；`taskkill /T` 打不到父已死的孤儿。
- 修复后 A/B：旧安装器 `rewrote_target=False`，新安装器 `installed_hash_matches_build=True`。
- PE 版本资源修复前后 `FileVersion` 空 → `1.1.9.0`（同大小同版本双二进制对照）。
- L0 12/12、L3 7/7 KILLED、L4 1/1、`verify-installed` `problems=0`，均为本轮实跑。
- 远端 Release 资产 digest 与本地构建产物逐字节一致。
- 用户侧：安装无报错、Esc 后面板行为符合预期。

### 推断（未直接观测，但有强证据）

- 19 个孤儿的父进程消失发生在 `WeaselServer.exe` 被强杀/崩溃时（只剩 1 个父仍活，且其命令行 pid 能对上）。
- guard 测例实际是 16 条（9 BLOCK + 4 ALLOW + 3 负对照），与 `TOTAL=16` 自洽。

### 未验证

- 交互安装器（带 UI）的报错文案与中止点。
- 1.1.9 在**其它机器**（干净环境 / 无残留进程）上的安装行为。
- CI `gate.yml` 在 GitHub Actions 上的首跑结果。
- `D:\Program Files\fluxing\weasel\fluxing\` 这坨嵌套残留的成因（只确认了它存在且删不掉）。

## 6. 证据与锚点（完整路径）

### 必读文档顺序（新会话照此读）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`
2. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
3. `F:\soft\00selfmade\rime_claude\AGENTS.md`（§1 首屏 / §4 危险区 / §5 checklist）
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`（R1–R9 / P1–P8）
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§A 与 §11）
6. `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（**末尾 L174 / L175 必读**）
7. `F:\soft\00selfmade\rime_claude\docs\analysis\2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`
8. `F:\soft\00selfmade\rime_claude\docs\adr\0033-regression-asset-and-staged-gate.md`
9. `F:\soft\00selfmade\rime_claude\docs\adr\0032-own-ui-client-identity-not-focus-order.md`
10. `F:\soft\00selfmade\rime_claude\test\regression\README.md` + `DEFECTS.yaml`
11. `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`（只在改 `output/data/**/*.yaml` 时读）

### 代码锚点

- `include/WeaselIPC.h`：`OwnUiClientExeName()` 返回 `"FluxingPhrasesDialog.exe"`；正则 `OwnUiClientExeName\(\)\s*\{\s*return\s*"([^"]+)"`。
- `WeaselServer/PhrasesDialogIPC.cpp:112`：`TerminatePhrasesDialog()` 只在优雅退出路径。
- `WeaselUI/PhrasesDialog.cpp:967`：Esc → `Hide()`。
- `output/install.nsi`：清理块 :218/:460/:615/:1327（`FluxingPhrasesDialog.exe`，无 `/T`）；`WeaselServer.exe` 清理是 `/T` 版 :202/:445/:583/:1313；版本化 shim 段 `weasel.dll` :691-762 / `weaselx64.dll` :779-851（`File /nonfatal /oname=weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.$FluxingShimSeq.dll` :823、`regsvr32` :833）；`/DFLUXING_OUTFILE=` :62/:65-66，默认 `archives\fluxing-${FLUXING_VERSION}-installer.exe` :68。
- `output/guard_install_path_test.nsi`（155 行，UTF-8 BOM）：`:1-18` 头部契约（结果文件 + 末行 `TOTAL=` + 退出码语义）、`:10-14` 产物路径与行格式、`:20 Unicode true`、`:25-27 GUARD_TEST_OUT 默认 $EXEDIR`、`:29-34 !ifdef GUARD_TEST_OUTFILE`、`:47 FileOpen …\guard_install_path_result.txt w`、`:53-119` 16 条用例、`:121 FileWrite TOTAL=… MISMATCH=…`、`:124-131 SetErrorLevel 0/1 + Abort`。**cwd 必须是 `output\`**（`!include` 是相对路径）。过判据 = `TOTAL=16 MISMATCH=0` 且 `rc=0`。
- `scripts/gate/lib/GateChecks.ps1`（24983 B / 450 行）：`Test-Utf16LeFileShape` / `Test-InstallerOwnUiKillBlocks` / `Test-NsiCompiles` / `Test-InstallPathGuardUnitTest` / `Get-VersionFacts`。
- `scripts/gate/L0-static.ps1`（287 行）：`$script:ProductFiles`、`Resolve-MakensisPath`（先查 `C:\Program Files (x86)\NSIS\Bin\makensis.exe`，再 `Get-Command makensis`）、双目录解析表、汇总段。
- `scripts/gate/run-all.ps1`（6051 B / 170 行）：`Invoke-Step` 输出直通、L0/L3/L4/L5 编排、汇总。
- `scripts/install/verify-installed.ps1`（11517 B / 267 行）：`Resolve-RepoRoot`、`Resolve-BuildArtifact`、`Get-InstalledCandidates`、`Get-DisplayVersion`、五段校验、`exit 1/2/0`。

### 脚本与产物

- 构建：`xbuild.bat weasel installer`（30–90 s）/ `build.bat weasel installer`（5–15 min）。
- 门禁：`scripts\gate\run-all.cmd`（或 `run-all.ps1 -Root <repo> -SkipSandboxGate`）。
- 装机校验：`powershell -File scripts\install\verify-installed.ps1`（默认 `install_dir=D:\Program Files\fluxing\weasel`、`build_dir=<repo>\output`）。
- 证据日志（均在 git-ignored 的 `tmp/`）：`gate-l0-fix4.log`（L0 12/12）、`gate-runall2.log` / `gate-runall-final.log`（串跑）、`gate-fix3.log`（guard 隔离测 + verify-installed 全绿）、`install119-ab-v2.txt`（A/B 判据）、`verify-installed-fix2.log`。
- 发布产物：`output/archives/fluxing-1.1.9-installer.exe`。

## 7. 本轮工具坑（勿重试）

- **`Get-Command makensis` 在本机返回空**；`C:\Program Files (x86)\NSIS\makensis.exe` 也不存在。真实路径 `C:\Program Files (x86)\NSIS\Bin\makensis.exe`。`Get-Item` 对象用 `.FullName`，只有 `Get-Command` 才有 `.Source`。
- **makensis `/O` 不是 guard nsi 的覆盖开关**（契约是 `!ifdef GUARD_TEST_OUTFILE`）⇒ 必须 `/DGUARD_TEST_OUTFILE=<绝对路径>`，且值不能含空白。
- **NSIS `Unicode true` 下 `FileWrite` 写 UTF-16LE + BOM** ⇒ `Get-Content -Encoding utf8` 读成乱码，要用 `[System.IO.File]::ReadAllText`。
- **PowerShell `-shl` 会把结果截断回左操作数类型** ⇒ 读 UTF-16LE 码元必须 `[int]$b[$i] -bor ([int]$b[$i+1] -shl 8)`。
- **`& $exe 2>&1` 在宿主 stdout 未重定向时抛** `StandardOutputEncoding is only supported when standard output is redirected` ⇒ 用 `ProcessStartInfo`。
- **`Out-Host` 绕过 `*>` 重定向** ⇒ 子进程输出要用 `| ForEach-Object { Write-Host $_ }`。
- **`$r = & $block` 会把子进程每行输出收成 `Object[]`** ⇒ `[int]$r` 抛错且吞输出；用 `$r = @(& $block)`。
- **NSIS 静默模式吞掉 `File` 失败**：退出码 0、安装段跑完、注册表写到尾，但目标文件没重写。
- **`D:\Program Files\` 受硬安全策略保护**：`rd /S /Q`、`rm --`、`Remove-Item`、`robocopy` 均被拦。
- **bash 工具里 `cd` 后写文件被拦** ⇒ 用绝对路径或 `git -C <root>`。
- **GUI 子系统 exe 用 `Start-Process -Wait` 会挂死** ⇒ 交给 `.cmd` 经 `cmd /c` 启动。
- **`.cmd` 必须纯 ASCII**（UTF-8 BOM 会让首行崩）。
- **`Set-StrictMode -Version Latest` 下 `$x += …` 引用未声明变量会抛异常**，声明位置放错会让检查变成恒绿假牙。
- **正则 `$Matches` 会被同函数后一次 `-match` 覆盖** ⇒ 取分组立刻落成具名变量。
- **`Test-NsiCompiles` 在临时目录编译 `install.nsi` 会因 `!include` 相对路径断** ⇒ M06 控制组/变异组必须放 `output\` 内。
- **`gh release view --json isLatest` 不支持** ⇒ 用 `gh release list`；取 digest 用 `gh api .../releases/tags/<tag> --jq '.assets[].digest'`。
- **`weasel.props` / `env.bat` / `tmp/` 均 git-ignored**，`librime` 子模块脏改动不得提交。
- 仓库**无 `.gitattributes`** ⇒ 行尾不被 git 归一化，写文件必须自己保证行尾。

## 8. 仓库状态（交棒时）

### 8.1 本轮入库内容

- 分支 `Fluxing`，remote `kizemo` = `https://github.com/kizemo/fluxing.git`（公开仓库）。
- 已发布提交链：`2cece41..778ce35`、`778ce35..35d6885`；tag `v1.1.9` → `778ce35`。
- 本轮新增/修改**已全部入库并推送**（远端 HEAD = `980a425`），提交链：
  - `b55f39d` `fix(installer)`：升级段写失败回滚旧文件 + 收尾清理存在性守卫（含 L176）
  - `9f4891c` `ci(gate)`：分层门禁 L0/L3/L4 + 回归资产库 + CI 工作流 + `verify-installed.ps1` + `version.txt`
  - `2bd4fe2` `docs(adr)`：ADR 0033 + 交接卡定稿
  - `980a425` `WeaselDeployer`：rc 行尾归一化（纯行尾，逐行文本 diff 为 0）
- 尚未提交：仅 `librime` 子模块脏改动（**永不提交**）。
- `CHANGELOG.md` 已有 `[1.1.9] - 2026-11-03`（`fix(fluxing)` + `fix(installer)`）；`.specify/memory/lessons-learned.md` 已有 L174 / L175。
- `librime` 子模块脏（` m librime`）**不得提交**。

### 8.2 对外发布状态（已发布，用户已签字）

- Release `https://github.com/kizemo/fluxing/releases/tag/v1.1.9` 为 **Latest**。
- 远端资产 digest `sha256:8d5dd1a877e7f9ff40dfd87b4bdbd060070a80038184d1397d323fda8ad6c18e`、size=42797082。
- 本机已装 1.1.9，用户已确认两条修复生效。

## 10. 追加（同日深夜）：L176 安装器升级段「Rename 成功后 File 失败」缺陷

> 触发：用户贴截图，弹框标题 `WeaselServer.exe`，正文「Windows 找不到文件 'WeaselServer.exe'。请确定文件名是否正确后，再试一次。」，并问「是正在修改文件导致的，还是 bug 复现」。
> **定性结论：不是 L174（1.1.8 孤儿锁）复现。** L174 已修好且用户已验收。这是 `install.nsi` 升级段**自身的第二个缺陷**，一直存在、此前未触发。

### 10.1 根因（与 L174 完全不同的机制）

- `install.nsi` 升级段对每个 exe 做**两次独立 I/O**：① `Rename` 把旧 `X.exe` → `X.exe.old.tmp`；② `File` 把新 exe 写入 `X.exe`。
- 当 ② 失败时（目标被**正在运行**的 `WeaselServer.exe` / Deployer / Setup 挡住），`SetOverwrite try` **静默跳过且不置错误位**，静默安装退出码仍是 0、注册表写到尾 ⇒ **旧文件名已消失、新文件没写入** ⇒ 之后任何 `start WeaselServer.exe` 都报「找不到文件」。
- 触发条件：安装器运行期间该 exe 正在运行。本轮执行触发者是回归脚本 C0032 刚 `Start-Server` + 22:22 跑安装器。
- 与 L174 的区别：L174 是「目标被**孤儿进程独占锁**，`File` 打不开」；L176 是「**Rename 已经成功**、目标名腾空后 `File` 因同目录新文件仍被运行中的映像占用而失败」。两者都表现为装不上，但机制、修法、预防手段都不同。

### 10.2 修法（提交 `b55f39d`）

- 三个 exe（`WeaselServer` / `WeaselDeployer` / `WeaselSetup`）的 `File` 后紧跟 `IfErrors 0 <X>_rename_ok`；失败分支立刻
  `Rename "$INSTDIR\<X>.exe.old.tmp" "$INSTDIR\<X>.exe"` 把旧文件**还原**，再 `StrCpy $FluxingDegraded 1` + `DetailPrint` + `Goto <X>_done`。
- 收尾 sweep 的 `Delete "$INSTDIR\<X>.exe.old.tmp"` 用 `${If} ${FileExists} "$INSTDIR\<X>.exe"` / `${EndIf}` 包住（边际加固：回滚成功后 `.old.tmp` 已不存在，`Delete` 无害）。
- 补丁脚本 `tmp/patch-installer-upgrade-rollback.py`（幂等 + 原子写 + 锚点计数断言）。`install.nsi` 68193 → 69937 B，BOM=True，crlf=1429，bare_lf=0，`NSIS_EXIT=0`。

### 10.3 预防（两层静态齿，均已造坏数据验证有牙）

1. **L0 新检查 `installer-upgrade-rollback`**：每个 exe 三判据 —— ① `IfErrors 0 <X>_rename_ok` 存在且 label 恰好 1 次；② 回滚 `Rename` 在 guard 之后、done label 之前；③ 最后一次 `Delete .old.tmp` 前必须有 `${If} ${FileExists}` 守卫。4 个变异（删 IfErrors / 删 Rename / 删尾部守卫 / 删收尾 Delete）**全部翻红**，控制组保持 `OK=True`。
2. **`scripts/install/verify-installed.ps1` §0 必备文件存在性**：`WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` / `uninstall.exe` / `FluxingPhrasesDialog.exe` 缺一即 `problems`；`*.old.tmp` 存在归 `notes`（是退路，说明写入/回滚被打断，需人看但不等于安装失败）。有牙验证：`tmp\fake-install`（缺 `WeaselServer.exe`）+ `-SkipHash` → **EXIT=1**。

> **未做运行时复现**的原因已写进 L176：`File` 失败需要「`Rename` 成功后瞬间目标被加锁」的竞态，不可确定性复现 ⇒ 用两层静态齿覆盖，而不是靠一次手工复现。

### 10.4 门禁终态（本轮实跑，日志 `tmp/gate-final-rollback.log`）

```
L0 RESULT mode=all checks=13 pass=13 fail=0 skip=0     exit=0 (90.6s)   ← 由 12 项增到 13 项
L3 RESULT mutations=7 killed=7 survived=0 invalid=0    exit=0 (115.9s)
L4 REGRESSION level=fast cases=1 pass=1 fail=0 skip=0  exit=0 (2.2s)
L5 sandbox-gate  未出结果（只测已发布的 archives\fluxing-1.1.9-installer.exe 与本轮未改的 scripts/sandbox-gate ⇒ 与本轮 diff 无因果）
```

L0 新增的第 13 项就是 `installer-upgrade-rollback`（PASS）。

### 10.5 现场恢复与现状

- 唯一合规的恢复路径是重跑安装器（`bash` 工具里任何 `Copy-Item` 目标是 `D:\Program Files\` 都会被硬安全策略整条拦掉）。
  `installer.exe /S` → exit 0 ⇒ `D:\Program Files\fluxing\weasel\` 5 个必备 exe 齐全，`WeaselServer.exe` 2,287,104 B / FileVersion `1.1.9.0` / SHA256 与 `output\Win32\WeaselServer.exe` 一致；`verify-installed` **EXIT=0**；server 已启动。
- `weasel.dll.old.tmp`(1675264 B) 与 `weaselx64.dll.old.tmp`(1981952 B) 仍留在安装目录，`verify-installed` 以 notes 报出。
- 产物 `output\archives\fluxing-1.1.9-installer.exe` = 42,797,082 B / mtime 2026/11/3 20:36:38，**未被本轮冒烟编译覆盖**（与已发布 Release 仍逐字节一致）。
- ⚠️ 本轮 makensis 冒烟编译曾因未传 `/DFLUXING_OUTFILE` 误覆盖本地 `output\archives\fluxing-1.0.0-installer.exe`（旧版本历史归档，未发布、无远端对应）。

### 10.6 本轮新增的工具坑（补充 §7）

- **makensis 编译 `install.nsi` 必须传 `/DFLUXING_OUTFILE=<绝对路径>`**（该文件自己声明的宏，:62/:65-66/:68）；`/O` 无效。不传会覆盖 `archives\fluxing-<ver>-installer.exe`，且**直接编译时版本会退化成 `1.0.0`**。
- **`File /oname=` 才是 NSIS 改名语法**；`File "src" "dst"` 会把两个路径都当源文件 ⇒ `no files found.` 并中止打包。
- **NSIS `SetOverwrite try` 静默跳过且不置错误位** ⇒ 判断写入成败必须用 `IfErrors`，不能靠退出码。
- **`.cmd` 必须纯 ASCII**（UTF-8 BOM 让首行崩）。
- **补丁脚本必须原子写**（全内存改完再落盘）；三次 ABORT / 异常都没有污染文件。
- **用户报错时先 `task_stop` 正在跑的门禁以保现场**，再诊断。

## 9. 接力 prompt（一句话）

见 `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-03-fluxing-119.md`。