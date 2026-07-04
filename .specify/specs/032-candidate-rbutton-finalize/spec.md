# 032 · 火流猩输入法 v2 · 候选字编辑收尾（spec 008 T007 + T008）

> 收尾 spec 008 的最后两个 P1 任务：
> - T007: 暗色主题订阅（WeaselPanel 收 WM_SETTINGCHANGE 切换 palette）
> - T008: 托盘面板 "恢复" 按钮（清空 user_ignore.txt + Refresh）
>
> **T007 partial ship note**: spec 004 §9.4 显式要求
> "006 是 F11 的 ship 起点：006 引入 FluxingComponents/Theme + FluxingDarkModeBridge"。
> spec 006 未 ship。 本 spec 走 partial 路径：WeaselPanel 直接收 WM_SETTINGCHANGE +
> IsUserDarkMode() (已存在 helper) + 切换 palette。 spec 006 后续 ship
> FluxingDarkModeBridge 时，可能要 refactor 本 spec 的 `OnSettingChange` handler
> 把 palette 切换逻辑迁过去。 这不影响本 spec ship (P1 acceptance 仍达成：
> "切暗色 → 候选面板 200ms 内变暗" — 本 spec 路径直接满足)。
>
> **T008 full ship**: 不依赖 spec 006（user_ignore.txt 是 spec 008 stage 3 客户端
> 状态，server 端能直接调 WeaselUserDataPath 写文件）。 ID_WEASELTRAY_RESTORE_IGNORED
> 加在现有 11 项 tray menu 里，handler 清空 user_ignore.txt + Refresh。 跟 spec 006 的
> mac 风快速面板是独立 UI 入口，但行为对等（用户恢复已屏蔽候选）。

## 0. Why now (intent before implementation)

- spec 008 开了 30+ 天，4 个 phase 完成 3 个（spec 028/030/031 = stage 1/2/3）。
  剩 T007 + T008 两个 P1 任务。
- 用户在 spec 008 §1 验收里说"屏蔽的候选之后任何输入不再出现" — 没有"恢复"路径
  数据持久但无 UI 入口。 T008 补 UI 入口。
- 暗色主题是 spec 004 §9 F11 横切关注点，已 ship 入口端（IsUserDarkMode helper），
  T007 把这个入口接到 WeaselPanel Refresh palette。

## 1. Acceptance criteria

### T007 - 暗色主题订阅

- `WeaselUI/WeaselPanel.h` `BEGIN_MSG_MAP` 加 `MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)`。
- `WeaselUI/WeaselPanel.h` 加 `OnSettingChange(UINT, WPARAM, LPARAM, BOOL&)` 声明。
- `WeaselUI/WeaselPanel.cpp` 加 `OnSettingChange` 实现：
  - lParam 是 `"ImmersiveColorSet"` (Windows 10+ 暗色切换消息) → 调 `IsUserDarkMode()`
  - 调 `_RefreshStylePalette()`（新 helper，swap m_style palette pointer）
  - 调 `Refresh()` 触发重绘
- `_RefreshStylePalette()`:
  - 如果 dark → 调 `m_style.set_dark_palette()` (如果存在)；否则 fallback：调
    `m_style.back_color = dark ? 0x1E1E1E : 0xF0F0F0` 浅色/深色切换
  - **fallback** 路径使用硬编码 2 个色（spec 004 §9.3 dark palette 待 spec 006 完整定义）
- 不引入新依赖，不改 librime / WeaselTSF / RimeWithWeasel。
- 200ms 渐变过渡：spec 004 §9.3 要求，**本 spec 跳过**（spec 006 负责，本 spec 只
  做到"立即切换 palette 触发 Refresh()"）。

### T008 - 托盘面板 "恢复" 按钮

- `WeaselServer/resource.h` 加 `#define ID_WEASELTRAY_RESTORE_IGNORED 40017`。
- `WeaselServer/WeaselServer.rc` 3 个语言菜单（中文 简体/繁体/英文）各加一项
  "恢复被屏蔽的候选 (&I)" (use &I for "Ignored"，避免跟 &R Deploy 冲突)。
- `WeaselServer/WeaselServerApp.cpp` `SetupMenuHandlers()` 加：
  ```cpp
  m_server.AddMenuHandler(
      ID_WEASELTRAY_RESTORE_IGNORED,
      [this] {
        // 清空所有 schema 的 <user_ignore.txt>（保守：全 schema 扫）
        std::wstring userDir = WeaselUserDataPath().wstring();
        if (userDir.empty()) return false;
        WIN32_FIND_DATAW fd;
        std::wstring pattern = userDir + L"\\*.user_ignore.txt";
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return true;  // 没有 ignore 文件 = 成功
        do {
          DeleteFileW((userDir + L"\\" + fd.cFileName).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        // Refresh 候选窗 — 通过 m_handler (RimeWithWeaselHandler) 调 _UpdateUI
        // 但 tray 在 server 进程，需通过 IPC 客户端触发。 spec 004 §5 允许的最小
        // 动作是 WeaselServer 直接 _RefreshTrayIcon 自身。 候选窗 Refresh 留给
        // 下次输入触发。
        return true;
      });
  ```

  **注**: server 进程无法直接调 WeaselPanel (TSF 进程内)。 Refresh 候选窗需要
  IPC roundtrip（client → server: refresh 命令 → server: process → 推回所有 client）。
  spec 026 已有 TestWeaselIPC 的 AddSession/FindSession/RemoveClient 测试覆盖的
  IPC 协议。 本 spec 不扩展 IPC 协议（避免 spec 030 类似的 IPC 拓扑改动），仅
  删 ignore 文件 + 通过已有 `_UpdateUI` 路径触发（如果有 schema 焦点，会自然 refresh）。
- `client.TrayCommand(ID_WEASELTRAY_RESTORE_IGNORED)` 在 `WeaselServer.cpp` 已有
  `case ID_WEASELTRAY_*` 块里加 default → 透传到 m_handler 处理。 **或** 在
  WeaselServerApp.cpp 的 AddMenuHandler 内直接执行（不走 client）。后者更简单。

### 测试

- 新建 `test/TestPanelDarkModeSubscribe.cpp`：3 真实 assertions:
  1. 收到 `WM_SETTINGCHANGE` + `"ImmersiveColorSet"` lParam → 调 IsUserDarkMode
  2. 调 _RefreshStylePalette → 切换 m_style.back_color 到 dark (0x1E1E1E)
  3. 调 Refresh() 触发重绘（已存在）
- 新建 `test/TestTrayRestoreIgnored.cpp`：3 真实 assertions:
  1. 创建临时 user_ignore.txt + 几行
  2. 调 handler → 文件被删
  3. handler 返回 true (没有 user_dir 时也返回 true, 不报错)

`scripts/test-infra/run-test-suite.bat` → 11/11 PASS。
`scripts/test-infra/verify-test-binaries-fresh.bat` → 11/11 FRESH。

## 2. Out of scope

- 不 ship spec 006 mac 风快速设置面板（独立 spec，6h+ 工作量）。
- 不 ship FluxingDarkModeBridge（spec 006 引入）。
- 不 ship dark palette 完整定义（spec 006 引入）。
- 不 ship 200ms 渐变过渡（spec 006 引入）。
- 不扩展 IPC 协议（不走 client → server 推 refresh 路径）。
- 不做 user.db 备份（spec 006 US3，P2）。

## 3. Approach

### 3.1 T007 简化实施

```cpp
// WeaselUI/WeaselPanel.h
BEGIN_MSG_MAP(WeaselPanel)
  ...
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  ...
END_MSG_MAP()
LRESULT OnSettingChange(UINT uMsg, WPARAM wParam,
                       LPARAM lParam, BOOL& bHandled);

// WeaselUI/WeaselPanel.cpp
LRESULT WeaselPanel::OnSettingChange(UINT uMsg, WPARAM wParam,
                                     LPARAM lParam, BOOL& bHandled) {
  // Windows 10+ 暗色切换广播 WM_SETTINGCHANGE + lParam = "ImmersiveColorSet"
  if (lParam != 0) {
    const wchar_t* section = (const wchar_t*)lParam;
    if (wcscmp(section, L"ImmersiveColorSet") == 0) {
      _RefreshStylePalette();
      Refresh();
    }
  }
  bHandled = false;  // 继续 propagate 给其他 handler
  return 0;
}

void WeaselPanel::_RefreshStylePalette() {
  bool dark = IsUserDarkMode();
  // partial ship palette: 2 个硬编码色
  if (dark) {
    m_style.back_color = 0x1E1E1E;       // dark background
    m_style.text_color = 0xE0E0E0;
    m_style.hilited_candidate_back_color = 0x2D2D30;
    m_style.hilited_candidate_text_color = 0xFFFFFF;       // light text
    m_style.hilited_text_color = 0xFFFFFF;
  } else {
    m_style.back_color = 0xF0F0F0;
    m_style.text_color = 0x000000;
    m_style.hilited_candidate_back_color = 0xD0D0D0;
    m_style.hilited_candidate_text_color = 0x000080;        // light background
    m_style.text_color = 0x000000;
    m_style.hilited_text_color = 0x000080;
  }
  // 触发全局 layout 重计算 (背景色变了，padding 可能要改)
  _CreateLayout();
}
```

### 3.2 T008 简化实施

```cpp
// WeaselServer/resource.h
#define ID_WEASELTRAY_RESTORE_IGNORED 40017

// WeaselServer/WeaselServer.rc (3 个 language 块各加一项)
MENUITEM "恢复被屏蔽的候选 (&I)", ID_WEASELTRAY_RESTORE_IGNORED

// WeaselServer/WeaselServerApp.cpp
void WeaselServerApp::SetupMenuHandlers() {
  ...
  m_server.AddMenuHandler(
      ID_WEASELTRAY_RESTORE_IGNORED,
      [this] {
        std::wstring userDir = WeaselUserDataPath().wstring();
        if (userDir.empty()) return true;  // no-op
        WIN32_FIND_DATAW fd;
        std::wstring pattern = userDir + L"\\*.user_ignore.txt";
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return true;
        bool ok = true;
        do {
          std::wstring path = userDir + L"\\" + fd.cFileName;
          if (!DeleteFileW(path.c_str())) ok = false;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return ok;
      });
  ...
}
```

### 3.3 测试

test 文件用 boost lightweight_test pattern (L23/L25 模板)，不链接 WeaselPanel.cpp
/ WeaselServer.cpp (避免 WTL/ATL + tray icon 依赖)。 抽 free function 测核心逻辑。

## 4. Risks

- **R1**: T007 简化 palette 跟 spec 004 §9.3 完整 dark palette 不一致。 spec 006
  ship 时要 refactor。 本 spec 接受 — 跟 L36 "fix in 1 file is not fix in N files"
  同一家族。
- **R2**: T008 server 进程删 ignore 文件不直接 Refresh 候选窗（IPC 协议不动）。
  用户需切 schema / 重新 deploy / 重启 WeaselServer 才生效。 spec 008 T008 原文
  "用户已确认不要 5s toast" — 接受 no-auto-refresh。
- **R3**: WeaselPanel `m_style` 字段是 `weasel::UIStyle&` (reference, 非 copy)。
  `_RefreshStylePalette` 改的是引用指向的对象的成员。 验证 build 接受。
- **R4**: spec 006 / 031 / 030 多次 ship 后，`m_style` 类型 / palette 接口可能要
  refactor。 L36 / L38 已记录这个 pattern。

## 5. References

- spec 008 T007 / T008 (parent)
- spec 004 §9.4 F11 (dark mode 横切规范)
- spec 006 (前置但未 ship — partial ship 是 spec 032 唯一可行路径)
- spec 031 (m_ignoreList / user_ignore.txt 状态) — T008 删除的目标
- `WeaselUI/WeaselUI.h` (UIStyle 结构)
- `WeaselUI/WeaselPanel.h:23-100` (msg map + 成员段)
- `WeaselUI/WeaselPanel.cpp:1147,1156` (m_hoverIndex = -1 重置点)
- `WeaselServer/WeaselServerApp.cpp:48-77` (SetupMenuHandlers)
- `WeaselServer/resource.h:15-29` (ID range 40001-40016)
- `include/WeaselUtility.h` (IsUserDarkMode 已存在 helper)
- L22 / L23 / L25 / L28 / L31 / L36 / L37 / L38
- TDD.md §3.1 — 9 集成测试清单，本 spec 加第 10/11 个
- AGENTS.md sec 5 — pre-commit gate