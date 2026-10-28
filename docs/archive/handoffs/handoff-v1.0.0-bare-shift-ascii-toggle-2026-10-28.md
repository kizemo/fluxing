# 接力交接：Fluxing 1.0.0 — 裸 Shift 的 3 个缺陷已修并推送；T2（抓键 layout-aware）实现待做

> 本文件是**给下一个会话读的完整上下文**。下一个会话的 prompt 只负责指路，不重复本文件内容。
> 生成时间：2026-10-28（会话 `mvs_54823d93f0e0466f87ec8986862896dd`）
> 仓库：`F:\soft\00selfmade\rime_claude`（远端 `kizemo/Fluxing`，**不是 origin**）
> **当前 HEAD = `ceff6b6`**，本地与远端已同步（工作区只剩 ` m librime`，是子模块构建产物，**不可 git add**）

---

## 0. 一句话现状

1.0.0（用户实测反馈的 3 个缺陷修复 + 版本号 3 段化）**代码已全绿、安装包已出、已推送 `kizemo/Fluxing`**；
**沙箱权威门禁在本机跑不起来**（环境故障），**真机按键验收只有人能做**；
下一阶段仍是 **T2（设置界面抓键 layout-aware 改造，ADR 0011 `Proposed` + spec 083）**，其中夹着
**必须先实测再修**的疑点 **T20（CapsLock 打开时快捷键疑似全部失配）**。

---

## 1. 必读文档（按顺序，路径已给全）

1. `F:\soft\00selfmade\rime_claude\CONTEXT.md` — 身份 / 模块 / 词汇 / 不变量（现有 17 条）
2. `F:\soft\00selfmade\rime_claude\AGENTS.md` — 操作手册；§2.5 是沙箱 harness 触发条件，§4 是危险区
3. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` — 法则层（R1–R9 / P1–P8）
4. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` — 部署路径 / 注册表 / 三层覆盖架构
5. `F:\soft\00selfmade\rime_claude\Fluxing-二次开发内容清单.md` — **任务总账**：哪些做完了、哪些没做、下一阶段做什么
6. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` — 工程技能消费契约
7. 本轮专项（已落地）：
   - `F:\soft\00selfmade\rime_claude\docs\adr\0012-shift-ipc-response-body-and-session-local-ascii.md`（**Accepted**）
   - `F:\soft\00selfmade\rime_claude\docs\adr\0010-bare-shift-toggles-ascii-mode.md`（Accepted）
8. **下一阶段专项（待实现）**：
   - `F:\soft\00selfmade\rime_claude\.specify\specs\083-capture-keys-by-produced-character\{spec.md,plan.md,tasks.md}`
   - `F:\soft\00selfmade\rime_claude\docs\adr\0011-capture-keys-by-produced-character.md`（`Proposed`）
9. 上一份交接卡（**历史，勿推翻其结论**）：
   `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-v0.21.0.9-bare-shift-ascii-toggle-2026-10-28.md`

> **读清单第 5 条优先**——它是任务进度的唯一权威；spec 里的勾选框不可信，以 CHANGELOG + 门禁数字为准。

---

## 2. 本轮（1.0.0）已完成的东西

### 2.1 提交（已全部推送至 `kizemo/Fluxing`）

| 提交 | 标题 | 内容 |
|---|---|---|
| `3147919` | `style(fluxing): 整文件 clang-format 本次触及的 C++ 文件` | 8 个触及文件整文件格式化，含 2 处历史遗留违规（`WeaselServerImpl.cpp:74`、`LanguageBar.cpp:328`）。**只改格式，不改行为** |
| `ceff6b6` | `fix(fluxing): 裸 Shift 上屏候选、会话隔离与语言栏图标（ADR 0012）` | 功能 + 版本号 + 测试 + CHANGELOG + lessons L151 + ADR 0012（15 files, +538/−42） |

拆分提交的理由见 §6.1（CI lint 查整文件）；先例提交 `a023ee1`。

### 2.2 用户反馈的 3 个缺陷与根因（ADR 0012，Accepted）

| # | 用户反馈 | 根因（字节级取证，不是推理） |
|---|---|---|
| ③ | 候选窗开着时裸 Shift **不能**上屏第 2 / 3 候选（候选窗消失但仍处未确认状态） | 两个原因叠加：① **Shift 的 IPC 没有响应体**——`WeaselIPC/PipeChannel.cpp` 的 `_Receive` 只有在服务端真的写出 body 时才刷新客户端 buffer，服务端不回 body ⇒ 客户端继续解析**上一次**的响应，拿不到 `commit=`；② `ShiftUp` 的 `kSelectCandidate` 分支只 `select_candidate`，librime 在 `_auto_commit=false` 时 `Context::Select` → `composition().Forward()`，composition 仍在 ⇒ 候选窗消失、状态未确认 |
| ⑥ | 两个窗口各一会话，A 的裸 Shift 会**同步**切 B | `_Respond` 用 `m_global_ascii_mode`（yaml `global_ascii` 与运行时状态**混为一谈**）做跨会话传播，而且比较的是**缓存**的 `session_status.status.is_ascii_mode`；裸 Shift 走 `set_option` 直线、**不刷新缓存** ⇒ 下一个响应事件才触发传播，把切换过的会话复制给所有其它会话 |
| 要求 2 | 任务栏中英文状态图标**恒显「中」** | 与 ③ **同源**：客户端 `_status` 不刷新（因为没收到新 body）；`WeaselTSF/LanguageBar.cpp` 的 `GetText()` 改前固定返回 `L"WeaselTSF Button"`，`GetIcon()` 本来就按 `ascii_mode` 切 `IDI_EN`/`IDI_ZH` |

**改法**：

- `include/WeaselIPC.h`、`WeaselIPCServer/WeaselServerImpl.cpp`、`include/RimeWithWeasel.h`、
  `RimeWithWeasel/RimeWithWeasel.cpp`：`ShiftDown` / `ShiftUp` 增加 `EatLine eat`，服务端按 `OnChangePage`
  的写法回响应体。**线上协议不变**（命令号与参数布局不动）。
- `ShiftUp` 的 `kSelectCandidate` 分支在 `select_candidate` 之后补 `rime_api->commit_composition(session)`
  （幂等），再 `_Respond(ipc_id, eat)` → `_UpdateUI(ipc_id)`。
- 拆分 `m_global_ascii_configured`（yaml `global_ascii`，**不可变**）与 `m_global_ascii_mode`（运行时镜像）；
  **跨会话传播从 `_Respond` 移到 `SetOption`**，且只在 `global_ascii: true` 时生效。
  裸 Shift 走 `set_option` 直线 ⇒ **永不广播**。
- `include/ShiftStateMachine.h` 新增无依赖纯函数 `DecideAsciiModeScope(bool explicit_control, bool global_ascii_configured)`。
- `WeaselTSF/LanguageBar.cpp`：`GetText()` 随模式返回 `L"\u82f1"`（英）/ `L"\u4e2d"`（中）；
  `UpdateWeaselStatus()` 补发 `TF_LBI_TEXT`。
- 版本号：`build.bat` / `xbuild.bat` / `output/install.nsi` 改 3 段 `1.0.0`；
  Windows 资源与 `VIProductVersion` **仍 4 段**（`1.0.0.0`）。

### 2.3 验证数字（本机实跑，可复现）

| 门禁 | 结果 |
|---|---|
| `scripts\test-infra\run-test-suite.bat` | 整包 exit 0，日志 `F:\soft\00selfmade\tmp-dllcmp\test_suite3.log`（29,210 B） |
| `Release\TestShiftIPCStateMachine.exe` | **34 PASS / 0 FAIL**（原 28 + 新增 #18/#19/#20 共 6 条断言） |
| `Release\TestWeaselIPC.exe /verify-shift-body` | `[SHIFT] down=1 down_status=1 up=1 up_commit=1 up_status=1` ⇒ PASS |
| `Release\TestHotkeyBindings.exe`（**源** `output/data/**`） | `SUMMARY\|bindings=39\|reachable=39\|dead=0\|known_dead=0` + `RESULT\|PASS` |
| `Release\TestHotkeyBindings.exe`（**部署产物** 解包后的 `pkg_100`） | 同上 `bindings=39\|reachable=39\|dead=0\|known_dead=0` + `RESULT\|PASS` |
| clang-format 18.1.8（`C:\Python314\Scripts\clang-format.exe`，CI 同版本） | 8 个触及文件整文件 re-check 全 `CLEAN` |
| 编码校验（U+FFFD / C0 控制符 / EOL / BOM） | 全 PASS |
| 构建 | `cmd /c "...\build.bat data"` → `cmd /c "...\build.bat weasel installer"` 均 exit 0 |
| 安装包解包比对 | `pkg_100` 196 文件 / 160,750,970 B；`default.yaml` `4EDC29A11294`、`weasel.yaml` `84B7E5763408`、`rime_ice.schema.yaml` `EC4B0DD02AC2`、`t9.schema.yaml` `099A1AA7A6D1` **全部 MATCH** |

**CI**（run `40632079796`，分支 `Fluxing`）：`lint` ✅ success；`test` / `build (msbuild)` / `build (xmake)` ⏳ 见 §4.3。
（`sandbox-gate` job 永久排队，是基础设施缺口，不是代码问题，见 §4.1。）

### 2.4 交付物

- 安装包：`F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.0.0-installer.exe`
  （**42,748,195 B**，2026-10-28 12:07:05，SHA256 `B1C17AB840948374F95571E79A5ECF8CB5BBC556CFF07BE3440EED572EA0808F`）
- 3 个 custom.yaml 落在 `pkg_100\$R3\user1\fluxing\{double_pinyin_sogou,radical_pinyin,rime_ice}.custom.yaml`

---

## 3. 还没做的（下一阶段的起点）

| 项 | 状态 | 说明 |
|---|---|---|
| **真机按键验收** | ⏳ 等用户 | 8 条清单见 §3.1。**只有人能做**，代码侧已全绿 |
| **清单 T2**（设置界面抓键路径 layout-aware 改造） | 🟡 **已定稿，实现待做** | ADR 0011（`Proposed`）+ spec 083（R1–R7 / plan M1–M6 / tasks）。方案要点见 §3.2 |
| **清单 T20**（CapsLock 打开时快捷键疑似全部失配） | ⚠️ **疑点，未实测** | **必须先测量再决定怎么修**（ADR 0011 Q2），见 §3.3 |
| **清单 T19**（沙箱权威门禁本机跑不起来） | ⚠️ 环境故障 | 见 §4.1；**不阻塞提交** |
| **清单 T3/T4/T5** | ⏸ 未开工 | 见清单 §4.1 表格 |
| **共享 `data/` 目录其余文件升级** | ⚠️ 待用户操作 | 除 `default.yaml` 外仍是 2026-08 旧版；**装 1.0.0 安装包**后一致 |
| **1.0.0 进 `release/` 发布链** | ⚠️ 未做 | `release/` 里还没有它 |
| **T2.13 / T14** | ⚠️ 已知遗留 | `test/v0_19_0_30_e2e/build_e2e.bat` 写死 4 处机器路径；`build (xmake)` arm64 目标缺失 |
| **CI `sandbox-gate` job** | ⛔ 排队中 | 缺带 `sandbox` 标签、支持嵌套虚拟化的 Win11 自托管 runner（基础设施缺口，非代码问题） |

### 3.1 真机验收清单（用户照着敲，8 条）

| # | 操作 | 期望 |
|---|---|---|
| 1 | 中文模式、候选窗**没开** ⇒ 单独按一下 `Shift` | 切英文 |
| 2 | 再单独按一下 `Shift` | 切回中文 |
| 3 | 中文模式、**先打拼音**让候选窗弹出 ⇒ 单独按一下 `Shift` | **上屏第 2 个候选**，**不**切模式 |
| 4 | 同上，再按一次 `Shift`（候选仍在） | **上屏第 3 个候选**，**不**切模式 |
| 5 | 英文模式、候选窗**没开** ⇒ 按一下 `Shift` | 切回中文 |
| 6 | 任意状态下 `Shift+X` | 只出 `X`，**不**切模式 |
| 7 | 两个窗口各开一个会话 ⇒ 在 A 里按裸 `Shift` | 只切 A，B 不变 |
| 8 | 切到英文后看任务栏输入法图标 / 文字 | 图标变「英」、文字变「英」；切回中文变回「中」 |

> 第 3/4 条是本轮修的主缺陷；第 7 条是会话隔离；第 8 条是语言栏图标。

### 3.2 T2 方案要点（ADR 0011 `Proposed`，别再重新推导）

**问题**：设置界面抓键用**虚拟键码**拼 `accept:`，能造出 `Control+Shift+9` 这类**死绑定**。
**根因链**：本 fork 的 TSF 前端发的是「按键**产生的字符**」（`WeaselTSF/KeyEvent.cpp:51` 的 `ToUnicodeEx`），
librime 按 `(keycode, modifier)` **精确匹配**（`librime/src/rime/key_event.h:43`）。

**已坐实（对照实验）**：把 `output/data/default.yaml` 副本里的 `Control+Shift+numbersign`
改成 `Control+Shift+9` 再跑门禁 ⇒ `DEAD_KEYCODE`（前端实际发 `0x0028 parenleft`，不是 `0x0039`）+ `RESULT|FAIL`；
同文件 `Control+Shift+numbersign` 仍是 `REACHABLE`。脚本 `F:\soft\00selfmade\tmp-dllcmp\probe_t2_binding.py`。

**三条已核实的源码事实（实现时必须照抄）**：

- **F1 — 带修饰键的绑定只能用 Rime 有名字的字符。**
  `librime/src/rime/key_event.cc:51-82`：单字符 `accept:` 直接取字符码；带修饰键走 `RimeGetKeycodeByName`，
  而它（`librime/src/rime/key_table.cc:2026-2033`）对 `keys_by_keyval` 做**纯 strcmp** —— 该表**只有大写字母 A–Z**。
  ⇒ `Control+f` 连解析都失败；**只有 `Control+Shift+F` 可表达**（这正是内置表用 `Control+Shift+F` 的原因）。
- **F2 — 前端在算字符前会清掉 Ctrl / Alt。**
  `WeaselTSF/KeyEvent.cpp:44-51`：`ToUnicodeEx` 前把 `VK_CONTROL` / `VK_MENU` 置零
  ⇒ 字符大小写**只由 Shift 决定** ⇒ 不复刻这条就分不清 `Ctrl+F` 和 `Ctrl+Shift+F`。
- **F3 — CapsLock 打开时前端会多带一位 `LOCK_MASK`（疑点，未实测）** ⇒ 见 §3.3。

**ADR 0011 的 D1–D6**（已定稿，实现时逐条落地）：

- **D1 单一来源**：「字符 → Rime 键名」与「组合键 → 字符」放进**一个共享、无 GUI 依赖**的单元，抓键与门禁都消费它
- **D2 实时布局 + 复刻前端换算**：按**当前输入布局**算，规则与 `WeaselTSF/KeyEvent.cpp` 逐条一致
- **D3 修饰键按前端掩码精确输出**：Shift / Control / Alt；**Win 键直接拒绝并提示，不静默改写**；
  librime 无名的字符（`Ctrl`+小写字母）⇒ **拒绝 + 可操作提示**，绝不产出死绑定
- **D4 门禁即验收器**：界面能产出的每条绑定都要被 `TestHotkeyBindings` 判 `REACHABLE`
- **D5 不放宽门禁、不动 `librime/`**（宪法 P3；known-dead 基线保持空 = 最严）
- **D6 共享单元放 `include/KeyNameMapper.h`（header-only，不新建 .vcxproj）**

**两条抓键路径都已漂移，改造时都要切**：
① `FluxingConfigEditor/KeyRecorder.cpp:22-88`（`MapKeyCodeToLibrimeName`，纯 vk 映射，字母恒小写，`:118` 把 `win` 错拼成 `Control+`）
② `WeaselServer/ShortcutSettings.cpp:622-699`（`VKeyToName`）+ `:701-718`（`NormalizeCombo`，把 `VK_OEM_PLUS` 映射成 `equal`，而 ① 映射成 `plus`）

**落盘链路**：① → `WeaselDeployer/HotkeyEditorDialog.cpp:281` → `FluxingConfigEditor/HotkeyBinding.cpp:169`（`b.accept = rec.accept()`）→ yaml 的 `accept:`。

**不要顺手改的两处**：
- `WeaselServer/ShortcutSettings.cpp:339-402` 的内置表是**硬编码 accept 串**（不是生成），其中 `Control+Shift+F` 符合 F1
- `FluxingConfigEditor/HotkeyBinding.cpp:96-137` 的 `NormalizeAccept`（`comma`→`Comma`、`space`→`Space`）
  **与它自己的文档 `HotkeyBinding.h:80-83` 相反**（librime 只认小写名），且**全仓无调用点** ⇒ 保持不接线

### 3.3 T20（CapsLock 疑点）—— 先实测，别先改代码

- 现象（源码层）：`WeaselTSF/KeyEvent.cpp:18-19` 在 CapsLock 打开时 `|= ibus::LOCK_MASK`（`0x2`，`include/KeyEvent.h:225`）；
  `RimeWithWeasel/RimeWithWeasel.cpp:44-46` 的 `expand_ibus_modifier` 保留低 8 位 ⇒ **LOCK 位被带进 librime**；
  而 librime **精确比较 modifier**（`key_event.h:43-44`）⇒ **CapsLock 打开时所有没写 `Lock+` 的绑定疑似全部失配**。
- 全仓**无任何地方清除该位**（`KeyEvent.cpp:30-35` 只在「按下的就是 VK_CAPITAL 本身」时回退）。
- **下个会话第一件事就该是实测**：在真机上开着 CapsLock 按已配置的快捷键，看是否真的不触发。
  记录成门禁数字或 CHANGELOG 事实，再按 ADR 0011 Q2 的三个候选选一个：
  ① 前端清 `LOCK_MASK`（一行，改所有人的行为）／② 抓键产出 `Lock+` 前缀／③ 门禁纳入 caps 维度。

---

## 4. 环境障碍（不是代码问题，别去"修代码"）

### 4.1 沙箱权威门禁本机跑不起来（T19 / lessons L150）

`run-verify.cmd` 两轮均报 orchestrator exit 4，`C:\Users\Duanyi\sandbox-artifacts\rime\logs\` 下无 `boot.log`
⇒ Windows Sandbox 虚机未启动（Hyper-V vSwitch 异常）。重试需先 `Get-VM` 清理残留 VM，必要时重启 `hns` + `vmms`
（**有副作用**：可能打断本机网络 / Docker / WSL，**须先问用户**）。沙箱不可用时**如实报告**，不用高风险动作凑证据。

### 4.2 本机静默装机的副作用

TSF 注册会指向临时目录 ⇒ 本机**不做**静默安装；本机真机验收在别机。

### 4.3 CI 的真实状态

只看 4 个门禁 job：`lint` / `test` / `build (msbuild)` / `build (xmake)`。
`gh run list` 里会看到大量 `queued` 的旧 run —— 那是 `sandbox-gate` job 永久排队拖住整条 workflow，**不是代码问题**。
**不要 `gh run watch`**（会被 sandbox-gate 卡死）。

### 4.4 MiniMax Code 运行时反复重启

1 小时内重启 6 次会杀掉在飞后台任务（已提报 `F:\soft\00selfmade\mavis-bug-reports\2026-10-26-runtime-restart-loses-background-task.md`）。
长任务要么放后台并记 task_id，要么拆小。

---

## 5. 下一步可以直接用的命令

```powershell
# 构建 / 测试
cmd /c "F:\soft\00selfmade\rime_claude\build.bat data"
cmd /c "F:\soft\00selfmade\rime_claude\build.bat weasel installer"
cmd /c "F:\soft\00selfmade\rime_claude\scripts\test-infra\run-test-suite.bat"

# 定点验证：Shift IPC 确实回响应体（需先起 mock 服务端，否则管道可能被真实 WeaselServer 占用）
$repo='F:\soft\00selfmade\rime_claude'
$p=Start-Process -FilePath "$repo\Release\TestWeaselIPC.exe" -ArgumentList '/start' `
  -RedirectStandardError 'F:\soft\00selfmade\tmp-dllcmp\srv_stamp.txt' -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 2
& "$repo\Release\TestWeaselIPC.exe" '/verify-shift-body'   # => [SHIFT] PASS
Stop-Process -Id $p.Id -Force

# 快捷键门禁（出货配置；scan 的每个文件都是一份配置副本，源 + 产物都要扫）
& 'F:\soft\00selfmade\rime_claude\Release\TestHotkeyBindings.exe' --baseline `
  'F:\soft\00selfmade\rime_claude\test\TestHotkeyBindings\known-dead-bindings.txt' `
  'F:\soft\00selfmade\rime_claude\output\data\default.yaml' `
  'F:\soft\00selfmade\rime_claude\output\data\rime_ice.custom.yaml' `
  'F:\soft\00selfmade\rime_claude\output\data\double_pinyin_sogou.custom.yaml' `
  'F:\soft\00selfmade\rime_claude\output\data\radical_pinyin.custom.yaml'
# 期望：SUMMARY|bindings=39|reachable=39|dead=0|known_dead=0 + RESULT|PASS

# 推送（远端名是 kizemo，不是 origin）
git -C F:\soft\00selfmade\rime_claude -c http.proxy= push kizemo Fluxing

# CI 复核（只盯 4 个门禁 job，别 watch）
gh run list --branch Fluxing --limit 4
gh run view <run-id> --json status,jobs
```

---

## 6. 本轮踩过的坑（别重试）

### 6.1 CI 的 C++ lint 门禁只查「本次 push 触及的文件的整个文件」

任何被修改的 `.cpp/.h` 都必须**整文件** clang-format 干净（Chromium style / 80 列 / `SortIncludes: false`）。
本轮 `WeaselServerImpl.cpp:74`（spec 070 的中文注释超长）与 `LanguageBar.cpp:328`（`RegGetStringValue` 超长）
是**历史遗留违规**，但因为本轮动了这两个文件就必须一起修。
做法 = **拆两个提交**：先 `git checkout HEAD --` 复原成干净版本 → 整文件格式化 → `style(fluxing):` 提交 →
再把工作区的功能改动恢复回来 → `fix(fluxing):` 提交（先例 `a023ee1`）。
**工作区备份必须先落到仓外**（本轮用 `F:\soft\00selfmade\tmp-dllcmp\backup_fmt_cpp_v2`），且**不要删目录**
（本机 `Remove-Item` 被硬安全策略禁止，永久删除一律走 mavis-trash）。

### 6.2 集成测试必须先证明「管道另一端是我以为的那个进程」（**本轮新踩，最贵**）

- **事故 A（环境污染）**：套件里 `start "" /B "Release\TestWeaselIPC.exe" /start` 起的 mock **启动即退出**
  （日志只有 `handler ctor.` + `handler dtor: 0`，`m_counter=0` ⇒ 从未服务过任何会话），
  客户端实际连到了一个**真实 WeaselServer.exe**（响应体是 `action=status,ctx,config / status.schema_id=double_pinyin_sogou …`，
  而 mock 只回 `Greeting=Hello`）。
  旧二进制 Shift 不回 body ⇒ 客户端读到**陈旧 body**（恰好含 `status.ascii_mode=0`）⇒ `down_status=1` **假阳性**。
- **修复**：`verify_shift_body_main` 加**身份探针**（先 `ProcessKeyEvent`，断言响应含 `Greeting=`，
  否则以错误码 `-6` 明确报「管道被别的进程占用」）；`run-test-suite.bat` 加 `tasklist` 前置守卫
  （命中 `WeaselServer.exe` / `TestWeaselIPC.exe` 就 `FAIL=1` + `goto ipc_done`，**大声失败**而不是静默测错对象）。
- **推广**：凡是通过命名管道 / IPC 与「另一个进程」交互的集成测试，**先验对象身份，再验返回值**。

### 6.3 mock 的 `override` 必须写 `override` 关键字（**事故 B，更隐蔽**）

`TestRequestHandler::ProcessKeyEvent` 原本写的是 `UINT session_id`，基类虚函数是 `DWORD session_id`。
**Windows LLP64 下 `unsigned int` 与 `unsigned long` 是不同类型** ⇒ 那条**根本没有 override**，
服务端一直调基类空实现（`return FALSE`）⇒ 套件里历次都是 `server replies: 0`，
**键事件路径自 spec 026 以来从未被真正测过**。
已改为 `DWORD session_id` + `override` 关键字（让编译器直接指出签名不匹配）。
**推广**：写 mock 时每个 override 都加 `override`；跨平台 LLP64 尤其要注意 `UINT`/`DWORD`/`size_t` 的差异。

### 6.4 文档批量更新的正确姿势

「幂等 Python 补丁脚本 + 锚点强校验 + 自动保 EOL/BOM + 原子写」；写含中文文件后做 U+FFFD 全量校验 + C0 控制符校验。
补丁脚本在首个 `ANCHOR FAIL` 处 `sys.exit(3)` ⇒ **必须按文件拆批**，否则后面的文件一个都没改。

### 6.5 PowerShell / 工具链的老坑（沿用）

- `$R3` / `$R` 这类变量名会被 PowerShell 解释 ⇒ 路径要用**单引号**或换名（`$root`）。
- `build.bat` 第 19 行 `set "RELEASE_BUILD="` 会清空变量；**版本来源实际是 `env.bat`**，改版本必须同步 `env.bat`。
- `cmd /c "cd /d X && …"` 被拦 ⇒ 用完整路径调用。
- 7z 解 NSIS 包会同时产出 `$PLUGINSDIR` / `$R3` / `$TEMP`；custom.yaml 在 `$R3\user1\fluxing\`。
- `Get-ChildItem -Path <dir> -Include '*.h' -File` 不带 `-Recurse`/通配符会扫到空目录 ⇒ 用 `"<dir>\*.h"`。
- `grep` 工具的 `path` 参数会退化 ⇒ 严格限定时用 `Select-String -Path <具体文件>`。
- `include/WeaselTSF.h` 不存在（真实 `WeaselTSF/WeaselTSF.h`）；`WeaselTSF/ResponseParser.cpp` 不存在（真实 `WeaselIPC/ResponseParser.cpp`）。

---

## 7. 决策备忘（下一会话别推翻）

1. `accept:` 键名必须写「组合键产生的字符」的键名（WeaselTSF 语义），`send:` 必须是单个 ASCII 字符
2. known-dead 基线清零 = 最严阈值，空基线不是调松
3. 门禁扫描范围必须覆盖配置的全部副本（源 + 部署产物各扫一遍）
4. ADR 已 Accepted 后**不得改正文**（要改就新 ADR supersede）；`Proposed → Accepted` 的状态迁移 + 开放问题裁决记录是合法的
   （ADR 0011 落地后再转）；新术语进 `CONTEXT.md` §3，改不变量要走宪法修正流程；实现计划放 `.specify/specs/NNN-*/`
5. 注释里凡是断言「生效/不生效/数量」的，都必须有门禁或测试数字背书
6. 文档批量更新走「幂等 Python 补丁脚本 + 锚点强校验 + 自动保 EOL/BOM」（见 §6.4）
7. spec 的 tasks 勾选框不可信 —— 以 CHANGELOG + 门禁数字为准
8. 验收门禁要指向**部署产物**再跑一遍（源绿 ≠ 产物绿）
9. **提交消息 / 文档里断言的数字必须真实落地**；未实跑的结论不得写死
10. **子代理的结论必须逐条复核**
11. **不动已 Accepted 的 ADR 正文**；也不要顺手改 `ShortcutSettings.cpp:339-402` 的内置表
12. **本机静默装机不跑**（TSF 注册副作用）；沙箱不可用时**如实报告**，不用高风险动作凑证据（L150）
13. 用户要求：按清单推进开发并**持续更新清单**；到需要实机验收的节点就**打包 exe、告知路径、提醒验收**；
    在**合适的实机节点自动 commit，不用请示**
14. **版本号对外 3 段 = `1.0.0`**（安装包文件名 / `FileVersion` / `DisplayVersion` / `CHANGELOG`）；
    Windows 资源与 `VIProductVersion` 仍 4 段（`1.0.0.0`）；版本化 shim 名保留 `${WEASEL_BUILD}`
15. **任何 Shift 类 IPC 都必须回响应体** —— PipeChannel 只在服务端真的写 body 时才刷新客户端 buffer；
    不回 body ⇒ 客户端解析**上一次**的响应（陈旧 body）。这是 IPC 层的协议事实，不是实现细节
16. **yaml 配置与运行时状态必须拆成两个变量** —— 配置不可变、运行时有镜像；
    跨会话传播只能由**显式动作**（`set_option`）触发，且只在配置开启时生效；**响应路径不做隐式传播**

---

## 8. 用户当前待办

- 装 `F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.0.0-installer.exe`
  （42,748,195 B，SHA256 `B1C17AB840948374F95571E79A5ECF8CB5BBC556CFF07BE3440EED572EA0808F`）
- 按 §3.1 八条做真机按键验收
- 装完顺手把共享 `data/` 目录升级到 1.0.0（清单 T17）
- **顺带可帮下个会话的忙**：开着 CapsLock 按一下已配置的快捷键，看是否真的不触发（清单 T20 / ADR 0011 Q2 的实测输入）
