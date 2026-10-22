# 078 - 修复 lessons-learned.md / CHANGELOG.md 的历史编码损坏

> **Docs-only spec.** 两份最珍贵的项目历史文档（踩坑记录 + 发版日志）里的
> 中文内容在 2026-06 之前就被批量损坏，本 spec 负责把受损行逐条重写回可读状态。
> 不改任何代码、不改 IPC 协议、不动 librime。

## 0. Status: DONE（2026-10-22）

- 受损面已完全量化（见 §1），逐行清单见
  [`damage-inventory.md`](damage-inventory.md)（记录的是**修复前**状态）。
- 恢复工具已落地并加固：
  [`scan-encoding-damage.py`](../../../scripts/test-infra/scan-encoding-damage.py)、
  [`apply-line-rewrites.py`](../../../scripts/test-infra/apply-line-rewrites.py)、
  [`make-damage-inventory.py`](../../../scripts/test-infra/make-damage-inventory.py)、
  [`verify-encoding-repair.py`](../../../scripts/test-infra/verify-encoding-repair.py)。
- 交付：lessons-learned.md 重写 **180 行**，CHANGELOG.md 重写 **5 行**；
  两文件扫描器均归零（`damaged_lines=0 fffd=0 nul=0 c0=0 exit=0`）。
- 行数 / CR 数 / BOM 状态三项字节不变量全部保持不变。
- 证据：[`verification-coverage-audit.md`](verification-coverage-audit.md)、
  [`verification-provenance.md`](verification-provenance.md)、
  [`rewrite-map-lessons-learned.json`](rewrite-map-lessons-learned.json)、
  [`rewrite-map-changelog.json`](rewrite-map-changelog.json)。

## 1. 受损清单（2026-10-22 实测，修复前状态）

| 文件 | 总行数 | 受损行 | U+FFFD | NUL | C0 控制符 | 干净基线 |
|---|---|---|---|---|---|---|
| `.specify/memory/lessons-learned.md` | 10,245 | **172** | 445 | 13 | 273 | `879cb2e914` (2026-06-30) |
| `CHANGELOG.md` | 5,751 | **36 + 5** | 153 | 6 | 82 | `06eadeccf5` (2026-07-01) |

- lessons-learned 的 172 行 = **118 行含 FFFD/NUL** + **54 行仅含控制符**。
- CHANGELOG 的 36 行（FFFD/NUL）在 2026-10-22 首轮已修；本轮又揪出
  **5 行 C0 残留**（3204 / 3214 / 3694 / 3919 / 5600）——首轮门禁漏判了这一类。

## 2. 损坏机制（取证结论，已用干净基线逐字符验证）

损坏是**四代叠加**的。只按 `EF BF BD` 计数会**严重低估**受损面：

**第一代 · GBK 有损解码（不可逆）**
写入时文本被逐字符走了一次有损解码：无法解码的字节被替换成 `U+FFFD`
（字节 `EF BF BD`）。**ASCII 全部幸存，中文大面积消失。**
证据：受损行里 `always: Shift+Shift_L/R`、`TestDefaultHotkeys 25/25 PASS`、
`keycode=Shift_L/R`、`Control+1/2/3..9` 等 ASCII 片段完整无损，
而它们之间的中文全部变成 `U+FFFD`。

**第二代 · NUL 替换（不可逆）**
一个字符被替换成 `0x00`。位置已知，字符不知（`0x001E1E1E` / `0x00000804` /
`0x18.8` / `F:\soft00selfmade` 这类丢失的 ASCII 数字 `0` 属于此类）。

**第三代 · `chr(ord(c) & 0xFF)` 降字节（可逆）**
**每个 CJK 字符被替换成它的低字节。** 这解释了本轮全部控制符：

| 原字符 | 码位 | 低字节 | 落地形态 |
|---|---|---|---|
| `（` U+FF08 | `EF BC 88` | `0x08` | BS |
| `与` U+4E0E | `E4 B8 8E` | `0x0E` | SO |
| `—` U+2014 | `E2 80 94` | `0x14` | DC4 |
| `。` U+3002 | `E3 80 82` | `0x02` | STX |
| `，` U+FF0C | `EF BC 8C` | `0x0C` | FF |

两个关键后果：

1. **低字节落在 `0x0A`–`0x0D`（LF/CR/VT/FF）时，字符被当作换行符吃掉，
   一个逻辑行被劈成多个物理行。** 于是「上一轮机械恢复把完整段落写在拆段
   后的**后一行**、前几行保持受损未改」——这正是历次遗留碎片的成因。
2. **低字节落在 `0x20`–`0x7E`（可打印 ASCII）时，损坏是**不可见**的。**
   例如 `防` U+9632 → `0x32` = `'2'`，标题 `### 防 L19 复发` 变成
   `### 2 L19 `。**字节扫描完全看不见这一类**，本轮共补 8 行。

**第四代 · C 转义序列被反转义（可逆）**
`\a` / `\b` / `\v` / `\f` 被还原成真实控制字节，**并吞掉构成转义的那个字母**：
`accept:` → BEL + `ccept:`、`version` → VT + `ersion`、
`add_shflags` → BEL + `dd_shflags`、`\fluxing` → FF + `luxing`。
还原规则：`0x07→'a'`、`0x08→'b'`、`0x0B→'v'`、`0x0C→'f'`（路径分隔符场景为 `\`+字母）。

**时间线（实测）**

| 时间 | lessons-learned | CHANGELOG | 事件 |
|---|---|---|---|
| 2026-06-30 | `879cb2e914` 起 C0 出现 | — | 第三代降字节写入 |
| 2026-07-01 | — | `06eadeccf5` 为最后一个零损坏版本 | — |
| 2026-07-03 | FFFD/NUL 涌入 | 153 FFFD + 6 NUL + 74 C0 | 第一、二代批量重写 |
| 2026-07-06 | +66 FFFD | — | spec 041 追加 L53 |
| 2026-10-22 | 本轮修复 180 行 | 本轮修复 5 行（首轮 36 行） | spec 078 |

## 3. 可恢复性判定（决定了任务性质）

| 手段 | 产出 | 结论 |
|---|---|---|
| NUL → ASCII `0` | 38 处 | ✅ 全部修完 |
| GBK 往返解码 `line.encode('gb18030').decode('utf-8')` | 15 行 | ✅ 可机械应用 |
| 干净父基线逐字搬运 | **71 行** | ✅ **首选**：父基线里该行完好 |
| 父基线子串 | 4 行 | ✅ 可用 |
| 控制符反解义自证（`chr(ord & 0xFF)` 逆运算） | **36 行** | ✅ 可机械应用 |
| 拆段碎片（内容已被相邻行覆盖） | **50 行** | ✅ 写空字符串，避免重复段落 |
| 人工重写（无任何干净源） | **19 行** | ⚠️ 需上下文依据：其中 8 行父基线可佐证（含父基线自身的损坏需一并修好），11 行是 L52 DPI 段（损坏之后才写入，历史无干净版本） |
| 字节级还原 FFFD | — | ❌ **不可能** —— `U+FFFD` 替换是信息销毁，不可逆 |

→ **结论：172 行里 161 行可机械证明正确，19 行需要上下文判断。**
交付物因此是**逐行工作清单 + 工具 + 三重校验门禁**，而不是一次盲改。

## 4. 重写规则（人工部分）

1. **不得凭空编造**。每行重写只能依据三样东西：
   - 该行幸存的 ASCII 骨架（事实锚点：文件名、测试计数、md5、hex 常量、版本号）
   - 干净父基线 `0c0716c36a`（lessons-learned）/ `06eadeccf5`（CHANGELOG）
     中的**同一条**段落
   - 该行上下的干净上下文行
2. **中文一律用脚本从父基线直接搬运，不手打**（`set-from-parent.py`），
   杜绝转录误差。
3. **拆段碎片写空字符串**。当某行的内容已被相邻行（干净行或已恢复行）
   完整覆盖时，写 `""` 而不是重复写一遍——否则文档会出现两段同样的内容。
   该规则由 `audit-coverage.py` 的重复碎片检测自动把关。
4. **不删行、不改行数、不调顺序**。
5. 术语沿用仓库既有译法（见 `CONTEXT.md` §3 词汇表）：
   候选/上屏/切中英/静默安装/注册表/托盘面板 等。

## 5. 验收标准

### 5.1 功能

- `lessons-learned.md` 与 `CHANGELOG.md` 的 U+FFFD / NUL / **C0 控制符** 计数均为 **0**
- 两文件严格 UTF-8 解码通过
- 行尾不变：两文件全 CRLF，bare LF = 0
- BOM 规则不变：lessons-learned 有 BOM、CHANGELOG 无 BOM
- 受损行数 = 0（`scan-encoding-damage.py` 退出码 0）

### 5.2 验证（字节级，per L40 / L44）

```powershell
& python scripts\test-infra\verify-encoding-repair.py --target .specify\memory\lessons-learned.md
& python scripts\test-infra\verify-encoding-repair.py --target CHANGELOG.md --map .specify\specs\078-repair-encoding-corruption\rewrite-map-changelog.json
# 两条命令都应打印 ALL CHECKS PASSED 且 exit=0
```

该门禁逐项校验：严格 UTF-8、行数 / CR 数 / BOM 与修复前修订一致、
U+FFFD / NUL / C0 归零、扫描器 exit=0、**改动行集合与工作清单完全一致**
（既不漏改也不错改）。

基线默认**自动定位**：取最近一次触及该文件的提交的父提交（即修复前修订），
因此修复提交之后这套命令依然可用；要手工指定时加 `--before-rev <sha>`。
该规则刻意**不看工作清单**——若按「改动集合 == 清单」来挑基线，清单就会
自己确认自己。

> ⚠️ 门禁必须同时认「C0 控制符」这一类。只查 FFFD / NUL 的旧版扫描器
> 会在 CHANGELOG 仍有 5 行受损时误报「干净」。

### 5.3 语义

- 重写后的中文与父基线段落语义一致（逐条对照 §4.1 三样依据）
- 涉及版本号 / 测试计数 / 文件名 / md5 的事实性内容，**必须**与父基线
  逐字一致，不允许"约等于"

## 6. 范围外

- 不动 `PRD.md` / `TDD.md` / `CONTEXT.md`（本轮已刷新的快照，不再碰）
- 不动任何 `.rc`（UTF-16LE，NUL 属正常）
- 不重写 `lessons-learned.md` 里 L108 之后的新条目（未受损）
- 不删历史条目（只重写，不改写历史 —— per R7）
- 仓库内其余文件的同类损坏（`PRD.md` / `TDD.md` / `spec 014` 等 10 个文件
  仍有 C0 残留）**不在本 spec 范围**，但检测器已就位，可另开 spec

## 7. 依赖

- `scripts/test-infra/scan-encoding-damage.py`（检测，含 C0 判定）
- `scripts/test-infra/apply-line-rewrites.py`（写回，行数/CR/BOM 不变量守卫）
- `scripts/test-infra/make-damage-inventory.py`（受损清单生成）
- `scripts/test-infra/verify-encoding-repair.py`（验收门禁）
- 干净父基线 `0c0716c36a`（lessons-learned）/ `06eadeccf5`（CHANGELOG）
- `CONTEXT.md` §3 词汇表（术语一致性）
- L01 / L02 / L09 / L40 / L44（字节级读写纪律）

## 8. 任务分解

见 [`tasks.md`](tasks.md)。工作顺序：先 lessons-learned 的 L18/L19 块
（行 1053–1343，语义密度最高），再 CHANGELOG 的 v0.18.6–0.18.8 段
（行 3559–3936），最后两个文件的散点行。
