# 交接 · 用户资料云同步「下一阶段执行序」产出（2026-11-10）

> 状态：**规划已落盘；批次 X 可立刻开工**；阶段 E（账号页）被 D1 阻塞。
> 本文件只记「已发生什么 / 还缺什么」，**技术细节全部指向指针文档，不复述**。

---

## 1. 本轮起点与用户原话

用户 2026-11-10：

> 读 `docs/archive/handoffs/handoff-2026-11-10-batch-v-panel-anchor-and-theme.md`，按其中的「已验证/推断/未验证」与「下阶段」推进；批次 V 的代码/单测/装机/提交均已完成（`bac34e7`），未闭环的只有真机复验……
> 问题反馈：之前去除的托盘「中英」图标，又出现了，需要去除。
> 在修复这个 bug 后，下一阶段，希望进入用户资料云同步的任务。请制定计划。

本轮实际交付两件事：

1. 托盘「中英」图标回潮 → 根因定位 + v14 修复 + 真机验证 + 文档 + 提交（批次 W，`f425526`）。**已完成闭**，详见 `handoff-2026-11-10-batch-w-langbar-item-removed.md`。
2. 云同步「制定计划」→ 产出下一阶段执行序（本文档 + 计划文档）。

---

## 2. 云同步规划的真实处境（避免下个会话重复劳动）

spec 100 三件套**早在 2026-11-07 就已落盘**，不是空白：

| 文件 | 大小 | 内容 |
|---|---|---|
| `.specify/specs/100-fluxing-cloud-account-sync/spec.md` | 11,192 B | 用户视角需求 + 验收判据（AC-12 / R9） |
| `.specify/specs/100-fluxing-cloud-account-sync/plan.md` | 23,324 B | 选型 / 协议 / 算法 / 变更面 / 风险 R1 |
| `.specify/specs/100-fluxing-cloud-account-sync/tasks.md` | 22,645 B | 7 阶段 30 条任务 T101–T604 |

**所以本轮不重写 spec，而是产出「计划漂移处置 + 首批次精确范围 + 门禁表 + 不做清单」。**

产出物：`.specify/specs/100-fluxing-cloud-account-sync/next-phase-plan.md`（8,306 B，LF）。

---

## 3. 本轮发现的计划漂移（下个会话必须承认这四条）

11-07 定稿后仓库发生了批次 U / V / W，**部分任务前提已失效**。处置口径是**改入口，不改需求**。

| # | 漂移 | 后果 |
|---|---|---|
| D1 | `tasks.md` T401/T405 假定「快捷面板第 4 槽升级为账号页」，但批次 U（2026-11-09）已按用户指示摘掉该槽 | 账号页**零 UI 入口** → 阻塞阶段 E |
| D2 | T403 要改 `AboutDialog` 旧文案，但批次 W 移除了语言栏项 ⇒ 该对话框可能**已无用户可达路径** | 改文案收益为零；「关于」也失去展示面 |
| D3 | 批次 W 后快捷设置栏只剩 `Alt+,` 一个入口（`WeaselServer/WeaselServerImpl.cpp:81`） | 账号入口的发现成本变高 |
| D4 | T602 引用 ADR 0030 的旧性能三数字；批次 V 已把面板改成分层窗口（`UpdateLayeredWindow`） | 拿 10 月数字当基线会误判破线；按当前 HEAD 重测 |

---

## 4. 下一阶段可直接开工的范围（批次 X）

**不需要任何用户决策**，因为不产生用户可见行为：

| 任务 | 产出 | 优先 |
|---|---|---|
| **T102 词库/短语合并语义** | `docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md` + `tmp/` 取证脚本（不入库） | **最高** |
| T101 邮箱验证码通道 | `docs/research/fluxing-cloud-2026-11/email-verify-channel.md` | 高 |
| T103 端点可达性 | `docs/research/fluxing-cloud-2026-11/endpoint-reachability.md` | 中 |
| T104 测试工程骨架 | `test/TestFluxingCloud/TestFluxingCloud.{cpp,vcxproj}` + `xmake.lua` | 中 |

**T102 必须遵守的三条防翻车要点**（计划文档 §2.1 已写死）：

1. 用 `RIME_USER_DATA_DIR` 指向 `tmp/` 副本的**隔离用户数据目录**，**不要动真机 `%APPDATA%\Rime`**（不可逆污染）。
2. `RimeLeversApi::{export,import}_user_dict` **必须被 `StartMaintenance` / `EndMaintenance` 包裹**，否则 leveldb LOCK 失败。
3. 只写取证结论 + 脚本，**不写生产代码**。

**批次 X 出口判据**：三份 md 各自附「实测输出片段路径」（不接受「我认为应该」）；`test/TestFluxingCloud` 编译 + 冒烟通过，输出口径对齐 `tmp\build-qpanel-tests.cmd`；`winhttp.lib` / `bcrypt.lib` / `advapi32.lib` 已接进构建链。

**硬前置红线**：`T503 合并实现` 在 T102 结论落盘前**不得开工**。

---

## 5. 待用户决策（只阻塞阶段 E，不阻塞 A/B/C/D）

### D1 账号页入口

| 选项 | 评价 |
|---|---|
| **A（推荐）** | 快捷面板第 4 槽重新成为「账号」入口 —— 唯一不新增 IPC、不改用户配置、面板设计里本就有的位置 |
| B | `WeaselServer` 托盘右键加「账号…」 —— 违反 `output/data/weasel.yaml:81 display_tray_icon: false`（用户主动关的），否决 |
| C | `WeaselDeployer` 入口 —— 触发 IPC 四边同步（`include/WeaselIPCData.h` + 三端 + ADR + `test/TestWeaselIPC`），首版否决 |

### D2 `AboutDialog` 归属

与 D1 同源：入口定在哪，「关于 + 账号」分区就挂在哪，然后才执行 T403 文案改写（旧文案「本输入法不含云端账号…不上传、不同步」与新功能自相矛盾，**必须改**）。

**处理口径**：D1/D2 按「推荐 A + 不阻塞开工」处理 —— 先推批次 X，不为此发问卷打断。

---

## 6. 已验证 / 推断 / 未验证

**已验证（有命令输出或文件字节为证）**

- 批次 W v14 真机成立：`tmp/probe-v14-explorer2.log` → `VERDICT=PASS_VALID_PRECONDITION`、`FOUND_FLUXING_ACTIVE=True`、`INPUT_INDICATOR_COUNT=1`（基线 2）、`OUR_TOOLTIP_HIT=0`；宿主 explorer pid 61200 模块表含 `weaselx64.1.2.1.3.0.dll`，SHA256 ≡ 构建产物 `415FE7C5…6121`。
- 装机脚本 `scripts\install\silent-install.ps1 'D:\Program Files\fluxing'` → `==== RESULT: ALL HARD CHECKS PASSED ====`、`INSTALL_EXIT = 0`（C1/C2/C3 28 个 shim/C3b/C4 全过）。
- 单测 `TestFluxingLangBar 8/8 PASS`、`TestFluxingInstallPaths 19/19`、`TestFluxingComponents 6/6`；`f425526` 已提交，`git status -sb` = `ahead 42`。
- spec 100 三件套字节数与任务编号区间（T101–T604）已实测核对。

**推断（未独立取证）**

- 计划中 T101–T604 的工时估算（4 h / 2 h 等）沿用 11-07 估算，未按当前仓库重估。
- 「`AboutDialog` 已无用户可达路径」是从「唯一 `AddItem` 在 `LanguageBar.cpp:450`、且 v14 早退于 L437-438」推得，**未逐个搜索其它调用点**。

**未验证（下轮必须补）**

- 三份取证 md **均不存在**（`docs/research/fluxing-cloud-2026-11/` 空目录）。
- `test/TestFluxingCloud` 不存在。
- 未申请任何邮箱服务商账号 / 未探任何端点（有意为之，见计划 §2.2）。
- 未做 D4 的性能重测。

---

## 7. 本轮工具坑（省下一轮试错）

1. `git commit -F -` **不读 stdin**（`-F` 只接受文件名）⇒ 消息写临时文件再 `-F <文件>`。
2. `edit` 对含全角引号 / 换行的长中文段落精确匹配易失败 ⇒ 改用脚本按行号 + 前缀匹配；改完必做残留检查。
3. 本地安全策略拦「先 `Set-Location` 再对旧路径写删」⇒ 一律 `git -C <repo>` 全路径式调用。
4. `RimeWithWeasel/RimeWithWeasel.cpp` 与 `librime` 是**并发会话 / 子模块假脏，勿提交**。
5. `docs/design/quickpanel-v6/design-demos/*.html`（约 1.3 MB）**已声明不提交**。
6. **子代理不可用**（Token Plan 用量上限，错误码 2056）⇒ 大块写作必须主会话自撰，不要再派 worker。

---

## 8. 指针

| 内容 | 路径 |
|---|---|
| **云同步下一阶段执行序（本文核心产出）** | `.specify/specs/100-fluxing-cloud-account-sync/next-phase-plan.md` |
| spec 100 三件套 | `.specify/specs/100-fluxing-cloud-account-sync/{spec,plan,tasks}.md` |
| 云同步架构 ADR | `docs/adr/0039-cloud-account-sync-architecture.md` |
| 上一轮云同步规划 handoff | `docs/archive/handoffs/handoff-2026-11-07-fluxing-cloud-sync-plan.md` |
| 批次 W handoff（v14 语言栏项移除） | `docs/archive/handoffs/handoff-2026-11-10-batch-w-langbar-item-removed.md` |
| 批次 W ADR | `docs/adr/0053-langbar-item-permanently-hidden.md` |
| 待建取证目录（6 份文件目前均不存在） | `docs/research/fluxing-cloud-2026-11/` |
| `Alt+,` 快捷键登记点 | `WeaselServer/WeaselServerImpl.cpp:81` |
| 托盘图标开关（用户主动关闭） | `output/data/weasel.yaml:81` |