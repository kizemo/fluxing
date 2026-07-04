# 032 · Plan · 候选字编辑收尾（spec 008 T007 + T008）

## 1. 技术上下文

- **C++ 17 / WTL / ATL**（与 WeaselUI / WeaselServer 同栈）。
- **修改文件**（5 个）：
  - `WeaselUI/WeaselPanel.h` — 加 `OnSettingChange` 声明 + `WM_SETTINGCHANGE` 消息 map + `_RefreshStylePalette` 声明。
  - `WeaselUI/WeaselPanel.cpp` — 加 `OnSettingChange` + `_RefreshStylePalette` impl。
  - `WeaselServer/resource.h` — 加 `ID_WEASELTRAY_RESTORE_IGNORED 40017`。
  - `WeaselServer/WeaselServer.rc` — 3 个语言菜单块各加 1 项。
  - `WeaselServer/WeaselServerApp.cpp` — `SetupMenuHandlers()` 加 `ID_WEASELTRAY_RESTORE_IGNORED` handler。
- **新建**（6 个文件）：
  - `test/TestPanelDarkModeSubscribe/TestPanelDarkModeSubscribe.cpp` — 3 真实 assertions。
  - `test/TestPanelDarkModeSubscribe/TestPanelDarkModeSubscribe.vcxproj` — 10 号 test project。
  - `test/TestTrayRestoreIgnored/TestTrayRestoreIgnored.cpp` — 3 真实 assertions。
  - `test/TestTrayRestoreIgnored/TestTrayRestoreIgnored.vcxproj` — 11 号 test project。
- **编辑**（3 个）：
  - `weasel.sln` — 加 2 个 test project 节点（GUIDs: D5C9D3E1-7B2F / E5C9D3E1-7B2F）。
  - `scripts/test-infra/run-test-suite.bat` — 9 → 11。
  - `scripts/test-infra/verify-test-binaries-fresh.bat` — 9 → 11。
- **不引入新依赖**。
- **不修改** `output/install.nsi`（不 release installer）。
- **不修改** `env.bat` / `weasel.props`（bookkeeping sub-release）。

## 2. Architecture

### 2.1 T007 暗色主题订阅

```
Windows 10+ 切换暗色 → OS 广播 WM_SETTINGCHANGE + lParam = "ImmersiveColorSet"
       |
       v
WeaselPanel::OnSettingChange (新加) 收到消息
       |
       v
_RefreshStylePalette() (新加)
       |
       v
IsUserDarkMode() (L25 已 ship helper) → bool
       |
       v
m_style.back_color / text_color / hilited_candidate_back_color 切换 (partial ship palette)
       |
       v
_ResizeWindow() + Refresh() 触发重绘
```

**注**: `m_style` 字段名是 `weasel::UIStyle&` (引用, 来自 `WeaselIPCData.h:195`)。
  实际字段是 `back_color` / `text_color` / `hilited_candidate_back_color` /
  `hilited_candidate_text_color` / `border_color` / `shadow_color` (不是 `bg_color`)。

### 2.2 T008 tray 恢复按钮

```
User right-clicks tray icon -> 选择 "恢复被屏蔽的候选 (&I)"
       |
       v
WeaselServerApp::SetupMenuHandlers 注册的 ID_WEASELTRAY_RESTORE_IGNORED handler
       |
       v
Win32 FindFirstFileW(<userDir>\*.user_ignore.txt) + DeleteFileW for each
       |
       v
Return true (no IPC push; 下次输入 / schema 切换时 Refresh 自然发生)
```

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec 032 §0 明确 T007 partial ship + T008 full ship |
| II. Test | OK | 2 new tests, 3 assertions each |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | T007 partial ship 风险已记 L## 候选 (R1) |
| V. Incremental | OK | spec 008 4-phase ship 第 4 阶段独立 |
| R1-R9 | OK | 引用 L22-L38 |
| P1-P8 | OK | P8 brand-fork scope |
| L36 | OK | T007 partial ship 风险明确；spec 006 上线时 refactor |
| L37 | OK | 写文件 byte-level; m_style 引用类型小心 |
| L38 | OK | xmake lua 没改（不改构建系统），build 风险低 |

## 4. 风险

- **R1**: T007 partial ship palette 跟 spec 004 §9.3 完整 dark palette 不一致。 spec 006 ship 时要 refactor。 接受。
- **R2**: T008 server 进程删 ignore 文件不直接 Refresh 候选窗。 用户切 schema / 重新 deploy 才生效。 spec 008 T008 原文 "用户已确认不要 5s toast" — 接受 no-auto-refresh。
- **R3**: WeaselPanel `m_style` 字段是 `weasel::UIStyle&` (reference, 非 copy)。 `_RefreshStylePalette` 改的是引用指向的对象的成员。 build 验证。
- **R4**: spec 004 §9.4 要求 200ms 渐变过渡。 本 spec 跳过 (spec 006 负责)。 接受 immediate switch。

## 5. 验证步骤

1. `cmd /c xbuild.bat weasel` → 0 errors。
2. `cmd /c scripts\test-infra\run-test-suite.bat` → 11/11 PASS。
3. `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` → 11/11 FRESH。
4. AGENTS.md sec 5 五步 pre-commit gate。
5. 不做 NSIS smoke test（不改 install.nsi）。

## 6. Release

- **version bump**：无（bookkeeping sub-release，v0.18.18.0 仍 latest）。
- **tag**：无。
- **commit**：`feat(fluxing): spec 032 - candidate rbutton finalize (T007+T008 of 008)`。
- **push**：`git push kizemo Fluxing`。

## 7. References

- spec 008 / 031 / 004 §9.4 / 006
- TDD.md §3.1 — 9 集成测试清单
- L22-L38
- AGENTS.md sec 5 pre-commit gate