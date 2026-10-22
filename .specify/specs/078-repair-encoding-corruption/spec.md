# 078 - 修复 lessons-learned.md / CHANGELOG.md 的历史编码损坏

> **Docs-only spec.** 两份最珍贵的项目历史文档（踩坑记录 + 发版日志）里的
> 中文内容在 2026-06 之前就被批量损坏，本 spec 负责把受损行逐条重写回可读状态。
> 不改任何代码、不改 IPC 协议、不动 librime。

## 0. Status: READY

- 受损面已完全量化（见 §1），逐行清单见
  [`damage-inventory.md`](damage-inventory.md)。
- 恢复工具已落地：[`scripts/test-infra/scan-encoding-damage.py`](../../../scripts/test-infra/scan-encoding-damage.py)。
- 不依赖 boost、不依赖重编、不依赖装机验证 —— 纯文档任务。

## 1. 受损清单（2026-10-22 实测）

| 文件 | 总行数 | 受损行 | U+FFFD | NUL | 干净基线 |
|---|---|---|---|---|---|
| `.specify/memory/lessons-learned.md` | 10,185 | **168** | 762 | 27 | `096c6e0` (2026-07-03) |
| `CHANGELOG.md` | 5,751 | **70** | 153 | 6 | `a3a008a` (2026-07-03) |

- 已在本轮修掉：**38 个 NUL**（spec 037 ×4、spec 014 ×5、PRD ×8、TDD ×3、
  lessons-learned ×14、CHANGELOG ×4）—— 全部是丢失的 ASCII 数字 `0`
  （`0x001E1E1E` / `0x00000804` / `0x18.8` / `F:\soft00selfmade`）。
- 剩余待重写：**209 行**（lessons-learned 153 + CHANGELOG 56）——
  即仍含 U+FFFD / NUL 的那些行，它们的中文被第一代损坏销毁。
- 另已机械修复 **29 行第二代损坏**（lessons-learned 15 + CHANGELOG 14）——
  用 GBK 往返还原，样本已逐条目视确认（`鍋滄`→`停止`、`瀹炴柦`→`实施`、
  `瀹炶`→`实装`、`鏉€`→`杀`）。

## 2. 损坏机制（取证结论）

损坏是**两代叠加**的，所以朴素的字节扫描会漏判：

**第一代（不可逆，字符真正丢失）**
写入时文本被逐字符走了一次有损解码：无法解码的字节被替换成 `U+FFFD`
（字节 `EF BF BD`）。**ASCII 全部幸存，中文大面积消失。**
证据：受损行里 `always: Shift+Shift_L/R`、`TestDefaultHotkeys 25/25 PASS`、
`keycode=Shift_L/R`、`Control+1/2/3..9` 等 ASCII 片段完整无损，
而它们之间的中文全部变成 `U+FFFD`。

**第二代（可逆，但会掩盖第一代）**
整个文件又经历了一次「按 GBK 读、按 UTF-8 写」：
- `U+FFFD`（`EF BF BD`）被 GBK 读成 `锟` + `?`（`EF BF 3F`）
- `§`（UTF-8 `C2 A7`）被 GBK 读成 `搂`

→ **只看 `EF BF BD` 计数会低估损坏面**（`096c6e0` 基线统计为 0，实际已经坏了）。
检测必须以「行内是否含非 ASCII 且能被 GBK 往返解码」为准，见工具实现。

**时间线**

| 时间 | lessons-learned U+FFFD | CHANGELOG U+FFFD | 事件 |
|---|---|---|---|
| 2026-06-30 | 0（实为已损坏的第二代形态） | 0（实为已损坏） | 第一代损坏已存在 |
| 2026-07-03 | 0 → **696** | 0 → **153** | spec 024/025 批量重写 L18/L19 与 v0.18.6-0.18.8 条目 |
| 2026-07-06 | 696 → **762** | — | spec 041 追加 L53 |
| 至今 | 762（其中 27 个 NUL 已修） | 153（其中 6 个 NUL 已修） | — |

## 3. 可恢复性判定（决定了任务性质）

| 手段 | 产出 | 结论 |
|---|---|---|
| NUL → ASCII `0` | 38 处 | ✅ 已全部修完 |
| GBK 往返解码 `line.encode('gb18030').decode('utf-8')` | 15 行 | ✅ 可机械应用，但只覆盖第二代损坏 |
| git 历史行匹配（ASCII 锚点全等） | 59 行 | ⚠️ 匹配到的历史行**同样是坏的**（第一代损坏早于所有历史版本），无净收益 |
| 干净基线 `096c6e0` / `a3a008a` | — | ⚠️ 基线里的对应行也带第一代损坏；只有英文段落是干净的 |
| 字节级还原 | — | ❌ **不可能** —— `U+FFFD` 替换是信息销毁，不可逆 |

→ **结论：209 行的中文必须人工重写。** 这不是"修 bug"，是"重写"，
所以本 spec 的交付物是**可执行的逐行工作清单 + 工具 + 验收门禁**，
而不是一次自动修复。

## 4. 重写规则（人工部分）

1. **不得凭空编造**。每行重写只能依据三样东西：
   - 该行幸存的 ASCII 骨架（事实锚点：文件名、测试计数、md5、hex 常量、版本号）
   - 干净基线 `096c6e0` / `a3a008a` 中的**同一条**英文段落
   - 该行上下的干净上下文行
2. **英文段落优先保留原样**。基线里英文是干净的，能确定措辞就原样搬回，
   只重写被毁掉的中文片段 —— 这样重写结果可与基线逐行 diff 验证。
3. **每行重写后必须能被基线佐证**：`git diff 096c6e0 -- <file>` 里
   该行的差异应只落在中文片段上。
4. **不删行、不改行数、不调顺序**（除 §6 明确允许的合并）。
5. 术语沿用仓库既有译法（见 `CONTEXT.md` §3 词汇表）：
   候选/上屏/切中英/静默安装/注册表/托盘面板 等。

## 5. 验收标准

### 5.1 功能

- `lessons-learned.md` 与 `CHANGELOG.md` 的 U+FFFD 计数均为 **0**
- 两文件的 NUL 计数均为 **0**
- 两文件的 `0xC0` / `0xC1` 计数均为 **0**（无 GBK 污染残留）
- 两文件严格 UTF-8 解码通过（`UTF8Encoding($false, $true)` 不抛异常）
- 行尾不变：lessons-learned 全 CRLF、CHANGELOG 全 CRLF，bare LF = 0
- BOM 规则不变：lessons-learned 有 BOM、CHANGELOG 无 BOM
- 受损行数 = 0（`scan-encoding-damage.py` 退出码 0）

### 5.2 验证（字节级，per L40 / L44）

```powershell
& python scripts\test-infra\scan-encoding-damage.py --path .specify/memory/lessons-learned.md
& python scripts\test-infra\scan-encoding-damage.py --path CHANGELOG.md
# 两条命令都应无输出且 exit=0
```

### 5.3 语义

- 重写后的中文与基线英文段落语义一致（人工逐条对照 §4.1 三样依据）
- 涉及版本号 / 测试计数 / 文件名 / md5 的事实性内容，**必须**与基线英文段落
  逐字一致，不允许"约等于"

## 6. 范围外

- 不动 `PRD.md` / `TDD.md` / `CONTEXT.md`（本轮已刷新的快照，不再碰）
- 不动任何 `.rc`（UTF-16LE，NUL 属正常）
- 不重写 `lessons-learned.md` 里 L108 之后的新条目（未受损）
- 不删历史条目（只重写，不重写历史 —— per R7）

## 7. 依赖

- `scripts/test-infra/scan-encoding-damage.py`（本 spec 随附）
- git 历史基线 `096c6e0` / `a3a008a`
- `CONTEXT.md` §3 词汇表（术语一致性）
- L01 / L02 / L09 / L40 / L44（字节级读写纪律）

## 8. 任务分解

见 [`tasks.md`](tasks.md)。工作顺序：先 lessons-learned 的 L18/L19 块
（行 1053–1343，语义密度最高），再 CHANGELOG 的 v0.18.6–0.18.8 段
（行 3559–3936），最后两个文件的散点行。
