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
| R-007 | `shift+<key>` release event 误匹配 key_binder | 中 | 中 | **L18 修复（`Shift+space` 切中英）；待 0.18.6 release 实测** | 005 spec §2.4 R3 |
| R-008 | CI 不执行单测（`ci.yml` 缺 test job） | 高 | 高 | **TDD.md §6 提出补 test job 方案** | 全局 |

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

## 10. 状态快照（2026-07-01）

- **PRD v1.0**（本文）首次撰写。
- 7 份子 spec 已 ship spec.md/plan.md/tasks.md 3 件套（commit 0fe1cd9）。
- spec 005 已 ship 0.18.0-0.18.5；L18 修复（commit e4095f2）待 0.18.6 release + 用户实测。
- 6 份 sub-spec（006-011）design 完成，代码未开始。
- TDD.md v1.0 同日撰写。