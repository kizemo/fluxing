# 报告 — v0.19.0.45 输入框中文输入失效 (Bug 4) 根因 + 修法

> 装机 v0.19.0.45 user 反馈: 列表框 / 输入框随 UI 边界调整已 work; 剩余 bug: **在输入框中仍无法输入中文**。
> 4 个 ship 版本 (v0.19.0.35 / v0.19.0.36 / v0.19.0.43 / v0.19.0.45) 装机端都复现 → 远超 3 次复发门槛, 需正式 lessons-learned L##。
> **本文档只做根因报告; 修复需用户授权后由 Claude 改 PhrasesDialog.cpp + TestPhrasesDialog.cpp + 重新 ship。**

---

## 1. 根因 (5 阶段 systematic-debugging)

### 1.1 复现

装机 v0.19.0.45 (commit `a7048db` installer, WeaselServer.exe md5 `b19d4338d360a3cc1a9b8491d56f4eae`) → 打开常用短语 dialog → 点击顶部 input 框 → 按拼音打字 → **中文候选词不出**, 输入框内仅出现英文, IME 状态栏不显示。

### 1.2 数据流跟踪

按 WndProc 调用链:

1. `PhrasesDialog::Show()` (line 200) `CreateWindowExW` 创建 dialog, 同步 dispatch `WM_CREATE` → `OnCreate` 跑
2. `OnCreate` line 644-648: `s_hInput = CreateWindowExW(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ...)` — 创建 input edit control
3. **line 671-675 (关键, v0.19.0.43 e658e42 引入, v0.19.0.45 没动)**:
   ```cpp
   HIMC himc = ImmCreateContext();         // 创建 isolated HIMC
   if (himc) {
     ImmAssociateContext(s_hInput, himc);  // 把 himc 关联给 s_hInput
     // 不调 ImmReleaseContext (per MSDN, 假设 himc 跟 hwnd 同生死)
   }
   ```
4. line 758 `PopulateList(s_hList)` → line 766 `LayoutDialog(...)` → line 776 `SetFocus(s_hList)` (默认焦点在 ListView)
5. user 点 input 框 → `SetFocus(s_hInput)` 触发 → Windows 想给 s_hInput 配 IME
6. **但 s_hInput 已关联 our isolated himc** (line 673) → Windows 不会重新配 → TSF shim 也不会接管 (per-hwnd 已 lock) → IME 候选词走不通

### 1.3 多组件边界分析 (per systematic-debugging 1.4)

| 层级 | 状态 | 证据 |
|---|---|---|
| 1. WeaselServer 进程 (PPL) | OK | md5 `b19d4338d360a3cc1a9b8491d56f4eae` = 装机源 |
| 2. weaselx64.dll (TSF shim) | OK | L66 keys 已写 (HKCU\0x00000804 + HKLM\KnownClasses) |
| 3. rime.dll (librime) | OK | IME 候选词本身能在主编辑框出 |
| 4. PhrasesDialog WndProc | **FAIL** | `ImmCreateContext` + `ImmAssociateContext` 锁住 isolated himc, 屏蔽 TSF / system default IME |
| 5. s_hInput edit control | **FAIL** | 已关联 isolated himc, 跟 TSF shim 互斥 |

→ **Layer 4 失败**: `PhrasesDialog.cpp:671-675` 的 IME 关联代码在 TSF 进程下不正确工作。

### 1.4 v0.19.0.45 真因 (vs v0.19.0.44 真因)

| 版本 | commit | 改动 | IME 行为 |
|---|---|---|---|
| v0.19.0.35 | fa196049 | 引入 `ImmCreateContext` + `ImmAssociateContext` + `ImmDestroyContext` | IME 死 (ImmDestroyContext 毁 himc) |
| v0.19.0.36 | 79d522b6 | revert 改成 `ImmReleaseContext(s_hInput, himc)` 2 参 | IME 死 (refcount 1→0 销毁 himc) |
| v0.19.0.37-42 | (no IME change) | 累积 bug | (stale L97 binary 掩盖) |
| v0.19.0.43 | e658e42 | 改成**不调** `ImmReleaseContext` | **IME 仍死** (TSF 进程不认 isolated HIMC) |
| v0.19.0.44 | e2f0a60 | resize layout + drag reorder (没动 IME) | IME 仍死 (carry-over) |
| v0.19.0.45 | 2d842fc | input 拉伸 + UI margin 缩小 (没动 IME) | **IME 仍死 (本次装机反馈)** |

### 1.5 真因 (一句话)

`PhrasesDialog.cpp:671-675` 的 `ImmCreateContext` + `ImmAssociateContext` 模式创建了 isolated HIMC 给 `s_hInput`, **在 TSF shim 进程 (WeaselServer.exe) 下被 TSF IME 路径互斥** — TSF 要求 hwnd 用 system default IME context, 我们强行给他一个 isolated himc 反而让 IME 候选词不出。**正确做法**: 让 Windows / TSF 自动给 hwnd 配 system default IME context, 不要自己创建 isolated HIMC。

---

## 2. 3 个候选修法

### 方案 A — 完全删除 IME 关联代码 (最小风险, 强烈推荐)

```cpp
// WeaselServer/PhrasesDialog.cpp line 671-675 改为:
if (s_hInput && hfUi) {
  SendMessageW(s_hInput, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  // v0.19.0.46: 删除 ImmCreateContext + ImmAssociateContext。
  // TSF shim (weaselx64.dll) 自动给 hwnd 配 system default IME context
  // (跟 WeaselTSF/WeaselServer 主编辑框同路径), isolated HIMC 在 TSF
  // 进程下反而阻塞 IME。line 671-675 走历史弯路 4 个版本 (L97/L103
  // 候选), 4 次装机复现, 改回"裸 CreateWindowExW"路径最稳。
}
```

**预期**: 装机后中文输入恢复 (跟 v0.19.0.32 之前未引入 IME 关联代码时一致, 那时候装机 user 没报过"不能输中文")。

**风险**:
- 极低 — 这是 `git revert` `fa196049` 之前的状态
- 单元 test 不需要改 (TestPhrasesDialog 没测 IME 关联, sandbox 不能跑 IME)

**测验证据**:
- L02/L01 没有"裸 EDIT 控件无 IME"的 lessons — 因为 TSF shim 自动配
- v0.19.0.32 ship 之前 (e.g. v0.19.0.30) 装机 user 没报 IME bug

### 方案 B — OnDestroy 显式清理 (保留 context 分配)

```cpp
// OnCreate line 671-675 保留
// 加 OnDestroy handler:
LRESULT PhrasesDialog::OnDestroy(HWND hwnd) {
  // v0.19.0.46: 显式 disassociate + 销毁 isolated himc
  if (s_hInput && IsWindow(s_hInput)) {
    HIMC oldHimc = ImmAssociateContext(s_hInput, NULL);
    if (oldHimc) ImmDestroyContext(oldHimc);
  }
  // ... 原 OnDestroy 后续
}
```

**预期**: 同样恢复中文输入 (TSF 主导), 同时避免 context leak。

**风险**:
- 中等 — 假设"TSF 会在 OnCreate 期间 disassociate our himc" 成立; 如果不成立, 仍 fail
- 跟 L100/L103 lessons 累积矛盾 (per-hwnd isolated HIMC 跟 TSF 互斥是 V0.19.0.43 已经踩过的坑)

### 方案 C — 切到 TSF-native (ITfThreadMgr 接管)

不调 IMM32 系列, 改用 `ITfThreadMgr::AssociateFocus(hwnd, nullptr)` + `ITfInputProcessorProfileMgr` 让 TSF 主导。

**风险**:
- 高 — PhrasesDialog 是普通 dialog, 跟 TSF text service (WeaselTSF.dll) 不是同进程。ITfThreadMgr 跨进程关联语义复杂, 容易引入新 bug
- 改动量大 (新增 ITfThreadMgr 初始化 / release, 加 ~80 行), 不符合"最小修复"原则

### 推荐

**方案 A** — 完全删除 `ImmCreateContext` + `ImmAssociateContext` 调用, 回到 `git revert fa196049` 之前状态。

理由:
- 4 个 ship 版本 (v0.19.0.35/36/43/45) 反复踩同一坑
- 4 次装机 user 反馈"不能输中文"
- v0.19.0.32 之前没这行代码, 装机端 IME 没 fail
- TSF shim (weaselx64.dll) 在 TSF 进程下自动配 system default IME 给所有 child hwnd
- 单元 test 不用改
- 改动最小 (删 5 行)

---

## 3. 配套修改 (方案 A)

| 文件 | 改动 | 估算行数 |
|---|---|---|
| `WeaselServer/PhrasesDialog.cpp` | line 670-676 删 5 行 (HIMC + ImmCreate + ImmAssociate) | -5 |
| `CHANGELOG.md` | 加 v0.19.0.46 entry 标记 IME 修 | +5 |
| `.specify/memory/lessons-learned.md` | 加 L103 (Imm 关联 + TSF 进程互斥) | +30 |
| `_check_install_v2.ps1` | 升 WeaselServer.exe expected md5 + install date | +2 |
| `release/fluxing-0.19.0.46-installer.exe` | 新建 | (binary) |

Test 改动:
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` 不改 — TestPhrasesDialog sandbox 不能验 IME, IME 行为必须装机真测
- 装机端 5 项 verify 加 1 项: 打开 dialog → 点 input → 用微软拼音 / 小狼毫拼音 → **中文候选词出 + Enter 候选词进 input 框** + AddTop 按钮点 → 短语进 ListView

---

## 4. 验证 (装机端 user flow)

按 `verification-before-completion` skill 必跑 5 项:

1. **sanbox TestPhrasesDialog**: `167/167 PASS` (删 5 行不影响 layout / list / 按钮 / reorder)
2. **编译**: `WeaselServer.vcxproj` Release Win32 Exit 0
3. **`_check_install_v2.ps1` Module 1+2 md5** = 新 binary md5 (例如 `xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`)
4. **装机 (silent)**: `taskkill + reg delete + installer /S /D=...` 命令照 L13/L54/L66 强约束
5. **真 user flow 5 项**:
   - 打开 dialog → 列表显示
   - 点 input → focus 切到 input (UI 视觉确认)
   - 切到微软拼音 / 小狼毫 → **打"ni"** → 候选词出"你/尼/泥"等
   - **按数字键 1 选第一个候选词** → "你" 进 input 框 (核心验收)
   - 点 AddTop → "你" 进 ListView (端到端 commit 成功)

如果第 5 步 5 项全 pass → v0.19.0.46 ship 候选。

---

## 5. lessons-learned L## 候选 (待写)

`L##-PhaseH-IME-TSF-Mutex`:
- 症状: 4 个 ship 版本 (v0.19.0.35/36/43/45) 装机端"输入框不能输中文"
- 根因: `ImmCreateContext` + `ImmAssociateContext` 在 TSF 进程下创建 isolated HIMC, 跟 weaselx64.dll (TSF shim) 互斥 → s_hInput 锁在 isolated himc, TSF 不接管
- 修法: 完全删除 IMM32 系列调用, 让 TSF shim 自动配 system default IME context
- 防重犯: 任何 TSF 进程下的 hwnd **不要**自己创建 isolated HIMC; 让 system TSF shim 主导
- Pattern-Key: `ime.tsf-process-association` (新增)
- Recurrence-Count: 4 (累计触发 4 次 ship, 远超 L1 门槛)

---

## 6. 一句话总结

> v0.19.0.45 装机端「输入框不能输中文」是 `PhrasesDialog.cpp:671-675` 的 `ImmCreateContext` + `ImmAssociateContext` 在 TSF shim 进程下跟 system TSF IME 互斥; **推荐方案 A**: 删 5 行, 让 TSF shim 自动配 system default IME context。装机 user flow 验收 (打拼音 → 候选词出 → Enter 上屏) 是 ship 前的必跑门。

---

**等待用户授权**:
1. 是否同意走方案 A (删 5 行)?
2. 同意后, 我会:
   - Edit `WeaselServer/PhrasesDialog.cpp` 删 line 670-676 (含 5 行 IME 关联 + 周边 1 行注释)
   - 编译 + 跑 TestPhrasesDialog (sandbox verify)
   - 写 `report-4bug-v0.19.0.46-ime-fix.md` + CHANGELOG + lessons-learned L##
   - 用 `commit 2d842fc` 一样的流程: source commit + release commit + installer commit
   - 你装机验证 5 项 user flow

如果想走方案 B (OnDestroy cleanup) 或方案 C (TSF-native), 也告诉我, 我按对应路径写。

---

— END OF REPORT —
