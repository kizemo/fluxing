# 078 · plan — 逐行重写受损历史文档

## 1. 技术路线

不追求"自动恢复"，追求**可审计的人工重写**：

```
受损行  →  抽取 ASCII 骨架（事实锚点）
        →  在 git 基线里定位同一条干净英文段落
        →  人工按基线措辞重写中文片段
        →  字节级写回（保持 BOM / CRLF）
        →  扫描器复验：U+FFFD = 0, NUL = 0, 行数不变
```

## 2. 工具

### 2.1 `scripts/test-infra/scan-encoding-damage.py`

- 输入：仓库相对路径
- 行为：
  1. 字节读入 → 严格 UTF-8 解码（失败即报，不静默替换）
  2. 按 `\n` 切行（**不用 `str.splitlines()`** —— 它会额外按 `\x0b \x0c \x85
     U+2028 U+2029` 切，导致行号与 git / 编辑器对不上）
  3. 标出含 `U+FFFD` 或 `NUL` 的行
  4. 对每条受损行抽取 ASCII 锚点（连续可打印 ASCII ≥ 8 字符）
  5. 用**索引**（anchor → 历史行）而非全量扫描做匹配：
     O(受损行) 而不是 O(受损行 × 全部历史行) —— 后者是 150 × 1,000,000 次比较，
     实测跑不完（先做了这个坑，见 §5）
  6. 产出 markdown 报告：受损行 → 历史候选 → 匹配置信度
- 退出码：仍有损坏 = 1，干净 = 0（可直接当门禁用）

### 2.2 检测口径

**不能只用 `EF BF BD` 计数。** 第二代 GBK 误读会把 `U+FFFD` 变成 `锟?`
（`EF BF 3F`），把 `§` 变成 `搂` —— 字节扫���看不见它们。
正确口径：**行内含非 ASCII，且 `line.encode('gb18030').decode('utf-8')` 成功**
→ 该行是 GBK 误读形态，损坏；解码失败 → 该行是干净 UTF-8。

## 3. 逐行重写流程

对每条受损行：

1. 从基线取同一条的英文段落（`git show 096c6e0:.specify/memory/lessons-learned.md`）
2. 对照当前行的 ASCII 骨架，确认事实一致（版本号 / 计数 / 文件名 / 常量）
3. 只重写被毁掉的中文片段，英文与代码标识符原样保留
4. 字节级写回：`[System.IO.File]::ReadAllBytes` → 字符串替换 →
   `[System.IO.File]::WriteAllBytes`（保住 BOM 与 CRLF，见 L01 / L09）
5. 写完立刻跑扫描器复验

## 4. 写回纪律

- **不要用 `Get-Content` / `Set-Content` / `Out-File`**（PS 5.1 按 ANSI 处理 → 毁 CJK）
- 不要用 `str.splitlines()` 重新拼行（见 §2.1）
- 行尾：lessons-learned 全 CRLF + BOM；CHANGELOG 全 CRLF + 无 BOM
- 一行一改，改一行验一行；不要批量正则替换（会把正常行也改坏）

## 5. 本轮踩过的坑（避免重做）

| 坑 | 现象 | 正解 |
|---|---|---|
| O(n²) 匹配 | 150 受损行 × 100 历史版本 × 10k 行 ≈ 1.5 亿次比较，跑不完 | 先建 anchor→行 的倒排索引 |
| `splitlines()` 切行 | 行号与 git 差好几十 | 只按 `\n` 切 |
| 控制台显示乱码 | 满屏 `�`，误以为文件是 GBK | **控制台代码页是 GBK，中文永远显示成乱码**；判损坏只认字节（严格 UTF-8 解码 / `EF BF BD` 个数 / `0x00` 个数） |
| 统计脚本用 `-like '?? *'` | 未跟踪文件数虚报 265→1,144 | `?` 在 `-like` 里是通配符，必须用 `Substring(0,2) -eq '??'` |

## 6. 验证

```powershell
# 1) 扫描器（退出码 0 = 干净）
python scripts\test-infra\scan-encoding-damage.py --path .specify\memory\lessons-learned.md
python scripts\test-infra\scan-encoding-damage.py --path CHANGELOG.md

# 2) 字节级复验（strict UTF-8 / NUL / C0 / C1 / 行尾）
$b = [System.IO.File]::ReadAllBytes($f)
$strict = New-Object System.Text.UTF8Encoding($false, $true)
$null = $strict.GetString($b)      # 不抛 = 合法 UTF-8
```

## 7. 回滚

- 重写只动文本，不动任何代码路径
- 提交粒度：**一个文件一个 commit**（`docs(fluxing): ...`），
  便于逐条 review 与单文件回退（`git revert <sha>`）
