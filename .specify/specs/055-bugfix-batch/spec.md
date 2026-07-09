# 055 - Bugfix Batch: QuickPanel v4 + 中文输入 3 用户报告问题

> **Hotfix spec**。修复 3 个用户实测发现的严重 bug。这些 bug 在 0.18.30.0 ship 后被报告,必须 hotfix 在 0.18.34.0 之前。

## 0. 触发

2026-07-09,用户实测 0.18.30.0 报告 3 个问题:

| Bug | 现象 | 严重度 |
|---|---|---|
| 1 | 切到火流猩输入法,候选窗完全不出现,无法输入中文 | 🔴 阻断 |
| 2 | 即使没切到火流猩输入法,快捷设置栏也总显示在右下角 | 🟡 UX |
| 3 | 快捷设置栏外观与设计稿不符,有深色难看边框,甚至完全不显示 | 🟡 UX |

## 1. Bug 2 根因(已定位,代码证据齐全)

### 1.1 现象

WeaselServer 启动后,**无论用户是否切到火流猩输入法,QuickPanelDialog 立即在屏幕右下角弹出**,20% 透明度(179 alpha)长显。

### 1.2 根因(spec 052 实施偏差)

**文件**: `WeaselServer/WeaselServerApp.cpp:39-41`
```cpp
tray_icon.Create(m_server.GetHWnd());
tray_icon.Refresh();

// spec 052: auto-show QuickPanel in always-show mode on service start
QuickPanelDialog::EnableAlwaysShowMode();   // ← BUG

int ret = m_server.Run();
```

**问题**: spec 052 的"always-show mode"原本设计是"切到火流猩输入法时显示",但 codex 把它实现成"**WeaselServer 启动即长显**"——完全没监听 `OnFocusIn` / TSF 激活事件。

**根因代码**(spec 049+052 commit `6815aad7`):

```cpp
// QuickPanelDialog.cpp:412 EnableAlwaysShowMode()
void QuickPanelDialog::EnableAlwaysShowMode() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    s_mode = Mode::kAlwaysShow;
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    StartFadeTo(s_hwnd, QP_ALPHA_DEFAULT);
    return;
  }
  // Create panel immediately, no TSF focus check
  ...
}
```

**spec 052 用户原始需求**:
> US052-A: 切到火流猩输入法 → 屏幕右下角自动出现 5 行 8 入口面板,20% 透明度
> US052-B: 鼠标悬停面板 → 立即变 100% 不透明

**codex 实施偏差**: 启动 `WeaselServer` 就调 `EnableAlwaysShowMode()`,**从未**等 TSF focus 事件。

### 1.3 修复方案

**选项 A(最小修复)**: 删掉 `WeaselServerApp.cpp:40` 的 `EnableAlwaysShowMode()` 调用,改为监听 TSF `OnFocusIn` / `OnFocusOut`。

**选项 B(完整 spec 052 实施)**: 实施 spec 052 真正定义的行为:
- `WeaselServerApp` 注册 TSF 焦点回调(`m_handler->OnSessionChange` 或新 IPC)
- 当 session 从其他 IME 切到火流猩时 → `EnableAlwaysShowMode()`
- 当 session 从火流猩切走时 → `Hide()`
- 不再"启动即长显"

**推荐选项 B**(完整 spec 052,因为 spec 052 的 v0.18.31.0 ship 标签已存在)。

### 1.4 修改文件清单

| 文件 | 修改 |
|---|---|
| `WeaselServer/WeaselServerApp.cpp` | 删 L40 的 `EnableAlwaysShowMode()`,改为监听 TSF 焦点 |
| `WeaselServer/QuickPanelDialog.cpp` | 加 `OnFluxingActivated(bool)` / `OnFluxingDeactivated()` API |
| `WeaselServer/QuickPanelDialog.h` | 声明新方法 |
| `WeaselTSF/WeaselTSF.cpp` | TSF focus 事件转发 IPC 消息 |
| `include/WeaselIPC.h` | 加 `WEASEL_IPC_IME_ACTIVATED` / `WEASEL_IPC_IME_DEACTIVATED` 命令 |
| `WeaselIPCServer/WeaselServerImpl.cpp` | 处理新 IPC 命令 |

## 2. Bug 3 根因(已定位)

### 2.1 现象

用户反馈:"快捷设置栏外观与设计稿不符,有深色难看边框,甚至完全不显示"

### 2.2 根因(QuickPanelDialog.cpp DoPaint 代码证据)

**文件**: `WeaselServer/QuickPanelDialog.cpp:258-325` (DoPaint 函数)

**问题点 1 - 硬编码深色边框**:

```cpp
// Line 277-278
DrawRoundRect(&g, bar_rc, 14.0f,
              Gdiplus::Color(0xF0, 0xF6, 0xF6),    // 浅灰背景
              Gdiplus::Color(0x14, 0x14, 0x14));     // ← 深色边框(ARGB alpha=0x14 = 20)
```

`0x14, 0x14, 0x14` = alpha 0x14 (20/255 = 8%) + R=G=B=0x14 (20/255)。**这是个浅到几乎透明的黑色边框**,但在白色背景上看起来像"深色难看边框"。

**设计稿要求**(spec 049 §1.4):
- 14px 圆角卡片 + 7px 圆角按钮
- `rgba(10,132,255,0.12)` 激活态透明蓝
- **未明确边框**——设计稿意图是"无明显边框,毛玻璃融入背景"

**问题点 2 - 毛玻璃未实现**:

```cpp
// Line 270-272
SolidBrush bg(Gdiplus::Color(0xF0, 0xF6, 0xF6));  // ← 不透明浅灰
g.FillRectangle(&bg, 0, 0, crc.right, crc.bottom);
```

`SolidBrush + 不透明 alpha 0xF0` 完全不是毛玻璃。spec 049 §1.2 要求 GDI+ `ImageAttributes + ColorMatrix` 实现 4% alpha 半透明叠加。

**问题点 3 - 图标用 GDI+ DrawLine/FillEllipse 手绘,不是 SF Symbols SVG**:

```cpp
// Line 213-217 Btn 1 (方案):
g->DrawLine(&pen, ...);   // 3 条横线
g->DrawLine(&pen, ...);
g->DrawLine(&pen, ...);
```

spec 049 §2.2 要求 SF Symbols 风格图标,实际是 GDI+ `DrawLine` 临时画的简单图形。

**问题点 4 - 用户反馈"完全不显示了"**:

最可能是 0.18.30.0 → 0.18.31.x → 0.18.32.0 → 0.18.33.0 中某个 hotfix 的 GDI+ 渲染 crash(参考 L48: D2D/GDI+ 渲染器 atexit crash,需要 ExitProcess),导致 ShowWindow 后立刻 crash 或黑屏。

### 2.3 修复方案

**P1 最小修复**(1-2 小时):
- 删边框(L278 改为 alpha=0)
- 改背景为 `Color(0xFA, 0xFA, 0xFA)` 极浅灰(alpha 250)
- 加 GDI+ `ImageAttributes::SetColorMatrix` 做 8% 半透明
- 保留 GDI+ DrawLine 图标(降级方案)

**P2 完整重写**(4-6 小时):
- 重写 QuickPanelDialog.cpp 用真 SVG path(`Gdiplus::GraphicsPath::AddPath` 加载 SVG)
- 真毛玻璃背景(`Graphics::DrawImage` + 半透明 PNG mask)
- 完整对照 spec 049 §1.3 验收

**推荐 P1**(因为 P2 需要外部 SVG 资源,需要先准备 SVG icon 6 个)。

### 2.4 修改文件清单

| 文件 | 修改 |
|---|---|
| `WeaselServer/QuickPanelDialog.cpp` | DoPaint 重写:无边框 + 半透明背景 + 清晰图标 |
| `WeaselServer/QuickPanelDialog.h` | 可能加 GDI+ ColorMatrix helper |
| `WeaselServer/WeaselServer.rc` | (P2 才需要) 加 6 个 SVG icon 资源 |
| `docs/design/04-quick-settings-v3-macos.html` | 更新为实施版 |

## 3. Bug 1 根因(待用户实测验证)

### 3.1 现象

"切到火流猩输入法,候选窗完全不出现,无任何响应"

### 3.2 可能根因(按概率排序)

**假设 A**: TSF IPC 链路断开
- `WeaselTSF::ActivateEx` 调 `m_client.Connect()` 失败
- 或 `m_client.ProcessKeyEvent` 返回 0 (无响应)
- 排查命令: `Get-CimInstance Win32_Process -Filter "Name='WeaselServer.exe'"` 看进程是否在跑
- 日志: `%TEMP%\rime.weasel\rime.log`

**假设 B**: WeaselServer 进程未启动 / 被杀
- L48 D2D/GDI+ atexit crash 可能让 WeaselServer 启动后立即 crash
- 用户看不到托盘图标就以为没启动
- 排查命令: 任务管理器 → 详细信息 → 找 WeaselServer.exe

**假设 C**: rime_api 初始化失败
- schema yaml 加载失败(rime_ice 依赖 lua,0.18.30.0 之前 L10 修过)
- 用户数据目录权限问题
- 排查命令: 看 `%TEMP%\rime.weasel\rime.log` 找 init 错误

**假设 D**: `RimeWithWeaselHandler::Initialize` 失败
- spec 053 R6 fix (`b6d28ab4`) 加了 "abandoned mutex detection + lazy recovery",可能引入新 bug
- 排查命令: 看 rime.log 里 `[Fluxing]` 日志

**假设 E**: 安装路径错(用户实测了 v0.18.33.0 而不是 0.18.30.0)
- L58 iron rule 设默认 `D:\Program Files\fluxing`
- 如果用户装在 `C:\Program Files\Fluxing` 旧路径,可能有混合 arch 问题(L14)

### 3.3 修复方案(无法定位,需要用户实测)

**步骤 1**: 给用户提供 5 个诊断命令,让他跑后回报输出

```powershell
# 1. 检查 WeaselServer 是否在跑
Get-CimInstance Win32_Process -Filter "Name='WeaselServer.exe'" | Select-Object ProcessId, ExecutablePath

# 2. 检查 rime.log 最后 50 行
Get-Content "$env:TEMP\rime.weasel\rime.log" -Tail 50 -Encoding UTF8

# 3. 检查 HKLM InstallDir
Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue

# 4. 检查 HKCU RimeUserDir
Get-ItemProperty "HKCU:\Software\Fluxing\Weasel" -ErrorAction SilentlyContinue

# 5. 检查 weasel.dll 是否加载
Test-Path "D:\Program Files\fluxing\weasel\weasel.dll"
```

**步骤 2**: 根据用户输出,定位到具体根因(A/B/C/D/E)

**步骤 3**: 写针对性 hotfix

### 3.4 修改文件清单(待根因明确)

- A 假设:`WeaselTSF/WeaselTSF.cpp` 加 `_EnsureServerConnected` 详细日志
- B 假设:`WeaselServer/WeaselServerApp.cpp` 加崩溃 watchdog
- C 假设:`RimeWithWeasel/RimeWithWeasel.cpp` 加 rime_api 初始化失败处理
- D 假设:`WeaselServer/WeaselServerApp.cpp` 检查 spec 053 R6 改动
- E 假设:重装到正确路径

## 4. 验收

### 4.1 Bug 1 验收(等用户实测)

- 切到火流猩输入法
- 按拼音键,候选窗出现
- 候选词正常显示
- 上屏正常

### 4.2 Bug 2 验收

- 启动 WeaselServer,**QuickPanel 不立即出现**
- 切到火流猩输入法 → QuickPanel 出现(20% alpha)
- 切走(火流猩→其他 IME) → QuickPanel 消失
- Alt+, → 手动切换显示/隐藏

### 4.3 Bug 3 验收

- QuickPanel 背景半透明(肉眼可见)
- 边框不可见(透明或 1px 极浅)
- 6 个图标清晰可见(不是临时画的几条线)
- 144 DPI 下文字清晰
- 暗色主题下暗背景

## 5. 任务分解

### P0 (必做,1-2 天)

- T001: 跑 L46 3-path gate (基线)
  - `xbuild.bat weasel installer` exit 0
  - `msbuild weasel.sln` exit 0
  - `scripts\test-infra\run-test-suite.bat` 16/16 PASS

- T002: Bug 2 修复(选项 B)
  - 删 `WeaselServerApp.cpp:40` EnableAlwaysShowMode
  - 加 `OnFluxingActivated/Deactivated` API
  - WeaselTSF IPC 转发
  - 测试: 用户实测切 IME 验证显示/隐藏

- T003: Bug 3 修复(P1)
  - 删硬编码深色边框
  - 半透明背景
  - 重画 6 个图标(清晰版)
  - 测试: 用户截图验证外观

### P1 (做前先问用户)

- T004: Bug 1 调查(用户提供诊断命令输出)
  - 看 rime.log / 进程状态 / 注册表
  - 根据输出定位根因
  - 写针对性修复

### P2 (后续)

- T005: 升级到 spec 049 v4 完整实施(6 SVG icons + 毛玻璃)
- T006: 加 regression tests for these 3 bugs

## 6. 依赖

- spec 054 cleanup-baseline (已 ship) - 工作区干净基线
- L48: link-probe test exit mode(`ExitProcess(rc)`)
- L52: D2D DPI handling (P1 fallback)
- L42: byte-verify for dark-mode palette

## 7. 风险

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| Bug 2 修复引入 TSF focus 监听 bug | 中 | 中 | spec 052 已 ship 测试,加新 IPC 命令测试 |
| Bug 3 P1 修复让用户失望 | 中 | 低 | 完整 v4 是 P2,后续 spec 049 实施 |
| Bug 1 根因找不到 | 中 | 高 | 必须用户实测提供诊断命令输出 |
| 0.18.34.0 ship 时间压力 | 高 | 中 | P0 优先,P1 排下个版本 |

## 8. 状态

- 2026-07-09: spec 创建,3 bug 调查完成(2 已定位,1 待用户实测)
- 等待用户跑诊断命令(Bug 1)
- P0 (Bug 2 + Bug 3) 立即可开始

## 9. 关联文档

- [spec 049 - QuickPanelDialog v4 macOS 设计](049-quickpanel-v4-macos/spec.md)
- [spec 052 - QuickPanel 长显模式](052-quickpanel-always-show/spec.md)
- [AGENTS.md §4.1 NSIS 危险区](AGENTS.md)
- [lessons-learned.md L48/L49/L52](memory/lessons-learned.md)