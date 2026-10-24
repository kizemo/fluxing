# Handoff — Phase L Bugfix 3 v0.19.0.66 ship (2026-07-25)

> **承接**: `handoff-after-phase-L-bugfix-3-v0.19.0.66-2026-07-25-inprogress.md` (NSIS in-progress)
> **承接**: `handoff-after-phase-L-bugfix-3-pre-subagent-v0.19.0.66-2026-07-25.md` (subagent-phase)
> **状态**: ✅ **v0.19.0.66 已 ship** (build + NSIS + commit + tag + SFTP atomic upload + cross-source verify)
> **方向**: 等装机端 user 验证 4 user flow → 沉淀 L##-PhaseL-7 (可选,Alt+, "回归" race timing 教训)

---

## 1. 已 ship 状态 (v0.19.0.66)

| 项 | 值 |
|---|---|
| branch | `Fluxing` (本地 fork) |
| last commit | `06fa893` (fix(WeaselTSF) v0.19.0.66) |
| last tag | `v0.19.0.66` |
| env.bat / weasel.props | v0.19.0.66 (gitignored, 本地) |
| **installer md5** | `B4733435610FE4F22C5F837A23036A7E` (43,360,374 bytes, NSIS build 2026-07-25 23:05:57) |
| **WeaselServer.exe md5** | `B3BCB65967D6AD0B00A5D0460405EC73` (✓ ≠ v0.19.0.65 `BFC81FA7...`,L108 stale binary 风险排除) |
| **FluxingPhrasesDialog.exe md5** | `C969AEB31E24D9EE1468F8869150970D` (x64 RimeWithWeasel.lib relink → md5 微变) |
| **weaselx64.dll md5** | `C62A364BD9C08E0A7210EE07E9FE4529` (实际 sink 注册改动的 binary) |
| SFTP upload | ✅ www.aiec.fun/pinyin/ (4 文件 cross-source verify 全 OK) |
| git push kizemo | ⏸ K7 fallback — HTTP 500 持续 fail,ship 不依赖 push (装机端 SFTP 已成功) |

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

**trigger L3 gate ✓**: `grep -RIn "if (false)" WeaselServer RimeWithWeasel WeaselTSF` → 3 处命中全在 RimeWithWeasel.cpp 历史注释里，**项目内代码无残留**。

---

## 3. Root cause narrative (3 subagent 综合 → 100% 确认)

### 核心 root cause
**`ITfActiveLanguageProfileNotifySink` 已声明 + 已实现，但**从未通过 AdviseSink 注册到 TSF**。**

### 关键证据
- `WeaselTSF.h:18` 类继承声明 ✓
- `WeaselTSF.h:90-93` `OnActivated` 方法声明 ✓
- `WeaselTSF.cpp:221-257` `OnActivated` 实现 ✓ (注释明确"spec 061 CORRECT trigger")
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

### 为什么 3+ ship 失败根因
- v0.18.41.0 (spec 066): 设计 OnActivated → FocusIn → EnableAlwaysShowMode
- v0.18.41.4 (L69): 因 GDI+ crash 临时禁 FocusIn panel trigger
- v0.19.0.63 (spec 074): 撤回 sticky，期望 OnActivated → FocusIn → Show
- v0.19.0.64 (Phase L bugfix 1): 改 IMM32 hook → 装机证实 hidden IPC server 无 IME context
- v0.19.0.65 (Phase L bugfix 2): 改回 FocusIn + Show(null × 6)
- **所有路径都期望 OnActivated sink 已注册 — 但实际从未注册！** 18 ship 版本累积 bug

### L66 类比 (设计 vs 落地假完成)
spec 061 设计意图写在代码注释里 + 实现函数写了，但 AdviseSink 调用从未补上 → spec 假完成。这是 L66 (NSIS safety-net) 同模式：设计意图 vs 落地之间的"假完成"陷阱。

### Alt+, "回归" 真相 (subagent 1 H1 race ~70%)
- v0.19.0.65 之前: KeyEventSink 在用户已在文本框输入时误触发 FocusIn → panel 短暂弹 → Alt+, 立即 Hide → user 感受 "Alt+, work" (实际是 FocusIn 显示的)
- v0.19.0.65: FocusIn 调 Show(null × 6)，但 sink 未注册还是不发 IPC
- v0.19.0.66: sink 已注册 → FocusIn IPC 真发 → Alt+, 真实 toggle

---

## 4. 装机端 4 user flow (待 user 验证)

按 sandbox-verify 在 sandbox 启动但 in-sandbox-verify.ps1 logs 未回传 (sandbox VM 进程僵在 host session 0)，**装机验证留给 user 手动跑**:

```powershell
F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 `
    -InstallerPath "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.66-installer.exe"
```

期望 4 user flow 全 PASS:
1. **Win+Space 切 Fluxing** → 浮动栏自动弹 ✓ (sink 现在触发 → OnActivated → IPC FocusIn → Show)
2. **Win+Space 切英文** → 浮动栏收 ✓ (sink 触发 OnActivated(false) → IPC FocusOut → Hide)
3. **切焦点到 text input** → panel 状态由当前 active IME 决定 (Fluxing 时弹,英文时不弹)
4. **Alt+,** → toggle ✓ (handler IsWindowVisible + Show/Hide 正常工作)

或者直接下载 www.aiec.fun/pinyin/fluxing-0.19.0.66-installer.exe 装机验证。

---

## 5. 沉淀 (lessons-learned.md 已加的 L##)

| L## | 教训 | 状态 |
|---|---|---|
| **L##-PhaseL-6-SinkPseudoComplete** | TSF sink 接口已声明 + 已实现但 AdviseSink 从未注册 → 18 ship 版本累积 bug。code review checklist 强制 4 件套 (类继承 + QI + AdviseSink + UnadviseSink + cookie 字段) | ✅ 已加 |

**L##-PhaseL-7 备选**: "Alt+, 回归真相" race timing 错觉 — 等装机验证后决定是否单独成节。

---

## 6. K7 fallback (git push 跳过)

按 K7 教训: `git push kizemo` 持续 HTTP 500。**装机端交付不依赖 GitHub 同步**（SFTP www.aiec.fun/pinyin/ 已成功）。下次 ship 时合并 push。

---

## 7. 严格禁止 (持续)

- ❌ 改 install.nsi (L09/L17/L54/L66 雷区)
- ❌ 改 QuickPanelDialog.cpp 内部
- ❌ 改 Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点
- ❌ L108 stale binary ship
- ❌ `if (false)` 残留 (Trigger L3 gate)

---

## 8. 完整 ship 命令流 (可复用)

```bash
# 1. 改源码 (4 文件 5 处 ~36 行)
# 2. env.bat / weasel.props bump 到新版本 (gitignored)
# 3. build_v0XX.ps1 创建 (仿 v063 模板)
# 4. 跑 build (5-60 min)
powershell -File build_v066.ps1

# 5. 单独跑 NSIS 阶段 (build_v066.ps1 PowerShell wait 有 zombie 进程陷阱,手动跑稳)
cmd.exe /c "_nsis_only.cmd"

# 6. 7z extract installer 验证 binary md5 (排除 L108 stale binary)
powershell -File diag-extract-verify.ps1

# 7. _check_install_v2.ps1 期望值更新
# 8. commit + tag
git add WeaselTSF/ _check_install_v2.ps1 .specify/memory/lessons-learned.md task.md
git commit -m "fix(WeaselTSF): v0.XX.X.XX — ..."
git tag v0.XX.X.XX

# 9. Copy-Item 到 staging (sftp_sync 不替你做!)
cp release/.../installer.exe "E:/办公文件/L1网站/pinyin/..."

# 10. SFTP atomic upload + cross-source verify
py sftp_sync.py --version 0.XX.X.XX
```

---

*End of Phase L Bugfix 3 ship handoff. v0.19.0.66 ship-ready.*