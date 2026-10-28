# 接力交接：Fluxing 1.0.0 — 裸 Shift 的 3 个缺陷已修并推送；T2 · M1 共享单元 + M2 抓键改造 + M3 第二条路径 + M4 抓键产物专项 + M6 打包已落地；只剩实机验收 / ADR 转正 / T20 实测

> 本文件是**给下一个会话读的完整上下文**。下一个会话的 prompt 只负责指路，不重复本文件内容。
> 生成时间：2026-10-28（会话 `mvs_54823d93f0e0466f87ec8986862896dd`）
> 仓库：`F:\soft\00selfmade\rime_claude`（远端 `kizemo/Fluxing`，**不是 origin**）
> **最新功能提交 = `9e07743`**（T2 · M4 抓键产物专项 fixture + 门禁保真模式；M6 无产品代码改动，改的是**装器载荷与交付物**），本卡自身的 docs 提交紧跟其后。本地与远端已同步（工作区只剩 ` m librime`，是子模块构建产物，**不可 git add**）

---

## 0. 一句话现状

1.0.0（用户实测反馈的 3 个缺陷修复 + 版本号 3 段化）**代码已全绿、安装包已出、已推送 `kizemo/Fluxing`**；
**沙箱权威门禁在本机跑不起来**（环境故障），**真机按键验收只有人能做**；
**T2（设置界面抓键 layout-aware 改造，ADR 0011 `Proposed` + spec 083）的 M1（共享键名单元 `include/KeyNameMapper.h`）、M2（抓键改造 `FluxingConfigEditor/KeyRecorder.cpp`）、
**M3（第二条路径 `WeaselServer/ShortcutSettings.cpp`）、M4（抓键产物专项 fixture + 门禁保真模式）、M6（打包：装器载荷 `data\build\` 全量重编 + 新装包）已落地并推送**，只剩实机验收与 ADR 转正；
下一阶段仍是 T2，其中夹着
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
| `ed7f5ba` | `docs(fluxing): 更新任务总账与交接卡到 1.0.0 状态` | 1.0.0 阶段的 docs 提交（交接卡 + 任务总账） |
| `3dd2b06` | `refactor(fluxing): 抽出共享键名映射单元 include/KeyNameMapper.h（ADR 0011 D1）` | T2 · M1：共享键名单元 + 键名表迁移 + 门禁 `--selfcheck-mapper`（8 files, +1913/−1312）；**内部重构，非用户可见 ⇒ 不记 CHANGELOG（P4）** |

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
| **清单 T2**（设置界面抓键路径 layout-aware 改造） | 🟡 **M1 + M2 + M3 + M4 + M6 已完成，只剩实机验收** | ADR 0011（`Proposed`）+ spec 083（R1–R7 / plan M1–M6 / tasks）。M1 = 共享键名单元 `include/KeyNameMapper.h`（D1/D6），已提交 `3dd2b06`；M2 = 抓键改造 `FluxingConfigEditor/KeyRecorder.cpp`（D1/D3/D6），已提交 `7e2f4be`；M3 = 第二条路径 `WeaselServer/ShortcutSettings.cpp`（D1/D3），已提交 `2784ace`；M4 = 抓键产物专项 fixture + 门禁保真模式 `--selfcheck-recorder`（D3），已提交 `9e07743`；M6 = 打包（装器载荷 `data\build\` 全量重编 + 新装包 `fluxing-1.0.0-installer.exe`）；方案要点见 §3.2，M1 记录见 §9，M2 记录见 §10，M3 记录见 §11，M4 记录见 §12，M6 记录见 §13 |
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

**M1 已落地（`3dd2b06`）**：D1（单一来源）与 D6（共享单元放 `include/KeyNameMapper.h`，header-only）已完成；**D2/D3/D4/D5 尚未开始**（抓键两条路径仍是原实现）。详见 §9。

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

**run `40632079796`**（1.0.0 阶段 `ceff6b6`；其后的 docs 提交 `ed7f5ba` 对应 run `40632830628`）：

| job | 结论 |
|---|---|
| `lint` | ✅ success |
| `test` | ✅ success |
| `build (msbuild)` | ✅ success |
| `build (xmake)` | ✅ success |

**run `40648960781`**（T2 · M1，`3dd2b06`）：

| job | 结论 |
|---|---|
| `lint` | ⏳ in_progress |
| `test` | —（job 未出现） |
| `build (msbuild)` | —（job 未出现） |
| `build (xmake)` | —（job 未出现） |

> `sandbox-gate` job 永久排队，不计入上表。

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

# 共享键名单元自检（M1 新增；CI test job 也会跑）
& 'F:\soft\00selfmade\rime_claude\Release\TestHotkeyBindings.exe' --selfcheck-mapper
# 期望：MAPPER|table names=1097 / MAPPER|chords=4032|refused=652 / MAPPER-SUMMARY|failures=0 / RESULT|PASS

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

### 6.6 clang-format 干净 ≠ 能编译（本轮最隐蔽的一颗雷）

**M3 的行级补丁把 `FluxingConfigEditor/KeyRecorder.cpp` 写坏了**：吞掉了两处
`display_label_.SetWindowTextW(...)` 调用（只剩一个无副作用的表达式），
还留下一个孤立的 `switch (error) {` ⇒ `namespace` 不闭合 ⇒ **该文件编译不过**。

**为什么三道门禁都没拦住**：

- `clang-format --dry-run --Werror` 对**语法坏掉**的文件照样「干净」
  （它只比格式，不解析语义）⇒ 格式门禁过了；
- 门禁工程 `TestHotkeyBindings` **不编译**这个文件 ⇒ 门禁自检也过了；
- 本轮只单独编了 `WeaselServer.vcxproj`，`FluxingConfigEditor` 那一支**根本没编**。

⇒ **凡是本轮改动过的 C++ 文件，必须逐个被某个编译单元编到**；
只编自己碰过的那一个工程是不够的。**整解编译（`weasel.sln`）是唯一省心的做法**。

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
  （**M6 新包**：42,756,263 B, SHA256 F040D989872FB8D135917B9416E40DE4D3010002EB669EF43AA98544A8344A8A；
  上一版留档 `fluxing-1.0.0-installer.pre-m4.exe`，42,748,195 B, SHA256 B1C17AB840948374F95571E79A5ECF8CB5BBC556CFF07BE3440EED572EA0808F）
- 按 §3.1 八条做真机按键验收
- 装完顺手把共享 `data/` 目录升级到 1.0.0（清单 T17）
- **顺带可帮下个会话的忙**：开着 CapsLock 按一下已配置的快捷键，看是否真的不触发（清单 T20 / ADR 0011 Q2 的实测输入）
- **T2 只剩验收**：M6 已打包（见 §13），等真机按键结果再把 ADR 0011 `Proposed` → `Accepted`（T6.4）

---

## 9. T2 · M1 里程碑（2026-10-28 已完成并推送 · ADR 0011 D1/D6）

### 9.1 做了什么

把「键名 ↔ 键码」「accept 串构造/解析」「VK → 字符」
三件事从**两份重复实现**（前端 `WeaselTSF/KeyEvent.cpp` +
门禁 `test/TestHotkeyBindings/TestHotkeyBindings.cpp`）抽成一个 **header-only 共享单元**
`include/KeyNameMapper.h`（`namespace fluxing`，无 GUI 依赖，符合 D1/D6）。

| 文件 | 动作 |
|---|---|
| `include/KeyNameMapper.h` | **新增**（490 行）：`KeycodeByName` / `ModifierByName` / `KeyName` / `ModifierName`、`AcceptError` + `AcceptErrorText`、`AcceptKey`、`ParseAccept`、`IsModifierKeyCode`、`AppendModifiers`、`BuildAccept`（std::string / std::wstring 两重载）、`TranslateKeycode`、`ResolveCharacter` |
| `include/keysyms_generated.inc` | **从** `test/TestHotkeyBindings/keysyms_generated.inc` **迁入**（`git mv`，93% 相似）：加 include guard + `namespace fluxing { namespace keysyms {`，函数内 `static` 表（C++11 安全），**保留 librime 原表序** |
| `test/TestHotkeyBindings/gen-keysyms.ps1` | 输出路径改 `include\keysyms_generated.inc`；新增 `$ordered`（librime 原表序）；`-Check` 仍可用 |
| `WeaselTSF/KeyEvent.cpp` | 整文件重写为「只算掩码 + 调 `fluxing::ResolveCharacter(vkey, kinfo, keyState, NULL)`」；**掩码逻辑逐字保留** |
| `test/TestHotkeyBindings/TestHotkeyBindings.cpp` | include 改 `<KeyNameMapper.h>`；删本地 `FindKey` / `FindModifier`；`KeyName` / `ParseRimeKey` 转调共享单元；表自检改用 `fluxing::ModifierByName` / `fluxing::KeycodeByName` |
| `test/TestHotkeyBindings/TestHotkeyBindings.vcxproj` | `<ClInclude Include="..\..\include\keysyms_generated.inc" />` |
| `scripts/test-infra/run-test-suite.bat` | 在 `gen-keysyms.ps1 -Check` 之后新增 `"Release\TestHotkeyBindings.exe" --selfcheck-mapper < nul`，失败计入 `FAIL` ⇒ CI `test` job 自动覆盖 |
| `.specify/specs/083-capture-keys-by-produced-character/tasks.md` | T1.1–T1.5 / T5.1–T5.4 勾选 + 实跑数字 |

### 9.2 新增的门禁模式 `--selfcheck-mapper`

五类交叉验证（失败即打 `MAPPER-FAIL|..`，最后打 `MAPPER-SUMMARY|failures=N`）：

1. **表名回环**：每个键名 → 键码 → 回键名，须回到原名
2. **accept 串回环**：构造的 `accept:` → 解析 → 回构造，须逐字相同
3. 18 个固定和弦的构造/解析比对
4. 14 个 librime 解析边界（空串 / 无名键 / 未知修饰键 / 修饰键自身…）
5. **前端枚举**：vkey `0x08`–`0xFE` × (extended / shift / ctrl / alt / keyup) 共 **7904 次**尝试，
   与 `ResolveCharacter` 逐项对账（实测 `chords=4032`、`refused=652`）

**一条重要的实现教训（初版返工过一次）**：
「修饰键本身不能构成 `accept:`」**必须拒绝**（`AcceptError::kAcceptModifierKey`），
但**拒绝必须与 `IsModifierKeyCode(keycode)` 一致才算通过**——
初版把「被 BuildAccept 拒绝」当成失败，误报了 11 条
`MAPPER-FAIL|build|Shift_L|modifier-key`。对应 ADR 0011 D6。

### 9.3 本机实跑数字（全绿）

| 门禁 | 结果 |
|---|---|
| `Release\TestHotkeyBindings.exe --selfcheck-mapper` | `MAPPER\|table names=1097`、`MAPPER\|chords=4032\|refused=652`、`MAPPER-SUMMARY\|failures=0`、`RESULT\|PASS`、exit 0 |
| 仓库源门禁（`output/data/**`） | `SUMMARY\|bindings=39\|reachable=39\|dead=0\|known_dead=0\|known_dead_keys=0\|expectations=0\|mismatches=0\|unused=0` + `RESULT\|PASS`（**与改动前基线逐字相同**） |
| 负控（故意塞死绑定） | `SUMMARY\|bindings=9\|reachable=3\|dead=6\|...\|expectations=9\|mismatches=0` + `RESULT\|PASS` |
| 全量套件 `scripts\test-infra\run-test-suite.bat` | `=== ALL TESTS PASSED ===`（含 mapper 自检、门禁两次、`gen-keysyms.ps1 -Check`、`TestWeaselIPC [SHIFT] PASS`）；日志 `F:\soft\00selfmade\tmp-dllcmp\test_suite_m1.log` |
| 完整解决方案 `weasel.sln` Release/Win32 | exit 0；产出 `output\weasel.dll` / `WeaselServer.exe` / `Win32\WeaselDeployer.exe` / `WeaselSetup.exe` / `Win32\FluxingPhrasesDialog.exe` + 21 个测试 exe；日志 `m1_sln_build2.log` |
| 部署产物门禁（源 YAML） | 装器载荷 `pkg_100` → `bindings=39\|reachable=39\|dead=0` + `RESULT\|PASS` |
| 部署产物门禁（**重新编译后**） | `rime_deployer.exe --build <user> <shared>` 全量重编 12 个 schema → `SUMMARY\|bindings=337\|reachable=337\|dead=0\|known_dead=0` + `RESULT\|PASS`、exit 0（脚本 `F:\soft\00selfmade\tmp-dllcmp\m1_deploy_check.py`） |
| clang-format 18.1.8（CI 同版本） | 3 个 C++ 文件整文件 `--dry-run --Werror` exit 0 |

### 9.4 M1 期间新坐实的源码事实（M2/M3 必须照抄）

- **F4 — librime 键表只有大写 `A–Z`** ⇒ `Control+a` **连解析都失败**
  （`RimeGetKeycodeByName` 对 `keys_by_keyval` 做纯 strcmp）。
  ⇒ 抓键遇到小写字母 + 修饰键，必须产出 `Control+Shift+F` 这种形态，或拒绝并提示。
- **F5 — 修饰键本身不能构成 `accept:`** ⇒ 必须**拒绝**（`kAcceptModifierKey`），
  **绝不静默改写**（D6）。
- **F6 — `ToUnicodeEx` 的第二个实参是 `UINT(kinfo)`**（= lParam 低 32 位，
  **不是**纯扫描码）⇒ 共享单元必须原样保留。
- **F7 — 前端掩码的取法**：`SHIFT ← keyState[VK_SHIFT]&0x80`；
  `LOCK ← keyState[VK_CAPITAL]&0x01`；`CONTROL ← keyState[VK_CONTROL]&0x80`；
  `ALT ← keyState[VK_MENU]&0x80`；`RELEASE ← kinfo.isKeyUp`；
  且 `vkey==VK_CAPITAL && !kinfo.isKeyUp ⇒ mask ^= LOCK_MASK`（**异或**，不是清位）。

### 9.5 附带发现（既有遗留，不属于 M1）

装器载荷里带着**陈旧的** `data\build\*.schema.yaml`（ADR 0009 之前的旧编译产物），
直接对 `data\build\` 跑门禁仍会报 **8 条死绑定**
（`Shift_L` / `Shift_R` / `Shift+l` / `Shift+r` / `Control+Shift+9` / `Control+Shift+0` 等）。
根因机制：`rime_deployer` 的 `prebuilt_data_dir = shared_data_dir / "build"` ⇒ 会**复用**陈旧编译产物。
⇒ 打包里程碑（M6）必须先把载荷里的 `data\build\` 剔除再全量重编，
否则用户装完仍然是死绑定。

### 9.6 M1 之后的下一步

> **状态更新（2026-10-28）：M2 / M3 / M4 也已完成，见 §10 / §11 / §12。**
> 下文是 M1 完成当时写下的计划；里程碑编号以 spec 083 `tasks.md` 为准（单一真相源）。

- **M2** ✅ 两条抓键路径改用共享单元 ——
  ① `FluxingConfigEditor/KeyRecorder.cpp:22-88`（`MapKeyCodeToLibrimeName`）→ **已改**（§10）；
  ② `WeaselServer/ShortcutSettings.cpp:622-699`（`VKeyToName`）与 `:701-718`（`NormalizeCombo`）→ **已改**（§11）
- **M3** ✅（spec 083）：第二条路径 `WeaselServer/ShortcutSettings.cpp` 切到共享单元，
  消除 `equal` vs `plus` 矛盾 —— 见 §11
- **M4**（spec 083）：抓键产物专项 fixture + 负控
- **M5**：门禁复跑 —— M1 时已做完，M2 / M3 后各复跑一遍（§10.5 / §11.5）
- **M6**：打包里程碑（含 §9.5 的 `data\build\` 清理）
- **T20 实测**仍未做，是 ADR 0011 Q2 的必需输入。

---

## 10. T2 · M2 里程碑（2026-10-28 已完成并推送 · ADR 0011 D1/D3/D6）

### 10.1 做了什么

把「组合键 → `accept:` 串」这个决策从 UI 层搬进共享单元：

- `include/KeyNameMapper.h` 新增 `fluxing::CaptureAccept(vkey, kinfo, key_state, hkl)`
  与 `fluxing::CapturedAccept`（`ok` / `pattern` / `keycode` / `modifier` / `error`），
  另加 `kAcceptWinKey`、`kAcceptNoCharacter` 两个错误码、`ParseAcceptWide()`
  （宽字符解析）与 UTF-8↔UTF-16 辅助（`NarrowToWide` / `WideToNarrow`，
  顺带修掉 `BuildAccept` 宽字符重载的逐字节加宽）。
- `FluxingConfigEditor/KeyRecorder.cpp` **整文件重写**：本地
  `MapKeyCodeToLibrimeName`（虚拟键 → 键名自造表）整段删除，改调 `CaptureAccept`；
  字符与修饰键取自**同一次** `GetKeyboardState()` 快照。
- `win → Control+` 的误拼删除：Win 键（按下或按住）一律**拒绝**并中文提示。
- 拒绝原因在 UI 层翻译成简体中文（宪法 P2）；共享单元错误码保持 ASCII。

### 10.2 门禁新增两类检查

`Release\TestHotkeyBindings.exe --selfcheck-mapper` 增加：

- **第 6 类「捕获一致性」**：vkey 0x08–0xFE × extended/shift/ctrl/alt 全枚举，
  逐条比对 `CaptureAccept` 的 `(keycode, modifier)` 与前端 `ConvertKeyEvent`
  的 `(keycode, modifier)`，并要求 `pattern` 经 `ParseAcceptWide` 解析回来与
  前两者**完全一致**；「拒绝」也必须有理由（前端那一对同样不可表达）。
- **第 7 类 8 个固定断言**：`Control+Shift+F` ⇒ `Shift+Control+F`；
  `Control+f` ⇒ `kAcceptNoName`；`Win+F` ⇒ `kAcceptWinKey`；
  裸 `a` ⇒ `a`；`Shift+A` ⇒ `Shift+A`；`space` / `Return` / `F1` 原样。
- **死键兜底**：枚举时同一和弦连解两次，结果不一致即判为「该布局含死键」
  并跳过该和弦（计数 `dead_key_skipped`），避免把布局噪声误判成抓键缺陷。

### 10.3 附带修掉的一个既有缺陷

对话框的占位提示原本是**乱码**：源码里的 `L"\xe7\xa2\x9c\xe6\x8c\x91..."`
解出来是「亟慑组合键...」而不是「请按组合键...」（两个字符的 UTF-8 转义写错了）。
本次重写时改为正确的「请按组合键...」。

### 10.4 未决依赖

修饰键只输出 **Shift / Control / Alt**、**不输出 Lock** —— CapsLock 维度属清单
**T20**（ADR 0011 Q2），必须先实测再定，本次不预判。若实测确认失配，
修复点仍在前端掩码（`WeaselTSF/KeyEvent.cpp`），不是对话框。

### 10.5 提交与本机实跑

| 提交 | 内容 |
|---|---|
| `351b97d` | `style(fluxing): 格式化 FluxingConfigEditor/KeyRecorder.cpp 以通过 clang-format 门禁` |
| `7e2f4be` | `refactor(fluxing): 抓键路径改吃共享单元 include/KeyNameMapper.h（ADR 0011 D1/D3/D6）` |

- 门禁自检 `Release\TestHotkeyBindings.exe --selfcheck-mapper`：`MAPPER|table names=1097`、`MAPPER|chords=4032|refused=652`、`MAPPER|captured=1716|refused=348|dead_key_skipped=0`、`MAPPER-SUMMARY|failures=0`、`RESULT|PASS`
- 全套件 `scripts/test-infra/run-test-suite.bat`：`=== ALL TESTS PASSED ===`（exit 0）；源门禁 `SUMMARY|bindings=39|reachable=39|dead=0|known_dead=0|expectations=0|mismatches=0|unused=0`、`RESULT|PASS`；`keysyms_generated.inc` 与 librime `key_table.cc` 一致（1097 键名 / 17 修饰键）
- 部署产物复跑（装器载荷重编 12 个 schema）：`LAYOUT|00000409|00000409`、`LAYOUT-PROBE|Shift+quote=0x22 OK`、`SUMMARY|bindings=337|reachable=337|dead=0|known_dead=0|expectations=0|mismatches=0|unused=0`、`RESULT|PASS`
- `WeaselDeployer`（含 `FluxingConfigEditor/KeyRecorder.cpp`）Release/Win32 编译 exit 0
- clang-format 19.1.5 整文件 `--dry-run --Werror`：`include/KeyNameMapper.h`、`test/TestHotkeyBindings/TestHotkeyBindings.cpp`、`FluxingConfigEditor/KeyRecorder.cpp` 全部干净


---

## 11. T2 · M3 里程碑（2026-10-28 已完成 · ADR 0011 D1/D3）

> 最新功能提交 = `2784ace`。M3 只动「第二条抓键路径」，
> **没有改内置表**（spec 083 T3.3），也没有改任何 `output/data/**` 里的 yaml。

### 11.1 做了什么

把**第二条抓键路径**也切到共享单元：

- `WeaselServer/ShortcutSettings.cpp` **删掉**自造的 `VKeyToName()` 与
  `NormalizeCombo()`（不再存在第二套键名 / 组合键拼法），改调
  `fluxing::CaptureAccept()`。
- 低阶键盘钩子（`LowLevelKeyboardProc`）用 `KBDLLHOOKSTRUCT` 造 `BYTE[256]`
  快照（`state[vk]=0x80` + Shift / Ctrl / Alt / Win 位），`KeyInfo` 由
  `scanCode<<16 | LLKHF_EXTENDED` 构造，HKL 取 `GetKeyboardLayout(0)`
  ⇒ 与对话框那条路径**同源**。
- `include/KeyNameMapper.h` 新增 `fluxing::AcceptErrorMessage()`（两条路径共用的
  中文拒绝文案）与 `fluxing::AcceptEquals()`（accept 串的**语义比较**）。
- `WeaselServer/ShortcutSettings.h` 删 `NormalizeCombo` / `VKeyToName` 声明，
  加 `SetCaptureHint()`。

### 11.2 为什么冲突检测要改成语义比较

共享单元按 librime 的位序产出 `Shift+Control+F`，而内置表（**按 T3.3 一个字没动**）
写的是 `Control+Shift+F` —— 拼写不同、语义相同。librime 比较的是 keycode + modifier
掩码，不是拼写，所以 `CheckConflict()` 的两处字符串精确比较改成了
`fluxing::AcceptEquals()`（解析成 keycode + modifier 再比）。
顺带让旧 yaml 里的 `Control+Shift+F` 与新抓的 `Shift+Control+F` 也能判成重复。

### 11.3 顺带消掉的三处漂移

| 场景 | 旧行为 | 新行为 |
|---|---|---|
| `Shift` + `=` | 写 `equal`（**永不触发**） | 写 `Shift+plus` |
| `Ctrl` + 小写字母 | 写出 librime 解析不了的名字（**死绑定**） | **拒绝** + 中文提示 |
| `Win` 键 | 写 `Super+`（**伪绑定**） | **拒绝** + 中文提示 |

### 11.4 用户可见变化

- 抓到的组合键实时显示在面板 popover 与底部状态行（`SetCaptureHint()`）。
- **拒绝时清空 pending chord**（所见即所存），并显示中文原因。

### 11.5 门禁与验收数字（本机实跑，可复现）

- 门禁自检新增第 8 类「第二路径语义一致性」：14 条内置表用例 + `equal` /
  `Shift+plus` 矛盾用例 ⇒ `MAPPER|builtin_equivalents=15`；
  `MAPPER-SUMMARY|failures=0` + `RESULT|PASS`。
- 源门禁：`SUMMARY|bindings=39|reachable=39|dead=0|known_dead=0|known_dead_keys=0|`
  `expectations=0|mismatches=0|unused=0` + `RESULT|PASS`。
- 部署产物复跑（装器载荷重编 12 个 schema）：
  `SUMMARY|bindings=337|reachable=337|dead=0|...` + `RESULT|PASS`。
- 全套件 `scripts/test-infra/run-test-suite.bat`：`=== ALL TESTS PASSED ===`。
- 完整解决方案 Release/Win32 编译 exit 0。
- 5 个改动文件 clang-format 19.1.5 整文件 `--dry-run --Werror` 干净。

### 11.6 下一步

- **M4**：已完成，见 §12（抓键产物专项 fixture + 门禁保真模式）。
- **M6**：打包（含 §9.5 的 `data\build\` 清理）+ 实机验收
  ⇒ ADR 0011 `Proposed` → `Accepted`。
- **T20** 仍未实测，是 ADR 0011 Q2 的必需输入。

---

## 12. T2 · M4 里程碑（2026-10-28 已完成 · ADR 0011 D3）

> 最新功能提交 = `9e07743`。M4 只动**测试与门禁**，没有改任何产品代码，
> 也没有改 `output/data/**` 里的 yaml ⇒ 用户侧行为零变化。

### 12.1 做了什么

- **两个 fixture 文件**（`test/TestHotkeyBindings/fixtures/`）：
  - `recorder-output.yaml`：20 条**真实抓键产物** + 负控 `Control+Shift+9`；
  - `recorder-output.expect.txt`：逐条 `REACHABLE` + 负控 `DEAD_KEYCODE`。
- **门禁新增两个模式**（`test/TestHotkeyBindings/TestHotkeyBindings.cpp`）：
  - `--dump-recorder`：导出抓键真实产出的 accept（850 条）——
    **fixture 就是从它生成的**，所以 fixture 不可能与实现漂移；
  - `--selfcheck-recorder`：保真自检（见 §12.2）。
- **CI 接线**：`scripts/test-infra/run-test-suite.bat` 增加 2 次门禁调用
  （逐条判定 + 保真自检）⇒ 自动进 CI `test` job，**不必改 `.github/workflows`**。

### 12.2 保真自检做三件事

1. 抓键能产出的每一个 accept 都必须**可达**（前端真发这个 keycode + modifier）
   ⇒ 抓键**永不产出死绑定**（ADR 0011 D3 的核心不变量）。
2. fixture 里每条判 `REACHABLE` 的 accept 必须**真能被抓出来**
   ⇒ fixture 不会退化成装饰性静态清单。
3. 负控 `Control+Shift+9` 必须**抓不出来** ⇒ 门禁不会被改瞎。

### 12.3 两个必须记住的细节

- **accept 串用 librime 位序拼写**（`Shift` → `Control` → `Alt`）：
  `Shift+Control+F`、`Shift+Control+numbersign`、`Shift+plus`（**不是** `Shift+equal`）。
  保真检查是**字符串**比对，写成 `Control+Shift+F` 会判「抓不出来」。
- **负控不放进抓键产物集合**：抓键永远产不出 `Control+Shift+9`
  （它产的是 `Shift+Control+parenleft` —— 前端发的是字符 `(`，不是键 `9`）。

### 12.4 门禁与验收数字（本机实跑，可复现）

- 保真自检：`RECORDER|products=850|rows=21|produced=20|not_produced=0|negative_ok=1`
  + `RECORDER-SUMMARY|failures=0` + `RESULT|PASS`。
- fixture 判定：`SUMMARY|bindings=21|reachable=20|dead=1|known_dead=0|mismatches=0|unused=0`
  + `RESULT|PASS`；负控详情：前端发的是 `0x0028 (parenleft)`，不是 `0x0039 (9)`。
- 源门禁（4 个 yaml）：`bindings=39|reachable=39|dead=0` + `RESULT|PASS`。
- 映射表自检：`MAPPER-SUMMARY|failures=0`（`builtin_equivalents=15`、`captured=1716`）。
- 全套件 `scripts/test-infra/run-test-suite.bat`：`=== ALL TESTS PASSED ===`。
- 完整解决方案 Release/Win32 编译 exit 0。
- 1 个改动文件 clang-format 19.1.5 整文件 `--dry-run --Werror` 干净。

### 12.5 下一步

- **M6**：已完成打包（见 §13）；剩下实机验收 ⇒ ADR 0011 `Proposed` → `Accepted`。
- **T20** 仍未实测，是 ADR 0011 Q2 的必需输入。

---

## 13. T2 · M6 里程碑（2026-10-28 已完成 · ADR 0011 D3）

> 最新功能提交仍是 `9e07743` —— M6 没有产品代码改动，改的是**装器载荷**与**交付物**。

### 13.1 做了什么

| 项 | 落点 | 状态 |
|---|---|---|
| 版本号 | `env.bat`：`FLUXING_VERSION=1.0.0` / `WEASEL_BUILD=0` / `RELEASE_BUILD=1`（gitignored） | ✅ |
| 装包 | `build.bat weasel installer` ⇒ `output\archives\fluxing-1.0.0-installer.exe` | ✅ |
| 载荷重编 | 装器载荷 `data\build\` **29 个文件全量重编**（§9.5 那条遗留） | ✅ |
| 产物复跑 | 门禁指向**解包后的载荷**再跑一遍 | ✅ |

### 13.2 §9.5 那条遗留是怎么处理的（记住这条）

`output\install.nsi` 用 `File "data\build\*.bin"` + `File "data\build\*.yaml"` 收录预编译产物，
**两条都不是 `/nonfatal`** ⇒ `data\build\` 空了 NSIS 直接 abort。
所以**不能靠删目录规避**，只能**重编**：用 `rime_deployer --build <共享目录> <用户目录>`
（注意**第一个参数才是共享目录**；传反会编译空目录、26 秒就"成功"）从当前 `output\data` 重编，
再把新的 `build\` 拷回工作区。

重编前后对照：载荷 `data\build\default.yaml` 里的死绑定
`Control+Shift+9` / `Control+Shift+0` 从 **各 1 处 ⇒ 各 0 处**（这两条 ADR 0009 就删了，
但 2026-06-30 编译的旧产物一直没重编，装器一直在带）。

### 13.3 门禁与验收数字（本机实跑，可复现）

- 新装包：42,756,263 B, SHA256 F040D989872FB8D135917B9416E40DE4D3010002EB669EF43AA98544A8344A8A。
- 旧包装档：`fluxing-1.0.0-installer.pre-m4.exe`，42,748,195 B, SHA256 B1C17AB840948374F95571E79A5ECF8CB5BBC556CFF07BE3440EED572EA0808F。
- 载荷 `data\build\`：29 个文件，与工作区**逐一 SHA256 全 MATCH**，无残留陈旧文件。
- 载荷门禁：`data\build\*.yaml` ⇒ `bindings=325|reachable=325|dead=0` + `RESULT|PASS`；
  `data\*.yaml` + `user-custom\*.custom.yaml` ⇒ `bindings=40|reachable=40|dead=0` + `RESULT|PASS`。
- 7 个二进制与工作区产物**全 MATCH**：`weasel.dll` / `weaselx64.dll` / `WeaselServer.exe` /
  `WeaselDeployer.exe` / `WeaselSetup.exe` / `rime.dll` / `FluxingPhrasesDialog.exe`。
- NSIS 8 条 7010 警告（ARM/ARM64 文件缺失 + `data\*.gram` / `data\lua\*.txt` 缺失）
  —— **与上一版同源**，非本次引入。

### 13.4 下一步

- **T6.3 真机按键验收**（§3.1 八条）：抓键可用性 + 原有快捷键不回归。
- **T6.4** 实测通过后 ADR 0011 `Proposed` → `Accepted`。
- **T20** 仍未实测，是 ADR 0011 Q2 的必需输入。
