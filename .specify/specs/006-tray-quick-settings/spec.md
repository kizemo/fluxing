# 006 · 火流猩输入法 v2 · 托盘快速设置面板

> 元 spec 004 拆分。点击托盘图标或 `Alt+,` 弹出 mac 风格快速设置面板；旧 7 级菜单保留作兼容回退入口。

## 0. 上下文

- 现有 `WeaselServer/WeaselServerApp.cpp` 有 `ID_WEASELTRAY_*` 11 项菜单。
- v2 改造为：左键单击托盘 → 快速设置面板；右键保留旧菜单；`Alt+,` 全局热键 = 弹出面板。
- 启动 ≤ 100ms（spec 004 SC-003 性能预算）。

## 1. 产品视角

### 1.1 目标

把"使用频率最高的 8-12 个设置"做成 mac 风格快速面板，让用户在不离开当前输入位置的情况下 1 次点击完成常用切换。

### 1.2 用户故事

- US2-A [P1]：任意 app 焦点下按 `Alt+,` → 弹出 mac 风快速设置面板。
- US2-B [P1]：点击面板上的"中/英"切换 → 立即切到英文，再点切回中文。
- US2-C [P1]：面板启动 ≤ 100ms（preloaded）。
- US2-D [P1]：面板支持鼠标拖动、ESC 关闭、失焦 1s 自动关闭。

### 1.3 验收

- Given Fluxing v2.0.0 已安装，
- When 用户在 notepad 按 `Alt+,`，
- Then 屏幕右下角（系统托盘上方）弹出 mac 风面板，启动 ≤ 100ms。
- And 面板顶部显示三个开关：中/英、简/繁、全/半角；当前态高亮。
- And 点击"中/英" → 切换成功，托盘图标变 ASCII 态。

## 2. Out of scope

- 不重写 WeaselUI/WeaselPanel 候选面板（spec 004 §2.2）。
- 不引入 .NET / WPF（spec 004 §5 全局约束）。
- 不做云同步设置入口（P2 由 spec 010 接入，v2.0 占位）。
- 暗色主题：详细规范见 spec 004 §9 F11 暗色主题横切规范；本 spec 实施时引用之，不重复定义。

## 3. 依赖

- spec 005 `Shift+space` 切中英键位（已在 0.18.5+ ship）。
- spec 004 §9 F11 暗色主题横切规范。
- Windows `RegisterHotKey(HWND_BROADCAST, ID_HOTKEY_QUICK, MOD_ALT, VK_OEM_COMMA)` 全局热键（L04 类似机制）。
- 现有 `WeaselServer/WeaselServerApp.cpp` + `WeaselIPC` 协议（spec 004 §5 IPC 约束）。