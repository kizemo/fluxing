# 040 - Tasks

> **Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)
> **Convention**: Each task is ≤ 4h, 1-3 files, has [USxxx] + P# tag (R5 / R3).
> **Pre-commit checklist** (per R4): `git status -sb` + `git diff --stat` before each commit (per CLAUDE.md §2).

## Phase 0 — Spec 签字（已完成 2026-07-12）

- [x] **T000**: Claude + User 签字 spec.md / plan.md / tasks.md（spec-check 0 BLOCKER，0 remaining WARNING）
- [x] **T000a**: **决议保留 `hilited_back` 灰底**（spec 037 R3 不动）。理由：(1) 品牌橙预算留给 QuickPanel hover；(2) Apple systemBlue 预算留给候选框 hilited；(3) spec 037 已 ship 用户反馈中性，改需要灰度。Button.h 不改。
- [x] **T000b**: **决议采用品牌橙→紫渐变** `linear-gradient(135deg, RGB(255,95,49), RGB(155,81,224))`。理由：(1) 火流猩 = 火系品牌，暖色匹配 fire/sunset 意象；(2) "chrome 暖 / 功能冷"对比清晰；(3) 与上游 Apple-clone IME 拉开视觉差。FLUENT-UI-TOKENS.md 新增 `color.brand.gradient` token（实现推迟到 v0.19.0.19+ 单独议，本 spec 不实现）。

---

## P1 (must)

### Phase 1 — Palette struct 扩展

- [ ] **T001**: 扩展 `RimeWithWeasel/FluxingDarkModeBridge.h` 的 `struct Palette` 增加 10 字段（8 DWORDs + 2 BYTEs：panel_glass_top/bot、icon_dim、accent_primary/secondary、hairline、highlight_top、destructive、alpha_panel_top/bot）[US001-C, FR-002, R-007]（P1, 1 file, 1.5h）
- [ ] **T002**: 更新 `RimeWithWeasel/FluxingDarkModeBridge.cpp` 的 `kPaletteDark` / `kPaletteLight` 增补 10 字段；改 `kPaletteLight.hilited_text` = `0x0A84FF` [US002-A, FR-003, R-004, R-006]（P1, 1 file, 1h）
- [ ] **T003**: 新增 `test/TestDarkModeBridge/PaletteFieldTest.cpp`，含 `static_assert(offsetof + sizeof)`（钉布局 R-007）+ 字段 round-trip 单测 [US001-C, SC-001-2, SC-002-1, SC-002-2]（P1, 1 file, 2h）
- [ ] **T004**: 在 `TestDarkModeBridge.vcxproj` / xmake 中注册 PaletteFieldTest；运行 `TestDarkModeBridge.exe` 验证全绿（spec 033 不破）[US001-C, SC-001-2]（P1, 1 file, 1h）
- [ ] **T005**: 在 `TestDarkModeBroadcast.vcxproj` / xmake 中验证现有 spec 022 / 034 测试套件不破 [US001-C, SC-001-2]（P1, 1 file, 0.5h）

**Commit 1**（P1 收口）：
```
feat(RimeWithWeasel): extend Palette with 9 fields + Apple systemBlue hilited (spec 040 T001-T005)

- Palette struct: +panel_glass_top/bot, icon_dim, accent_primary/secondary,
  hairline, highlight_top, destructive, alpha_panel_top/bot
- kPaletteLight.hilited_text: 0x000080 -> 0x0A84FF (Apple systemBlue, R-004)
- TestDarkModeBridge: +PaletteFieldTest (sizeof static_assert R-007)
- TestDarkModeBroadcast: verified backward-compat

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

### Phase 2 — Material Backend Split

- [ ] **T101**: 新增 `RimeWithWeasel/BuildInfo.h` / `.cpp`，封装 `RtlGetVersion` 调用；导出 `bool IsWin11BuildAtLeast(DWORD min_build)` [FR-004, R-002]（P1, 2 files, 2h）
- [ ] **T102**: 在 `test/TestDarkModeBridge/` 新增 `BuildInfoTest.cpp`，mock build = 19045 / 22620 / 22621 / 22622 四值，断言后端选择正确 [SC-007-1, R-002]（P1, 1 file, 2h）
- [ ] **T103**: 扩展 `WeaselUI/FluxingComponents/FluxingTheme.h`，新增 `enum class MaterialBackend { kPerPixelAlpha, kMica }` + `static MaterialBackend DetectMaterialBackend()` + 字段 `material_backend_` [FR-004, FR-008, R-002]（P1, 1 file, 2h）
- [ ] **T104**: `FluxingTheme.cpp` 构造函数中调用 `material_backend_ = DetectMaterialBackend()`（一次性，不订阅 refresh）[FR-008]（P1, 1 file, 1h）
- [ ] **T105**: 新增 `WeaselServer/QuickPanelMica.h` / `.cpp`，实现 `IsSupported()` / `Enable(HWND)`（DWMWA_SYSTEMBACKDROP = DWMSYSBT_MAIN + DwmExtendFrameIntoClientArea）/ `Paint(HWND, HDC)`（系统材质下不画背景，仅画 icon / 文字）[US001-B, FR-004, R-001]（P1, 2 files, 4h）
- [ ] **T106**: 修改 `WeaselServer/QuickPanelDialog.h`，5 个野生 RGB 常量标 `[[deprecated("use FluxingTheme palette instead")]]`（保留 1 版本灰度）；新增 `static MaterialBackend s_backend_` [FR-005]（P1, 1 file, 1h）
- [ ] **T107**: 修改 `WeaselServer/QuickPanelDialog.cpp` 的 `OnCreate`：`s_backend_ = QuickPanelMica::IsSupported() ? kMica : kPerPixelAlpha`；`Paint` / `OnPaint` 走分支（kMica → `QuickPanelMica::Paint`；else → 现有 L87 路径）[FR-004, FR-005, FR-008, R-003, R-005]（P1, 1 file, 4h）
- [ ] **T108**: 新增 `test/TestQuickPanelDialog/BackendSelectionTest.cpp`：mock build 号 + 断言 s_backend_ 选择正确 + 模拟 DwmSetWindowAttribute 调用次数 [US001-B, SC-003-1, SC-003-2, SC-007-1]（P1, 1 file, 3h）
- [ ] **T109**: 在 `WeaselServer.vcxproj` + `WeaselUI.vcxproj` + `TestQuickPanelDialog.vcxproj` 中注册新文件；运行 `TestQuickPanelDialog.exe` + `TestQuickPanelRefactor.exe` 验证 [SC-005-1]（P1, 3 files, 1h）
- [ ] **T110**: 手动视觉回归矩阵（per plan.md R-001 mitigation）：
  - 双屏 + 100% / 150% DPI 切换
  - Win10 (build 19045) + Win11 22H2 (22621) + Win11 22H2+ (22622) 三平台
  - 系统主题 dark ↔ light 切换
  - 截图与 spec 070 L87 baseline 对比，diff < 0.1%
  
  输出：截图 + 报告 docs/design/spec-040-mica-regression.md [US001-B, SC-003-1, SC-003-2, SC-005-1, R-001]（P1, 0 file, 2h）

**Commit 2**（P1 收口）：
```
feat(WeaselServer): QuickPanel material backend split - Mica on Win11 22H2+ (spec 040 T101-T110)

- BuildInfo: +IsWin11BuildAtLeast() (RtlGetVersion wrapper, R-002 mitigation)
- FluxingTheme: +MaterialBackend enum + DetectMaterialBackend()
- QuickPanelMica: new module - DWMWA_SYSTEMBACKDROP=DWMSYSBT_MAIN
- QuickPanelDialog: 5 wild RGB constants marked [[deprecated]] (1-version grace)
- QuickPanelDialog: OnCreate/Paint branches by backend
- Tests: BackendSelectionTest (R-002 mock coverage) + visual regression matrix (R-001)
- QuickPanelRefactor: pixel diff < 0.1% vs L87 baseline (R-005)

NOTE: WeaselServer is PPL-protected (L17/L18). New WeaselServer.exe
requires user logout/login or reboot to load.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

### Phase 3 — Geometry / Spacing / Radius Token Export

- [ ] **T201**: 新增 `WeaselUI/include/GeometryTokens.h`，namespace `fluxing::tokens`，导出 11 个 inline constexpr int（kPanelW / kPanelH / kPanelPadding / kPanelRadius / kBtnSize / kBtnGap / kBtnRadius / kIcoSize / kBrandSize）[US001-C, FR-001, FR-006]（P1, 1 file, 2h）
- [ ] **T202**: 修改 `WeaselServer/QuickPanelDialog.h`，移除 11 个本地 `static constexpr int`；替换为 `using fluxing::tokens::kPanelW;` 等 alias（保持 ABI 不变）[FR-001, FR-006, R-005]（P1, 1 file, 2h）
- [ ] **T203**: 运行 `TestQuickPanelDialog.exe` + `TestQuickPanelRefactor.exe` + 视觉回归截图，验证像素 diff < 0.1% [US001-C, SC-005-1, SC-006-1, R-005]（P1, 0 file, 1.5h）

**Commit 3**（P1 收口）：
```
refactor(WeaselUI): extract 11 QuickPanel geometry constants to GeometryTokens.h (spec 040 T201-T203)

- WeaselUI/include/GeometryTokens.h: namespace fluxing::tokens (kPanelW/H/Padding/Radius,
  kBtnSize/Gap/Radius, kIcoSize, kBrandSize)
- QuickPanelDialog.h: 11 static constexpr int -> using aliases
- TestQuickPanelDialog + TestQuickPanelRefactor: pixel diff < 0.1%

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

## P2 (should)

### Phase 4 — Typography（可选）

- [ ] **T301**: 扩展 `output/data/weasel.yaml`，加 `style.font_ui_size_tiny/small/body/button/label`（11/12/13/14/17），并更新 `default.yaml` 注释说明 [FR-006, US001-C]（P2, 1 file, 2h）
- [ ] **T302**: 修改 `WeaselUI/FluxingComponents/FluxingComponents.cpp`，首次绘制时读 weasel.yaml typography，缓存；DirectWrite `SetFontSize()` 应用 [FR-006]（P2, 1 file, 3h）
- [ ] **T303**: 新增 `test/TestFluxingComponents/TypographyTest.cpp`，mock yaml 注入 + 断言 typography 选择 [US001-C, SC-001-1]（P2, 1 file, 2h）

**Commit 4**（P2，可独立决定 ship）：
```
feat(WeaselUI): typography tokens from weasel.yaml (spec 040 T301-T303)

- weasel.yaml: +style.font_ui_size_{tiny,small,body,button,label}
- FluxingComponents: DirectWrite SetFontSize from yaml on first paint
- TestFluxingComponents: typography override test

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

## P3 (could)

### Phase 5 — Animation（依赖 spec 039 完成）

- [ ] **T401**: 在 `docs/design/FLUENT-UI-TOKENS.md` 新增 §3.6 Transition（duration.short=150ms / normal=200ms / long=300ms + easing.standard=cubic-bezier(0.2,0,0,1)）[FR-001]（P3, 1 file, 1h）
- [ ] **T402**: 在 QuickPanel 系统主题切换时，应用 200ms 渐变（依赖 spec 039 回调扩展）；material backend 切换不应用渐变 [FR-004, FR-008]（P3, 1 file, 3h）

**Commit 5**（P3，spec 039 后）：
```
feat(WeaselServer): QuickPanel transition tokens on theme swap (spec 040 T401-T402)

- FLUENT-UI-TOKENS.md: +transition.duration, easing
- QuickPanelDialog: 200ms cubic-bezier(0.2,0,0,1) gradient on theme change
- (no transition on material backend swap per R-008)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

## CHANGELOG

- [ ] **CHANGELOG-1**: 在 `CHANGELOG.md` 加 v0.19.0.18 头条：
  ```
  ### 主要更新
  - **现代化候选框高亮色**为 Apple systemBlue `#0A84FF`（light）/ 白色（dark），替代 rime/weasel 上游遗留的 navy（spec 040 T002, R-004）
  - **QuickPanel 在 Win11 22H2+（build 22621+）启用系统材质（Mica）**，旧版 Win10 / Win11 fallback 到 per-pixel alpha 渐变（spec 040 T101-T110, R-001/R-005）
  - **11 个 QuickPanel 几何常量收敛到** `WeaselUI/include/GeometryTokens.h`，供未来组件复用（spec 040 T201-T203）
  - ⚠️ **PPL caveat**：WeaselServer 是受保护进程，新版本需用户**注销重登录或重启**才能加载（spec 070 L17/L18 + spec 040 R8）
  ```
  [R-004, R-001, R-005, R-8]（P1, 1 file, 0.5h）

- [ ] **CHANGELOG-2**: 追加 sub-section "修复"：列出 TestDarkModeBroadcast / TestFluxingComponents 等通过的回归测试 ID（per spec 033 / 037 既有流程）（P1, 1 file, 0.5h）

**Commit 6**（P1 收口）：
```
docs(fluxing): CHANGELOG v0.19.0.18 entries for spec 040 (T-CHANGELOG-1/2)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

---

## Lessons-Learned

- [ ] **L88**: Phase 2 完成时，在 `.specify/memory/lessons-learned.md` 新增 L88：
  ```
  L88 — Mica 集成经验（spec 040 Phase 2, 2026-07-XX）
  - DWMWA_SYSTEMBACKROP 在 build < 22621 silently no-op，必须显式 RtlGetVersion 检测
  - 多屏 + DPI 切换时 z-order 偶发异常，验证矩阵必跑
  - 系统材质下 GDI 不能画背景（系统已画），仅画 icon / 文字
  - 详见 spec 040 plan.md R-001/R-002/R-005 mitigation
  ```
  [R-001, R-002, R-005]（P1, 1 file, 0.5h）

---

## Verification

- [ ] **V001**: `xbuild.bat weasel installer` 全量构建——exit 0，无 warning
  ```bash
  cd F:/soft/00selfmade/rime_claude
  ./xbuild.bat weasel installer
  echo $?  # 期望 0
  ```
- [ ] **V002**: 三个 test 项目全绿：
  ```bash
  ./test/TestDarkModeBridge/Release/TestDarkModeBridge.exe        # 期望 0
  ./test/TestQuickPanelDialog/Release/TestQuickPanelDialog.exe    # 期望 0
  ./test/TestQuickPanelRefactor/Release/TestQuickPanelRefactor.exe # 期望 0
  ```
- [ ] **V003**: 视觉截图——候选框 light 主题高亮文字 `#0A84FF`：
  ```bash
  # 手动：安装 v0.19.0.18，切换 light 主题，输入拼音，截图候选框
  # 像素采样：在 hilited_text 位置读 RGB，应为 (10, 132, 255)
  ```
- [ ] **V004**: 视觉截图——QuickPanel 在 Win11 22H2+ 显示 Mica：
  ```bash
  # 手动：在 Win11 build 22621+ 系统上 Show QuickPanel
  # Process Hacker 观察：WeaselServer.exe 无大量 GDI ExtTextOut / per-pixel alpha scan
  ```
- [ ] **V005**: 视觉截图——QuickPanel 在 Win10 / Win11 < 22621 显示 per-pixel alpha 渐变（与 spec 070 L87 byte-equal）
- [ ] **V006**: `git status -sb` + `git diff --stat` 在每个 commit 前（per CLAUDE.md §2 / R4）
- [ ] **V007**: 5 野生 RGB 常量 grep 验证：
  ```bash
  cd F:/soft/00selfmade/rime_claude
  grep -rn "kBgTop\|kBgBot\|kIconDim\|kAccent\b\|kAccent2" WeaselServer/ | grep -v "[[deprecated"
  # 期望：无输出
  ```
- [ ] **V008**: 11 几何常量迁出验证：
  ```bash
  grep -n "static constexpr int kPanelW\|kBtnSize\|kBtnGap" WeaselServer/QuickPanelDialog.h
  # 期望：无输出（已迁到 GeometryTokens.h）
  ```
- [ ] **V009**: FLUENT-UI-TOKENS.md 5 张表存在性 + 来源列覆盖度（绑 SC-001-1）：
  ```bash
  test -f docs/design/FLUENT-UI-TOKENS.md && echo "exists"
  grep -cE "^\| \`" docs/design/FLUENT-UI-TOKENS.md   # 期望 ≥ 60 行 token
  grep -c "来源" docs/design/FLUENT-UI-TOKENS.md      # 期望 ≥ 5（每张表表头有"来源"列）
  grep -c "^### 3\." docs/design/FLUENT-UI-TOKENS.md  # 期望 ≥ 5（Color/Typography/Spacing/Radius/Elevation）
  ```

---

## Quality Gates (per Constitution Quality Gates)

- [ ] **C-FMT-1**: Commit 1 提交前：`clang-format -i RimeWithWeasel/FluxingDarkModeBridge.h RimeWithWeasel/FluxingDarkModeBridge.cpp test/TestDarkModeBridge/PaletteFieldTest.cpp`（constitution Quality Gate 1 + P1）
- [ ] **C-FMT-2**: Commit 2 提交前：`clang-format -i` 所有 Phase 2 修改文件（BuildInfo.*, FluxingTheme.*, QuickPanelMica.*, QuickPanelDialog.*, BackendSelectionTest.cpp）（constitution Quality Gate 1 + P1）
- [ ] **C-FMT-3**: Commit 3 提交前：`clang-format -i WeaselUI/include/GeometryTokens.h WeaselServer/QuickPanelDialog.h`（constitution Quality Gate 1 + P1）
- [ ] **C-FMT-4**: Phase 4/5 提交前同样 `clang-format -i`（constitution Quality Gate 1 + P1）

> 注意：上述 C-FMT 与 commit message 同 PR，但不计入 commit scope；commit message 仍按 P4 用 `feat(scope):` / `refactor(scope):` 格式

---

## Cross-spec Coordination

- [ ] **COORD-1**: 与 spec 041 (FluxingComponents DPI validation) 协调——Phase 3 几何 token 引用可能受 DPI 倍率重计算影响；若 spec 041 先于本 spec 完成，本 spec T201-T203 应在 spec 041 完成后回归
- [ ] **COORD-2**: 与 spec 039 (FluxingComponents animations) 协调——Phase 5 T402 等待 spec 039 回调签名扩展；若 spec 039 推迟，本 spec Phase 5 同步推迟

---

*Last updated: 2026-07-12 · Spec 040 Phase 1-3 (P1) ready for user approval*