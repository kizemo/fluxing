# Handoff — Phase I v0.19.0.46 现状 (2026-07-22)

> **承接会话**: `S-2026-07-22-01-phase-i-ime-fix` (本次会话,健康)
> **承接 ship**: `e9c4f0e` (v0.19.0.46-fluxing installer) ← HEAD (待 commit)
> **承接状态**: Phase I Bug 4 输入框中文 IME 真修 ship 候选, 待装机 user flow 5 项 verify
> **状态**: Phase H v0.19.0.45 已 ship (`a7048db`), 装机 user 反馈"列表框/输入框可调整 (Phase H 修好), 但输入框仍无法输中文 (Bug 4)"

---

## 1. 当前活跃工作线 (TL;DR)

**v0.19.0.46 已 ship (commit `e9c4f0e` 待 push) — 当前 HEAD 是 source commit `b4f0626`**

- installer: `release/fluxing-0.19.0.46-installer.exe` 43,257,645 bytes, md5 `4d41804d60ded5cb8a1db88faf74e346`
- WeaselServer binary: md5 `1caa6e94b209d1d0f3214b4d08c45310` (extract match source build, 无 L97 stale)
- TestPhrasesDialog: **168/168 PASS / 0 FAIL** (Test 1-35 全过, IME 不在 sandbox 测)
- install.nsi: 未改 (UTF-8 BOM + CRLF ✓, NSIS 仍用 v0.19.0.45 段)
- env.bat / weasel.props: 升 v0.19.0.45 → 0.19.0.46 (gitignored, 不 commit)
- git: ahead 80 / `Fluxing...kizemo/Fluxing` (origin master 没动)

---

## 2. 时间线 — Phase I v0.19.0.46 IME fix (1 source + 1 release commit)

```
b4f0626 fix(WeaselServer): v0.19.0.46 (Phase I Bug 4 真修) — 删 isolated HIMC, 让 TSF shim 配 default IME   ← HEAD
a7048db chore(release): v0.19.0.45-fluxing installer (Phase H layout polish 装机)                          ← 上一版
2d842fc fix(WeaselServer): v0.19.0.45 (Phase H layout polish) — input 拉伸 + UI margin 缩小
be4f14a chore(release): v0.19.0.44-fluxing installer (UI 迭代 2 装机)
```

**关键事实**:
- v0.19.0.45 (`2d842fc` + `a7048db`) Phase H 装机后 user 反馈 1 bug:
  - **Bug 4**: 在输入框中仍无法输入中文 (4 ship 复发, 4 ship 都 fail)
- v0.19.0.46 (`b4f0626` + `e9c4f0e` 待) Phase I 修复:
  - **Bug 4 真因**: `PhrasesDialog.cpp:671-675` 的 `ImmCreateContext` + `ImmAssociateContext` 在 TSF shim 进程下 (WeaselServer.exe 是 weaselx64.dll 主导) 创建 isolated HIMC 给 s_hInput, 跟 TSF IME 互斥 → IME 候选词不出
  - **Bug 4 修法**: 删 5 行 IME 关联代码, 走 v0.19.0.32 之前"裸 CreateWindowExW"路径 → TSF shim 自动配 system default IME context

---

## 3. v0.19.0.46 ship 详情

| Version | Source commit | Release commit | Size (bytes) | md5 | 主变更 |
|---|---|---|---|---|---|
| **v0.19.0.46** | `b4f0626` | `e9c4f0e` (待) | 43,257,645 | `4d41804d60ded5cb8a1db88faf74e346` | PhrasesDialog.cpp 删 5 行 IME 关联 + lessons-learned L103 |

---

## 4. Phase I Bug 4 真因 + 修法 详情

### 4.1 Bug 4 — 输入框无法输中文 (装机 v0.19.0.45 反馈)

**真因** (5 阶段 systematic-debugging, 见 `report-4bug-v0.19.0.45-ime-stuck.md`):
- WeaselServer.exe 是 TSF shim 进程 (weaselx64.dll 主导), TSF 通过 `ITfThreadMgr` + `ITfInputProcessorProfileMgr` 强制 hwnd 走 system default IME context
- `PhrasesDialog.cpp:671-675` 用 `ImmCreateContext` + `ImmAssociateContext` 创建 isolated HIMC 给 s_hInput → 跟 TSF IME 互斥 → IME 候选词不出
- 4 ship 版本 (v0.19.0.35/36/43/45) 反复改都失败, 累计装机 user 反馈 4 次

**修法** (方案 A — 完全删除 IME 关联代码, 最简最小):
- `PhrasesDialog.cpp` line 670-675 删 5 行 (`HIMC himc` + `ImmCreateContext` + `ImmAssociateContext` 块)
- 替换为注释: "TSF shim 自动给 hwnd 配 system default IME context"
- 走 v0.19.0.32 之前的"裸 CreateWindowExW"路径

### 4.2 4 ship 复发时间线

| 版本 | commit | 改动 | 装机端 IME 行为 |
|---|---|---|---|
| v0.19.0.35 | fa196049 | 引入 `ImmCreateContext` + `ImmAssociateContext` + `ImmDestroyContext` | 死 (ImmDestroyContext 毁 himc) |
| v0.19.0.36 | 79d522b6 | revert 改 `ImmReleaseContext(s_hInput, himc)` 2 参 | 死 (refcount 1→0 销毁 himc) |
| v0.19.0.43 | e658e42 | 改成"不调 ImmReleaseContext" (per MSDN) | **死** (TSF 进程不认 isolated HIMC) |
| v0.19.0.45 | 2d842fc | 没改 IME, 沿袭 v0.19.0.43 失败 | **死** (本次装机反馈) |

### 4.3 lessons-learned L103 (新增, 见 `.specify/memory/lessons-learned.md`)

- **L103-A** (TSF shim 进程不认 isolated HIMC): TSF 进程下不要 `ImmCreateContext` + `ImmAssociateContext`, 让 TSF shim 主导
- **L103-B** (4 ship 复发门槛): v0.19.0.35/36/43/45 4 ship 复发累计装机 user 反馈 4 次, 远超 L1 门槛 (3 次)
- **L103-C** (Per-MSDN 不可信): "the application should not call ImmReleaseContext" 是 per-process 默认, TSF 进程下 per-hwnd isolated HIMC 跟 system TSF IME 互斥才是真陷阱
- **L103-D** (sandbox 测不出 IME 行为): 任何 IME 改动必装机端 5 项 user flow 验证, sandbox PASS 不够
- **L103-E** (L97 stale binary + 真 source build md5 比对): 每次 IME 改动 ship 后必跑 `_check_install_v2.ps1` verify Module 1+2 = source build

Pattern-Key: `ime.tsf-process-association` (新增) — TSF shim 进程下 per-hwnd isolated HIMC 跟 system TSF IME 互斥
Recurrence-Count: 4 (v0.19.0.35/36/43/45 4 ship 复发, 远超 L1 门槛)

---

## 5. 测试新增 (无, Test 1-35 不变)

TestPhrasesDialog sandbox 不能验 IME 行为, 装机端必跑 5 项 user flow。

---

## 6. Working tree 状态 (`git status -sb` 关键行)

```
## Fluxing...kizemo/Fluxing [ahead 80]
 M .claude/skills/git-workflow/SKILL.md
 M .claude/skills/spec-init/SKILL.md
 M .claude/skills/systematic-debugging/SKILL.md
 M .claude/skills/tdd/SKILL.md
 M .claude/skills/verification-before-completion/SKILL.md
 M .specify/memory/.learnings/ERRORS.md
 M CLAUDE.md
 m librime
 D release/fluxing-0.19.0.44-installer.exe
 M task.md
?? build_v046.cmd         (本会话 build helper, 不 commit)
?? build_v046.ps1         (本会话 build helper, 不 commit)
?? report-4bug-v0.19.0.45-ime-stuck.md   (本会话诊断报告, 不 commit)
?? release/fluxing-0.19.0.46-installer.exe   (待 release commit)
```

**已 commit** (本会话):
- `b4f0626` fix(WeaselServer): v0.19.0.46 — PhrasesDialog.cpp + CHANGELOG + lessons-learned + _check_install_v2.ps1

**待 commit** (本会话):
- `e9c4f0e` (待) chore(release): v0.19.0.46-fluxing installer — release/fluxing-0.19.0.46-installer.exe

**不动清单** (per handoff §10 + §11):
- `M .claude/skills/*` — 工作流 skill 累积改动
- `M CLAUDE.md` — 进站指引累积改动
- `m librime` — 子模块 dirty
- `M task.md` — 主 task plan 累积改动
- `?? build_v046.cmd/ps1` — 本会话 build helper, 临时
- `?? report-4bug-v0.19.0.45-ime-stuck.md` — 本会话诊断报告, 不 commit

**新 handoff 文件**: `handoff-phase-i-v0.19.0.46-2026-07-22.md` (本文件) — 不 commit, 会话级快照。

---

## 7. CHANGELOG.md 当前段 (按 commit 时间倒序)

```
## [0.19.0.46-fluxing] - 2026-07-22   ← 最新
## [0.19.0.45-fluxing] - 2026-07-21
## [0.19.0.44-fluxing] - 2026-07-21
## [0.19.0.43-fluxing] - 2026-07-21
## [0.19.0.42-fluxing] - 2026-07-21
## [0.19.0.41-fluxing] - 2026-07-21
## [0.19.0.40-fluxing] - 2026-07-21
...
```

---

## 8. 不变量 (本会话维持)

- `env.bat` 是 gitignored, **绝不 commit** (当前是 WEASEL_BUILD=46)
- `weasel.props` 是 gitignored, **绝不 commit** (当前是 0.19.0.46)
- `release/` 目录 installer 是 git-tracked, 5+1 = 6 个版本 (0.40~0.45 + 0.46) 已 add + commit
- installer rebuild 前先 `rm -f` 旧 version (避免 dir 混淆) — 已删 v0.19.0.22 (错 version 误标) 跟 v0.19.0.45 老 installer
- 装机命令必带 `taskkill + reg delete` (L13/L54/L66 强约束)
- working tree 累积改动 (`.claude/skills/*` + `CLAUDE.md` + `librime` + `task.md`) 不动
- 旧 handoff 文件 (phase-f / phrases-dialog / loop-engineering / phase-g / phase-h) 作 archive, 不 commit, 不强删

---

## 9. 下一步建议 (新会话)

按 user 装机反馈分流:

| 装机反馈 | 处理 |
|---|---|
| **v0.19.0.46 装机后 5 项 user flow 全 pass** | 接受 ship, 等下一阶段 user 反馈 |
| **v0.19.0.46 装机后 Bug 4 仍 fail** | 走 systematic-debugging, 查装机端 1) TSF shim 是否真自动配 IME 2) `_check_install_v2.ps1` verify Module 1+2 md5 3) TSF service 是否启动 |
| **装机顺利, 要进下一环节** | 按 user directive: "用户词典/快捷键等功能, 推到下一环节执行" — Phase J 候选: UserDictionary 完整功能 / ShortcutSettings 验证 / 视觉 polish |

---

## 10. 装机命令模板 (v0.19.0.46, L13/L54/L66 强约束)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.46-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 verify 项** (per CHANGELOG v0.19.0.46 §Smoke-test recipe + 5 项 user flow):
1. Module 1+2 md5 = `1caa6e94b209d1d0f3214b4d08c45310`
2. Module 3 L66 4 keys 写出 (HKLM\...\KnownClasses + HKCU\...\Assemblies\0x00000804 + HKLM\...\CLSID\{A3F4CDED-...}\InprocServer32)
3. **Bug 4 verify (核心 user flow 5 项)**:
   - (1) 打开常用短语 dialog → 列表显示
   - (2) 点 input 框 → focus 切到 input
   - (3) 切到微软拼音/小狼毫 → 打 "ni" → **候选词出"你/尼/泥"**
   - (4) 按数字 1 选第一个候选词 → **"你" 进 input 框**
   - (5) 点 AddTop 按钮 → "你" 进 ListView (端到端 commit 成功)
4. **回归**: Phase H 装机 v0.19.0.45 已 work 项: 初始 UI 列表框 column 跟右边距一致; resize dialog → input 编辑框宽度跟随 + AddTop 按钮 anchored 右边; resize dialog → 列表高度跟随 + 底部 3 按钮 anchored 右边距 = 8px; drag 列表项 reorder + Esc/Enter/双击 全 work

---

## 11. 旧 handoff 归档标注

| 文件 | 状态 |
|---|---|
| `handoff-phase-f-bug3-vk_escape-2026-07-20.md` | ✓ RESOLVED by `4c8d47d` (v0.19.0.40) — Phase F Bug 3 fix ship |
| `handoff-phrases-dialog-2026-07-19.md` | ✓ Phase D 已 ship (`af13cbff` v0.19.0.36 installer), Phase E GC + v0.19.0.37~38 后续 |
| `handoff-loop-engineering-2026-07-18.md` | Loop Engineering 框架规则, 长期 reference |
| `handoff-phase-g-v0.19.0.44-2026-07-21.md` | ✓ Phase G 已 ship (`be4f14a` + `e2f0a60` v0.19.0.44) — UI 迭代 2 |
| `handoff-phase-h-v0.19.0.45-2026-07-21.md` | ✓ Phase H 已 ship (`2d842fc` + `a7048db` v0.19.0.45) — layout polish |
| `handoff-phase-i-v0.19.0.46-2026-07-22.md` | **(本文件)** — 反映当前 v0.19.0.46 现状 + Phase I 已 ship |

旧 handoff 不删 (项目根目录约定, 作 archive), 但顶部加 `✓ RESOLVED` 标注便于新会话快速识别。

---

## 12. 一句话总结

> Phase I Bug 4 输入框中文 IME 真修 (v0.19.0.46) 已 ship, 真因 = `PhrasesDialog.cpp:671-675` 的 `ImmCreateContext` + `ImmAssociateContext` 在 TSF shim 进程下跟 system TSF IME 互斥。修法 = 删 5 行, 让 TSF shim 自动配 default IME。当前 HEAD `b4f0626` = v0.19.0.46 source commit; release commit `e9c4f0e` (待) = installer (43,257,645 bytes, md5 `4d41804d60ded5cb8a1db88faf74e346`), TestPhrasesDialog 168/168 PASS / 0 FAIL (sandbox 不验 IME, 装机端必跑 5 项 user flow)。装机命令 + verify 项见 §10, 装机反馈分流见 §9。

---

— END OF HANDOFF —
