# 081 - 快捷键「可触发性」验收与死绑定修复 (spec 077 之后的第 7 轮)

> Scope: 把「快捷键到底能不能被触发」变成**可判定、可回归**的门禁，并修掉当前配置里
> 6 条**永远不可能触发**的 `key_binder` 绑定。
>
> 关联: ADR 0009（L131 记录了根因）、L113（同一批绑定的前一次翻车）、
> spec 077（Shift 键被前端吞掉的那次改造）、`Rime-设置说明书-重点快捷键.md` §9/§10。

## 0. Background

- 用户实机反馈「快捷键有问题」。上一轮沙箱复验把范围缩到「快捷键」后，
  第 6 轮用**产品自己的** `ConvertKeyEvent()` 逐条判定，定位到 6 条死绑定：

  | # | 来源 | `accept:` | 想要的 keycode | 前端实际发 | 功能是否有可达的孪生键 |
  |---|---|---|---|---|---|
  | 1 | `output/data/default.yaml` | `Control+Shift+9` | `0x39` (`9`) | `0x28` (`(`) | 有（`Control+Shift+numbersign`） |
  | 2 | `output/data/default.yaml` | `Control+Shift+0` | `0x30` (`0`) | `0x29` (`)`) | 有（`Control+Shift+dollar`） |
  | 3 | `user-custom/rime_ice.custom.yaml` | `Shift+apostrophe` | `0x27` (`'`) | `0x22` (`"`) | **无** |
  | 4 | `user-custom/rime_ice.custom.yaml` | `Shift+backslash` | `0x5C` (`\`) | `0x7C` (`|`) | **无** |
  | 5 | `user-custom/rime_ice.custom.yaml` | `Shift+bracketleft` | `0x5B` (`[`) | `0x7B` (`{`) | **无** |
  | 6 | `user-custom/rime_ice.custom.yaml` | `Shift+bracketright` | `0x5D` (`]`) | `0x7D` (`}`) | **无** |

- 根因（ADR 0009）：TSF 前端对可打印键发的是**键产生的字符**，librime 用**精确比较**
  匹配 `(keycode, modifier)` ⇒ `accept:` 必须写**字符的键名**。
- 既有测试**全都测不出来**：
  - `TestDefaultHotkeys` 断言 yaml 里**存在** `Control+Shift+9`（第 5 行 PASS），
    不断言它**能不能被触发**；
  - `TestBindingResolution` 断言 `accept:` 能被**解析**，不解析「解析出来的那对
    (keycode, modifier) 前端发不发得出」。
- `user-custom/rime_ice.custom.yaml` 里的注释写着「保留的 4 条：send 是单个 ASCII 字符，
  librime 能解析，实际生效」——**这句是错的**，那 4 条从写下起就没生效过。

## 1. Product Angle (PRD section)

### 1.1 Goal

让「快捷键是否可用」变成**可判定的事实**：

1. 每一条 `key_binder` 绑定要么**可达**（前端能发出它要的 `(keycode, modifier)`），
   要么在 `known-dead-bindings.txt` 里**登记为已知缺陷**；
2. 新增死绑定 ⇒ 门禁变红（构建期拦住），不是等到用户投诉；
3. 当前这 6 条死绑定要么修好，要么从配置里删掉 —— 不留「以为能用其实不能用」的条目。

### 1.2 User Stories

- **US1-A** [P1]: 用户按 `Ctrl+Shift+9` 想切中英标点。
  **期望**：能切。（现状：按了没反应，因为前端发的是 `(` 不是 `9`。）
- **US1-B** [P1]: 用户按 `Ctrl+Shift+0` 想切简繁。
  **期望**：能切。（现状：同上，前端发的是 `)`。）
- **US1-C** [P1]: 用户按 `Shift+'` 期望得到 `"`（ASCII 双引号，绕过中文标点转换）。
  **期望**：得到 `"`。（现状：绑定死掉，走 punctuator，结果与期望不符。）
- **US1-D** [P1]: 用户按 `Shift+\` / `Shift+[` / `Shift+]` 期望得到 `|` / `{` / `}`。
  **期望**：分别得到。（现状：绑定死掉。）
- **US1-E** [P1]: 维护者改 `output/data/**/*.yaml` 的快捷键。
  **期望**：改完跑 `scripts\test-infra\run-test-suite.bat`，若新绑定不可达则**立即红**，
  并在输出里告诉维护者「前端实际发的是什么字符、哪些键名可以替代」。
- **US1-F** [P2]: 维护者在**沙箱**里复验已部署的输入法。
  **期望**：门禁读的是**部署后的** `build\*.schema.yaml`，不是仓库里的源 yaml。
- **US1-G** [P2]: 维护者改了 `librime` 的键名表。
  **期望**：`gen-keysyms.ps1 -Check` 报「键名表已过时」，门禁不会拿旧表糊弄过去。

### 1.3 Acceptance (GWT)

- Given `output/data/default.yaml` + `output/data/user-custom/rime_ice.custom.yaml`
  When 运行 `Release\TestHotkeyBindings.exe`
  Then 输出 `SUMMARY|bindings=33|reachable=33|dead=0|known_dead=0`，`RESULT|PASS`

- Given 同一份配置，其中一条绑定被改成 `accept: "Shift+semicolon"`
  When 运行同一命令（**不带**基线）
  Then 该条判为 `DEAD_KEYCODE`，`RESULT|FAIL`，退出码 1

- Given `test/TestHotkeyBindings/fixtures/negative-controls.yaml`（7 条故意写坏的绑定）
  When 运行 `Release\TestHotkeyBindings.exe --expectations <expect.txt> <fixture>`
  Then `mismatches=0` 且 `unused=0`，`RESULT|PASS`
  （证明门禁**能分辨**好坏，而不是「见谁都说坏」）

- Given 沙箱里已安装并部署完成的输入法
  When 沙箱门禁跑到 P10 阶段
  Then P10.1（坏数据对照）PASS；P10.2（部署后配置）PASS 且 `dead=0`；
  P10.3（服务器日志无 `invalid key binding`）PASS

- Given `librime/src/rime/key_table.cc` 被改动
  When 运行 `gen-keysyms.ps1 -Check`
  Then 退出码 1 并提示「键名表已过时」

### 1.4 Non-goals

- ❌ 不改 `WeaselTSF/KeyEvent.cpp` 的按键语义（ADR 0009 已否决该方向）。
- ❌ 不改 `librime/`（P3 子模块禁改）。
- ❌ 不在本阶段做「快捷键自定义 UI」（spec 009 / 050 另有归属）。
- ❌ 不用静态门禁去证明 `send:` 的**运行时效果**（`send: Shift+Left` 是否真的移光标）——
  那是 M3 真机验收的事。

## 2. Technical

### 2.1 门禁工具 `test/TestHotkeyBindings`

- 把产品自己的 `WeaselTSF/KeyEvent.cpp` **编进测试二进制**
  （`<ClCompile Include="..\..\WeaselTSF\KeyEvent.cpp" />`）⇒ 判定用的是产品逻辑，
  不是测试里重写的映射。
- 枚举 `vkey ∈ [0x08, 0xFE] × extended × shift × ctrl × alt × keyup`，
  调 `ConvertKeyEvent()`，得到「前端能产生的 (keycode, librime modifier) 全集」。
- 复刻两条前端规则：
  1. `WeaselTSF/KeyEventSink.cpp:44` —— Shift_L / Shift_R **被整段吞掉**，不计入可达集；
  2. `RimeWithWeasel.cpp:43 expand_ibus_modifier()` —— ibus 掩码 → librime 掩码
     （`RELEASE` 位 14→30、`SUPER` 10→26 等），工具内固定了这几个已知值做自检。
- 逐条解析 `accept:`（复刻 `librime/src/rime/key_event.cc:51 KeyEvent::Parse`），
  判定 REACHABLE / DEAD_KEYCODE / DEAD_ACCEPT / DEAD_SEND / DEAD_WHEN / DEAD_NO_ACTION。
- 键名表由 `gen-keysyms.ps1` 从 `librime/src/rime/key_table.cc` **机械生成**
  （1097 个键名 + 17 个修饰键），生成器自带 X11 规范映射自校验；`-Check` 模式做防漂移。
- **零额外运行时依赖**：`dumpbin /dependents` 只有 `KERNEL32.dll` + `USER32.dll`
  ⇒ 沙箱里可以单独跑，不需要 rime.dll。

### 2.2 配置层修法（ADR 0009）

| 现在 | 改成 | 理由 |
|---|---|---|
| `accept: "Shift+apostrophe"` | `accept: "Shift+quotedbl"` | 前端发的是 `"` (0x22) |
| `accept: "Shift+backslash"` | `accept: "Shift+bar"` | 前端发的是 `|` (0x7C) |
| `accept: "Shift+bracketleft"` | `accept: "Shift+braceleft"` | 前端发的是 `{` (0x7B) |
| `accept: "Shift+bracketright"` | `accept: "Shift+braceright"` | 前端发的是 `}` (0x7D) |
| `accept: Control+Shift+9` | **删除** | 与 `Control+Shift+numbersign` 功能重复且永远不触发 |
| `accept: Control+Shift+0` | **删除** | 与 `Control+Shift+dollar` 功能重复且永远不触发 |

### 2.3 沙箱门禁阶段 P10

见 `scripts/sandbox-gate/in-sandbox-claims.ps1` 的 `PHASE 3.5`：

- **P10.1** 坏数据对照（工具有判别力）
- **P10.2** 读**部署后**的 `user1\fluxing\build\*.schema.yaml` 逐条判定
- **P10.2b** 基线条数 == 实测已知缺陷条数（基线不许留过期条目）
- **P10.3** 服务器日志里不许出现 `invalid key binding`

## 3. Verification

| 层级 | 命令 | 判据 |
|---|---|---|
| 仓库 | `scripts\test-infra\run-test-suite.bat` | 全部 PASS（含两条新门禁断言） |
| 仓库 | `Release\TestHotkeyBindings.exe --expectations ... negative-controls.yaml` | `RESULT|PASS` |
| 仓库 | `Release\TestHotkeyBindings.exe --baseline ... output\data\default.yaml output\data\user-custom\rime_ice.custom.yaml` | `dead=0` |
| 仓库 | `powershell -File test\TestHotkeyBindings\gen-keysyms.ps1 -Check` | `OK: ... up to date` |
| 沙箱 | `run-verify.cmd -RepoRoot <repo>` | `P10.1/P10.2/P10.2b/P10.3` 全 PASS |
| 真机 | M3 的按键对照表 | 每条绑定都有「触发/未触发」的实测结论 |
