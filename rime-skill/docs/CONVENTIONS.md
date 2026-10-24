# CONVENTIONS：遵循了哪套约定

> 本技能同时参考了 `book-to-skill`（文档→技能的转换方法）与 `skill-forge`（技能质量原则）。
> 两者在 **frontmatter 键集**上冲突，本文说明本技能的选择与转换方法。

## 一、本技能遵循的约定：skill-forge / gbrain 约定

`skill-forge/docs/PRINCIPLES.md` § 1 规定了 8 条 frontmatter 原则，本技能逐条满足，
并用 skill-forge 自己的校验器验证为**零告警**：

```python
from scripts._lib.frontmatter import load, body_of, validate_skill_frontmatter, validate_skill_body
fm = load(skill_dir / "SKILL.md")
assert validate_skill_frontmatter(fm, skill_dir) == []
assert validate_skill_body(skill_dir / "SKILL.md", fm) == []
```

| 原则 | 本技能 | 状态 |
|---|---|---|
| 1.1 description ≤1024 字符、≤50 词、以 "Use when" 开头 | 110 字符 / 17 词 | 通过 |
| 1.2 含 `:` 或 `'` 时用 YAML 块标量 | `description: >-` | 通过 |
| 1.3 `triggers:` ≥3 条具体短语 | 7 条 | 通过 |
| 1.4 `mutating:` 显式布尔 | `true` | 通过 |
| 1.5 `name:` 小写 kebab-case | `rime-skill` | 通过 |
| 1.5b `name` == 目录名 | 一致 | 通过 |
| 1.6 `brain_first:` 显式声明 | `exempt` | 通过 |
| 1.7 `when_to_use:` 存在 | 有 | 通过 |
| 1.8 `version:` semver | `1.0.0` | 通过 |

body 侧 9 条原则（Iron Law / When to use / Phases / Common Rationalizations / Red Flags /
Verification / Anti-Patterns / Output Format / When it fails）全部落地，
标题采用**中英双语**（`## 危险信号（Red Flags）`）——既让中文读者顺眼，也让
skill-forge 的 gotcha 段落正则能命中。

## 二、冲突：skill-creator 的 quick_validate.py 只允许 5 个键

`skill-creator/scripts/quick_validate.py` 的允许集是：

```
allowed-tools, description, license, metadata, name
```

它会拒绝 `triggers` / `mutating` / `tools` / `version` / `when_to_use` / `brain_first`：

```
Unexpected key(s) in SKILL.md frontmatter: brain_first, mutating, tools, triggers, version, when_to_use
```

**这不是缺陷，是两套约定的分歧**：

- `skill-creator` 沿用 Anthropic 式的最小允许集；
- `skill-forge` / gbrain 市场约定用更丰富的键（`skill-forge` 自己的 SKILL.md 也带这些键，
  同样过不了 `quick_validate.py`）。

**本技能的选择：遵循 skill-forge**（用户明确点名），因为：
1. skill-forge 的 `validate_skill_frontmatter()` 是它验证清单里点名的门禁；
2. `brain_first` / `triggers` 是 gbrain 市场的路由约定，丢掉会削弱触发质量；
3. 实测 MiniMax Code 的技能加载器**容忍额外键**（它已装技能里就有 `disable-model-invocation`
   这类不在允许集里的键，且正常生效）。

## 三、若要迁到严格允许集

把 gbrain 专有键收进 `metadata`（它在允许集里），但要接受 skill-forge 校验器随之报缺键告警：

```yaml
---
name: rime-skill
description: >-
  Use when customizing a Rime 输入法 …
metadata:
  version: 1.0.0
  mutating: true
  brain_first: exempt
  when_to_use: >-
    Use when the user asks how to customize a Rime input method.
  triggers:
    - "rime 补靪 patch 怎么写"
    - "自定义 rime 快捷键"
  tools: [bash, read, write, edit, grep, glob]
---
```

代价：`validate_skill_frontmatter()` 会报 5 条缺键告警（`triggers` 等不再在顶层）。
**只有在目标宿主的加载器确实拒绝额外键时才做这个转换。**

## 四、其他约定

| 项 | 决定 |
|---|---|
| 技能名 | `rime-skill`（与目录名一致，见原则 1.5b） |
| `mutating` | `true` —— 技能会指导写 `.custom.yaml` 文件 |
| `brain_first` | `exempt` —— 本技能自足（chapters + references 离线可用），GBrain 只是深水区检索 |
| 章节预算 | technical + study：每章 2000–3000 tokens（book-to-skill 矩阵） |
| SKILL.md body | ≤150 行 / ≤1000 词（skill-forge 门禁）；实际 148 行 / 787 词 |
| 重内容位置 | `chapters/`（按需读）、`references/`（查表）、`docs/`（溯源）——渐进披露 |
| 链接写法 | 内部链接一律相对路径单段式；不引入上游的 GitHub 别名式 |
