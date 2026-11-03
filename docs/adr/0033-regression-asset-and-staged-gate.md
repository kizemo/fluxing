# 0033. 把验收资产产品化：缺陷登记表 + 分层门禁 + 变异自检

- **Status**: Accepted
- **Date**: 2026-11-03
- **Deciders**: Fluxing 维护者
- **Related**: ADR 0032（自家 UI 客户端身份）、`docs/analysis/2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`、L174 / L175

## Context

### 为什么现在必须做

用户反复提出的问题是「之前开发好的功能丢失、修好的 bug 又复现」。复盘（见 Related 的事后分析）
把 8 条根因收敛到一句话：**问题不是代码质量，而是验收资产从来没有被当成产品资产管理**。

具体事故链：

- **RC1（资产不在仓库里）**：证明 ADR 0032 有效的三阶段探针脚本 `run-0032-verify-v3.ps1` 放在
  `tmp/quickpanel-evidence/`，而 `tmp/` 是 git-ignored。换一台机器、换一个会话，
  能力证明就消失了 —— 于是「已修好的 bug」在下一次改动时没有回归网兜，静默复活。
- **RC7（版本号三处手工同步）**：`weasel.props` / `env.bat` / `CHANGELOG.md` 各写一遍版本号，
  且前两者是机器本地文件（git-ignored）。没有任何检查发现「构建出来的版本和 CHANGELOG 说的不一样」。
- **L174**：v1.1.8 安装报「无法打开要写入的文件：FluxingPhrasesDialog.exe」。根因是 20 个
  孤儿 `FluxingPhrasesDialog.exe` 独占锁住目标文件，而安装器只用 `taskkill /T` 清理 —— `/T` 只沿
  **活着的**进程树走，父进程已死的孤儿不在任何进程树里。更糟的是：这一行为**没有任何自动化检查**
  守着，所以它是「修完了靠人记得」。
- **L175**：四个 `.rc` 里 `VALUE "FileVersion", FILE_VERSION` 的宏展开成无引号 `1.1.9.0`，
  存不进字符串表 ⇒ `AGENTS.md §4.3` 里那条版本核验命令（读 `FileVersionInfo.FileVersion`）
  永远返回空。**门禁命令本身没人回归测试**，于是它长期失效而无人察觉。

### 目标

1. 每个已修 bug 都在仓库里有一条可执行的守护用例，且用例本身是版本管理的资产。
2. 版本号有单一真相源，多处一致性由机器检查，而不是靠人记。
3. 门禁自身有齿：能证明「检查会翻红」，而不是只看它打印了 PASS。
4. 分层，让日常改动能快速跑（秒级），破坏性验收默认关闭。

## Decision

### D1. 版本号单一真相源 = 仓库根 `version.txt`

入库 `version.txt`（内容如 `1.1.9`，无结尾换行、无 BOM）。`weasel.props` / `env.bat` 保持
git-ignored 的机器本地产物（`weasel.props` 里有机器特有 `BOOST_ROOT`，整体生成会破坏本机构建），
但由 `Test-VersionConsistency` 在它们**存在时**逐项比对 `version.txt`、`CHANGELOG.md` 顶部版本
与两文件里的 `VERSION_MAJOR/MINOR/PATCH`、`PRODUCT_VERSION`、`FILE_VERSION`。
文件缺失时该组跳过并在 detail 里显式说明 —— 否则 CI 干净 clone 第一天就恒红，
逼人直接把门禁关掉，比没有门禁更差。

### D2. 缺陷登记表 `test/regression/DEFECTS.yaml` 是唯一索引

每条已修缺陷登记 `id` / `title` / `class` / `status` / `fixed_ref` / `lesson` / `guard`
（`case` + `level` + `needs`）。`Test-DefectsRegistry` 强制**双向**一致：

- `status: guarded` 的条目必须指向 `cases/` 下真实存在的用例；
- `cases/` 下的用例必须被某条目引用（防孤儿用例）；
- 用例头部 `# CASE-LEVEL:` 必须与登记表 `level` 一致（防「登记成 fast、实际要真机」）；
- 用例内**禁止**出现 `tmp/` 或 `tmp\`（RC1 的齿：资产会随 git-ignore 丢失）。

### D3. 分层门禁 L0–L6

| 层 | 内容 | 是否默认跑 | 破坏性 |
|---|---|---|---|
| L0 | 静态：文件形状（BOM/CRLF）、rc UTF-16LE、安装器裸杀块、版本一致性、脚本可解析、登记表一致、NSIS 编译、路径守卫单测 | 是 | 否 |
| L1 | 构建（`xbuild.bat weasel installer`） | 手动 | 否 |
| L2 | 单测（`scripts/test-infra/run-test-suite.bat`） | 手动 | 否 |
| L3 | 变异自检：故意改坏资产，证明 L0 的检查会翻红 | 是 | 否（只碰临时副本） |
| L4 | 回归用例 `test/regression/run-cases.ps1 -Level fast` | 是 | 否 |
| L5 | `scripts/sandbox-gate/run-verify.cmd` | 是 | 否 |
| L6 | 安装器打包 + 装机验收（`scripts/install/verify-installed.ps1`） | 发版前手动 | 是 |

`scripts/gate/run-all.ps1`（入口 `run-all.cmd`）串起 L0 → L3 → L4 → L5。
任何一层非 0 ⇒ 整体失败；**全部步骤被跳过 ⇒ 也算失败**（空跑不算通过）。

### D4. 破坏性用例默认关闭，但绝不静默通过

- `C0032`（用户短语界面按 Esc 不收起快捷设置栏）需要 UI 自动化 ⇒ 要求 `FLUXING_ALLOW_UI_AUTOMATION=1`。
- `C0174`（安装器孤儿锁）的 A/B 段需要真的跑两次安装器 ⇒ 要求 `FLUXING_ALLOW_INSTALL_AB=1`
  并提供 `FLUXING_AB_OLD_INSTALLER` / `FLUXING_AB_NEW_INSTALLER`。
  但它的 **§A 静态段**任何机器都能跑（检查 `install.nsi` 里裸杀块 ≥4、`/T` 版 0 处），
  这一段的齿由 L3 的 M01/M02 证明。

不开开关时用例记 SKIP（退出码 2），不是 PASS。

### D5. L3 变异自检：证明检查有齿

对每个受守护资产造「控制组 / 变异组」两副本：

- 控制组（未变异副本）必须绿 —— 否则无法区分「检查有齿」和「检查本身坏掉」，记 `INVALID`；
- 变异组必须红 —— 仍绿或返回 `$null` 记 `SURVIVED`；
- 变异组被判红记 `KILLED`。

`survived > 0` 或 `invalid > 0` ⇒ L3 失败。变异体全部写在 `%TEMP%\fluxing-l3\run-<时间戳>\`
（每轮天然隔离，不删除工作目录本身）。唯一例外是 M06（NSIS 编译）：变异体必须落在 `output/` 内，
否则 `!include` 的相对路径断裂、控制组也会红，变异就失去判别性。

### D6. 装机验收不信退出码

`scripts/install/verify-installed.ps1` 比对五件事实：五产物 sha256 == 构建产物、
PE 版本资源两层非空且一致、`DisplayVersion` 等于期望版本、无残留 `FluxingPhrasesDialog.exe`、
目标文件可被 `FileAccess.Read + FileShare.None` 独占打开。
最后一项刻意不用 `ReadWrite`：`D:\Program Files` 下非管理员对文件无写权限，
`ReadWrite` 会抛 `UnauthorizedAccessException` 把人误导成「文件被锁」。

退出码 0 通过 / 1 失败 / 2 结论不确定（缺构建目录、无权限探测）。

### D7. CI 只跑源码层

`.github/workflows/gate.yml` 在 `windows-latest` 上跑 `L0 -Mode source -SkipNsisCompile -SkipGuardUnitTest`、
`L3 -SkipCompile`、`run-cases.ps1 -Level fast`。理由：CI 上没有 `output\Win32\*` 构建产物、
不保证有 makensis、也装不了本机产品。产物层（`-Mode artifact`）与 L6 留在发版机的实跑门禁里。

## Consequences

### 好处

- 已修缺陷有了「登记 → 用例 → 门禁」闭环，`DEFECTS.yaml` 就是防止功能丢失的账本。
- 版本号不一致在提交前就会红，不用等到装机发现 `FileVersion` 是空的。
- 门禁自身被 L3 反向验证，堵住了「检查恒绿」这一最难发现的失效模式
  （本轮实现时就踩到一次：`Test-DefectsRegistry` 里末尾重新 `$problems = @()` 让检查永远绿）。
- 分层使日常改动秒级反馈，破坏性验收显式开关。

### 代价与风险

- 每条新修的 bug 都要写用例 + 登记，短期增加单次修复成本。这是刻意的交换：
  用一次性的写用例成本，换掉「同一个 bug 修 N 次」的重复成本。
- 用例与产品代码同仓 ⇒ 产品行为变更时必须同步改用例，否则门禁红。这是期望行为。
- L0 的文件形状检查对编辑器敏感：写文件必须自己保证 BOM/行尾（仓库无 `.gitattributes`，
  git 不做行尾归一化）。工具侧已固化为 `Test-TextFileShape` / `Test-Utf16LeFileShape`。
- `weasel.props` / `env.bat` 缺失时版本一致性只覆盖 `version.txt` ⇄ `CHANGELOG.md`，
  覆盖面变窄。这是可接受的：本地开发机两者都在，CI 上本来就构建不了产品。

## 未做项与触发条件

以下三项**明确未做**，不假装完成：

1. **P7-a：用户短语窗口在 pipe 断开时自退**。
   现状 `PhrasesDialog.cpp` 的 Esc 只做 `Hide()`，server 被强杀后窗口成孤儿。
   改自退语义属于用户可见行为变更 ⇒ 需要单独 ADR + 重建 + A/B 验收。
   **触发条件**：再次出现「孤儿进程锁住安装目标」或用户报告窗口残留。
   临时兜底已就位：安装器按映像名清理（L174）。

2. **P7-b：server 启动时回收上一代遗留的自家 UI 进程**。
   **触发条件**：同 1。当前由安装器覆盖该场景。

3. **安装器内部自校验**（在 NSIS 里比对落盘哈希）。
   当前用外部 `verify-installed.ps1` 承担；理由：NSIS 里做哈希需要额外插件，
   且静默模式吞 `File` 失败的根因在 NSIS 的 `File` 指令语义（`SetOverwrite try` 静默跳过），
   外部校验更可靠也更容易演进。
   **触发条件**：出现「外部校验通过但用户侧仍失败」的情形。

另记一项已知缺口（不属于本 ADR 范围）：`FluxingPhrasesDialog.exe` 是独立 vcxproj 且没有 `.rc`，
完全没有版本资源。C0175 已显式排除该文件并在注释里说明，补 `.rc` 需要单独改动。
