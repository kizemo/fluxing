# 004 · 火流猩输入法 v2 路线图（Fluxing v2 Roadmap）

> 元 spec。定义"火流猩输入法"在已落地的品牌化（spec 000-003）之后，v2 阶段的产品愿景、用户故事、需求范围、跨子 spec 依赖、里程碑。
> 不写实现细节；实现细节分散到 005-011 七份子 spec。

## 0. 上下文

- 当前仓库已 fork 自 rime/weasel，分支 `Fluxing`，已发布 `fluxing-0.17.5.0-installer.exe`（spec 000-003 落地）。
- 品牌：中文名"火流猩输入法"，英文名 `Fluxing`。
- 代码栈：已确定 C++/原生 Win32 控件 + C++ 基础库（Weasel 输入法框架）+ RIME 引擎 1.13.1 子模块 + 现代风格视觉（mac 风格）/高质量字体渲染（候选面板）。
- 用户数据目录：已迁移至 `%LocalAppData%\Fluxing`（spec 000、002 落地）。
- 整套 v2 在此基础上加 **6 项原始需求 + 6 项 agent 补充建议 = 12 项**。

## 1. 产品愿景

**一句话**：让火流猩输入法在不破坏"低资源占用"前提下，提供"现代中文输入法应该有的"全部体验——可视化设置、跨设备同步、本地化快捷短语。

**目标用户画像（优先级排序）**：
1. **中文重度打字者**：每天 5000 字以上，需要快速自定义短语、便捷同步。
2. **程序员 / 写作者**：中英混输，常用标点 + 颜文字 + Emoji。
3. **设计 / 内容创作者**：关心 mac 风格外观与暗色主题。
4. **偶尔打字者**：需要"装上就能用"，不读文档。

**核心价值主张**：
- **本地优先**：所有功能在没有网络的情况下都可用；云同步是**增量增强**而非前置依赖。
- **隐私优先**：不上传任何输入历史；只同步用户词典 / 常用短语 / 自定义方案。
- **资源占用低**：新增功能**不引入 .NET 运行时**，全部自绘（与现有候选面板同栈）。

## 2. 范围 / 非目标

### 2.1 范围（v2 全部要做）

| 编号 | 需求 | 简述 | 来源 | 优先级 |
|---|---|---|---|---|
| F1 | 默认快捷键调整 | `,`/`.` 翻页；`Ctrl+Shift+9/0` 切标点/简繁；`Shift_L/Shift_R` 切中英；`Shift_L/Shift_R` 选 2/3 候选 | 用户 | P1 |
| F2 | 托盘快速设置面板 | 托盘左键或 `Alt+,` 弹现代风格视觉（mac 风格）面板 | 用户 | P1 |
| F3 | yaml 可视化设置 UI | 快捷键 / 短语 / 词典 / 方案 mac 风编辑器 | 用户 | P1 |
| F4 | 候选字删除 | 右键一键删（≥2 字节用户词典命中）；不可删的"屏蔽" | 用户 | P1 |
| F5 | 常用短语独立库 | `Alt+K` 弹列表，上下 + Enter 上屏，可编辑 | 用户 | P1 |
| F6 | 跨设备云同步 | Vercel + 邮箱 + 同步用户词典/短语/方案 | 用户 | P2（仅设计） |
| F7 | 候选字窗第二行：剪贴板 + 输入历史 | 借鉴快捷面板的"最近 5 条"面板 | agent 建议 1 | P2 |
| F8 | 中英混合输入 / ASCII 模式 auto 态 | 两套主词库（中文 rime_ice + 英文 luna_english） | agent 建议 2 | P1 |
| F9 | 托盘图标合成叠点 | 状态点（自绘合成，不新加 ICO） | agent 建议 3 | P2 |
| F10 | 导出/导入方案（.rime.zip） | 离线降级同步方案 | agent 建议 4 | P2 |
| F11 | 暗色主题跟随系统 | `WM_SETTINGCHANGE` + 渐变 200ms | agent 建议 5 | P1 |
| F12 | Emoji 面板（`Alt+E`） | 静态 emoji JSON + 复用品类 009 面板 | agent 建议 6 | P2 |

### 2.2 非目标（v2 明确不做）

- **不重写候选面板**（沿用 WeaselUI/WeaselPanel）。
- **不动 TSF 文本服务**（沿用 WeaselTSF）。
- **不动 RIME 引擎子模块**（fork 不上推）。
- **不引入 .NET 运行时 / WPF**（保持零新增依赖）。
- **不开放 Web/移动端**（仅 Windows 客户端）。
- **不做"输入法引擎本身"的特性**（RIME 引擎层面的 schema 调整允许，但不上推）。
- **不做 macOS / Linux 客户端**（专注 Windows）。
- **不重做安装/卸载界面**（保留安装器风格 → 推到 spec 011 bootstrapper）。

## 3. 用户故事

### US1 · 调整默认快捷键不读文档 [P1, MVP]

- **Why P1**：开箱即用；不读文档就能上手。
- **Independent test**：全新安装 Fluxing v2.0.0，按 `,` 看到候选翻下一页；按 `Ctrl+Shift+9` 切换中英标点。
- **Acceptance**：
  - Given 全新安装 Fluxing v2.0.0
  - When 用户在任意输入框打字并出现候选
  - Then 按 `,` 翻下一页，按 `.` 翻上一页；按 `Shift_L` 或 `Shift_R` 任一可在中英文之间切换；按 `Ctrl+Shift+9` 切换中英标点；按 `Ctrl+Shift+0` 切换简繁；按 `Shift_L` 上屏第 2 候选，`Shift_R` 上屏第 3 候选。

### US2 · 托盘一键打开快速设置 [P1, MVP]

- **Why P1**：替代 Weasel 7 级菜单深度；提供"使用频率最高的设置"门面。
- **Independent test**：点击托盘图标，弹出 mac 风面板。
- **Acceptance**：
  - Given Fluxing 已在系统托盘运行
  - When 用户左键单击托盘图标，或按 `Alt+,` 全局热键
  - Then 弹出 mac 风快速设置面板，含"切换中英 / 标点 / 简繁 / 方案 / 部署 / 同步 / 用户文件夹 / 偏好"等 8-12 个最常用入口。
  - 面板在所有虚拟桌面可见，可拖动；ESC 关闭。

### US3 · 改快捷键不直接编辑 yaml [P1, MVP]

- **Why P1**：解决原始需求 1 的核心痛点。
- **Independent test**：打开设置 UI → 快捷键页 → 修改"翻页下一页"为 `/` → 保存 → 立即生效。
- **Acceptance**：
  - Given 用户在 Fluxing 设置 UI
  - When 进入"快捷键"页，列表显示当前 default.yaml 的所有 `key_binder.bindings` 项
  - Then 用户可点任一项弹出"按键录制"小窗（按下任意键/组合自动记录为 `mods+key`），保存后立即写入 `default.yaml` 的对应位置，下次打字生效；UI 同时显示"是否与系统快捷键冲突"的检测结果。

### US4 · 候选字右键一键删除（仅用户词典）[P1, MVP]

- **Why P1**：解决"想删掉的词总是优先出现"的痛点。
- **Independent test**：候选中出现"测试短语"（用户词典），右键它，从候选窗消失。
- **Acceptance**：
  - Given 候选窗列出 5 个候选
  - When 用户右键点击其中第 3 候选 "测试短语"（来源 = 用户词典，长度 ≥ 2 字节）
  - Then 该候选立即从候选窗消失，**且**从 `%LocalAppData%\Fluxing\user.db` 中删除对应词条；不弹任何菜单、确认、toast。
  - When 用户右键点击第 1 候选（单字，或来源 = 共享词典）
  - Then 该候选"屏蔽"——加入 `%LocalAppData%\Fluxing\<schema_id>.user_ignore.txt`（一行一条），下次加载时该候选不出现在候选窗；不弹菜单、确认、toast。

### US5 · 常用短语独立于用户词典 [P1, MVP]

- **Why P1**：常用短语与动态用户词典是两类不同心智模型。
- **Independent test**：`Alt+K` 弹列表，选 3 短语按 Enter 上屏。
- **Acceptance**：
  - Given Fluxing 正在运行
  - When 用户按 `Alt+K` 全局热键
  - Then 弹出 mac 风列表 UI，分页显示所有常用短语（默认 50 条/页，支持上下箭头翻页、`PgUp/PgDn` 跳页、输入拼音首字母过滤、`Enter` 上屏高亮项、`Esc` 关闭），列表底部有"+ 新增"和"✎ 编辑"两个按钮。
  - 短语数据保存至 `%LocalAppData%\Fluxing\phrases.json`（与用户词典 `user.db` 物理隔离）。

### US6 · 暗色主题跟随系统 [P1, MVP]

- **Why P1**：mac 风的另一半 = 暗色；现代用户基本都开暗色。
- **Independent test**：把 Windows 切到暗色，候选面板 200ms 内变暗。
- **Acceptance**：
  - Given Fluxing v2.0.0 运行中
  - When 用户在系统设置里把"个性化 → 颜色 → 模式"从"浅色"切到"深色"
  - Then 候选面板、托盘快速设置面板、所有 mac 风 UI 在 200ms 内（渐变过渡）切换为暗色；不重启服务、不丢当前输入。

### US7 · 跨设备云同步（v2 P2，仅设计）[P2]

- **Why P2**：原始需求 2；规模大、风险高，需要单独 spec 走流程。
- **Independent test**：本 spec 不实施；只交付 spec 010 详细设计。
- **Acceptance**：
  - spec 010 的 FR / SC 覆盖：注册、登录、Passkey、同步对象（用户词典/常用短语/自定义方案）、冲突策略、隐私边界、网络失败降级。

### US8-US12 · 其他 5 项建议

- 详细 spec 留到 007 / 008 / 009 / 011。

## 4. 跨子 spec 依赖图

```
                ┌──────────────────────────────────────┐
                │ 004 (本 spec) · 产品愿景、范围、依赖  │
                └──────────────┬───────────────────────┘
                               │
            ┌─────────┬────────┼────────┬──────────┐
            │         │        │        │          │
            ▼         ▼        ▼        ▼          ▼
        ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐  ┌──────────┐
        │ 005  │ │ 006  │ │ 007  │ │ 008  │  │ 010      │
        │ 快捷键│ │ 托盘  │ │ 配置UI│ │ 候选删│  │ 云同步   │
        └───┬──┘ └───┬──┘ └───┬──┘ └───┬──┘  │ (P2 设计)│
            │        │        │        │     └────┬─────┘
            │        ▼        ▼        ▼          │
            │     ┌──────┐ ┌──────┐ ┌──────┐      │
            │     │ 009  │ │ 011  │ │ ...  │      │
            │     │ 常用短│ │引导器│ │      │      │
            │     │ 语   │ │      │ │      │      │
            │     └──────┘ └──────┘ └──────┘      │
            │                                     │
            └──── F1 全部 ─────────────────────────┘
                  (F1 之外的子 spec 互不阻塞)
```

**P1 实施顺序（按依赖最少到最多）**：
1. **005** default-hotkeys（无依赖；纯 yaml 改）
2. **008** candidate-edit（依赖 005 的 `Shift_L/Shift_R` 行为，但可独立 ship）
3. **006** tray-quick-settings（无代码依赖；可独立 ship）
4. **009** personal-shortcuts（依赖 006 的 mac 风组件库，但可独立 ship）
5. **007** yaml-config-ui（依赖 006 + 009 的面板组件库）
6. **暗色主题**（F11）作为 006/007/008/009 的横切关注点，统一在 spec 004 末尾收口

**P2 顺序**：F7 / F10 / F9 / F12 在 P1 全部 ship 后再做。

**P3**：F6 云同步、spec 011 bootstrapper。

## 5. 全局约束（对所有子 spec 生效）

| 约束 | 值 |
|---|---|
| UI 风格 | mac 风（半透明磨砂、12px 圆角、SF Pro / PingFang SC、3px 系统焦点环、200ms 渐变） |
| UI 资源 | 零新增运行时；与 WeaselUI 候选面板同栈；不引入 .NET / WPF |
| UI 字体 | 中文 PingFang SC（mac）/ Microsoft YaHei UI（win）；英文 SF Pro / Segoe UI Variable；emoji Apple Color Emoji / Segoe UI Emoji |
| 数据目录 | 用户词典 `user.db` / 常用短语 `phrases.json` / 屏蔽 `<schema_id>.user_ignore.txt` / 同步配置 `sync.json` 全部位于 `%LocalAppData%\Fluxing` |
| 安装目录 | `%ProgramFiles%\Fluxing\weasel`（不增加版本子目录，便于覆盖更新） |
| 配置文件 | `default.yaml`（全局）、`weasel.yaml`（样式）、`<schema>.schema.yaml`（方案）、`phrases.json`（新增）、`sync.json`（新增） |
| IPC | 沿用命名管道（`WeaselIPC`）；新增自定义命令时**只加 action 字段值，不改协议格式**（向前兼容） |
| 线程模型 | TSF 主线程处理按键；自绘渲染在 UI 线程；RIME 引擎 API 在 `RimeWithWeaselHandler` 线程；新增功能不得跨线程共享可变状态 |
| 日志 | 沿用 `DLOG` 宏 + `%LocalAppData%\Fluxing\weasel.log` |
| 国际化 | UI 字符串全部走 `WeaselServer.rc` 字符串表（zh-CN / zh-TW / en 三语，与现有对齐） |
| 测试 | TDD where feasible（RIME 引擎 API 调用有 `test/` 单测覆盖；UI 行为记录手动验证步骤） |
| 性能 | 候选面板 P95 渲染 ≤ 16ms（60 FPS）；托盘面板启动 ≤ 100ms；快捷键生效 ≤ 50ms（按键→上屏） |
| 内存 | v2 全部功能加完后，常驻内存 ≤ 80MB（当前约 35MB） |
| 隐私 | 不上传输入历史；不上传用户配置文件中的"上次输入内容"等字段 |

## 6. 子 spec 索引

| 子 spec | 对应需求 | 状态 | 文件 |
|---|---|---|---|
| 005 default-hotkeys-rev2 | F1 | P1 实施 | `005-default-hotkeys-rev2/design.md` |
| 006 tray-quick-settings | F2, F11(横切) | P1 实施 | `006-tray-quick-settings/design.md` |
| 007 yaml-config-ui | F3, F7, F11(横切) | P1 实施 | `007-yaml-config-ui/design.md` |
| 008 candidate-edit | F4 | P1 实施 | `008-candidate-edit/design.md` |
| 009 personal-shortcuts | F5, F12, F11(横切) | P1 实施 | `009-personal-shortcuts/design.md` |
| 010 cloud-sync | F6 | P2 仅设计 | `010-cloud-sync/design.md` |
| 011 fluxing-bootstrapper | 安装/卸载现代化 | P3 仅设计 | `011-fluxing-bootstrapper/design.md` |

## 7. 验收（v2 整体）

- **SC-001**：Fluxing v2.0.0 安装包 ≤ 25MB（当前 0.17.5.0 = 44.65MB；目标 = 25MB，因不引入 .NET）。
- **SC-002**：v2 全部 P1 功能开启后，常驻内存 ≤ 80MB。
- **SC-003**：候选面板 60 FPS 稳定（P95 渲染时间 ≤ 16ms）。
- **SC-004**：暗色模式切换在 200ms 内完成渐变过渡，无闪烁。
- **SC-005**：跨子 spec 整合后无 yaml 冲突（用 `rime_deployer --debug` 验证 deploy 通过）。
- **SC-006**：所有新增 UI 元素在 Windows 8.1 / 10 / 11 三平台视觉一致（DPI 100% / 150% / 200% 各验证一次）。

## 8. 风险登记

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| 自绘 mac 风工作量大、影响 005/008 早期 ship | 中 | 中 | 005/008 不依赖面板 UI 库，可先 ship；mac 风组件库推迟到 006 之前完成 |
| 暗色渐变与候选面板高 DPI 缩放冲突 | 中 | 中 | spec 006 单独列任务验证 Win10/11 + DPI 100/150/200 |
| 用户词典删除误触（无确认） | 低 | 高 | US4 明确"无确认无 toast"；但保留 `phrases.json` 与 `user.db` 的"上次备份"，供用户手动还原（在 US2 托盘面板加"恢复"按钮） |
| RIME 引擎 1.13 `user_ignore` 钩子不存在 | 中 | 中 | spec 008 给出"librime customization 钩子 + schema patch"双方案；钩子不存在时走 schema patch 路线 |
| 云同步需求范围扩张（用户想要"输入历史同步"） | 中 | 中 | spec 004 明确"v2 同步范围不含输入历史"；spec 010 单独评估 |
| mac 风格在 Windows 上违反 MS 风格指南 → 用户被投诉"非原生" | 低 | 低 | 在 settings UI 加"主题模式"开关（mac 风 / 原生 Win11 风），默认 mac 风，可切 |