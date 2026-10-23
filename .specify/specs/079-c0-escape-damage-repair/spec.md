# spec 079 - 修复仓库剩余的 C 转义损坏（spec 078 之后残留的 14 个文件）

## 1. 问题

spec 078 修好了 `lessons-learned.md`（172 行）与 `CHANGELOG.md`（5 行）里的编码损坏，
但仓库里还有 **14 个文件 / 87 行** 带着同一类损坏没有修。

### 1.1 首轮扫描发现的 10 个文件 / 72 行

| target | 损坏行 |
|---|---|
| `.specify/PRD.md` | 5 |
| `.specify/TDD.md` | 3 |
| `.specify/specs/001-user-visible-strings/tasks.md` | 9 |
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/plan.md` | 10 |
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/spec.md` | 18 |
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/tasks.md` | 15 |
| `.specify/specs/016-behavior-level-test-framework/spec.md` | 5 |
| `.specify/specs/037-fluxing-components-v0/plan.md` | 3 |
| `.specify/specs/037-fluxing-components-v0/spec.md` | 3 |
| `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` | 1 |

### 1.2 复检时发现的扫描器盲区：Tab 类损坏 19 行

首轮修复落地后做全仓库复检，发现**扫描器对 TAB 完全失明**：

`0x09` 是 C0 控制字符，但它是合法缩进字符，所以 `c0_controls()` 把它排除在外。
于是 `	` 被解转义成真实 TAB 之后，损坏在扫描器眼里**根本不存在**。

实测全仓库 261 个 tracked Markdown，行内 TAB 候选 44 处，人工逐条判定：

| 类别 | 处数 | 处置 |
|---|---|---|
| 真损坏（`\t` 解转义吞掉了字母 t） | 19 | 修复 |
| 合法 TSV（rime userdb 头，`project-knowledge.md`） | 9 | 不动 |
| 故意展示损坏字节的对照行（spec 078 `verification-provenance.md`） | 22 | 不动 |

19 处分布在 5 个文件里：

| target | Tab 类损坏行 | 其中首轮 map 已修 | 本轮补修 |
|---|---|---|---|
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/plan.md` | 5 | 4 | 1 |
| `.specify/memory/lessons-learned.md` | 5 | 0 | 5 |
| `.specify/specs/015-fix-test-infrastructure/tasks.md` | 1 | 0 | 1 |
| `.specify/specs/037-fluxing-components-v0/tasks.md` | 4 | 0 | 4 |
| `CHANGELOG.md` | 4 | 0 | 4 |

首轮 map 漏掉 `014/plan.md` 第 15 行是**行首** TAB（`\test/TestShiftSelectBinding/`），
其余 4 处 Tab 损坏都夹在行中间，所以首轮按"控制字节集合"扫描时把行首那处一并漏了。

全部 91 处（含 72 + 19）损坏**只**是控制字节这一类：0 个 U+FFFD、0 个 NUL。

## 2. 损坏模型（2026-10-23 标定）

源码里的一处 C 转义序列被当成普通文本解转义了：**反斜杠连同它所代表的那个字母，
一起被写成了原始控制字节**。

| 控制字节 | 来源转义 | 被吞掉的字母 | 实测样例 |
|---|---|---|---|
| `0x07` BEL | `\a` | `a` | `add_shflags` / `accept:` / `ascii_composer` / `aiec.fun` |
| `0x08` BS | `\b` | `b` | git 短哈希 `b543d5e` |
| `0x09` TAB | `\t` | `t` | `tasks.md` / `test/TestDefaultHotkeys/...` |
| `0x0B` VT | `\v` | `v` | `vcvars32.bat` |
| `0x0C` FF | `\f` | `f` | `feat(fluxing)` / `fix(fluxing)` / `fluxing` |
| `0x0D` CR | `\r` | `r` | `release/fluxing-...` / `rime::KeyEvent` |

这条模型不是推测：`scripts/test-infra/verify-rewrite-provenance.py` 的注释里已经
写了同一套映射（`\accept:` → BEL + `ccept:`），本 spec 只是把它补全到
`a / b / t / v / f / r` 六个字母并逐条落到具体行上。

**还原是机械的、唯一的**：把控制字节换回它吞掉的那个字母，行内其余字节逐字保留。

## 3. 范围

### 3.1 在范围内
- 上述 14 个文件的 87 行。
- 为表达这一类损坏所必须的工具修正（见 §4）。

### 3.2 明确不在范围内
- `output/data/**`（词典数据）、`librime/**`（子模块）、`release/**`：
  这些目录里有大量**正常中文**被扫描器的 GBK 启发式误报，但它们本身没有损坏，
  本 spec 一行都不改。
- spec 078 已完成的 `lessons-learned.md` / `CHANGELOG.md` 的修复内容本身。
- spec 078 `verification-provenance.md` 里刻意保留的损坏字节对照行。

## 4. 工具修正（随本 spec 一起落地）

| 工具 | 问题 | 修正 |
|---|---|---|
| `apply-line-rewrites.py` | 不变量要求"CR 数不变"，而 **CR 本身就是损坏字节**（`\r` → `0x0D`），于是这类修复根本写不进去 | CR 数允许按"本次替换实际移除的 CR 字节数"变化；LF 数 / 行数 / BOM 不变的不变量保留 |
| `scan-encoding-damage.py` | GBK 往返检测对**干净的简体中文**误报：`状态` 的 GBK 字节恰好是合法 UTF-8，往返成 `״̬`；更糟的是 `--apply-roundtrip` 会把正确中文改写成乱码 | 往返结果必须"在本文档的字符集内"才被采信。字符集 = 当前文件 + 全部历史修订的字符并集 |
| `scan-encoding-damage.py` | **对 TAB 类损坏完全失明**：`0x09` 是合法缩进字符，被排除在 C0 判定之外，于是 `\t` 解转义造成的损坏扫描器看不见 | 新增 `inline_tab_damage()`：按文档上下文判定（行内 TAB / 行首吞字 / TSV 块 / inline code / fenced code / `- OLD:` 对照行六类），并单独计入 `tab=` 统计与退出码 |
| `verify-encoding-repair.py` | 同 applier，验收门禁把合法的 CR 修复判成 `CR count changed` | 与 applier 用同一条规则核对 CR 数 |
| `verify-encoding-repair.py` | 基线按"最新一次改动该文件的提交的父提交"选，再**逐行按位置**比对：只要该文件在 spec 078 之后新增过段落（0.21.0.2 的 51 行），行号整体推移，门禁就把 5497 行判成"未声明的改动" | 基线改为：**工作区与 HEAD 不同时取 HEAD**（未提交修复的起点），一致时才回退到最新提交的父提交；比对改用 `difflib` 对齐，`insert` 块单独声明 |
| `verify-encoding-repair.py` | 内部调 scanner 时用 `REPO / "scripts/..."` 定位脚本，换一个仓库检查时就找不到工具，scanner 退出码 2 被误读成"门禁失败" | scanner 脚本路径固定取**工具自身所在仓库**，被检查的仓库只通过 `--repo` 传 |
| `test_encoding_toolchain.py`（新增） | 上述行为此前没有回归测试 | 11 组断言，把"干净中文不误报 / 真损坏仍能报出 / CR 修复可写入 / 行数变化仍被拒 / TAB 类正反例 / 行尾不被改写 / 基线取 HEAD / 工具路径不随仓库漂移"钉死 |

## 5. 验收标准

1. 14 个目标文件全部满足：`U+FFFD = 0`、`NUL = 0`、C0 控制字符 = 0、Tab 类损坏 = 0。
2. 14 个目标文件全部满足：`scan-encoding-damage.py` 报 `damaged_lines=0`、`tab=0` 且 exit 0。
3. 全仓库 261 个 tracked Markdown 的 TAB 类误报 = 0。
4. 每个被改的行都在对应的 `rewrite-map-*.json` 里声明，且行内除被吞的字母外**逐字节不变**。
5. `scripts/test-infra/test_encoding_toolchain.py` 全绿。
6. 14 个目标文件的 `verify-encoding-repair.py` 全部 `ALL CHECKS PASSED`。

## 6. 遗留项（不在本 spec）

- 扫描器对 `output/data/**` 的 GBK 误报（词典数据）与本 spec 无关，未处理。
- spec 078 文档里记录的验收命令**不能在同一棵树上重跑**：它验的是"当时那棵树"，
  而 spec 079 又在同一批文件上改了 15 行。等价验证见 `verification.md` §5。

## 7. 参考

- 损坏模型与工具：spec 078 `docs`（`verify-rewrite-provenance.py` 注释）
- 目录约定：`.specify/specs/078-repair-encoding-corruption/`
- 本 spec 的清单与映射：同目录 `damage-inventory.md` / `rewrite-map-*.json`
