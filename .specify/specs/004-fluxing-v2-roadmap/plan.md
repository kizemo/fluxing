# 004 · Plan · 火流猩输入法 v2 路线图

## Constitution Check

| Rule | Status | Notes |
|---|---|---|
| R1 intent + acceptance | OK | spec.md US1-US12 全部带"独立可测"接受条件。 |
| R2 spec vs plan | OK | spec.md 0 处出现具体技术词（仅"与现有面板同栈"概述）；本 plan 才出现技术词。 |
| R3 priority | OK | US1-US6/US11 = P1；US7 + 4 个建议 = P2；spec 010/011 = P3 仅设计。 |
| R4 Constitution Check | OK | 此表。 |
| R5 task granularity | OK | 每条 Txxx < 4h，1-3 文件。 |
| R6 done = evidence | OK | v2 整体 release 时需附：installer sha1、msbuild 日志、UI 截图三平台。 |
| R7 one source of truth | OK | spec/plan/tasks 全在 `.specify/specs/004-fluxing-v2-roadmap/`。 |
| R8 specs versioned | OK | 本 spec 与 005-011 子 spec 一起 commit。 |
| R9 lookup beats memory | OK | 已在 handoff 阶段扫过 `docs/Fluxing-code-map` 全部六份、spec 003 全部三件套。 |

## Architecture

### 进程与组件（v2 增量）

```
┌──────────────────────────────────────────────────────────────────────┐
│ Windows                                                              │
│                                                                      │
│  ┌──────────┐  TSF  ┌─────────────────────────┐                       │
│  │ Focus App├──────►│ WeaselTSF (weaselx64.dll)│  (不改动)             │
│  └──────────┘       └──────────┬──────────────┘                       │
│                                │ 命名管道                              │
│                                ▼                                     │
│                    ┌──────────────────────────────┐                  │
│                    │ WeaselServer (FluxingServer)  │                  │
│                    │  - RimeWithWeaselHandler       │                  │
│                    │  - WeaselTrayIcon (+ 合成点)   │                  │
│                    │  - FluxingPanelHost (新)       │◄───── Alt+,/托盘  │
│                    │  - FluxingShortcutRecorder(新)│                  │
│                    │  - FluxingCandidateEdit (新)  │                  │
│                    └──────┬───────────────────────┘                  │
│                           │ RIME 引擎 C API                            │
│                           ▼                                            │
│                    ┌──────────────────────────────┐                  │
│                    │ RIME 引擎 (rime_api)           │                  │
│                    │  + customization 钩子 (P1)     │                  │
│                    └──────────┬───────────────────┘                  │
│                               ▼                                        │
│                    ┌──────────────────────────────┐                  │
│                    │ %LocalAppData%\Fluxing         │                  │
│                    │  - user.db                     │                  │
│                    │  - phrases.json   (新)          │                  │
│                    │  - sync.json      (新, P2)     │                  │
│                    │  - <schema>.user_ignore.txt (新)│                 │
│                    └──────────────────────────────┘                  │
│                                                                      │
│  ┌────────────────────────┐                                           │
│  │ FluxingPanelHost (新) │  ◄── 单进程，UI 线程主循环                  │
│  │  - 自绘 mac 风        │      IPC = 命名管道 + window message       │
│  │  - 承载 quick settings │      启动 ≤ 100ms (preloaded by Server)    │
│  │  - 承载 phrases 列表   │                                           │
│  │  - 承载 shortcuts UI   │                                           │
│  └────────────────────────┘                                           │
│                                                                      │
│  ┌────────────────────────┐                                           │
│  │ Vercel (P2)            │  ◄── 跨设备同步                             │
│  │  - Next.js API         │      - Upstash Redis (KV)                  │
│  │  - Vercel Postgres     │      - WebAuthn Passkey                    │
│  └────────────────────────┘                                           │
└──────────────────────────────────────────────────────────────────────┘
```

### 关键模块（新增文件 / 改动文件）

| 模块 | 路径 | 角色 | 依赖子 spec |
|---|---|---|---|
| `FluxingPanelHost` | `FluxingPanelHost/{main,AppWindow,Theme,BlurBehind,KeyboardNav}.{h,cpp}` + `xmake.lua` | mac 风宿主进程；承载所有 mac 风窗口 | 006, 007, 009 |
| `FluxingComponents` | `FluxingComponents/{Button,List,KeyCap,TextField,Toast}.{h,cpp}` | mac 风基础控件 | 006 → 007, 009 |
| `FluxingShortcutRecorder` | `RimeWithWeasel/{ShortcutRecorder.{h,cpp}}` | 按键录制器 | 005, 007 |
| `FluxingCandidateEdit` | `RimeWithWeasel/{CandidateEdit.{h,cpp}}` | 右键删除 + 屏蔽管道 | 008 |
| `FluxingPersonalShortcuts` | `FluxingPanelHost/Phrases/{PhrasesWindow,PhrasesStore}.{h,cpp}` | 常用短语独立 UI + 存储 | 009 |
| `FluxingYamlEditor` | `FluxingPanelHost/ConfigEditor/{HotkeyPage,SchemaPage,PhrasesPage,UserDictPage}.{h,cpp}` + `YamlRoundTrip.{h,cpp}` | yaml 可视化编辑；保留注释 + key 顺序 | 007 |
| `FluxingTheme` | `FluxingComponents/Theme.{h,cpp}` | 主题（亮/暗/自定义色板） | 006, 007, 008, 009 |
| `FluxingDarkModeBridge` | `RimeWithWeasel/DarkModeBridge.{h,cpp}` | `WM_SETTINGCHANGE` → 渐变 200ms 切色 | 006 |

### 现有文件改动

| 文件 | 改动 |
|---|---|
| `output/data/default.yaml` | 005 改 key_binder.bindings、ascii_composer、switcher、navigator |
| `output/data/weasel.yaml` | 004 加 `theme.light / theme.dark` 双套色板；007 暴露到 UI |
| `output/data/<schema>.schema.yaml` | 008 在方案 patch 中加 user_ignore 钩子（若 RIME 引擎不支持 customization） |
| `RimeWithWeasel/RimeWithWeaselHandler.cpp` | 加暗色钩子 + 右键消息接收 + 短语存储调用 |
| `WeaselServer/WeaselServerApp.cpp` | 启动 `FluxingPanelHost` 子进程；注册 `Alt+,`/`Alt+K` 全局热键 |
| `WeaselServer/WeaselServer.rc` | 托盘菜单"快速设置"项（点击后调 PanelHost） |
| `WeaselUI/WeaselPanel.cpp` | 008 加 WM_RBUTTONDOWN 接收；不重写 DoPaint |
| `include/WeaselConstants.h` | 加 `WEASEL_HOTKEY_TOGGLE_QUICK_SETTINGS` 等 |

## Build Order

1. 005 (default-hotkeys) → 编译 → installer → release 标记 `fluxing-0.18.0.0-installer.exe`
2. 008 (candidate-edit) → 编译 → installer → `fluxing-0.19.0.0-installer.exe`
3. 006 (tray-quick-settings) → 引入 FluxingPanelHost.exe + FluxingComponents → installer → `fluxing-0.20.0.0-installer.exe`
4. 009 (personal-shortcuts) → 依赖 006 组件库 → installer → `fluxing-0.21.0.0-installer.exe`
5. 007 (yaml-config-ui) → 依赖 006 + 009 → installer → `fluxing-0.22.0.0-installer.exe`
6. **v2.0.0 整合** = 上面 5 个增量 + 暗色主题（横切）→ `fluxing-2.0.0-installer.exe`
7. P2 (F7/F9/F10/F12) → 后续独立小版本
8. P3 (F6 云同步) → spec 010 实施时再排

## Environment

沿用 spec 003 已搭好的 `F:\b183\Boost 1.83.0` + VS 2022 BuildTools 14.44 + NSIS 3.x。

## Patches to upstream

- 不上推 RIME 引擎子模块。
- 替换 `WeaselUI/WeaselPanel.cpp` 的右键消息处理——保留原行为，新加 `OnRButtonDown(UINT, CPoint)`。
- 安装器升级到 `fluxing-2.0.0-installer.exe` 的输出（spec 011 bootstrapper 不在 v2 范围）。

## Release Directory

- `release/fluxing-2.0.0-installer.exe` (目标 ≤ 25MB)
- 子 spec 各自的中间版本：0.18.0.0、0.19.0.0、0.20.0.0、0.21.0.0、0.22.0.0

## Intermediate Release Versions

每个子 spec 单独 ship 时的中间版本号（保留主版本号 `0.1x` 走小版本递增）：

| 子 spec | 完成 release | 路径 |
|---|---|---|
| 005 | `fluxing-0.18.0.0-installer.exe` | `release/fluxing-0.18.0.0-installer.exe` |
| 008 | `fluxing-0.19.0.0-installer.exe` | `release/fluxing-0.19.0.0-installer.exe` |
| 006 | `fluxing-0.20.0.0-installer.exe` | `release/fluxing-0.20.0.0-installer.exe` |
| 009 | `fluxing-0.21.0.0-installer.exe` | `release/fluxing-0.21.0.0-installer.exe` |
| 007 | `fluxing-0.22.0.0-installer.exe` | `release/fluxing-0.22.0.0-installer.exe` |
| **v2.0.0 整合** | `fluxing-2.0.0-installer.exe` | `release/fluxing-2.0.0-installer.exe` |

每个 release 前必须：installer hash（SHA1） + msbuild 日志 + UI 截图三平台三 DPI 留底到对应子 spec 的 `evidence/` 目录。

| P2/P3 后续版本 | 文件 | 说明 |
|---|---|---|
| `fluxing-2.1.0-installer.exe` | spec 010 | 实施云同步（仅设计已落 010/design.md） |
| `fluxing-2.2.0-installer.exe` | spec 011 | 实施现代化安装/卸载/首启引导（仅设计已落 011/design.md） |

## Complexity Tracking

无违反项。spec 004 元 spec 仅做"愿景/范围/依赖/约束"，所有技术实现下沉到子 spec。