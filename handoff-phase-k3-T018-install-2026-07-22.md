# Handoff — Phase K3 T018 装机验证 (v0.19.0.54, 2026-07-22)

> **承接会话**: Phase K3 ship 3 commits (c465254) + v0.19.0.54 重新打包
> **承接 HEAD**: `c465254` (3 commits ahead of 1c85b47)
> **Installer**: `output/archives/fluxing-0.19.0.54-installer.exe` (T014 重 build)
> **目的**: 真机 Win11 装机,验 Option B (foreground-ordering fix) 是否 work

---

## 1. 装机 (T018 step 1) — 静默装到 D 盘

⚠️ **PPL 锁 (L103-I 经验)**: WeaselServer.exe 是 PPL 进程,taskkill 表面成功但 mmap 仍锁。
装机前建议先重启 → installer 装完会 stage binary for next-boot swap (L72-fix + Phase J Stage 2)。

```powershell
# 1.1 重启 (推荐,避免 PPL 锁)
shutdown /r /t 0

# 1.2 重启后:静默装机 (用户报告用 D 盘,改 /D=)
& "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.54-installer.exe" `
  /S /D=D:\Program Files\Fluxing

# 1.3 等待装完 (~30s,silent 模式无 console)
Start-Sleep -Seconds 30

# 1.4 验 (Phase J L103-I 验法)
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 `_check_install_v2.ps1` 输出:
- [1] Running WeaselServer path = `D:\Program Files\Fluxing\weasel\WeaselServer.exe`
- [2] Install WeaselServer.exe md5 = `__V0_19_0_54_WEASELSERVER_MD5__` (MATCH)
- [2] 也有 `FluxingPhrasesDialog.exe` md5 = `__V0_19_0_54_FLUXING_PHRASES_DIALOG_MD5__` (MATCH)
- [3] HKLM InstallDir = `D:\Program Files\Fluxing\weasel\`
- [5] newest file = installer 装的 timestamp (今天 19:xx)
- 无 AppHang / Application Error events

(具体 md5 填入见 handoff 末尾 section 4,build_v054 跑完会更新 _check_install_v2.ps1)

---

## 2. Option B 端到端验证 (T018 step 2) — 核心

这是 Phase K3 整个 refactor 的最终目的:**短语注入文本要进 user 的 app,而非 dialog 自身**。

### 2.1 准备 phrases.yaml

```powershell
# 检查 phrases.yaml 是否存在
$yaml = "$env:APPDATA\Rime\phrases.yaml"
if (-not (Test-Path $yaml)) {
  @"
- text: "你好世界"
- text: "火流猩输入法"
- text: "Option B 验证"
- text: "SendInput 抢回 foreground"
"@ | Out-File -FilePath $yaml -Encoding utf8
  Write-Host "Created $yaml"
}
```

### 2.2 端到端测试 (4 场景,任一 fail → Option B 未 work)

| 场景 | 操作 | 期望 | 失败真因 |
|---|---|---|---|
| **A. Alt+. → dblclick** | Notepad 焦点 → 按 Alt+. → 弹出 FluxingPhrasesDialog → dblclick "你好世界" | "你好世界" 进 Notepad (不是 dialog 输入框) | foreground 顺序回归未修 |
| **B. Alt+/ → dblclick** | Word/微信 焦点 → 按 Alt+/ → 弹出 dialog → dblclick "火流猩输入法" | "火流猩输入法" 进 app edit | 同上 |
| **C. QuickPanel button → dblclick** | 点系统托盘 → QuickPanel 弹出 → 按钮 1 (Phrase) → dblclick "Option B 验证" | 文本进当前 edit (Notepad 之类) | QuickPanel 抢 foreground 未释放 |
| **D. 增删改短语** | dialog 里加 "SendInput 抢回 foreground" → 关闭 → 重新打开 | 列表有 4 条;dblclick 新增的 | pipe 协议 fail (45/45 PASS 应已 cover) |

每场景都重 dblclick 测 2-3 个不同 phrase (变化 text 长度,覆盖 ASCII + 中文)。

### 2.3 失败迹象

| 迹象 | 解释 | fallback |
|---|---|---|
| 文本进 dialog 输入框,Notepad 无变化 | foreground 抢回失败 (Option B 未生效) | 检查 WeaselServer.exe md5 = 0.19.0.54;FocusIn handler 是否调用 CaptureFromCurrentThread (DebugView 抓 log) |
| 弹 dialog 时 user app 失焦 | SetForegroundWindow 太激进 | 减小 SetForegroundWindow 调用频次或加 AllowSetForegroundWindow |
| dblclick 后 app 短暂失焦但文本没进 | SetForegroundWindow 失败但 AttachThreadInput 抢到 input cluster | 验 SetForegroundWindow 返回值;Win11 22H2+ 可能需 AllowSetForegroundWindow(targetPid) 升级 |
| phrases.yaml 不读 | WeaselUserDataPath 读到错的路径 | 检查 HKCU\Software\Rime\Weasel\RimeUserDir |

---

## 3. DebugView 抓 log (T018 step 3) — 出问题时用

如果 Option B 不 work,装 DebugView (Sysinternals) 抓 `[PhrasesDialogIPC]` 开头的 OutputDebugStringW:
- `[PhrasesDialogIPC] Pipe created: \\.\pipe\FluxingPhrasesDialog\{pid}`
- `[PhrasesDialogIPC] Client connected`
- `[PhrasesDialogIPC] Client disconnected`
- `[PhrasesDialogIPC] Worker thread exiting`

外加:在 `FocusIn` handler 头部临时加 `OutputDebugStringW(L"[ForegroundCapture] hwnd=%p tid=%lu\n", hwnd, tid)` 看 capture 值。

---

## 4. 装机验通过 (PASS) → 决策

如果 4 场景全 PASS:
- **ship v0.19.0.54** 为 production release (替换 v0.19.0.49 装机)
- 把 `output/archives/fluxing-0.19.0.54-installer.exe` 发给装机端
- (Optional) 加一条 `chore(release)` commit 标记 v0.19.0.54 ship

如果 Option B 不 work (T011 fix 仍 fail):
- **回到代码层修**: 抓 DebugView log → 真因在 [PhrasesDialogIPC] 输出里 → 修 PhrasesDialogIPC / RimeWithWeasel / ForegroundCapture
- 重新打包 v0.19.0.55 + 重试

如果 4 场景部分 fail (e.g. C 失败, A/B 成功):
- QuickPanel 抢 foreground 问题在 WeaselServerApp.cpp 按钮路径的 Hide() 顺序 (L70 修过,但 0.19.0.49 后可能回归)
- 单独修 QuickPanel 的 foreground 释放逻辑

---

## 5. md5 同步到 _check_install_v2.ps1 (T018 step 4)

build_v054.ps1 跑完后,update `_check_install_v2.ps1` 第 55-57 行的占位符:

```powershell
# v0.19.0.54 binaries md5 (从 build output 拿):
$expectedMd5          = '?'   # output\Win32\WeaselServer.exe md5
$expectedFluxingMd5   = '?'   # output\FluxingPhrasesDialog.exe md5
$expectedInstallerMd5 = '?'   # output\archives\fluxing-0.19.0.54-installer.exe md5
$expectedBuildTime    = '?'   # NSIS timestamp (today 19:xx)
```

(Script 已经预设占位符 `__V0_19_0_54_*__`,build 完后 grep sed 替换)

---

## 6. 已知限制 (装机后 user 反馈再迭代 — L104 known limits)

1. **Cached HWND 滞后**: FocusIn → Alt+Tab → 按 Alt+. → dblclick 时, 注入去 last
   edit field, 不是 user 当前窗口。Mitigations: CBT hook (out of scope, 需 DLL 注入)。
2. **Win11 22H2+ SetForegroundWindow 仍可能拒**: 改进需 capture targetPid +
   AllowSetForegroundWindow(targetPid) 模式 (out of scope for v0.19.0.54)。
3. **Pipe ACL 未收紧** (SUGGESTION S66 反模式): 同 user 进程可任意连,装
   phrases.yaml 写权限暴露 (out of scope, 等 spec 052 单独 ship)。

这些不阻塞 ship,但装机 user 反馈后会复盘 spec 052+。

— END OF T018 HANDOFF —
