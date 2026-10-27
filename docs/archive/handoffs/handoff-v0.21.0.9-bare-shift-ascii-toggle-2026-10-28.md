# 接力交接：Fluxing 0.21.0.9 — 裸 Shift 切中英文已落地，T2（抓键 layout-aware）已定稿待实现

> 本文件是**给下一个会话读的完整上下文**。下一个会话的 prompt 只负责指路，不重复本文件内容。
> 生成时间：2026-10-28（会话 `mvs_3303bd8edf4d4ada80136cf7b3b6e929`，本文件已按该会话末尾的状态更新）
> 仓库：`F:\soft\00selfmade\rime_claude`（远端 `kizemo/Fluxing`，**不是 origin**）
> **当前 HEAD = `20aa324`**，本地与远端已同步（工作区只剩 ` m librime`，是子模块构建产物，**不可 git add**）

---

## 0. 一句话现状

0.21.0.9（裸 Shift 切中英文）**代码已全绿、安装包已出、已推送**，但**沙箱权威门禁在本机跑不起来**（环境故障）、
**真机按键验收只有人能做**；下一阶段的 **T2（设置界面抓键 layout-aware 改造）已定稿为 ADR 0011 + spec 083，实现待做**，
其中夹着一个**必须先实测再修**的疑点 **T20（CapsLock 打开时快捷键疑似全部失配）**。

---

## 1. 必读文档（按顺序，路径已给全）

1. `F:\soft\00selfmade\rime_claude\CONTEXT.md` — 身份 / 模块 / 词汇 / 不变量（现有 17 条）
2. `F:\soft\00selfmade\rime_claude\AGENTS.md` — 操作手册；§2.5 是沙箱 harness 触发条件，§4 是危险区
3. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` — 法则层（R1–R9 / P1–P8）
4. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` — 部署 + NSIS 配方
5. `F:\soft\00selfmade\rime_claude\Fluxing-二次开发内容清单.md` — **任务总账**：哪些做完了、哪些没做、下一阶段做什么
6. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` — 工程技能消费契约
7. 本轮专项（已落地）：
   - `F:\soft\00selfmade\rime_claude\.specify\specs\082-bare-shift-ascii-toggle\{spec.md,plan.md,tasks.md}`
   - `F:\soft\00selfmade\rime_claude\docs\adr\0010-bare-shift-toggles-ascii-mode.md`
8. **下一阶段专项（待实现）**：
   - `F:\soft\00selfmade\rime_claude\.specify\specs\083-capture-keys-by-produced-character\{spec.md,plan.md,tasks.md}`
   - `F:\soft\00selfmade\rime_claude\docs\adr\0011-capture-keys-by-produced-character.md`（`Proposed`）

> **读清单第 5 条优先**——它是任务进度的唯一权威；spec 里的勾选框不可信，以 CHANGELOG + 门禁数字为准。

---

## 2. 本轮（0.21.0.9）已完成的东西

### 2.1 提交（全部已推送至 `kizemo/Fluxing`）

| SHA | 标题 | 内容 |
|---|---|---|
| `a023ee1` | `style(fluxing): clang-format the C++ files the bare-Shift feature has to touch` | 4 个 C++ 文件整体格式化（45/3/8/15 行违规）。**这是 CI lint 门禁的前置**，见 §6.1 |
| `258e708` | `feat(fluxing): bare Shift toggles Chinese/English when no candidate menu is open` | 功能本体 + 全部配套文档 + spec 082 + ADR 0010 + CONTEXT invariant 17 |
| `5fbdc35` | `docs(fluxing): add the second-development checklist and the session handoff` | 把《Fluxing-二次开发内容清单.md》纳入版本控制 + 本交接文档 |
| `e0d6908` | `docs(fluxing): record the 0.21.0.9 install metadata` | 0.21.0.9 的安装包元数据（路径 / 大小 / SHA256 / 未进 `release/`） |
| `20aa324` | `docs(fluxing): record the 0.21.0.9 sandbox failure, the real CI status, and the T2 design` | 沙箱失败记录（T19 / L150）+ CI 实况改写 + **ADR 0011 + spec 083 全套（spec/plan/tasks）** |

### 2.2 功能语义（ADR 0010，Accepted）

- 候选窗**没开** ⇒ 裸按 `Shift` ⇒ 中↔英切换（**只切当前会话**，与 yaml 的 `Shift+space` 逐字对齐）
- 候选窗**开着** ⇒ 裸按 `Shift` ⇒ 仍然选第 2/3 个候选（spec 077 行为不变，**无双击逃生门**）
- 开关：`output/data/weasel.yaml` 的 `[global settings]` → `bare_shift_toggles_ascii: true`
  - **缺失即默认开** ⇒ 老用户升级即生效，不需要改配置
  - 关掉：`bare_shift_toggles_ascii: false`
- 关键实现事实（别再重新推导）：
  - 「菜单开着」谓词 = `ctx.menu.num_candidates > 0 && !status.is_ascii_mode`
    —— **不能只看 `is_composing`**：英文模式下 librime 仍保留 1 个候选的 raw 段
    （`ascii_segmentor.cc:20`），且 `key_binder.cc:255` 自己构造 `has_menu` 上下文时
    也要 `&& !ascii_mode`。只看 `is_composing` ⇒ 英文模式切不回中文。
  - 切换走 `get_option`/`set_option(session, "ascii_mode", !cur)` + `_UpdateUI(ipc_id)`
    —— **绝不走 `SetOption()`**（那个函数会写 `m_global_ascii_mode` 全局镜像）
  - `interveningKey` 守卫天然挡住 `Shift+X` / `Shift+space` / `Shift+标点`
  - 判定逻辑抽成 `include/ShiftStateMachine.h` 的纯函数 `DecideShiftUpAction()`
    （无 rime_api / IPC / GUI 依赖，可单测，沿用 spec 077 模式）

### 2.3 验证数字（本地，格式化后复跑）

```
full suite:                  === ALL TESTS PASSED ===
TestShiftIPCStateMachine:    Result: 28 PASS / 0 FAIL   (17 个测试函数 / 28 条断言)
TestDefaultHotkeys:          Passed: 89 / 89
TestShiftIPCWire:            Result: 10 PASS / 0 FAIL
TestPipeChannelRace:         Result: 12 passed, 0 failed
hotkey gate (出货配置):        SUMMARY|bindings=39|reachable=39|dead=0|known_dead=0|
                                   expectations=0|mismatches=0|unused=0
                              RESULT|PASS
clang-format-18 (CI 同版本) 对全部触及的 .cpp/.h: clean
```

### 2.4 交付物

- 安装包：`F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.21.0.9-installer.exe`
  - 大小 **42,761,559 B**
  - SHA256 **D1FCF78304FB89955F61F8925C894AB1F2CA5941FF9E9283D859D93A3E820E1C**
  - 注意：这是**格式化后重编**的包；格式化前那个 42,749,322 B / `1B5CEA8E…` 的包已作废
- `env.bat`（gitignored，不进 commit）：`WEASEL_BUILD=9`、`PRODUCT_VERSION=FILE_VERSION=0.21.0.9`

---

## 3. 还没做的（下一阶段的起点）

| 项 | 状态 | 说明 |
|---|---|---|
| **真机按键验收** | ⏳ 等用户 | 6 条清单见 §3.1。**只有人能做**，代码侧已全绿 |
| **清单 T2**（设置界面抓键路径 layout-aware 改造） | 🟡 **已定稿，实现待做** | ADR 0011（`Proposed`）+ spec 083（R1–R7 / plan M1–M6 / tasks）。方案要点见 §3.2 |
| **清单 T20**（CapsLock 打开时快捷键疑似全部失配） | ⚠️ **疑点，未实测** | **必须先测量再决定怎么修**（ADR 0011 Q2），见 §3.3 |
| **清单 T19**（沙箱权威门禁本机跑不起来） | ⚠️ 环境故障 | 见 §4.1；**不阻塞提交**，但 0.21.0.9 的沙箱断言目前一条没跑 |
| **清单 T3/T4/T5** | ⏸ 未开工 | 见清单 §4.1 表格 |
| **共享 `data/` 目录其余文件升级** | ⚠️ 待用户操作 | 除 `default.yaml` 外仍是 2026-08 旧版；**装 0.21.0.9 安装包**后一致 |
| **0.21.0.9 进 `release/` 发布链** | ⚠️ 未做 | `release/` 里还没有它 |
| **T2.13 / T14** | ⚠️ 已知遗留 | `test/v0_19_0_30_e2e/build_e2e.bat` 写死 4 处机器路径；`build (xmake)` arm64 目标缺失 |
| **CI `sandbox-gate` job** | ⛔ 排队中 | 缺带 `sandbox` 标签、支持嵌套虚拟化的 Win11 自托管 runner（基础设施缺口，非代码问题） |

### 3.1 真机验收清单（spec 082 §3.4，用户照着敲）

1. 中文模式、候选窗**没开** ⇒ 单独按一下 `Shift` ⇒ 切英文
2. 再单独按一下 `Shift` ⇒ 切回中文
3. 中文模式、**先打拼音**让候选窗弹出 ⇒ 单独按一下 `Shift` ⇒ **仍选第 2 个候选**，**不**切模式
4. 英文模式、候选窗**没开** ⇒ 按一下 `Shift` ⇒ 切回中文
5. 任意状态下 `Shift+X` ⇒ 只出 `X`，**不**切模式
6. 两个窗口各开一个会话 ⇒ 在 A 里按裸 `Shift` ⇒ 只切 A，B 不变

### 3.2 T2 方案要点（ADR 0011 `Proposed`，别再重新推导）

**问题**：设置界面抓键用**虚拟键码**拼 `accept:`，能造出 `Control+Shift+9` 这类**死绑定**。
**根因链**：本 fork 的 TSF 前端发的是「按键**产生的字符**」（`WeaselTSF/KeyEvent.cpp:51` 的 `ToUnicodeEx`），
librime 按 `(keycode, modifier)` **精确匹配**（`librime/src/rime/key_event.h:43`）。

**已坐实（对照实验，不是推理）**：把 `output/data/default.yaml` 副本里的 `Control+Shift+numbersign`
改成 `Control+Shift+9` 再跑门禁 ⇒ `DEAD_KEYCODE`（前端实际发 `0x0028 parenleft`，不是 `0x0039`）+ `RESULT|FAIL`，
`bindings=27 reachable=26 dead=1`；同文件 `Control+Shift+numbersign` 仍是 `REACHABLE`。
脚本：`F:\soft\00selfmade\tmp-dllcmp\probe_t2_binding.py`，日志 `probe_t2_binding.log`。

**三条已核实的源码事实（实现时必须照抄）**：

- **F1 — 带修饰键的绑定只能用 Rime 有名字的字符。**
  `librime/src/rime/key_event.cc:51-82`：单字符 `accept:` 直接取字符码；带修饰键走 `RimeGetKeycodeByName`，
  而它（`librime/src/rime/key_table.cc:2026-2033`）对 `keys_by_keyval` 做**纯 strcmp** —— 该表**只有大写字母 A–Z**。
  ⇒ `Control+f` 连解析都失败；**只有 `Control+Shift+F` 可表达**（这正是内置表用 `Control+Shift+F` 的原因）。
- **F2 — 前端在算字符前会清掉 Ctrl / Alt。**
  `WeaselTSF/KeyEvent.cpp:44-51`：`ToUnicodeEx` 前把 `VK_CONTROL` / `VK_MENU` 置零
  ⇒ 字符大小写**只由 Shift 决定** ⇒ 不复刻这条就分不清 `Ctrl+F` 和 `Ctrl+Shift+F`。
- **F3 — CapsLock 打开时前端会多带一位 `LOCK_MASK`（疑点，未实测）** ⇒ 见 §3.3。

**ADR 0011 的 D1–D6**：

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

### 4.1 沙箱权威门禁本轮跑不起来（T19 / lessons L150）

- 工装：`F:\soft\00selfmade\sandbox-verify\rime-claude\`（`run-verify.cmd` 一键入口；`README.md` 里写了七个坑）
- 2026-10-25 那轮**成功**过：`PASS=38 FAIL=2 / 共 46`（E.2 无 D: 盘、E.4 无外网，是常驻环境事实）
- 2026-10-28 本轮**两轮都失败**：都是「3 次启动都没拿到 claims.json」（orchestrator exit code 4），
  `C:\Users\Duanyi\sandbox-artifacts\rime\logs\` 里**连 `boot.log` 都没有** ⇒ 沙箱 VM 根本没起来
- **分层判定法（L150）**：看 artifacts 目录里**哪个文件没出现**，不要只看最后一行报错
  - 连 `boot.log` 都没有 ⇒ **启动层**（Hyper-V / vSwitch / 虚机），再重跑同一条命令无意义
  - 有 `boot.log`、无 `claims.json` ⇒ **安装层**（静默安装 / `/D=` 参数）
  - 有 `claims.json`、断言不全 ⇒ **断言层**（这才是门禁该管的事）
- 诊断到的线索：`Microsoft-Windows-Hyper-V-VmSwitch` 报 NIC「延迟删除操作请求一文件对象的非关闭操作」；
  `vmcompute`/`vmms`/`hvhost` 都在 Running，主机是裸机 ASUS（嵌套虚拟化应当可用）；`Get-VM` 无残留 VM
- 下一会话若要重试：先 `Get-VM` 看有无残留沙箱 VM，清掉；必要时重启 `hns` + `vmms`（**有副作用**：
  可能短暂打断本机网络 / Docker / WSL，先问用户）
- 旧证据已归档为 `C:\Users\Duanyi\sandbox-artifacts\rime\FINAL-claims-20261025-pre090.{json,log}`
- **注意**：AGENTS.md §2.5 的"必须跑沙箱 harness"规则只对**改了 `output/install.nsi` 或
  `output/data/user-custom/**`** 的提交生效。本轮两者都没动 ⇒ 不强制。
- **不要用本机静默装机替代**（见 §4.2）

### 4.2 本机静默装机的副作用（所以没在本机做）

`output/install.nsi` 安装时会对 TSF DLL 跑 `regsvr32`，并写 `HKLM\...\CTF\KnownClasses`、
`HKCU\...\CTF\Assemblies\0x00000804\{3D02CAB6-…}`、`HKLM\SOFTWARE\Microsoft\CTF\TIP\{A3F4CDED-…}`。
在开发机上静默装到临时目录 ⇒ **会把用户正在用的 `D:\Program Files\fluxing` 的输入法注册指到临时目录**。
所以走沙箱（虚拟机）而不是本机。真机验收由用户自己装。

### 4.3 CI 的真实状态（截至本文件生成）

- run `40538755392`（head `e0d6908`）：`lint` = **success**；
  `test` / `build (msbuild)` / `build (xmake)` 在交接时**仍 `in_progress`**
- run `40541485193`（head `20aa324`，本文件这次提交触发）：交接时刚起，`in_progress`
- run `40537923350`（head `5fbdc35`）：同样在跑
- `sandbox-gate` job **长期排队**（缺 runner），**不要用 `gh run watch` 等它**（永不退出）
- **只盯 4 个门禁 job**：`lint` / `test` / `build (msbuild)` / `build (xmake)`
- 交接前若 CI 已出终态，请把本节与 CHANGELOG / 清单里的 CI 行一并改成实况
  （**未实跑的结论不得写死**，这是本轮专门纠正的错误）

### 4.4 MiniMax Code 运行时反复重启

1 小时内重启 6 次会杀掉在飞的后台任务（根因未定位，无 crash dump）。
已提报：`F:\soft\00selfmade\mavis-bug-reports\2026-10-26-runtime-restart-loses-background-task.md`

---

## 5. 下一步可以直接用的命令

```powershell
# 构建 / 测试
cmd /c "F:\soft\00selfmade\rime_claude\build.bat data"
cmd /c "F:\soft\00selfmade\rime_claude\build.bat weasel installer"
cmd /c "F:\soft\00selfmade\rime_claude\scripts\test-infra\run-test-suite.bat"

# 快捷键门禁（出货配置；scan 的每个文件都是一份配置副本，源 + 产物都要扫）
& "F:\soft\00selfmade\rime_claude\Release\TestHotkeyBindings.exe" --baseline `
  "F:\soft\00selfmade\rime_claude\test\TestHotkeyBindings\known-dead-bindings.txt" `
  "F:\soft\00selfmade\rime_claude\output\data\default.yaml" `
  "F:\soft\00selfmade\rime_claude\output\data\user-custom\rime_ice.custom.yaml" `
  "F:\soft\00selfmade\rime_claude\output\data\user-custom\double_pinyin_sogou.custom.yaml" `
  "F:\soft\00selfmade\rime_claude\output\data\user-custom\radical_pinyin.custom.yaml"

# 推送（远端名是 kizemo，不是 origin）
git -C "F:\soft\00selfmade\rime_claude" -c http.proxy= push kizemo Fluxing

# CI 复核（只盯 4 个门禁 job，别 watch）
gh run list --repo kizemo/Fluxing --workflow ci.yml --limit 5
gh api /repos/kizemo/Fluxing/actions/runs/<run_id>/jobs

# 沙箱权威门禁（先读 README 的七个坑；注意 §4.1 的分层判定）
$sb = "F:\soft\00selfmade\sandbox-verify\rime-claude"
Start-Process cmd.exe -ArgumentList "/c","$sb\run-verify.cmd -RepoRoot F:\soft\00selfmade\rime_claude -InstallerPath F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.21.0.9-installer.exe -OldInstallerPath F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.21.0.2-installer.exe" -WorkingDirectory $sb -RedirectStandardOutput "F:\soft\00selfmade\tmp-dllcmp\sandbox-090c.log" -RedirectStandardError "F:\soft\00selfmade\tmp-dllcmp\sandbox-090c.err" -WindowStyle Hidden -PassThru
```

---

## 6. 本轮踩过的坑（别重试）

### 6.1 CI 的 C++ lint 门禁只查「本次 push 触及的文件的整个文件」

`scripts/lint-changed-cpp.sh` 用 `git diff --name-only $BASE HEAD -- '*.cpp' '*.h'` 取文件，
然后对每个文件跑 `clang-format --style=file --dry-run --Werror`。
⇒ **动 C++ 就必须把那个文件整体格式化干净**，哪怕你只改了一行。
本仓库的既定做法（先例 `4ab52db` / `a023ee1`）：单独一个 `style(fluxing):` 提交，body 写清「违规行数」。

- 本机 clang-format 在 `C:\Python314\Scripts\clang-format.exe`（pip 版 18.1.8），**CI 用的是
  apt 的 clang-format-18**（Linux）——版本对得上，但记得先把 clang-format 目录加进 PATH
- `.clang-format` 全历史只有**一个上游提交** `afecb64`，fork 没改过 ⇒ `BasedOnStyle: Chromium`
  + 80 列是既定配置，别去改它来"迁就"代码
- `--style=file` 会从**文件所在目录**向上找 `.clang-format` ⇒ 跨目录必须写 `--style=file:<绝对路径>`
- 拆两个提交的技巧：`clang-format(cat-file blob HEAD:<file>)` = 纯格式化版，
  `clang-format(工作区文件)` = 含功能版；先提交前者，再写后者
- `git cat-file blob HEAD:<path> > file` 必须走 `cmd /c`（cmd 的 `>` 是字节重定向；
  PowerShell 的 `>` 会写成 UTF-16）

### 6.2 文档批量更新的正确姿势（本轮新增）

- 走「**幂等 Python 补丁脚本 + 锚点强校验 + 自动保 EOL/BOM + 原子写**」，不用 edit 逐条手改
- **锚点必须按目标文件的 EOL 转换后再 count**（CRLF 文件里直接数 `\n` 锚点会得 0）
- `count != 1` 就 abort（0 = 锚点写错或已幂等生效；>1 = 锚点不唯一，**绝不能默默替换**）
- 写完必须做 **U+FFFD 全量校验 + C0 控制符校验**
- **原子写**：先写 `.tmp` 再 `os.replace()`；不要用 `write_text` 直接覆盖（先截断再写，失败就毁掉旧内容）
- 脚本留在 `F:\soft\00selfmade\tmp-dllcmp\`（可重跑）：`patch_t2prep.py` / `patch_t2prep2.py` /
  `patch_ci_truth.py` / `patch_spec_count.py`
- PowerShell 内联 python 的引号常被吃 ⇒ **一律 write 脚本再跑**，
  运行形态 `cmd /c "python X.py > X.log 2>&1"` 再 `Get-Content -Encoding UTF8` 读日志

### 6.3 PowerShell / 工具链的老坑（沿用）

- `Remove-Item` 被硬安全策略拦截 → 删除一律 `mavis-trash.cmd`，或换全新唯一目录
- bash 工具里 `cd X; ... 写 Y` 被硬拦 → 用绝对路径或 write 工具
- `cmd /c "cd /d X && ..."` 同样被拦（写目标无法校验）→ 用 `git -C`
- 构建进程持有日志独占句柄 → `[IO.File]::ReadAllText` 会报「正由另一进程使用」→ 用 `Get-Content -Tail N`
- 控制台输出中文/emoji 会 GBK 报错 → 重定向到 UTF-8 文件再读
- `Get-Content` 读 UTF-8 中文文件默认 cp936 解码 → 判断真伪用 UTF-8 方式读
- `Select-Object -First N` 会掐断管道；`-Last N` 可用
- `.NET` 的相对路径按会话工作区解析 → 读文件一律绝对路径
- git 读中文路径：`-c core.quotepath=false`；输出要 `encoding="utf-8"`；
  **中文路径传给 git 走 Python subprocess**（`F:\soft\00selfmade\tmp-dllcmp\commit_push_091.py` 是范例）
- `git` / `gh` 的 `sh: warning: setlocale` 会让 PowerShell 误判失败 → 用 `git log` 复核
- `gh run watch` 在 `sandbox-gate` 永久 queued 时永不退出 → 只盯 4 个门禁 job
- `gh api … --jq` 的 jq 表达式在 PowerShell 里反斜杠被吃 → 改用 `ConvertFrom-Json` 解析
- **不要连续轮询同一个后台任务**（运行时第 3 次读会发提醒）→ 用后台完成通知自动唤醒
- 写含中文的文件后必须做 U+FFFD 全量校验 + C0 控制符校验；每个文件保持 HEAD 的原始行尾风格
  （`RimeWithWeasel.cpp` / `RimeWithWeasel.h` 是 CRLF；`ShiftStateMachine.h` / 测试 cpp 是 LF；
  `CHANGELOG.md` CRLF、`Fluxing-二次开发内容清单.md` LF、`.specify/memory/lessons-learned.md` CRLF+BOM）

---

## 7. 决策备忘（下一会话别推翻）

1. `accept:` 键名必须写「组合键产生的字符」的键名（WeaselTSF 语义），`send:` 必须是单个 ASCII 字符
2. known-dead 基线清零 = 最严阈值，空基线不是调松
3. 门禁扫描范围必须覆盖配置的全部副本（源 + 部署产物各扫一遍）
4. ADR 已 Accepted 后**不得改正文**（要改就新 ADR supersede）；`Proposed → Accepted` 的状态迁移 +
   开放问题裁决记录是合法的（ADR 0011 落地后再转）；新术语进 `CONTEXT.md` §3，
   改不变量要走宪法修正流程；实现计划放 `.specify/specs/NNN-*/`
5. 注释里凡是断言「生效/不生效/数量」的，都必须有门禁或测试数字背书
6. 文档批量更新走「幂等 Python 补丁脚本 + 锚点强校验 + 自动保 EOL/BOM」（见 §6.2）
7. spec 的 tasks 勾选框不可信 —— 以 CHANGELOG + 门禁数字为准
8. 验收门禁要指向**部署产物**再跑一遍（源绿 ≠ 产物绿）
9. **提交消息 / 文档里断言的数字必须真实落地**；未实跑的结论不得写死
   （本轮专门纠正了 CHANGELOG 里预写的「CI 四项全 success」）
10. **子代理的结论必须逐条复核** —— 本轮 explore 给的两处根因是错的
    （①「大小写去重丢了 209 个小写键名」⇒ 实际是 librime `keys_by_keyval` 本来就只有大写字母；
    ②「`RimeGetKeycodeByName("f")` 返回 0x66 合法」⇒ 实际是 `strcmp` 精确匹配、找不到就 `XK_VoidSymbol`）
11. **不动已 Accepted 的 ADR 正文**；也不要顺手改 `ShortcutSettings.cpp:339-402` 的内置表
12. **本机静默装机不跑**（TSF 注册副作用），改用沙箱 harness；沙箱不可用时**如实报告**，
    不用高风险动作凑证据（L150）
13. 用户要求：按清单推进开发并**持续更新清单**；到需要实机验收的节点就**打包 exe、告知路径、
    提醒验收**；在**合适的实机节点自动 commit，不用请示**

---

## 8. 用户当前待办

- 装 `F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.21.0.9-installer.exe`
  （42,761,559 B，SHA256 `D1FCF78304FB89955F61F8925C894AB1F2CA5941FF9E9283D859D93A3E820E1C`）
- 按 §3.1 六条做真机按键验收
- 装完顺手把共享 `data/` 目录升级到 0.21.0.9（清单 T17）
- **顺带可帮下个会话的忙**：开着 CapsLock 按一下已配置的快捷键，
  看是否真的不触发（清单 T20 / ADR 0011 Q2 的实测输入）
