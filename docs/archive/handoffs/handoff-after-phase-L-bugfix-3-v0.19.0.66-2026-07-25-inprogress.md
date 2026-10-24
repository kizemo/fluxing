# Handoff — Phase L Bugfix 3 v0.19.0.66 (in-progress, 2026-07-25)

> **承接**: `handoff-after-phase-L-bugfix-3-pre-subagent-v0.19.0.66-2026-07-25.md`
> **承接**: 3 subagent 报告 (`alt-comma-regression` / `focusin-reality` / `quickpanel-trigger-matrix`)
> **状态**: 🟡 **方案 A 改源码完成 + /t:Rebuild build 成功 + NSIS in-progress** — 等 sandbox-verify
> **方向**: 等 NSIS 完成 → sandbox-verify 4 flow → ship v0.19.0.66 (commit + tag + nsis + installer + SFTP)

---

## 1. 已 ship 状态 (v0.19.0.66 WIP)

| 项 | 值 |
|---|---|
| branch | `Fluxing` (本地 fork, ahead 943 / behind 831 + 本会话改动) |
| last commit | `84f01c5` (v0.19.0.65) — **本会话尚未 commit v0.19.0.66 改动** |
| env.bat / weasel.props | bumped 到 v0.19.0.66 (gitignored, 本地修改) |
| **WeaselServer.exe 新 md5** | `B3BCB65967D6AD0B00A5D0460405EC73` (✓ ≠ v0.19.0.65 `BFC81FA7...`，L108 stale binary 风险排除) |
| **FluxingPhrasesDialog.exe 新 md5** | 待 NSIS 完成输出 installer 后从 7z extract 验证 |
| installer | **NSIS 阶段正在跑** (cmd.exe pid 2492) — `output/archives/fluxing-0.19.0.66-installer.exe` 尚未生成 |

---

## 2. 改源码 diff 摘要 (5 处, 36 行新增)

| 文件 | 改动 |
|---|---|
| `WeaselTSF/WeaselTSF.h` | +5 行：`_InitActiveLanguageProfileSink` / `_UninitActiveLanguageProfileSink` 方法声明 + `_dwActiveLanguageProfileSinkCookie` 字段 |
| `WeaselTSF/WeaselTSF.cpp` 构造函数 (cpp:30) | +1 行：cookie 字段初始化 `TF_INVALID_COOKIE` |
| `WeaselTSF/WeaselTSF.cpp` QueryInterface (cpp:67-70) | +3 行：`IID_ITfActiveLanguageProfileNotifySink` 分支 |
| `WeaselTSF/WeaselTSF.cpp` Deactivate (cpp:107-108) | +2 行：`_UninitActiveLanguageProfileSink()` 调用 |
| `WeaselTSF/WeaselTSF.cpp` ActivateEx (cpp:166-169) | +4 行：`_InitActiveLanguageProfileSink()` 调用 + goto ExitError |
| `WeaselTSF/WeaselTSF.cpp` init/uninit 实现 (cpp:230-251) | +22 行：仿 `_InitThreadFocusSink` 模板 |

**改动模式**: 仿 `_InitThreadFocusSink` 模板（cpp:203-219），对称 init/uninit。

---

## 3. Root cause narrative (3 subagent 综合)

### 核心 root cause
**`ITfActiveLanguageProfileNotifySink` 已声明 + 已实现，但**从未通过 AdviseSink 注册到 TSF**。**

### 关键证据
- `WeaselTSF.h:18` 类继承声明 ✓
- `WeaselTSF.h:90-93` `OnActivated` 方法声明 ✓
- `WeaselTSF.cpp:221-257` `OnActivated` 实现 ✓
- **修复前**:`grep -RIn "IID_ITfActiveLanguageProfileNotifySink"` 全工程 **0 命中**
- **修复后**:`grep` WeaselTSF 显示 **2 处命中** (cpp:70 QI + cpp:237 AdviseSink) ✓

### 调用链修复前后对比

```
修复前 (3+ ship 失败):
[Win+Space 切 Fluxing]
    ↓
[TSF manager 切 language profile]
    ↓ 应触发 ──❌── sink 未注册 ──→ OnActivated 永不触发
    ↓
[m_client.FocusIn()]  ← 这步根本不进
    ↓
[IPC FocusIn → RimeWithWeasel::FocusIn]
    ↓
[QuickPanelDialog::Show(false, nullptr×6)]
    ↓
[浮动栏弹]  ← 不发生

修复后 (v0.19.0.66):
[Win+Space 切 Fluxing]
    ↓
[TSF manager 切 language profile]
    ↓ 触发 ──✓── sink 已注册 ──→ OnActivated(true) 调 m_client.FocusIn()
    ↓
[m_client.FocusIn()]  ← 现在能进
    ↓
[IPC FocusIn → RimeWithWeasel::FocusIn]
    ↓
[QuickPanelDialog::Show(false, nullptr×6)]
    ↓
[浮动栏弹]  ← 期望 work
```

### Alt+, "回归" 真相 (subagent 1 H1 race)
- v0.19.0.65 之前: KeyEventSink 误触发 FocusIn + Alt+, Hide race timing 让 panel 短暂弹 → user 感受"Alt+, work"
- v0.19.0.65: FocusIn 调 Show(null × 6)，但 sink 未注册 IPC 不发
- v0.19.0.66: sink 已注册 → FocusIn IPC 真发 → Alt+, 真实 toggle（弹/收/弹）

### QuickPanelDialog 单例 state (subagent 1/3 报告)
- **state 未损坏** — `Show()` 永远不验证 callback null，无拒绝路径
- `Show(null × 6)` 视觉弹 OK，仅在用户点 panel 按钮时 callback null 会 silently skip（不影响视觉）

---

## 4. Phase 4.5 架构决策 (已采纳)

**用户选择方案 A**: 最小 sink 注册 (4 文件 ~30 行 diff)

**Phase 4.5 强制要求** 已满足: user 显式同意后改源码,走完 spec-init-equivalent 流程 (root cause 分析 + 4 选项 user 决策 + 改动范围控制)。

---

## 5. Trigger L3 gate ✓

```
$ grep -RIn "if (false)" WeaselServer/ RimeWithWeasel/ WeaselTSF/
RimeWithWeasel/RimeWithWeasel.cpp:436:  //      L69-fix 残留 `if (false)` 一直没改回(只有 L81-fix 改了 FocusOut)。
RimeWithWeasel/RimeWithWeasel.cpp:456:  // L81-fix: 把 L69-fix 残留的 `if (false)` 重新启用。
RimeWithWeasel/RimeWithWeasel.cpp:458:  // L70+ spec 070 重新启用并改纯 GDI (L75)。但 L69 的 `if (false) QuickPanelDialog::Hide()`
```

**3 处命中全在 RimeWithWeasel.cpp 历史注释里**(讲 L69-fix 历史,非实际死代码)。**项目内代码无残留 `if (false)`** ✓

---

## 6. /t:Rebuild smoke test ✓

```
[DONE] Exit: 0
```

`_buildflow.cmd` 已用 `msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32`(L108 教训)。WeaselServer.exe 新 md5 ≠ v0.19.0.65 → 真 rebuild 不是 stale binary。

---

## 7. 沉淀 (lessons-learned.md 已加的 L##)

| L## | 教训 | 状态 |
|---|---|---|
| **L##-PhaseL-6-SinkPseudoComplete** | TSF sink 接口已声明 + 已实现但 AdviseSink 从未注册 → 18 ship 版本累积 bug。code review checklist 强制 4 件套 (类继承 + QI + AdviseSink + UnadviseSink + cookie 字段) | ✅ 已加 (本会话) |

**L##-PhaseL-7 备选**: "Alt+, 回归真相" race timing 错觉 — 等 v0.19.0.66 装机验证后再决定是否单独成节（合并到 L##-PhaseL-6 的 AP-L##-E anti-pattern 即可）

---

## 8. 装机端 sandbox-verify 方案

待 NSIS installer 跑完后跑：

```powershell
F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 `
    -InstallerPath "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.66-installer.exe"
```

期望 4 user flow 全 PASS:
1. Win+Space 切 Fluxing → 浮动栏自动弹 (sink 现在触发 → OnActivated → IPC FocusIn → Show)
2. Win+Space 切英文 → 浮动栏收 (sink 触发 OnActivated(false) → IPC FocusOut → Hide)
3. 切焦点到 text input → panel 状态由当前 active IME 决定 (Fluxing 时弹,英文时不弹)
4. Alt+, → toggle (handler `IsWindowVisible` + Show/Hide 正常工作)

---

## 9. ship 流程 (待执行)

NSIS 完成后:
1. ✅ 改源码 (5 处 36 行)
2. ✅ env.bat / weasel.props bump (本地,gitignored)
3. ✅ build_v066.ps1 写好
4. ⏸ NSIS installer 跑完 → 抓 binary md5
5. ⏸ sandbox-verify 4 flow
6. ⏸ commit `fix(WeaselTSF) v0.19.0.66 Phase L Bugfix 3 sink 注册真修`
7. ⏸ chore(release) `v0.19.0.66 installer`
8. ⏸ tag `v0.19.0.66`
9. ⏸ SFTP sync `www.aiec.fun/pinyin/`
10. ⏸ _check_install_v2.ps1 期望 md5 同步更新
11. ⏸ 装机端 user flow 验收

---

## 10. 严格禁止 (持续)

- ❌ 改 install.nsi (L09/L17/L54/L66 雷区)
- ❌ 改 QuickPanelDialog.cpp 内部
- ❌ 改 Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点
- ❌ L108 stale binary ship
- ❌ `if (false)` 残留 (Trigger L3 gate)

---

## 11. 新会话必读顺序 (如有接力)

1. 本 handoff (`handoff-after-phase-L-bugfix-3-v0.19.0.66-2026-07-25-inprogress.md`)
2. `_prompt_phase_L_bugfix_3_next_session_v0.19.0.66_2026-07-25.md`
3. `task.md` Phase L Bugfix 3 段
4. `lessons-learned.md` L##-PhaseL-6
5. 关键源码: `WeaselTSF/WeaselTSF.cpp:30,67-70,107-108,166-169,230-251`

---

*End of Phase L Bugfix 3 in-progress handoff. NSIS 跑完后自动继续.*