# 接力交接：Fluxing spec 099「参考逆向搜狗输入法优化快捷设置栏」— P0-3 与 P0-7 均已提交并推送；下一步 P0-6 装机 DPI 复验

> 生成时间：2026-11-04 · 仓库：`F:\soft\00selfmade\rime_claude` · 分支 `Fluxing`（远端 `kizemo/Fluxing`）
> 全文结论按 **已验证 / 推断 / 未验证** 三级标注。凡是没在本轮实跑过的，一律写「未验证」，不写「应该」「大概」。

---

## 0. 一句话现状

spec 099 这一阶段的 P0 任务共 7 个。**P0-3**（4 槽全部产生确定效果）已提交并推送；**P0-7**（面板位置持久化）本轮完成代码 + 测试 + 变异证明 + 门禁，**已提交并推送**（`ad8fb9b`）；P0-6 / P0-4 / P0-5 未开工；**P0-1 / P0-2 需用户在场**做黑盒采集。

**已验证**：P0-7 的 32 条 T24 断言全绿（120/120）；两个变异都能让对应断言变红，恢复后 SHA256 字节级一致；harness 抖动修复后 30 轮 0 失败；门禁 L0 13/13 PASS、L3 变异自检 7/7 KILLED（0 survived）、L4 回归 PASS（L5 sandbox-gate 本轮未跑，发版前必须补 `-Build`）。

**未验证**：真实多显示器拔插、100% / 150% / 200% DPI 切换下的面板实际落点 —— 本环境是非交互会话，无法端到端验证，**必须装机手点复验**。

---

## 1. 必读文档（按顺序，路径已给全）

| # | 路径 | 用途 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站指引（5 份必读 + 强制项） |
| 2 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册：§1 项目图 / §2 构建测试 / §3 提交规范 / §4 危险区 / §5 提交前清单 / **§9 阶段收尾制度** |
| 3 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层（R1–R9 / P1–P8） |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md` | 本阶段规格 |
| 6 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md` | 任务清单（勾选状态 = 真实进度） |
| 7 | `F:\soft\00selfmade\rime_claude\docs\adr\0035-quickpanel-position-persistence.md` | P0-7 决策（位置落点 / 钳制规则） |
| 8 | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` | 事故溯源（**L177 / L178 / L179** 与本阶段直接相关） |

---

## 2. 本阶段已完成

### 2.1 P0-3 · 4 槽全部产生确定效果（**已提交并推送**）

- 提交：`b4fec83 feat(WeaselServer): 快捷设置栏 4 槽全部产生确定效果，新增「关于/账号说明」页` → 已推送 `kizemo/Fluxing`（`a309751..b4fec83`）。
- 决策：`docs/adr/0034-quickpanel-about-dialog-and-slot0-schema-switch.md`。
- 根因：旧代码 `POINT cur; GetCursorPos(&cur);` 失败时读未初始化内存 ⇒ 位移是栈残留 ⇒ 点击被误判成拖拽 ⇒ 槽位回调不派发；翻转机制 = 改测试文件即换栈残留。修法见 `lessons-learned.md` **L178**。
- 本轮**没有重做** P0-3 的代码，只修掉了它留下的**测试抖动**（见 §2.2 的 L179）。

### 2.2 P0-7 · 面板位置持久化（**已提交并推送**）

- 提交：`ad8fb9b feat(WeaselServer): 面板位置持久化（独立 quickpanel.position + 多屏钳制 + 启动期恢复）` → 已推送 `kizemo/Fluxing`（10 files changed, 815 insertions(+), 21 deletions(-)）。
- 阶段收尾制度与交接件（`AGENTS.md` §9 / `CLAUDE.md` §2 / 本 handoff / 接力 prompt）另起一次 `docs(fluxing)` 提交。

**决策（ADR 0035）**：位置写入**独立文件** `<用户数据目录>\quickpanel.position`，与常驻开关文件 `quickpanel.state` **同目录、不同文件**。

- **不塞进 `quickpanel.state`** —— 该文件的读盘判据 `always_show::ResolveAlwaysShowFromFiles` 依赖「第一行内容精确等于 `always_show=1` / `always_show=0`」，塞第二个键会破坏判据，并制造「一个文件两个真相源」⇒ 违 constitution **R7**。
- 落盘格式：`pos_x=<int>` / `pos_y=<int>` 两行 ASCII，十进制整数，**允许负坐标**；两键必须同时存在；重复键以第一次出现为准；坐标绝对值 > 100000 视为损坏；容忍 CRLF。
- **读**：服务端**启动期**读一次并缓存（`WeaselServerApp` 构造函数内 `QuickPanelDialog::RestorePosition()`），`Show()` 只读内存缓存 ⇒ **显示路径零 I/O**（守 **P2**）；恢复**只回填缓存、不 auto-show**（守 spec 055）。
- **写**：**拖动结束**时写**原始位置**（不在保存时钳制，避免松手瞬间面板跳动）；纯点击不写盘。写盘发生在 WeaselServer 进程内、非 TSF 回调 ⇒ 不违 P2。
- **钳制三段**（恢复时与 `WM_DPICHANGED` 时执行）：
  1. 面板矩形**完整落在某个工作区内** ⇒ 原样保留（承载多屏负坐标）；
  2. 否则 ⇒ 选「面板中心点最近、且尺寸容得下面板」的工作区，在其中钳 `x ∈ [wa.left, wa.right-w]`、`y ∈ [wa.top, wa.bottom-h]`；
  3. 一个工作区都容不下面板 ⇒ `(0, 0)`。
- 工作区取 `EnumDisplayMonitors` + `GetMonitorInfoW` 的 `rcWork`（排除任务栏）。
- **共享纯函数下沉** `include/FluxingPanelPosition.h`（header-only，namespace `fluxing::panel_position`）⇒ 生产代码与单元测试**同源**（R7 精神）。
- **未改动 `include/WeaselIPCData.h` ⇒ 不触发 IPC 四边同步。**

**改动文件面（权威清单）**

| 文件 | 改动 | 编码 |
|---|---|---|
| `include/FluxingPanelPosition.h` | **新建**（header-only 单一事实源） | 无 BOM + LF |
| `WeaselServer/QuickPanelDialog.h` | +49 行（`SetPositionPath` / `HasSavedPosition` / `SavedPosition` / `RestorePosition` / `SavePositionAt` / `SavePosition`） | 无 BOM + LF |
| `WeaselServer/QuickPanelDialog.cpp` | +50 行（接线 + `WM_LBUTTONUP` 拖动结束写盘 + `WM_DPICHANGED` 钳制 + `Show()` 建窗恢复） | 无 BOM + LF |
| `WeaselServer/WeaselServerApp.cpp` | +4 行（构造函数内 `RestorePosition()`） | 无 BOM + **CRLF** |
| `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` | +T24 系列 32 条断言 + 消除槽位点击抖动 | **BOM + LF** |
| `docs/adr/0035-quickpanel-position-persistence.md` | **新建**（ADR） | 无 BOM + LF |
| `docs/adr/README.md` | +0035 索引行 | 无 BOM + LF |
| `CHANGELOG.md` | +未发布段条目 | 无 BOM + **CRLF** |
| `.specify/specs/099-quickpanel-sogou-benchmark/tasks.md` | P0-7 三项打勾 + 验收输出 | 无 BOM + LF |
| `.specify/memory/lessons-learned.md` | +L179 | **BOM + CRLF** |
| `AGENTS.md` / `CLAUDE.md` | +阶段收尾制度（§9 / 必须强制一条） | 见 §6.5 |

**验收输出（已验证）**

- `test/TestQuickPanelDialog`：**120 / 120** 断言全绿（其中 T24 系列 32 条）。
- **造坏数据（变异）证明有牙**（脚本 `tmp/p0-7-mutation.ps1`，字节级备份 + 锚点唯一性断言 + 恢复后 SHA256 比对）：
  - **M1** 破坏右边界上界 `const int hi_x = best->right - w;` → `best->right;` ⇒ **T24s + T24u 红**（118/120，exit=1）⇒ 恢复后 120/120，SHA 一致。
  - **M3** 包含判据参数顺序写反 `RectContains(wa, panel)` → `RectContains(panel, wa)` ⇒ **T24r2 红**（119/120，exit=1）⇒ 恢复后 120/120，SHA 一致。
  - **踩过的坑（别重试）**：第一次试的变异 `RectContains(wa, panel) && wa.left >= 0` **不改变任何结果 ⇒ 全绿**，证明不了「有牙」。根因：规则 2 的「最近中心」在左侧副屏上给出同一答案。⇒ 为了让**规则 1** 有判别性，补了 T24r2（两块**重叠**屏幕：面板完整落在主屏内，但「中心最近」的小屏更近 ⇒ 规则 1 缺失时结果会变成 `(222,524)` 而非 `(100,500)`）。
- **稳定性**：先修掉槽位点击用例自身的全局光标抖动（见 §2.3 / L179）⇒ 基线 30 轮 1 红（118/119，红的是随机的 T20j）→ 修复后 **30 轮 0 失败**。

### 2.3 附带修掉：用例自身的全局光标抖动（lessons-learned **L179**）

- **现象**：`test/TestQuickPanelDialog` 的 T20 系列在**生产代码一行未改**的前提下偶发红，**红的是哪个槽位是随机的**（T20g / T20i / T20j 都出现过）。
- **定性**：不是生产回归，是**用例依赖了全局可变状态**。合成点击要把真实光标搬到槽位上，而生产侧判定「拖拽 vs 点击」读的就是**真实光标位置**：按下与抬起之间真实光标位移 ≥4 物理像素 ⇒ 判成拖拽 ⇒ 不派发点击回调。
- **环境事实已变**：本机 `GetCursorPos` / `SetCursorPos` **可用**（与 L178 时期「非交互 window station 必然失败」不同）⇒ `SetCursorPos` 会真的移动全局光标。
- **修法（全部在 harness 侧，不动生产语义）**：① 抽 `pin_cursor()`：`SetCursorPos` 后**读回自证**，落点不对就重钉（≤4 次）；② `WM_LBUTTONDOWN` 与 `WM_LBUTTONUP` 之间**不做任何慢 I/O**（原来夹着一个 `Check()`/printf，把漂移窗口从微秒拉到毫秒）；③ 一次尝试没派发 ⇒ 重新钉住再试（≤5 次）。
- **判别性不受影响**：生产代码真回归时光标并不漂移 ⇒ 重试不会让它变绿 ⇒ 重试用尽后断言仍红。

---

## 3. 任务全表（`tasks.md` 勾选状态 = 真实进度）

| 任务 | 状态 | 说明 |
|---|---|---|
| P0-1 搜狗面板黑盒采集 | ⛔ **需用户在场** | 截图 + UIA 树 + 手工清单；未观察项写「未验证」，禁止推测 |
| P0-2 搜狗交互行为采集 | ⛔ **需用户在场** | 同上 |
| P0-3 4 槽全部产生确定效果 | ✅ 已推送 `b4fec83` | ADR 0034 |
| P0-4 槽位状态回显 | ⬜ 未开工 | 需先读 ADR 0029 的隐藏/交付顺序约定 |
| P0-5 键盘可达性 + tooltip | ⬜ 未开工 | **必守 D0032**（Esc 只收面板、不得连带收起候选窗） |
| P0-6 装机版 DPI 复验 | ⬜ **需用户在场** | 100% / 150% / 200% + 多屏 |
| P0-7 面板位置持久化 | ✅ **本轮收口** | ADR 0035 |

---

## 4. 下阶段计划（下一会话从这里开始）

**顺序：P0-6 → P0-4 → P0-5；P0-1 / P0-2 排最后（必须用户在场）。**

### 4.1 P0-6 · 装机版 DPI 复验（需要用户在场，会话本身跑不了）

1. 探测 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 版本（应为 1.1.9.0）。
2. **先停计划任务 `FluxingDeployerMaintenance` 再 `Start-Process`** 起 server。
3. 在 150% DPI 下量面板物理尺寸，期望 368×72（修复后）；若仍是 245×48 ⇒ 登记 `test/regression/DEFECTS.yaml` 新 `D00xx` 并写 heavy 复现用例。
4. 100% / 200% 各测一次；多屏各测一次。
5. **本轮新增的 P0-7 验收点**：100% / 150% / 200% 下拖动面板 → 重启 WeaselServer → 位置应保留；拔掉副屏 / 改分辨率后位置应被钳回可见区。跨屏拖拽产生的「悬空半屏」位置**重启后会被吸附进某一个屏**（已知取舍，ADR 0035 已记录）。

### 4.2 P0-4 · 槽位状态回显

- 开工前先读 `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md`（隐藏/交付/排序的约定）与 `docs/adr/0034` §三（`SlotRect` 单一事实源）。
- 测试先行；新增断言必须**造坏数据证明会红**。

### 4.3 P0-5 · 键盘可达性 + tooltip

- 失败测试先行：Tab / Shift+Tab / ← → 移焦、Enter|Space 激活、Esc 收起。
- **Esc 必守 D0032**：只收面板、不得连带收起候选窗；回归用例 `test/regression/cases/D0032-esc-no-cascade.ps1`。
- 扩展 `scripts/gate/L1-quickpanel-hotkey.ps1` 并**造坏数据证明新断言会红**。

### 4.4 每个阶段任务收尾（**AGENTS.md §9 已制度化**）

阶段任务完成时必须同时产出：① 更新 `docs/archive/handoffs/` 下的 handoff 文档；② 写好移交新会话的接力 prompt（只给全路径指针，**不复述 handoff 内容**）。两者缺一不可。

---

## 5. 证据路径

| 结论 | 证据 |
|---|---|
| P0-7 决策与取舍 | `F:\soft\00selfmade\rime_claude\docs\adr\0035-quickpanel-position-persistence.md` |
| 共享纯函数实现 | `F:\soft\00selfmade\rime_claude\include\FluxingPanelPosition.h` |
| 面板侧接线 | `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.h` / `.cpp`（`SavePosition` 在 `DefaultStatePath()` 之后） |
| 启动期恢复 | `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServerApp.cpp`（构造函数内 `AboutDialog::Initialize();` 紧跟 `QuickPanelDialog::RestorePosition();`） |
| T24 系列断言 | `F:\soft\00selfmade\rime_claude\test\TestQuickPanelDialog\TestQuickPanelDialog.cpp` |
| 变异证明脚本 | `F:\soft\00selfmade\rime_claude\tmp\p0-7-mutation.ps1`（输出 `tmp/p0-7-mutation.log`） |
| 稳定性基线/修复后 | `tmp/p0-7-flake-baseline.log`（基线 30 轮 1 红） / `tmp/p0-7-stability.log`（修复后 30 轮 0 红） |
| 门禁输出 | `F:\soft\00selfmade\rime_claude\tmp\p0-7-gate.log`（L0 13/13 PASS；L3 mutations=7 killed=7 survived=0；L4 PASS） |
| P0-7 提交 | `ad8fb9b` —— `feat(WeaselServer): 面板位置持久化（独立 quickpanel.position + 多屏钳制 + 启动期恢复）` |
| L179 事故条目 | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（文末） |
| 阶段收尾制度 | `F:\soft\00selfmade\rime_claude\AGENTS.md` §9 |

---

## 6. 环境事实与工具坑（本轮实测，别重试）

1. **门禁入口没有 `-Level` 参数**，只有 `-SkipL0` / `-SkipL3` / `-SkipCases` / `-SkipSandboxGate`。L0+L4：`& '.\scripts\gate\run-all.ps1' -SkipL3 -SkipSandboxGate`；L3：`& '.\scripts\gate\run-all.ps1' -SkipL0 -SkipCases -SkipSandboxGate`。
2. **单独构建 `.vcxproj` 必须显式传 `/p:SolutionDir=<repo>\`（带尾斜杠）**，否则 `$(SolutionDir)` 相对 include 路径全断（C1083）。librime 是 Win32-only ⇒ 只构建 Win32。
3. **后台 bash 里 `cd` 之后再写文件会被安全策略拦**（报 "write/delete targets cannot be verified safely after a command working directory change"）⇒ 脚本一律用**绝对路径**，不 `cd`。
4. **中文文档补丁必须字节级**（`[IO.File]::ReadAllBytes` / `WriteAllBytes`）+ 回读自证。`Out-File` / `Set-Content` 会改编码与行尾。惯例：`CHANGELOG.md` = 无 BOM + CRLF；`lessons-learned.md` = **BOM + CRLF**；`docs/adr/*.md` = 无 BOM + LF；`tasks.md` = 无 BOM + LF；`AGENTS.md` / `CLAUDE.md` / `docs/archive/**` = 见 §6.5。
5. **中文「字节数 ≠ 字符数」**：拼 BOM 时必须先 `GetBytes()` 再按字节长度定数组，否则 `CopyTo` 越界（第一次追加 L179 就是这么炸的，异常发生在写盘前 ⇒ 文件未被破坏）。
6. **`tmp/` 已被 `.gitignore` 覆盖**（`.gitignore:185` = `/tmp/`）⇒ 临时脚本、日志、补丁脚本都可以放 `tmp/`，不会进提交。
7. **` m librime` 是子模块脏标记**，**不得**进任何提交（`git add` 必须逐路径，绝不 `git add .`）。
8. **野生构建目录 `F:\soft\00selfmade\rime_claudeoutput`** 仍在仓库外（无 git 影响）。运行时策略拒绝删除工作区外目标 ⇒ **需要用户手工执行**：
   ```powershell
   Remove-Item -LiteralPath 'F:\soft\00selfmade\rime_claudeoutput' -Recurse -Force
   ```
9. **L5 sandbox-gate 本轮未跑**（发版前必跑 `-Build`）。沙箱无 `D:` 盘 / 无外网时该项固定红 —— 属**环境能力项**，**不得改成 SKIP 刷绿**。
10. `GetCursorPos` / `SetCursorPos` 在本机**可用**（L178 时期的事实已过期）—— 写「本机环境事实」时必须标注**测于何时、何种会话**。

---

## 7. 合规红线（不可越界）

- **只读静态证据 + 黑盒行为观察**。不提取、不复用第三方的二进制、资源、词库、`.cupf` 皮肤包与专有文案。
- **永不连接** `evidence/urls-hosts.txt` 内列出的 185 台主机。
- **面板不得新增默认联网行为**（「打开数据目录」= 本地 `explore`，不联网、不等待）。
- 每条借鉴必须写「**来源证据 + 差异化实现**」；未观察到的项一律标「未验证」，禁止推测。
- 决策分流：架构决策 → `docs/adr/NNNN-<kebab>.md`；事故 / bugfix → `.specify/memory/lessons-learned.md` 的 `L##`。**不要把决策埋在 commit message / PR body**。

---

## 8. 需要用户在场 / 需要用户拍板的事

| 事项 | 为什么需要用户 |
|---|---|
| P0-1 / P0-2 搜狗黑盒采集（截图 + UIA 树 + 手工清单） | 需要在**装了搜狗的机器**上手工观察，本会话环境没有 |
| P0-6 装机 DPI 复验（100 / 150 / 200% + 多屏） | 本会话是非交互 window station，无法驱动真实 DPI 变化与多屏拔插 |
| P0-7 遗留：真实多屏拔插 / DPI 切换后的落点 | 同上 |
| 删除 `F:\soft\00selfmade\rime_claudeoutput` | 运行时策略拒绝删除工作区外目标 |
| 装包实测 | 需要真机跑 `WeaselServer.exe` + TSF 链路 |

---

## 9. 接力入口

新会话请先读本文件，再读：

1. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-04-fluxing-spec099-quickpanel-sogou-benchmark.md`（本文件）
2. `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md`
3. `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md`
4. `F:\soft\00selfmade\rime_claude\AGENTS.md`
5. `F:\soft\00selfmade\rime_claude\CLAUDE.md`

然后执行 §4 的下阶段计划。**不要重做 P0-3 / P0-7**（已收口）。
