# 078 · tasks — 逐行重写受损历史文档

> 顺序即依赖顺序。每完成一个 block 就提交一次，便于逐块 review / revert。
> 判定标准统一用 §5.2 的扫描器（退出码 0）与字节级复验。
>
> **2026-10-22 收口说明**：本轮把 A/B/C 三个 block 合并成一次提交
> （lessons-learned 180 行 + CHANGELOG 5 行），因为机械恢复 + 逐行人工重写
> 交叉进行，拆成多次提交反而难以 review。T000g–T000m 是本轮新发现并补做的
> 工作项。

## Block A — lessons-learned.md · L18/L19 块（行 1053–1343，约 100 行）

- [x] **T001** 取基线：`git show 0c0716c36a:.specify/memory/lessons-learned.md`，
      定位 L18/L19 段的英文段落，逐行建立"基线行号 ↔ 当前行号"对照表
- [x] **T002** 跑扫描器导出本块受损行清单（`--report`），
      逐行标注：可从基线直接搬回的英文片段 / 需重写的中文片段
- [x] **T003** 逐行重写标题行与正文行（块内约 100 行），
      依据 = 基线英文段落 + 当前行 ASCII 骨架 + 上下干净行
- [x] **T004** 字节级写回，校验：BOM 保留、CRLF 全量、bare LF = 0、
      行数不变、U+FFFD = 0
- [x] **T005** 扫描器复验 exit=0；`git diff 0c0716c3 -- <file>` 确认差异
      只落在中文片段上
- [x] **T006** commit

## Block B — CHANGELOG.md · v0.18.6–0.18.8 段（行 3559–3936，约 55 行）

- [x] **T007** 取基线：`git show 06eadeccf5:CHANGELOG.md`，同上建立对照表
- [x] **T008** 导出本块受损行清单
- [x] **T009** 逐行重写（发版条目的事实项：版本号 / md5 / 测试计数 /
      文件名必须与基线逐字一致）
- [x] **T010** 字节级写回 + 校验（无 BOM、CRLF 全量、行数不变、U+FFFD = 0）
- [x] **T011** 扫描器复验 exit=0
- [x] **T012** commit（首轮 36 行：`bcc7583`）

## Block C — 两个文件的散点行

- [x] **T013** lessons-learned 剩余散点逐行重写
- [x] **T014** CHANGELOG 剩余散点逐行重写
- [x] **T015** 字节级写回 + 校验 + 扫描器复验
- [x] **T016** commit

## Block D — 收尾

- [x] **T017** 全量字节级复验：两文件 U+FFFD = 0、NUL = 0、**C0 = 0**、
      严格 UTF-8 解码通过、行尾与 BOM 规则不变
- [x] **T018** 行数与基线一致性核对（`git diff --stat` 只应显示内容行变化，
      不应出现整段增删）
- [x] **T019** 术语抽查：重写后的中文与 `CONTEXT.md` §3 词汇表一致
- [ ] **T020** 更新 `CHANGELOG.md` 顶部 + `.specify/PRD.md` §10 /
      `.specify/TDD.md` §10 的状态快照 —— **本轮有意不做**：
      spec §6 明确把 PRD/TDD 列为范围外（且两文件自身仍有 C0 残留，
      此时追加条目会引入新的不一致）；CHANGELOG.md 顶部没有 Unreleased 段，
      首轮修复提交 `bcc7583` 同样没有给自己写条目，沿用该先例。
- [x] **T021** spec.md §0 Status 改为 DONE，附实测数字

## 本轮新增（2026-10-22，第二轮）

- [x] **T000g** 破解第三代损坏模型 `chr(ord(c) & 0xFF)`：逐字符对齐
      干净父基线确认（`（`→0x08、`与`→0x0E、`—`→0x14、`。`→0x02、`，`→0x0C）
- [x] **T000h** 确认第四代 C 转义反转义（`\a`/`\b`/`\v`/`\f` 吞掉字母）
- [x] **T000i** **修复扫描器的盲区**：旧版只查 FFFD/NUL/GBK 误读，
      漏判 C0 控制符（CHANGELOG 仍有 5 行受损时旧门禁误报"干净"）
- [x] **T000j** 补 8 行**不可见损坏**（低字节落在可打印 ASCII，如
      `防`→`'2'` 使标题变成 `### 2 L19 `），旧扫描器完全看不见
- [x] **T000k** 建立校验器并全部通过（均已入库于 `scripts/test-infra/`）：
      `verify-rewrite-provenance.py`（覆盖率 + 重复碎片 + 基线逐字 /
      反转义自证三源溯源，实测 172 受损行、0 uncovered、0 duplicate、
      71 逐字 / 4 子串 / 36 自证 / 50 置空 / 19 人工）、
      `verify-encoding-repair.py`（与修复前修订逐项比对的验收门禁）
- [x] **T000l** 中文一律用脚本从父基线搬运（`set-from-parent.py`），杜绝手打
- [x] **T000m** 验收门禁升级为「与修复前修订逐项比对」
      （`verify-encoding-repair.py`：行数 / CR / BOM / 改动行集合全部核对）

## 已完成（2026-10-22，随 spec 立项一起落地）

- [x] **T000** 受损面全量量化
- [x] **T000b** 修掉 38 个确定性 NUL（丢失的 ASCII `0`）：
      spec 037 spec/plan/tasks、spec 014 tasks、PRD、TDD、
      lessons-learned、CHANGELOG
- [x] **T000c** 取证损坏机制（第一代有损解码 → U+FFFD；
      第二代 GBK 误读 → `锟?` / `搂`，会掩盖第一代）
- [x] **T000d** 取证可恢复性：NUL 可判、GBK 往返可判 15 行、
      git 历史匹配 59 行但**同样已损坏**（无净收益）、
      字节级还原**不可能**（U+FFFD 是信息销毁）
- [x] **T000e** 落地工具 `scripts/test-infra/scan-encoding-damage.py`
      （索引式匹配，10 秒/文件，退出码可当门禁）
