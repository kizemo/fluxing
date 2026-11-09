# Handoff · 批次 R · spec 045 T301–T304 可测入口 + T201/T202/T203 界面补全 + Fix A 分层窗口

- 日期：2026-11-09
- 分支：`Fluxing`
- 上一批：`handoff-2026-11-09-batch-q-t105-add-modal.md`（批次 Q，T104+T105「+ 自定义」子 modal）
- 本轮**全部改动未提交**（`git add` / commit 待用户确认）

---

## 1. 本轮目标（用户指令）

1. **T301–T304**：把 `LoadHotkeys` / `SaveHotkeys` / `CheckConflict` 提到可测入口，补
   round-trip、读写路径一致、deploy 替身四条断言 —— 这几条正是当初 T001–T003 没被发现的根因。
2. **T201 / T202 / T203**：filter chips 与方案下拉、圆形 ✕ 关闭键、「取消」时未保存确认。
   按推荐方案直接改，不发问卷。
3. 修「当前在快捷设置栏还无法启动快捷键设置页面」。
4. 「修改快捷键设置后，点击保存，应该启动一次重新部署」。

用户唯一要求先问的场景：**真机点保存写 `default.custom.yaml` 并触发部署**（改真实配置类验收）。

---

## 2. 已完成（代码 + 门禁证据）

### 2.1 T301–T304（单测 `PASS=246 FAIL=0`）

| 任务 | 落点 | 断言 |
|---|---|---|
| T301 | `WeaselServer/ShortcutSettings.h` public 区新增 `struct ConflictReport`、`CheckConflictAgainst`、`CheckConflict` | `TestT301ConflictDetectable` 6 条 |
| T302 | 同左（`LoadHotkeys` 提公开）+ 解析器修复 | `TestT302YamlRoundTrip` |
| T303 | `LoadHotkeys` / `SaveAndDeploy` + `SetYamlPath` / `MutableHotkeys` | `TestT303SaveAndLoadSamePath` |
| T304 | `using DeployLauncher = bool (*)(const std::wstring&)` + `SetDeployLauncherForTest` | `TestT304DeployTriggeredOnceAfterSave`（`g_deployCalls==1`，路径小写含 `weaseldeployer.exe`；`SetYamlPath(L"")` 后返回 false 且不部署） |

**T302 抓出并修掉一个真实 P0**：`RenderRow`（`WeaselServer/ShortcutSettingsT110.h:382-399`）写出
`"- { when: W, accept: \"A\", action }"`（`-` 与 `{` 之间**有空格**），而 `LoadHotkeys` 解析条件
写的是 `trimmed[1] == L'{'`（只认 `-{` 紧贴）⇒ **自己写出去的行永远读不回来**。症状＝
「保存成功但重新打开还是旧键位 / 引擎没变」。
修法（`WeaselServer/ShortcutSettings.cpp` `LoadHotkeys`）：按 `find(L'{', 1)` 容错定位，
判定条件 `trimmed[0]==L'-' && trimmed.back()==L'}' && brace != npos`，
`body = trimmed.substr(brace+1, trimmed.size()-brace-2)`。
**没有改写入端**（改成 `-{` 会破坏既存用户文件兼容性）。

### 2.2 Fix A：`WS_EX_LAYERED` 窗口不参与 DWM 合成（入口点不动，窗口看不见）

- 现象：快捷设置栏/托盘点「快捷键设置」**完全没反应**。
- 入口链路逐段核实**无断线**（`WeaselServerApp.cpp:22-25` / `:362` / `:488-497`、
  `QuickPanelDialog.cpp:781` / `:1362-1415`、`.h:412 kVisualToLogical`）。
- 真因：`ShortcutSettings.cpp` 的 `CreateWindowExW` 用了 `WS_EX_LAYERED`（`@198`），
  但全文件**既无 `SetLayeredWindowAttributes` 也无 `UpdateLayeredWindow`**
  （`RepaintLayered` 只 `RedrawWindow`）。Win32 规则：分层窗口在调用二者之一前不参与 DWM 合成
  ⇒ 窗口对象存在、UIA 能点到控件、探针 14/14 PASS，但**屏幕一像素不显示**。
- 修法（`ShortcutSettings.cpp:364-369`）：补 `SetLayeredWindowAttributes(s_hwnd, 0, 255, LWA_ALPHA);`
  —— 视觉等价、改动可逆；已核实无 `UpdateLayeredWindow` 调用 ⇒ 无互斥。
- 对照：`QuickPanelDialog` 有 `UpdateLayeredWindow` 路径（cpp:1005-1040）所以正常显示。
- `OnCreate` 里旧的 `// v0.19.0.31 fix: 删 SetLayeredWindowAttributes(LWA_ALPHA)。` 注释已改写为
  「历史注记 —— 勿照抄旧结论」，避免下一个人又删回去。

### 2.3 T201 filter chips + 方案下拉

- 5 颗 chips「全部 / 切换 / 选词 / 面板 / 自定义」：`OnPaint` 自绘 + `WM_LBUTTONDOWN` 命中，
  **绘制与命中共用 `ChipRect()`**，不会错位。`s_filterCategory` 在 `PopulateTable()` 前置过滤
  （`s_filterCategory > 0 && RowCategory(h) != s_filterCategory ⇒ continue`）。
- 方案下拉：主窗**直接子控件** `COMBOBOX`（`CBS_DROPDOWNLIST`，`(HMENU)ID_SCHEME=2160`，HWND 存
  `s_hComboScheme`）。首项固定「全局」→ `default.custom.yaml`；其余项取
  `<ParentDirOf(s_yamlPath)>\*.schema.yaml` 词干（`ListSchemaStems` 自行冒泡排序，
  `FindFirstFileW` 顺序不可复现）。
- 切换语义：`sel == g_schemePrevSel` 直接返回；有 dirty 行时弹中文确认，拒绝则 `CB_SETCURSEL` 回滚；
  通过则 `LoadHotkeys` → 失败回退 `LoadDefaults` → 重置 `s_filterCategory` → `PopulateTable` + 重绘。
- 死成员 `s_hToolbar`（恒 `nullptr`）已删除，含 `.cpp:405` / `:2174` 两处同步清理。

### 2.4 T202 圆形 ✕ + T203 取消确认

- `WndProc` 新增 `case WM_LBUTTONDOWN`：`HitClose` 命中（圆心 + 4px 容差）→ `RequestCancel()`；
  `HitChip` 命中 → 切筛选并重绘；否则 `DefWindowProcW`。
- ✕ 绘制由 `Ellipse` 改 `RoundRect`（依据 memory #37：小尺寸 `Ellipse` 光栅化成四角星），
  圆心 `kCloseCx=776 / kCloseCy=18 / kCloseR=10` 与命中判定共用常量。
- 新增 `ShortcutSettings::RequestCancel()`：判据与 `RefreshStatus` **完全一致**
  （`accept_new` 非空且 `!= accept`），>0 时弹中文 `MessageBoxW`
  「有 N 处快捷键修改尚未保存，关闭后将丢失。确定关闭？」`MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2`
  （默认「否」）。`ID_BTN_CANCEL` 与圆形 ✕ 都走同一入口。

### 2.5 保存即部署（用户第 4 项）

- 链路早已存在：`SaveAndDeploy` → `SaveHotkeys` 成功且 `!no_change` → `SpawnDeploy()`
  → 拉起 `WeaselDeployer.exe`。T304 已用替身断言「恰好调用一次」。
- **必须向用户说明的两点**：
  1. 底栏**没有「确定」按钮**，只有 `ID_BTN_SAVE=2115`「保存」与 `ID_BTN_CANCEL=2114`「取消」。
  2. `g_saveStats.no_change == true` 时**刻意不部署**（状态行显示 `✓ 无改动 · 未触发部署`）——
     没有任何改动就不做一次全量重建。

---

## 3. 验证状态（严格区分）

### 已验证 ✅
- `test\TestShortcutSettings\Debug\TestShortcutSettings.exe` **`PASS=246 FAIL=0`**（EXIT=0），
  构建 `BUILD_EXIT=0`，二进制 `LastWriteTime=2026/11/9 14:51:52`（非陈旧）。
- `WeaselServer` 构建成功（`& .\xbuild.bat weasel` → `BUILD_EXIT=0`）。
- tasks.md：T201/T202/T203/T301/T302/T303/T304 共 7 行已勾选并附完成注记
  （`UNCHECKED_T_LEFT=0`）。

### 未验证 / 属推断 ⚠️
- **Fix A 的「窗口真的显示出来」只有真机能证明**。headless 探针只能看到窗口对象与 UIA 树，
  看不到 layered 窗口的**像素** ⇒ 不要拿探针 PASS 当「已显示」的证据。
- T201 方案下拉里 `m_pristine` 的语义（切方案时沿用旧基线）未经真机/单测验证；
  推理上所有 `.custom.yaml` 同目录、基线目录不变，应正确。
- **T201 已知设计限制**：候选来自 `ParentDirOf(s_yamlPath)\*.schema.yaml`，即**用户数据目录**。
  Rime 的 schema 通常在共享数据目录 ⇒ 若用户数据目录下没有 `*.schema.yaml`，
  下拉只会显示「全局」一项。仓库内未找到共享数据目录访问器
  （`SharedDataDir|RimeGetSharedDataDir` 在 `WeaselServer` 下零命中）。
- L0 静态门禁本轮**未跑**（上一批 15/15）。
- 未打包装机、未提交。

---

## 4. 关键文件与锚点（编辑后实际行号）

- `WeaselServer/ShortcutSettings.h`
  - `:182 s_hComboScheme` · `:183 s_filterCategory` · `:227 ID_SCHEME = 2160` · `:279 RequestCancel()`
  - public 区：`struct ConflictReport` / `CheckConflictAgainst` / `CheckConflict` / `LoadHotkeys` /
    `SaveAndDeploy` / `DeployLauncher` / `SetDeployLauncherForTest`
- `WeaselServer/ShortcutSettings.cpp`
  - `:29-30` 静态成员定义 · `:78-190` T201 基础设施块（`kChipX`…`ListSchemaStems`、`HitChip`、`HitClose`、`RowCategory`）
  - `:364-369` Fix A
  - `:455-591` `LoadHotkeys`（含 T302 解析器修复）
  - `:1523-1527` `PopulateTable` 分类过滤
  - `:2002` 前 `case WM_LBUTTONDOWN` · `:2010`「历史注记」注释 · `:2044` 起 combo + search
  - `:2252` 区域 chips 自绘 + 圆形 ✕ `RoundRect`
  - `:2338` 起 `OnCommand`（`case ID_SCHEME` 在 `case ID_BTN_CANCEL` 之前）
  - `:2433` 前 `RequestCancel()` 定义
- `WeaselServer/ShortcutSettingsT110.h:382-399` `RenderRow`（写出 `- { … }`）
- `test/TestShortcutSettings/TestShortcutSettings.cpp`：`namespace test` @42 · 新块 `using SS` @~1000 ·
  四条用例 @1063/1111/1150/1185 · 注册 @1256-1259

---

## 5. 工具坑（本轮新增 / 复现）

1. **分层窗口（`WS_EX_LAYERED`）必须配 `SetLayeredWindowAttributes` 或 `UpdateLayeredWindow`**，
   否则窗口存在、UIA 可点、但屏幕不显示。探针 PASS ≠ 用户可见。
2. **`Ellipse` 小尺寸光栅化成四角星** ⇒ 小圆点/圆角一律 `RoundRect`（memory #37）。
3. **`edit` 一次只能传一组 `old_string`/`new_string`**，多改必须多次调用并 `grep` 逐处核对落盘。
4. **单测必须直接打 vcxproj**（`MSBuild test\TestShortcutSettings\TestShortcutSettings.vcxproj /t:Build`），
   不指定 `/p:Configuration` 默认 **Debug**，产物在 `test\TestShortcutSettings\Debug\`；
   根 `release\TestShortcutSettings.exe` 是陈旧二进制，会造成假绿。
5. **直接 `MSBuild WeaselServer\WeaselServer.vcxproj` 会失败**（缺 `wtl/atlapp.h`，
   `WeaselUI` / `WeaselIPC` / `WeaselIPCServer` 均 `C1083`）⇒ 必须走 `xbuild.bat`，且必须在工作区根执行
   （它用 `%CD%` 定 `WEASEL_ROOT`）。
6. `FindFirstFileW` 枚举顺序不可复现 ⇒ 展示前必须自行排序。
7. 本 TU 无 `<windowsx.h>` ⇒ 用 `static_cast<short>(LOWORD(lp))` / `HIWORD(lp)` 取点击坐标。
8. `namespace SS = ShortcutSettings;` 报 `C2879` ⇒ 只能 `using SS = ShortcutSettings;`，
   且函数局部别名在 `namespace test` 作用域看不到，新块要在块首再声明一次。
9. 写 `tasks.md` 这类文件时用 `[System.IO.File]::WriteAllText` + 探测原 BOM
   （本轮 `BOM_WAS=False`），别用 `Set-Content`（会补尾换行）。

---

## 6. 下一步（建议顺序）

1. **提交**：`WeaselServer` + `test` + `docs` 三块分开提交，模块前缀
   （`WeaselServer: ...` / `test(shortcut-settings): ...` / `docs(spec-045): ...`）。
   提交前 `git status -sb` + `git diff --stat`，排除脏子模块 `librime` 与
   `weasel.props` / `env.bat` / `*.obj`。
2. **L0 静态门禁**（上一批 15/15）。
3. **打包 + 装机真机验收**（需用户同意后才做「点保存写真实 `default.custom.yaml`」）：
   - Fix A：窗口真的出现在屏幕上 + 截图；
   - T201：chips 点击筛选生效、下拉切换方案；
   - T202：圆形 ✕ 可点；
   - T203：有未保存修改时取消弹确认。
4. 批次 Q §6.4 的四项「刻意未做」仍属未覆盖（不算失败）：
   真按键录键 + 保存后 `default.custom.yaml` 落地证据、应用级两行只读复验、
   DPI 多比例矩阵、保存/部署失败路径中文提示。

---

## 7. 相关文档

- ADR：`docs/adr/0046-spec045-verify-no-sendinput-and-add-custom-modal.md`
- 教训：`.specify/memory/lessons-learned.md`（L09/L13/L17/L18/L19/L40/L66/L94/L97/L145/L158/L203）
- 上一批 handoff：`docs/archive/handoffs/handoff-2026-11-09-batch-q-t105-add-modal.md`
- spec：`.specify/specs/045-shortcut-settings/`（tasks.md 已同步勾选）