# 082 - 裸 Shift 切换中英文（ADR 0010 上下文消歧）

> Scope: 让「单独按一下 `Shift` = 切中英 / 切回中文」在 **Fluxing** 里真正可用。
> 这**不是**一条 yaml 配置改动（那条路已被 ADR 0007 关死），而是在**服务端 Shift 状态机**
> 里按「候选窗开没开」做一次上下文消歧。
>
> 关联: ADR 0010（决策，Accepted）、ADR 0007（Shift 改走 IPC 状态机）、
> ADR 0009（TSF 前端发的是字符不是物理键）、spec 077（裸 Shift 选第 2/3 候选）、
> spec 081（快捷键可触发性门禁）、L18 / L19 / L21 / L148。

## 0. Background

### 用户要什么

「单独按一下 Shift 切中英文」—— 搜狗、微软拼音都这么做，用户在 Fluxing 上按了没反应。

### 为什么 yaml 路径是死的

`WeaselTSF/KeyEventSink.cpp:44-58` 把 `Shift_L` / `Shift_R` **整段吃掉**：它把两半
转发给 Server 的 IPC（`m_client.ShiftDown` / `ShiftUp`），然后 `*pfEaten = TRUE` 返回，
事件**永远不会**到 `m_client.ProcessKeyEvent`，因此**永远不会**到 librime。

后果（ADR 0009 已记）：

- **任何 `key_binder` 绑定都不可能写 Shift。** `accept: "Shift_L"`、`accept: "Shift_R"`、
  `accept: "Shift+Shift_L"` 是**构造上就死**，不是碰巧不匹配。
- **`ascii_composer/switch_key` 里的 Shift 条目也不可能触发。** `switch_key` 是在
  librime **内部**匹配的，而 librime 永远看不到这个键。

所以「往 `ascii_composer/switch_key` 里加一行 `Shift_L: set_ascii_mode`」这条
**一行答案**在本 fork **不可达**。v0.21.0.1 时它还是可达的 —— 这也是设置说明书至今
仍在描述它的原因；spec 077 把这条路关了。

### L18 的历史阻塞已失效

L18 记过「裸 Shift 很危险」：yaml 里的 `accept: Shift+...` 会**同时匹配 `Shift+X` 的
release 半边**（modifier 是 `Release` 而不是 `Shift`），于是 `Shift+X` 会触发那条 Shift 绑定。

那个根因 lives 在**前端把 Shift 转给 librime** 的年代。ADR 0007 之后前端**根本不转**，
所以 **release 误匹配这一类 bug 不可能再发生** —— L18 记的阻塞不再适用。

### 但裸 Shift 已经被占用了

ADR 0007 / spec 077 给裸 Shift 派了活。`RimeWithWeaselHandler::ShiftUp`
（`RimeWithWeasel/RimeWithWeasel.cpp:1765-1781`）在 `downRecorded && !interveningKey`
时调 `rime_api->select_candidate(idx)`，`idx = 1`（左 Shift）/ `2`（右 Shift）——
「选第 2 / 第 3 候选」。0.21.0.1 发布，**在日常使用中**。

于是两个功能**撞车**了。

### 但这个撞车不是硬冲突

如果「选第 2 个候选」在**没有菜单**时也做点可见的事，那就是硬冲突。它没有：

- `Context::Select()` 在 composition 为空时直接 `return false`
  （`librime/src/rime/context.cc:113-115`）。
- `Segment::GetCandidateAt(index)` 对越界 index 返回 null，
  所以 `select_candidate(idx=1)` 配一个候选也是空操作。

⇒ **「没有候选菜单时选第 2 候选」本来就是空操作**。这个功能恰好占住了
「切换中英文」最有用的那个状态，而在那里它什么都不做。

另外两个事实让拆分是安全的：

- `interveningKey` 守卫（`RimeWithWeasel/RimeWithWeasel.cpp:319-327`）由 Shift down 与
  Shift up 之间**任何非 release、非 Shift 的 keydown** 置位 ⇒ `Shift+X` 永远到不了裸 Shift 分支。
- `ShiftUp` 无条件重置状态（`s = ShiftState{}`）⇒ 不会跨周期泄漏脏状态。

## 1. Product Angle (PRD section)

### 1.1 Goal

把「裸 Shift 切中英」从**做不到**变成**做得到**，而且**不牺牲**任何一个已发布的功能：

1. 候选窗**没开**时，裸 Shift 切换当前会话的中/英；
2. 候选窗**开着**时，裸 Shift **保持现状**（选第 2 / 第 3 候选）；
3. 两者由**一个可观测的谓词**分开（菜单开没开），**不靠时序猜测**，**没有双击逃生门**；
4. 行为**可关**：配置开关缺省即开，显式写 `false` 才关。

### 1.2 User Stories

- **US1-A** [P0]: 用户在中文模式下，候选窗没开，单独按一下 `Shift`。
  **期望**：切到英文模式。再按一下切回中文。
- **US1-B** [P0]: 用户在**英文模式**下，单独按一下 `Shift`。
  **期望**：切回中文。（英文模式下 librime 仍保留 1 个候选的 raw 段，
  所以判定必须看 `is_ascii_mode`，不能只看 `is_composing`。）
- **US1-C** [P0]: 用户打了拼音，候选窗开着，单独按一下 `Shift`。
  **期望**：**行为不变** —— 选第 2 个候选（spec 077，0.21.0.1 起在用）。
- **US1-D** [P0]: 用户打 `Shift+X`。
  **期望**：正常输入 `X`（或按该组合键绑定的行为），**绝不**触发中英切换。
- **US1-E** [P1]: 用户在两个窗口各开一个会话，在窗口 A 里按裸 Shift。
  **期望**：**只切 A**（与 yaml 的 `Shift+space` 同作用域），窗口 B 不受影响。
- **US1-F** [P1]: 维护者想把裸 Shift 切中英**关掉**。
  **期望**：在 `weasel.yaml` 的 `[global settings]` 写 `bare_shift_toggles_ascii: false`，
  部署后生效，裸 Shift 退回纯粹的「选第 2/3 候选」。
- **US1-G** [P1]: 老用户升级。
  **期望**：**不写任何配置**就能用（缺省即开）；新装也是默认开。

### 1.3 Acceptance (GWT)

- Given `weasel.yaml` 的 `[global settings]` **没有** `bare_shift_toggles_ascii` 这一键
  When 运行 `Release\TestShiftIPCStateMachine.exe`
  Then `Result: 28 PASS / 0 FAIL`（其中包含「功能默认开」的断言）

- Given 功能开启、候选窗**开着**、干净的一次 Shift 按下抬起
  When 运行状态机单测
  Then 动作为 `kSelectCandidate`（**不是** `kToggleAsciiMode`）

- Given 功能开启、候选窗**没开**、干净的一次 Shift 按下抬起
  When 运行状态机单测
  Then 动作为 `kToggleAsciiMode`

- Given 功能**关闭**
  When 运行状态机单测
  Then 动作为 `kSelectCandidate`（退回 0.21.0.8 及以前的行为）

- Given Shift down 与 up 之间**有**任何非 Shift 的 keydown
  When 运行状态机单测
  Then 动作为 `kNone`（`Shift+X` 永远不触发）

- Given 16 种 `(down_recorded, intervening_key, menu_open, feature_enabled)` 组合
  When 运行穷举断言
  Then 每一组的期望动作都被命中，且无越界

- Given 装配好的安装包
  When 真机装上、部署、在记事本里做 §3.4 的三步按键
  Then 三步的实测结论与本 spec 一致

### 1.4 Non-goals

- ❌ 不改 `WeaselTSF/KeyEventSink.cpp` 的「整段吃掉 Shift」行为 ——
  那是 ADR 0007 的核心，动它会重开 L18 的 release 误匹配。
- ❌ 不改 `librime/`（宪法 P3，子模块禁改）。
- ❌ 不给裸 Shift 加**双击**逃生门（ADR 0010 P3：候选窗吃掉这次按下，
  要切模式请先按 Esc 关掉候选窗，或用 `Shift+space`）。
- ❌ 不让裸 Shift 切换**全局** ascii 模式（ADR 0010 P2：只切当前会话）。
- ❌ 不区分左右 Shift（两者都切；候选选择才区分左=第 2、右=第 3）。
- ❌ 不用静态门禁去证明「切了之后 app 真的收到英文」—— 那是 §3.4 真机验收的事。

## 2. Technical

### 2.1 判定逻辑抽成纯函数（`include/ShiftStateMachine.h`）

为了让判定表能**不依赖 rime_api / IPC / GUI** 地被单测，沿用 spec 077 的「纯逻辑抽出」模式：

```cpp
enum class ShiftUpAction {
  kNone,             // 不是裸 Shift（没有 Down，或中间有别的键）
  kSelectCandidate,  // 候选窗开着 -> spec 077 行为
  kToggleAsciiMode,  // 候选窗没开 -> ADR 0010 行为
};

inline ShiftUpAction DecideShiftUpAction(bool down_recorded,
                                          bool intervening_key,
                                          bool menu_open,
                                          bool feature_enabled);
```

判定顺序（顺序本身就是规格）：

1. `!down_recorded || intervening_key` → `kNone`
2. `!feature_enabled` → `kSelectCandidate`（**关掉开关就精确退回旧行为**）
3. `menu_open` → `kSelectCandidate`
4. 否则 → `kToggleAsciiMode`

### 2.2 服务端接线（`RimeWithWeasel/RimeWithWeasel.cpp`）

`ShiftUp()` 里：

```cpp
bool menu_open = false;
bool is_ascii_mode = false;
RIME_STRUCT(RimeStatus, shift_status);
if (rime_api->get_status(to_session_id(ipc_id), &shift_status)) {
  is_ascii_mode = !!shift_status.is_ascii_mode;
  rime_api->free_status(&shift_status);
}
RIME_STRUCT(RimeContext, shift_ctx);
if (rime_api->get_context(to_session_id(ipc_id), &shift_ctx)) {
  menu_open = shift_ctx.menu.num_candidates > 0 && !is_ascii_mode;
  rime_api->free_context(&shift_ctx);
}
```

**`menu_open` 的谓词为什么不能只看 `is_composing`**（本轮实测确认）：

- `librime/src/rime/gear/ascii_segmentor.cc:20` —— 英文模式下 segmentor 仍产出
  **raw 段**（1 个候选）；
- `librime/src/rime/gear/key_binder.cc:255` —— librime 自己插 `has_menu` 上下文时
  也要 `ctx->HasMenu() && !ctx->get_option("ascii_mode")`。

⇒ 只看 `is_composing` 会导致**英文模式下按裸 Shift 切不回中文**。

### 2.3 切换的作用域（ADR 0010 P2）

```cpp
bool ascii = rime_api->get_option(to_session_id(ipc_id), "ascii_mode");
rime_api->set_option(to_session_id(ipc_id), "ascii_mode", !ascii);
_UpdateUI(ipc_id);
```

- **只切当前会话**，与 yaml 的 `Shift+space` **逐字对齐**；
- **绝不**走 `SetOption()` —— 那个函数（`RimeWithWeasel.cpp:596-614`）会写
  `m_global_ascii_mode` 全局镜像；
- `global_ascii: true` 时由既有 `_UpdateUI` 传播逻辑接管
  （`Shift+space` 同此行为，**不开特例**）。

### 2.4 配置开关

`output/data/weasel.yaml` 的 `[global settings]`：

```yaml
bare_shift_toggles_ascii: true               # 裸 Shift 切换中英文（无候选窗时）：true；false（ADR 0010）
```

读取机制与 `global_ascii` 同一套（`rime_api->config_get_bool`）。**键缺失 ⇒ 保持构造
默认值 `true`** ⇒ 老用户升级即生效；显式写 `false` 才关。

### 2.5 测试扩展（`test/TestShiftIPCStateMachine`）

新增 6 个测试函数 / 11 条断言：

| # | 测试函数 | 钉住什么 |
|---|---|---|
| 12 | `Test_BareShift_NoMenu_TogglesAsciiMode` | 无菜单 ⇒ 切 ascii_mode |
| 13 | `Test_BareShift_MenuOpen_SelectsCandidate` | 有菜单 ⇒ 选候选 |
| 14 | `Test_FeatureDisabled_AlwaysSelectsCandidate` | 开关关 ⇒ 退回选候选 |
| 15 | `Test_NoDown_NoAction` | 没有 ShiftDown ⇒ 什么都不做 |
| 16 | `Test_Intervening_NoAction` | 中间有别的键 ⇒ 什么都不做 |
| 17 | `Test_DecisionTable_Exhaustive` | 16 种组合穷举 |

⇒ 合计 **17 个测试函数 / 28 条断言**，`Result: 28 PASS / 0 FAIL`。

## 3. Verification

| 层级 | 命令 | 判据 |
|---|---|---|
| 仓库 | `cmd /c scripts\test-infra\run-test-suite.bat` | `=== ALL TESTS PASSED ===` |
| 仓库 | `Release\TestShiftIPCStateMachine.exe` | `Result: 28 PASS / 0 FAIL` |
| 仓库 | `Release\TestDefaultHotkeys.exe` | `Passed: 89 / 89` |
| 仓库 | `Release\TestHotkeyBindings.exe --baseline ... output\data\default.yaml output\data\user-custom\*.custom.yaml` | `SUMMARY\|bindings=39\|reachable=39\|dead=0\|known_dead=0` + `RESULT\|PASS` |
| 产物 | 同上，路径换成 `<user_data_dir>\build\*.schema.yaml` | `dead=0` + `RESULT\|PASS` |
| 产物 | `cmd /c build.bat weasel installer` | 产出 `output\archives\fluxing-0.21.0.9-installer.exe` |
| 产物 | AGENTS.md §2.5 静默装机 smoke test | 布局不变式全部通过 |
| 真机 | §3.4 三步按键 | 每一步都有「切了 / 没切」的实测结论 |

### 3.4 真机按键验收清单（**只能人手敲**）

| # | 前置 | 操作 | 期望 |
|---|---|---|---|
| 1 | 中文模式，候选窗**没开** | 单独按一下 `Shift` | 切英文；状态栏显示英文 |
| 2 | 接着 | 再单独按一下 `Shift` | 切回中文 |
| 3 | 中文模式，**先打拼音**让候选窗弹出 | 单独按一下 `Shift` | **仍选第 2 个候选**，**不**切模式（spec 077 行为不变） |
| 4 | 英文模式，候选窗**没开** | 单独按一下 `Shift` | 切回中文 |
| 5 | 任意 | `Shift+X` | 只出 `X`，**不**切模式 |
| 6 | 两个窗口各开一个会话 | 在 A 里按裸 Shift | 只切 A，B 不变 |
