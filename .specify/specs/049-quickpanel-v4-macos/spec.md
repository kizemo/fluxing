# 049 - QuickPanelDialog v4 macOS 风格（v0.19.0.0 设计意图）

> **元 spec**。本 spec **不实施**，只把 v0.19.0.0 路线图中的 QuickPanelDialog v4 macOS 风格重写设计意图记录下来，让未来 session 接手时能直接继续。
>
> 现状：v0.18.29.0 已 ship 的 8 入口 QuickPanelDialog（spec 045）使用 D2D + FluxingComponents 控件库，是 v3 系统化设计。本 spec 提议的 v4 macOS 风格是在 v3 基础上的"视觉语言升级"，完全替代当前实现。
>
> **本 session 已完成的工作**（2026-07-07，由 cursor/rimetrae 项目经验启发）：
> - 4 个 HTML 设计稿对比（v1/v2/v3/v3-macos），v3-macos 胜出
> - 1 个新 logo 资源（`resource/fluxing-logo.png`，47KB）
> - 1 个 helper 头文件（`FluxingConfigEditor/DeployerUiHelper.h`，372 行，cursor 项目经验）
> - QuickPanelDialog.cpp v4 半成品（998 行，不编译，已 revert）
>
> **本 spec 范围**：仅文档化，不实施。设计稿 + 资源 + helper 全部保留为 future-work 资产。

## 0. 背景

### 0.1 现状（v0.18.29.0 spec 045）
- QuickPanelDialog 8 入口：标题 / 中英 toggle / 简繁 toggle / 全半角 toggle / 当前方案+切换 / 用户文件夹 / 程序文件夹 / 部署 / 退出 / 关闭 X
- 渲染：spec 037 FluxingComponents 控件库（Button/Toggle/Panel/Label + D2DRenderer + FluxingTheme）
- 视觉：方形按钮、12px 圆角、纯白底（亮色）/深灰底（暗色）、hover 实色变、120ms transition
- 大小：300×220（v0.18.27.0 之前是 300×150；v0.18.29.0 扩到 220 高）
- 触发：Alt+, 全局热键 + 左键托盘图标
- 自动关闭：1s 失焦 + ESC + 关闭 X

### 0.2 问题（设计稿 docs/design/00-index.html 整理）
- **不够 macOS**：矩形按钮 + 12px 圆角 + 实色 hover 都偏"工程硬角"
- **缺少毛玻璃**：和原项目 WeaselPanel 候选框的现代感脱节
- **缺少品牌**：纯文字标题，没有 logo
- **不显眼的状态指示**：3 个 toggle 并排时用户难以一眼看出当前是哪个状态在 active

### 0.3 替代方案对比（docs/design/）

| 版本 | 思路 | 尺寸 | 圆角 | hover | 评价 |
|---|---|---|---|---|---|
| **v1（已废弃）** | 紫蓝调 + 大 emoji + 巨 padding | 320×230 | 16px | 实色 | 占用屏幕、按钮超出 7 个 |
| **v2 契约式** | 按钮是 API 契约 | 300×36 | 5px | `#f0f0f3` | 工程味重、可视部分凭感觉 |
| **v3 设计系统** | 复用 IconButton 组件 | 288×36 | 6px | `#f0f0f3` | 复用好、视觉一般 |
| **v3-macos ⭐推荐** | v3 之上做 macOS 视觉 | 300×36 行 | 14px 卡片 + 7px 按钮 | `rgba(0,0,0,0.04)` 4% | **毛玻璃 + SF Symbols + 微变** |

### 0.4 v3-macos 关键决策
- **毛玻璃背景**：和 WeaselPanel 候选框一致，macOS 用户视觉习惯
- **14px 圆角**：macOS Big Sur+ 标志值，比 v3 的 6px 更原生
- **SF Symbols 风格**：1.5px stroke + round cap/join（细线条）
- **微变 hover**：4% 黑叠加，比实色 hover 更克制
- **激活态透明蓝**：`rgba(10,132,255,0.12)` 而非 `#e6f0ff`
- **120ms ease transition**（v3 已有，v4 保留）

## 1. 目标（v0.19.0.0 实施时）

### 1.1 视觉目标
- macOS 风格毛玻璃背景（半透明 + 背景模糊）
- 14px 圆角卡片 + 7px 圆角按钮
- 6 个图标按钮（替换当前 8 入口中的部分）：
  1. **schema** 方案选择（cascades to submenu or popup list）
  2. **dict** 用户词典
  3. **phrase** 常用短语
  4. **full-half** 全/半角 toggle
  5. **keyboard** 软键盘 / 符号
  6. **login** 登录（v2.1+ 云同步前置）
- 1 个 brand 区域：logo + 标题 "Fluxing"
- 1 个 native close X 保留

### 1.2 技术目标
- 渲染：放弃 D2D（spec 037 FluxingComponents 路线），改用 **GDI+ + ColorMatrix** 半透明
  - 理由：GDI+ 的 `Graphics::DrawImage` + `ImageAttributes` + `ColorMatrix` 可一行实现半透明叠加，比 D2D 的 `ID2D1Bitmap::SetOpacity` 简单
- 控件：6 个图标按钮用 SVG path + GDI+ `Graphics::DrawPath` 渲染（不是 Fluxing 控件）
- 主题：暗色跟随系统（spec 033 FluxingDarkModeBridge）
- 工具：复用 `FluxingConfigEditor/DeployerUiHelper.h` 的：
  - `EnableDarkTitleBar`
  - `GetDpiForWindow` / `Scale`
  - `CreateUiFont`
  - `BringDialogToFront`
  - `EnableResizableFrame`

### 1.3 验收（v0.19.0.0 实施时）
- 6 个图标按钮全部 hover 微变（4% 黑叠加）+ active 蓝色透明背景
- 毛玻璃背景在亮/暗主题下都正确（半透明 + 模糊）
- 144 DPI 下文字清晰（spec 041+ DPI 经验）
- 暗色主题 200ms 渐变（spec 037 动画经验）
- Alt+, 触发 + ESC 关闭 + 失焦 3s 自动关闭（注意 v4 是 3s，不是当前 1s）

## 2. 范围

### 2.1 改动文件（v0.19.0.0 实施时）
- `WeaselServer/QuickPanelDialog.h` - 完全重写
- `WeaselServer/QuickPanelDialog.cpp` - 完全重写（v3-macos 实现）
- `WeaselServer/WeaselServer.rc` - 加新 icon / logo 资源
- `WeaselServer/WeaselServerApp.cpp` - Show() 调用点更新
- `WeaselTSF/LanguageBar.cpp` - 左键托盘调用更新
- `WeaselTSF/WeaselTSF.rc` - popup menu 6 个 icon 入口
- `WeaselTSF/WeaselTSF.h` - 加 message ID
- `WeaselTSF/ThreadMgrEventSink.cpp` - 资源加载
- `WeaselIPCServer/WeaselServerImpl.h` - 加 6 个 IPC handler
- `WeaselIPCServer/WeaselServerImpl.cpp` - 转发 6 个新 handler
- `include/resource.h` - 新 ID 定义
- `README.md` - 文档更新
- `docs/design/04-quick-settings-v3-macos.html` - 实施时更新为 final
- `test/TestQuickPanelDialog/` - 重写测试（基于 v3-macos 6 入口）
- `test/TestQuickPanelRefactor/` - 合并到 TestQuickPanelDialog

### 2.2 不在本 spec 范围
- 任何代码改动（本 spec 是 design-only）
- 实际的 v0.19.0.0 ship 流程（要做时新建 spec 049-impl）

## 3. 风险

| 风险 | 缓解 |
|---|---|
| 6 个按钮的具体 icon 没定 | v0.19 实施前需用 `frontend-ui-engineering` 技能确定 SF Symbols 列表 |
| 毛玻璃 + GDI+ 性能 | 实测；若不够好，回退到 D2D 路线（spec 037 经验）|
| 6 入口比当前 8 入口少 | 在 v0.19 实施时通过右击展开二级菜单补全（schema 选 dict/phrase 入口）|
| DPI 144+ 下毛玻璃背景模糊计算开销 | 降低 backing store 尺寸到 logical / 1.5 缩放 |

## 4. 设计资产（已就位，本 spec 不动）

| 资产 | 路径 | 状态 |
|---|---|---|
| v1 设计稿 | `docs/design/01-quick-settings-panel.html` | 7.9KB，反例保留 |
| v2 设计稿 | `docs/design/02-quick-settings-v2-contract.html` | 6.1KB |
| v3 设计稿 | `docs/design/03-quick-settings-v3-system.html` | 8.2KB |
| v3-macos 设计稿 ⭐ | `docs/design/04-quick-settings-v3-macos.html` | 11.4KB，**实施依据** |
| 对比索引 | `docs/design/00-index.html` | 6.5KB，4 选 1 决策记录 |
| logo 资源 | `resource/fluxing-logo.png` | 47KB PNG |
| helper 头文件 | `FluxingConfigEditor/DeployerUiHelper.h` | 7.7KB / 372 行 inline，零编译开销 |
| 备份 v0.18.29.0 binary | `output/backup.0.18.29.0/` | 1.7MB weasel.dll + 1.3MB WeaselServer.exe（spec 049 in-progress 前的最后可用 binary）|
| 备份 v4 半成品 cpp/h | `output/backup.b-pre-revert/` | 12 个 in-progress 文件，v0.19 实施时可直接参考 |

## 5. 依赖

- spec 037 FluxingComponents 控件库 v0（已 ship）
- spec 041 FluxingComponents 144 DPI 视觉修复（已 ship）
- spec 033 FluxingDarkModeBridge 暗色主题（已 ship）
- cursor/rimetrae 项目的视觉参考（已沉淀到 project-knowledge.md 附录 A）

## 6. 关联

- 路线图：spec 004 §2.1 提及 v2.0 后 mac 风设置面板
- PRD：§1.2 价值主张 "mac 风组件库" 落地
- project-knowledge.md §7 QuickPanelDialog 状态（v0.18.29.0）
- lessons-learned.md L50（spec 037 ship 时未加 GDI fallback）、L51（lang bar 集成）、L52（DPI 3 次失败）、L53（PE binary verify）

## 7. 状态

- 设计阶段：**完成**（v3-macos 选定 2026-07-07）
- 资源阶段：**完成**（logo + helper + 设计稿 + 备份）
- 半成品 code：**已 revert + 备份**（output/backup.b-pre-revert/ 12 文件，v0.19 实施时可参考）
- 实施阶段：**未开始**（留待 v0.19.0.0）

## 8. 触发条件（v0.19 实施前要确认）

- [ ] 当前 v0.18.x 路线图（Phase 1 / F3-F5）全部 ship 并用户反馈稳定
- [ ] v3-macos 设计稿在 144 DPI 实机截图通过用户评审
- [ ] 6 个 icon 的 SVG path 选定（用 `frontend-ui-engineering` 技能）
- [ ] 毛玻璃 + GDI+ 在 100/150/200 DPI 都视觉正确
- [ ] 6 入口 vs 8 入口的 UX 决策（缺 dict/phrase 直接入口？放在 schema 二级？）