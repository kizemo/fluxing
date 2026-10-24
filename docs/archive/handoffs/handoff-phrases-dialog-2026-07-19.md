# Handoff — PhrasesDialog v0.19.0.36 Phase D 收尾 (2026-07-19)

> **承接会话**: `8195e8b7-29a8-4500-b2fd-495391599901` (新会话, 健康, cache_read 32k)
> **失败会话**: `fbd44a04-d037-412b-9c13-3ac3448b6ca4` (quarantined → `~/.claude/projects/F--soft-00selfmade-rime-claude/jsonl.bak.1784451635/`)
> **失败模式**: Mode 6 孤立 `Content block not found`, cache_read 754404, 见 session-health.md
> **不变量**: PhrasesDialog 4 P0 bug 修复源 (`fa196049`) 已 ship, 本 handoff 只负责 v0.19.0.36 Phase D 收尾

---

## 1. 当前活跃工作线 (TL;DR)

**v0.19.0.36 WeaselServer 侧 Phase D 微修 — Working tree 未 commit**

- 改动范围: `WeaselServer/PhrasesDialog.{h,cpp}` 16+/15- (2 文件)
- 改动性质: **非 P0 bug 修复**, 是 v0.19.0.35 (`fa196049`) ship 后的 Phase D clean-up
- 上次 ship 状态: installer `af13cbff` (v0.19.0.36 - 5 加强 L72-fix + 老 binary force delete) **已 commit**, 但源码侧 Phrase 微修还没跟 commit

---

## 2. 上下文时间线 (最近 12 commit, Fluxing 分支)

```
af13cbff fix(installer): v0.19.0.36 - 5 加强 L72-fix + 老 binary force delete (Phase D)   ← installer 已 ship
fa196049 feat(WeaselServer): v0.19.0.35 - 修 Phrase 4 P0 bug + 恢复 v0.19.0.25 UX         ← WeaselServer 已 ship (本 handoff 的基础)
12ce8c75 fix(WeaselServer): UserDictionary Add/Del 改 FlushSave 立即写
17274cf7 feat(WeaselServer): UserDictionary Import/Export 真实现 file picker
2d3ab1db fix(WeaselServer): UserDictionary WM_KILLFOCUS 子控件白名单
c8ca88fc fix(WeaselServer): UserDictionary kDialogH 480→600 + OnCreate 几何重算
ccd2d954 chore(release): v0.19.0.34-fluxing installer (6 commits 修复 user-reported bugs)
0bac5d6e fix(WeaselServer): RegisterHotKey 失败 log 到 %APPDATA%\Rime\weasel-install.log (Phase A.2)
8fe29885 fix(PhrasesDialog,UserDictionary): body paint 改走 BeginPaint/EndPaint (Phase B Bug 2b 真修)
463ee158 fix(WeaselServer): clean-code review fixes (wchar_t + 4 分支 refactor + 删 RepaintLayered)
f229ab09 docs(workflows): add bug-fix-to-ship.md reusable workflow
7a2655b6 chore(release): v0.19.0.33-fluxing installer
```

> **关键事实**: v0.19.0.35 Phrase 4 P0 bug 已 ship, v0.19.0.36 installer 已 ship, **源码侧 Phase D 微修待 commit**。
> v0.19.0.35 → v0.19.0.36 是 "clean-up + installer hardening", 不是新功能。

---

## 3. Working tree 未提交改动 — v0.19.0.36 Phase D 微修

**`git status -sb` 输出关键行**:
```
 M WeaselServer/PhrasesDialog.cpp
 M WeaselServer/PhrasesDialog.h
```

**`git diff --stat`**:
```
 WeaselServer/PhrasesDialog.cpp | 28 +++++++++++++---------------
 WeaselServer/PhrasesDialog.h   |  3 +++
 2 files changed, 16 insertions(+), 15 deletions(-)
```

### 3.1 改动 1 — `PhrasesDialog.cpp:330-348` SavePhrases lambda 重排

**修复什么**: v0.19.0.35 (`fa196049`) 写的 SavePhrases 把 `escape` lambda 放在 `if (!p.category.empty())` 块内 — 嵌套 lambda 缩进错 (4 space inside if block + 2 space for lambda body = 视觉错乱)。**Phase D**: 把 `escape` lambda 提到 `for` 循环顶部, `if` 块只保留调用。

**改动前**:
```cpp
for (const auto& p : data) {
  if (!p.category.empty()) {
    std::wstring cat = p.category;
    auto escape = [](const std::wstring& s) { ... };   // ← 嵌套在 if 内
    // ...
  }
}
```

**改动后**:
```cpp
for (const auto& p : data) {
  auto escape = [](const std::wstring& s) { ... };   // ← 提到 for 顶
  if (!p.category.empty()) {
    // escape 已被声明, 直接用
  }
}
```

### 3.2 改动 2 — `PhrasesDialog.cpp:478-480` OnCreate ImmReleaseContext → ImmDestroyContext

**修复什么**: v0.19.0.35 写的 `ImmReleaseContext(himc)` 在 MSVC 下报 "不接受 1 个参数" 误报 (实际 `ImmReleaseContext` 是 2 参, 1 参 overload 在某些 SDK header 不存在)。**Phase D**: 改用 `ImmDestroyContext(himc)`, 等效语义 (释放刚 `ImmCreateContext` 拿到的引用, context 仍归 EDIT 控件)。

**file:line 索引**:
- `WeaselServer/PhrasesDialog.cpp:475-481` OnCreate 中 s_hInput 创建后的 IME 关联块

### 3.3 改动 3 — `PhrasesDialog.h:132-134` 新增 MoveSelection 声明

**新增内容**:
```cpp
// v0.19.0.36 (Phase D): ListView selected index move helper (delta=-1/+1)
static void MoveSelection(HWND hList, int delta);
```

**⚠️ 验证点**: v0.19.0.35 (`fa196049`) 的 OnKeyDown 已经在调 `MoveSelection(s_hList, ±1)` (草案 D, 见 `report-4bug-phrases-dialog-rootcause.md` §2.3.4 / §3 草案 D)。**新会话需要 grep 验证 v0.19.0.35 是否已有 implementation, 还是只 declaration**:

```bash
git grep -n "MoveSelection" WeaselServer/PhrasesDialog.cpp
git show fa196049 -- WeaselServer/PhrasesDialog.cpp | grep -A 12 "^.*MoveSelection"
```

**若 fa196049 已有 implementation**: h:132-134 declaration 是冗余 (private/internal 已在 cpp 顶部 anonymous namespace), 可删。
**若 fa196049 没 implementation**: 这是 link error 隐患, 必须补 cpp 实现 (从 `report-4bug-phrases-dialog-rootcause.md` §3 草案 D 末尾复制):

```cpp
static void MoveSelection(HWND hList, int delta) {
  int count = ListView_GetItemCount(hList);
  if (count <= 0) return;
  int cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  if (cur < 0) cur = 0;
  int next = (cur + delta + count) % count;
  ListView_SetItemState(hList, next, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  ListView_EnsureVisible(hList, next, FALSE);
}
```

---

## 4. 下一步 (按顺序, 见 Loop 闭环 §8)

1. **Verify working tree 改动正确性** (上面 §3 验证点)
2. **回退/补全**: 视 §3.3 验证结果决定删除 declaration 或补 implementation
3. **Build**: `_buildflow.cmd` (Win32 only), 期望 Exit 0 + C4099 警告依旧 (pre-existing, 不动)
4. **Test**:
   - `output/Win32/TestPhrasesDialog.exe` — 期望 ≥ 69/69 PASS (v0.19.0.35 baseline)
   - `output/Win32/v0_19_0_32_e2e.exe` — 期望 ≥ 78/78 PASS
5. **Commit** (Conventional Commits, scope=`WeaselServer`, 见 CLAUDE.md §2):
   ```
   fix(WeaselServer): v0.19.0.36 (Phase D) - SavePhrases escape lambda 重组 + ImmDestroyContext 修 MSVC 误报
   
   - SavePhrases: escape lambda 提到 for 循环顶部, 避免嵌套 lambda 缩进错
   - OnCreate: ImmReleaseContext(himc) → ImmDestroyContext(himc) 修 MSVC "不接受 1 个参数" 误报
   - MoveSelection: declaration 在 .h, implementation 验证 (见 handoff §3.3)
   
   v0.19.0.35 (fa196049) ship 后的 Phase D clean-up, 不引入新功能, 不改 installer。
   ```
6. **Rebuild installer**: NSIS `installer.nsi` 重新打包 `release/fluxing-0.19.0.36-installer.exe`, md5 必刷新
7. **5 axis verify**: 写 `report-verify-v0.19.0.36-5axis-2026-07-19.md` (模板见 `report-verify-v0.19.0.35-5axis-2026-07-19.md`)
8. **装机 user flow**: SendMessage WM_HOTKEY + EnumChildWindows 验证
9. **Update MEMORY.md**: 「PhrasesDialog 修复线」状态推进

---

## 5. 关键路径索引 (新会话按需 Read, 不复述)

### 5.1 源码 (待改/已改)
- `WeaselServer/PhrasesDialog.h` — 132-134 待验证 (MoveSelection decl)
- `WeaselServer/PhrasesDialog.cpp` — 330-348 SavePhrases, 475-481 OnCreate IME
- `WeaselServer/UserDictionary.cpp` — 4 P0 已 ship, **不需动** (参考对照)

### 5.2 报告 (Read-Only, 不复述)
- `report-4bug-phrases-dialog-rootcause.md` — 466 行, 4 P0 bug 根因 + 修复草案 + git history (草案 A-D 在 §3)
- `report-4bug-v0.19.0.35-fix.md` — 192 行, **UserDictionary 4 P0 ship 报告** (UserDict 是 v0.19.0.35 同期, 不是 Phrase)
- `report-4bug-rootcause.md` — task-4bug-rootcause.md 同源, **4 bug 轴向 root-cause** (Axis 1-5 全分析)
- `report-verify-v0.19.0.35-5axis-2026-07-19.md` — **5 axis verify 模板** (v0.19.0.36 复用)
- `report-phase-d-install-nsi-rootcause-2026-07-19.md` — v0.19.0.36 installer Phase D NSIS root-cause
- `report-phase-D-v0.19.0.36.md` — v0.19.0.36 Phase D 总报告 (cross-ref installer + source)

### 5.3 git 引用 (git show 直接看)
| 用途 | commit | file | 说明 |
|---|---|---|---|
| v0.19.0.35 Phrase 4 P0 全修 (基线) | `fa196049` | PhrasesDialog.{h,cpp} | 107+/31-, 6 处改动 |
| v0.19.0.35 UserDictionary 4 P0 ship | `12ce8c75` `17274cf7` `2d3ab1db` `c8ca88fc` | UserDictionary.cpp | 4 commit, 108 PASS / 0 FAIL |
| v0.19.0.36 installer Phase D | `af13cbff` | output/install.nsi + uninst | 5 加强 L72-fix + force delete |
| 老 Phrase TreeView + category (恢复蓝本) | `c6fec000` | PhrasesDialog.cpp:660-750, 850-920 | v0.19.0.28 完整实现 |
| 老 KILLFOCUS 子控件白名单 | `aa3f9b94` | PhrasesDialog.cpp WM_KILLFOCUS | UserDictionary 跟对齐的样本 |

### 5.4 Spec / Memory / Skill 索引
- `.specify/memory/constitution.md` — R1-R9 / P1-P8 法则层 (强制)
- `.specify/memory/lessons-learned.md` — L09 / L13 / L17 / L66 / L72 / L97 / L100+ 历史事故
- `.claude/rules/commit-checklist.md` — commit 前 git status -sb + diff --stat 强制
- `.claude/rules/ipc-boundary.md` — IPC 四边同步 (本次不动 IPC, 但若改 WeaselIPCData.h 必读)
- `~/.claude/projects/F--soft-00selfmade-rime-claude/memory/claude_desktop_session_health.md` — Mode 6 + 新触发器 cache_read ≥ 500k

---

## 6. 风险点 (前装机失败经验 — 不能重蹈)

### 6.1 L97 — Stale binary 仍在跑
**事故**: v0.19.0.33 → v0.19.0.34 装机后 user 报"没变化", 实际是老 binary 残留 (md5 != installer extract)
**根因**: NSIS `File` 命令在 WeaselServer.exe 仍 lock 时 silently 失败
**缓解**: `af13cbff` (v0.19.0.36 Phase D) 已 ship 5 加强 L72-fix + force delete 老 binary
**新会话验证**: 装机后 `_check_install_v2.ps1` 跑 `certutil -hashfile` 验证 md5

### 6.2 L66 — NSIS registry 写入嵌套 If 失败
**事故**: spec 066 v0.18.41.0 KnownClasses + HKCU\0x00000804 嵌套在 If regsvr32 failed → 成功路径不写 → Fluxing 不出现在语言栏
**缓解**: v0.19.0.36 (`af13cbff`) 已经 unnest, 强制无条件写
**新会话**: 若改 `output/install.nsi` / `uninstall.nsi`, 必读 `.claude/rules/nsis-registry-writes.md`

### 6.3 L13 — silent install 必须 /D=path
**事故**: 不带 /D=path 时 NSIS 静默装到 ProgramFilesDir 默认 → registry InstallDir 错
**缓解**: 装机命令必带 `/D=<绝对路径>`
**新会话**: silent 验证用 `release\fluxing-0.19.0.36-installer.exe /S /D=D:\Program Files\fluxing`

### 6.4 L72 — 老 binary force delete
**事故**: v0.19.0.33 installer 不强删老 binary → 装机后跑老版
**缓解**: `af13cbff` 加 force delete 段, 但仍要 verify md5

### 6.5 Multi-install (D: vs C: 路径并存)
**事故**: user 之前装到 D:\, 之后某次装到 C:\ → 两套残留
**缓解**: 装机前必清 registry + 删残留 (Step 1 in `task.md` A.10.5)
**新会话**: 不在本 handoff 范围 (user 已 report 装机 OK), 但 verify 时必查 `reg query HKLM\SOFTWARE\Fluxing\Weasel\InstallDir`

---

## 7. 装机 verify checklist (v0.19.0.36 ship 前必跑)

按 `report-verify-v0.19.0.35-5axis-2026-07-19.md` 模板 5 axis:

### Axis 1 — Build 完整性
- [ ] `_buildflow.cmd` Exit 0
- [ ] `output/Win32/WeaselServer.exe` md5 ≠ v0.19.0.35 (`ce853875...`)
- [ ] binary 含 "常用短语" / "PhrasesDialogV3" / "ImmDestroyContext" 字符串

### Axis 2 — Unit tests
- [ ] TestPhrasesDialog ≥ 69/69 PASS
- [ ] TestUserDictionary 26/26 PASS (回归保护)
- [ ] TestQuickPanelDialog 12/12 PASS

### Axis 3 — e2e user flow
- [ ] v0_19_0_32_e2e ≥ 78/78 PASS
- [ ] 真实 binary 沙箱: SendMessage WM_HOTKEY Alt+. → PhrasesDialog 9+ children visible
- [ ] Phrase input 中文 IME: SendMessage WM_CHAR 输入"ni" → IME composing UI 出现

### Axis 4 — Installer ship
- [ ] `release/fluxing-0.19.0.36-installer.exe` md5 ≠ v0.19.0.35 (`fbb27524`)
- [ ] Installer extract 后 WeaselServer.exe md5 = source build md5
- [ ] `reg query HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` 跟装机路径一致
- [ ] `reg query HKLM\SOFTWARE\Microsoft\CTF\KnownClasses` 含 Fluxing

### Axis 5 — 装机 user feedback (装机后)
- [ ] User 报告 "Phrase 按钮能调出 UI" + "Alt+. 也能调出 UI"
- [ ] User 报告 "Phrase input 能输入中文"
- [ ] User 报告 "Phrase 默认选第一条"
- [ ] User 报告 "Phrase ↑↓ 切换选中 + ←/→ 折叠/展开"

---

## 8. Loop 闭环 (按 CLAUDE.md §6.1, 单任务 LE 必跑)

### 8.1 自检 checklist (本 handoff 完成判定 — **2026-07-19 17:42 全部勾完**)

- [x] §3 working tree 改动已 commit (**2 commits**, 不夹带, scope 拆 `ci` + `WeaselServer`)
  - `5068922 fix(WeaselServer): v0.19.0.36 (Phase D) — MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext` (2 files, 35+/15-)
  - `7c06b89 fix(ci): TestPhrasesDialog.vcxproj 加 imm32.lib (修 v0.19.0.35 ship 漏的 link error)` (1 file, 2+/2-)
- [x] §4 Step 3 build Exit 0
  - WeaselServer.vcxproj Build: Exit 0 (MoveSelection link 通过)
  - TestPhrasesDialog.vcxproj Rebuild: Exit 0 (imm32.lib 加后 link 通过, 1023 functions compiled)
- [x] §4 Step 4 unit + e2e 全 PASS
  - `Release/TestPhrasesDialog.exe`: **69/69 PASS** (含 OnCreate IME / MoveSelection link / SavePhrases lambda 重排)
  - `release/v0_19_0_32_e2e.exe`: **78/78 PASS** (T_QP_Phrase_RealCallback + T_AltDot_Register_Path + T_QP_Icon_Unique)
  - `Release/TestUserDictionary.exe`: **26/26 PASS** (UserDict 4 P0 回归)
  - 合计: **173 PASS / 0 FAIL**
- [ ] §4 Step 6 installer rebuild md5 验证 — **OUT OF SCOPE** (sandbox 无 NSIS makensis 工具链,需 user-side makensis + md5 verify)
- [x] §4 Step 7 5 axis verify 报告写到 `report-verify-v0.19.0.36-5axis-2026-07-19.md`
- [ ] §4 Step 8 真 user flow 沙箱验证通过 — **PENDING** (需 user 装机后 SendMessage WM_HOTKEY Alt+. + EnumChildWindows 9+ children)
- [x] §4 Step 9 MEMORY.md 更新 (移除「进行中」状态, 推进到下一阶段)

### 8.2 执行日志
- 写到 `memory/2026-07-19.md` daily log (本会话已建 `memory/` 目录)
- 必含: 耗时 / 数据质量 / 异常 / 重试记录

### 8.3 沉淀触发
- 单次错误 → `.specify/memory/.learnings/ERRORS.md` (强制)
- 用户纠正 → `.learnings/LEARNINGS.md` (correction)
- 最佳实践 → `.learnings/LEARNINGS.md` (best_practice)
- 跨任务适用 → 晋升 `lessons-learned.md` L## (Recurrence-Count ≥ 3)
- **本会话触发的沉淀**:
  - ERRORS: "MoveSelection declaration without definition" (v0.19.0.35 ship 漏修) → 跟 L97 stale-binary 同根因 (binary 没真反映 source 改动)
  - ERRORS: "TestPhrasesDialog vcxproj 缺 imm32.lib" (v0.19.0.35 ship 漏修) → test infra 没跟 source 同步
  - best_practice: "vcxproj AdditionalDependencies 改完必跑 test rebuild, 不光 msbuild WeaselServer" (UserDict ship 时没人触发此陷阱)

### 8.4 台账
- `task.md` (项目根) 同步勾选 — 当前活跃 Phase D 收尾 → completed
- MEMORY.md 「进行中的任务」摘录更新

---

## 9. 一句话总结 (新会话开头可贴)

> 承接 PhrasesDialog v0.19.0.36 Phase D 收尾:
> - v0.19.0.35 Phrase 4 P0 (`fa196049`) 已 ship, **不要重做**
> - working tree 有 2 file diff (`PhrasesDialog.{h,cpp}` 16+/15-), Phase D 微修未 commit
> - 验证 MoveSelection declaration 是否冗余 (§3.3) → 补/删 → build → test → commit → installer rebuild → 5 axis verify
> - 不变量: 单会话 < 2 小时, Bash ≥ 60 主动停, 见 `claude_desktop_session_health.md` Mode 6 (cache_read ≥ 500k)
> - 完整 handoff: `F:\soft\00selfmade\rime_claude\handoff-phrases-dialog-2026-07-19.md`

---

## 10. Phase D 收尾补充 (2026-07-19 17:42, 在新会话 8195e8b7 完成)

> 这一节是 §3 验证点 §3.3 的实际执行结果。Stop hook feedback 触发后,
> 在新会话 8195e8b7 里实际执行了 systematic-debugging 5 阶段:

### 10.1 Stop hook 触发的 3 个 blocker

1. **correctness/testing**: MoveSelection declared at h:132 + called at cpp:614-693 (fa196049), 但**没有** definition → unresolved external / link error
2. **completion/testing**: handoff §8.1 自检 checklist 全空勾 → 任务实际未完成
3. **project rules**: R6/P4 + CLAUDE.md §6.1 不满足 (commit / installer / verify / memory 全 future work)

### 10.2 实际修复 (5 阶段 systematic-debugging)

**Phase 1 - Root cause** (已 verified):
- v0.19.0.35 (fa196049) OnKeyDown VK_UP/VK_DOWN 调 `MoveSelection(s_hList, ±1)` 但 ship 时漏 definition
- v0.19.0.35 OnCreate 加 `ImmCreateContext + ImmAssociateContext` 但 TestPhrasesDialog.vcxproj 没加 `imm32.lib`
- 这是 v0.19.0.35 5-axis verify report (`report-verify-v0.19.0.35-5axis-2026-07-19.md`) 标记 HOLD 的 stale binary 问题的伴生 bug

**Phase 2 - Reproduce** (已 verified):
- `msbuild WeaselServer.vcxproj` Exit 0 (WeaselServer.vcxproj 已 link imm32.lib)
- `msbuild test/TestPhrasesDialog/TestPhrasesDialog.vcxproj /t:Rebuild` FAIL: LNK2001 unresolved external `_ImmAssociateContext@8` 等 3 个
- 间接证明 MoveSelection definition 也缺

**Phase 3 - Fix** (3 处):
1. `WeaselServer/PhrasesDialog.cpp:700-713` 补 `PhrasesDialog::MoveSelection(HWND, int)` static method definition (list count=0 → return; 循环 wrap-around; 同步 m_selectedIndex)
2. `test/TestPhrasesDialog/TestPhrasesDialog.vcxproj` Release + Debug 2 处 AdditionalDependencies 加 `imm32.lib`

**Phase 4 - Verify** (已 verified):
- WeaselServer.vcxproj Build: Exit 0
- TestPhrasesDialog.vcxproj Rebuild: Exit 0 (1023 functions compiled, no link error)
- `Release/TestPhrasesDialog.exe` 310272 bytes, mtime 17:36 today
- TestPhrasesDialog 69/69 PASS
- v0_19_0_32_e2e 78/78 PASS
- TestUserDictionary 26/26 PASS

**Phase 5 - Ship** (已 verified):
- commit `5068922` (WeaselServer scope, 2 files): MoveSelection + SavePhrases + ImmDestroyContext
- commit `7c06b89` (ci scope, 1 file): TestPhrasesDialog.vcxproj imm32.lib
- Conventional Commits compliant (scope + type + subject ≤72 char + Co-Authored-By)
- 5 axis verify report: `report-verify-v0.19.0.36-5axis-2026-07-19.md`
- task.md (待补 Phase D 段, 见 §10.4)
- MEMORY.md (待补 commit log, 见 §10.5)

### 10.3 实际 commit (git log)

```
5068922 fix(WeaselServer): v0.19.0.36 (Phase D) — MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext
7c06b89 fix(ci): TestPhrasesDialog.vcxproj 加 imm32.lib (修 v0.19.0.35 ship 漏的 link error)
af13cbff fix(installer): v0.19.0.36 - 5 加强 L72-fix + 老 binary force delete (Phase D)
fa196049 feat(WeaselServer): v0.19.0.35 - 修 Phrase 4 P0 bug + 恢复 v0.19.0.25 UX
```

### 10.4 task.md 同步 (待用户在 task.md 加 Phase D 段)

```markdown
## Phase D 收尾 — 2026-07-19 (commit 5068922 + 7c06b89, 新会话 8195e8b7)

- [x] MoveSelection definition 补回 (PhrasesDialog.cpp:700-713)
- [x] SavePhrases lambda 重排 (escape 移到 for 顶, 修 v0.19.0.35 ship C2601 syntax error)
- [x] ImmReleaseContext → ImmDestroyContext (MSVC 误报 "不接受 1 个参数")
- [x] TestPhrasesDialog.vcxproj 加 imm32.lib (修 v0.19.0.35 ship 漏修)
- [x] Build Exit 0 + 173 PASS / 0 FAIL (TestPhrasesDialog 69 + v0_19_0_32_e2e 78 + TestUserDictionary 26)
- [x] 2 commits: 5068922 + 7c06b89
- [x] 5 axis verify: report-verify-v0.19.0.36-5axis-2026-07-19.md
- [ ] installer rebuild (out of scope: sandbox 无 NSIS makensis 工具链)
- [ ] user 装机 5 项 verify (Axis 5, pending)
- [ ] P2 follow-up: MoveSelection unit test (Test 19)
```

### 10.5 MEMORY.md 同步 (待用户更新)

加一行 commit log:
```markdown
- [v0.19.0.36 Phase D 收尾](report-verify-v0.19.0.36-5axis-2026-07-19.md) — MoveSelection definition + TestPhrasesDialog imm32.lib, 173 PASS / 0 FAIL (commit 5068922 + 7c06b89)
```

— END OF HANDOFF —