# 078 · tasks — 逐行重写受损历史文档

> 顺序即依赖顺序。每完成一个 block 就提交一次，便于逐块 review / revert。
> 判定标准统一用 §5.2 的扫描器（退出码 0）与字节级复验。

## Block A — lessons-learned.md · L18/L19 块（行 1053–1343，约 100 行）

- [ ] **T001** 取基线：`git show 096c6e0:.specify/memory/lessons-learned.md`，
      定位 L18/L19 段的英文段落，逐行建立"基线行号 ↔ 当前行号"对照表
- [ ] **T002** 跑扫描器导出本块受损行清单（`--report`），
      逐行标注：可从基线直接搬回的英文片段 / 需重写的中文片段
- [ ] **T003** 逐行重写标题行与正文行（块内约 100 行），
      依据 = 基线英文段落 + 当前行 ASCII 骨架 + 上下干净行
- [ ] **T004** 字节级写回，校验：BOM 保留、CRLF 全量、bare LF = 0、
      行数不变、U+FFFD = 0
- [ ] **T005** 扫描器复验 exit=0；`git diff 096c6e0 -- <file>` 确认差异
      只落在中文片段上
- [ ] **T006** commit：`docs(fluxing): rewrite L18/L19 lesson block (spec 078)`

## Block B — CHANGELOG.md · v0.18.6–0.18.8 段（行 3559–3936，约 55 行）

- [ ] **T007** 取基线：`git show a3a008a:CHANGELOG.md`，同上建立对照表
- [ ] **T008** 导出本块受损行清单
- [ ] **T009** 逐行重写（发版条目的事实项：版本号 / md5 / 测试计数 /
      文件名必须与基线逐字一致）
- [ ] **T010** 字节级写回 + 校验（无 BOM、CRLF 全量、行数不变、U+FFFD = 0）
- [ ] **T011** 扫描器复验 exit=0
- [ ] **T012** commit：`docs(fluxing): rewrite v0.18.6-0.18.8 changelog block (spec 078)`

## Block C — 两个文件的散点行

- [ ] **T013** lessons-learned 剩余散点（行 160-161 / 927 / 1034 / 1050 /
      1858 / 3562-3609 等，共约 53 行）逐行重写
- [ ] **T014** CHANGELOG 剩余散点（行 3998 / 5422 / 5431 等，共约 1 行）
- [ ] **T015** 字节级写回 + 校验 + 扫描器复验
- [ ] **T016** commit：`docs(fluxing): rewrite scattered damaged lines (spec 078)`

## Block D — 收尾

- [ ] **T017** 全量字节级复验：两文件 U+FFFD = 0、NUL = 0、C0 = 0、C1 = 0、
      严格 UTF-8 解码通过、行尾与 BOM 规则不变
- [ ] **T018** 行数与基线一致性核对（`git diff --stat` 只应显示内容行变化，
      不应出现整段增删）
- [ ] **T019** 术语抽查：重写后的中文与 `CONTEXT.md` §3 词汇表一致
- [ ] **T020** 更新 `CHANGELOG.md` 顶部 + `.specify/PRD.md` §10 /
      `.specify/TDD.md` §10 的状态快照（追加 spec 078 ship 条目，
      per spec 035 只追加不重写）
- [ ] **T021** 在本 spec 的 spec.md §0 把 Status 从 READY 改为 SHIPPED，
      附实测数字（U+FFFD 归零、受损行归零）

## 已完成（2026-10-22，随 spec 立项一起落地）

- [x] **T000** 受损面全量量化：两文件 209 行受损（lessons-learned 153 /
      CHANGELOG 56），U+FFFD 915 个、NUL 33 个
- [x] **T000b** 修掉 38 个确定性 NUL（丢失的 ASCII `0`）：
      spec 037 spec/plan/tasks、spec 014 tasks、PRD、TDD、
      lessons-learned、CHANGELOG
- [x] **T000c** 取证损坏机制（两代叠加：第一代有损解码 → U+FFFD；
      第二代 GBK 误读 → `锟?` / `搂`，会掩盖第一代）
- [x] **T000d** 取证可恢复性：NUL 可判、GBK 往返可判 15 行、
      git 历史匹配 59 行但**同样已损坏**（无净收益）、
      字节级还原**不可能**（U+FFFD 是信息销毁）
- [x] **T000e** 落地工具 `scripts/test-infra/scan-encoding-damage.py`
      （索引式匹配，10 秒/文件，退出码可当门禁）
