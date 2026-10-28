# 086 实施计划：左右 Shift 上屏候选方向修正 + 输入法弹出菜单补齐「快速面板常驻」

## 1. 技术上下文

### 1.1 现状（已核验）

**缺陷 A · 左右颠倒（反馈 5）**

线缆契约（spec 077 §2.3，钉在 `test/TestShiftIPCWire/TestShiftIPCWire.cpp:27-30` 的注释与 Test #1 / #2 断言里）：

```
ShiftDown / ShiftUp : wParam bit0 = is_left (0=L, 1=R); lParam = session_id
```

逐环节核对：

| 位置 | 代码 | 判定 |
|---|---|---|
| `include/WeaselIPC.h:163-169` | `ShiftDown/ShiftUp(bool is_left)`，注释「is_left: true=Shift_L, false=Shift_R」 | 语义正确 |
| `WeaselTSF/KeyEventSink.cpp:44-55` | `isLeft = (ke.keycode == ibus::Shift_L)` → `m_client.ShiftDown/ShiftUp(isLeft)` | 正确 |
| `include/KeyNameMapper.h:391-396` | `case VK_SHIFT: if (kinfo.scanCode == 0x36) return ibus::Shift_R; else return ibus::Shift_L;` | 正确 |
| `WeaselIPC/WeaselClientImpl.cpp:95-107` | `_SendMessage(WEASEL_IPC_SHIFT_DOWN, is_left ? 0u : 1u, ...)` | **符合契约** |
| `WeaselIPCServer/WeaselServerImpl.cpp:396-429` | `m_pRequestHandler->ShiftDown((wParam & 1u) != 0, lParam, eat);` | ❌ **反解** |
| `include/ShiftStateMachine.h:30-47` | `s.lastIsLeft = is_left`；`r.index = it->second.lastIsLeft ? 1u : 2u;` | 正确（0-based ⇒ 左=第2、右=第3） |
| `RimeWithWeasel/RimeWithWeasel.cpp:1857-1862` | `idx = s.lastIsLeft ? 1 : 2;` → `select_candidate(session, idx)` | 正确 |

⇒ 错点**唯一**：服务端把 `bit0` 反读。契约（0=左、1=右）与发送端一致 ⇒ 单看发送端永远看不出问题，这正是它能活到用户实测才暴露的原因。

**缺陷 B · 弹出菜单缺项（反馈 4）**

菜单有**两份**，各自独立：

| 菜单 | 资源 | 内容 |
|---|---|---|
| 服务端托盘图标菜单 | `WeaselServer/WeaselServer.rc` 的 `IDR_MENU_POPUP`（含 zh-CN / zh-TW / en 三个语言块） | spec 084 已加「快速面板常驻 (&A)」✅ |
| 输入法前端弹出菜单 | `WeaselTSF/WeaselTSF.rc` 的 `IDR_MENU_POPUP` / `IDR_MENU_POPUP_HANT` / `IDR_MENU_POPUP_HANS` | **没有该项** ❌ |

路由链（**不缺**）：

```
WeaselTSF/LanguageBar.cpp:171-190  LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP))
                                  → TrackPopupMenuEx → _HandleLangBarMenuSelect(wID)
WeaselTSF/LanguageBar.cpp:310-355  switch 的 default: 分支 → m_client.TrayCommand(wID)
WeaselIPCServer/WeaselServerImpl.cpp:548-550  按命令编号查处理器并转发
WeaselServer/WeaselServerApp.cpp:371  AddMenuHandler(ID_QUICKPANEL_ALWAYS_SHOW, …)
```

编号来源：`WeaselServer/resource.h:52` 有 `#define ID_QUICKPANEL_ALWAYS_SHOW 40019`；但前端用的共享头 `include/resource.h` **没有**（它只有 `ID_WEASELTRAY_QUICK_PANEL 40018`）⇒ 前端资源里写不出这一项。

资源级取证（PE 资源树探针，本轮临时编写、未入库；方法见本节描述）：

| 二进制 | MENU 105 / 108 / 109 是否含「快速面板常驻」 |
|---|---|
| `output/Win32/WeaselServer.exe` | **含**（zh-CN MENU/105，utf16_off=2112460） |
| `output/weasel.dll` | **不含**（utf16_off = −1） |
| `output/weaselx64.dll` | **不含**（utf16_off = −1） |

⇒ 用户右键的是**输入法弹出菜单**（`LanguageBar.cpp:182` `LoadMenuW(g_hInst, …)`），那条路径上从来没有这一项 ⇒ 反馈 4 成立，且 spec 084 的 T013「托盘右键出现该项」是**服务端**路径，用户走的是**前端**路径。

### 1.2 为什么不用另外两个方案

- **只把服务端那一行反过来**（`(wParam & 1u) != 0` → `(wParam & 1u) == 0`）：能修好症状，但「0=左」这个约定仍然散落在客户端的字面量与服务端的位运算两处。下次有人动其中任一处，仍可能再次漂移。**采纳为同源化的动机，不采纳为最终形态。**
- **在弹出菜单里复刻一份新的命令编号**（例如 40021 专供前端）：会让「同一个开关」在代码里有两个编号，服务端要注册两个处理器，菜单与托盘的行为开始分叉。**拒绝**——共享头补同一个编号即可。
- **给弹出菜单加勾选标记**：需要新增「查询常显开关状态」的往返消息 ⇒ 触及 `include/WeaselIPCData.h` ⇒ 触发「server / TSF / deployer 三端同步 + 一个 ADR + 一个测试」的重流程（宪法硬规则），而收益只是菜单上多一个勾。**本条不做，列为遗留。**

## 2. 技术方案

### 2.1 Shift 方向：编解码同源（核心）

`include/ShiftStateMachine.h`（共享单元、header-only、命名空间 `fluxing`）新增一对纯函数：

```cpp
inline std::uint32_t EncodeShiftIsLeft(bool is_left) { return is_left ? 0u : 1u; }
inline bool DecodeShiftIsLeft(std::uint32_t wparam) { return (wparam & 1u) == 0u; }
```

- 客户端 `WeaselIPC/WeaselClientImpl.cpp` 的两处 `_SendMessage(…, is_left ? 0u : 1u, …)` 改调 `fluxing::EncodeShiftIsLeft(is_left)`。
- 服务端 `WeaselIPCServer/WeaselServerImpl.cpp` 的两处 `(wParam & 1u) != 0` 改调 `fluxing::DecodeShiftIsLeft(wParam)`。
- `include/ShiftStateMachine.h` 里写明约定与事故原因（ADR 0015 指路），避免后来人再把它当「随手改改的位运算」。

⚠️ **结构约束**：`ShiftStateMachine.h` 已被 `test/TestShiftIPCStateMachine` 引入 ⇒ 回归断言可以直接放在同一个测试工程里，**不新增工程、不改 include path**。

### 2.2 弹出菜单补项

- `include/resource.h`：在 `ID_WEASELTRAY_QUICK_PANEL 40018` 之后新增 `#define ID_QUICKPANEL_ALWAYS_SHOW 40019`，注释写明「与服务端自己的头文件取值必须一致」。
- `WeaselTSF/WeaselTSF.rc`（UTF-16LE）：三个 `MENU` 各在「快捷设置栏」那一项之后插一行 `MENUITEM "快速面板常驻 (&A)", ID_QUICKPANEL_ALWAYS_SHOW`；繁体语言块用「快速面板常駐」。

⚠️ **实现陷阱**：`.rc` 是 UTF-16LE 带 BOM 的 Windows 资源文件，常规读工具会读成乱码 ⇒ 一律用脚本按字节改，且改完必须复核「三个语言块各插了一行、繁体用繁体字、缩进与列对齐与相邻行一致」。同族陷阱还有：`WeaselServer.rc` 与 `WeaselTSF.rc` 是**两份**菜单资源，改一份不影响另一份。

### 2.3 回归测试

`test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.cpp` 新增 `Test_EncodeDecode_RoundTrip()`：左编码 = 0、右编码 = 1、左往返回 true、右往返回 false。

**为什么这条断言能证伪**：它把「约定」写成了可执行的数字。若将来服务端再反解，断言里的 `DecodeShiftIsLeft(EncodeShiftIsLeft(true))` 仍然为 true ⇒ 断言不会红。

⇒ **这一点必须在实现时补上**：断言必须**直接断言解码函数对**具体位值**的返回值**（`DecodeShiftIsLeft(0u) == true` / `DecodeShiftIsLeft(1u) == false`），而不是只断言「往返不变」。只断言往返不变的话，单端漂移会被「两端一起错」掩盖过去。

## 3. 里程碑

### Q1 · 两处修复（核心）

- Shift 编解码同源 + 弹出菜单补项 + 回归断言。

### Q2 · 验证与发布

- 整解构建、全套件、格式检查全绿；打包 1.1.0 并对**部署产物**复跑门禁 + 资源级复核「弹出菜单里确实有该项」。
- CHANGELOG、任务总账、交接卡同步；提交并推送；CI 四 job 终态确认。

## 4. Constitution Check

| 原则 | 落点 |
|---|---|
| **I · 先定意图** | R1 满足：动手前先写明意图与验收——意图＝Shift 左右约定同源化 + 弹出菜单补项；验收＝SC-086-1…SC-086-6 写在 `spec.md` §5，每条可复算。 |
| **II · 测试支撑 / R6 证据先行** | R6 满足：先取证再改码——缺陷 A 七个环节、缺陷 B 五个环节逐环落到文件与行号（§1.1），并贴 PE 资源级取证表；改动后门禁数字贴进 `spec.md` §5.3。R2 满足：`spec.md` 不含技术栈词，文件名与库名只出现在 `plan.md`。 |
| **III · ≥3 文件需 spec/plan/tasks** | 满足：本条新建 `086-shift-direction-and-tsf-quickpanel-menu/{spec,plan,tasks}.md`；改动文件为 7 个（`include/ShiftStateMachine.h`、`include/resource.h`、`WeaselIPC/WeaselClientImpl.cpp`、`WeaselIPCServer/WeaselServerImpl.cpp`、`WeaselTSF/WeaselTSF.rc`、`test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.cpp`、`docs/adr/0015-*.md`）。 |
| **IV · 澄清** | 满足：两处歧义已在本条内消解——①「弹出菜单要不要勾选标记」明确为**不做**（协议扩张）并写入 §4 非目标与 `spec.md` §5.4 遗留；②「CapsLock 失效要不要一起改」明确为**不改**（部署侧问题，见 §1.1 与 `spec.md` §0），用户可据 §5.4 复测。 |
| **V · 增量交付** | 满足：Q1（修复）与 Q2（验证发布）分界清晰，可单独回退；本条不夹带其它功能，**不扩协议、不动引擎本体**。 |

**必披露项**

| 披露项 | 结论 |
|---|---|
| 是否触及遗留用户数据路径 | **否**。只改按键状态上报的编解码与菜单资源，不读写用户词库/配置目录，不涉及 leveldb 与 `StartMaintenance`/`EndMaintenance` 包裹。P6 豁免不适用。 |
| 是否新增 IPC 消息类型 | **否**。弹出菜单复用既有「托盘命令」通路（`TrayCommand`），服务端处理器已存在；Shift 方向复用既有 `ShiftDown`/`ShiftUp` 消息的位约定。**不触碰 `include/WeaselIPCData.h`** ⇒ server / TSF / deployer 三端无需同步。 |
| 用户可见变更是否写 CHANGELOG | **是**。左右 Shift 上屏方向修正 + 弹出菜单补项均属用户可见修复，写入 CHANGELOG `### 主要更新`。 |
| 是否修改 `librime/` | **否**（宪法 P3）。上游源码仅作只读取证依据。 |
| 是否新增第三方依赖 | **否**。 |

## 5. 未决问题

- **Q-086-1**：弹出菜单没有勾选标记，常显开关的当前状态只在服务端托盘菜单可见。补它需要新增「查询状态」的往返消息 ⇒ 协议扩张。**留到后续条目**，本条不实现。
- **Q-086-2**：反馈 1 / 2（CapsLock 打开后候选窗消失、快捷键全失效）经取证为**部署侧问题**：装机时 64 位前端 DLL 被占用未替换，安装器已排好开机补换，机器尚未重启。**本条不含相关代码**；换装后需用户复测，若复测仍失败再另立条目。
- **Q-086-3**：反馈 2 的语义仍有歧义（用户写「英文模式按 CapsLock ⇒ 任务栏显示中文，输入为大写英文」，与 `ascii_composer` 的 `SwitchAsciiMode(!key_event.caps())` 推演不符）。判定为旧前端 DLL 的症状，**待换装后复测再定性**，不在本条下结论。

## 6. 提交纪律

- Conventional Commits + 模块前缀；本条改动跨 `WeaselIPC` / `WeaselIPCServer` / `WeaselTSF` / `include` / `test` ⇒ 拆两个提交：`fix(fluxing): …`（含测试）与 `docs(fluxing): …`（spec/ADR/账本/交接卡）；若需重打包收录 `release/`，再加 `chore(fluxing): …`。
- 提交前：`git status -sb` + `git diff --stat`（防 `weasel.props` / `env.bat` / `*.obj` 误提交），格式检查、整解构建、全套件、CHANGELOG 四项门禁全绿。
- commit message 以 UTF-8 无 BOM 写文件后 `git commit -F <file>`。
