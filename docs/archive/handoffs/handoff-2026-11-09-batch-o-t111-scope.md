# 批次 O 立项笔记 · spec 045 T111 / T104 / T105（**只做到取证，未改码**）

> 日期：2026-11-09 · 会话：`mvs_f3e2a36a2f5345e28a293b9c8bc2cbd1` · 分支 `Fluxing`（`ahead 18`，基线 `b162ecd`）
> 性质：**调研/取证笔记**，不是阶段收尾交接。T111/T104/T105 三个任务本轮**都还没实现**，
> `.specify/specs/045-shortcut-settings/tasks.md` 未改状态，CLAUDE.md 的「本批交接指针」仍指批次 N。
> 上一棒：[批次 N · T107 正文级真合并 + T110 整表覆盖修复](handoff-2026-11-08-batch-n-t107-merge-design.md)（§8 已含 T110 收口轮，是真实现，已提交 `0a8cbd9`）。

---

## 0. 一句话现状

读完 T111 的判定材料后，**T111 不是「写脏 yaml」的格式问题，而是「应用级热键根本没接配置」的功能问题** ——
应用级热键（快捷面板 Alt+`,`、用户短语 Alt+`.`）在 WeaselServer 里是**硬编码 `RegisterHotKey`**，
既不读 `key_binder/bindings`，librime 也不认 `custom:`。所以面板让用户改这些行的快捷键，**存下去必然不生效**。
这条结论还需要**最后一次取证**（见 §3 缺口 1）才能定案，因此本轮不动码。

---

## 1. 本轮取证到的源码事实（逐条带路径行号，均为本轮实读，不是转述批次 N）

### 1.1 `WeaselServer/ShortcutSettings.h`（283 行，本轮全文读）

| 事实 | 位置 |
|---|---|
| `Action_VerifyHotkey = 28` 注释原文「「立即验证」按钮 (design §7.3, **T104 保留, 当前无 UI 入口**)」 | `ShortcutSettings.h:97` |
| `Action_CustomUser = 29` 注释「由面板『+ 自定义』按钮新增行, accept 由用户录入」 | `ShortcutSettings.h:98` |
| `Hotkey` 结构里已有承载 T111 所需字段：`action` / `action_desc` / **`action_yaml`**（注释＝「`custom:` 写入 yaml 的 action 名」）/ `when` / `accept` / **`accept_new`**（录入缓冲，空＝未改动）/ **`yaml_raw`**（「非空＝原样写回该行，录入新 accept 时替换 accept 部分」）/ **`is_custom`**（「用户自定义行，删除时只移除不恢复」）/ `is_default` / `baseline_index`（`-1`＝不在基线，须新建） | `ShortcutSettings.h:158-176` |
| 内置行常量：`Action_Select0..4`（`send: k0..k4`，`when: composing`）、**`Action_TriggerDeploy`＝`Ctrl+F4`**、**`Action_OpenQuickPanel` / `Action_OpenPhrases` 两条应用级行**（`is_default=true, is_custom=false`） | `ShortcutSettings.h:258-265` |
| `kCustomPlaceholder`：`Action_CustomUser` + `custom: user_added` + `is_custom=true` ＝「**librime 侧无生效动作, 仅登记**」的占位行（T105 要替换的目标） | `ShortcutSettings.h:269-275` |
| 录入状态机已具备：`s_capturingRow` / `s_pendingChord`（`VK_SHIFT=1 / VK_CONTROL=2 / VK_ALT=4 / 8=锁 Shift / 16=仅左 / 32=仅右`）、timer `8501` 清残留 chord、`8502` 无键超时复位；冲突判据 `ConflictScore` 1=警告 / 2=功能级冲突 / 3=同槽重复（L19） | `ShortcutSettings.h:108-121, 185-199, 277-280` |
| 单测接缝：public `SaveHotkeys(path, data, overwrite_file=false)` + `SetBaselinePath()`（注释说明 `weasel_deployer` 只在 `find_weasel_yaml()` 兜底与 `patch:` 插入两处用到路径，注入不影响其它行为） | `ShortcutSettings.h:139-151` |

⇒ **T105 / T111 不需要新增数据结构字段**，`is_custom` + `yaml_raw` + `accept_new` + `baseline_index` 够用。

### 1.2 librime 侧「可执行动作」白名单（submodule 只读，未改）

`librime/src/rime/key_binder.cc:38-43` 的六个 action 常量 + `:46-52` 的 `kActions` 映射表：
`send` / `send_sequence` / `toggle` / `set_option` / `unset_option` / `select`。
未知动作（如 `custom`）走 else 分支 `LOG(WARNING) << "unknown action: "` + `continue`
—— **只跳过该条 binding，不清表**（批次 N §8.1 实测与代码一致，故 T110 之后不会引发级联失败）。

### 1.3 面板自己的动作白名单（T110 引入）

`WeaselServer/ShortcutSettingsT110.h:37` + `:47-53`：
`kActionKeys = {send, send_sequence, toggle, set_option, unset_option, select, sync}`，
`supportAction(action)` 用它判定「可执行条目」。
⇒ **注意：`sync` 不在 §1.2 的 librime 六个动作里**。这可能是第二类和 `custom` 一样「面板认、librime 不认」的动作，
也可能是 weasel 自己在别处处理。**必须查清再定 T111 方案**（§3 缺口 2）。

### 1.4 应用级热键的真实注册路径（T111 的关键前提）

- `lessons-learned.md:3516`：spec 036 在 `WeaselIPCServer/WeaselServerImpl.cpp` **line 76（OnCreate 内）**
  加 `RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA)` + OnDestroy 里 `UnregisterHotKey`。
- `CHANGELOG.md:4783`：同一处描述（`WeaselIPCServer/WeaselServerImpl.{h,cpp}` adds `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` + 硬编码 Alt+COMMA）。
- `lessons-learned.md:13547 / 14338`：`RegisterHotKey` 是**会话级**作用域；服务端不在 ⇒ 全局热键全失。
- `WeaselServerApp.cpp` 另有 `RegisterHotKey(MOD_ALT, VK_OEM_2 /* / */, ID_HOTKEY_USER_DICT_ALT_SLASH)`（`CODE_MAP.md:48`、`.learnings/FEATURE_REQUESTS.md:134`）。

⇒ 这些都是**硬编码键值**，没有任何一条证据显示它们从 `weasel.yaml` 的 `key_binder/bindings` 读取。
⇒ 若证实：面板里改「快捷面板 / 用户短语」两行的快捷键＝**改了个不生效的设置**，
　　那 T111 的正解不是「把应用级行区分开写」，而是**要么让 Server 真去注册面板配的键（需要新增配置通道），
　　要么把这两行标成只读/明确告知不生效**。这是产品级决策，见 §4。

---

## 2. 环境/门禁事实（本轮实测）

- `codebase-memory-mcp`（CLAUDE.md §2「必查图谱」）**在本会话不可调用**，且 `F-soft-00selfmade-rime-claude`
  图谱在批次 N §3/§4.1 已被实测证明**只覆盖 TSF/UI/deployer 文件、不含 `WeaselServer/ShortcutSettings.*`**。
  ⇒ 本轮直接走 `Read`/`Grep`（批次 N 已因同一原因被拦过一次，那次拦下的是真缺陷）。
  **待办**：下次跑 `mcp list` 确认服务名是否存在，把降级理由写进本文件，别默默绕过 CLAUDE.md 强制项。
- 工作树：`git status -sb` = ` m librime`（submodule 脏，**未碰**）+ `ahead 18`，无其它改动。
- `AGENTS.md` 读到 §8（offset 1-661）；**§9 handoff/relay prompt 全文尚未读**（offset 662 起）。
  `.claude/rules/hotkey-binding.md` 与 `Rime-设置说明书-重点快捷键.md` §5-§7/§9/§10 **本轮未读** ⇒
  动 `ShortcutSettings.cpp` 落盘逻辑前必须补读（CLAUDE.md 强制项）。

---

## 3. 定案前必须补的三个取证缺口

1. **`WeaselIPCServer/WeaselServerImpl.cpp` 的 `RegisterHotKey` 是否读配置**：读 OnCreate/OnDestroy 全部
   `RegisterHotKey` 调用 + 搜 `hotkey` / `quick_panel` / `phrases` / `ID_HOTKEY_` 是否有从
   `weasel.yaml` / `app_options` / `WeaselCustomOption` 解析键值的代码；并确认改键后面板是否会
   重新注册（批次 N 已知「改 Alt+, 后当前会话仍按老键生效，重启服务才更新」——若真是硬编码，这句提示本身也要改）。
2. **`sync` 动作到底谁实现**：搜 `librime` 里 `sync` / `KeyBinder::Sync`、`Weasel*` 里对 `sync` 的处理，
   判定它是「librime 认的第三种形态」还是「和 `custom` 同类的不生效动作」。
3. **`ShortcutSettings.cpp` 里应用级行的来源与出口**（本轮被调用次数预算打断，未读）：
   `BuildManagedSubBlockV2` / `SaveHotkeys`（批次 N 给的行号 L756-942 区段）里，
   `Action_OpenQuickPanel` / `Action_OpenPhrases` / `Action_*AppLevel` 是否被写进 `key_binder/bindings`、
   `OnCommand` 里这些 action 是否走 `QuickPanel::Show()` 一类本地分发（面板运行时能触发 ≠ 写进 yaml 后能生效）。

---

## 4. T111 方案选项（**待 §3.1 取证后二选一，属产品决策，需用户点头**）

- **方案 A（推荐，改动最小且诚实）**：应用级两行（快捷面板 / 用户短语）标为**只读 + 说明**，
  不进 `key_binder/bindings` 托管块；面板里明确「此快捷键由输入法程序注册，本版本暂不支持自定义」。
  代价：用户改不了（现状也改不了），但不再给出「改了会生效」的错觉。
  好处：`refused` / 冲突判据 / 托管块都不用重写，librime 表不再被无意义条目污染（T111 的原始诉求一并解决）。
- **方案 B（真功能）**：新增 weasel 自有配置通道（如 `patch:` 下 `weasel/hotkeys/quick_panel`），
  面板写那里，`WeaselServerImpl` 读取并 `RegisterHotKey`（改键后重新注册 + 失败要 GUI notify，
  参照 AP-L100-N / lessons L1771/L1772/L1785 的会话级注册坑）。
  代价：跨组件改动 + 需 ADR（改 IPC/配置边界要四边同步？先判是否走 IPC）+ 会话级/单实例风险。
  这条能真正完成「快捷键设置」的产品承诺，但不是 T111 一句话能带过的工作量。
- **方案 C（只治表面）**：保留现写法，仅把应用级行与自定义行在 yaml 注释里分组。
  批次 N §8.2 曾推荐它，**但取证后建议放弃** —— 它不解决「设置不生效」这个用户可感知缺陷。

## 5. T104 / T105 的前置约束（沿用批次 N，未变）

- T104「立即验证」design §7.3 的 `SendInput` 注入路线**必须先过 P2 / AP-045-B**（hook 与 TSF 回调内
  禁止 IPC 与输入注入）。未过审不得编码；可先把按钮与「只做本地冲突检测」的安全版闭环拆出来。
- T105 自定义子 modal：`Action_CustomUser` + `kCustomPlaceholder` 是占位实现（`accept=""`、
  `custom: user_added`），需要「动作类型 + 动作值 + when/accept」选择器；结构体字段已够用（§1.1）。

## 6. 下阶段计划（顺序不变，但 T111 拆成 6.0 取证 → 决策 → 实施）

1. **6.0 取证**（§3 三条，预计 3-5 次读取）→ 结论写回本文件 §1.4/§3。
2. 若判定为方案 A：补读 `.claude/rules/hotkey-binding.md` + `Rime-设置说明书-重点快捷键.md` §5-§7/§9/§10
   → 实施 + 单测（断言「托管块内不含 `custom: open_*` 行」+「只读行不可录入」）
   → `tmp/build-batchM.cmd` + 测试工程 msbuild + `L0-static.ps1 -Mode source`（判 PASS 前核 exe mtime）。
   若判定为方案 B：**先写 ADR**，再与用户确认是否在本 spec 内做。
3. T104 / T105 各自先出接缝，再编码。
4. 阶段收尾：更新本文件 + `tasks.md` 状态 + CLAUDE.md 交接指针 + AGENTS.md §9 的 relay prompt 规则。

> **更正**：AGENTS.md §9 全文本轮**已读**（`AGENTS.md:662-713`，非凭记忆）。要点：任一 `tasks.md` 阶段任务
> 勾 `[x]` 时，必须**同一个变更**里有 ① 当日 handoff（同日同 spec **就地改写，不新建副本**；固定顺序：
> 现状段落 → 必读文档全路径表 → 逐任务 done/in-flight/pending → 下阶段计划及排序理由 → 证据路径 →
> 不可从仓库推出的环境事实与工具坑 → 需要用户在场的项）；② 接力 prompt **一句话**，只给全路径指针、
> 不复述 handoff 内容；③ `CLAUDE.md` §2 的「本批交接指针」行。handoff 是**状态**不是日记。

---

# 本轮（批次 O）定案：**T111 = 方案 A（只读 + 不进托管块）；T104 = 安全版闭环；真机验收 = 提交前一次**

问卷 `ask_a236b93048bb71dc75b438e4` 的用户答复（2026-11-09）。方案 B（真接 weasel 自有配置通道 + 真注册）
被**搁置**；批次 N §8.2 曾推荐的方案 C 未采纳 —— 它不解决「用户能感知到的设置不生效」。

## T111 方案 A 的落点清单（行号 = 2026-11-09 当前源码）

| # | 落点 | 改法 | 性质 |
|---|---|---|---|
| A1 | `WeaselServer/ShortcutSettings.h` 新增 public static `IsAppLevelAction(Hotkey::Action)` | `a == Action_OpenQuickPanel \|\| a == Action_OpenPhases`（**不含 `Action_TriggerDeploy`**，见「待核」）；生产与单测共用同一判据 | 新接缝 |
| A2 | `MergeHotkeys` 第 6 步 `cpp:773-794` | 给这两行设 `read_only = true`；`Hotkey` 加 `bool read_only = false;`（紧邻 `is_custom` 放，保持 POD） | 数据层 |
| A3 | `BuildManagedSubBlockV2` 第 2 段循环 `cpp:1062-1081` | 在 `if (h.baseline_index >= 0) continue;` 之后加 `if (IsAppLevelAction(h.action)) continue;` ⇒ **应用级行永不落进 `key_binder/bindings`** | **核心：从根上堵污染** |
| A4 | `CountEffectiveRows` `cpp:666-679` | 同步跳过应用级行 ⇒ 保住「wrote == 有效行数」与 `writes_bindings` 判定 | **易漏的一致性** |
| A5 | `OnNotify` 的 `NM_CLICK/NM_DBLCLK` `cpp:1998-2005` | 不进 `EnterCapturing`，改 `SetPopoverHint(L"该快捷键由程序注册，当前版本不支持自定义")` 后直接返回 | UI 层 |
| A6 | 右键「重置此行」`cpp:2021-2030` | 只读行不提供该项（复位无意义） | UI 层 |
| A7 | 列表单元格 | 两行仍**显示真实值** `Alt+,` / `Alt+.`，不得显示成空或「（自定义）」——否则用户更困惑 | UI 层 |

**先想好的回归判据**：在 `test/TestShortcutSettings/TestShortcutSettings.cpp` 追加 `t111_a_*` ——
以「已存在基线（`when: composing` 形态）+ 全默认表」为输入调 `SaveHotkeys`，断言落盘托管块内
`custom: open_quick_panel` / `custom: open_phrases` 出现次数 = **0**，且 `stats.wrote == stats.countEffective`。
**该判据改前必须为假**（当前必落这两行）⇒ 先跑一次证明判据能看见目标，再动手（agent memory 第 41 条：
「0 命中」不能直接当「已生效」证据）。

## 两件「待核」——别顺手扩大范围

1. **`trigger_deploy` 是否同属不生效**：`Action_TriggerDeploy` 内置 `custom: trigger_deploy`（Control+F5，
   `cpp:431-439`）。librime 无此 action（`key_binder.cc:38-43` 只有 6 个），理论上同样改了不生效。
   **本轮不并入方案 A**（用户拍板的是「快捷面板 + 用户短语」两行）⇒ 先查实 weasel 侧是否有别的路径处理它，
   要处理另开批次问用户。
2. **`sync` 是不是第二类假动作**（§3 缺口 2 仍未查）：`ShortcutSettingsT110.h:47-53` 的 `kActionKeys` 含 `sync`
   而 librime 不认。同样查实、不在本轮动。

## T104 安全版闭环（本轮另一次拍板）

「立即验证」只做**本地**：冲突检测 + 占用提示 + 生效条件文案（如需重启 WeaselServer / 需部署后生效）；
**不做 `SendInput` 真注入** ⇒ 绕开 AP-045-B 与 TSF 线程安全红线。`Action_VerifyHotkey = 28`
（`h:97`）注释已写明「T104 保留, 当前无 UI 入口」⇒ T104 要真接 UI。

## 实现轮待办清单（A1–A7 未写一行代码）

1. **CLAUDE.md 强制项，本轮还没做**：读 `Rime-设置说明书-重点快捷键.md` §5–§7、§9（含 §9.3 WeaselTSF
   `RELEASE_MASK` 坑）、§10；并读 `output/data/weasel.custom.yaml` 现状。
2. 按 A1→A7 落地；单文件多处改动**分次 `edit`**，每次一组精确 old/new，**改完必须 grep 逐处核对落盘**
   （agent memory 第 40 条：一次调用塞多组会被静默丢弃）。
3. 单测：先「改前判据为假」再「改后为真」+ 全量（上次基线 PASS=149 FAIL=0）。
4. 重建：`cmd /c tmp/build-batchM.cmd` + `msbuild test/TestShortcutSettings/TestShortcutSettings.vcxproj
   /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="F:\soft\00selfmade\rime_claude"`（不带尾反斜杠）；
   exe 真实落点 `Release\TestShortcutSettings.exe`，判 PASS 前核 mtime。
5. T104 / T105 先读 `design.md` §7.3 / §2.3 / §11 再动。
6. 提交前：打包 exe + 一次真机验收。
7. **YAML 层 binding 编辑与 C++ hotkey 代码不得同 commit**（`.claude/rules/hotkey-binding.md` 的
   anti-pattern）⇒ A3/A4（落盘语义）与 A5/A6/A7（UI）可能需拆 commit，按模块前缀 `WeaselServer` 提交。

**本文件定位**：T111 的**取证 + 定案**记录，不是阶段收尾交接。实现完成那一轮按 §9 写正式收尾 handoff，
并决定本文件并入收尾 handoff 还是留作取证附件 —— 别让仓库出现两份互相矛盾的「T111 现状」。
