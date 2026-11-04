# 接力交接：Fluxing spec 099「参考逆向搜狗输入法优化快捷设置栏」— P0-6 / P0-4 / P0-5 完成（状态回显 + 键盘可达 + 每槽 tooltip + DPI 几何）；P0-1 / P0-2 待用户在场

> 生成时间：2026-11-05 · 仓库：`F:\soft\00selfmade\rime_claude` · 分支 `Fluxing`（远端 `kizemo/Fluxing`）
> 全文结论按 **已验证 / 推断 / 未验证** 三级标注。凡本轮没实跑过的，一律写「未验证」，不写「应该」「大概」。

---

## 0. 一句话现状

spec 099 的 7 个 P0 任务里，**P0-3**（`b4fec83`）、**P0-7**（`ad8fb9b`）在上一阶段已提交推送；本轮完成 **P0-6（可无头部分）/ P0-4 / P0-5**（代码 + 单测 + 门禁 + 变异证明；真机 e2e 与 P0-6 的 DPI 手点见第 3 节），
新增 4 个 header-only 单一真相源 + 面板当前状态回显 + 键盘可达性 + 每槽 tooltip，并补齐 **ADR 0036 / CHANGELOG / L180 / L181 / L1 静态门禁**。
单测从 120 条扩到 **190 条全绿**；两轮变异证明分别证明「单测有齿」与「门禁有齿」。

**已验证**：190/190 断言绿（`Release\TestQuickPanelDialog.exe` exit=0，既有 120 条无回归）；5 处定向变异全部被抓住并字节级还原（SHA256 一致）；
新门禁 `scripts/gate/L1-quickpanel-hotkey.ps1` 在 pwsh 7.6.9 下 15/15 PASS，其「有牙」证明 3 处变异全部翻红、control/original 保持绿；
装机版 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 版本探测 = Fluxing / 1.1.9.0 / 1.1.9.0（Length 2287104）；
150% DPI 下物理面板尺寸断言 = **368×72**（T25b / T25g 钉住）。

**未验证（必须装机手点）**：真实 DPI 切换 / 多屏拔插下的面板落点与实际像素尺寸；
`CaptureKeyboardFocus()` 跨线程 `AttachThreadInput` 的真机行为；面板出现时收不到按键（`WS_EX_NOACTIVATE` 的固有代价，需先点一下面板）。

---

## 1. 必读文档（按顺序，路径已给全）

| # | 路径 | 用途 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站指引（5 份必读 + 强制项） |
| 2 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册：§1 项目图 / §2 构建测试 / §3 提交规范 / §4 危险区 / §5 提交前清单 / **§9 阶段收尾制度** |
| 3 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层（R1–R9 / P1–P8） |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md` | 本阶段规格 |
| 6 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md` | 任务清单（**勾选状态 = 真实进度**） |
| 7 | `F:\soft\00selfmade\rime_claude\docs\adr\0036-quickpanel-status-echo-keyboard-reachability-and-tooltip.md` | **本轮决策**：状态回显 / 状态区几何 / DPI 下沉 / 键盘可达 / 焦点方案 / Esc 语义调和 / tooltip |
| 8 | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` | 事故溯源（**L180 / L181** 与本阶段直接相关） |
| 9 | `F:\soft\00selfmade\rime_claude\docs\archive\evidence\spec099-mutation-proof.txt` | 单测「有牙」证据 |
| 10 | `F:\soft\00selfmade\rime_claude\docs\archive\evidence\spec099-l1-gate-toothproof.txt` | 门禁「有牙」证据 |

---

## 2. 本轮产出（全部已验证）

### 2.1 新增 4 个 header-only 单一真相源（constitution R7）

| 文件 | 内容 |
|---|---|
| `include/FluxingPanelDpi.h` | `ScaleLogicalByDpi(logical, dpi)` = `(logical*dpi+48)/96`（dpi≤0 退化 96，结果≤0 回退 logical）+ `PanelPhysicalSize` |
| `include/FluxingPanelStatus.h` | `PanelState{ascii_mode,fullwidth,always_show,schema_name,version}` + `AsciiModeText`(英/中) + `ShapeModeText`(全/半) + 状态区排版常量 |
| `include/FluxingPanelKeyboard.h` | `Action{kNone,kSwallow,kMoveFocus,kActivate,kClose}` + `State{focused,slot_count,interactive}` + `vk::` + `MoveFocus` + `HandleKey` |
| `include/FluxingPanelTooltip.h` | `SlotName` / `SlotStateText` / `BuildTooltip`（分隔符「 · 」） |

> 测试**不复制**这些公式：断言全部打在这 4 个 header 的公开 API 上 —— 这正是变异证明能咬住的原因。

### 2.2 P0-6 DPI 几何（可无头部分已闭合）

- 物理布局推演集中在 `WeaselServer/QuickPanelDialog.cpp` 的 `ComputeLayout()` / `SlotRectForLayout()` / `StatusRectForLayout()`；
  对外只暴露 `SlotRectForSize()` / `StatusRectForSize()`，测试不复制布局公式（R7）。
- 三档 DPI 断言：100% → **245×48**、150% → **368×72**、200% → **490×96**；`dpi<=0` 退化 96；就近舍入（245@144% = 368，不是 367）；`logical=0 ⇒ 0`。
- 装机版探测（实跑）：`D:\Program Files\fluxing\weasel\WeaselServer.exe` → ProductName `Fluxing` / ProductVersion `1.1.9.0` / FileVersion `1.1.9.0`，Length 2287104。

### 2.3 P0-4 当前输入状态回显（中/英 + 全/半角）

- 状态来源：`PanelState` 内存快照由 `StateProvider` 注入（`WeaselServerApp.cpp`），面板在**已有的 100ms timer（id=2）**里拉取并比对 5 字段，**变化才重绘**。
- **显示路径零 I/O（P2）**：`WeaselServerApp::Run()` 在 `m_handler->Initialize();` 之后强制预热 `(void)m_handler->IsFullShape(); (void)m_handler->IsSimplification();`，
  把 `m_cached_options` 置真 ⇒ 首次轮询不会触发 `user_config_open`。（这条判据见 L180。）
- 常驻开关的**唯一真相源是 `AlwaysShowMode()`**：`RefreshInputState()` 强制覆盖 provider 带来的 `always_show`，provider 只做镜像（R7）。
- 状态区几何（**不是**写死 x=225）：左界 = 最后一个槽位右界 + 间隙（`kStatusGap=1`），右界 = 内容右界（面板宽 − padding）。
  100% → x=225..240（宽 15）；150% → x=341..360（宽 19）；200% → x=450..480（宽 30）。任意 DPI 下与 4 个槽位两两不相交。

### 2.4 P0-5 键盘可达性 + 每槽 tooltip

- 键位裁决全部走 `fluxing::panel_keyboard::HandleKey()`；`WM_CHAR` 一律吞掉；`WM_SETFOCUS` 置 `interactive=true` 并重绘，**不自动设置 `focused`**。
- Tab / Shift+Tab / ← → / Home / End 循环移焦；**Enter|Space 激活焦点槽；无焦点时只吞键，绝不误触发槽 0**。
- **焦点方案（已定，不再征询）**：保留 `WS_EX_NOACTIVATE` + `SW_SHOWNOACTIVATE`（不抢用户编辑框焦点 —— L89-fix「无法输入中文」事故的前车之鉴）；
  用户**点击面板**时才 `CaptureKeyboardFocus()`（`GetForegroundWindow` + `GetWindowThreadProcessId` + 跨线程 `AttachThreadInput` 记住原焦点窗口 → `SetFocus(hwnd)`），
  `Hide()` 时 `RestoreKeyboardFocus()` 归还（仅当 `GetFocus()==s_hwnd` 才归还）。
- **Esc 语义调和（ADR 0036）**：用「键盘交互态」这一**可判定状态位**分流 ——
  未交互 ⇒ Esc 只吞掉、面板不隐藏（守 ADR 0019 D2）；已交互 ⇒ Esc 收起**面板自身**、不触碰任何其他窗口（守 D0032）。
  按状态判断而非事件顺序 ⇒ 可单测、可判别。
- tooltip：自绘轻量顶层窗（类名 `FluxingQuickPanel_Tooltip`；`WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT`），
  不引 comctl32、不改 `.vcxproj`；文本由 `BuildTooltip()` 生成；位置在面板上方并钳制在工作区内。
  **hover 真相源是 100ms 轮询**而非 `WM_MOUSEMOVE`（L84-fix 已证投递不可靠）。

### 2.5 验证：单测 190/190 + 两轮变异证明

- 单测新增：T25 DPI 缩放与物理布局 / T26 状态区几何与文字映射 / T27 键盘状态机纯逻辑 / T28 每槽 tooltip / T29 生产级键盘端到端（走真实 `WndProc`：Show → Tab 得焦点 → Enter 触发槽 0 → Esc 收起面板自身）。
  既有 120 条全部保持绿，**无回归**。
- 变异证明 ①（单测有齿）：`docs/archive/evidence/spec099-mutation-proof.txt` —— 5 处定向变异全部被抓住（`MUTATION_DETECTED=True`）：
  M1 删 DPI 就近舍入、M2 tooltip 取错状态源、M3 Shift+Tab 方向写反、M4 交互态判定恒真、M5 状态区左界算错；
  每轮还原后 `restored_sha256_match=True`，最终 pristine 树重跑 190/190 绿、`fail_lines=0`。
- 变异证明 ②（门禁有齿）：`docs/archive/evidence/spec099-l1-gate-toothproof.txt` —— 门禁用 `-Root` 指向仓库外副本树，
  CONTROL 绿 / M1（删掉 `WM_TIMER` 里的 `RefreshInputState()`）红 / M2（本地抄一份 DPI 公式）红 / M3（`FluxingPanelStatus.h` 引入 `WeaselIPCData.h`）红 / ORIGINAL 绿 ⇒ `TOOTHPROOF=OK`。
- 新增静态门禁 `scripts/gate/L1-quickpanel-hotkey.ps1`（15 项：A 资产存在 / B DPI 单一真相源 / C 状态回显接线 / D 键盘接线 / E tooltip 接线 / F 显示路径零 I/O / G 中文文案 / H IPC 边界未破），
  已接进 `scripts/gate/run-all.ps1`（新增 `-SkipL1` 开关 + L1 分层）。

---

## 3. 未验证 / 需用户在场（本轮做不了，必须手点）

| # | 事项 | 为什么做不了 | 手工步骤（已写死，照做即可） |
|---|---|---|---|
| 1 | **P0-6 真实 DPI 复验**（P0-7 遗留项同源） | 非交互会话，无法切换系统 DPI、无法拔插显示器、无法用尺量像素 | ① 显示缩放切 150%（或 200%），确认已重登生效；② 唤出快捷设置栏；③ 截图量面板外框物理尺寸，应为 **368×72**（200% → 490×96）；④ 截图存 `docs/archive/evidence/`，并在本文件追加结论 |
| 2 | **P0-5 焦点获取真机行为** | `AttachThreadInput` 跨线程 + 面板在 `WeaselServer.exe` 进程内能否稳定拿到键盘焦点，只能装机确认 | ① **先点一下**快捷设置栏（否则收不到按键，`WS_EX_NOACTIVATE` 固有代价）；② 按 Tab，确认蓝色虚线焦点环逐槽移动；③ 按 Enter/Space，确认对应槽位效果发生；④ 按 Esc，确认只收起面板自身、当前编辑框内容不受影响 |
| 3 | **P0-1 / P0-2 搜狗黑盒采集** | 需要在装了搜狗输入法的机器上截图 + 抓 UIA 树 | ① 打开搜狗「设置属性」；② 截「输入法状态 / 中英切换 / 全半角」相关区域；③ 抓 UIA 树；④ 手工列出「面板显示哪些信息、怎么切换、排布如何」清单 |

> 合规红线不变：**只读静态证据 + 黑盒观察**；不提取 / 复用第三方二进制、资源、词库、`.cupf` 皮肤与专有文案；
> 永不连接 `evidence/urls-hosts.txt` 内那 185 个主机；每条借鉴必须同时写「来源证据 + 差异化实现」。

---

## 4. 下一步（按优先级）

1. **等用户在场**做第 3 节的 ① / ② / ③，补齐真机证据，回填 `tasks.md` 勾选与本文件第 3 节结论。
2. **P0-1 / P0-2 采集完成后**做「差异化实现」：把搜狗面板上我们没有、但值得抄的**信息维度**补进 `PanelState`
   （`include/FluxingPanelStatus.h` 仍是唯一真相源），并补对应单测与 L1 门禁检查项。
3. spec 099 收尾时把 `tasks.md` 勾选状态、`docs/adr/README.md` 索引、`CHANGELOG.md` 的 `## [Unreleased]` 一并对齐。
4. **发版前必须跑 L5 sandbox-gate 带 `-Build`**（本轮未跑；沙箱无 `D:` 盘 / 无外网时该项固定红，属环境能力项，**不得改成 SKIP 刷绿**）。

---

## 5. 环境事实与工具坑（本轮踩到的，照这个来别再试错）

1. **门禁宿主必须是 pwsh 7，不能用 Windows PowerShell 5.1**。`scripts/gate/lib/GateChecks.ps1` 是 **UTF-8 无 BOM + 中文**，
   5.1 按 ANSI(GBK) 解码会把中文字面量的右引号吃掉 ⇒ 报 `ParserError: Missing closing '}'`，**且报错行号指向共享库、不指向调用方**。
   CI 全部 `shell: pwsh`，`run-all.ps1` 用 `(Get-Process -Id $PID).Path` 作子门禁宿主 ⇒ 以 pwsh 7.6.9 为准。（详见 L181）
2. **5.1 的 `Parser.ParseFile()` 报 0 错 ≠ `-File` 能跑通**。兼容性必须两个宿主各跑一次才算数。
3. **`msbuild` 不在 PATH**，必须用实体路径：`C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`。
   仓内旧脚本 `test\v0_19_0_30_e2e\build_e2e.bat` 写的是 2022 路径，**过期勿照抄**。
4. **Shell 实为 PowerShell**（不是 bash）：`cd /f/...` 会变成 `F:\f\...` 失败，必须写 `cd F:\soft\00selfmade\rime_claude`。
5. **中文文档必须字节级改**（`ReadAllBytes`/`WriteAllBytes`）+ 回读自证；`Out-File`/`Set-Content` 会改编码与行尾。
   `New-Object System.Text.UTF8Encoding($true).GetBytes()` **不输出 BOM**，须用 `.GetPreamble() + $bytes`。
6. **行尾约定**：`CHANGELOG.md` = 无 BOM + 全 CRLF；`lessons-learned.md` = **有** BOM + 全 CRLF；`docs/adr/*.md` / `tasks.md` / handoff / prompt = 无 BOM + 全 LF。
7. **`edit` 工具的精确匹配陷阱**：`WM_MOUSEMOVE` 与 `WM_TIMER` 两处的 `if (hit != s_hoveredIdx) {...}` 块仅缩进不同（8 vs 10 空格），
   用它当锚点会命中两次；改用 `RefreshInputState();` 或 `case WM_TIMER: {` 作唯一锚点。
8. **`grep` 工具全仓大范围正则会超时**，先 `glob` 定位文件再定向 `grep`。
9. **`git add` 必须逐路径**，绝不 `git add .`；` m librime` 是子模块脏标记，**不得进任何提交**。
10. **删除走可恢复路由**：运行时硬拦永久删除命令（`Remove-Item` 等），删除须走 `mavis-trash` 顶层路由。
11. **仓库外野生目录** `F:\soft\00selfmade\rime_claudeoutput` 仍在，运行时拒绝永久删除；请用户手工处理或走可恢复删除路由。
12. **worker 后台写文件期间不要读它产出的文件**：本轮曾读到「脚本 27438B、证据 5133B」，几秒后变成 16215B —— worker 还在写，
    期间读到的可能是半截内容（当时那份 L1 脚本一度解析失败）。**先确认 worker 已结束再验收。**

---

## 6. 本轮改动清单（供复核）

新增：`include/FluxingPanelDpi.h`、`include/FluxingPanelStatus.h`、`include/FluxingPanelKeyboard.h`、`include/FluxingPanelTooltip.h`、
`docs/adr/0036-quickpanel-status-echo-keyboard-reachability-and-tooltip.md`、`docs/archive/evidence/spec099-mutation-proof.txt`、
`docs/archive/evidence/spec099-l1-gate-toothproof.txt`、`scripts/gate/L1-quickpanel-hotkey.ps1`、
`docs/archive/handoffs/handoff-2026-11-05-fluxing-spec099-quickpanel-p0-4-p0-5-p0-6.md`、
`docs/archive/prompts/prompt-next-session-2026-11-05-quickpanel-p0-1-p0-2-and-machine-verify.md`

修改：`WeaselServer/QuickPanelDialog.{h,cpp}`、`WeaselServer/WeaselServerApp.cpp`、`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`、
`scripts/gate/run-all.ps1`、`docs/adr/README.md`、`CHANGELOG.md`、`.specify/memory/lessons-learned.md`、
`.specify/specs/099-quickpanel-sogou-benchmark/tasks.md`

**不在提交内**：`librime`（子模块脏标记 ` m librime`）。
