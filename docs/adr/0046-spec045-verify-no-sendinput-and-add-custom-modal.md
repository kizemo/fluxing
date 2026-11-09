# ADR 0046 — spec 045 T104 否决 SendInput 即时验证;T105「+ 自定义」子 modal 形态

- 状态: 已采纳 (2026-11-09)
- 影响范围: `WeaselServer/ShortcutSettings.{h,cpp}`(「+ 自定义」按钮、保存状态栏文案)、`test/TestShortcutSettings`
- 决策类型: 安全审查结论(P2 / AP-045-B)+ UI 架构偏差记录

## Context

design §7.3 要求「立即验证」按钮用 `SendInput` 模拟一次新快捷键以确认生效;
design §2.3 要求「+ 自定义」通过子 modal 收集 动作类型 / 动作值 / 触发条件 三要素。
现状: `Action_VerifyHotkey` 只存在于枚举;「+ 自定义」直接 push 一行 placeholder
(`accept=""` / `custom: user_added`), 三要素完全缺失(tasks.md T104/T105)。
快捷设置主窗是 `WS_EX_LAYERED` 顶层窗, 并带 `WM_KILLFOCUS` grace 自动隐藏(T008);
`ShortcutSettings.h` 文件头雷区禁止 GDI+、禁止硬编码颜色(必须走 FLUENT-UI-TOKENS)、
禁止在低级钩子回调内 `SendInput` / IPC / 同步 I/O(constitution P2)。

## Decision

1. **T104: 否决 `SendInput` 即时验证**, 改为保存后的状态栏验证指引文案:
   `✓ 已保存 · 已触发部署，请在候选窗自按新快捷键验证`。否决理由:
   - `SendInput` 注入落到**用户当前前台应用**, 不保证进入输入法自己的组合链,
     验证结果无效且可能污染用户正在编辑的文档;
   - UIPI 限制: 低完整性进程向高完整性窗口注入会被静默丢弃, 结果不可信;
   - 部署是异步进程(`SpawnDeploy` 拉起 WeaselDeployer), 完成时序不可控,
     保存后立即注入必然读到旧配置 ⇒ 系统性假阴性, 反而制造「快捷键坏了」的误报;
   - 违背 AP-045-B 安全边界(输入注入不得进入快捷键捕获/服务链路)。
   `Action_VerifyHotkey` 枚举保留不动, 避免破坏既有断言。原文案
   「已触发部署，新快捷键已生效」是 UI 过度承诺, 一并改为如实表述。

2. **T105 modal 采用 `WS_CHILD` 子窗覆盖层, 而非 owned 顶层窗**。owned 顶层会抢走前台
   焦点, 触发主窗 `WM_KILLFOCUS` grace 自动隐藏(T008 的豁免条件只对
   `next == hwnd || IsChild(hwnd, next)` 成立); 子窗与 T111 的 `s_hPopover` 同型
   (主窗 owned/child, `RepaintLayered` 已带 `RDW_ALLCHILDREN`)。
   chrome 偏差记录: modal 用实底 `kBgTop` + 标准控件(STATIC/COMBOBOX/EDIT/BUTTON),
   不做主窗同款圆角/渐变/分层 —— 层叠子窗自绘分层成本高收益低; 颜色全部走既有
   `kBgTop/kTextColor/kSearchBg/kTextSecondary/kWarningText` 常量, 不新增硬编码色值。

3. **modal 不内嵌按键捕获**。确定后由纯函数 `ShortcutSettings::BuildCustomRow(action_type,
   action_value, when)` 校验三要素并构造真实自定义行(send / toggle / select 三类;
   when ∈ {always, composing, has_menu, paging}; select 值限单个 1-9 数字), push 进
   `m_hotkeys`, `PopulateTable()` 之后复用既有 `EnterCapturing()` —— popover、实时冲突
   检测、Esc 还原 / Backspace 清空一并继承, 且捕获全在内存(AP-045-B 合规)。
   必须先 `PopulateTable()` 再 `EnterCapturing(newRow)`: 后者内部
   `PositionPopover(DisplayItemForRow(row))` 依赖表里已有该行。
   未定键的新行 `accept/accept_new` 皆空, 落盘循环按既有语义计入 `skipped_no_action`
   跳过, 不污染基线表(T110 语义不变)。

## Consequences

- design §7.3 的「立即验证」按钮不再实现; 验证手段 = 用户在候选窗自按新快捷键,
  状态栏文案如实反映「已触发部署 ≠ 已生效」。
- 「+ 自定义」产出的是真实 `send:` / `toggle:` / `select` 绑定行, 与恢复默认、
  冲突检测、导出/导入全链路兼容; 旧的 `custom: user_added` placeholder 行不再产生。
- `BuildCustomRow` 为 public static 纯函数, 单测直接调生产实现断言(constitution R7);
  modal 窗口本身(纯 UI 路径)不写单测断言, 与批次 P 用例 d 先例一致。
- 新增子控件 ID 2140–2144(modal 专用段), 不新增定时器; modal 同一时刻仅一个
  (`s_hModal` 非空时 `ShowAddModal()` 直接返回)。
