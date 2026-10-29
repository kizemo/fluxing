# 0019. 快捷面板常驻态跨进程持久化，且 ESC 不再收起面板

- **状态**: Accepted
- **日期**: 2026-10-29
- **关联**: spec `084-quickpanel-always-show` FR-007（被本 ADR 推翻）/ spec `087-shift-side-and-quickpanel-persist` FR-087-B（被本 ADR 修订）/ ADR 0013 D7（被本 ADR 修订）/ ADR 0016 / ADR 0018
- **相关**: `WeaselServer/QuickPanelDialog.h` · `WeaselServer/QuickPanelDialog.cpp` · `WeaselServer/WeaselServerApp.cpp` · `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`

## 背景（Context）

v1.1.1 装机验收（2026-10-29）回报：

1. Shift 部分选词问题已解决（ADR 0018 闭环，无遗留动作）。
2. **快捷设置栏「调出后经过几秒钟自动消失，并非输入或使用其他按键造成」**，并提出新口径：
   **「通过快捷键显示和隐藏快捷设置栏；使用 ESC 不要隐藏快捷设置栏」**。

取证（逐条核实生产 Hide() 路径）：

- 100ms 轮询器（grace 2000ms + 鼠标在面板外累计 ≥1500ms ≈ 3.5s）、`OnTimer` id=1、
  `OnKillFocus`、`WM_ACTIVATEAPP`、`RimeWithWeasel.cpp` FocusOut —— **全部带
  `!AlwaysShowMode()` 守卫**，无一失效。
- 「几秒后消失」的时序与**瞬时模式**的设计行为完全吻合 ⇒ 用户机上常驻态实际为关。
- 根因：`s_mode` 是纯内存态，全仓**没有任何持久化/恢复链路**（spec 084 FR-007 原本
  刻意规定「常驻状态不写入注册表也不写入配置文件；服务重启后回到默认(关)」）。
  1.1.1 装包后为完成 shim 换版必须重启 WeaselServer ⇒ 用户勾上的「常驻」清零 ⇒
  面板退回瞬时模式 ⇒ 用户观察到「自动消失」。**时间线完全吻合。**

因此这不是守卫回归，而是**持久化缺失 + 版本升级重启**的组合缺陷；同时用户借验收
推翻了 spec 084 FR-007 与 ADR 0013 D7 的「ESC 收起」分支。

## 决策（Decision）

### D1 常驻勾选态持久化到 `%APPDATA%\Rime\quickpanel.state`（**落点已被 ADR 0020 D2 修订**）

- 介质：**独立单行 ASCII 小文件**，内容 `always_show=1\n` / `always_show=0\n`。
  - 不用 `default.custom.yaml` / `weasel.custom.yaml`：那是 rime 部署链路，UI 开关
    不应触发重部署。
  - 不用注册表：与 ShortcutSettings 同目录（`%APPDATA%\Rime\`）一致性更好，
    且备份用户数据目录时状态随目录一起走。
- 写点在 `SetAlwaysShowMode()` 内、模式**变化时**同步落盘；写完回读自证，
  0 字节坏文件按缺失处理（本仓「备份/写盘必须能自证」纪律）。
- `WeaselServerApp::Run()` 启动路径调用 `RestoreAlwaysShowMode()`：**只回填 s_mode，
  绝不 auto-show 面板**（spec 055「启动不自动显示面板」不变量保留），也不回写盘。
- 读取容错：文件缺失 / 打不开 / 坏内容 / 非 `always_show=1` ⇒ 一律 false（默认关）；
  行尾 `\r` 容忍。
- 测试注入口 `SetStatePath()`：单元测试把落点改道临时文件，**绝不污染真实用户状态**。
- **修订（ADR 0020 D2，2026-10-29）**：本条选定的落点 `%APPDATA%\Rime` **不在本 fork 的用户数据
  路径上** —— `HKCU\Software\Fluxing\Weasel :: RimeUserDir` 指向
  `D:\Program Files\fluxing\user1\fluxing`，fork 默认用户数据目录是 `%AppData%\fluxing`，
  `%APPDATA%\Rime` 是**已迁走的旧路径**（P5/P6/P8 的「一次性迁移」语义）。
  ⇒ 「与 ShortcutSettings 同目录」「随用户数据目录一起备份」两条理由**均未兑现**。
  落点改为 `<用户数据目录>\quickpanel.state`，旧落点降级为**只读兜底**。
  D1 的其余决策（单行 ASCII 键值、写点在模式变化时同步落盘并回读自证、启动只回填不 auto-show、
  读取容错、测试注入口）**不变**。

### D2 ESC 不再隐藏快捷设置栏

- `OnKeyDown(VK_ESCAPE)` 从 `Hide()` 改为**吞掉**（`return 0`），面板不产生任何动作。
- 显隐统一由 **Alt+, 快捷键**（`ToggleQuickPanel` → Show/Hide）负责；托盘菜单、
  点「词库」按钮等其余手势路径不动。
- 修订 **ADR 0013 D7**：原文明确列 ESC 为「明确收起手势」之一，现按用户口径移除。
- 修订 **spec 087 FR-087-B**：原文「Esc 等明确手势在常驻开启时仍有效」的 Esc 分支作废
  （常驻关闭时 Esc 也不再收起）。
- 推翻 **spec 084 FR-007**：「不持久化、重启回到默认(关)」不再成立——装机换版必须
  重启，该设计让「常驻」承诺在升级场景下必然失效，属于需求级缺陷。
- 历史 spec 正文不改写（取证档案），以本 ADR + spec 087 findings 追记收口。

## 后果（Consequences）

- (+) 装包/重启/升级后勾选「常驻」的用户不再遭遇「几秒自动消失」；验收口径闭环。
- (+) `Hide()` 本体仍无条件收起的原有设计保留（Alt+, 语义不变），改动面最小。
- (−) 新增一个用户数据文件，卸载残留一个 17 字节小文件（可接受；与 ShortcutSettings 同级同性质）。
- (−) spec 084「不持久化」的原始理由（避免 UI 开关触碰配置）被用户口径覆盖；若未来要同步
  多机，需另立 spec 讨论是否并入 weasel.custom.yaml 同步链路。
- 回归测试：TestQuickPanelDialog T13a0–T13h（持久化往返 / CRLF / 坏文件 / Restore 不
  auto-show）+ T14a–T14f（ESC 吞掉且不收起、Hide 路径仍有效）；T4 的
  「WM_KEYDOWN VK_ESCAPE returns 0」断言与新行为天然一致，不改。
