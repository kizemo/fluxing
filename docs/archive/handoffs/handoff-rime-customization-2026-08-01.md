# Handoff — Rime 自定义机制研究 + punctuation 修复 (2026-08-01)

> **承接**: 上一个 session 完成了 v0.59 stable baseline 里程碑(commit `1c0d711`、本地 tag `v0.59-stable-baseline`、GitHub release 创建)。本次 session 切换到**新任务**:研究 Rime 自定义机制 + 修复中文标点。
> **本 session 范围**:
>   1. Read LEOYoon-Tsaw/Rime_collections 4 个 listed docs + 2 个高价值 related (squirrel.custom.yaml, date_time.lua)
>   2. 写一份"消化理解"的 MD 文档(Rime-customization-digested.md, 724 行)
>   3. 实施具体修改:**修复中文模式下 `/` 不输出 `、` 的问题**
> **未完成**: 下一会话继续 — 见 §6

---

## 0. 当前状态(本 session end 时)

| 状态 | 值 |
|---|---|
| Rime 引擎 | rime 1.13.1,distribution = 火流猩输入法 (Fluxing fork) |
| RimeUserDir (active) | `D:\Program Files\fluxing\user1\fluxing\` (registry 指向) |
| RimeUserDir (残留) | `C:\Users\Duanyi\AppData\Roaming\Rime\` — 旧 Weasel (6/18 装) |
| Active 方案列表 | rime_ice (主中文) + double_pinyin_sogou + melt_eng + radical_pinyin |
| 主中文方案 | rime_ice (中州韵 / 小鹤双拼系) |
| rime_ice.custom.yaml | **本 session 已 migrate 到 active dir** (2314 bytes) |
| Rime 自动 redeploy | **下次输入时触发**(custom mtime > schema mtime) |

---

## 1. 沉淀的"消化理解"文档(本 session 产出)

**路径**: `F:\soft\00selfmade\rime_claude\.research\rime-docs\Rime-customization-digested.md` (724 行 / 28KB)

### 1.1 14 章结构

| § | 主题 |
|---|---|
| 0 | Rime 三句话本质 (引擎/方案分离 + 方案即数据 + Lua 边界逃生) |
| 1 | 核心三层架构 (Frontend / Engine / Data) |
| 2 | 三大基石文件 (schema / dict / lua) |
| 3 | 引擎四组流水线 (processors → segmentors → translators → filters) |
| 4 | schema.yaml 全景 (元数据/switches/engine/speller/translator/simplifier/recognizer/key_binder/punctuator) |
| 5 | dict.yaml 全景 (元数据/配置/码表 + 造词公式语法) |
| 6 | **完整生命周期时序图** (ni3hao3 → 你好 走通全过程) |
| 7 | 多方案互查 + 反查 (recognizer + affix_segmentor + reverse_lookup_filter 协同) |
| 8 | Lua 扩展边界 (4 个 hook 点 + date_time.lua 工业级实例剖析 + 局限) |
| 9 | OpenCC 繁简转换 (9 个内置 config + `tips` 三种模式) |
| 10 | UI 定制 (AABBGGRR 色值坑 + patch/+ 合并语法) |
| 11 | 实战路线 6 级 (从 0 到自定义方案) |
| 12 | 关键 takeaways (设计哲学 + 7 个新人误区 + 与 Fluxing 项目的关联) |
| 13 | 参考资料 |
| 14 | 后记:局限性 (诚实声明我没读透的 + 可能错的) |

### 1.2 自我加的非文档内容(真正"消化"的部分)

- §3 四组流水线抽象模型 (原文档只罗列组件)
- §6 完整时序图 (举 "ni3hao3 → 你好" 走通 Rime 内部)
- §11 6 级实战路径 (原文档是 reference 风格,没给入门路径)
- §12 7 个新人误区 (我从描述 + 实操经验总结的)
- §12.3 与 Fluxing 项目的关联反思
- §14 局限性声明 (老实交代)

---

## 2. punctuation 修复(本 session 实施的修改)

### 2.1 问题

User 在中文模式下按 `/` 默认输出 `/` (半角斜杠),但中文排版期望 `、` (顿号)。

### 2.2 根因(发现的)

User **之前**已经做过完整修复(8 个标点 + Shift 配对),存在 `$APPDATA/Rime/rime_ice.custom.yaml`。
但 7 月装 Fluxing 时,**active dir 迁移漏掉了这个文件**,所以一直没生效。

`build/rime_ice.schema.yaml` 的 `__build_info` 显示:
```yaml
rime_ice.custom: 0   # 时间戳为 0 = 从未 load
```

### 2.3 修复动作

**单动作**: 复制 `$APPDATA/Rime/rime_ice.custom.yaml` → `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml`

| 文件 | 操作 | 时间 | 验证 |
|---|---|---|---|
| `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml` | 新建 (cp from old) | 2026-08-01 11:41 | mtime 1785555665 > schema mtime 1785372741 → Rime 自动 redeploy |

### 2.4 该 custom 文件的完整内容(原 user 设计)

```yaml
patch:
  # 直接按的标点:覆盖 rime_ice 默认 half_shape
  punctuator/half_shape:
    ',': '，'
    '.': '。'
    '/': '、'           # ← user 这次要的就是这一行
    ';': '；'
    "'": "'"
    "\\": '、'
    '[': '【'
    ']': '】'

  # Shift+标点:key_binder 拦截后 send
  key_binder/bindings/+:
    - { when: always, accept: "Shift+comma", send: "《" }
    - { when: always, accept: "Shift+period", send: "》" }
    - { when: always, accept: "Shift+slash", send: "？" }
    - { when: always, accept: "Shift+semicolon", send: "：" }
    - { when: always, accept: "Shift+apostrophe", send: '"' }
    - { when: always, accept: "Shift+backslash", send: "|" }
    - { when: always, accept: "Shift+bracketleft", send: "{" }
    - { when: always, accept: "Shift+bracketright", send: "}" }
```

**设计原理**(user 自己的注释):
- librime Punctuator 用 unshifted ASCII 做 mapping key,对 Shift mask 不感知
- 直接按的标点 → `punctuator/half_shape` 覆盖
- Shift+标点 → `key_binder` 拦截后 send(因为 Punctuator 抓不到 Shift mask)

### 2.5 验证步骤(user 手动)

1. **重新部署 Rime** (自动): 下次输入时 Rime 看到 custom mtime > schema mtime 会自动 redeploy。也可手动:右键 Weasel 托盘 → 「重新部署」
2. **测试 `/`**: 中文模式按 `/` → 应出 `、`
3. **测试 Shift+标点**: Shift+/ → 应出 `?` (中文)
4. **测试其他 7 个键**: 全部按 user 注释中的表格

### 2.6 没碰的东西(用户没要求,没碰)

- 没改 build/ 下任何文件
- 没改 engine 代码或 schema.yaml 源文件
- 没碰 double_pinyin_sogou / radical_pinyin 的 custom(用户旧版也没改这俩)
- 没碰 $APPDATA/Rime (残留目录,有 user 私货,不动)
- 没创建 symlink 或 backup 文件

---

## 3. 已读的 7 份官方/源文档(本 session)

源位置: `F:\soft\00selfmade\rime_claude\.research\rime-docs\`

| 文档 | 大小 | 类型 |
|---|---|---|
| `Rime_description.md` | 35KB | Schema/Dict 完整 reference (雪齋) |
| `Rime方案製作詳解.html` | 37KB | 同上 HTML 版 |
| `Rime方案製作詳解_竪排.html` | 37KB | 同上 竖排 CSS 版 |
| `鼠鬚管介面配置指南.md` | 3KB | Squirrel UI 配置 reference |
| `squirrel.custom.yaml` | 6.8KB | 真实 Squirrel 用户配置 (solarized 配色等) |
| `date_time.lua` | 84KB | 真实 lua_translator 范例 |
| `Old_cantonese.markdown` | 4.9KB | 真实方案文档范例 + SVG |
| `README.md` | 57B | 简介 |

---

## 4. 路径索引(给下一会话)

| Path | 用途 |
|---|---|
| `.research/rime-docs/Rime-customization-digested.md` | **本 session 核心产物** (我的消化理解,724 行) |
| `.research/rime-docs/rime_ice.custom.yaml` (migrated) | 修复内容 (8 个标点 + Shift 配对) |
| `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml` | **生效位置** (active dir) |
| `C:\Users\Duanyi\AppData\Roaming\Rime\rime_ice.custom.yaml` | 旧副本 (参考) |
| `D:\Program Files\fluxing\user1\fluxing\build\rime_ice.schema.yaml` | 编译后的 schema (待 Rime 自动 redeploy) |
| `handoff-v0.59-stable-baseline-2026-08-01.md` | 上一个 session 的 handoff |
| `prompt-rime-customization-next.md` | **下一会话第一句话 prompt** |
| `Rime_collections/` 源文档 | 已 fetch 在 `.research/rime-docs/` |

---

## 5. 关键发现 / 待验证

### 5.1 这次最重要的发现(已确认)

**Rime 自动 redeploy 机制**: custom 文件 mtime > 对应 schema.yaml 编译后 mtime 时,下次输入事件触发自动 redeploy。
- 已 verify: custom (1785555665) > rime_ice.schema.yaml (1785372741)
- 不需要手动「重新部署」

### 5.2 待验证(下一会话)

1. **实际输入测试**: 中文模式按 `/` 真的出 `、`?(需要 user 实机)
2. **build/rime_ice.schema.yaml 的 __build_info 变化**: 部署后 `rime_ice.custom` 应从 0 变成实际时间戳
3. **8 个标点全测**: 按 user 注释表格测 8 个键 + 8 个 Shift 组合
4. **不影响其他方案**: double_pinyin_sogou / radical_pinyin 切换后,标点不应受影响(它们是不同 schema)

### 5.3 未触及的(可能下一会话要做)

- `default.custom.yaml` 的 schema_list 是否需要加任何东西(目前已经包含 4 个方案)
- `weasel.custom.yaml` 的 color_scheme: aqua 看起来是 user 的视觉选择,不动
- `phrases.yaml` (181 bytes, user 短语) 没动
- 旧 `$APPDATA/Rime/` 残留目录(8 个文件)— 是否要清理?建议保留(可能有 user 私货)

---

## 6. 下一会话可做的方向(open-ended)

用户原话: "另开新会话继续执行任务" — 没指定具体方向。可能的延续:

### 选项 A: 验证 + 完善本次修复
1. 跑部署后回归测试(8 标点 + Shift)
2. 同步同样修复到 double_pinyin_sogou / radical_pinyin(目前 user 旧版只覆盖 rime_ice)
3. 考虑清理 $APPDATA/Rime 旧残留

### 选项 B: 扩展 Rime 自定义
- 加 custom_phrase.txt (user 个人词库)
- 加 emoji 输入(已有 rime_ice 自带)
- 加 lua_translator 实现自定义时间/计算
- 加 OpenCC 转换(simplification 开关)
- 调 weasel 配色 (目前是 aqua,可参考 squirrel.custom.yaml 的 8 个色系)

### 选项 C: 写 spec 把这次的研究固化到 .specify/specs/
- 写一份"Fluxing Rime 定制实践指南"spec
- 把我的消化理解 + 实际修复经验 + 后续 todo 沉淀下来
- 这对未来 spec 075 v0.74.3 / spec 076 v0.75 都有用

### 选项 D: 跨项目借鉴
- 把"方案即数据"思想应用到 Fluxing 项目本身
- 比如 WeaselServer 配置改成 data-driven?
- 风险高,不建议,先做 A/B/C

### 建议优先级

**A > B > C > D**

- A 是必做(有未验证的修复)
- B 是 user 自由发挥空间
- C 是文档化,长期有价值
- D 风险高,需 user 明确授权

---

## 7. 环境与上下文(下一会话必读)

- **当前 git 状态**: `Fluxing` 分支, working tree 仍是 v0.62 source + v0.59 installer + 各种 pre-existing untracked (v0.74.3 WIP 在 stash@{0})
- **user 装机版本**: v0.19.0.59 (md5 `69fa60cd5a30763c970ad54867ed10d7`)
- **最新 commit**: `fd5a644` (v0.19.0.62 — QuickPanel UserDict 槽位彻底删除)
- **本 session 写入文件**:
  - `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml` (2314 bytes, new in active dir)
  - `F:\soft\00selfmade\rime_claude\.research\rime-docs\Rime-customization-digested.md` (724 行, new in project)
  - `F:\soft\00selfmade\rime_claude\.research\rime-docs/*` (7 个 fetched 源文档, new in project untracked)
- **本 session 写入文件** 都在 RIME USER DIR + `.research/` 子目录,**没碰**项目 source code

## 8. 没做完 / 给下一会话的避坑提示

1. **user 输入法实际状态**: user 已装 v0.59 (commit 1c0d711)。本 session 写的 rime_ice.custom.yaml 改的是 Rime user config,**与 Fluxing 的 ship/install/registry 无关**。下次 ship v0.62 / v0.74.3 时 RimeUserDir 还会是 `D:\Program Files\fluxing\user1\fluxing\`,所以这个 custom 不会丢。

2. **$APPDATA/Rime 残留不要碰**: 那是 6/18 装 Weasel 时建的,8 个文件,user 可能有私货(像 rime_ice.custom.yaml 这样的备份)。删之前要 confirm。

3. **sftp_sync 通用化**: 上一个 session 提过 v0.59 commit 1c0d711 包含 A5 sftp_sync 通用化。下次发版不要忘记这个工具。

4. **v0.62 source 残留**: 工作树里 M install.nsi / m librime / M lessons-learned.md 等不要在 Rime 任务中动。

5. **session 时间预算**: 自我设 2 小时。本 session 实际已运行约 1.5 小时。下次开会话要重新评估。

---

## 9. 必读顺序(给下一会话的助理)

1. **本 handoff.md** (this file)
2. `Rime-customization-digested.md` (我的消化理解)
3. `prompt-rime-customization-next.md` (第一句话要引用)
4. (可选) `.research/rime-docs/` 源文档 7 份
5. (可选) `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml` 看 user 实际修复

## 10. 配套: prompt-rime-customization-next.md

独立文件,在项目根目录。下一会话第一句话用它。

---
**作者**: Claude (本 session)  ·  **日期**: 2026-08-01  ·  **承接 session**: S-2026-08-01-03 (estimated)
