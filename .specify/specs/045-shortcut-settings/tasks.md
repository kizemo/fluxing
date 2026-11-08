# 045 — 快捷键设置 UI (ShortcutSettings) — tasks

> 设计文档：`.specify/specs/045-shortcut-settings/design.md`（本目录此前**只有 design.md**，没有 tasks.md）
> 完成度审计（2026-11-08 批次 L）：结论是**不能称「编辑完」** —— 界面 chrome 完整，但**四条数据链路是断的**。
> 事故沉淀：`.specify/memory/lessons-learned.md` **L203**；相关决策 `docs/adr/0044`（托盘）、`docs/adr/0045`（版本号推导）
> 状态图例：`[x]` 已完成 · `[ ]` 待办 · `[~]` 部分完成

## P0 — 数据链路断裂（2026-11-08 批次 L 已补齐）

- [x] **T001 YAML 解析器 off-by-two** — `trimmed.substr(size-18) == L"key_binder/bindings:"`，字面量是 **20** 个字符却取尾 **18** 个 ⇒ 永远不相等 ⇒ 用户 yaml 的 bindings 段**从来没被解析到**，界面里的行全是内置默认值。改 `WeaselServer/ShortcutSettings.cpp` 取尾 20 字符。
- [x] **T002 落盘路径写错目录** — 硬编码 `%APPDATA%\Rime\default.custom.yaml`，本 fork 真实用户目录是 `%APPDATA%\fluxing`（ADR 0020 单一事实源 `WeaselUserDataPath()`）⇒ 写完 librime 读不到。改用 `WeaselUserDataPath()`，与 `WeaselDeployer/HotkeyEditorDialog.cpp` 同源。
- [x] **T003 deploy 是空函数但状态栏在撒谎** — `SpawnDeploy()` 函数体只有注释，`SaveAndDeploy()` 却无条件写「✓ 已保存 · deploy 已触发」。改为真的 `ShellExecuteW(<WeaselServer 所在目录>\WeaselDeployer.exe, "/deploy")`，并按返回值给文案（成功/未触发两态）。
- [x] **T004 导入/导出是假实现且危险** — 导入 = 重读同一个线上文件（丢未保存编辑）；导出 = **覆写同一个线上文件**（既不 deploy 又骗人）。改为走一对独立往返文件 `default.custom.exported.yaml`，线上配置只由「保存」按钮改。
- [x] **T005「恢复默认」等于没做** — 还原的是 `m_defaults`，而 `m_defaults` = 打开时读到的内容 ⇒ 对有自定义的用户点「恢复默认」什么都不会变。改为真的回到 schema 内置默认表（`LoadDefaults`）。
- [x] **T006 yaml 能打开但没有 bindings 段 ⇒ 表格 0 行** — 原来只判 `LoadHotkeys` 的返回值。改为「解析失败 **或** 结果为空」都回落到内置默认表。

## P1 — 功能接线缺失（实现已存在但零调用点）

- [x] **T101 冲突检测接线** — `CheckConflict` / `GetBuiltinBindings` 实现完整但全仓无调用点 ⇒ 冲突 UI 从未生效。在 `EnterCapturing` / `ExitCapturing` 调用，结果写进 popover 与状态行。**(2026-11-08 完成: `SetPopoverHint` / `ConflictHintText` / `UpdateCaptureConflict`; 硬错误只拦 L18/L19 单键, 同表重复冲突仍可保存)**
- [x] **T102 状态行实时更新** — 现在只在创建时写死一次「未保存 (N 修改)」/「⚠ N 个冲突」，编辑后不刷新。**(2026-11-08 完成: `RefreshStatus()` 统一在 `PopulateTable()` 末尾收口)**
- [x] **T103 搜索框真正过滤** — `ApplySearchFilter` 算了 `match` 但不隐藏行（注释自承「LVS_REPORT 不支持…简化为刷新」）。**(2026-11-08 完成: 按匹配重建列表; `lParam` 存数据下标, 行号→`m_hotkeys` 映射走 `TableItemData()`, 因为 SDK 10.0.26100 没有 `ListView_GetItemData`)**
- [ ] **T104 design §7.3「立即验证」按钮** — `Action_VerifyHotkey` 只存在于枚举与一条断言，无 UI、无实现。**注意**：design §7.3 要求用 `SendInput` 模拟按键，须先过 P2 审查（AP-045-B：不在 hook 里做 IPC / 输入注入）。
- [ ] **T105 design §2.3 自定义快捷键子 modal** — 「+ 自定义」当前只 push 一行 placeholder（`accept=""` / `custom: user_added`），没有动作类型 / 动作值 / 触发条件的选择。
- [x] **T106 写盘是全量而非增量 patch** — 现在写出全部行（含默认行），且 `action_yaml` 为空的行会写出**无动作的坏 binding**。**(2026-11-08 完成: 托管块 `# >>> fluxing shortkey-managed` 只含需 override 的行; 无 `action_yaml` 一律跳过并计入状态文案; 零行不写空 `patch:`/`bindings:`; 首次接管先 `BackupFile`, 失败即不写; 旧文件已有顶层 `patch:` 且无托管块 ⇒ 拒写报「未写入」; 正文级真合并留 T107)**
- [ ] **T107 与既有 `patch:` 正文级真合并** — T106 当前对「旧文件已有顶层 `patch:` 且无托管块」采取**拒写**（避免 YAML 重复键毁配置）。需实现按 key 递归合并后再写，才能接管老用户配置。

## P2 — 界面补全

- [ ] **T201 filter chips + luna_pinyin 下拉** — design §2.1 有「全部 / 编辑类 / …」筛选条与方案下拉，现在只建了 search EDIT，`s_hToolbar` 恒 `nullptr`。
- [ ] **T202 圆形 ✕ 关闭键** — 画在标题栏右侧，但**没有 `WM_LBUTTONDOWN` 处理**，点了没反应。
- [ ] **T203 取消时的未保存确认** — design T19 要求「取消」在有未保存修改时弹确认，当前无。

## 测试缺口（design §11 矩阵）

- [ ] **T301 把 `LoadHotkeys` / `SaveHotkeys` / `CheckConflict` 提到可测入口** — 三者当前是 private ⇒ **零测试**；这正是 T001/T002 没被发现的原因。
- [ ] **T302 round-trip：写入后重开能读回** — 这条测试本可立刻抓到 T001 的 off-by-two。
- [ ] **T303 写盘路径 == librime 读盘路径断言** — 抓 T002。
- [ ] **T304 deploy 触发断言** — 抓 T003（现在 `SpawnDeploy` 会真的拉起 `WeaselDeployer.exe`，单测里需要可注入的替身）。

## 本批（批次 L）已交付的入口

- [x] **T401 托盘右键 / 快捷设置栏品牌图标右键 →「快捷键设置」菜单项** — `WeaselServer/WeaselServer.rc` 的 `IDR_MENU_POPUP` **三份语言版本**同步新增 `MENUITEM "快捷键设置 (&B)"`；新命令 ID `ID_WEASELTRAY_SHORTCUT_SETTINGS = 40023`（`WeaselServer/resource.h`）；`WeaselServer/WeaselServerApp.cpp::SetupMenuHandlers()` 注册 handler → `ShortcutSettings::Show()`（跨线程时按既有模式弹回 UI 线程）。
      依据 ADR 0038：托盘右键与快捷设置栏右键**共用同一份菜单资源**，所以一个菜单项同时覆盖两个入口。

## 批次 L 真机验收证据（2026-11-08）

- [x] **T401 端到端闭环** —— `tmp/v-batchL17.txt`：① 快捷设置栏品牌区 `WM_RBUTTONDOWN` ⇒ 真实弹出菜单窗口（类 `#32768`，PID 归属 WeaselServer）；② 对**该运行时弹出菜单**做 Win32 枚举（`MN_GETHMENU` + `GetMenuStringW(MF_BYPOSITION)`）得 19 项，其中 `pos=3 id=40023 text='快捷键设置 (&B)'`；③ `WM_COMMAND(40023)` ⇒ 「设置」对话框打开（窗口类 `FluxingShortcutSettings`，标题是通用「设置」）。截图 `tmp/v-batchL17-menu.png`（菜单，含「快捷键设置 (B)」项）/ `tmp/v-batchL17-dialog.png`（对话框）。菜单点击 → 命令的映射由 `WeaselServer/SystemTraySDK.cpp:852` 的 `PostMessage(hTargetWnd, WM_COMMAND, uItem, 0)` 承担，`uItem` 即菜单项 ID 40023。
- [x] **T402 装机版资源级坐实** —— `tmp/v-batchL2.txt` 解析 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 的 `RT_MENU id=105` ⇒ 根 POPUP「Fluxing」下 `pos=3 id=40023 text="Shortcut settings (&B)"`。
- [x] **T403 P0 六条复验** —— `release\TestShortcutSettings.exe` **PASS=52 FAIL=0**（EXIT=0）；L0 静态门禁 `checks=14 pass=14 fail=0 skip=0`；装机四件哈希 MATCH=True、WeaselServer 重启 pid 606736。

## 下一阶段排期建议

1. **P1 优先 T101 / T102** —— 冲突检测与状态行实时更新，实现已存在但零调用点，接线成本最低、用户可感知收益最高。
2. **P1 T103** —— 搜索框真正过滤（需把行隐藏从「重画」改为按匹配重建列表）。
3. **P1 T106** —— 写盘改增量 patch，且 `action_yaml` 为空的行不得写出坏 binding。
4. **P2 T201–T203** —— filter chips + 方案下拉、标题栏圆形 ✕ 的 `WM_LBUTTONDOWN`、取消时未保存确认。
5. **测试缺口 T301–T304** —— 把 `LoadHotkeys` / `SaveHotkeys` / `CheckConflict` 提到可测入口，这是 T001/T002 长期没被发现的根因。
6. **T104「立即验证」按钮需先过 P2 审查**（AP-045-B：`SendInput` 注入不得放进 hook 路径）。
