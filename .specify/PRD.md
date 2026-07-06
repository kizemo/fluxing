# Fluxing v2 · Product Requirements Document (PRD)

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

| 编号 | 需求 | 优先级 | 子 spec | 状态（2026-07-05, 0.18.26.0） |
|---|---|---|---|---|
| F1 | 默认快捷键（`,`/`.` 翻页 / `Shift_L/R` 选候选 / `Shift+space` 切中英 / `Ctrl+Shift+9/0` 标点/简繁） | P1 | 005 | **已 ship 0.18.0-0.18.5** |
| F2 | 托盘快速设置面板（`Alt+,` / 左键单击托盘 → 8-12 个常用入口） | P1 | 006/036/038 | **ship 0.18.24.0-0.18.25.0** (spec 036) + spec 038 待 ship (QuickPanelDialog 重构) |
| F3 | yaml 可视化设置 UI（4 类配置可视化编辑） | P1 | 007 | design 完成，代码未开始 |
| F4 | 候选字右键一键删除/屏蔽 | P1 | 008 | design 完成，代码未开始 |
| F5 | 常用短语独立于用户词典（`Alt+K` 列表） | P1 | 009 | design 完成，代码未开始 |
| F6 | 暗色主题跟随系统（横切 006/007/008/009） | P1 | 004/033/037 | **ship 0.18.22.0 + 0.18.26.0** (spec 037 FluxingComponents 控件库 v0) |

### 3.2 P2 仅设计（2 项）

| 编号 | 需求 | 优先级 | 子 spec | 状态 |
|---|---|---|---|---|
| F7 | 候选字第二行：剪贴板 + 输入历史 | P2 | 007 | 推迟到 v2.1+ |
| F8 | 中英混合输入 / ASCII 模式 auto | P2 | 005 扩展 | 推迟到 v2.1+ |
| F9 | 托盘图标合成叠点 | P2 | 006 扩展 | 推迟到 v2.1+ |
| F10 | 导出/导入方案（rime.zip） | P2 | 010 | P2 设计中 |
| F11 | 暗色主题跟随系统 | P1 | 033/034 | **ship 0.18.22.0-0.18.25.0** (spec 033 FluxingDarkModeBridge + 034 TestDarkModeBroadcast) |
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

3 个 test vcxproj (TestBindingResolution, TestResponseParser, TestYamlRoundTripE2E) 的 <IntDir>msbuild\... 缺 \ 反斜杠, 触发 MSB3491 imemsbuild 路径 (L31 root cause B). 0.18.25.0 ship 时已修.

## 

## 10.8 v0.18.27.2 hotfix 已 ship (2026-07-06)

L51 hotfix 修复 3 个 user-reported 问题 (after installing 0.18.27.1):

**Bug A (QuickPanelDialog layout)**:`n- 顶部黑色横条 (TitleLabel 文字被截, 17pt Large 需要 ~28px height, 旧 height=20px 截断).`n- 中部白色 (CardPanel D2D rt 失败 fallback 缺失, hbrBackground=nullptr, 透出 dialog COLOR_WINDOW+1).`n- "Deplo" 文字 (deploy_rc width=95 太窄, 加 6px 圆角后文字区只 65px).`n- X 按钮与 TitleLabel 重叠 (用 QP_WIDTH-30 不用 client.right-25, 跨 WS_BORDER 边界).`n`n**Bug B (lang bar integration)**:`n- 左键中英文状态托盘 -> 切 ASCII 而非 QuickPanel (LanguageBar.cpp::OnClick TF_LBI_CLK_LEFT 走 ENABLE_ASCII, 不走 ID_WEASELTRAY_QUICK_PANEL). spec 036 US036-B 设计是左键 -> QuickPanel, 但只接了 WeaselServer tray icon, 没接 lang bar.`n- 右键 lang bar menu 找不到 QuickPanel item (3 个 popup menu.rc 都缺 ID_WEASELTRAY_QUICK_PANEL entry).`n`n**Bug C (post-install UX)**:`n- 安装后未重启 WeaselServer.exe, 旧 binary 仍在内存, 所有修复不可见. Installer 没提示重启.`n`n**Cure (10 files)**:`n1. WeaselServer/QuickPanelDialog.cpp - L50 layout: title_rc height 20->26, X button client.right-25 (not QP_WIDTH-30), card_rc right client.right-5 -> client.right-2, deploy_rc width 95->100, toggle/deploy 切换到 card-local coords.`n2. WeaselUI/FluxingComponents/Panel.cpp - L50 D2D fallback: CreateSolidBrush(pal.back) + FillRect + DeleteObject.`n3. WeaselUI/FluxingComponents/Label.cpp - L50 D2D fallback: CreateFontIndirectW(Segoe UI, 17pt) + DrawTextW.`n4. WeaselUI/FluxingComponents/Button.cpp - L50 D2D fallback: FillRect(colors.fill) + DrawTextW.`n5. WeaselUI/FluxingComponents/Toggle.cpp - L50 D2D fallback: FillRect(track_color) + Ellipse knob.`n6. WeaselTSF/LanguageBar.cpp - L51 OnClick TF_LBI_CLK_LEFT -> _HandleLangBarMenuSelect(ID_WEASELTRAY_QUICK_PANEL) (m_client.TrayCommand IPC -> WeaselServer.AddMenuHandler -> QuickPanelDialog::Show). ASCII 切换仍可走 Shift+Space (spec 005) + QuickPanelDialog 内部 toggle.`n7. WeaselTSF/WeaselTSF.rc - 3 个 popup menu (POPUP/POPUP_HANS/POPUP_HANT) 加 MENUITEM 快捷设置栏 (&K)/tAlt+, ID_WEASELTRAY_QUICK_PANEL 在 Settings 之后.`n8. include/resource.h - 加 ID_HOTKEY_QUICK_PANEL 9001 + ID_WEASELTRAY_QUICK_PANEL 40018 (WeaselTSF/LanguageBar.cpp 通过 include path 用).`n9. output/install.nsi - Section Fluxing 完成前加 IfSilent-wrapped MessageBox MB_OK|MB_ICONINFORMATION 提示用户重启 WeaselServer.exe 或注销后重新登录. /S 静默安装跳过 messagebox (避免无人值守部署阻塞).`n10. BOM + line ending cleanup: 5 个 .cpp 文件移除 stray UTF-8 BOMs (上次 spec 037 + spec 038 byte-level patch 引入的 L47 violation), 统一为 LF only line endings (匹配 HEAD convention).`n`n**Verification (L46 三路径 hard gate, 全 0 errors)**:`n- xbuild.bat weasel installer -> installer 42,873,293 bytes (vs 0.18.27.1 42,861,509 bytes; +11,784 bytes 因 L50 D2D fallback GDI code).`n- msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /m -> 0 errors. First attempt failed with error C2065: ID_WEASELTRAY_QUICK_PANEL not declared, cure: 加 ID 到 include/resource.h.`n- scripts/test-infra/run-test-suite.bat -> ALL TESTS PASSED. 16 test projects (TestQuickPanelRefactor 9/9 + TestFluxingComponents 4/4 + TestDefaultHotkeys 20/20 + 12 others + TestWeaselIPC integration).`n- AGENTS.md sec 2.5 silent-install smoke test 8 invariants PASS + L14 arch-verify (6 binary arch consistent).`n- L42 byte-verify: 0x1E1E1E triple 在 weasel.dll 15 occurrences.`n- L47 byte-verify: 所有 modified .cpp/.h BOM=False LF only; install.nsi BOM=True CRLF only.`n- L49 pre-flight guard: MESSAGE_HANDLER(WM_HOTKEY, OnHotkey) 命中, exit 0.`n`nrelease/fluxing-0.18.27.2-installer.exe (42,873,293 bytes, vs 0.18.27.1 42,861,509 bytes; +11,784 bytes 因 L50 D2D fallback GDI code).`n`nL51 正式追加 (D2D fallback mandatory for production UI components; Windows TSF standard lang bar 左键 = toggle ASCII conflicts with spec 036 US036-B 设计; post-install restart prompt mandatory for binaries locked by Windows TSF shim).`n`n## `n`nv0.18.27.1 hotfix 已 ship (2026-07-06)

L49 hotfix 修复 spec 036 (0.18.24.0) 起的 Alt+, global hotkey bug (4 个 release 版本未工作).

**Root cause**: WeaselIPCServer/WeaselServerImpl.h 声明了 OnHotkey 函数 + WeaselServerImpl.cpp:76 注册 hotkey + 函数体正确实现 (PostMessage WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL), 但 BEGIN_MSG_MAP 块未加 MESSAGE_HANDLER(WM_HOTKEY, OnHotkey). WM_HOTKEY 消息无 handler 路由, 被 ATL 默认 handler 丢弃.

**Cure (3 changes)**:
1. WeaselServerImpl.h line 31 加 MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)
2. TestQuickPanelRefactor.cpp 加 T0: ActiveHwnd() == NULL before any Show() call (9/9, was 8/8)
3. scripts/test-infra/run-test-suite.bat 加 L49 pre-flight guard: findstr /C:"MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)" WeaselIPCServer\\WeaselServerImpl.h. 缺失该行就 [L49 GUARD FAIL] 退出 with code 1

**Verification (L46 recipe, 3 paths all PASS)**: xbuild.bat weasel installer (42.86 MB) + msbuild weasel.sln (0 errors) + run-test-suite.bat (15/15 PASS, TestQuickPanelRefactor 9/9). L49 guard 验证: 临时删 MESSAGE_HANDLER 行 + 跑 run-test-suite -> 在 build 之前就 [L49 GUARD FAIL] 退出 with code 1. 恢复后 PASS.

release/fluxing-0.18.27.1-installer.exe (42,861,509 bytes, vs 0.18.27.0 42,873,668 bytes; -12,159 bytes 因 VERSION string tables 变化).

L49 正式追加 (ATL/WTL 消息映射是 runtime construct, 编译器无法静态验证 function reachable via message map; 编译通过 != 消息路由正确; "OnHotkey 函数存在 != hotkey 工作"; 长期修复 spec 039 follow-up: 加 GUI-loop 集成测试真实 instantiate ServerImpl + RegisterHotKey + 发 WM_HOTKEY + verify handler fired).

## 11. lessons-learned 累计 (更新)

- L01-L49 (L01 - L49): 49 lessons by 2026-07-06 (L47 + L48 + L49 added).
- L49 (新): spec 036 (0.18.24.0) 起 OnHotkey 函数存在但 MESSAGE_HANDLER 缺失, 4 个 release 版本 (0.18.24.0-0.18.27.0) Alt+, 全无效. ATL/WTL message map 是 runtime construct, 编译通过 != 路由正确. Cure: 加 MESSAGE_HANDLER(WM_HOTKEY, OnHotkey) + L49 pre-flight guard 防止再删 + T0 lifecycle invariant test. 0.18.27.1 hotfix 已应用.
10.7 v0.18.27.0 已 ship (2026-07-05)

spec 038 (QuickPanelDialog 重构 + FluxingComponents 化) + L48 防御性测试退出模式追加. 5 production files (QuickPanelDialog.h+.cpp 重写, WeaselServer.vcxproj + xmake.lua 加 include paths) + 7 test files (TestQuickPanelRefactor/ 全套 7 文件 + TestQuickPanelDialog.cpp 改写为 spec 038 适配 15 assertions) + weasel.sln + TestQuickPanelDialog.vcxproj + run-test-suite.bat 更新.

**TestQuickPanelRefactor** (新项目): 8 assertions PASS (T1 5 descendants + T2a/T2b toggle initial state + T3a/T3b WM_LBUTTONUP no crash + T4 dark-mode re-invalidate + T5 Deploy button Primary style).

**TestQuickPanelDialog** (改写): 15 assertions PASS + ExitProcess(rc) 跳过 atexit static destructors (L48 防御性测试退出模式, FluxingD2DRenderer singleton 析构 crash 防护).

15/15 test projects (含 TestQuickPanelRefactor 新项目), ALL TESTS PASSED. L42 byte-verify: 0x001E1E1E 仍在 weasel.dll (15 triple matches). L14 arch-verify: WeaselServer/Deployer/Setup/rime.dll = 0x14C x86, weaselx64.dll = 0x8664 x64, weaselARM64.dll = 0xAA64 ARM64, weaselARM.dll = 0x01C4 ARM. L47 byte-verify: 全部 source file byte-healthy (C0=0 C1=0, .h/.cpp LF, .sln/.bat CRLF).

release/fluxing-0.18.27.0-installer.exe (42,873,668 bytes, +20,472 bytes from 0.18.26.0).

L48 正式追加 (7 root-causes from spec 038 verification: FluxingD2DRenderer atexit crash + GetClassNameW NULL buffer bug + sln ProjectConfiguration line-separation + msbuild vcxproj-direct SolutionDir trap + QuickPanelDialog s_hwnd lifecycle + unique_ptr<Fluxing*> accessor pattern AP-038-A + WM_LBUTTONUP callback contract). spec 039 bootstrap (FluxingComponents 动画扩展 + 4 新控件 Slider/Dropdown/Checkbox/Radio) 已创建.

## 11. lessons-learned 累计 (更新)

- L01-L48 (L01 - L48): 48 lessons by 2026-07-05 (L47 + L48 added).
- L47: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), VsDevCmd.bat 触发 .NET Hashtable 异常. workaround: vcvars32.bat (纯 cmd). vcxproj <IntDir> 缺 \ 反斜杠触发 MSB3491. 0.18.25.0/0.18.26.0 ship 时已修.
- L48 (新): spec 038 期间发现 7 个 root-causes (FluxingD2DRenderer atexit crash → ExitProcess + GetClassNameW NULL buffer → 0 + sln ProjectConfiguration 必须 line-separated + msbuild vcxproj-direct SolutionDir 是 project-local + QuickPanelDialog s_hwnd 仅 Show() 设置 + unique_ptr 静态成员需要 public accessors + WM_LBUTTONUP callback contract). 0.18.27.0 ship 时已应用全部 fix.
 验证: msbuild_path2.log 中无 imemsbuild 字符串, un-test-suite.bat exit=0.

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

## 10.6 v0.18.26.0 已 ship (2026-07-05)

spec 037 (FluxingComponents 控件库 v0) 28 tasks 全 [x]. 14 production files (Button / Toggle / Panel / Label + D2DRenderer + FluxingTheme) + 4 test files (TestFluxingButton / Toggle / Panel / Theme + TestFluxingMain) + vcxproj + xmake.lua 集成 + weasel.sln 1 project 注册.

TestFluxingComponents: 35+ assertions PASS (Button 8/8 + Toggle 12/12 + Panel 6/6 + Theme 9/9). 

15/15 test projects, ALL TESTS PASSED. L42 byte-verify: 0x001E1E1E 仍在 weasel.dll. L14 arch-verify: 6 binary x86=0x14C, weaselx64.dll=0x8664.

release/fluxing-0.18.26.0-installer.exe (42,853,196 bytes).

L47 正式追加 (6 byte/syntax bugs from spec 037 reactive fix cascade). spec 038 bootstrap (QuickPanelDialog 重构使用 spec 037 4 个控件) 已创建.

## 11. lessons-learned 累计

- L01-L47 (L01 - L47): 47 lessons by 2026-07-05.
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

## 12. spec 041 v0.18.28.0 已 ship (2026-07-06)

### 12.1 问题 (L52 root cause)

v0.18.27.2 ship 后用户在 144 DPI 显示器上反馈 QuickPanelDialog 视觉错乱: 黑顶条覆盖整个 dialog 顶部 26px + 标题"Quick Panel"文字看不到 + CardPanel 不显示圆角 + Deploy 按钮看不到文字。spec 037 R2 已明确推迟 DPI validation 到 v0.18.28+, 0.18.27.x 多版本 hotfix (L50 / L51) 均未真正修复 DPI 处理。

### 12.2 修复路径 (3 次失败, 第 4 次成功)

- **Attempt 1 (v0.18.27.2 L50)**: D2D rt dpi=96 default + backing store = child logical size + D2D1::RectF = logical. 144 DPI 下 child 物理 170×17, 文字 17pt @ dpi=96 = 17 物理像素 → 装入 17 物理像素 child 高度时 ascent+descent 22.7 物理像素 > 17 backing store → 文字上下被裁. **失败**.
- **Attempt 2 (spec 041 v0.18.28.0 first pass)**: GetPhysicalClientRect 转换 logical→physical, backing store pixelSize = logical × 96/dpi 缩. 144 DPI 下 backing store 113×11 比 HWND 物理 170×17 还小, D2D1::RectF physical. backing store < HWND 物理 surface → 文字渲染区域不足. **失败**.
- **Attempt 3 (v0.18.28.0 working)**: GetPhysicalClientRect 返回 raw GetClientRect (= HWND 物理 size, V1 child physical = logical × 96/dpi 实际, 与 top-level logical × dpi/96 方向相反), backing store = HWND 物理 size, D2D rt dpi=96 default (不传 dpi), D2D1::RectF = physical (1:1 to backing store), 4 控件加 `rt->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f))` 防止 D2D opaque-black backing store 透出. **成功**.

### 12.3 实现 (5 production files + 5 targetver + 0 test 增量)

- `WeaselUI/FluxingComponents/D2DRenderer.{h,cpp}` - GetPhysicalClientRect 返回 raw physical, CreateHwndRenderTarget 用 `D2D1_HwndRenderTargetProperties(hwnd, physical_size)`.
- `WeaselUI/FluxingComponents/{Label,Panel,Button,Toggle}.{h,cpp}` - HandlePaint 加 `rt->Clear(COLOR_WINDOW)`, WM_DPICHANGED handler ReleaseHwndRenderTarget + InvalidateRect.
- `WeaselUI/FluxingComponents/targetver.h` + `WeaselUI/targetver.h` - 升 _WIN32_WINNT_WIN10 (GetDpiForWindow).
- `WeaselServer/stdafx.h` - _WIN32_WINNT 0x0603 → 0x0A00 (C4005 macro redefine 修复).
- `test/{TestFluxingComponents,TestQuickPanelDialog,TestQuickPanelRefactor}/targetver.h` - 同步升 WIN10.

### 12.4 验证 (L46 recipe, 3 paths all PASS)

- **xbuild.bat weasel installer** → exit 0, installer 42,859,365 bytes (vs 0.18.27.2 42,873,293 bytes; -13,928 bytes 因 D2D/DPI 路径优化), PE arch 一致 x86=0x14C.
- **msbuild weasel.sln** → 0 errors, 0 warnings.
- **scripts/test-infra/run-test-suite.bat** → 15/15 test projects PASS, 200+ assertions (TestQuickPanelDialog 10/10 + TestQuickPanelRefactor 9/9 + TestFluxingComponents 4/4 + TestDefaultHotkeys 35/35 + 11 others), "=== ALL TESTS PASSED ===".
- **L42 byte-verify**: weasel.dll 包含 0x001E1E1E (palette 仍在).
- **L14 arch-verify**: 6 binary x86=0x14C, weaselx64.dll=0x8664.
- **L47 byte-verify**: 全部 source file byte-healthy (C0=0 C1=0, .h/.cpp LF, .sln/.bat CRLF).
- **L52 visual verify (144 DPI 实机)**: QP 物理 200×100, Label "Quick" 文字可见, CardPanel 圆角浅色背景, Toggle knob 圆形 + 灰白轨道, Deploy 按钮位置正确. **接受 spec 041 R4 "visual improved but not perfect"** (Label "Panel" 部分超出 child physical width 170, Deploy "Deploy" 文字 13pt > button 67 物理宽度的可用空间, Toggle 圆心略偏) — 完整 QP 重设计留给 spec 044+ (违反 spec 041 AP-041-B "不改 QP 几何").

### 12.5 已知问题 (符合 spec 041 R4 接受的 partial fix)

- Label "Panel" 文字部分超出 child physical width (spec 037 17pt @ 144 dpi child 物理 170 宽只能装下 "Quick " 部分, "Panel" 被 sibling 切掉). 完整 fix 需要 child logical width 在 144 dpi = 268 × 1.5 = 402 (违反 AP-041-B "不改 QP 几何"). 推迟到 spec 044+ QP 重新设计.
- Deploy button "Deploy" 文字 13pt 在 button physical 67 宽度内 visible 不完整. 同上, 推迟.
- Toggle 圆心在 144 dpi 下从 child logical 50 宽 → physical 33 宽, 圆 r=0.4*13=5.2 物理, 圆 left=10.4 物理; knob 圆 (cx, cy) = (knob_r + progress × (w - 2*knob_r), h/2) = (5.2 + 0 × (33-10.4), 6.5) = (5.2, 6.5) — knob 偏左因为 child width 收缩. 视觉可接受.

### 12.6 L52 正式追加 (DPI handling 完整 pattern)

详见 .specify/memory/lessons-learned.md L52. 关键 6 个技术要点:
1. D2D1_HwndRenderTargetProperties.pixelSize = HWND 物理 size, 不要 logical × dpi/96 缩 (Attempt 2 错误).
2. D2D1::RenderTargetProperties dpiX/dpiY 默认 96, 不要传 dpi (Attempt 2 错误).
3. D2D rt backing store 默认 opaque black, 必须 `rt->Clear()`.
4. V1 child physical = logical × 96/dpi, V1 top-level physical = logical × dpi/96 — 方向相反 (本 spec 041 plan 未察觉这一不对称).
5. GetDpiForWindow 总是 per-monitor DPI, 不等于 system DPI.
6. WM_DPICHANGED 处理: ReleaseHwndRenderTarget + InvalidateRect, 不直接 Resize.

### 12.7 release/fluxing-0.18.28.0-installer.exe

- Size: 42,859,365 bytes.
- SHA256: 5F051468E0C0FFF3245AD5883B99C636CC3D5C83265F01C38DED2B4A0C6A6205.
- 部署到 C:\Program Files\fluxing\weasel\ (weasel.dll 1,737,728 bytes / WeaselServer.exe 1,981,952 bytes / WeaselDeployer.exe 591,360 bytes), C:\Program Files\fluxing\user1\fluxing\ 数据保留.
- vcxproj 验证: xbuild.bat / msbuild 0 errors / test suite 15/15 PASS / smoke test 7 invariants PASS.

### 12.8 spec 041 spec / plan / tasks 状态

- .specify/specs/041-fluxing-components-dpi-validation/spec.md (8962 B).
- .specify/specs/041-fluxing-components-dpi-validation/plan.md (3526 B).
- .specify/specs/041-fluxing-components-dpi-validation/tasks.md (2594 B, 29 tasks T001-T029).
- 26/29 tasks [x] (T001-T013 production code + T019-T026 ship 完成, T014-T018 DPI test cases 推迟到 spec 044+ 与 QP 重新设计合并, T027-T029 P2/P3 follow-up).
- Constitution Check 通过 (I-V + R1-R9 + P1-P8 OK, AP-041-A/B/C/D/E 全部满足).

## 11. lessons-learned 累计 (更新)

- L01-L52 (L01 - L52): 52 lessons by 2026-07-06 (L47 + L48 + L49 + L50 + L51 + L52 added).
- L50 (L51 hotfix 时追溯追加): D2D 渲染不可用时 (driver hangs, registry ACLs, GPU virtualization) FluxingComponents 必须有 GDI fallback. spec 037 ship 时未加, 0.18.27.2 L51 追加. 4 控件均加 `if (!rt) { GDI fallback }` 分支.
- L51 (新): spec 036+037+038 ship 0.18.24.0-0.18.27.1 多 release 缺 LanguageBar.cpp OnClick TF_LBI_CLK_LEFT 路径 (Windows TSF 默认 ascii toggle 而非 spec 036 US036-B QuickPanel) + WeaselTSF.rc 3 popup menu 缺 ID_WEASELTRAY_QUICK_PANEL entry + QuickPanelDialog CreateFluxingControls X button 在 D2D 失败 + WS_BORDER 下重叠. 0.18.27.2 hotfix 6 文件 9 处修复 + NSIS post-install restart prompt.
- L52 (新): spec 041 v0.18.28.0 DPI handling 3 failed attempts 后第 4 次成功. 6 个关键 D2D+V1 PerMonitor DPI 技术要点 + V1 child vs top-level 物理缩放方向相反 (本 spec 041 plan 未察觉) + D2D rt backing store 默认 opaque black 必须 `rt->Clear()`. 0.18.28.0 ship 时已应用.


## 13. spec 041 v0.18.28.0 systematic-debugging re-verification (2026-07-06, L53)

Phase 2 systematic-debugging 复盘 (调用 systematic-debugging 技能)：

- **false-positive BUG #3 排除**: 之前用 ASCII strings search 报告 WeaselServer.exe 不含 GetPhysicalClientRect / HandleDpiChanged / WM_DPICHANGED symbols 是 false-positive — C++ mangled symbols 在 Windows PE .debug section 用 **UTF-16LE** 编码，ASCII search 找不到。dumpbin /DISASM 验证 WeaselServer.exe **实际**含所有 spec 041 DPI fix 函数 + Fluxing 控件代码 + QuickPanelDialog。
- **L43 /LTCG:OFF 已被 L43 cure 解决**: WeaselTSF/xmake.lua 已加 dd_shflags('/DEBUG /LTCG:OFF /OPT:NOREF /OPT:NOICF', {force = true})，global xmake.lua line 64 也加 dd_ldflags('/LTCG:OFF /INCREMENTAL:NO', {force = true})。WeaselServer.exe 不存在 dead-strip 问题。
- **L53 正式追加**: 「Windows PE binary verification 必须用 UTF-16 + dumpbin /DISASM + byte-pattern count，never trust ASCII strings alone」。

Visual verify (96 DPI Todesk session) 重新跑通：QP 300×150, 5 children 全部可见 (FluxingLabel 256×26 title + FluxingPanel 291×111 card + FluxingToggle 50×20 ascii + FluxingButton 100×24 deploy + native close 20×20)。Test suite 16/16 PASS, 200+ assertions。

installer rebuild (含全部 source data)：
- 新 SHA256: 168D4ACC9C2842D2F207B49E150A4E585621F1E3357E5824554232285711630E
- Size: 42,884,456 bytes
- 部署路径不变: C:\Program Files\fluxing\weasel\ (用户安装路径未改)

## 14. spec 042+ 路线图 (v0.18.29+)

按 PRD §10 路线图，spec 041 完成后 P1 任务还有 F3/F4/F5 (yaml UI / 候选字右键删除 / Alt+K 短语) 未 ship。P2 全部未开始。

下一阶段候选 (按优先级):
- **spec 042** - FluxingPanelHost 独立进程 + 8-12 入口 grid 布局 (P1 路线图) - 重构 QuickPanelDialog 为独立进程容器，可扩展入口网格
- **spec 043** - FluxingTheme 接入 QuickPanelDialog + 200ms 渐变 (spec 039 follow-up, animation polish)
- **spec 044** - QP 重新设计 (解决 spec 041 §12.5 已知问题: Label "Panel" 文字溢出 / Deploy 文字 13pt 超出 67 物理宽 / Toggle 圆心偏)
- **spec 045** - 多 DPI 验证清单 (DPI 100/150/200) + 窗口 resize handler (spec 040 推迟的全面验证)
- **spec 046** - T014-T018 DPI test cases (spec 041 推迟, 9 个 test cases 包含 child physical vs logical cross-DPI matrix)
- **spec 047** - WeaselPanel / FluxingPanel 集成 DPI 处理 (V1 child physical vs top-level 方向不一致, spec 037 控件在 WeaselPanel 主面板验证)

spec 042 优先级最高 (P1 路线图需求 F2 完整化), 其它按需选取。