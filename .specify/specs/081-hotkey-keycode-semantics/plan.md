# 081 实施计划：快捷键「可触发性」验收与死绑定修复

> 配套：[spec.md](spec.md)、[ADR 0009](../../../docs/adr/0009-tsf-frontend-sends-character-not-key.md)、L131。
>
> 本轮（2026-10-25）已完成的部分标 **[已做]**；其余是下一阶段的工作项。

## 0. 本轮已交付（下阶段可以直接用）

| 交付物 | 位置 | 状态 |
|---|---|---|
| 验收工具 | `test/TestHotkeyBindings/` | **[已做]** 编译产品 `ConvertKeyEvent`，逐条判定可达性 |
| 键名表生成器 | `test/TestHotkeyBindings/gen-keysyms.ps1` | **[已做]** 从 librime `key_table.cc` 机械生成 + `-Check` 防漂移 |
| 坏数据对照 | `test/TestHotkeyBindings/fixtures/` | **[已做]** 7 种死法 + 1 条好绑定，7/7 期望命中 |
| 已知死绑定基线 | `test/TestHotkeyBindings/known-dead-bindings.txt` | **[已做]** 6 条，附「改成什么」的建议 |
| 流水线接入 | `scripts/test-infra/run-test-suite.bat` | **[已做]** 构建 + 两条门禁断言 + 防漂移检查 |
| 解决方案注册 | `weasel.sln` | **[已做]** |
| 沙箱门禁阶段 | `scripts/sandbox-gate/in-sandbox-claims.ps1` PHASE 3.5 | **[已做]** P10.1/P10.2/P10.2b/P10.3 |
| 事故库 | `.specify/memory/lessons-learned.md` L131 | **[已做]** |
| ADR | `docs/adr/0009-*.md` | **[已做]** 状态 Proposed，待拍板 |
| 下阶段 spec | `.specify/specs/081-hotkey-keycode-semantics/` | **[已做]** 本目录 |

## 1. 里程碑

### M0 · 拍板与文档债清理（0.5 天）

**目标**：把方向定下来，把仓库里**已经过时或错误**的快捷键描述改对。

| # | 任务 | 验收 |
|---|---|---|
| T0.1 | 用户确认 ADR 0009：**改配置，不改前端按键语义** | ADR 状态 `Proposed` → `Accepted` |
| T0.2 | 修《Rime-设置说明书-重点快捷键.md》§9.1：它仍写「8 条 `send: "《"` 中文全角绑定」（L113 已删 4 条），且算式「共 22 条 = 4+2+4+16+8」实际是 34 条 | 说明书里的条数与 `output/data/**` 实测条数一致（当前 33 条） |
| T0.3 | 修同文 §10.2：把「`Ctrl+Shift+9` 不生效」归因于「被系统级 hotkey 抢了」——**归因错误**，真实原因是 keycode 映射 | 说明书不再给出错误归因，改为指向 ADR 0009 |
| T0.4 | `CONTEXT.md` §4 加 invariant 16：「TSF 前端发的是键产生的字符，不是物理键」 | §4 编号连续 |
| T0.5 | `CONTEXT.md` §7 说「`docs/adr/` is currently empty」——已过时（已有 0001/0007/0008） | §7 与实际目录一致 |
| T0.6 | `CONTEXT.md` §7 的 ADR 章节格式与 `docs/adr/README.md`（MADR 四段式）不一致 | 两处格式统一 |

**依赖**：无。**风险**：低（纯文档）。

---

### M1 · 修死绑定（1 天）

**目标**：6 条死绑定全部消失，配置里不再有「以为能用其实不能用」的条目。

| # | 任务 | 验收 |
|---|---|---|
| T1.1 | `output/data/user-custom/rime_ice.custom.yaml`：4 条 Shift+标点绑定改成**字符的键名**（`Shift+quotedbl` / `Shift+bar` / `Shift+braceleft` / `Shift+braceright`） | `TestHotkeyBindings` 对该文件报 `dead=0` |
| T1.2 | `output/data/default.yaml`：删掉 `Control+Shift+9` 与 `Control+Shift+0` 两条（功能分别由 `Control+Shift+numbersign` / `Control+Shift+dollar` 承担） | 同上 |
| T1.3 | `test/TestHotkeyBindings/known-dead-bindings.txt` 清空（改完后没有已知死绑定了） | 门禁在**不带** `--baseline` 时也是绿的 |
| T1.4 | 同步 `test/TestDefaultHotkeys`：它现在断言「切换中英标点 = `Control+Shift+9`」**存在**——那条要改成断言 `Control+Shift+numbersign` | `TestDefaultHotkeys` 全绿 |
| T1.5 | 同文件里的注释（「保留的 4 条……实际生效」）改成说明**为什么**要写字符键名 | 注释与 ADR 0009 一致 |
| T1.6 | 另两个 `user-custom`（`double_pinyin_sogou` / `radical_pinyin`）里同样的 4 条 Shift+标点绑定 **一并修**（它们与 rime_ice 同构） | 三个 custom 文件都被门禁判为 `dead=0` |
| T1.7 | 按 AGENTS.md §2.5：改了 `output/data/**` ⇒ 重新打包安装包 ⇒ **必须跑静默装机 smoke test** | smoke test 日志在 commit message 里 |

**依赖**：M0.1（方向定了才敢改配置）。
**风险**：
- ⚠️ `accept: "Shift+quotedbl"` 这类写法**依赖 US 键盘布局**。中文输入法用的
  「中文(简体,美式)」布局标点位置与 US 一致 ⇒ 成立；但若用户切到别的布局，
  这 4 条会再次失效。**缓解**：门禁把布局钉在 0409 并在输出里报告实际布局；
  真机验收（M3）要用真实布局再确认一次。
- ⚠️ `send: '"'` / `'|'` / `'{'` / `'}'` 是**单字符**，librime 能解析，但它们会
  **绕过 punctuator** 直通 app —— 这正是原设计的意图，但改完后这 4 个键的
  行为会**从「走中文标点」变成「出 ASCII 符号」**，属于用户可见变化 ⇒ 必须写进 CHANGELOG。

---

### M2 · 门禁进 CI（1 天，含一个外部阻塞项）

**目标**：死绑定在**构建期**被拦住，而不是等用户投诉。

| # | 任务 | 验收 |
|---|---|---|
| T2.1 | `[已做]` 工具接入 `weasel.sln` | `weasel.sln` 里出现 `TestHotkeyBindings` |
| T2.2 | `[已做]` 接入 `scripts/test-infra/run-test-suite.bat`（构建 + 负对照 + 真实配置 + 防漂移） | 仓库内跑一轮全绿 |
| T2.3 | `[已做]` 沙箱门禁 PHASE 3.5（P10.1 / P10.2 / P10.2b / P10.3） | 沙箱一轮 P10 全绿（332 条绑定 / 11 个 schema） |
| T2.4 | `[已做]` 沙箱工装暂存三个辅助文件（fixture / expect / baseline） | 沙箱里 `P10.1` 的负对照能跑起来 |
| T2.5 | `[已做]` CI 跑 `run-test-suite.bat`：CI 的 `test` job 调 `scripts\run-tests.bat`，它是 `run-test-suite.bat` 的薄包装 | CI 日志出现 `TestHotkeyBindings` 段 |
| T2.6 | `[已做]` `gen-keysyms.ps1 -Check` 接进 CI（在 `run-test-suite.bat` 里） | CI 里出现 `OK: ... up to date` |
| T2.8 | `[已做·2026-10-25]` **拆掉把整条流水线挡死的 lint 墙**（事故 L132）：`lint` job 自品牌分叉起就红（整树 lint，175 个文件里 78 个不合规），`build` needs `lint` ⇒ `build`/`test`/`sandbox-gate` **全被跳过**，门禁一次都没真正跑过。改用增量 lint（只查本次改动）+ 欠账单独报告 | 远端 CI 的 `lint` 变绿，且 `test` job 不再是 `skipped` |
| T2.9 | `[可选]` 清掉 78 个文件的样式债（跑一遍 `clang-format -i`），清完可删掉 `--all --report-only` 那一步 | 欠账数 = 0 |
| T2.10 | `[已做·2026-10-25]` **CI 补上 librime 依赖库的构建**：拆掉 lint 墙后 `build` job 第一次真跑就撞上 `Error: the librime dependency libraries are not built yet.`——`Weasel*` 要链接 `librime/deps/yaml-cpp/.../yaml-cpp.lib`，而 CI 的 workflow 从上游继承、上游不链接 yaml-cpp ⇒ **分叉特有的 CI 缺口**。在 Weasel 构建前加 `.\build.bat rime` | `build` job 不再在这一步失败 |
| T2.7 | **仍阻塞**：`sandbox-gate` job 需要带 `sandbox` 标签的**自托管 Windows runner**（Windows Sandbox 是嵌套虚机，GitHub 官方 windows runner 没有嵌套虚拟化） | 要么自建 runner，要么把 P10 阶段拆成「可在官方 runner 上跑的纯静态部分」 |

**依赖**：M1（基线清空后门禁才可能在 CI 里裸跑）。
**风险**：**T2.7 是已知外部阻塞项**（上一轮就存在，本轮仍未解决）。
⚠️ 但它**已经不再是「门禁跑不起来」的原因**了——lint 墙已在 T2.8 拆掉，
现在只差一台支持嵌套虚拟化的机器。

---

### M3 · 真机行为验收（1–2 天）——门禁覆盖不到的部分

**目标**：静态门禁只能证明「**能触发**」，证明不了「**触发后效果正确**」。

待验清单（每条都要「触发了 / 没触发」+「app 实际收到什么」的实测结论）：

| # | 绑定 | 要验的效果 | 备注 |
|---|---|---|---|
| T3.1 | `Shift+space` | 切中英 | 静态判定可达，但**从来没在真机验过** |
| T3.2 | `Control+1` / `Control+2` | 选第 2 / 第 3 候选 | |
| T3.3 | `Shift+Tab` / `Tab`（`send: Shift+Left` / `Shift+Right`） | 移动拼音光标 | **存疑**：librime 的 editor 是否处理「带 Shift 的方向键」存疑 |
| T3.4 | `Alt+Left` / `Alt+Right` | 移动光标 | 同上 |
| T3.5 | `comma` / `period`（`has_menu`） | 上页 / 下页 | 注意 `key_binder.cc:309 ReinterpretPagingKey` 会把「`.` 后跟字母」重解释掉 |
| T3.6 | `KP_*` 共 16 条 | 小键盘映射到主键盘 | |
| T3.7 | `Control+Shift+numbersign` | 切中英标点 | |
| T3.8 | `Control+Shift+dollar` | 切简繁 | |
| T3.9 | 修好后的 4 条 Shift+标点 | 出 `"` / `|` / `{` / `}` | M1 之后才有得验 |
| T3.10 | spec 077：单按 Shift | 选第 2（左）/ 第 3（右）候选 | `TestShiftIPCStateMachine` 已覆盖状态机，但**端到端**（真 app 里）没验过 |

**做法**：仿 `test/TestWeaselIPC/TsfProfileVerify.cpp` 写一个「按键 → 期望 app 收到什么」的驱动，
在真机上跑，产出一张对照表。

**依赖**：M1（否则测的是坏配置）。
**风险**：**这是本阶段最花时间的一块**，且**不能在沙箱里做**（沙箱只能装、不能模拟真人打字）。

---

### M4 · 收口（0.5 天）

| # | 任务 | 验收 |
|---|---|---|
| T4.1 | CHANGELOG 写清「4 条 Shift+标点快捷键此前从未生效，现已修好」+「2 条重复绑定已移除」 | 「主要更新」段落有记录 |
| T4.2 | `[已做·2026-10-25]` 复验报告回填 §3.14（新增）/ §3.12（修正一条错误结论）/ §2（P10.x 四行）/ §4（15–17 条） | 报告与本轮实测一致 |
| T4.3 | 版本号 + 发布 | — |

**依赖**：M1 + M3。

---

## 2. 总览

| 里程碑 | 内容 | 估时 | 依赖 | 风险 |
|---|---|---|---|---|
| M0 | 拍板 + 文档债 | 0.5 d | — | 低 |
| M1 | 修 6 条死绑定 | 1 d | M0 | 中（布局依赖 + 用户可见变化） |
| M2 | 门禁进 CI | 1 d | M1 | 中（T2.7 外部阻塞；**lint 墙已在本轮拆掉，见 T2.8**） |
| M3 | 真机行为验收 | 1–2 d | M1 | 高（不能在沙箱做） |
| M4 | 收口发布 | 0.5 d | M1, M3 | 低 |

**建议顺序**：M0 → M1 →（M2 与 M3 可并行）→ M4。

## 3. 明确不做的事

- ❌ 改 `WeaselTSF/KeyEvent.cpp` 让前端发基础键码（ADR 0009 已否决）。
- ❌ 改 `librime/`（P3）。
- ❌ 做「快捷键自定义 UI」（spec 009 / 050 另有归属；本阶段只保证**默认键位**可用）。
- ❌ 用静态门禁去证明 `send:` 的运行时效果（M3 的真机验收负责）。
- ❌ 在没有自托管 runner 之前硬上 `sandbox-gate` CI job（T2.7 是阻塞项，不要绕）。
  ⚠️ 但**不要再拿「CI 里已经有 sandbox-gate job」当成「门禁在跑」**——
  2026-08 到 2026-10 的事实是：那道 lint 墙让它一次都没跑过（事故 L132）。
