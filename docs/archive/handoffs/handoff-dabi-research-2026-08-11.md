# Handoff: Dabi 浏览器/侧边栏功能逆向分析

**日期**: 2026-08-11  
**状态**: 阶段一完成 — 架构理解 + 代码提取，待开新项目深入  
**分支**: Fluxing (不影响 rime_claude 本身)  
**相关性**: cc-haha 功能增强可行性评估的前置研究

---

## 1. 任务背景

用户想理解 Dabi（安装路径: `D:\Program Files\dabi`）中以下功能的实现原理：
- 左侧导航「桌面」中打开完整浏览器
- 浏览器作为侧边栏（sidebar）嵌入到 AI 对话界面

目标：评估是否可将此功能移植到 cc-haha。

## 2. 已完成工作

### 2.1 安装目录结构梳理

Dabi 是 Electron 应用，核心文件:

| 路径 | 说明 |
|---|---|
| `dabi.exe` (232MB) | Electron 主可执行文件 |
| `resources/app.asar` (21MB) | Dabi 前端 + Electron 主进程代码 |
| `resources/openclaw.asar` (230MB) | OpenClaw 框架 (含所有 node_modules) |
| `resources/browser-config/default.json` | 浏览器站点配置 (淘宝商家域名、登录域等) |
| `resources/browser-extensions/dabi-helper/` | Chrome 扩展: 达笔助手 (图片处理) |
| `resources/browser-extensions/frame-bridge/` | Chrome 扩展: 跨域页面桥 (alimama 同源请求代理) |
| `resources/openclaw-plugins-builtin/openclaw-plugin-dabi/` | Dabi 内置插件 (淘宝上架工具 + webSearch) |
| `resources/openclaw-plugins/` | 外部插件 (dingtalk, lark, weixin, wecom) |

### 2.2 app.asar 已完整提取

位置: `/tmp/dabi-app-extract/`

关键子目录:
```
dist-electron/electron/
├── main.js         (3.1MB, bundled 主进程代码)
├── preload.js      (45KB, renderer 预加载脚本)
└── browser/
    ├── preload/
    │   ├── browser-tab.js       (标签页 preload: IPC + sessionStorage 持久化)
    │   ├── browser-toolbar.js   (工具栏 preload)
    │   ├── browser-popover.js   (弹出面板 preload)
    │   └── browser-group-panel.js (分组面板 preload)
    ├── sidepanel-platform-runtime/ (exceljs, spreadsheet, tesseract 运行时)
    ├── session/serialization/
    └── automation/markdown-runtime.js

dist/renderer/
├── index.html       (React SPA 入口)
├── assets/
│   ├── index-CnIAF8Gz.js     (主 bundle, 1.1MB)
│   ├── LeftRailView-*.js     (左侧导航组件, 11KB)
│   ├── client-*.js           (IPC 客户端/API 定义)
│   └── ipc-contract-*.js     (文件类型/MIME 注册表)
├── browser-*.html            (浏览器功能 HTML 页面)
└── ...
```

### 2.3 核心架构发现

#### 三层 BrowserPanel 架构

Dabi 主进程创建了 **3 个 BrowserPanel 实例**:

```javascript
// main.js 中的实例化逻辑
browserPanel = new BrowserPanel(..., "user")    // 桌面浏览器
agentBrowser = new BrowserPanel(..., "agent")   // AI 对话嵌入浏览器
canvasBrowser = new BrowserPanel(..., "canvas") // 侧边栏 Dock 浏览器

browserRegistry = new BrowserRegistry(browserPanel, agentBrowser, canvasBrowser)
```

每个 BrowserPanel 负责:
- WebContentsView 标签页池管理
- 标签页 freeze/lifecycle (SWEEP_INTERVAL_MS)
- 历史记录 (historySink)
- 密码管理 (passwordStore)
- 书签管理 (bookmarkStore)
- detach/merge (独立窗口 ↔ 主窗口)
- 商户多账号模式切换

#### 浏览器侧边栏 (Canvas Dock) 机制

```
渲染进程                   主进程
─────────                ─────────
browser:canvasDockOpen ──→ canvasBrowser.open()
browser:canvasDockHide ──→ canvasBrowser.hide()
browser:canvasDockClose ─→ canvasBrowser.closeAllTabsAndPanel()
browser:openCanvasConversation(sessionKey, url) ──→ canvasBrowser.openCanvasConversation()
browser:closeCanvasConversation(sessionKey)       ──→ canvasBrowser.closeCanvasConversation()
```

物理嵌入方式:
```javascript
// WebContentsView 添加到主窗口的 contentView
host.contentView.addChildView(view)
view.setBounds({x, y, width, height})  // 定位在侧边栏区域
view.setVisible(true/false)            // 显示/隐藏切换
```

#### 关键 IPC 通道清单

| 通道 | 功能 |
|---|---|
| `browser:canvasDockOpen/Hide/Close` | 侧边栏开关 |
| `browser:openCanvasConversation` | 侧边栏中打开 URL |
| `browser:closeCanvasConversation` | 关闭侧边栏对话 |
| `browser:cmd:detach` | 标签分离为独立窗口 |
| `browser:cmd:merge` | 独立窗口合并 |
| `browser:cmd:group` | 标签分组 |
| `browser:cmd:open-group-panel` | 打开分组面板 |
| `browser:getState` | 获取浏览器状态 (variant: user/agent/canvas) |
| `browser:setPanelVisible` | 面板可见性控制 |
| `browser:activateAgentConversation` | 激活 Agent 对话浏览器 |
| `group-panel:action/data/resize` | 分组面板通信 |
| `popover:action/data/resize` | 弹出面板通信 |

#### 左侧导航 (LeftRailView)

从 `LeftRailView-*.js` 提取的导航项定义:
```javascript
const navItems = [
  {id:"browser", label:"桌面", Icon: TvMinimalIcon},
  {id:"chat",    label:"AI",   Icon: AstroidIcon},
  {id:"files",   label:"文件", Icon: FolderClosedIcon},
  {id:"skills",  label:"技能", Icon: BookOpenTextIcon},
  {id:"tasks",   label:"任务", Icon: AlarmClockCheckIcon},
]
```

`activeScope` 状态决定当前显示哪个面板。

#### 浏览器扩展

1. **dabi-helper** (Manifest V3): 图片复制/替换，跨电商平台 (淘宝/天猫/1688/JD/Pinterest)
2. **frame-bridge**: 跨域 iframe 桥接，通过 `postMessage` + 白名单策略代理同源请求 (alimama ↔ sycm.taobao.com)

### 2.4 cc-haha 现状

cc-haha 是 Claude Code 的桌面 UI 插件:
- 位置: `~/.claude/cc-haha/`
- 配置: `desktop-ui.json` (schemaVersion 4, sidebar/pet/profile)
- 来源: `github.com/NanmiCoder/cc-haha`
- **不是独立 Electron 应用**，没有主进程权限
- 无法直接获得 WebContentsView / contentView.addChildView 能力

## 3. 改动文件清单

| 文件 | 操作 | 说明 |
|---|---|---|
| `/tmp/dabi-app-extract/` | 新建 | app.asar 完整提取 |
| (无 rime_claude 项目文件改动) | — | 纯研究工作 |

## 4. 下一步必交付

1. **新项目初始化**: 为 Dabi 研究创建独立 repo/项目目录
2. **openclaw.asar 提取**: 230MB 的 OpenClaw 框架尚未提取分析
3. **BrowserPanel 源码级还原**: 从 minified main.js 中还原 BrowserPanel 类的关键方法
4. **cc-haha 桌面源码定位**: 找到 cc-haha 的渲染层代码 (React 组件注入方式)
5. **可行性路线图**: 明确 cc-haha 要获得浏览器侧边栏能力需要的改造

## 5. 环境/基线

- **分支**: Fluxing (无代码改动)
- **Dabi 版本**: 基于 OpenClaw >=2026.6.1
- **asar 提取工具**: `npx asar` (在 PATH 中)
- **asar 提取输出**: `/tmp/dabi-app-extract/` (Linux tmp, 重启可能丢失)

## 6. 避坑提示

- app.asar 的 `extract-file` 子命令有 bug (路径解析问题)，用 `extract` 全量解压
- main.js 是单文件 3.1MB bundle，类名被混淆，需以方法签名 + IPC channel 字符串定位
- cc-haha 不是 npm 包 (npm list -g 为空)，而是 `~/.claude/` 下的配置文件 + 可能的 hook 集成
- cc-haha 的渲染层代码可能在 Claude Code 桌面客户端的闭源部分中

## 7. 必读顺序

1. 本文件 (handoff)
2. `/tmp/dabi-app-extract/dist-electron/electron/main.js` — 搜索 "BrowserPanel", "canvasBrowser"
3. `/tmp/dabi-app-extract/dist/renderer/assets/index.html` — 前端入口
4. cc-haha GitHub 仓库: `github.com/NanmiCoder/cc-haha` — 了解插件架构
