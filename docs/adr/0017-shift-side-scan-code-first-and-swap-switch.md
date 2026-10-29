# 0017. 裸 Shift 左右方向：前端改用物理扫描码判定，服务端加「左右互换」开关

- **状态**: Proposed
- **日期**: 2026-10-29
- **关联**: spec 087、任务总账本轮条目（T21 真机复验）
- **相关**: ADR 0015（左右约定的唯一来源）、ADR 0016（**本条接替它的 D7 开放问题**）、
  ADR 0014（CapsLock 锁存状态——与本条正交）

## 背景（Context）

4.1. **症状**（spec 087 A.1，2026-10-29 真机复测）：裸 Shift 松开时，
物理**左** Shift 上屏**第 3** 个候选、物理**右** Shift 上屏**第 2** 个候选，
与期望（左 = 第 2、右 = 第 3）相反；且该症状在服务端解码修正（`a2fa205`）前后**完全一致**。

4.2. **ADR 0016 留下的唯一开放问题**是：发送侧的左右判定只有两个输入
（虚拟键与物理扫描码），二者必有一个在运行期与物理键不符。

4.3. **本轮把这两个输入本身也查完了**（spec 087 A.7）：链路上每一环都被证明是对的。

| 环节 | 位置 | 结论 |
|---|---|---|
| 位域布局 | `include/KeyEvent.h:3-15` | 全部位域基类型都是 `UINT`，布局 = Windows lParam 布局（bit16-23 = scan code、bit31 = key up）；旧件与新件**逐字相同** |
| 键名映射 | `include/KeyNameMapper.h:391-396`、`:551-554` | `VK_SHIFT` → scan 0x36 为右、其余为左；`VK_LSHIFT` → 左；`VK_RSHIFT` → 右。**2012 年至今每个历史版本都没有写反** |
| 前端判定 | `WeaselTSF/KeyEventSink.cpp:44-58` | `isLeft = (ke.keycode == ibus::Shift_L)`；旧 64 位件（10-27 23:50）与 HEAD **逐字相同** |
| 客户端编码 | `WeaselIPC/WeaselClientImpl.cpp:96-110` | `EncodeShiftIsLeft(is_left) = is_left ? 0 : 1`，自 08-15 起未变 |
| 服务端解码 | `WeaselIPCServer/WeaselServerImpl.cpp:406-437` | `DecodeShiftIsLeft(wParam) = (wParam & 1) == 0`；部署件与 `output\Win32\WeaselServer.exe` 逐字节相同 |
| 状态机 → 选词 | `RimeWithWeasel/RimeWithWeasel.cpp:1812-1822`、`:1869-1872` | `s.lastIsLeft = is_left`；`idx = s.lastIsLeft ? 1 : 2` → `select_candidate(idx)`（0-based ⇒ 左 = 第 2、右 = 第 3） |

4.4. **运行时探针**（`tmp/shiftprobe/shiftprobe.cpp`，直连运行中的服务器管道）证明接收侧**完全正确**：
`ShiftDown(bit0=0)` → 第 2 个候选、`ShiftDown(bit0=1)` → 第 3 个候选，
与 `SelectCandidate(1)` / `SelectCandidate(2)` 的基准一致 ⇒ **发送侧送反**。

4.5. **OS 层互换也已排除**：`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layout` 无
`Scancode Map`；`HKCU\Keyboard Layout\Preload` 只有 `00000409` / `00000804`；
注入实验（`tmp/shiftprobe/scancode-inject.cpp`）注入 scan `0x2A` 时低层钩子看到
`vk = VK_LSHIFT`（**未反**）。

4.6. **运行日志取不到值**：`DLOG(INFO)` 在 Release 构建里被编译掉，
`%TEMP%\rime.weasel\*.INFO.*.log` 里没有任何 Shift 行 ⇒ 无法从日志回放运行期的左右值。

⇒ **结论**：链路上每一环都正确，因此反转只能来自**宿主交给 TSF 的 (vkey, scan code) 这一对输入本身**
（应用层交换，或更下面的驱动层交换）。软件无法分辨驱动层的交换 —— 这正是必须留一个
「用户显式翻转」开关的原因。

## 决策（Decision）

**D1 —— 前端按物理扫描码判定左右，虚拟键只作兜底。**
`include/KeyNameMapper.h` 的 `TranslateKeycode` 把三个 Shift 虚拟键合并成一支：
scan `0x2A` → 左、scan `0x36` → 右；没有可用扫描码时才回落到虚拟键
（`VK_LSHIFT` → 左、`VK_RSHIFT` → 右、泛键 `VK_SHIFT` → 左）。
扫描码是键的物理身份，比宿主给的虚拟键更可信。

**D2 —— 服务端提供「左右互换」开关。**
`weasel.yaml` 的 `[global settings]` 新增 `shift_left_right_swap`（默认 `false`）。
判定走纯函数 `fluxing::ApplyShiftSideSwap(is_left, swap)`（`include/ShiftStateMachine.h`），
在 `RimeWithWeaselHandler::ShiftDown` 落进 `s.lastIsLeft` 之前施加。

**D3 —— 开关放在服务端，前端保持无状态。**
与 `bare_shift_toggles_ascii` / `global_ascii` 同一处机制（`RimeWithWeasel.cpp` 的 yaml 读取块），
前端不读配置、不缓存状态 ⇒ 换开关不需要重装前端。

**D4 —— 线缆不变。**
本条不新增 IPC 命令、不改 `include/WeaselIPCData.h` 与协议布局 ⇒ 不触发四边同步。

**D5 —— 默认不改变任何行为。**
`shift_left_right_swap` 缺省为 `false`；D1 的新规则只在「宿主给的虚拟键与扫描码互相矛盾」时
才与旧行为不同，宿主的两个输入一致时两者等价。

## 后果（Consequences）

- 宿主交换了虚拟键但没交换扫描码 ⇒ **D1 直接治好**。
- 宿主/驱动把扫描码也交换了 ⇒ D1 会跟着错，此时**只有 D2 的开关能救** ⇒
  开关必须在说明书里写清楚，并把「怎么确认自己是不是这种情况」也写出来。
- 用户的**用户目录** `weasel.yaml`（`%APPDATA%\Rime\weasel.yaml`）不会自动带上新键
  ⇒ 缺省即 `false`，行为不变；需要翻转的用户自行加一行。

## 验证（Verification）

- `test/TestHotkeyBindings`（`--selfcheck-mapper`）：新增 6 条 Shift 侧断言
  （`VK_SHIFT/0x2A`→左、`VK_SHIFT/0x36`→右、`VK_RSHIFT/0x2A`→左、`VK_LSHIFT/0x36`→右、
  无扫描码时按虚拟键回落）。
- `test/TestShiftIPCStateMachine`：新增 4 条断言钉住 `ApplyShiftSideSwap` 的四个真值。
- 真机复测：**CapsLock 关闭态**下，左 Shift 应上屏第 2 个候选、右 Shift 第 3 个；
  若仍相反，在用户目录 `weasel.yaml` 加 `shift_left_right_swap: true` 后重测。

## 未决（Open questions）

- **Q-001**：本机究竟是应用层交换还是驱动层交换，仍需一次**真实按键**
  （物理左 Shift 时 `GetAsyncKeyState(VK_LSHIFT)` 是否亮）才能定论。
  D1 + D2 的组合在两种情况下都能给出可用结果，所以这条未决**不阻塞**本条落地，
  只影响「要不要建议用户打开开关」。
