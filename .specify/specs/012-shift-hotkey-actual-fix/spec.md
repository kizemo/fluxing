# 012 · Spec — Shift 切换中英 / 候选上屏 真正生效

> 范围：填补 spec 005 中"Shift_L / Shift_R 切换中英 + 上屏第 2/3 候选"
> 这部分在 0.18.x 的实现里**没有接通**的缺陷。
>
> 这是 spec 005 的"复盘 + 修复"切片，不是新增能力。验收通过的
> 行为与 spec 005 §1.1 表格完全一致。
>
> **范围限制（用户确认，2026-06-30）**：本 spec **只**改用
> 左 Shift / 右 Shift 单键（Shift_L / Shift_R）实现中英切换与
> 第 2/3 候选上屏。**不**引入 `Shift+l` / `Shift+r`（按住 Shift
> 再按字母）的组合键路径——用户明确表示"只是测试用"，不期望
> 该路径在最终产品中保留。

## 0. 上下文

- spec 005（v2 候选热键）在 design.md §2.2 写了 6 条 `key_binder/bindings`，
  意图让 Shift_L/R 单键接管"中英切换 + 上屏第 2/3 候选"。
- 实际 `output/data/default.yaml` 里这 6 条全部（或几乎全部）**没生效**，
  导致本机安装 Fluxing 后用户报告"按 Shift 切不动中英、也不能上屏第 2/3 候选"。
- 调试日志（`log/rime.weasel.DY-DELL.duanyi.log.*.20260629-215458.*.log`）
  明确记录：
  ```
  E key_event.cc:69  parse error: unrecognized modifier 'shift'
  ```
- `.specify/memory/lessons-learned.md` L04 错误地记录
  "`accept: shift+l`（小写）also works"——与 librime 1.13.1 源码
  `src/rime/key_table.cc:7` 的 `modifier_name[]` 表（区分大小写、首字母大写）
  不一致。这是 spec 005 design.md 也写成小写的源头。

## 1. 产品视角（PRD 段）

### 1.1 目标

完成 spec 005 §1.1 表格里 "Shift_L / Shift_R 单键 → 切中英 + 选 2/3 候选"
的承诺，让本机 `0.18.x` 用户实际按 Shift 行为符合表格。

**只**使用单键 Shift_L / Shift_R；不引入 Shift+l / Shift+r 组合键路径。

### 1.2 用户故事

- **US1-A** [P1]：用户在中文输入状态、候选词已弹出时按 `Shift_L`，
  期望：上屏第 2 候选，输入栏上屏该候选词（搜狗拼音习惯）。
- **US1-B** [P1]：用户在中文输入状态、无候选时按 `Shift_L`，
  期望：切到英文状态，状态指示器变 `A`。
- **US1-C** [P1]：同上对 `Shift_R` / 第 3 候选 也成立。
- **US1-D** [P2]：WeaselServer / WeaselDeployer / WeaselSetup 启动日志不再出现
  `unrecognized modifier 'shift'` 和 `invalid key binding` 警告。

### 1.3 验收（GWT）

- Given 全新安装 Fluxing 0.18.x
- When 在 notepad 中文输入，候选词已弹出，按 `Shift_L`
- Then 第 2 候选上屏，候选窗关闭，输入栏显示该候选词

- Given 中文输入无候选
- When 按 `Shift_L`
- Then 输入法切到英文，状态栏 `A`

- Given WeaselServer 进程启动
- When 检查 `log/rime.weasel.*.INFO.*.log`
- Then 不再出现 `parse error: unrecognized modifier`、
      `invalid key binding #6/#7/#9/#10`（之前小写 shift+l/r 报的错误）

## 2. 技术视角（TDD 段）

### 2.1 改动文件

| 文件 | 改动 |
|---|---|
| `output/data/default.yaml` | `key_binder/bindings` 段改 4 行 + 删 4 行（共 8 行） |
| `.specify/memory/lessons-learned.md` | 勘误 L04 的 "lowercase form also works" 段；新增 L16 |
| `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` | **需要**修订断言以匹配本 spec 的范围（仅 Shift_L/R，不含 Shift+l/r） |

### 2.2 default.yaml 改动清单

`key_binder/bindings` 段当前共有 8 条 shift 相关 binding：
- line 231-235 段（has_menu → send 候选上屏）4 条
- line 254-258 段（always → toggle ascii_mode 切中英）4 条

8 行 → 4 行（净 −4 行）：

| 当前 line | 现状 | 处置 | 原因 |
|---|---|---|---|
| 231 `accept: Shift_L, send: 2` | parse OK，运行时永不匹配 | **改**为 `Shift+Shift_L, send: 2` | TSF 发 `{Shift_L, SHIFT_MASK}`，当前 binding 是 `{Shift_L, 0}` |
| 232 `accept: Shift_R, send: 3` | 同上 | **改**为 `Shift+Shift_R, send: 3` | 同上 |
| 234 `accept: Shift+l, send: 2` | parse OK，运行时匹配 `Shift+l` 事件（按 Shift 不松 + L） | **删** | 用户明确不要组合键路径 |
| 235 `accept: Shift+r, send: 3` | 同上 | **删** | 同上 |
| 254 `accept: Shift+l, toggle: ascii_mode` | parse OK，运行时匹配 `Shift+l` 事件 | **删** | 用户明确不要组合键路径 |
| 255 `accept: Shift+r, toggle: ascii_mode` | parse OK | **删** | 同上 |
| 257 `accept: Shift_L, toggle: ascii_mode` | parse OK，运行时永不匹配 | **改**为 `Shift+Shift_L, toggle: ascii_mode` | 与 231 对称 |
| 258 `accept: Shift_R, toggle: ascii_mode` | 同上 | **改**为 `Shift+Shift_R, toggle: ascii_mode` | 与 232 对称 |

**最终 4 行**（line 231-232 + line 257-258 段）：

```yaml
# 火流猩 v2: 单键 Shift_L / Shift_R — 候选词时上屏第 2/3 候选，无候选时切中英
# 注意：TSF 实际发的是 {Shift_L, SHIFT_MASK}，须写 Shift+Shift_L（不是 Shift_L）
- { when: has_menu, accept: Shift+Shift_L, send: 2 }
- { when: has_menu, accept: Shift+Shift_R, send: 3 }
# 候选词之外，always 段接管中英切换
- { when: always, toggle: ascii_mode, accept: Shift+Shift_L }
- { when: always, toggle: ascii_mode, accept: Shift+Shift_R }
```

**保留**：`ascii_composer/switch_key.Shift_L/R: noop`（继续让 ascii_composer
不接管，避免与 key_binder 双触发）。

### 2.3 为什么是 `Shift+Shift_L` 而不是 `Shift_L` / `shift+l`

librime 1.13.1 `KeyEvent::operator==`（`librime/src/rime/key_event.h:64`）
要求 keycode + modifier **全等**。

- TSF（`WeaselTSF/KeyEventSink.cpp:31`）在 `VK_SHIFT` 按下时强制
  `result.mask |= SHIFT_MASK`，所以传给 librime 的 key event 是
  `{keycode=Shift_L, modifier=SHIFT_MASK}`。
- librime `KeyEvent::Parse("Shift_L")` → `{keycode=Shift_L, modifier=0}`
  —— 两者不匹配。
- librime `KeyEvent::Parse("Shift+Shift_L")` → `{keycode=Shift_L, modifier=SHIFT_MASK}`
  —— 匹配。

> **L04 勘误**：L04 把"lowercase form"也算作 valid 是错的，需要在
> lessons-learned.md 明确：librime 1.13.1 key_binder 的 modifier 名字
> 必须是首字母大写（`Shift`/`Control`/`Alt`/`Super`/`Hyper`/`Meta`/
> `Lock`/`Mod2..Mod5`/`Release`）。

### 2.4 验证步骤

1. **单测修订**：`test\TestDefaultHotkeys\TestDefaultHotkeys.cpp` 中
   关于 `Shift+l` / `Shift+r` 的断言（line 33-40、44-45）需要删掉或
   改写为本 spec 范围——本 spec **只**测 `Shift+Shift_L` / `Shift+Shift_R`。
   然后跑：
   ```
   test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe output\data\default.yaml
   ```

2. **构建并部署**：
   ```
   xbuild.bat weasel installer
   ```

3. **日志验证**：
   重启 WeaselServer，检查 `log/rime.weasel.*.INFO.*.log` 不再出现
   `parse error: unrecognized modifier 'shift'` 与
   `invalid key binding #6/#7/#9/#10`。

4. **手动验证**：
   - 候选词时按 `Shift_L` → 第 2 候选上屏
   - 无候选时按 `Shift_L` → 切英文
   - 切换 `Shift_R` 同样成立

### 2.5 风险

- **R1**：TSF 实际发给 librime 的 key event mask 严格等于 `SHIFT_MASK`，
  `Shift+Shift_L` 的 binding mask 也是 `SHIFT_MASK`（位或），匹配成功。
- **R2**：WeaselServer 启动后才会加载 default.yaml。改完需要重装或
  `WeaselDeployer.exe /deploy` 让 rime engine 重新 initialize。
- **R3**：用户 `default.custom.yaml`（在 RimeUserDir）目前只有 schema_list patch，
  不会覆盖 key_binder。无需协调。
- **R4**：删除原 `accept: Shift+l, send: 2` 等 4 条后，**该路径在
  reinstall / 升级后永久消失**。用户确认这是 intended。

## 3. Out of scope

- 不动 `weasel.yaml`（preedit 样式、字体、配色）
- 不动 `*.schema.yaml`（speller/algebra）
- 不动 RIME 引擎 / librime 子模块
- 不为不同方案做独立 key_binder patch
- 不修改 `output/install.nsi`（已通过 0.18.4.0 的 L14 修复）
- **不**为 `Shift+l` / `Shift+r` 任何组合键路径保留或新增配置
- 不发版（只发一个 commit + 必要的 tag，按本仓库 §3.4 约定，
  本地打 lightweight tag `v0.18.4.1` 仅作占位；是否推到 kizemo 由用户决定）

## 4. 本 spec 自身完成度

- [ ] T001 改 `output/data/default.yaml`：改 4 行 + 删 4 行（line 234/235/254/255 删除）
- [ ] T002 修订 `test\TestDefaultHotkeys\TestDefaultHotkeys.cpp`：删除 / 改写 Shift+l/r 相关断言
- [ ] T003 `test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe` 重新编译 + 全 PASS
- [ ] T004 勘误 `.specify\memory\lessons-learned.md` L04（标错 + 解释）
- [ ] T005 新增 `.specify\memory\lessons-learned.md` L16（modifier 大小写）
- [ ] T006 `xbuild.bat weasel installer` 重新构建 + 部署
- [ ] T007 重启 WeaselServer + 日志确认无 `unrecognized modifier 'shift'`
- [ ] T008 commit `fix(fluxing): spec 012 — wire Shift_L/R in key_binder (single-key only)`