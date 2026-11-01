# 095 · Spec · 「快捷面板常驻」开关收进统一设置面板

## 1. 目标

把「快捷面板常驻」这个开关从**托盘菜单**搬进**统一的「设置」弹窗**
（复用 `ShortcutSettings`），让快捷键与界面设置共用一个入口：

- 弹窗标题（窗口标题 + 面板标题）从「快捷键设置」改为「设置」；
- 工具栏行与状态行之间新增一个「界面」分组；
- 「界面」分组内新增「快捷面板常驻」复选框（控件 ID `2130`）；
- 勾选态、持久化落盘、跨进程重启恢复，全部**复用既有实现**，不新增第二套语义。

## 2. 现状

| 能力 | 今天的唯一入口 | 落点 |
|---|---|---|
| 快捷面板常驻 开/关 | 托盘菜单（`WeaselServerApp::ToggleAlwaysShowMode`） | 运行期内存 + `<用户数据目录>\quickpanel.state` |
| 快捷键自定义 | `ShortcutSettings` 弹窗（「快捷键设置」） | `default.custom.yaml` |

问题：

1. **设置面板里看不到常驻态**。用户在设置弹窗里改快捷键，却找不到「面板总是
   显示」这个开关；面板常驻与否只能靠托盘菜单，且托盘菜单不显示当前勾选态。
2. **运行期状态有两个真源的风险**。常驻态是 `QuickPanelDialog::s_mode` 这个
   静态成员；一旦要加第二个入口（设置面板），两个入口各写一份状态必然漂移。
3. **工程障碍**：`test/TestShortcutSettings` 只链接 `ShortcutSettings.cpp` +
   `ModalChrome.cpp`，**链不到** `QuickPanelDialog.cpp` ⇒ 想给常驻态写单元测试，
   逻辑必须下沉到共享头（宪法 R7：生产代码与测试共用同一实现）。

### 2.1 顺带必须修的编码事故

`WeaselServer/ShortcutSettings.cpp` 里的中文此前是**逐字节 `\xNN` 转义**。
MSVC **不会**在宽字符串字面量里重组字节转义（`L"\xe6\x8d" L"\xa2"` 不等于
`L"设"`）⇒ 装机后这个设置弹窗的中文全是乱码。它正是本次要交付的界面，
必须一并还原成真实 UTF-8 字符（工程全局 `/utf-8` 已就位）。

## 3. 功能需求

- **FR-001** 设置弹窗窗口标题与面板标题均改为「设置」；副标题改为中文
  「快捷键与界面设置 · N 条快捷键」。
- **FR-002** 工具栏行之后、状态行之前新增一个「界面」分组（自绘标题 + 分隔线）。
- **FR-003** 「界面」分组内新增「快捷面板常驻」复选框，控件 ID `2130`
  （`ShortcutSettings` 的 ID 段是 2100–2199，2130 空闲）。
- **FR-004** 打开弹窗时复选框的勾选态必须反映**运行期真实状态**
  （不是磁盘值、不是默认值）。
- **FR-005** 勾选 / 取消 ⇒ 立即改运行期状态 **并落盘**到
  `<用户数据目录>\quickpanel.state`，内容仍是单行 ASCII `always_show=0|1\n`。
- **FR-006** 复选框**只改状态与持久化，不主动 Show/Hide 面板**。托盘菜单的
  「开⇒显示／关⇒收起」语义保持不变（见 §6 差异说明）。
- **FR-007** 关闭弹窗不丢弃常驻态（勾选即落盘，无需再点「保存」）。
- **FR-008** 运行期状态与落盘逻辑下沉到 `include/FluxingAlwaysShow.h`
  （header-only），`QuickPanelDialog::s_mode` 删除 —— **不保留第二个真源**。
- **FR-009** 读盘语义与 ADR 0020 的旧实现**逐条等价**：primary 读到行 ⇒ 用它
  （内容不认识 ⇒ `false`）；primary 读不到行 ⇒ 试 legacy；都读不到 ⇒ 默认常驻。
- **FR-010** 视觉副作用（alpha 复位 + 重绘）由 `QuickPanelDialog` 注册的
  `ModeChangedCallback` 观察者承担，设置面板只管状态。

## 4. 验收标准

- **AC-001** `build.bat weasel` 构建成功，0 错误。
- **AC-002** `scripts/test-infra/run-test-suite.bat` 全绿，且 `TestShortcutSettings`
  正式接入套件的 build 列表与 run 列表（此前两边都没有它）。
- **AC-003** 既有 `test/TestQuickPanelDialog` 的断言**一行未改**仍全绿
  —— 这是 FR-009「逐条等价」的机器证据。
- **AC-004** 新增 `TestShortcutSettings` 用例覆盖：状态行格式 / 解析 / 三态读 /
  写盘回读自证 / 主旧落点优先级 / 生产 API 往返。
- **AC-005** `WeaselServer/ShortcutSettings.cpp` 中不再存在 2 位十六进制字节
  转义（`\xNN`）—— 编码事故的机器证据。
- **AC-006** 人工验证步骤见交付报告（界面行为无法自动化）。

## 5. 非目标

- **不做多页签**。本次是「单页 + 一个『界面』分组」，不是「快捷键 / 界面」两页。
- **不改托盘菜单入口**，也不移除它（两个入口并存，语义见 §6）。
- **不改 ADR 0020 的落点规则**：落点仍是 `<用户数据目录>\quickpanel.state`，
  `%APPDATA%\Rime\quickpanel.state` 仍是只读兜底。
- **不动 `output/data/**` 的任何 yaml**（本次不涉及键位/方案配置）。
- **不改 IPC 协议**（本 spec 不触碰 `include/WeaselIPC.h` 的消息语义）。

## 6. 两个入口的语义差异（必须写进交付报告）

| 入口 | 勾选 | 取消 |
|---|---|---|
| 托盘菜单 | 切到长显态 **并显示面板** | 回到瞬时态 **并收起面板** |
| 设置面板复选框 | 只改状态 + 落盘，**不主动显示面板** | 只改状态 + 落盘，**不主动收起面板** |

取舍理由：设置面板是「配置」窗口，用户在配置途中弹出一个常驻面板会造成
视觉干扰；而托盘菜单是「即时动作」入口，显隐是它的固有语义。
面板本身的自动隐藏路径仍由 `AlwaysShowMode()` 守卫（ADR 0013 D4/D5），
所以取消勾选后，**下一次**自动隐藏路径生效时面板就会收起。

## 7. 影响面

| 文件 | 变更 |
|---|---|
| `include/FluxingAlwaysShow.h` | **新增**。运行期状态 + 落盘 + 读盘策略 + 观察者（header-only） |
| `WeaselServer/QuickPanelDialog.h` | 删 `s_mode`；新增 `OnAlwaysShowModeChanged` 观察者入口 |
| `WeaselServer/QuickPanelDialog.cpp` | 状态/落盘/读盘策略下沉共享头；注册观察者；保留 legacy 落点 |
| `WeaselServer/ShortcutSettings.h` | 新增 `AlwaysShowChecked` / `SetAlwaysShowChecked` 公开 API；勾选框成员与 ID 2130 |
| `WeaselServer/ShortcutSettings.cpp` | 「界面」分组 + 复选框 + 标题改「设置」+ 副标题中文；编码还原 |
| `test/TestShortcutSettings/TestShortcutSettings.cpp` | 新增 5 组用例（共享头纯函数 + 生产 API） |
| `scripts/test-infra/run-test-suite.bat` | `TestShortcutSettings` 接入 build / run 列表 |

## 8. 风险与取舍

- **R-1 布局位移**：插入 `kPrefsH = 30` 的分组行后，表格高度从 458 → 428，
  按钮行从 606 → 606（`kBtnY_phys` 公式同步加了 `kPrefsH`，两者抵消）。
  已在 `OnCreate` 里同步改两个式子。
- **R-2 `s_mode` 删除是等价重构**：既有 `TestQuickPanelDialog` 的
  T12/T13/T16 共 12 条断言**一行未改**仍全绿 —— 等价性由机器背书，不是靠人眼。
- **R-3 落点注入点统一**：`StatePathOverrideRef()` 是唯一的注入点，
  `QuickPanelDialog::SetStatePath` 与 `TestShortcutSettings` 共用它 ⇒
  两个测试工程都不会污染真实用户目录里的 `quickpanel.state`。
- **R-4 编码还原的验收**：按**字节级「组」比对**（旧字节转义按 UTF-8 解码
  == 新实际文本），不是逐字面量比对 —— 原源码把一个中文字符的 UTF-8 字节
  拆到了相邻两个字面量（`L"…\xe6\x8d"` + `L"\xa2"`），C++ 会拼接，
  逐字面量比对会产生 17 处假阳性。
