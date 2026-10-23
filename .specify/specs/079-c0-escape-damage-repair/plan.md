# plan 079 - 修复仓库剩余的 C 转义损坏

## 1. 做法

不手工改字。损坏是**机械的、唯一的**：控制字节换回它吞掉的字母。因此流程是
"按规则生成映射 → 交给工具写入 → 用门禁复核"，而不是人肉编辑。

```
HEAD blob (损坏态)
   │  按 LETTER 表逐行重算
   ▼
rewrite-map-*.json          (行号 = 修复前基线行号)
   │  apply-line-rewrites.py  (守住行数 / LF 数 / BOM 不变)
   ▼
修复后的文件
   │  scan-encoding-damage.py + verify-encoding-repair.py
   ▼
damaged_lines=0, tab=0, exit=0
```

## 2. 为什么不用 git 历史回填

理想做法是把干净的历史版本整行搬回来。实际做不到：

- `.specify/PRD.md` 有干净祖先 `b9f89d0a3`，但损坏行全部在 386-605 行，
  **该祖先只有 297 行**，那些行是后来追加的附录，历史里没有对应行。
- `.specify/specs/014-*` / `016` / `037` 与 `test/TestShiftSelectBinding.cpp`
  **从创建起就带着损坏**（`git log -- <file>` 只有 1-2 个修订，且全部含损坏），
  没有任何干净版本可搬。
- 唯一有干净历史的 `.specify/specs/001-user-visible-strings/tasks.md`
  只覆盖前 287 行，损坏行在 297 行之后。

所以只能按规则还原。规则本身有两重佐证：

1. `verify-rewrite-provenance.py` 的注释里已经写了同一套映射（`\accept:` → BEL + `ccept:`）。
2. 逐条语义唯一：`accept:` / `ascii_composer` / `add_shflags` / `add_ldflags` /
   `vcvars32.bat` / `aiec.fun` / `feat(fluxing)` / `fix(fluxing)` / `b543d5e` /
   `release/fluxing-...` / `rime::KeyEvent` / `test/...` —— 还原后每个词都唯一成立，
   没有任何一处存在第二种读法。

## 3. 扫描器为什么漏掉了 TAB 类损坏

`0x09` 是 C0 控制字符，但它是**合法缩进字符**：每个 C/C++ 源文件都在用它，
rime userdb 头也用它分隔字段。所以 `c0_controls()` 把它排除在外。

后果是 `	` 被解转义成真实 TAB 之后，损坏在扫描器眼里**根本不存在**：
`\test/TestDefaultHotkeys/` 变成 `<TAB>est/TestDefaultHotkeys/`，一个字节扫描
看不出任何异常。

这不是理论风险。首轮修复落地后复检，实测全仓 261 个 tracked Markdown：

| 类别 | 处数 |
|---|---|
| 真损坏（`\t` 解转义吞掉了字母 t） | 19 |
| 合法 TSV（rime userdb 头，`project-knowledge.md`） | 9 |
| 故意展示损坏字节的对照行（spec 078 `verification-provenance.md`） | 22 |

因此本 spec 的第二部分工作是**给扫描器补上这一类判定**，并修掉因此暴露的
15 处残留损坏。判定必须带文档上下文（TSV 块 / inline code / fenced code /
`- OLD:` 对照行），因为 TAB 在这四种场景里都是合法的。

## 4. 工具修正的必要性

原工具链**无法表达这一类损坏**：

- `apply-line-rewrites.py` 的不变量是"CR 数与输入相同"。而 CR 本身就是损坏字节
  （`\r` → `0x0D`），于是 `release/` → `<CR>elease/` 这一类修复会被工具自己拒绝
  （实测报 `CR count changed (2 -> 1); nothing written`）。
- `scan-encoding-damage.py` 的 GBK 往返检测对干净中文误报。更严重的是
  `--apply-roundtrip` 会**把正确中文改写成乱码**：它对 PRD.md 的
  `### 10.5 spec 037 spec / plan / tasks 状态` 建议改写成
  `### 10.5 spec 037 spec / plan / tasks ״̬`（字节层已核实原文是正确 UTF-8：
  `e7 8a b6 e6 80 81`）。这条隐患与本 spec 的修复范围无关，但同属这套工具，
  一并修掉。
- `scan-encoding-damage.py` 对 TAB 类损坏完全失明（见 §3）。
- `verify-encoding-repair.py` 的基线选取与比对方式都会误报：
  基线按"最新一次改动该文件的提交的父提交"选，再逐行按位置比对，于是该文件
  在 spec 078 之后新增过段落（0.21.0.2 的 51 行）时，行号整体推移，
  门禁把 5497 行判成"未声明的改动"。另外它内部调 scanner 时用
  `REPO / "scripts/..."` 定位工具，换一个仓库检查时根本找不到 scanner，
  退出码 2 被误读成"门禁失败"。

## 5. 执行步骤

| # | 步骤 | 产出 |
|---|---|---|
| 1 | 扫描全仓，分离"真损坏"（控制字节）与"扫描器误报"（GBK 启发式） | 受损清单 |
| 2 | 查每个文件的历史拓扑，确认有没有干净祖先可搬 | §2 的结论 |
| 3 | 用干净祖先标定损坏模型（字母 ↔ 控制字节） | §2 的映射表 |
| 4 | 修正 3 个工具 + 新增 1 个回归测试 | `scripts/test-infra/` |
| 5 | 按规则生成 10 份 rewrite map + 清单 | `rewrite-map-*.json` / `damage-inventory.md` |
| 6 | 用 `apply-line-rewrites.py` 写入 | 修复后的文件 |
| 7 | 跑门禁：scanner / verify-encoding-repair / 回归测试 | `verification.md` |
| 8 | 复检：全仓普查行内 TAB，逐条人工判定真损坏 / 合法用法 | §3 的三分类表 |
| 9 | 给扫描器补 TAB 类判定（带文档上下文），生成并应用 4 份新 map | 19 处 Tab 类损坏修复完毕 |
| 10 | 修 verify 门禁的基线选取、比对方式与工具定位 | 门禁在 079 后重新可用 |

## 6. 验证

见 `verification.md`。四条硬指标：

1. 14 个目标文件 `U+FFFD=0 / NUL=0 / C0=0 / tab=0`；
2. 14 个目标文件 scanner 报 `damaged_lines=0`、`tab=0` 且 `exit=0`；
3. 全仓 261 个 tracked Markdown 的 TAB 类误报 = 0；
4. 13 个目标文件的 `verify-encoding-repair.py` 全部 `ALL CHECKS PASSED`。
5. `scripts/test-infra/verify-tab-detector.py` 全部 `ALL CHECKS PASSED`。

## 7. 风险与回滚

- 风险：**低**。改动只在 87 行上做"控制字节 → 字母"的替换，行内其余字节逐字保留，
  行数 / LF 数 / BOM 不变，可由 `git checkout` 单文件回滚。
- 风险：**中**（工具改动）。`apply-line-rewrites.py` 与 `verify-encoding-repair.py` 是
  spec 078 留下来的共享工具。改动后必须在 spec 078 的树上重放其两道门禁确认没被破坏，
  且 `test_encoding_toolchain.py` 覆盖全部新行为。
- 风险：**低**（TAB 类判定）。新规则带四种豁免，实测在全仓 261 个 Markdown 上
  误报为 0；反向验证（把 5 个受损文件的 HEAD 版本放进临时仓库）精确报出
  5 / 5 / 1 / 4 / 4 处。
- 回滚：工具改动与文件修复分两笔提交，可独立回退。
