# 087 · Tasks · 快速面板「常驻」守卫 + 64 位前端换装闭环 + 裸 Shift 左右方向收尾

> 配套：[spec.md](spec.md)、[plan.md](plan.md)、[ADR 0016](../../../docs/adr/0016-quickpanel-focusout-guard-and-x64-swap.md)、
> 取证底稿：[findings.md](findings.md)。

## Phase 1 · 面板「常驻」守卫（Q1）

- [x] T001 [P1] [US087-A] `RimeWithWeasel/RimeWithWeasel.cpp` 的 `FocusOut()` 里给 `QuickPanelDialog::Hide()` 加 `AlwaysShowMode()` 守卫（守卫加在**调用点**，**不动** `Hide()` 本体）
- [x] T002 [P1] [US087-B] 复核其余 8 个收起发起点：面板内部 4 条自动路径（`QuickPanelDialog.cpp:682` / `:740` / `:1317` / `:1326`）的既有守卫**保持不变**；5 条需用户手势的路径（ESC `:1334`、`ToggleMode` 内 `:1453`、`WeaselServerApp.cpp:387` / `:401` / `:421`）**不改**
- [x] T003 [P2] [US087-A] 记录附带缺口：语言栏弹出菜单 `WeaselTSF/LanguageBar.cpp:185` 裸 `TrackPopupMenuEx` **无勾选回填** ⇒ 用户在弹出菜单勾了看不到勾（**本条不修**，列遗留）

## Phase 2 · 64 位换装链路（Q1）

- [x] T004 [P1] [US087-C] `output/install.nsi:633-638`（32 位支）：版本化安装件的拷贝改成「先 `Delete` 掉陈旧版本化件 + `SetOverwrite on` + `File /nonfatal /oname=weasel.${FLUXING_VERSION}.${WEASEL_BUILD}.dll "weasel.dll"` + `SetOverwrite try`」。⚠️ `File "src" "dst"` **不是** NSIS 的改名语法（`makensis` 报 `no files found` 并中止），唯一改名语法是 `/oname=`（本轮打包实测订正）
- [x] T005 [P1] [US087-C] `output/install.nsi:694-698`（64 位支）：同上（`weaselx64.dll`），改名语法同为 `File /nonfatal /oname=weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll "weaselx64.dll"`
- [x] T006 [P1] [US087-C] `output/install.nsi:1062` / `:1088`：两处开机补换脚本落 `$WINDIR\Temp`（**无空格**）⇒ `output/install.nsi:1070` / `:1095` 的 `schtasks /TR` **不带内层引号**
- [x] T007 [P1] [US087-C] `output/install.nsi:1064` / `:1090`：开机补换脚本内的 `xcopy … "dst*"` 改成 `copy /Y "src" "dst" >NUL`
- [x] T008 [P1] [US087-C] `output/install.nsi:1064` / `:1090`：开机补换脚本内的**拷贝源**路径改为**安装期展开的绝对路径**（批处理里把 NSIS 的 `$TEMP` 在安装期展开，不再用 cmd 的 `%TEMP%` ⇒ 开机时 SYSTEM 不会解析到另一个目录）

## Phase 3 · 裸 Shift 方向取证（Q2）

- [x] T009 [P1] [US087-D] `tmp/shiftprobe/shiftprobe.cpp`：直连**运行中**服务的探针，验证接收侧解码极性（参考轮 / 实验轮各两轮）⇒ 结论「接收侧正确」
- [x] T010 [P1] [US087-D] `tmp/shiftprobe/shiftside.cpp` + `shiftside.exe`：`WH_KEYBOARD_LL` 钩子取**物理扫描码**（0x2A / 0x36）与**操作系统认知的虚拟键**，并用 `fluxing::ResolveCharacter` 复算前端键名 ⇒ 一次按键即可二选一；配套 `build-side.cmd` / `build.cmd`（链接命令行直传 `/LIBPATH` + 显式 `.lib`；**不要**用 `@link.rsp` 响应文件 —— 实测 LNK1104 打不开）
- [ ] T011 [P1] [US087-D] 取现场按键数据（用户按一下左 / 右 Shift，30 秒窗口，输出 `shiftside-out.txt`）⇒ 按 A.6 判据定形态（改用物理扫描码判定 / 提供「左右互换」开关）。**2026-10-29 更新：仍待办，但已不阻塞落地** —— 形态由「二选一」改为「两形态合并」（见 A.7 与 ADR 0017），本条只剩「要不要建议用户打开开关」这一个作用
- [x] T012 [P1] [US087-D] 按 ADR 0017 实施 Shift 方向修复 + 回归断言（**扫描码优先 + 服务端可配置左右互换开关**，落地明细见 T023–T026）

## Phase 4 · 验证（Q2）

- [x] T013 [P1] [US087-A] 静态复核守卫：常显态下「输入焦点离开」不再隐藏；非长显态行为不变；5 条手势路径行为不变
- [x] T014 [P2] [US087-C] 安装器改动后做**语法与链路复核**：两支版本化拷贝均带 `SetOverwrite on`、两处任务登记均无内层引号、两处拷贝均为 `copy /Y` ⇒ 逐环对照（拷贝 → 任务登记 → 开机执行）
- [ ] T015 [P1] [US087-A] 真机验收：勾「快速面板常驻」→ 切走再切回 ⇒ 面板仍在；Esc / 收起快捷键 / 点菜单项仍能收起
- [ ] T016 [P2] [US087-C] 真机验收：64 位程序里加载的**是新版本**且能正常加载；开机补换的痕迹可核对（标记文件 / 任务登记 / 日志行）

## Phase 5 · 文档与发布（Q2）

- [x] T017 [P2] [US087-E] CHANGELOG `### 主要更新` 追加 4 条（面板守卫 / 换装闭环 + 未闭合项 / Shift 取证 / CapsLock 口径）
- [x] T018 [P2] [US087-E] `.specify/memory/lessons-learned.md` 追加 L154 / L155 / L156
- [x] T019 [P2] [US087-E] `Fluxing-二次开发内容清单.md` 补 §4.1.8 小节 + §4.3 的 T24 / T25 / T26 / T27 + §10 索引计数
- [x] T020 [P2] [US087-E] ADR 0016 落 `Proposed`（D1–D9 + Consequences + Evidence + Open questions）
- [x] T021 [P2] [US087-E] 交接卡追加 `## 18.`（**§1–§17 一字不改**），含本轮实际改动、未闭合项、下阶段计划、待用户验收清单
- [ ] T022 [P2] [US087-A] 真机验收通过 ⇒ ADR 0016 `Proposed` → `Accepted`

## Phase 6 · ADR 0017 落地（2026-10-29 本轮）

- [x] T023 [P1] [US087-D] `include/KeyNameMapper.h`：`TranslateKeycode` 把 `VK_SHIFT` / `VK_LSHIFT` / `VK_RSHIFT` 合并成单一分支，**扫描码优先**（0x2A→左、0x36→右），虚拟键兜底（`VK_RSHIFT`→右、`VK_LSHIFT`→左、泛 `VK_SHIFT`→左）；删掉原先独立的 `case VK_LSHIFT / VK_RSHIFT` 分支（ADR 0017 D1）
- [x] T024 [P1] [US087-D] `include/ShiftStateMachine.h` 新增纯函数 `fluxing::ApplyShiftSideSwap(is_left, swap)`（ADR 0017 D2）
- [x] T025 [P1] [US087-D] `include/RimeWithWeasel.h` + `RimeWithWeasel/RimeWithWeasel.cpp`：新增成员 `m_shift_left_right_swap`（构造缺省 `false`）；yaml 读取块新增 `shift_left_right_swap`（缺省 `false`）；`ShiftDown` 改为 `s.lastIsLeft = fluxing::ApplyShiftSideSwap(is_left, m_shift_left_right_swap)`（ADR 0017 D2 / D3）
- [x] T026 [P1] [US087-D] `output/data/weasel.yaml` 的 `[global settings]` 新增 `shift_left_right_swap: false`（带中文注释，指向 ADR 0017）⇒ 缺省行为不变；用户的用户目录 `weasel.yaml` 不带该键时同样是 `false`
- [x] T027 [P1] [US087-D] 回归断言：`test/TestShiftIPCStateMachine` 新增 `Test_ShiftSideSwap`（4 条断言钉住四个真值）；`test/TestHotkeyBindings` 的 `RunMapperSelfCheck()` 新增 3b 段 6 条 Shift 侧断言（`VK_SHIFT/0x2A`→左、`VK_SHIFT/0x36`→右、`VK_RSHIFT/0x2A`→左、`VK_LSHIFT/0x36`→右、无扫描码时按虚拟键回落）（ADR 0017 Verification）
- [x] T028 [P2] [US087-E] ADR 0017 落 `Proposed`；ADR 0016 的 D7 补「已被 ADR 0017 接替」指向（**原文保留**作决策历史）
- [ ] T029 [P1] [US087-A] 真机验收：CapsLock 关闭态下左 Shift 上屏**第 2** 个候选、右 Shift **第 3** 个；若仍相反，在用户目录 `%APPDATA%\Rime\weasel.yaml` 加 `shift_left_right_swap: true` 后重测

## 状态

- 2026-10-29：立项（spec 087，ADR 0016 `Proposed`）。
- 2026-10-29：T001–T007 / T009 / T010 / T013 / T014 / T017–T021 落地。
  **T008 / T011 / T012 / T015 / T016 / T022 待办** —— 其中 **T008 是本轮明确未闭合的残留**
  （开机补换的拷贝源路径），**T011 / T012 依赖用户提供的现场按键数据**。
- 2026-10-29：Shift 方向**未改任何判定逻辑**（A.6 未闭合，宪法 R1）。
- 2026-10-29（第二轮）：**A.7 根因闭合** —— 链路每一环都对 ⇒ 反转来自宿主交给 TSF 的
  (虚拟键, 扫描码)。形态由「二选一」改为「**两形态合并**」：扫描码优先（ADR 0017 D1）
  + 服务端可配置左右互换开关（D2）⇒ **T012 解除对 T011 的依赖**并已落地（T023–T028）。
  **T011 仍待办**（用户未跑 `shiftside.exe`），但它只剩「要不要建议用户打开开关」这一个作用。
  **T008 / T015 / T016 / T022 仍待办**；新增 **T029**（Shift 方向真机验收）。
  门禁：Win32 整解 **exit 0**；TSF x64 **exit 0**；`TestShiftIPCStateMachine` **44 PASS / 0 FAIL**；
  `TestHotkeyBindings --selfcheck-mapper` `failures=0` / `RESULT|PASS`；
  仓库全套件 `scripts\run-tests.bat` **除 `TestWeaselIPC` 外全部通过**
  —— `TestWeaselIPC` 因**用户已安装的** `WeaselServer.exe`（PID 32304）占用命名管道而
  自带保护拒绝运行（ADR 0012），与本次改动无关（ADR 0017 D4 线缆不变）。
  ⚠️ **沙箱权威门禁仍跑不起来**（T019 同款）⇒ 本轮**没有产物级门禁数字**，不编造。

## 门禁结果 · spec 087 A.8 两个真机缺陷修复（2026-10-29，ADR 0018）

> 本机 Win32 / Release。判 CI 终态一律 `gh api .../actions/runs/<id>/jobs`（`run view --json jobs` 是滞后快照）。

| 步骤 | 命令 | 结果 |
|---|---|---|
| 整解 | `msbuild weasel.sln /p:Configuration=Release /p:Platform=Win32 /m` | `SOLUTION_EXIT=0` |
| 测试工程 | `msbuild test/TestShiftIPCStateMachine/...vcxproj` | `TESTPROJ_EXIT=0` |
| 判定 | `Release/TestShiftIPCStateMachine.exe` | `Result: 49 PASS / 0 FAIL`（原 44 + Test #22 五条） |
| 判定 | `Release/TestHotkeyBindings.exe --selfcheck-mapper` | `failures=0` / `RESULT\|PASS` |

- [x] T030 常驻态「点击」收起漏网点（`onPhrases` `WM_LBUTTONUP`）加 `AlwaysShowMode()` 守卫 —— ADR 0018 D1
- [x] T031 常驻态 idle alpha 地板 51(20%) → 153(60%) —— ADR 0018 D2（修「有输入动作后消失」观感）
- [x] T032 裸 Shift 部分选词不再 flush 整个 composition —— ADR 0018 D3 + `Test #22`（R2）
- [ ] T033 装机复验 T030/T031/T032（面板常驻 + 60% 浅态 + Shift 只上屏所选单字）—— **须打包 exe 交用户**

