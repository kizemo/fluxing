﻿# Fluxing v2 · Product Requirements Document (PRD)

> **项目级 PRD**。覆盖 spec 004 路线图与 7 份子 spec（005-011）的产品愿景、用户故事、验收标准、风险登记。
> 与子 spec 的关系：本文是"v2 全局视图"；子 spec 的 `spec.md` 是"局部详情"——本文是 entry point，子 spec 是 detail page。
> 撰写人：AI 助手（基于宪法 R1「intent before implementation」原则 + spec 004 路线图 + T011 评审清单）。
> 维护周期：每次子 spec 状态变更时同步更新；季度评审时与用户对账。

---

## 0. 项目一句话定义

**火流猩输入法（Fluxing）**是 **RIME（中州韻）输入法引擎**的 Windows 原生前端，由 TSF 文本服务 + 后台服务进程 + 设置/部署 GUI + 安装/卸载器组成，使用 C++/ATL/WTL/D2D/DirectWrite，通过命名管道（PipeChannel）以文本协议与 RIME 的 librime C API 对接。

**v2 阶段产品愿景**：在不破坏"低资源占用"前提下，提供"现代中文输入法应该有的"全部体验——可视化设置、跨设备同步、本地化快捷短语。

---

## 1. 目标用户画像（按优先级排序）

1. **中文重度打字者**（每天 5000 字以上）：核心痛点是快速自定义短语、便捷多设备同步。
2. **程序员 / 写作者**：中英混输，常用标点 + 颜文字 + Emoji。
3. **设计 / 内容创作者**：关心 mac 风外观与暗色主题。
4. **偶尔打字者**（"装上就能用"）：不读文档，开箱即用。

**v2 不针对的用户**：
- 移动端用户（spec 004 §5 明确仅 Windows 客户端）
- Linux / macOS 用户（v2 是 Windows-only fork）
- 依赖云同步作为前置功能的用户（spec 004 §1.1「本地优先」）

---

## 2. 核心价值主张

| 价值 | 落地形式 | 验证方式 |
|---|---|---|
| **本地优先** | 所有功能在无网络时可用 | 断网测试 SC-005 |
| **隐私优先** | 不上传输入历史、用户配置中的"上次输入内容"等字段 | 抓包审计 |
| **资源占用低** | 候选面板 60 FPS，常驻 ≤ 80MB | SC-002 / SC-003 |
| **现代化 UX** | mac 风组件库 + 暗色主题 + 200ms 渐变 | SC-004 / US6 |
| **可自定义** | 不读 yaml 也能改（spec 007） | US3-A/B/C/D |

---

## 3. v2 范围 / 12 项需求

来源：spec 004 §2.1 + §2.2；本节为产品视角的简明索引，详见各子 spec。

### 3.1 P1 实施（5 项 + 1 横切）

| 编号 | 需求 | 优先级 | 子 spec | 状态（2026-07-01） |
|---|---|---|---|---|
| F1 | 默认快捷键（`,`/`.` 翻页 / `Shift_L/R` 选候选 / `Shift+space` 切中英 / `Ctrl+Shift+9/0` 标点/简繁） | P1 | 005 | **已 ship 0.18.0-0.18.5** |
| F2 | 托盘快速设置面板（`Alt+,` / 左键单击托盘 → 8-12 个常用入口） | P1 | 006 | design 完成，代码未开始 |
| F3 | yaml 可视化设置 UI（4 类配置可视化编辑） | P1 | 007 | design 完成，代码未开始 |
| F4 | 候选字右键一键删除/屏蔽 | P1 | 008 | design 完成，代码未开始 |
| F5 | 常用短语独立于用户词典（`Alt+K` 列表） | P1 | 009 | design 完成，代码未开始 |
| F6 | 暗色主题跟随系统（横切 006/007/008/009） | P1 | 004 §9 | spec 004 已收口 |

### 3.2 P2 仅设计（2 项）

| 编号 | 需求 | 优先级 | 子 spec | 状态 |
|---|---|---|---|---|
| F7 | 候选字第二行：剪贴板 + 输入历史 | P2 | 007 | 推迟到 v2.1+ |
| F8 | 中英混合输入 / ASCII 模式 auto | P2 | 005 扩展 | 推迟到 v2.1+ |
| F9 | 托盘图标合成叠点 | P2 | 006 扩展 | 推迟到 v2.1+ |
| F10 | 导出/导入方案（rime.zip） | P2 | 010 | P2 设计中 |
| F11 | 暗色主题跟随系统 | P1 | 004 §9 | spec 004 已收口（与 F6 同义） |
| F12 | Emoji 面板（`Alt+E`） | P2 | 009 扩展 | 推迟到 v2.1+ |

### 3.3 P3 仅设计（1 项）

| 编号 | 需求 | 优先级 | 子 spec | 状态 |
|---|---|---|---|---|
| F13 | 安装/卸载 mac 风现代化 | P3 | 011 | design 完成，v2.2+ 实施 |

---

## 4. 验收标准（v2 整体 SC-001 ~ SC-006）

来源：spec 004 §7；本节为全局 SC，每个值都有可观测的验证方式。

| SC | 名称 | 目标 | 验证方式 | 验证时机 |
|---|---|---|---|---|
| **SC-001** | 安装包大小 | ≤ 25 MB（不引入 .NET） | 测 `output/archives/fluxing-X.Y.Z-installer.exe` 字节数 | release 前 |
| **SC-002** | 常驻内存 | ≤ 80 MB（含 5 项 P1 功能） | 装 0.18.6+ 后任务管理器测 `FluxingServer.exe` 内存 | manual 验证清单 |
| **SC-003** | 候选面板帧率 | 60 FPS 稳定（P95 ≤ 16ms） | Win10/11 + DPI 100/150/200 + Trace 录屏 | manual 验证清单 |
| **SC-004** | 暗色渐变 | 200ms 内完成渐变过渡，无闪烁 | 录屏 + 帧分析 | manual 验证清单 |
| **SC-005** | 跨 spec 无 yaml 冲突 | `rime_deployer --debug` deploy 通过 | release 前 CI 步骤 | release 前 |
| **SC-006** | 三平台一致 | Win8.1/10/11 × DPI 100/150/200 视觉一致 | manual 验证清单 | release 前 |

---

## 5. 子 spec 验收标准索引（SC-x-NNN 格式）

每份子 spec 的 `spec.md` 列了"局部 SC"，下表把它们的编号格式统一（SC-001 ~ SC-099 + 子 spec 后缀）。

| 子 spec | 局部 SC | 度量点 | 验证工具 |
|---|---|---|---|
| 005 | SC-005-1 翻页方向 | `,` 上页 / `.` 下页 | TestDefaultHotkeys 字符串断言 |
| 005 | SC-005-2 Shift 选候选 | has_menu 时 Shift_L/R 选 2/3 | TestDefaultHotkeys 字符串断言 |
| 005 | SC-005-3 Shift+space 切中英 | 单键 Shift **不**切中英 | TestDefaultHotkeys 字符串断言 + manual |
| 005 | SC-005-4 Ctrl+Shift+9/0 标点/简繁 | toggle ascii_punct / traditionalization | TestDefaultHotkeys 字符串断言 |
| 005 | SC-005-5 L18 回归 | `shift+=` `shift+Enter` `shift+<letter>` **不**切中英 | **manual 验证（spec 005 §2.3 第 4 步）** |
| 006 | SC-006-1 面板启动时间 | ≤ 100ms 冷启动（preloaded） | manual 录屏 + Trace 标记 |
| 006 | SC-006-2 失焦 1s 自动关闭 | 焦点离开 1s 后窗口关闭 | manual |
| 007 | SC-007-1 快捷键页加载项数 | 等于 default.yaml 实际 binding 数（动态测） | TestYamlRoundTrip + manual |
| 007 | SC-007-2 界面外观实时生效 | 透明度滑块 50% 立即反映 | manual |
| 008 | SC-008-1 删除不弹窗 | 右键命中后无任何 UI 弹出 | manual |
| 008 | SC-008-2 防抖 100ms | 100ms 内多次右键只写一次 | TestCandidateEdit mock |
| 009 | SC-009-1 列表 use_count desc | 第 1 项 = use_count 最高 | TestPhrasesStore |
| 009 | SC-009-2 拼音首字母过滤 | 输入"yx"过滤"邮箱"等 | TestPinyinHint |
| 010 | SC-010-1 多设备同步 | A 设备加 3 条短语 → B 设备 1 分钟内出现 | manual 双机测试 |
| 011 | SC-011-1 路径合法化 | 用户数据路径强制后缀 `fluxing` | TestBootstrapper mock |

---

## 6. 跨子 spec 集成场景（v2 整合验证清单）

下列场景**必须**作为整体验证（不是单 spec 验证），是 v2.0.0 release 的 hard gate。

### 6.1 启动序列集成（006 + 005 依赖）

- Given 用户装机后首次启动 WeaselServer，
- When WeaselServer 启动，
- Then 同时启动 `FluxingPanelHost` 子进程（preload 状态），
- And `Alt+,` 全局热键已注册（不与系统 / IDE 冲突），
- And 候选面板 60 FPS 渲染（SC-003），
- And `Shift+space` 切中英（spec 005 键位），
- And `Shift_L/R` 选候选（spec 005 键位）。

### 6.2 暗色主题横切（004 §9 F11 跨 006/007/008/009）

- Given 用户在系统设置切到"深色"，
- When `WM_SETTINGCHANGE` 触发，
- Then 候选面板 / 托盘面板 / 短语列表 / 设置 UI 全部在 200ms 内（渐变过渡）切到暗色（SC-004），
- And 切换期间候选输入不丢、不打断（spec 004 §9.1 US6）。

### 6.3 候选删除 + 托盘恢复（008 + 006）

- Given 用户右键候选"测试短语"删除，
- And 误删想恢复，
- When 用户在托盘面板点"恢复"按钮（spec 006 US2-恢复），
- Then 从最近一次自动备份还原 `user.db` 和 `phrases.json`，
- And 误删的"测试短语"重新出现在候选中。

### 6.4 yaml 冲突验证（007 + 005）

- Given spec 007 UI 改了快捷键 → 写回 `default.yaml`，
- And spec 005 提供的 L04/L16/L18 lessons-learned 注释保留，
- When `rime_deployer --debug` deploy，
- Then 无 yaml 冲突（SC-005），
- And binding 列表包含 spec 005 的 6 项 + spec 007 用户加的项。

### 6.5 多端首启引导（011 + 010）

- Given 用户首次安装 Fluxing v2.2+，
- When 装机后首启，
- Then mac 风首启引导弹出（spec 011），
- And 主题选择 → 写 weasel.yaml（spec 004 §9.2），
- And 云同步账户注册（spec 010），
- And 主方案选择（spec 004 F1 关联）。

---

## 7. 风险登记（v2 整体）

来源：spec 004 §8；本节为索引 + 缓解责任分配。

| ID | 风险 | 概率 | 影响 | 缓解 | 责任子 spec |
|---|---|---|---|---|---|
| R-001 | 自绘 mac 风工作量大、影响 005/008 早期 ship | 中 | 中 | 005/008 不依赖面板 UI 库，可先 ship；mac 风组件库推迟到 006 之前完成 | 006 plan R1 |
| R-002 | 暗色渐变与候选面板高 DPI 缩放冲突 | 中 | 中 | 006 单独列任务验证 Win10/11 + DPI 100/150/200 | 004 §9.5 / 006 plan R2 |
| R-003 | 用户词典删除误触（无确认） | 低 | 高 | US4 明确"无确认无 toast"；保留 user.db 备份（spec 006 恢复按钮） | 008 + 006 |
| R-004 | RIME 引擎 1.13 `user_ignore` 钩子不存在 | 中 | 中 | spec 008 双方案：customization 钩子（首选）/ schema patch fallback | 008 plan R2 |
| R-005 | 云同步需求范围扩张（用户想要"输入历史同步"） | 中 | 中 | spec 004 明确"v2 同步范围不含输入历史" | 010 spec §2 |
| R-006 | mac 风在 Windows 上违反 MS 风格指南 → 用户被投诉"非原生" | 低 | 低 | spec 007 UI 加"主题模式"开关（mac / 原生 Win11），默认 mac，可切 | 007 plan |
| R-007 | `shift+<key>` release event 误匹配 key_binder | 中 | 中 | **L19 修复（commit `e2c36b1`）：`default.yaml` 中 `keycode=Shift_L/R` 全部 binding 移除，候选选择改用 `Control+1/2`；切中英保留 `Shift+space`**。0.18.6.0 已 ship (commit `db70099` + tag `v0.18.6.0`)，L20 验证装出 default.yaml 字符串内容正确（含 `Control+1, send: 2`、不含 `Shift+Shift_L, send: 2`、含 `Shift+space` toggle）；**待用户重装 0.18.6 实测运行时 binding 行为** | 005 spec §2.4 R3 |
| R-008 | CI 不执行单测（`ci.yml` 缺 test job） | 高 | 高 | **TDD.md §6 提出补 test job 方案** | 全局 | **CLOSED v0.18.7.0 (verified 2026-07-04)**: ci.yml test job added at line 194; runs `scripts\run-tests.bat` (wrapper) -> `scripts\test-infra\run-test-suite.bat` (13/13 test projects PASS in 0.18.23.0 release cycle) |

---

## 8. 文档组织（entry / detail 分层）

```
.specify/
├── memory/
│   ├── constitution.md        # 项目宪法（5 原则 + 9 硬规则 + P1-P8）
│   └── lessons-learned.md     # L01-L18 教训库
├── PRD.md                      # 本文件：v2 全局 PRD
├── TDD.md                      # 项目级 TDD 策略（测试金字塔 + CI）
├── AGENTS.md 引用              # (项目根的 AGENTS.md)
└── specs/
    ├── 000-003  (4 份)         # 品牌化 + 安装 + 构建（已 ship）
    ├── 004-fluxing-v2-roadmap/ # v2 路线图（元 spec）
    ├── 005-011  (7 份)         # v2 7 份子 spec
    ├── 012-shift-hotkey-actual-fix/  # spec 005 修复（已 ship 0.18.4）
    └── 013-d-flag-silent-mode-override/  # /D= 修复（未 ship）
```

**查找路径**：
- 新人 onboarding → 先读 `PRD.md` + `TDD.md` + `constitution.md` + `lessons-learned.md`
- 某子 spec 实施 → 读 `specs/NNN-*/{spec,plan,tasks}.md`（3 件套）
- 某子 spec 详细技术讨论 → 读 `specs/NNN-*/design.md`
- 踩坑排查 → 搜 `lessons-learned.md` 的 L## 关键词
- 修 release 前的 install.nsi → 读 `AGENTS.md §4.1` + `L09` + `L13` + `L17`

---

## 9. 维护 / 评审节奏

- **每周**：子 spec 状态变更时同步更新 §3（状态）+ §5（局部 SC）。
- **每月**：跨子 spec 集成场景（§6）跑一遍，验证 SC-001 ~ SC-006。
- **每季度**：与用户对账 §1（用户画像是否还准确）+ §2（价值主张排序）。
- **每次 release**：风险登记（§7）按发生事件更新 ID + 缓解状态。

---

## 10. 状态快照

### 2026-07-01: v0.18.6.0 已 ship

- **v0.18.6.0 已 ship**（commit db70099 + tag v0.18.6.0）— installer /release/fluxing-0.18.6.0-installer.exe (40.6 MB)。
- **L19 修复**（commit e2c36b1）：default.yaml 中 keycode=Shift_L/R 全部 binding 移除；候选选择改用 Control+1/2；切中英保留 Shift+space。
- **L20 lessons-learned**（commit db70099）：NSIS silent install via PS 5.1 Start-Process -Wait hangs；workaround 用 cmd /c wrapper。同步记录 librime build.bat CMAKE_GENERATOR 空格分词 bug、WinSparkle.lib stub 问题、xmake after_build hook 遗漏。
- **Smoke test 8/8 PASS**（AGENTS.md §2.5）：fluxing\weasel\ 路径正确；HKLM InstallDir / HKCU RimeUserDir 正确；rime.dll 2.9 MB；prebuilt /rime_ice.table.bin 存在；L14 架构一致性（Weasel*.exe x86 + weaselx64.dll x64）合规。
- **装出 default.yaml L19 验证**（L20）：含 Control+1, send: 2 + Control+2, send: 3；不含 Shift+Shift_L, send: 2；含 Shift+space toggle ascii_mode。
- **TestDefaultHotkeys 31/31 PASS**（commit e2c36b1）：从 25/25 升级；新增 6 个 L19 负断言 + 1 个 L19 正断言（active toggle 路径数 == 1）。
- **待用户重装 0.18.6 实测**：shift+Enter / shift+<letter> 是否仍切中英（应不切）；Shift+space 是否仍切中英（应切）；候选窗打开时 Control+1 / Control+2 是否选第 2/3 候选（应选）。

### 2026-07-01: PRD v1.0 + TDD v1.0 首次撰写

- **PRD v1.0**（本文）首次撰写。
- **8 份子 spec** 已 ship spec.md/plan.md/tasks.md 3 件套（commit 0fe1cd9：004 + 005 + 006 + 007 + 008 + 009 + 010 + 011）。注：早期 PRD 草稿写"7 份"是少算了 spec 004 元 spec，**实际是 8 份**。
- spec 005 已 ship 0.18.0-0.18.5；L18 修复（commit e4095f2）**已被 L19 替代**（commit e2c36b1）。
- 6 份 sub-spec（006-011）design 完成，代码未开始。
- TDD.md v1.0 同日撰写。

### 2026-07-03: v0.18.7.0 已 ship

- **v0.18.7.0 已 ship** (commit 6014587) - ci.yml test job added; scripts\run-tests.bat wrapper; TestDefaultHotkeys vcxproj + sln entry. **Closes R-008** (CI 不执行单测, 高/高).
- 13 test projects currently PASS (post-v0.18.23.0 verified 2026-07-04).

### 2026-07-03: v0.18.19.0 已 ship

- **v0.18.19.0 已 ship** (commit 6bb68a6) - 关闭"5 个版本无 installer" gap; release/fluxing-0.18.19.0-installer.exe (~40.6 MB).
- **L40 lessons-learned**: PRD.md / TDD.md 0x3F corruption (literal question-mark substitution, NOT GBK->UTF-8 mojibake). 字节级验证纪律: 0x3F count + visual inspection, NOT just git hash-object.
- **L41 lessons-learned**: AGENTS.md §2.5 smoke test recipe path 修正 (L13-fix-2 redirected; D: drive path, unquoted /D=).
- 5/5 P1 修复 (ci.yml test job + vcxproj 4 test + 5-version release 链).

### 2026-07-04: v0.18.20.0 已 ship

- **v0.18.20.0 已 ship** (commit 0b29703) - spec 033 FluxingDarkModeBridge (F11 dark-mode cross-cut foundation).
- **L42 LATER discovered**: spec 033 提交了 production code 但未 rebuild installer; 0.18.20.0 binary 是 pre-033 state; bridge code 0x001E1E1E palette bytes NOT in weasel.dll.
- Production code: RimeWithWeasel/FluxingDarkModeBridge.{h,cpp} + TestDarkModeBridge (18 behavior-level assertions, L24 link-probe pattern).
- WeaselPanel.cpp refactored: OnSettingChange + _RefreshStylePalette 改用 bridge->Refresh() / CurrentPalette().
- TestDarkModeBridge.exe 18/18 PASS, exit 0.

### 2026-07-04: v0.18.20.1 已 ship (hotfix)

- **v0.18.20.1 已 ship** (commit 46ee0b7) - spec 033 reverted. Build pipeline blocks the bridge link (L42 false-positive). 保持 pre-033 binary.

### 2026-07-04: v0.18.21.0 已 ship

- **v0.18.21.0 已 ship** (commit 80e209d) - spec 008 finalization (TestUserDictUpdate 4/4 PASS post-0.18.17.0) + 4 个 test project ship 增量. release/fluxing-0.18.21.0-installer.exe (~42 MB).
- 4 spec 同步 ship: spec 008 / spec 019 (TestCandidateRButtonDown) / spec 020 (TestCandidateIgnoreFilter) / spec 032 (TestPanelDarkModeSubscribe).
- 7/7 test projects PASS.

### 2026-07-04: v0.18.22.0 已 ship

- **v0.18.22.0 已 ship** (commit 1149a63) - spec 033 retry success (F11 dark-mode bridge linked in weasel.dll).
- 0x001E1E1E palette bytes verified in weasel.dll (L42 byte-verify: 0x001E1E1E in weasel.dll = 1).
- **L43 lessons-learned**: /LTCG:OFF per-target cure (not global /LTCG removal). WeaselTSF target has its own add_shflags that re-enables LTCG; per-target /LTCG:OFF is the cure.
- 12/12 test projects PASS (107 assertions).
- Release/fluxing-0.18.22.0-installer.exe (~40.6 MB).

### 2026-07-04: v0.18.23.0 已 ship

- **v0.18.23.0 已 ship** (commit 696628e) - spec 034 TestDarkModeBroadcast (F11 cross-cut integration test, unblocks spec 022 placeholder).
- **L44 lessons-learned** (post-v0.18.22.0 audit): PowerShell Encoding.UTF8.GetString + IndexOf byte-vs-char miscalculation trap. CJK content silently misaligns char offsets vs byte offsets. Cure: byte-level pattern matching.
- **Bug fix** (commit 33efa00): weasel.sln TestPanelDarkModeSubscribe missing EndProject (spec 032 leftover). Also removed inline-test dead code in TestDarkModeBroadcast.cpp T3d else-branch.
- 13/13 test projects PASS, 14 new TestDarkModeBroadcast assertions PASS.
- release/fluxing-0.18.23.0-installer.exe (42,655,987 bytes).
- spec 022 placeholder now unblocked (was BLOCKED on spec 004 production code; block lifted by spec 033).

---

**Total test projects (post-v0.18.23.0):** 13 (TestDefaultHotkeys, TestShiftSelectBinding, TestBindingResolution, TestResponseParser, TestWeaselIPC, TestYamlRoundTripE2E, TestUserDictUpdate, TestCandidateRButtonDown, TestCandidateIgnoreFilter, TestPanelDarkModeSubscribe, TestTrayRestoreIgnored, TestDarkModeBridge, TestDarkModeBroadcast).

**Total assertions:** 35+107+6+4+4+5+3+3+18+14 = 199+ across the 13 test projects. (Exact counts vary per release; this is the post-v0.18.23.0 snapshot.)

**Installer release chain (post-L40 fix):** 0.18.19.0, 0.18.20.0, 0.18.20.1, 0.18.21.0, 0.18.22.0, 0.18.23.0 (all have matching release/fluxing-X.Y.Z-installer.exe in git; 0.18.8-0.18.18 tags were CHANGELOG-only and have NO installer binary).

**Lessons-learned count:** L01-L44 (44 lessons) by 2026-07-04.

**Spec count:** 35 spec directories (000-035, with 013, 025-bookkeeping, 025-bootstrapper naming variants per L23 history).

**Code coverage target (sec 7):** deferred to v2.1+ (no measurement tool wired into ci.yml yet).

---

## 9. v0.18.25.0 已 ship (2026-07-05)

### 9.1 L46 fix - xmake + msbuild dual-path parity

spec 033 (0.18.22.0) 和 spec 034 (0.18.23.0) 之前 ship 时, 仅 xmake path 验证, msbuild path 有 4 个 bug 未闭合. spec 037 (0.18.25.0) 闭合了全部 4 个:

1. weasel.props ResourceCompile PreprocessorDefinitions 改为 $(VERSION_MAJOR) 等 msbuild 变量展开 (RC2127 修复).
2. WeaselUI/WeaselUI.vcxproj ClCompile 加 ..\RimeWithWeasel\FluxingDarkModeBridge.cpp + ..\RimeWithWeasel\WeaselUtility.cpp (LNK2001 + LNK1120 修复).
3. RimeWithWeasel/RimeWithWeasel.vcxproj ClCompile 加 FluxingDarkModeBridge.cpp.
4. RimeWithWeasel/FluxingDarkModeBridge.cpp 加 #include "stdafx.h" 作为第一行 (C1010 修复).

L43 fix: WeaselTSF/xmake.lua per-target /LTCG:OFF (WeaselTSF target 自身 add_shflags 重启 LTCG; per-target 修复).
L46 #2 fix: WeaselServer/xmake.lua /OPT:REF /OPT:ICF → /OPT:NOREF /OPT:NOICF (L42 sibling bug for build path).

### 9.2 验证 (L46 recipe, 3 paths all PASS)

- Path 1: xbuild.bat weasel installer → exit 0, installer 42,850,220 bytes, PE arch 0x14C x86.
- Path 2: msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 → 0 errors / 2 pre-existing warnings (C4267 + C4101, 无关), 8/8 production targets success.
- Path 3: scripts\test-infra\run-test-suite.bat → 13/13 test exe PASS, 115 assertions / 0 FAIL, "=== ALL TESTS PASSED ===".
- L42 byte-verify: weasel.dll 包含  x001E1E1E (spec 033 dark-mode palette bytes in production binary).
- L14 arch-verify: WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe / rime.dll = 0x14C x86; weaselx64.dll = 0x8664 x64; weaselARM64.dll = 0xAA64 ARM64.

### 9.3 0.18.25.0 Test suite

13 test projects, 115+ assertions PASS:
- TestDefaultHotkeys 35/35 (spec 014/021 Shift_L/R recovery + L16/L19 regressions).
- TestQuickPanelDialog 10/10 (spec 036 v0 ship).
- TestDarkModeBridge 18/18, TestDarkModeBroadcast 14/14, TestPanelDarkModeSubscribe 3/3.
- TestUserDictUpdate 4/4 (spec 008 finalization), TestCandidateRButtonDown 4/4 (spec 019), TestCandidateIgnoreFilter 5/5 (spec 020).

### 9.4 L31 follow-up fix (L31 vcxproj OutDir path-glue 闭合)

3 个 test vcxproj (TestBindingResolution, TestResponseParser, TestYamlRoundTripE2E) 的 <IntDir>msbuild\... 缺 \ 反斜杠, 触发 MSB3491 imemsbuild 路径 (L31 root cause B). 0.18.25.0 ship 时已修. 验证: msbuild_path2.log 中无 imemsbuild 字符串, un-test-suite.bat exit=0.

L47 lessons-learned 同步追加 (PowerShell 5.1 + VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj $(SolutionDir)msbuild 缺 \ 引发 MSB3491; workaround: 用 cvars32.bat 不用 VsDevCmd.bat).

---

## 10. spec 037 已 bootstrap (2026-07-05)

### 10.1 范围 (YAGNI 切片)

spec 006 完整 mac 风面板设计的**第二阶段 ship 切片**. 仅 ship 4 个基础控件 (Button / Toggle / Panel / Label) + 1 个 D2DRenderer + 1 个 FluxingTheme 适配器, 闭环 v0.18.26.0 release. spec 038+ 才重构 QuickPanelDialog.

### 10.2 用户故事 (摘自 spec 037 spec.md §1.2)

- US037-A: 引入 <FluxingComponents/Button.h> 编译通过.
- US037-B: FluxingButton::Create(hwndParent, rect, L"中/英", FluxingButton::Style::Primary) 返回可绘制控件句柄.
- US037-C: FluxingPanel::Create(hwndParent, rect, FluxingPanel::Style::Card) 返回圆角矩形容器.
- US037-D: FluxingTheme 适配 FluxingDarkModeBridge::CurrentPalette(), 亮/暗色变化时通过订阅者自动重绘.
- US037-E: spec 038 重构 QuickPanelDialog 使用 4 个新控件 (推迟到 v0.18.27+).

### 10.3 完成定义 (v0.18.26.0 ship)

详见 .specify\specs\037-fluxing-components-v0\tasks.md. 28 tasks 分 7 phase (基础设施 / Theme 适配器 / 4 控件 / vcxproj+xmake 集成 / 测试 / 回归+字节验证 / Release).

### 10.4 spec 038+ 路线图

- spec 038 — QuickPanelDialog 重构 (用 FluxingButton + FluxingToggle + FluxingPanel + FluxingLabel 替换 win32 button).
- spec 039 — FluxingTheme 接入 QuickPanelDialog + 200ms 渐变.
- spec 040 — 多 DPI 验证清单 (DPI 100/150/200) + 弹窗 resize handler.
- spec 041 — FluxingComponents 拖动支持 + Alt+, 热键冲突检测.
- spec 042 — FluxingPanelHost 独立进程 + 8-12 入口 grid 布局.

### 10.5 spec 037 spec / plan / tasks 状态

-  37-fluxing-components-v0/spec.md (10623 B, BOM, CR=LF=120, 0 mojibake).
-  37-fluxing-components-v0/plan.md (7762 B, BOM, CR=LF=202, 0 mojibake).
-  37-fluxing-components-v0/tasks.md (4625 B, BOM, CR=LF=62, 0 mojibake).
- 28 tasks 全 [ ] (待实施).
- Constitution Check 通过 (I-V + R1-R9 + P1-P8 OK).

---

## 11. lessons-learned 累计

- L01-L46 (L01 - L46): 46 lessons by 2026-07-04.
- **L47 (待追加)**: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 是 PowerShell module 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 cvars32.bat (纯 cmd 脚本) 不用 VsDevCmd.bat. 0.18.25.0 ship 时已用此 workaround.
- L47 also: vcxproj <IntDir>msbuild\... 缺 \ 反斜杠触发 MSB3491 imemsbuild 路径错误. spec 037 之前 3 个 test vcxproj 命中此 bug; 0.18.25.0 ship 时已修.

---

## 9. v0.18.25.0 已 ship (2026-07-05)

### 9.1 L46 fix - xmake + msbuild dual-path parity

spec 033 (0.18.22.0) 和 spec 034 (0.18.23.0) 之前 ship 时, 仅 xmake path 验证, msbuild path 有 4 个 bug 未闭合. spec 037 (0.18.25.0) 闭合了全部 4 个:

1. weasel.props ResourceCompile PreprocessorDefinitions 改为 $(VERSION_MAJOR) 等 msbuild 变量展开 (RC2127 修复).
2. WeaselUI/WeaselUI.vcxproj ClCompile 加 ..\RimeWithWeasel\FluxingDarkModeBridge.cpp + ..\RimeWithWeasel\WeaselUtility.cpp (LNK2001 + LNK1120 修复).
3. RimeWithWeasel/RimeWithWeasel.vcxproj ClCompile 加 FluxingDarkModeBridge.cpp.
4. RimeWithWeasel/FluxingDarkModeBridge.cpp 加 #include "stdafx.h" 作为第一行 (C1010 修复).

L43 fix: WeaselTSF/xmake.lua per-target /LTCG:OFF (WeaselTSF target 自身 add_shflags 重启 LTCG; per-target 修复).
L46 #2 fix: WeaselServer/xmake.lua /OPT:REF /OPT:ICF → /OPT:NOREF /OPT:NOICF (L42 sibling bug for build path).

### 9.2 验证 (L46 recipe, 3 paths all PASS)

- Path 1: xbuild.bat weasel installer → exit 0, installer 42,850,220 bytes, PE arch 0x14C x86.
- Path 2: msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 → 0 errors / 2 pre-existing warnings (C4267 + C4101, 无关), 8/8 production targets success.
- Path 3: scripts\test-infra\run-test-suite.bat → 13/13 test exe PASS, 115 assertions / 0 FAIL, "=== ALL TESTS PASSED ===".
- L42 byte-verify: weasel.dll 包含  x001E1E1E (spec 033 dark-mode palette bytes in production binary).
- L14 arch-verify: WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe / rime.dll = 0x14C x86; weaselx64.dll = 0x8664 x64; weaselARM64.dll = 0xAA64 ARM64.

### 9.3 0.18.25.0 Test suite

13 test projects, 115+ assertions PASS:
- TestDefaultHotkeys 35/35 (spec 014/021 Shift_L/R recovery + L16/L19 regressions).
- TestQuickPanelDialog 10/10 (spec 036 v0 ship).
- TestDarkModeBridge 18/18, TestDarkModeBroadcast 14/14, TestPanelDarkModeSubscribe 3/3.
- TestUserDictUpdate 4/4 (spec 008 finalization), TestCandidateRButtonDown 4/4 (spec 019), TestCandidateIgnoreFilter 5/5 (spec 020).

### 9.4 L31 follow-up fix (L31 vcxproj OutDir path-glue 闭合)

3 个 test vcxproj (TestBindingResolution, TestResponseParser, TestYamlRoundTripE2E) 的 <IntDir>SolutionDir-msbuild-... 缺 \ 反斜杠, 触发 MSB3491 
imemsbuild 路径 (L31 root cause B). 0.18.25.0 ship 时已修. 验证: msbuild_path2.log 中无 
imemsbuild 字符串, 
un-test-suite.bat exit=0.

L47 lessons-learned 同步追加 (PowerShell 5.1 + VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj SolutionDir-msbuild 缺 \ 引发 MSB3491; workaround: 用 cvars32.bat 不用 VsDevCmd.bat).

---

## 10. spec 037 已 bootstrap (2026-07-05)

### 10.1 范围 (YAGNI 切片)

spec 006 完整 mac 风面板设计的**第二阶段 ship 切片**. 仅 ship 4 个基础控件 (Button / Toggle / Panel / Label) + 1 个 D2DRenderer + 1 个 FluxingTheme 适配器, 闭环 v0.18.26.0 release. spec 038+ 才重构 QuickPanelDialog.

### 10.2 用户故事 (摘自 spec 037 spec.md sec 1.2)

- US037-A: 引入 <FluxingComponents/Button.h> 编译通过.
- US037-B: FluxingButton::Create(hwndParent, rect, L"中/英", FluxingButton::Style::Primary) 返回可绘制控件句柄.
- US037-C: FluxingPanel::Create(hwndParent, rect, FluxingPanel::Style::Card) 返回圆角矩形容器.
- US037-D: FluxingTheme 适配 FluxingDarkModeBridge::CurrentPalette(), 亮/暗色变化时通过订阅者自动重绘.
- US037-E: spec 038 重构 QuickPanelDialog 使用 4 个新控件 (推迟到 v0.18.27+).

### 10.3 完成定义 (v0.18.26.0 ship)

详见 .specify\specs\037-fluxing-components-v0\tasks.md. 28 tasks 分 7 phase (基础设施 / Theme 适配器 / 4 控件 / vcxproj+xmake 集成 / 测试 / 回归+字节验证 / Release).

### 10.4 spec 038+ 路线图

- spec 038 — QuickPanelDialog 重构 (用 FluxingButton + FluxingToggle + FluxingPanel + FluxingLabel 替换 win32 button).
- spec 039 — FluxingTheme 接入 QuickPanelDialog + 200ms 渐变.
- spec 040 — 多 DPI 验证清单 (DPI 100/150/200) + 弹窗 resize handler.
- spec 041 — FluxingComponents 拖动支持 + Alt+, 热键冲突检测.
- spec 042 — FluxingPanelHost 独立进程 + 8-12 入口 grid 布局.

### 10.5 spec 037 spec / plan / tasks 状态

-  37-fluxing-components-v0/spec.md (10623 B, BOM, CR=LF=120, 0 mojibake).
-  37-fluxing-components-v0/plan.md (7762 B, BOM, CR=LF=202, 0 mojibake).
-  37-fluxing-components-v0/tasks.md (4625 B, BOM, CR=LF=62, 0 mojibake).
- 28 tasks 全 [ ] (待实施).
- Constitution Check 通过 (I-V + R1-R9 + P1-P8 OK).

---

## 11. lessons-learned 累计

- L01-L46 (L01 - L46): 46 lessons by 2026-07-04.
- **L47 (待追加)**: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 是 PowerShell module 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 cvars32.bat (纯 cmd 脚本) 不用 VsDevCmd.bat. 0.18.25.0 ship 时已用此 workaround.
- L47 also: vcxproj <IntDir>SolutionDir-msbuild-... 缺 \ 反斜杠触发 MSB3491 
imemsbuild 路径错误. spec 037 之前 3 个 test vcxproj 命中此 bug; 0.18.25.0 ship 时已修.