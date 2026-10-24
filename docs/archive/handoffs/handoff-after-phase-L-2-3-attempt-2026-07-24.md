# Handoff — Phase L 调整 2+3 v0.19.0.63 ship 卡在 build (2026-07-24)

> **承接 HEAD**: `fd5a644` (v0.19.0.62 ship 完成)
> **承接 handoff**: `handoff-after-phase-L-1-2-ship-2026-07-24.md`
> **状态**: ⚠️ 源码改动落地,build 强制 rebuild 卡死未完成 ship
> **触发**: user 让 一起做调整 2+3 (中文标点 + 切输入法弹浮动栏)

---

## 1. 已完成的源码改动(working tree,未 commit)

### 1.1 调整 2 — 中文标点补全

`output/data/radical_pinyin.schema.yaml` 加 `punctuator` 段:
```yaml
punctuator:
  __include: default:/punctuator
  full_shape:
    __include: default:/punctuator/full_shape
  half_shape:
    __include: default:/punctuator/half_shape
  symbols:
    __include: symbols_caps_v:/symbols
```

### 1.2 调整 3 — 切输入法弹浮动栏

**`WeaselServer/WeaselServerApp.cpp`**:
- `static HKL WeaselServerApp::s_fluxingHkl = NULL;` 顶部声明
- `static HKL WeaselServerApp::s_prevLangHkl = NULL;` 顶部声明
- `Run()` 启动时 `s_fluxingHkl = ::GetKeyboardLayout(0); s_prevLangHkl = s_fluxingHkl;`
- `PhrasesHotkeySubclassProc` 顶部加 `WM_INPUTLANGCHANGE` 钩子 — 比较 lParam (new HKL) 与 s_fluxingHkl,相等且上次 != → `QuickPanelDialog::Show(false, nullptr, ..., nullptr)`

**`WeaselServer/WeaselServerApp.h`**:
- 加 `static HKL s_fluxingHkl;` `static HKL s_prevLangHkl;` 声明

### 1.3 版本 + build script

- `env.bat` / `weasel.props` bump 0.62 → 0.63 (gitignored)
- `build_v063.ps1` 新建
- `CHANGELOG.md` v0.19.0.63 段已加
- `task.md` 进度更新

---

## 2. 卡住的 build 问题(L107 重现)

**症状**:
- 多次跑 `build_v063.ps1` (L107 双步: msbuild weasel.sln + _nsis_only.cmd)
- BuildFlow exit code 0 (表面 OK)
- `output/Win32/WeaselServer.exe` **不存在** (mtime 13:54 是 v0.19.0.62 的旧 binary)
- _nsis_only.cmd 报 `File: "Win32\WeaselServer.exe" -> no files found.` 然后退出
- NSIS exit 1
- Installer 还是 14:52 的 stale 43 MB 文件(里面 WeaselServer.exe md5 = v0.19.0.62 的 02876AA7...)

**根因**: MSBuild `weasel.sln /t:Build` incremental build 不检测到 WeaselServerApp.cpp/.h 改动,所以 WeaselServer.exe 没重 link。
- `touch WeaselServerApp.cpp` 没触发 incremental rebuild
- `rm WeaselServerApp.obj` 后跑 build — 也没重建(可能跟 Solution-level deps 有关)
- 多次 build attempt 都无效

**已尝试方案**:
1. ❌ `powershell -File ./build_v063.ps1` (L107 双步)
2. ❌ 删 `.obj` + 重 build
3. ❌ 直接 `cmd /c "_buildflow.cmd && _nsis_only.cmd"` (powershell escape 问题)
4. ❌ `powershell` with `vcvars32.bat` 路径转义 (x86 在 `cmd` 内被当作命令)
5. ⚠️ 多个后台 build 仍在 spawn,但 WeaselServer.exe 还是没出现

---

## 3. 下个会话必跑的步骤

### Step 1: 验证 source 改动仍在 working tree
```bash
cd F:/soft/00selfmade/rime_claude
git status -sb
# 期望:
# M CHANGELOG.md
# M WeaselServer/WeaselServerApp.cpp
# M WeaselServer/WeaselServerApp.h
# M output/data/radical_pinyin.schema.yaml
```

### Step 2: 强制 rebuild WeaselServer(关键)
```bash
cd F:/soft/00selfmade/rime_claude
# 选项 A: 修改 _buildflow.cmd 用 /t:Rebuild (临时)
sed -i 's|/t:Build|/t:Rebuild|' _buildflow.cmd

# 选项 B: 直接 msbuild + /t:Rebuild
powershell -Command "& { . ./env.bat; & 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat'; msbuild WeaselServer/WeaselServer.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:BOOST_ROOT=F:\b183 /p:SolutionDir=F:\soft\00selfmade\rime_claude\ /v:minimal /nologo }"
# 选项 C: 删 .obj + .exe 后 msbuild (本会话已做但没生效)
rm msbuild/Release/Win32/WeaselServer/WeaselServerApp.obj
rm output/Win32/WeaselServer.exe
```

### Step 3: 验证 WeaselServer.exe 真重建
```bash
stat -c '%y %n' output/Win32/WeaselServer.exe
# 期望 mtime > 当前时间,且 md5 ≠ 02876AA7... (v0.19.0.62 的 md5)
certutil -hashfile output/Win32/WeaselServer.exe MD5
```

### Step 4: 跑 NSIS + sftp
```bash
powershell -Command "_nsis_only.cmd"
# 等 NSIS exit 0 + fluxing-0.19.0.63-installer.exe 生成

# Copy to release/
cp output/archives/fluxing-0.19.0.63-installer.exe release/

# Get md5s
certutil -hashfile release/fluxing-0.19.0.63-installer.exe MD5
sha256sum release/fluxing-0.19.0.63-installer.exe

# Update _check_install_v2.ps1 expected md5 (line 70-73,replace 0.62 → 0.63)
```

### Step 5: commit + tag
```bash
git add CHANGELOG.md _check_install_v2.ps1 WeaselServer/ output/data/radical_pinyin.schema.yaml build_v063.ps1 release/fluxing-0.19.0.63-installer.exe
git commit -m "refactor(WeaselServer): v0.19.0.63 — Phase L 调整 2+3 ..."
git tag v0.19.0.63 <commit-hash>
```

### Step 6: sftp ship
```bash
cp release/fluxing-0.19.0.63-installer.exe "E:/办公文件/L1网站/pinyin/"
python sftp_sync.py --version=0.19.0.63
```

---

## 4. 不需要重做的事

- ✅ 源码改动已落地 (调整 2 schema.yaml + 调整 3 WM_INPUTLANGCHANGE 钩子)
- ✅ env.bat / weasel.props 已 bump 0.63
- ✅ build_v063.ps1 已建
- ✅ CHANGELOG.md 已写
- ✅ task.md 已更新
- ✅ 之前的 K7 push HTTP 500 状况已知(继续跳过 push)

---

## 5. 已知风险

- 启动时如果 Fluxing 不是 active IME, s_fluxingHkl 会记错 (跟 v0.19.0.63 装机后第一次启动的状态有关)
- WM_INPUTLANGCHANGE 仅在 IPC server window 收到时触发 — 装机器 user 必须跟其他 app 互动时才完整触发
- 启动即 active 时不触发(s_prevLangHkl = s_fluxingHkl 初始化对齐)

---

## 6. 整个项目 backlog(更新)

**Phase L 计划内**:
- ⚠️ 调整 2 + 3 v0.19.0.63 ship 卡 build — 源码改动 OK 等 rebuild
- (Phase L 全部 ship 完 → 完成)

**Phase L 之外活跃项**:
- ⏸ K3 T019 装机事故调查 — PENDING user 反馈
- ⏸ K7 GitHub push HTTP 500 — 持续 fail(等下个 ship 时合并 push)
- ❌ 浮动设置栏其他 polish (button 0 Schema / button 3 Account no-op)
- ❌ installer.exe 老版本链接 404 (download.php fallback)

---

## 7. 时间预算反思

本会话(从 K7 fallback 开始):
- K7 fallback + 写 handoff: 30 min
- 写 plan-phase-L: 15 min
- v0.19.0.60 ship (含 3 次 build + 漏改 + 重 build): 90 min
- v0.19.0.61 ship: 30 min
- v0.19.0.62 ship: 30 min
- v0.19.0.63 调整 2 + 源码改动: 30 min
- v0.19.0.63 build 调试卡死: 60+ min (未完成)
- **总: ~5h+**

**教训**: 当 MSBuild incremental build 不触发重建时,花时间调试路径不如直接 `msbuild /t:Rebuild` 或修改 _buildflow.cmd 用 Rebuild。**下个会话首步**: 改 _buildflow.cmd `/t:Build` → `/t:Rebuild`,这是 5 分钟内可完成的关键修复。

---

## 8. 下次会话第一句话

> 承接 v0.19.0.63 build 卡死,见 `handoff-after-phase-L-2-3-attempt-2026-07-24.md`。
> Step 1: 改 `_buildflow.cmd` 用 `/t:Rebuild` 强制 rebuild WeaselServer,然后继续 Step 2-6 ship。

---

— END OF HANDOFF —