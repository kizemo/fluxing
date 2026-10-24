# Handoff — Phrase UI Esc 退出 Bug 仍 fail (Phase F Bug 3 续修)

> **承接会话**: 当前 (会话 ID: `91ae53ab-55df-41da-a873-ee11d1a6d12e`)
> **承接 ship**: `c6d8b35` (v0.19.0.39-fluxing installer) ← `bba3683` (Phase F source fix)
> **本会话状态**: v0.19.0.39 已 ship + 装机,但 Bug 3 (Esc 退出) 仍 fail
> **下个会话任务**: 修 Bug 3 root cause (`OnNotify` 缺 `case LVN_KEYDOWN`) + ship v0.19.0.40

---

## 0. TL;DR (一句话)

承接 v0.19.0.39 (c6d8b35) 装机反馈:**Phrase UI Esc 键仍不退出 dialog**。Root cause 是 `OnNotify` 缺 `case LVN_KEYDOWN` — ListView 焦点时 Esc 转 LVN_KEYDOWN 给 parent,OnNotify 没处理,sandbox Test 24 是 false-positive (直接 dispatch WM_KEYDOWN 到 dialog WndProc,不模拟 ListView 焦点)。修法:OnNotify 加 case LVN_KEYDOWN 处理 VK_ESCAPE → Hide,加 Test 25 (模拟 ListView 焦点 + LVN_KEYDOWN),ship v0.19.0.40。

---

## 1. 项目导航 (新会话直接 Read)

按 CLAUDE.md §1 进站阅读顺序:

1. `F:\soft\00selfmade\rime_claude\CONTEXT.md` — 身份/模块/词汇/不变量
2. `F:\soft\00selfmade\rime_claude\AGENTS.md` — 操作手册,首屏 + 危险区 + checklist
3. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` — 法则层 R1-R9 / P1-P8
4. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` §A + §11 — 部署 + NSIS 配方
5. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` — 工程技能消费契约
6. `F:\soft\00selfmade\rime_claude\handoff-phase-f-bug3-vk_escape-2026-07-20.md` (本文件)

---

## 2. v0.19.0.39 (Phase F) ship 链

```
c6d8b35 chore(release): v0.19.0.39-fluxing installer (Phase F 装机)
bba3683 fix(WeaselServer): v0.19.0.39 (Phase F) — 按钮路径 Phrase UI 键盘路由 + Esc 退出
5ed804a ci: update _check_install_v2.ps1 expected md5 to v0.19.0.38
c5da883 chore: gitignore output/_extract_*/ and fluxing-logo PNG
52c8144 docs: correct v0.19.0.38 installer md5 + counts in CHANGELOG
```

**Phase F source 改了**:
- `WeaselServer/PhrasesDialog.cpp:179-191` — Show() 创建路径加 `AllowSetForegroundWindow(ASFW_ANY)` + `SetForegroundWindow(s_hwnd)`
- `WeaselServer/PhrasesDialog.cpp:50-54` — 加 setter (mock 函数指针)
- `WeaselServer/PhrasesDialog.h:42-50` — SetForegroundFn typedef + setter + static member
- `WeaselServer/WeaselServerApp.cpp:314-326` — onPhrases lambda 先 `QuickPanelDialog::Hide()` 释放 foreground

**Phase F 测试加了**:
- Test 23 — foreground API mock (Show() 调 AllowSetForegroundWindow + SetForegroundWindow 1 次)
- Test 24 — Esc handler (WM_KEYDOWN VK_ESCAPE → Hide) — **但这个 test 是 false-positive**

---

## 3. v0.19.0.39 装机反馈 (2026-07-20)

User 装机 v0.19.0.39 后报告:

| Bug | 报告 | 状态 |
|---|---|---|
| **Bug 1** (按钮路径 ↑↓/Enter) | "选中后回车或双击均可上屏" | ✅ **已修** (Phase F Option C: Hide QP + SetForegroundWindow) |
| **Bug 2** (Alt+. 全键盘) | (隐含 work,user 没明说) | ✅ **work** (回归保护) |
| **Bug 3** (Esc 退出) | **"使用键盘esc, UI仍无法消失"** | ❌ **仍 fail** |

---

## 4. Bug 3 root cause (5 阶段 systematic-debugging)

### Phase 1 - Root cause

**关键观察**: Test 24 (`SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0)`) sandbox GREEN,但装机 FAIL → Test 24 是 false-positive。

**真正的 keyboard event 路由**:
- 焦点在 ListView (s_hList) → 按 Esc → ListView 内部处理 → 转 **`LVN_KEYDOWN` 给 parent** (PhrasesDialog WndProc 通过 WM_NOTIFY)
- `PhrasesDialog::OnNotify` 现有 case list (line 724-790):
  ```cpp
  case LVN_ITEMCHANGED: ...
  case NM_DBLCLK: ...
  case NM_RETURN: ...
  case NM_CLICK: ...
  // ❌ 缺 LVN_KEYDOWN
  ```
- OnNotify 收到 LVN_KEYDOWN → switch fallthrough → `return 0` (line 789) → Esc 不响应

### Phase 2 - Reproduce (sandbox 已 verify Test 24 不够)

Test 24 直接 dispatch WM_KEYDOWN 到 dialog WndProc,绕过了 ListView 焦点 + WM_NOTIFY 路径。Test 24 应该 fail 但实际 pass,因为 Esc handler 在 WndProc WM_KEYDOWN level 已被触发。

**真正的 negative test (新会话必加)**:
- Test 25: 模拟 ListView 焦点 + 发 LVN_KEYDOWN VK_ESCAPE 给 dialog → 验证 Hide called
- 这种 dispatch 应该走 `SendMessageW(hwnd, WM_NOTIFY, ID_LIST, lParam)` 其中 lParam 是 `NMLVKEYDOWN`

### Phase 3 - Fix

**Option A (推荐)**: `OnNotify` 加 `case LVN_KEYDOWN`:
```cpp
case LVN_KEYDOWN: {
  LPNMLVKEYDOWN pnkd = reinterpret_cast<LPNMLVKEYDOWN>(lp);
  if (pnkd && pnkd->wVKey == VK_ESCAPE) {
    Hide();
  }
  return 0;
}
```

**Option B (备选)**: subclass ListView WndProc 处理 Esc。但 Option A 更简单,跟现有 NM_DBLCLK / NM_RETURN 模式一致。

### Phase 4 - Verify

- 新 Test 25: 发 WM_NOTIFY + LVN_KEYDOWN VK_ESCAPE → 验证 `s_hwnd == nullptr`
- 全 test suite 跑 (TestPhrasesDialog 期望 101 PASS / 0 FAIL)
- `_buildflow.cmd` Exit 0
- 跑 installer rebuild + md5 verify (用 env.bat WEASEL_BUILD=40)

### Phase 5 - Ship

- commit 1: `fix(WeaselServer): v0.19.0.40 (Phase F Bug 3 fix) — LVN_KEYDOWN handler for Esc`
- commit 2: `chore(release): v0.19.0.40-fluxing installer (Phase F Bug 3 装机)`
- CHANGELOG.md append v0.19.0.40 entry
- task.md append Phase F Bug 3 fix 段
- `_check_install_v2.ps1` update expect md5 (binary + installer + build time)

---

## 5. 关键 file:line (新会话直接 Edit)

### 修改 1: `WeaselServer/PhrasesDialog.cpp:789` (OnNotify 末尾 return 前)

加 case LVN_KEYDOWN:
```cpp
// 在 NM_CLICK case 后面加 (跟 NM_DBLCLK / NM_RETURN 风格一致):
case LVN_KEYDOWN: {
  // v0.19.0.40 (Phase F Bug 3 续修): ListView 焦点时按 Esc, ListView 自身不处理
  //   Esc, 转 LVN_KEYDOWN 给 parent. 原 OnNotify 没 case LVN_KEYDOWN → return 0
  //   → Esc 无响应. Test 24 (sandbox) false-positive 因为直接 dispatch WM_KEYDOWN
  //   到 dialog WndProc, 不模拟 ListView 焦点 + LVN_KEYDOWN 路径.
  //   修法: OnNotify 加 case LVN_KEYDOWN 处理 VK_ESCAPE → Hide.
  //   (跟 NM_DBLCLK / NM_RETURN 模式一致, 不 subclass ListView WndProc.)
  LPNMLVKEYDOWN pnkd = reinterpret_cast<LPNMLVKEYDOWN>(lp);
  if (pnkd && pnkd->wVKey == VK_ESCAPE) {
    Hide();
  }
  return 0;
}
```

### 修改 2: `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (Test 24 之后, main() 调用之前)

加 Test 25:
```cpp
// v0.19.0.40 (Phase F Bug 3 续修): OnNotify LVN_KEYDOWN VK_ESCAPE → Hide
//   真 root cause (装机反馈 v0.19.0.39): Test 24 false-positive (直接 dispatch
//   WM_KEYDOWN 到 dialog WndProc, 不模拟 ListView 焦点 + LVN_KEYDOWN 路径).
//   ListView 焦点时按 Esc → ListView 自身不处理 → 转 LVN_KEYDOWN 给 parent
//   (PhrasesDialog WndProc 通过 WM_NOTIFY). Test 25 模拟完整路径:
//   1) Show() 后 ListView 有焦点 (OnCreate line 568 SetFocus(s_hList))
//   2) SendMessage WM_NOTIFY + LVN_KEYDOWN + VK_ESCAPE → 验证 Hide called
static void TestLVNKeyDownEscapeHides() {
  std::cout << "\n[Test 25] OnNotify LVN_KEYDOWN VK_ESCAPE → Hide" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("25.0: s_hList + s_hwnd 已由 Show() 创建",
        hList && IsWindow(hList) && hwnd && IsWindow(hwnd));

  // ListView 焦点 (OnCreate 默认设了,显式 set 一次保险)
  SetFocus(hList);

  // 构造 NMLVKEYDOWN struct
  NMLVKEYDOWN nmlv = {};
  nmlv.hdr.hwndFrom = hList;
  nmlv.hdr.idFrom = (UINT_PTR)1100;  // ID_LIST
  nmlv.hdr.code = LVN_KEYDOWN;
  nmlv.wVKey = VK_ESCAPE;
  nmlv.flags = 0;

  // dispatch WM_NOTIFY
  SendMessageW(hwnd, WM_NOTIFY, nmlv.hdr.idFrom, (LPARAM)&nmlv);

  CHECK("25.1: LVN_KEYDOWN VK_ESCAPE → Hide 销毁 dialog (s_hwnd == nullptr)",
        PhrasesDialog::s_hwnd == nullptr);
}
```

main() 加调用:
```cpp
// v0.19.0.40 (Phase F Bug 3 续修: LVN_KEYDOWN)
test::TestLVNKeyDownEscapeHides();
```

### 修改 3: env.bat WEASEL_BUILD 39 → 40 (不 commit, gitignored)

```bash
set "WEASEL_BUILD=40"
set "PRODUCT_VERSION=0.19.0.40"
set "FILE_VERSION=0.19.0.40"
```

### 修改 4: `_check_install_v2.ps1` expect md5 update

```powershell
$expectedMd5 = '<新 WeaselServer.exe md5>'  # v0.19.0.40 binary
$expectedInstallerMd5 = '<新 installer md5>'  # v0.19.0.40 installer
$expectedBuildTime = '<NSIS build 时间>'  # v0.19.0.40 build
```

### 修改 5: CHANGELOG.md append v0.19.0.40 entry (跟 v0.19.0.39 段接)

### 修改 6: task.md append Phase F Bug 3 fix 段 (跟 Phase E 段接)

---

## 6. Test 24 fix (是否需要修改)

**不需要**。Test 24 验证 WndProc level WM_KEYDOWN VK_ESCAPE → Hide — 仍然 valid (用于 direct dispatch path, 比如 SendMessage WM_KEYDOWN 从外部强制 trigger)。

但建议 Test 24 加注释:
```cpp
// Test 24: WndProc WM_KEYDOWN VK_ESCAPE → Hide
//   验证 direct dispatch path (外部强制 WM_KEYDOWN). 跟 Test 25 (LVN_KEYDOWN
//   WM_NOTIFY path) 不同, 两条路径都覆盖.
```

---

## 7. 装机 verify 命令 (新会话完成后)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.40-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 user 端 verify**:
- Module 1+2 md5 = 新 binary md5 (MATCH)
- Module 3 L66 4 keys 写出
- Alt+. 启动 Phrase UI → ↑↓/Enter/DoubleClick/Esc 全 work (回归)
- 按钮路径启动 → ↑↓/Enter/DoubleClick/Esc 全 work (Phase F Bug 1 + Bug 3 全 fix)

---

## 8. 沉淀 (lessons-learned.md L## candidate)

**L100-VK-ESCAPE-FALSE-POSITIVE**:
- Test 24 (`SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0)`) 是 false-positive — 直接 dispatch WM_KEYDOWN 到 dialog WndProc 绕过了 ListView 焦点 + WM_NOTIFY 路径
- ListView 焦点时键盘事件转 `LVN_KEYDOWN` 给 parent,**不**通过 WM_KEYDOWN 到 dialog
- 修法:OnNotify 加 case LVN_KEYDOWN 处理 VK_ESCAPE
- **教训**:ListView / TreeView / Edit 等 common control 焦点时,键盘事件走 WM_NOTIFY (LVN_KEYDOWN / NM_KEYDOWN 等),不走 dialog WndProc WM_KEYDOWN。test 必须模拟完整 focus chain,否则 false-positive

候选入 `.learnings/ERRORS.md` (v0.19.0.40 ship 时记录):
- Pattern-Key: `listview-escape-wndmock-false-positive`
- 复发风险:任何涉及 list control + 键盘事件的 future fix
- 触发条件:test dispatch WM_KEYDOWN 到 dialog 但实际 keyboard 走 WM_NOTIFY path

---

## 9. 已 ship 状态保留 (新会话不要 reset)

✅ `c6d8b35` + `bba3683` 已 commit,`release/fluxing-0.19.0.39-installer.exe` 已 ship
✅ Phase E GC cleanup 完成 (`.git` 5.3G → 3.9G)
✅ 测试结果已记录 (`memory/2026-07-20.md` daily log)
✅ Working tree 累积改动未 commit (`.claude/skills/*` + `CLAUDE.md` + `librime` + `task.md`) — **不**动这些

新会话仅 commit **Phase F Bug 3 fix 的 3 files**:
- `WeaselServer/PhrasesDialog.cpp` (OnNotify 加 case LVN_KEYDOWN)
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (Test 25)
- `release/fluxing-0.19.0.40-installer.exe` (新 ship)
- (可选 `CHANGELOG.md` + `_check_install_v2.ps1` + `task.md` — Phase F Bug 3 段)

---

## 10. 不变量 (新会话不要 break)

- `env.bat` 是 gitignored,**绝不 commit**
- `release/` 目录 installer 是 git-tracked,新 ship 必须 add + commit
- installer rebuild 前先 `rm -f` 旧 version (避免 dir 混淆)
- Phase E git gc 已完成,不重新跑 (除非 user 反馈新的 dangling objects)
- 不动 `output/_extract_*` (已 gitignored,清理不优先)
- 维持 P4 module prefix: `fix(WeaselServer)` + `chore(release)`
- 装机命令必带 `taskkill + reg delete` (L13/L54/L66 强约束)

---

## 11. 时间预算

- 本会话已 ~80 min (从 16:00 起),处于 2 小时红线内
- 新会话建议活跃预算 < 1.5 小时 (Bug 3 fix 简单,不需要 2 小时)
- Bash count 估算 ~30-40 (Bug 3 fix 范围小)
- Installer rebuild 5-10 min,NSIS build 1-2 min,build 3-5 min,test 跑 ~5 min

---

## 12. 一句话总结 (新会话开头可贴)

> 承接 v0.19.0.39 (c6d8b35) 装机反馈:Phrase UI Esc 仍不退出 dialog。Root cause 是 OnNotify 缺 case LVN_KEYDOWN (ListView 焦点时 Esc 转 LVN_KEYDOWN 给 parent),Test 24 是 false-positive (直接 dispatch WM_KEYDOWN 绕过 ListView)。修法:OnNotify 加 case LVN_KEYDOWN + Test 25 (模拟 LVN_KEYDOWN dispatch) + ship v0.19.0.40。完整 handoff:`F:\soft\00selfmade\rime_claude\handoff-phase-f-bug3-vk_escape-2026-07-20.md`。

---

— END OF HANDOFF —