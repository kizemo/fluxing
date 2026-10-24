# Handoff — Phase K4: FluxingPhrasesDialog UX polish (2026-07-23)

> **承接 HEAD**: `88280dd` (chore(release): v0.19.0.56 installer)
> **承接 Ship**: v0.19.0.56 已 ship, 装机测试发现 5 项反馈 (3 已修, 2 待修 + 1 新功能)
> **Installer**: `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.56-installer.exe`
> **user 装机截图**: `C:\Users\Duanyi\.claude\uploads\c7b5152c-d3bb-44fd-baea-312aee58dccc\dfba94bc-0019-43eb-851b-80c39c42657c-pasted-image-1784779957995.png`
> **本次范围**: FluxingPhrasesDialog v0.19.0.57 (Phase K4)
> **重启必读**: `.specify/memory/lessons-learned.md` L100 / L101 / L102 / L104 / L105 / L106

---

## 1. 当前 ship 状态 (v0.19.0.56)

### 1.1 commits (HEAD = `88280dd`)
```
88280dd chore(release): v0.19.0.56 installer
44292d7 fix(WeaselServer + FluxingPhrasesDialog + installer): v0.19.0.56 (Phase K3 T019 catastrophic-recovery) — MSBuild rebuild real binary + L66 64-bit KnownClasses hardening
823bc1f chore(release): v0.19.0.55 T019 hotfix installer
8559ed4 fix(WeaselServer + FluxingPhrasesDialog): v0.19.0.55 (Phase K3 T019 hotfix) — pipe handle 泄漏 + NM_DBLCLK Hide 先于 INJECT + Option B 简化
```

### 1.2 v0.19.0.55 catastrophic regression 已修复
- 根因: xmake after_build 复制 2 MiB 全零 EXE 进 installer (L106)
- 修法: 改走 MSBuild 路径重建真 binary; install.nsi 加 `${DisableX64FsRedirection}` 包裹 KnownClasses 写入 (L66 hardening)
- 5-axis sanity: WeaselServer md5 `c93d3ff3...` ≠ v0.19.0.54 的 `4d446fff...`, 6 个关键 EXE 全部 real PE
- L106 lessons-learned 已记录

### 1.3 装机反馈 (user 2026-07-23)
装机 v0.19.0.56 后 user 跑 `_check_install_v2.ps1` 等工具, 现反馈 5 项:

| # | 状态 | 描述 |
|---|---|---|
| 1 | ❌ 待修 | 顶部蓝条仍然偏低, 想去除这部分 |
| 2 | ❌ 待修 | 鼠标长按拖动 UI, 但**点击标题栏任意位置** UI 迅速更换位置, 光标变为 UI 标题栏左上角 (snap to top-left bug) |
| 3 | ✓ 已修 | 双击/回车, 短语列表中短语均会正确上屏 |
| 4 | ✓ 已修 | 编辑栏可以输入中文 |
| 5 | ✓ 已修 | Esc 可以退出 UI |
| 6 (新增) | ❌ 待修 | 编辑栏输入文字后按回车, 应智能: 添加新词状态 → 完成新增; 编辑状态 → 完成编辑 |

---

## 2. 根因 + 修法 (基于代码实读)

### Bug 1: 顶部"蓝条"偏低/多余 (视觉)

**观察** (screenshot): UI 顶部有一条细的蓝色窄带, 与下方"常用短语"title text 区分明显。User 感觉"多余, 想删掉"。

**代码定位**: `FluxingPhrasesDialog/PhrasesDialog.cpp` L756-784 `OnPaint`

```cpp
ModalChrome::PaintBackgroundAndBorder(hdc, paintW, paintH, kBgTop, kBgBot,
                                      paintRadius, kBorderColor);
RECT titleRc = {0, 0, paintW, (int)(kTitleH * s_dpiScale)};  // 48*dpiScale
DrawTextW(hdc, L"\x5e38\x7528\x77ed\x8bed", -1, &titleRc,
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
```

**根因诊断**:
- `kBgTop=RGB(245,245,248)` 与 `kBgBot=RGB(220,222,230)` 形成垂直渐变, 整 client 都填。
- `kTitleH = 48` 物理像素 (DPI 1.0) 是 title text 容器 — 不创建独立的 title bar 色块。
- 用户看到的"顶部蓝条"很可能是:
  - 候选 A: WS_THICKFRAME (L175) + WS_POPUP 组合下, Windows 给 popup 窗口绘制 2px 系统 frame, 在高对比度主题下显示为蓝色细线 (但用户说"仍然偏低"说明它很薄)
  - 候选 B: `AdjustWindowRectEx` (L223) 给 WS_THICKFRAME 算了 caption 高度, 但 WS_POPUP 不应有 caption — 实测会生成 23px 顶部"non-client area", 但 WS_POPUP 又会忽略它 → 视觉残留
  - 候选 C: ModalChrome gradient 顶部第一段本身就是浅色带, 在视觉上独立于下半深色 — 这就是 user 说的"蓝条"

**最小修法** (推荐):
- L770 `PaintBackgroundAndBorder` 改用 single solid color: `ModalChrome::PaintBackground(hdc, paintW, paintH, kBgBot, kBgBot)` (top=bot → 单色填充)
- 或 `PaintBackgroundAndBorder` 直接换 helper
- 副作用: 视觉变扁平, 与"macOS Liquid Glass"渐变设计意图冲突, 但 user 优先

**完整修法** (备选, 推荐 P2 polish):
- WS_THICKFRAME 改回 WS_BORDER (L175) — 失去 resize 但消除 thick frame 视觉残留
- WS_EX_TOOLWINDOW (L170) 保留 — 让 dialog 不出现在 taskbar
- 单色 bg 方案同步

**测试要求**:
- Visual 截图: 截图 dpi=1.0 / dpi=1.5 / dpi=2.0 三档, 确认顶部无独立色块
- 真机 Win11 22H2 + Win10 21H2 各 1 套

---

### Bug 2: 点击 title bar 拖动时 UI 跳到左上角 (snap-to-topleft)

**代码定位**: `FluxingPhrasesDialog/PhrasesDialog.cpp` L1206-1231 `OnLButtonDown` + L1247-1273 `OnMouseMove` + L1328-1339 `BeginDrag`

```cpp
// OnLButtonDown (L1221-1226): 在 title 区直接 drag
if (y < titleH_phys) {
  BeginDrag(hwnd);     // ← **bug**: 没设置 s_dragOrigin / s_dragWndOrigin
  return 0;
}

// BeginDrag (L1328-1332): 只设 s_isDragging + SetCapture
void PhrasesDialog::BeginDrag(HWND hwnd) {
  s_isDragging = true;
  SetCapture(hwnd);
  // ❌ 缺: s_dragOrigin = GetCursorPos()
  // ❌ 缺: GetWindowRect(&rc) → s_dragWndOrigin = {rc.left, rc.top}
}

// OnMouseMove (L1255-1270): 用 stale origin 算 delta
if (s_isDragging) {
  POINT pt;
  GetCursorPos(&pt);
  int newX = s_dragWndOrigin.x + (pt.x - s_dragOrigin.x);  // ❌ stale origin
  int newY = s_dragWndOrigin.y + (pt.y - s_dragOrigin.y);
  SetWindowPos(hwnd, nullptr, newX, newY, 0, 0, ...);
}
```

**根因** (铁证):
- `s_dragOrigin` 和 `s_dragWndOrigin` 在 `BeginDrag` **未被设置**, 只在 `StartLongPressTimer` (L1306-1318) 设置。
- 第一次 drag: 静态字段保留初始值 `{0, 0}` (L60-61)。
- `OnMouseMove` 第一次触发: `newX = 0 + (pt.x - 0) = pt.x`。即 dialog 被移动到 `(pt.x, pt.y)` — 等于 cursor 在屏幕的绝对位置。
- 视觉: dialog 跳到 cursor 所在位置, cursor 相对新 dialog 位置 = (0, 0) = 左上角。
- 这正好匹配 user 描述 "UI 迅速更换位置, 光标变为在 UI 标题栏左上角"。

**修法** (single root cause fix):
- `BeginDrag` 必须初始化 origin (跟 `StartLongPressTimer` 行为一致):

```cpp
void PhrasesDialog::BeginDrag(HWND hwnd) {
  s_isDragging = true;
  // v0.19.0.57 (Phase K4 Bug 2 修): 初始化 drag origin, 避免 OnMouseMove
  // 用 stale {0,0} origin 计算 delta 导致 UI 跳到 cursor 绝对位置。
  POINT pt;
  GetCursorPos(&pt);
  s_dragOrigin = pt;
  RECT rc;
  GetWindowRect(hwnd, &rc);
  s_dragWndOrigin = {rc.left, rc.top};
  SetCapture(hwnd);
}
```

**可重构** (optional): 把"init origin"逻辑抽到 helper `CaptureDragOrigin(HWND hwnd)`, `BeginDrag` 与 `StartLongPressTimer` 都调, DRY。

**测试要求**:
- 新增 Test 29 `T_TitleBar_Drag_NoJump`: 模拟 WM_LBUTTONDOWN 在 title 区 → WM_MOUSEMOVE +5,+5 → 检查 SetWindowPos 的新坐标 = 原始 + (5,5), 不是 (cursor.x, cursor.y)
- 现有 Test (long-press drag) 应继续 PASS
- 真机验证: click title + 拖 5px, 5cm, 跨屏都应平滑跟随

---

### Bug 3 (新增功能): 编辑栏 Enter 智能 add/edit

**代码定位**: `PhrasesDialog.cpp` L838-867 `OnKeyDown case VK_RETURN` + L962-973 `OnNotify case LVN_ITEMCHANGED` + L1052-1115 `OnCommand case ID_BTN_ADD_TOP / ID_BTN_EDIT`

**当前行为**:
- input 焦点 + Enter → 永远发 ID_BTN_ADD_TOP click → 总是新增 (即便用户在编辑模式)
- 用户在 list 选 item → `LVN_ITEMCHANGED` 自动 fill input text, `m_selectedIndex` 仍 = 选中 idx
- 修法意图: list item 选中后按 Enter → 编辑模式 (走 ID_BTN_EDIT); input 是空 + Enter → 新增 (走 ID_BTN_ADD_TOP)

**问题边界**:
- 真正的"编辑模式"信号 = `m_selectedIndex >= 0` (list 有选中 item)
- input 自动 fill 的逻辑 (L967-970) 让 user 难以区分"我刚加完空 input 又填了一次 text"
- **额外**: input 失去焦点时 (e.g. user click list), 当前没清空 input → 残留旧 edit text
- **额外**: 新增完成 (ID_BTN_ADD_TOP) 已经 `SetWindowTextW(s_hInput, L"")` + `SetFocus(s_hInput)` (L1080-1081), OK

**最小修法** (推荐):
- OnKeyDown VK_RETURN 在 input 焦点时:
  ```cpp
  case VK_RETURN: {
    if (s_hInput && GetFocus() == s_hInput) {
      // v0.19.0.57 (Phase K4 Bug 3 修): 智能 add/edit — list 选中时 Enter
      // 触发 Edit (user 在编辑选中的 phrase), 否则触发 Add。
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_BTN_EDIT, BN_CLICKED), 0);
      } else {
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_BTN_ADD_TOP, BN_CLICKED), 0);
      }
      return 0;
    }
    // ... list 焦点的逻辑不变
  }
  ```

- ID_BTN_EDIT 完成时清 input + 复位 m_selectedIndex (跟 ADD 一致):
  ```cpp
  // case ID_BTN_EDIT 末尾 (L1115 之后):
  SetWindowTextW(s_hInput, L"");
  SetFocus(s_hInput);
  m_selectedIndex = -1;
  ```

- LVN_ITEMCHANGED 选中时: 当前 fill input OK, 但失去选中时 (itemState 改变 selected→not) 应清 input:
  ```cpp
  // case LVN_ITEMCHANGED:
  if (pn->uNewState & LVIS_SELECTED) {
    m_selectedIndex = pn->iItem;
    // fill input (现有)
  } else if ((pn->uChanged & LVIF_STATE) && !(pn->uNewState & LVIS_SELECTED)) {
    // v0.19.0.57 (Phase K4 Bug 3 polish): 失去选中 → 清 input 跟 reset
    // m_selectedIndex (避免 Enter 在 input 误触发 Edit)
    if (m_selectedIndex == pn->iItem) {
      m_selectedIndex = -1;
      if (s_hInput) SetWindowTextW(s_hInput, L"");
    }
  }
  ```

**测试要求**:
- 新增 Test 30 `T_Input_Enter_Adds_WhenNoSelection`: 空 input → type text → Enter → 验证 ID_BTN_ADD_TOP 触发, m_phrases.size += 1
- 新增 Test 31 `T_Input_Enter_Edits_WhenSelected`: list 选 item → input auto-fill → Enter → 验证 ID_BTN_EDIT 触发, m_phrases[idx].text 变 input text
- 现有 Test 17/24 (Enter in list / Esc) 继续 PASS
- 真机: 装机后跑 1) 新增 2) 编辑 3) Esc 三条 user flow

---

## 3. 不在本次范围

- Bug 3 (Dblclick/Enter 上屏) 已修 — 不动 `OnNotify case NM_DBLCLK/NM_RETURN` (L974-1019)
- Bug 4 (中文输入) 已修 — 不要改 `s_hInput` IMM32/TSF 任何路径 (v0.19.0.46/47/49 教训)
- Bug 5 (Esc 退出) 已修 — `case VK_ESCAPE` (L868-870) + `case LVN_KEYDOWN VK_ESCAPE` (L1031-1044)
- L66 / L106 hardening 不要改 — 已 ship, 验证过

---

## 4. 关键文件清单

| 路径 | 状态 |
|---|---|
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | L770 (Bg gradient) / L1217-1226 (OnLButtonDown title drag) / L1228-1231 (LongPress 入口) / L838-844 (OnKeyDown Enter input) / L962-973 (LVN_ITEMCHANGED fill input) / L1052-1115 (OnCommand Add/Edit handlers) / L1328-1339 (BeginDrag) — 5 处修改 |
| `FluxingPhrasesDialog/PhrasesDialog.h` | 仅当 helper 抽到 `CaptureDragOrigin` 时改, 否则不动 |
| `FluxingPhrasesDialog/ModalChrome.cpp` | 不动 (helper 不变, 调用方传 single color 即可) |
| `test/TestPhrasesDialog/TestPhrasesDialog.cpp` | 新增 Test 29 / 30 / 31 (drag-nojump + add/edit Enter) |
| `env.bat` | `WEASEL_BUILD=56→57`, `PRODUCT_VERSION=0.19.0.56→0.19.0.57`, `FILE_VERSION` 同步 |

---

## 5. ship 流程 (按既有)

1. 改代码 → msbuild weasel.sln /p:Configuration=Release /p:Platform=Win32 /p:WEASEL_BUILD=57 ...
2. `output\_verify_exe.ps1` 跑 (已在 v0.19.0.56 ship 时写, 复用)
3. `output\archives\fluxing-0.19.0.57-installer.exe` 生成
4. **不要**走 `build_v0XX.ps1` (xmake 路径 L106 教训)
5. 2 commits: `fix(FluxingPhrasesDialog): v0.19.0.57 (Phase K4) — title drag origin init + Enter add/edit 智能 + 顶部蓝条去除` + `chore(release): v0.19.0.57 installer`
6. amend Co-Authored-By trailer (CLAUDE.md §2 必)
7. copy release/ + sync aiec.fun (feedback_pinyin_release_workflow.md)

---

## 6. Phase K4 Tasks (建议)

| # | Task | Test |
|---|---|---|
| 1 | 顶部蓝条去除 (OnPaint 单色 bg 或 WS_BORDER) | Visual screenshot 3 dpi |
| 2 | Title drag origin init (BeginDrag 加 capture) | Test 29 T_TitleBar_Drag_NoJump |
| 3 | Enter 智能 add/edit (OnKeyDown + OnCommand 收尾) | Test 30/31 |
| 4 | 失选中清 input (LVN_ITEMCHANGED 离开 case) | Test 32 (optional) |
| 5 | 重建 + 真机 ship v0.19.0.57 | sandbox + 真机 |

---

## 7. 防重犯提醒

- **screenshot 验证** (LE §6.4): Bug 1 视觉类必须 visual verify before tag
- **Test 29 必加**: 任何 drag 路径改动必须验证 OnMouseMove 第一帧不跳变
- **.claude/rules/build-toolchain.md** L106 guardrail 暂未落地, 本次仍走 MSBuild
- **CLAUDE.md §2 必查清单**: `git status -sb` + `git diff --stat` 在 commit 前

---

## 8. 上下文读取建议 (新会话第一分钟)

1. 读 `MEMORY.md` + 本文件
2. 读 `.specify/memory/lessons-learned.md` L100-L106 (最近 7 个 lesson)
3. 读 `PhrasesDialog.cpp` L756-1339 (本文件涉及的范围)
4. 跑 `output\_verify_exe.ps1` (复用, 不用改) 确认 baseline binary OK
5. 开始 Phase K4 Task 1