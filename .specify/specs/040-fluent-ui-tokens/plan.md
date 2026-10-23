# 040 - Plan

## Constitution Check

| Principle | Status | Note |
|---|---|---|
| **I — 单一目的** | ✅ | token 收敛 + 视觉现代化 + 系统材质接入，三个目标收敛于"统一 Fluxing 视觉" |
| **II — 开放扩展** | ✅ | token 表预留 NG-001（theme.json）的字段命名空间，但 v1 不实现 |
| **III — 最小耦合** | ✅ | token 文档与代码解耦；`FluxingTheme` 单例是唯一耦合点 |
| **IV — 可解释** | ✅ | 每个 token 有"来源"列；每个 Phase 2 backend 选择有视觉对比证据 |
| **V — 持续验证** | ✅ | 每 Phase 配 SC + Verification；TestQuickPanelDialog 像素 diff 强制通过 |

| Hard Rule | Status | Note |
|---|---|---|
| **R1** (commit scope 前缀) | ✅ planned | T001-T203 使用 `WeaselUI` / `RimeWithWeasel` / `WeaselServer` / `installer` / `fluxing` scope |
| **R2** (spec.md 无 tech 词) | ✅ enforced | spec.md 中无 WTL/Boost/C++/MFC/ATL/WeaselTSF/D2D/Direct2D/GDI/DWMWA 等 |
| **R3** (task P1/P2/P3 + USxxx) | ✅ planned | tasks.md 每个 task 标 [USxxx-A] + P# |
| **R4** (commit checklist git status -sb + git diff --stat) | ✅ planned | tasks.md 中每个 commit 前置 checklist |
| **R5** (task ≤ 4h, 1-3 files) | ✅ planned | 全部 task ≤ 4h；多数 1 file |
| **R6** (test 并行) | ✅ planned | 每个 Phase 配独立 test 任务 |
| **R7** (registry 写安全) | N/A | 不动 registry |
| **R8** (PPL 进程替换) | ✅ planned | QuickPanel 替换触发 WeaselServer 重启（WeaselServer PPL 保护）—— 在 CHANGELOG 注明需注销重登录 |
| **R9** (CHANGELOG 头条) | ✅ planned | tasks.md 末尾 CHANGELOG-1 / CHANGELOG-2 |

| Project Rule | Status | Note |
|---|---|---|
| **P1** (process) | ✅ | 顺序：spec → plan → tasks → 等用户签字 → 实现 |
| **P2** (TSF 不阻塞 I/O) | ✅ planned | OS build 检测在 QuickPanel 创建时执行，**不在** TSF 回调；订阅 FluxingTheme 走 mutex |
| **P3** (librime 不动) | ✅ | 不触碰 librime/ |
| **P4** (commit 规范) | ✅ planned | 用 `WeaselUI(scope)` / `RimeWithWeasel(scope)` 等 |
| **P5** (中文用户字符串) | ✅ | 不修改用户可见字符串；token 是颜色代码 |
| **P6** (one-shot migration) | N/A | 不动 user-data 路径 |
| **P7** (PPL) | ✅ planned | QuickPanel 替换需要用户注销重登录（与 L17/L18 同源） |
| **P8** (brand fork 范围) | ✅ | 决议 1 锁定保留火流猩橙；不修 CLSID / 路径 |

---

## Tech Approach

### Phase 1 — Palette struct 扩展（零 UI 变更）

**目的**：扩展 `Palette` 数据结构，向后兼容现有 4 字段消费者。

#### 1.1 文件：`RimeWithWeasel/FluxingDarkModeBridge.h`
- `struct Palette` 增加 10 个字段：
  ```cpp
  DWORD panel_glass_top;   // QuickPanel 玻璃顶部色
  DWORD panel_glass_bot;   // QuickPanel 玻璃底部色
  DWORD icon_dim;          // 默认图标色（gray）
  DWORD accent_primary;    // 品牌橙（QuickPanel hover）
  DWORD accent_secondary;  // 品牌紫
  DWORD hairline;          // 1px hairline 色
  DWORD highlight_top;     // 顶部高光（macOS 风格）
  DWORD destructive;       // 红色 destructive
  BYTE  alpha_panel_top;   // 玻璃顶部 alpha (0-255)
  BYTE  alpha_panel_bot;   // 玻璃底部 alpha
  ```
- 不修改原 4 字段位置——保持现有 consumer byte-equal

#### 1.2 文件：`RimeWithWeasel/FluxingDarkModeBridge.cpp`
- `kPaletteDark` / `kPaletteLight` 增补 10 个字段值
- **`kPaletteLight.hilited_text` 改为 `0x0A84FF`**（Apple systemBlue）
- `kPaletteDark.hilited_text` 保持 `0xFFFFFF`

#### 1.3 文件：`test/TestDarkModeBridge/`
- 新增 `PaletteFieldTest.cpp`（**R-007 钉死布局防漂移**）：
  - `static_assert(offsetof(Palette, back) == 0)` 等 offsetof 测试钉 4 字段原位置
  - `static_assert(sizeof(Palette) == 52)` 防字段意外删除（4 字节对齐）
  - `static_assert(sizeof(Palette) % 4 == 0)` 4 字节对齐
  - 单字段 round-trip 测试（构造 → 断言 → 析构）
- 现有 `TestDarkModeBridge` / `TestDarkModeBroadcast` 必须**全绿**

### Phase 2 — Material Backend Split

**目的**：QuickPanel 根据 OS build 选择材质后端。

#### 2.1 文件：`WeaselUI/FluxingComponents/FluxingTheme.h`
```cpp
enum class MaterialBackend : uint8_t {
  kPerPixelAlpha,  // L87 路径（Win10 / 旧 Win11）
  kMica            // 系统材质（Win11 build ≥ 22621）
};

class FluxingTheme {
public:
  static MaterialBackend DetectMaterialBackend();  // 内部用 RtlGetVersion
  // ... existing API
private:
  MaterialBackend material_backend_;  // 构造时一次性初始化
};
```

#### 2.2 文件：`RimeWithWeasel/BuildInfo.{h,cpp}`（新增）
```cpp
// 封装 RtlGetVersion 调用，返回 win11 build 是否 ≥ 22621
bool IsWin11BuildAtLeast(DWORD min_build);
```
- 单测覆盖：mock build = 19045（Win10 22H2）、22620（Win11 21H2）、22621（Win11 22H2）、22622（Win11 22H2+）

#### 2.3 新文件：`WeaselServer/QuickPanelMica.{h,cpp}`
```cpp
namespace QuickPanelMica {
  bool IsSupported();  // 委托 BuildInfo
  void Enable(HWND hwnd);  // DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP, DWMSYSBT_MAIN) + DwmSetWindowAttribute(DWMWA_CAPTION_COLOR) + DwmSetWindowAttribute(DWMWA_BORDER_COLOR)
  void Paint(HWND hwnd, HDC hdc);  // 系统材质画背景，仅渲染图标 / 文字
}
```

#### 2.4 文件：`WeaselServer/QuickPanelDialog.h`
- 5 个野生 RGB 常量**保留**但标 `[[deprecated("use FluxingTheme instead")]]`——给现有 diff 留缓冲
- 新增 `static MaterialBackend s_backend_`（QuickPanel 进程级单次决定）

#### 2.5 文件：`WeaselServer/QuickPanelDialog.cpp`
- `OnCreate` 内：`s_backend_ = QuickPanelMica::IsSupported() ? kMica : kPerPixelAlpha`
- 绘制分支：if Mica → `QuickPanelMica::Paint()` else → 现有 L87 路径

#### 2.6 测试矩阵
- `TestQuickPanelDialog`：新增 backend selection 测试（mock build 号）
- 视觉回归：spec 070 L87 baseline 截图 vs v0.19.0.18 per-pixel alpha 截图 diff < 0.1%
- 多屏验证（手动）：双屏切换 + DPI 切换 + Win10 ↔ Win11 切换 + 系统主题切换

### Phase 3 — Geometry / Spacing / Radius Token Export

**目的**：把 11 个 QuickPanel 几何常量迁出到统一 token 头。

#### 3.1 新文件：`WeaselUI/include/GeometryTokens.h`
```cpp
namespace fluxing::tokens {
  // Panel
  inline constexpr int kPanelW = 252;
  inline constexpr int kPanelH = 48;
  inline constexpr int kPanelPadding = 5;
  inline constexpr int kPanelRadius = 20;
  // Button
  inline constexpr int kBtnSize = 39;
  inline constexpr int kBtnGap = 1;
  inline constexpr int kBtnRadius = 10;
  // Icon
  inline constexpr int kIcoSize = 21;
  // Brand block
  inline constexpr int kBrandSize = 39;
  // Add as needed
}
```

#### 3.2 文件：`WeaselServer/QuickPanelDialog.h`
- 移除 11 个 `static constexpr int`
- 替换为：`using fluxing::tokens::kPanelW;` 等 alias
- 测试：`TestQuickPanelDialog` 像素 diff 全绿

### Phase 4 — Typography (P2, optional)

#### 4.1 文件：`output/data/weasel.yaml`
- 增加 `style.font_ui_size_tiny: 11` / `font_ui_size_small: 12` / `font_ui_size_body: 13` / `font_ui_size_button: 14` / `font_ui_size_label: 17`
- **风险**：weasel.yaml 是上游 RIME schema，可能破坏 spec 001 — Phase 4 设为 P2，可独立决定是否 ship

#### 4.2 文件：`WeaselUI/FluxingComponents/FluxingComponents.cpp`
- 首次绘制时读 weasel.yaml typography，缓存
- DirectWrite `SetFontSize()` 应用

### Phase 5 — Animation (depends on spec 039)

**前置**：spec 039 完成回调签名扩展 `(old_palette, new_palette, progress)`。

#### 5.1 FLUENT-UI-TOKENS.md
- 增加 §3.6 Transition：
  ```
  transition.duration.short: 150ms
  transition.duration.normal: 200ms
  transition.duration.long: 300ms
  easing.standard: cubic-bezier(0.2, 0.0, 0.0, 1.0)
  ```

#### 5.2 QuickPanel 系统材质切换
- 系统主题变化时，应用 200ms 渐变
- material backend 切换（OS 升级后）不应用渐变——避免运行中闪烁（R-008 mitigation）

---

## File-level Changes (exact paths)

### 新增
- `.specify/specs/040-fluent-ui-tokens/{spec,plan,tasks}.md`
- `docs/design/FLUENT-UI-TOKENS.md`（已起草 2026-07-12）
- `RimeWithWeasel/BuildInfo.{h,cpp}`
- `WeaselUI/include/GeometryTokens.h`
- `WeaselServer/QuickPanelMica.{h,cpp}`
- `test/TestQuickPanelDialog/BackendSelectionTest.cpp`
- `test/TestDarkModeBridge/PaletteFieldTest.cpp`

### 修改
- `RimeWithWeasel/FluxingDarkModeBridge.h`（Palette 字段扩展）
- `RimeWithWeasel/FluxingDarkModeBridge.cpp`（kPalette* 字段增补 + hilited_text 改 blue）
- `WeaselUI/FluxingComponents/FluxingTheme.h`（MaterialBackend enum + Detect）
- `WeaselUI/FluxingComponents/FluxingTheme.cpp`（构造时初始化 material_backend_）
- `WeaselServer/QuickPanelDialog.h`（5 RGB 常量标 deprecated + 11 几何常量改 alias + 新增 s_backend_）
- `WeaselServer/QuickPanelDialog.cpp`（OnCreate 分支 + Paint 分支）
- `WeaselUI/FluxingComponents/FluxingComponents.cpp`（P4：读 weasel.yaml typography）
- `output/data/weasel.yaml`（P4：font_ui_size_*）
- `CHANGELOG.md`（v0.19.0.18 头条）
- `lessons-learned.md`（Phase 2 完成时新增 L88：Mica 集成经验）

### 不动
- `librime/`（P3）
- `WeaselTSF/`、`WeaselIPC/`、`WeaselDeployer/`（本 spec 不涉及；deployer 视觉不在 scope）
- installer / NSIS（P3 + 不动 registry）

---

## Build / Test / Release Impact

### Build
- **新文件**：7 个（含 3 个 spec 文档）—— `BuildInfo`, `GeometryTokens`, `QuickPanelMica`, 3 个 test cpp
- **新 deps**：无（`RtlGetVersion` 是 ntdll native API，无需额外 lib）
- **新 vcxproj 项**：3 个 cpp / h 加入 `WeaselServer.vcxproj` + `WeaselUI.vcxproj` + `RimeWithWeasel.vcxproj` + `TestDarkModeBridge.vcxproj` + `TestQuickPanelDialog.vcxproj`
- **构建时间**：+0（与 spec 033 / 037 同量级，< 1 分钟）

### Test
- 新增单测：~120 行（PaletteFieldTest）+ ~80 行（BackendSelectionTest）
- 现有测试必须全绿（不能破坏 spec 033 / 037 / 070）
- 视觉回归：手动截图比对（spec 070 L87 baseline）
- 验证矩阵（手动，Phase 2）：多屏 / DPI / Win10↔11 / 系统主题切换

### Release
- **版本号**：v0.19.0.18（接续 v0.19.0.17）
- **CHANGELOG 头条**：
  - "现代化候选框高亮色为 Apple systemBlue"
  - "QuickPanel 在 Win11 22H2+ 启用系统材质（Mica），保留旧版 fallback"
- **PPL caveat**（R8）：WeaselServer 是 PPL 保护（spec 070 L17/L18），新 `WeaselServer.exe` 上线需用户**注销重登录**——release notes 显式声明
- **NSIS**：不动（NG-007：不动 registry）

### Backward Compatibility
- 现有 Palette 4 字段（back / text / hilited_back / hilited_text）位置不变
- byte-equal 兼容性：`_RefreshStylePalette` 等老 consumer 不需改
- QuickPanel 5 RGB 常量从 `static constexpr` 改 `[[deprecated]]`——保留 1 个版本灰度，下个版本移除

---

## Risk Mitigation Detail

### R-001（DWMWA 多屏 z-order）
- Phase 2 完成前**手动验证矩阵**：
  1. 双屏扩展 + 主屏 100% DPI / 副屏 150% DPI
  2. 拖动 QuickPanel 跨屏
  3. 切换主副屏
  4. 系统主题切换 dark ↔ light
- 任一异常 → 把 Mica 默认值改为 "灰度 flag"，仅 opt-in 启用

### R-002（silently no-op）
- `BuildInfo::IsWin11BuildAtLeast` 单测覆盖 build = 22620 / 22621 / 22622 三边界
- QuickPanel `OnCreate` 内必须看到 `s_backend_` 与 build 号一致

### R-003（拆 backend 估时翻倍）
- Phase 2 第一刀只合 feature flag：
  - 默认 kPerPixelAlpha（现状不变）
  - 用户可通过 `%AppData%\Fluxing\config.yaml` 加 `experimental.mica: true` 启用
- 验证 1 周后默认开

### R-005（L87 回归）
- `TestQuickPanelDialog` 像素 diff 与 spec 070 L87 baseline 比对
- 强制阈值 < 0.1%

### R-007（Palette 布局漂移）
- `PaletteFieldTest` 内 `static_assert(sizeof(Palette) == ...)` 钉死
- CI 失败即拦截

### R-008（OS build 检测启动延迟）
- `RtlGetVersion` 是 native API（< 1ms）
- 不在 TSF 回调、不在 IPC 关键路径
- 不可观察影响

### R-006（决议 1 破坏老用户视觉习惯）
- 仅影响 light 主题 hilited text 颜色；dark 主题不变
- 灰度策略：v0.19.0.18 ship 后，CHANGELOG 头条 + first-run wizard "候选框高亮色已更新为 Apple systemBlue"
- 监控 user feedback 14 天；若负面反馈 > 10%，提供 `weasel.yaml` `style.candidate_hilited_text_override` 字段做 opt-out（spec 041+ 评估）