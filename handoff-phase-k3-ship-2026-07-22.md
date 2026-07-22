# Handoff — Phase K3 ship (v0.19.0.52, 2026-07-22)

> **承接会话**: Phase K2 named pipe IPC + K3 Option B foreground fix
> **承接 HEAD**: 工作树未 commit,HEAD = `1c85b47` (v0.19.0.49-fluxing installer)
> **产出 installer**: `output/archives/fluxing-0.19.0.52-installer.exe` (43.4 MB, NSIS LZMA)

---

## 1. 已完成 (T010-T014, v0.19.0.52)

### 新增文件

| 文件 | 用途 |
|---|---|
| `WeaselServer/ForegroundCapture.h` | `fluxing::foreground_restore` namespace API |
| `WeaselServer/ForegroundCapture.cpp` | 实现:CS 锁 + 静态 cache (HWND + DWORD tid) |
| `build_v052.ps1` | v0.19.0.52 build wrapper (vcvars + cd + xbuild) |

### 修改文件

| 文件 | 改动 |
|---|---|
| `WeaselServer/WeaselServerApp.cpp` | T010: 3 处 `PhrasesDialog::Show()` → `fluxing::PhrasesDialogIPC::Show()`; Run() 入口 SetYamlPath |
| `WeaselServer/PhrasesDialogIPC.cpp` | T011-B.3: Show() 惰性初始化 yaml path (BLOCKER #2 兜底); MT_INJECT 流程加 AttachThreadInput + SetForegroundWindow |
| `RimeWithWeasel/RimeWithWeasel.cpp` | T011-B.2: `FocusIn` handler 头部调 `CaptureFromCurrentThread()` |
| `WeaselServer/WeaselServer.vcxproj` | T010+T011-B.1+T012: 加 `PhrasesDialogIPC.cpp/.h` + `ForegroundCapture.cpp/.h`; 删 `PhrasesDialog.cpp/.h` 引用 |
| `output/install.nsi` | T014: 加 `File "FluxingPhrasesDialog.exe"` (L72-fix 之后) |
| `env.bat` (gitignored) | 版本 bump 49→52 |
| `task.md` | T010-T011 标 ✓ + T012-T013 标 ✓ |
| `.specify/memory/lessons-learned.md` | L104 记录 Option B 修复 |

### 删除文件 (T012)

- `WeaselServer/PhrasesDialog.cpp` (~1400 行)
- `WeaselServer/PhrasesDialog.h` (~80 行)

---

## 2. Option B 修复 — SendInput 抢回 user foreground

**问题**: out-of-process IPC 后 `SendInput` 命中 dialog 而非 user app。
原 in-process `Hide→Inject` 原子化,跨进程后必须显式补 `SetForegroundWindow + AttachThreadInput`。

**关键洞察**:
- `GetForegroundWindow()` 是 **session-global API**,不依赖 caller 进程
- 在 WeaselServer.exe IPC worker thread 调 = 拿 user app 当前 foreground
- 触发点: 用户在 edit field 输入 → TSF `OnSetFocus` → WeaselTSF.dll 发 `FocusIn` IPC → server 端 `FocusIn` handler 跑 → 此时 system foreground = user app

**Inject 流程** (PhrasesDialogIPC.cpp MT_INJECT):
```cpp
HWND target = GetHwnd();  // foreground_restore cache
DWORD targetTid = GetThreadId();
DWORD currentTid = GetCurrentThreadId();
bool attached = false;
if (target && IsWindow(target) && targetTid && targetTid != currentTid) {
  if (AttachThreadInput(targetTid, currentTid, TRUE)) attached = true;
}
SetForegroundWindow(target);
InjectText(text);
if (attached) AttachThreadInput(targetTid, currentTid, FALSE);
```

---

## 3. v0.19.0.52 installer 内容验证

```
$ 7z l output/archives/fluxing-0.19.0.52-installer.exe
$PLUGINSDIR\modern-wizard.bmp     (NSIS 内置)
$PLUGINSDIR\nsDialogs.dll          (NSIS 插件)
$PLUGINSDIR\System.dll             (NSIS 插件)
$PLUGINSDIR\nsExec.dll             (NSIS 插件)
fluxing-logo.png                   (QuickPanel 用)
weasel\fluxing-logo.png            (QuickPanel 用, 子目录副本)
weasel\fluxing-logo_small.png      (QuickPanel 用)
LICENSE.txt, README.txt, 7-zip-license.txt
7z.dll, 7z.exe, curl.exe, curl-ca-bundle.crt
rime-install.bat, rime-install-config.bat, start_service.bat, stop_service.bat
weasel.dll                         (TSF shim 32-bit)
weaselx64.dll                      (TSF shim 64-bit)
weaselARM.dll, weaselARM64.dll, weaselARM64X.dll
WeaselServer.exe                   (Win32, NEW 19:38 — T010+T011 改动)
WeaselDeployer.exe                 (Win32, 旧 8:37 — 无改动, link 失败 OK)
WeaselSetup.exe                    (NEW 19:51)
FluxingPhrasesDialog.exe           (x64, NEW 15:38 — T009 改动)  ← **新增!**
Win32\rime.dll, Win32\WinSparkle.dll
data\*.yaml, data\cn_dicts\*.dict.yaml, data\en_dicts\*
```

✅ Installer 包含 `FluxingPhrasesDialog.exe` (新增的 out-of-process 对话框)
✅ `WeaselServer.exe` 是 v0.19.0.52 fresh binary (有 T010+T011 改动)
✅ Uninstall wildcard `Delete "$INSTDIR\*.*"` 覆盖清理 (L848)

---

## 4. 构建命令 (供下一会话复用)

```powershell
# 不要直接用 powershell 调 cmd /c "call vcvars & cd & call xbuild" — cmd
# 不识别 quoted 的 call path。 用 build_v052.ps1 wrapper, 内部建一个
# _build_v052_step1.cmd 把 vcvars + cd + xbuild 串起来。
powershell -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\build_v052.ps1
```

输出: `F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.52-installer.exe`

---

## 5. 装机验证 (T018 — 待办,需 user 真机)

需要在 user 真 Win11 装机验:

```powershell
# 1. silent install 到 D:\Program Files\Fluxing\
& "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.52-installer.exe" `
  /S /D=D:\Program Files\Fluxing

# 2. 验证产物 (L66 / L103-I 验法)
reg query "HKLM\SOFTWARE\Microsoft\CTF\KnownClasses"
reg query "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804"
reg query "HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32"
md5sum "D:\Program Files\Fluxing\weasel\WeaselServer.exe"
md5sum "D:\Program Files\Fluxing\weasel\FluxingPhrasesDialog.exe"

# 3. 验证短语 dialog (核心)
#   - 启动 WeaselServer.exe
#   - 切到 Notepad
#   - 按 Alt+. → FluxingPhrasesDialog 弹出
#   - 双击 "你好" → 应进 Notepad,不是 dialog 自带输入框
#   - Alt+/ 同样路径

# 4. 清理
& "D:\Program Files\Fluxing\uninstall.exe" /S
```

---

## 6. 已知限制 (装机后用户反馈再迭代)

1. **Cached HWND 滞后**: FocusIn → Alt+Tab → 按 Alt+. → dblclick 时, 注入去
   last edit field, 不是 user 当前窗口。Mitigations: CBT hook / SetWinEventHook
   (out of scope, 需 DLL 注入或重写)。
2. **SetForegroundWindow Win11 22H2+ 仍可能拒**: 改进需 capture targetPid +
   AllowSetForegroundWindow(targetPid) 模式。
3. **Pipe ACL 未收紧** (SUGGESTION): 同 user 恶意 app 可连 named pipe,
   装 phrases.yaml 写权限暴露。可加 random token: `--pipe=<name>;token=<rand>`
   第一帧验证 (out of scope for v0.19.0.52)。
4. **test/TestPhrasesDialog/ orphan**: 测 in-process PhrasesDialog, vcxproj
   include `../../WeaselServer/PhrasesDialog.cpp` 已删。test 自身能 build 失败
   (但不在 weasel.sln, 不卡 sln build)。处置决策: 删 test / port to IPC API /
   disable — 等 user 决定。

---

## 7. Git 状态 (commit 准备)

```
D  WeaselServer/PhrasesDialog.cpp
D  WeaselServer/PhrasesDialog.h
M  WeaselServer/WeaselServer.vcxproj
M  WeaselServer/WeaselServerApp.cpp
M  RimeWithWeasel/RimeWithWeasel.cpp
M  WeaselServer/PhrasesDialogIPC.cpp
M  WeaselServer/PhrasesDialogIPC.h
M  output/install.nsi
M  .specify/memory/lessons-learned.md
M  task.md
?? WeaselServer/ForegroundCapture.cpp
?? WeaselServer/ForegroundCapture.h
?? build_v052.ps1
?? memory/2026-07-22-k3-foreground-ordering-regression.md
```

下一会话 commit 建议拆 3 个 changeset (AGENTS.md §3 / P8 锁定绑):
1. `refactor(WeaselServer)` PhrasesDialog 拆 out-of-process (T010-T012) — 删旧 + 集成 IPC + 集成 ForegroundCapture + install.nsi 加新 binary
2. `fix(WeaselServer)` foreground-ordering (T011 Option B)
3. `docs(memory)` L104 + k3-foreground-ordering-regression memory

— END OF HANDOFF —
