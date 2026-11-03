# 交接卡 · 火流猩 1.1.9 安装锁修复 + 版本资源修复 + 「功能丢失 / bug 复现」系统性预防机制

> 生成时间：2026-11-03 ｜ 仓库：`F:\soft\00selfmade\rime_claude` ｜ 分支 `Fluxing`（remote `kizemo` = `https://github.com/kizemo/fluxing.git`）
> 上一张卡：`docs/archive/handoffs/handoff-2026-11-03-fluxing-117-release-and-esc-quickpanel-bug.md`（1.1.7 发布 + Esc 根因）

---

## 1. 背景与目标

本轮由用户三条指令触发：

1. **修安装报错**：装 1.1.8 时报「无法打开要写入的文件：`D:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe`」。
2. **复盘**「开发过程中反复出现**已开发好的功能丢失**、**已修的 bug 复现**」，并给出**系统性预防机制**。
3. **更新完整 handoff 文档 + 一句话 prompt**（prompt 只给全路径引导，不复述 handoff 内容）。

本轮实际完成：① 安装报错的根因坐实 + **根治修复 + 1.1.9 装机验收通过**；② 复盘分析落盘（8 条根因 + 7 根支柱 + 固定验收矩阵）；③ 本交接卡 + 接力 prompt 落盘；④ 发布前自查发现并修掉 **PE 版本资源 `FileVersion` 恒空**（连带让 `AGENTS.md §4.3` 的版本门禁从「形同虚设」恢复成真门禁）。
**预防机制本身尚未落地**（见 §4）。

---

## 2. 本轮已完成

### 2.1 安装报错根因坐实（运行时取证，非读码推测）

- **症状**：交互安装报「无法打开要写入的文件：`D:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe`」。
- **独占打开探针**复现 `being used by another process` ⇒ 目标是**被锁**，不是权限/路径问题。
- **占用源**：**20 个** `FluxingPhrasesDialog.exe` 残留进程（`MainWindowTitle` 全空 ⇒ 隐藏孤儿，合计约 **350 MB** WS），命令行 `--pipe=\\.\pipe\FluxingPhrasesDialog\<serverpid>`。
- **父进程关系**：19 个的父进程**早已消失**（`<gone>`：153304×8 / 229904 / 233084 / 296924 / 367116 / 374476×3 / 378004 / 382432 / 458164 / 463992），仅 1 个（pid 415252）的父 `WeaselServer.exe` pid 84392 仍活。
- **产品侧成因**：短语窗口按 Esc 只 `Hide()`、**不退出进程**（L167）；`PhrasesDialogIPC::TerminatePhrasesDialog()` 只在 server **优雅**退出路径上被调用（`WeaselServer/PhrasesDialogIPC.cpp:112`）⇒ server 被强杀 ⇒ 子进程回收代码根本不执行 ⇒ 孤儿累积。
- **安装器侧成因**：`output/install.nsi` 的 kill 覆盖面**只有 `WeaselServer.exe`**（`.onInit` / `call_uninstaller` / install 段 / uninstall 段），**从未覆盖 `FluxingPhrasesDialog.exe`**；且 `taskkill /T` 只沿**活着的**子进程树走 ⇒ **父已死的孤儿不在任何进程树里**。
- **旧假设被证伪**：`install.nsi` 原注释断言「dialog 是短时 modal，install 期间被锁的概率极低」⇒ 实测不成立。
- **阻塞已解除**：杀掉 20 个残留进程后独占打开复测通过（Fluxing/Weasel 总 WS 415 MB → 65.1 MB）。

### 2.2 根治修复（`output/install.nsi`）

- 在 **4 处**按**映像名**杀自家 UI 进程（**不带 `/T`**，正因如此才能覆盖父已死的孤儿）：
  `.onInit` / `call_uninstaller` / install 段（`SetOverwrite on` 之前）/ uninstall 段。
  exe 名与 `fluxing::OwnUiClientExeName()`（`include/WeaselIPC.h:248`，ADR 0032）同源。
- **附带修 L09 违规**：该文件此前带 **46 处裸 LF**（spec 097 / L164 的 shim 补丁带进来）⇒ 已归一化为 **UTF-8 BOM + 100% CRLF**（`EF BB BF` / `crlf=1393 / bare_lf=0` / 68193 B）。
- 落地方式：**字节级补丁脚本 + 锚点计数断言 + 幂等复跑**（不是手改）：
  `tmp/patch-install-nsi-ownui-kill.py`（二次运行 ⇒ 4 处全部 `skipped`，字节不变）。

### 2.3 1.1.9 构建与装机验收（A/B v2，判据已修正）

- 版本抬到 **1.1.9**（`env.bat`：`FLUXING_VERSION` / `PRODUCT_VERSION=1.1.9.0` / `FILE_VERSION=1.1.9.0` / `VERSION_PATCH=9`；`env.bat` git-ignored ⇒ 版本号不入库，见 §7 坑 6）。
- 构建：`build.bat weasel installer` ⇒ `BUILD_EXIT=0`；**共两轮构建**：第一轮（安装锁修复）产物 42,786,961 B；**第二轮（版本资源修复，见 §2.6）产物 42,797,082 B**，`output/archives/fluxing-1.1.9-installer.exe` 现为第二轮产物。
- **复现场景（脏环境）**：先拉起 **6 个真孤儿**（`--pipe=` 指向不存在的管道）⇒ 独占打开必失败，报错与用户现场**一字不差**。
- **A/B v2 判据**（不用退出码，见 §7 坑 1）：

| | 旧 1.1.8 安装器 | 新 1.1.9 安装器 |
|---|---|---|
| 孤儿占锁下是否重写目标文件 | **否**（`rewrote_target=False`）⇒ 旧二进制留存 | **是**（安装后 sha256 == 构建产物） |
| 安装段是否走到尾 | 是（`DisplayVersion` 写成 1.1.8） | 是（`DisplayVersion=1.1.9`） |
| 退出码 | **0（失败被吞）** | 0 |
| 孤儿是否被清理 | **否**（6 个全留） | **是**（0 个） |
| 结论 | **被孤儿锁挡住 ⇒ 复现忠实** | **通过** |

- 第二轮构建后**重跑同一 A/B**，结论不变：`VERDICT=AB PASS`（报告 `tmp/install119-ab-v2.txt`，`time=2026-11-03 20:37:09`）。
- 验收脚本：`tmp/verify-install119.ps1`（单场景）与 `tmp/verify-install119-ab-v2.ps1`（A/B），报告落 `tmp/install119-verify.txt` / `tmp/install119-ab-v2.txt`。
- **静默模式把 NSIS 的 `File` 失败吞掉了**（退出码 0、安装段继续跑完、只是旧二进制没换掉）——这是本轮最重要的**副产物发现**，已写进 §4.4 与复盘文档。
- 装机现状核验（第二轮产物）：已装 `D:\Program Files\fluxing\weasel\WeaselServer.exe` sha256 前缀 `C372A3FAC8F71E2B` == `output/Win32/WeaselServer.exe`（无陈旧二进制）；`DisplayVersion=1.1.9`；自家 UI 残留进程 0；`WeaselServer` 1 个在跑。

### 2.4 「功能丢失 / bug 复现」复盘分析落盘

`docs/analysis/2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`（新建）：
**8 条根因**（RC1–RC8：验收资产不在版本控制内 / 验收矩阵缺边界维 / 探针无自证契约 / 探针有副作用无清理 / 缺陷无回归映射 / 无一键门禁 / 版本号不入库 / 清理覆盖面靠手工名单）
+ **7 根支柱**（P1 回归用例入库 / P2 缺陷→用例映射 / P3 一键门禁 L0–L6 / P4 CI 化 / P5 打包前强制门禁 / P6 探针契约 / P7 安装前环境清场）
+ **固定验收矩阵**（干净安装 / 升级 / **残留进程** / **孤儿进程** / 多会话 / DPI / 旧残留目录）。

### 2.5 PE 版本资源修复：`FileVersion` 字符串恒空（L175）

- **症状**：`(Get-Item output\Win32\WeaselServer.exe).VersionInfo.FileVersion` 读出**空字符串**，而 `ProductVersion` 正常为 `1.1.9.0`。
- **三层取证**（只看一层会被误导）：
  ① PE 内 `VS_FIXEDFILEINFO`（签名 `0xFEEF04BD`）的 `dwFileVersionMS/LS` = **1.1.9.0**、`dwProductVersionMS/LS` = 1.1.9.0（三处语言块全对）⇒ **固定版本信息没问题**；
  ② **字符串表**里的 `FileVersion` 条目是**空串** ⇒ 资源管理器属性页「文件版本」空白；
  ③ .NET 的 `FileVersionInfo.FileVersion` 读的是**字符串表**（不是固定版本信息）⇒ 固定版本信息完全正确也照样读成空。
- **根因**：四个 rc 写的是 `VALUE "FileVersion", FILE_VERSION`，而 `FILE_VERSION` 展开成**无引号**的 `1.1.9.0`，rc.exe 存不进字符串表 ⇒ 该条写成空串；同一处的 `VALUE "ProductVersion", STRZ(PRODUCT_VERSION)` 因 `STRZ(x)=STRZ2(x)=#x` 会**先展开再字符串化** ⇒ 一直是对的。**同一个文件里只差一个 `STRZ()`，一个对一个错。**
- **修法**：**10 处**（`WeaselServer` 3 / `WeaselTSF` 3 / `WeaselSetup` 1 / `WeaselDeployer` 3）统一改成 `VALUE "FileVersion", STRZ(FILE_VERSION)`；rc 是 **UTF-16LE + BOM**，用字节级补丁脚本落地（`tmp/patch-rc-fileversion-strz.py`：锚点计数断言 + BOM/行尾自证 + 幂等复跑）。
- **影响面**：`AGENTS.md §4.3` 把 `Get-Item output\Win32\WeaselServer.exe | ?{$_.VersionInfo.FileVersion}` 当作 `weasel.props` 版本一致性的**核验命令** ⇒ 该命令长期返回空 ⇒ **门禁形同虚设**（不是「版本对不上」，是「查不出来」）。
- **A/B**：同版本号（1.1.9.0）、同大小（2,287,104 B）的两个二进制仅差此一处 ⇒ 修复前 `.VersionInfo.FileVersion=''`、修复后 `'1.1.9.0'`。
- **修复后核验**：`WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` / `weasel.dll` / `weaselx64.dll` 五个产物 `FileVersion` 全部为 `1.1.9.0`（门禁 PASS）。
- **顺带纠正一处旧结论**：上一张卡（1.1.7）写「`FileVersion` 资源为空是**历史口径**，非回归」——该结论**方向对但结论过时**：不是「历史口径」，是**真缺陷**，且它让 `AGENTS.md` 的版本门禁长期失效（L175 已修正）。

### 2.6 文档入库

- `.specify/memory/lessons-learned.md` 追加 **L174**（安装器清理覆盖面必须覆盖孤儿、孤儿锁的判定方法、A/B 判据修正、静默模式吞失败）与 **L175**（版本资源三层取证、`STRZ` 规则、文档里的核验命令本身要回归测试）。
- `CHANGELOG.md` 的 `[1.1.9]` 段含两条：`fix(installer)`（安装锁）+ `fix(fluxing)`（版本资源）。
- 本交接卡 + `docs/archive/prompts/prompt-next-session-2026-11-03-fluxing-119.md`。

---

## 3. 仍未修的缺陷（本轮坐实，未动手）

### 3.1 短语窗口在 pipe 断开（server 死）时不自退

`PhrasesDialog.cpp:967` Esc → `Hide()`；进程常驻 ⇒ server 被强杀即成孤儿。
**建议修法**：客户端侧在 pipe 读失败/断开时主动退出（并保留「Esc 只隐藏」的产品语义）；或 server 侧用 Job Object 兜底。
**注意**：Esc 的「隐藏但保留管道」是 L167 的既定产品语义 ⇒ 改它属于**行为变更**，需 ADR，不要顺手改。

### 3.2 server 启动时不回收上一代遗留的自家 UI 进程

即孤儿产生的源头之一。可在 server 启动时按映像名清理上一代残留（安装器已做同样的事，可复用口径）。

### 3.3 静默安装的失败会被吞掉（安装器健壮性）

实测：NSIS `File` 失败 ⇒ 退出码 0、安装段继续跑完、**旧二进制留在盘上** ⇒ 用户以为装成功。
**建议修法**：安装段末尾做**写后校验**（安装后的 `FluxingPhrasesDialog.exe` / `WeaselServer.exe` 的 sha256 或版本资源 == 构建产物），不一致则 `SetErrorLevel` 非 0 + 落诊断日志。

### 3.4 受保护路径下的安装残留（需用户手动）

`D:\Program Files\fluxing\weasel\fluxing\`（223 文件 / 231,538,974 B）与 `weaselx64.dll.old.tmp` 仍在盘上。
已备份到 `D:\Program Files\fluxing\engine-nested-fluxing-bak-20261103`（逐文件计数 + 字节自证一致）。
**Agent 无法代劳**（L173：`D:\Program Files\` 下删除被硬策略拦），需用户在提权 cmd 执行，命令见 `tmp/release-118-status.md`。

### 3.5 `FluxingPhrasesDialog.exe` 完全没有版本资源

它有独立工程 `FluxingPhrasesDialog/FluxingPhrasesDialog.vcxproj`，但**没有配套 `.rc`** ⇒
`VersionInfo.FileVersion` / `ProductVersion` / `FileDescription` 全空。
本轮未修（不在 1.1.9 范围内）；若要补，需新增 rc 并接入 `VERSION_*` 宏（口径与 §2.5 一致：`STRZ(FILE_VERSION)`）。

---

## 4. 下阶段计划（下一会话执行，按优先级）

### 4.1 P1 + P2：回归用例入库 + 缺陷映射（**最高优先**）

- 把本会话的探针 v3、验收脚本、变异脚本**从 `tmp/` 提升为 `test/regression/`**（`tmp/` 被 git-ignore ⇒ 下一会话拿不到 ⇒ 这是「功能丢失」的机制性原因之一，见复盘 RC1）。
- 每个用例必须带：**case-id / 前置条件 / `finally` 自清理（杀自己拉起的进程、还原注册表）/ 退出码即结论**。
- 建 `test/regression/DEFECTS.yaml`：至少先录三条 —— ADR 0032（Esc 连带收起）、L174（安装孤儿锁）、**L175（版本资源 `FileVersion` 恒空 ⇒ `AGENTS.md §4.3` 门禁失效）**。
- 落点：`test/regression/`（新目录）。

### 4.2 P3：一键门禁 `scripts/gate/run-all.cmd`

阶段化，退出码分段可单跑：**L0 静态**（BOM/CRLF/锚点唯一性/`.ps1` 语法）→ **L1 构建** → **L2 单测**（`scripts/test-infra/run-test-suite.bat`）→ **L3 变异**（判定取反必须翻红）→ **L4 回归**（P1 用例，自带清理）→ **L5 装机**（`scripts/sandbox-gate/run-verify.cmd`）→ **L6 打包**。
**硬约束**：L6 必须先过 L0–L5。
**本轮新增的静态门禁项**：**版本资源门禁**（读 `output/Win32/WeaselServer.exe` 的 `.VersionInfo.FileVersion` 断言非空且 == 期望版本），否则同类回归会再次静默通过。

### 4.3 ADR 0033（机制决策必须走 ADR）

按 `CLAUDE.md` §3 分流：门禁/回归套件 = 构建流水线变更 ⇒ 写 `docs/adr/0033-*.md`（MADR 格式，参考 `docs/adr/README.md`）。
另需读但尚未读的：`docs/adr/0013` / `0016` / `0019` / `0030` / `0031`、`docs/research/sogou-reverse-2026-11/ROADMAP.md` §6.4。

### 4.4 §3.3 的写后校验（安装器健壮性）

安装段末尾比对「落盘文件 sha256/版本 == 构建产物」，不一致 ⇒ 非 0 退出 + 诊断落盘。
（§2.5 已经给出可用的比对口径：落盘文件 `.VersionInfo.FileVersion` 必须非空且等于构建版本。）

### 4.5 P7 产品侧加固（§3.1 / §3.2）

短语窗口 pipe 断开自退 + server 启动回收上一代自家 UI 残留。**前者是行为变更，需 ADR**。

### 4.6 P4 + RC7：CI 与版本号入库

GitHub Actions 跑 L0–L4（nightly 跑 L5）；`env.bat` 的版本号移到**入库的版本文件**（否则换机重建不可复现 ⇒ L107「同版本不同内容」安装器事故）。

### 4.7 仍未验证项（与 1.1.7 遗留相同，需用户环境）

96 DPI 零回归 / 跨屏动态 DPI / 第二登录会话隔离。

---

## 5. 已验证 / 推断 / 未验证

### 已验证（运行时取证）

- 20 个孤儿进程占锁；杀完后独占打开通过（阻塞已解除）。
- 旧 1.1.8 安装器在孤儿占锁下**没有重写目标文件**（`rewrote_target=False`）但退出码为 0 ⇒ 复现忠实 + 静默模式吞失败。
- 新 1.1.9 安装器在同一场景下：安装后 sha256 == 构建产物、孤儿清零、`DisplayVersion=1.1.9`（**两轮构建各跑一次，结论一致**）。
- 已装 `WeaselServer.exe` 与 `output/Win32/WeaselServer.exe` sha256 一致（`C372A3FAC8F71E2B`）⇒ 无陈旧二进制。
- `output/install.nsi`：4 处 kill 块、BOM `EF BB BF`、`crlf=1393 / bare_lf=0`、补丁幂等（二次运行零字节变化）。
- 补丁脚本后置校验必须与写入同换行空间（本轮踩过，见 §7 坑 4）。
- **PE 版本资源**：固定版本信息 `1.1.9.0`（直接解析 `VS_FIXEDFILEINFO`）；字符串表 `FileVersion` 修复前为空、修复后为 `1.1.9.0`；五个产物 `.VersionInfo.FileVersion` 全部 `1.1.9.0`。
- `lessons-learned.md` 追加 L174/L175 后仍为 **UTF-8 BOM + 100% CRLF**（`crlf=13758 / bare_lf=0`）；`CHANGELOG.md` 插入 `[1.1.9]` 后 `crlf=7040 / bare_lf=0`。

### 推断（未直接观测，但有强证据）

- 用户当次失败的因果链：**孤儿锁 ⇒ NSIS `File` 写失败**（A/B 已在等价场景复现同现象；用户现场的具体调用链未逐步抓包）。
- 20 个孤儿主要由本会话历次探针 `taskkill /F /IM WeaselServer.exe` 制造（父 pid 全 `<gone>` 与探针启动的 server pid 序列吻合）。

### 未验证

- 交互安装器（非静默）在同一场景下的报错文案与中止点（本轮只跑静默）。
- 96 DPI / 跨屏动态 DPI / 多登录会话隔离。
- 「写后校验」「pipe 断开自退」等修复尚未实现，自然未验证。
- `FluxingPhrasesDialog.exe` 的版本资源（本轮未做，见 §3.5）。

---

## 6. 证据与锚点（完整路径）

### 必读文档顺序（新会话照此读）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`
2. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
3. `F:\soft\00selfmade\rime_claude\AGENTS.md`
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§10 NSIS 陷阱 / §11 命令速查）
6. `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（重点 L167 / L169 / L171 / L172 / L173 / **L174** / **L175**）
7. `F:\soft\00selfmade\rime_claude\docs\adr\0032-own-ui-client-identity-not-focus-order.md`
8. `F:\soft\00selfmade\rime_claude\docs\analysis\2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`
9. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-03-fluxing-117-release-and-esc-quickpanel-bug.md`（上一张卡）

### 代码锚点

- `F:\soft\00selfmade\rime_claude\output\install.nsi` —— `.onInit` 守卫在 `:159-167`；4 处自家 UI kill 块（`.onInit` / `call_uninstaller` / install 段 / uninstall 段）；`File "Win32\FluxingPhrasesDialog.exe"`（原注释断言「被锁概率极低」已被证伪）
- `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServer.rc:116` / `:230` / `:324`（`VALUE "FileVersion", STRZ(FILE_VERSION)`，本轮由 `FILE_VERSION` 改为 `STRZ(FILE_VERSION)`）
- `F:\soft\00selfmade\rime_claude\WeaselTSF\WeaselTSF.rc:159` / `:171` / `:183`；`WeaselSetup\WeaselSetup.rc:321`；`WeaselDeployer\WeaselDeployer.rc:197` / `:330` / `:506`（同改）
- `F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialogIPC.cpp:112`（`TerminatePhrasesDialog()` 只在优雅退出路径）/ `:289-332`（`CreateProcessW`，`SW_HIDE`）/ `:334-344`
- `F:\soft\00selfmade\rime_claude\PhrasesDialog.cpp:967`（Esc → `Hide()`）
- `F:\soft\00selfmade\rime_claude\include\WeaselIPC.h:248`（`OwnUiClientExeName()`）
- `F:\soft\00selfmade\rime_claude\AGENTS.md:487`（`weasel.props` 版本一致性核验命令，本轮修好）
- `F:\soft\00selfmade\rime_claude\CHANGELOG.md`（`[1.1.9]` 段两条）
- `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（L174 / L175）

### 脚本与产物

- `F:\soft\00selfmade\rime_claude\tmp\patch-install-nsi-ownui-kill.py`（字节级补丁，锚点断言 + 幂等）
- `F:\soft\00selfmade\rime_claude\tmp\patch-rc-fileversion-strz.py`（rc 字节级补丁，UTF-16LE + 幂等）
- `F:\soft\00selfmade\rime_claude\tmp\verify-install119.ps1` / `tmp\verify-install119-ab-v2.ps1`（装机验收 + A/B）
- `F:\soft\00selfmade\rime_claude\tmp\install119-verify.txt` / `tmp\install119-ab-v2.txt`（报告）
- `F:\soft\00selfmade\rime_claude\tmp\append-l174-and-changelog-119.py` / `tmp\append-l175-and-changelog-119b.py`（文档补丁，幂等）
- `F:\soft\00selfmade\rime_claude\tmp\build-119-installer.log`（第一轮构建日志）/ `tmp\build-119b-installer.log`（第二轮构建日志，`BUILD_EXIT=0`）
- `F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.9-installer.exe`（**42,797,082 B**；sha256 `8D5DD1A877E7F9FF40DFD87B4BDBD060070A80038184D1397D323FDA8AD6C18E`；md5 `556BB87230FBB9E3C767DDF4796A818B`）
- `F:\soft\00selfmade\rime_claude\tmp\evidence-0032v3\ANALYSIS.md`（探针三阶段归因）
- `F:\soft\00selfmade\rime_claude\tmp\release-119-notes.md`（发布说明）/ `tmp\release-118-status.md`（1.1.8 收尾台账 + 手动清理命令）
- 权威门禁：`F:\soft\00selfmade\rime_claude\scripts\sandbox-gate\run-verify.cmd`；单测：`F:\soft\00selfmade\rime_claude\scripts\test-infra\run-test-suite.bat`

---

## 7. 本轮工具坑（勿重试）

1. **NSIS 静默模式下 `File` 失败不会反映在退出码上**：实测退出码 0、安装段跑完、注册表写到了尾，但**目标文件没被重写**。⇒ 验收安装**必须比对文件内容（sha256/版本）+ 注册表尾**，只看退出码会得出「旧安装器也成功 ⇒ 复现无牙」的错误结论。
2. `cmd /c $cmd` 在 PowerShell 里要取 **`$LASTEXITCODE`**；直接 `cmd /c` 赋值得到的是 stdout（本轮因此把「安装成功」误判成 `INSTALL_EXIT=` 空）。
3. NSIS **未开 `LogSet`** ⇒ `/LOG=` **不产生日志文件**；「安装是否完成」只能用注册表/文件判定。
4. **字节级补丁的后置校验必须在与写入相同的换行空间里数锚点**：本轮拿 LF 锚点去数已写成 CRLF 的文本 ⇒ 误报「锚点不唯一」，白慌一场。
5. `output/install.nsi` 此前带 **46 处裸 LF**（spec 097 / L164 的 shim 补丁带进来）⇒ 任何对它的补丁都必须同时做**行尾归一化 + BOM 断言**，并保留字节级幂等脚本。
6. `env.bat` 的版本变量是 `set "NAME=value"` **带引号**形式 ⇒ 正则 `^set\s+NAME=` 匹配不到；本轮改用字面 `.Replace`。
7. `Get-Process ... | Measure-Object` 的 `.Count` 在单结果时行为异常 ⇒ 一律 `@(...)` 包起来。
8. `env.bat` / `weasel.props` / `tmp/` 均 git-ignored ⇒ 改它们天然不入库（版本号不入库是 L107 事故的根因之一）。
9. `git status` 里的 ` m librime` 是子模块脏改动 ⇒ **不要提交**（P3 / L10）。
10. **rc 文件是 UTF-16LE + BOM**（`FF FE`），不是 UTF-8 ⇒ 读要用 `Encoding.Unicode`，补丁必须字节级，断言要按**字节**算长度（`STRZ()` 在 UTF-16 下是 12 字节，不是 6 —— 本轮踩过）。
11. **PE 版本要分三层验**：`VS_FIXEDFILEINFO`（数值）/ `StringFileInfo`（字符串）/ .NET `VersionInfo`（读的是字符串表）⇒ 只看一层会被另外两层掩盖（L175）。
12. **解析 PE 版本资源时别数错字段偏移**：`VS_FIXEDFILEINFO` 的 `dwSignature` 之后是 `dwStrucVersion`，**再往后**才是 `dwFileVersionMS/LS`；本轮第一次读数把 `dwStrucVersion` 当成了 FileVersion 一半 ⇒ 得出「1.0.1.1」的假象。

---

## 8. 仓库状态（交棒时）

- 分支 `Fluxing`，remote `kizemo/Fluxing`；HEAD 见 §8.1（提交信息见 commit message）。
- 工作树脏项应仅剩 ` m librime`（子模块，不提交）。
- 装机版本：**1.1.9**（`D:\Program Files\fluxing`，`HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel` → `InstallDir`），卸载键 `DisplayVersion=1.1.9`；**当前无自家 UI 残留进程**。

### 8.1 本轮入库内容

- `output/install.nsi`（4 处自家 UI kill + 46 处裸 LF 归一化）
- `CHANGELOG.md`（`[1.1.9]` 两条）
- `.specify/memory/lessons-learned.md`（L174 / L175）
- `WeaselServer/WeaselServer.rc`、`WeaselTSF/WeaselTSF.rc`、`WeaselSetup/WeaselSetup.rc`、`WeaselDeployer/WeaselDeployer.rc`（10 处 `STRZ(FILE_VERSION)`）
- `docs/analysis/2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`（新建）
- `docs/archive/handoffs/handoff-2026-11-03-fluxing-119-install-lock-and-regression-guardrails.md`（本卡）
- `docs/archive/prompts/prompt-next-session-2026-11-03-fluxing-119.md`（新建）

提交划分（Conventional Commits，单一 scope）：

- 提交 1 = `2eb8faa` —— `fix(installer)`：`output/install.nsi` + `CHANGELOG.md`
- 提交 2 = `aaaa889` —— `docs(fluxing)`：`lessons-learned.md`（L174）+ 复盘分析 + 本卡 + prompt
- 提交 3 = `fix(fluxing)`：4 个 rc + `CHANGELOG.md`（版本资源）
- 提交 4 = `docs(fluxing)`：`lessons-learned.md`（L175）+ 本卡更新

### 8.2 对外发布状态

`v1.1.9` 的 GitHub Release **未发布**（发布属对外操作，需用户签字）。待发资产：
`output/archives/fluxing-1.1.9-installer.exe`（**42,797,082 B**；
sha256 `8D5DD1A877E7F9FF40DFD87B4BDBD060070A80038184D1397D323FDA8AD6C18E`；
md5 `556BB87230FBB9E3C767DDF4796A818B`）。
发布说明草稿：`tmp/release-119-notes.md`。

---

## 9. 接力 prompt（一句话）

```
请先读 F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-03-fluxing-119-install-lock-and-regression-guardrails.md（交接卡：完整背景、目标、进度、下阶段计划、已验证/推断/未验证、证据全路径、本轮工具坑），再按该卡 §6 列出的顺序补读 F:\soft\00selfmade\rime_claude\CLAUDE.md、F:\soft\00selfmade\rime_claude\CONTEXT.md、F:\soft\00selfmade\rime_claude\AGENTS.md、F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md、F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md、F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md（末尾最新几条）、F:\soft\00selfmade\rime_claude\docs\adr\0032-own-ui-client-identity-not-focus-order.md、F:\soft\00selfmade\rime_claude\docs\analysis\2026-11-03-postmortem-feature-loss-and-bug-recurrence.md，充分掌握项目背景、目标、进度与下阶段计划后，再执行该卡 §4 的下阶段计划。
```

同一份 prompt 也存于：`F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-03-fluxing-119.md`
