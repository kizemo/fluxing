# tasks 079 - 修复仓库剩余的 C 转义损坏

## T1 盘点

- [x] T1.1 扫描全仓 tracked 文本文件，分离三类损坏：U+FFFD / NUL / C0 控制字节 / GBK 往返
- [x] T1.2 确认真损坏只落在 10 个文件、72 行（其余命中是扫描器对正常中文的误报）
- [x] T1.3 排除 `output/data/**`、`librime/**`、`release/**`（约束：不改）

## T2 标定损坏模型

- [x] T2.1 查每个受损文件的 git 历史拓扑，记录每个文件能否找到干净祖先
- [x] T2.2 用有干净祖先的文件（PRD.md / TDD.md / spec 001 tasks.md）比对，
      确认"控制字节吞掉字母"这一规律
- [x] T2.3 补全六个字母的映射：`a`(BEL) `b`(BS) `t`(TAB) `v`(VT) `f`(FF) `r`(CR)
- [x] T2.4 逐条核对还原后语义唯一（无第二种读法）

## T3 工具修正

- [x] T3.1 `apply-line-rewrites.py`：CR 数允许按本次替换实际移除的 CR 字节数变化
- [x] T3.2 `verify-encoding-repair.py`：CR 核对改用同一条规则
- [x] T3.3 `scan-encoding-damage.py`：GBK 往返结果必须在文档字符集内才采信
- [x] T3.4 新增 `scripts/test-infra/test_encoding_toolchain.py`

## T4 生成映射并写入

- [x] T4.1 按规则从 HEAD 基线生成 10 份 `rewrite-map-*.json`
- [x] T4.2 生成 `damage-inventory.md`（逐行 damaged → restored 对照）
- [x] T4.3 用 `apply-line-rewrites.py` 写入全部 10 个文件

## T5 复检：发现扫描器的 TAB 盲区

- [x] T5.1 复检时发现 `014/plan.md` 第 15 行仍有 TAB 残留 → 定位到 `0x09` 被排除在 C0 判定外
- [x] T5.2 全仓普查 261 个 tracked Markdown 的行内 TAB：44 处候选
- [x] T5.3 逐条人工判定三类：真损坏 19 / 合法 TSV 9 / 故意展示损坏的对照行 22
- [x] T5.4 用 git 历史交叉验证 15 处新增损坏的还原形态（`restored=False`、
      `literal=False` ⇒ 无干净祖先，只能按损坏模型机械还原）
- [x] T5.5 记录遗留项：spec 078 的 CHANGELOG 门禁因 0.21.0.2 条目新增而失效

## T6 给扫描器补 TAB 类判定

- [x] T6.1 `scan-encoding-damage.py` 新增 `inline_tab_damage()`：按文档上下文判定
      （行内 TAB / 行首吞字 / TSV 块 / inline code / fenced code / `- OLD:` 对照行）
- [x] T6.2 单独计入 `tab=` 统计与退出码，报告里单列 TAB 损坏段落
- [x] T6.3 生成 4 份新 map（lessons-learned / spec015-tasks / spec037-tasks / changelog），
      补齐 `rewrite-map-spec014-plan.json` 的第 15 行
- [x] T6.4 用 `apply-line-rewrites.py` 写入 15 处补修
- [x] T6.5 修正 applier 的行尾保持：001/tasks.md、016/spec.md、015/tasks.md 三个
      LF 文件此前被写成了 CRLF（逐行比对 HEAD 与工作区发现）→ 复位到 HEAD 后重跑

## T7 修 verify 门禁

- [x] T7.1 基线改为：工作区与 HEAD 不同时取 HEAD，一致时才回退到最新提交的父提交
- [x] T7.2 比对改为 difflib 对齐；map 行号语义统一为"修复前基线行号"
      （与 `apply-line-rewrites.py` 的契约一致）
- [x] T7.3 改写区聚合：delete + insert 相邻时算一次改写，行数守恒时不要求额外声明
- [x] T7.4 scanner 脚本路径固定取工具自身所在仓库，被检查的仓库只经 `--repo` 传
- [x] T7.5 门禁在 spec 078 的树上重放：map 仍被正确解读（180 / 5 行精确吻合），
      唯一的 PROBLEM 是 scanner 如实报出 078 遗留的 5 + 4 处 TAB 损坏

## T8 验收

- [x] T8.1 14 个文件 `U+FFFD=0 / NUL=0 / C0=0 / tab=0`
- [x] T8.2 14 个文件 scanner 报 `damaged_lines=0`、`tab=0` 且 exit 0
- [x] T8.3 全仓 261 个 Markdown 的 TAB 类误报 = 0
- [x] T8.4 反向验证：5 个受损文件的 HEAD 版本放进临时仓库，scanner 精确报出
      5 / 5 / 1 / 4 / 4 处 TAB 损坏
- [x] T8.5 `test_encoding_toolchain.py` → 11 组 30 条断言 ALL TESTS PASSED
- [x] T8.6 13 个目标文件 `verify-encoding-repair.py` → 全部 ALL CHECKS PASSED
- [x] T8.7 spec 078 的门禁能力未退化（在 078 的树上重放验证）

## T9 提交

- [x] T9.1 工具修正单独一笔提交
- [x] T9.2 文件修复 + spec 文档单独一笔提交
- [x] T9.3 推送 `kizemo/Fluxing`
