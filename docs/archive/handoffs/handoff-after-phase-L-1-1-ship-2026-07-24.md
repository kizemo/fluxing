# Handoff — Phase L 调整 1.1 v0.19.0.61 ship (2026-07-24)

> **承接 HEAD**: `aa37613` (refactor(WeaselServer): v0.19.0.61 — QuickPanel UserDict 按钮整槽 invisible)
> **承接 handoff**: `handoff-after-phase-L-1-ship-2026-07-24.md` (v0.19.0.60 ship)
> **状态**: ✅ v0.19.0.61 ship 完成 — commit + tag + installer + SFTP 上传 OK
> **触发**: user 反馈 v0.19.0.60 button 2 槽位仍可见 + click no-op 是"看起来无意义",要彻底删视觉本身
> **会话**: 继续 v0.19.0.60 同一会话,跨 12:00 → 12:50,约 50 min

---

## 1. v0.19.0.61 ship artifacts

| Artifact | 值 |
|---|---|
| Commit | `aa37613` refactor(WeaselServer): v0.19.0.61 — QuickPanel UserDict 按钮整槽 invisible |
| Tag | `v0.19.0.61` (lightweight, points at aa37613) |
| Installer | `release/fluxing-0.19.0.61-installer.exe` (43,350,653 bytes) |
| WeaselServer.exe md5 | `A4C731CF1585BA07F33B73EC8BA5CDAD` (新 build 12:39) |
| FluxingPhrasesDialog.exe md5 | `C969AEB31E24D9EE1468F8869150970D` (v0.19.0.59+ 未变) |
| Installer md5 | `F50AD773BB6813A2AA4692368EB1E3E7` |
| Installer sha256 | `d6353116bf24b37ea68fe0513c847948180ae46b46893b87c2ab08de0d1c6e7c` |
| SFTP 端 | `www.aiec.fun/pinyin/` (atomic upload + cross-source verify OK) |

---

## 2. 调整 1.1 完成清单

| 项 | 状态 | 文件 |
|---|---|---|
| QuickPanel case 2 paint no-op | ✓ | `WeaselServer/QuickPanelDialog.cpp` |
| `DrawIconUserDict()` 函数彻底删除 (~17 行) | ✓ | `WeaselServer/QuickPanelDialog.cpp` |
| `DrawIconUserDict` 声明删除 | ✓ | `WeaselServer/QuickPanelDialog.h` |
| `OnShowUserDict` typedef 删除 (孤儿) | ✓ | `WeaselServer/QuickPanelDialog.h` |
| env.bat / weasel.props bump 0.60 → 0.61 | ✓ (gitignored) | `env.bat` + `weasel.props` |
| build_v061.ps1 新建 (L107 双步 wrapper) | ✓ | `build_v061.ps1` |
| _check_install_v2.ps1 expected md5 同步更新 | ✓ | `_check_install_v2.ps1` |
| CHANGELOG.md v0.19.0.61 段新增 | ✓ | `CHANGELOG.md` |
| Release commit + tag | ✓ | `aa37613` + `v0.19.0.61` |
| sftp_sync atomic upload | ✓ | `www.aiec.fun/pinyin/` |

**改动量**: 6 files / +84 / -37 (小改,核心是删函数)

---

## 3. 保留的 layout

QuickPanel 5 槽位 (0..4) 仍循环,索引 0/1/3/4 行为不变;只有 2 是 no-op paint:
- 0 Schema — DrawIconSchema ✓
- 1 Phrase — DrawIconPhrase ✓
- **2 (no-op) — DrawIconUserDict 已彻底删除,槽位 invisible**
- 3 Settings — DrawIconSettings ✓
- 4 Account — DrawIconAccount ✓

槽位 layout 不变(buttons 仍排 5 个,中间 2 空着),hit test 仍返回 hit==2 (无视觉但 click 也不做啥)。
风险最低:不需要重排 buttons 索引 / 不动其他 4 个 icon / 不改 kPanelW geometry。

---

## 4. 装机端 user verify (5 项 after 装 v0.19.0.61)

```
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.61-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**期望**:
- Module 1+2: WeaselServer.exe md5 = `A4C731CF1585BA07F33B73EC8BA5CDAD`
- Module 2: installer md5 = `F50AD773BB6813A2AA4692368EB1E3E7`

**5 项装机 verify**:
1. 浮动设置栏:**只看到 4 个按钮** (Schema / Phrase / Settings / Account) — 槽位 2 invisible ✓
2. Alt+. → PhrasesDialog (回归 v0.19.0.59+) ✓
3. Ctrl+Shift+U → 无反应 (回归 v0.19.0.60) ✓
4. button 2 位置 (槽位 2 在索引 1 (Phrase) 跟 3 (Settings) 中间) → 不可见、不可点 ✓
5. SFTP 装机路径: `https://www.aiec.fun/pinyin/fluxing-0.19.0.61-installer.exe` 可下载 ✓

---

## 5. 已知坑 / 不动项

- ⚠️ build_v061.ps1 用 `Start-Process + Wait + tee | tail -20` 输出被吞了 NSIS step;最终靠手动 `powershell -Command .\build_v061.ps1` 才触发 NSIS。这是 PowerShell pipe 跟 Start-Process 交互的限制,不是 build 流程本身的 bug。后续可改成 inline 执行。
- ⚠️ git push kizemo v0.19.0.61 仍 HTTP 500 (跟 K7 v0.19.0.59 + v0.19.0.60 同样症状),ship 路径 SFTP 已成功,不阻塞。

---

## 6. 下一步 (Phase L 调整 2 / 3 仍未做)

按 `plan-phase-L-2026-07-24.md`:
- ❌ 调整 2: 所有输入法方案都支持中文标点
- ❌ 调整 3: 切换到火流猩输入法时,浮动设置栏自动弹出

下次会话第一句话: "承接 v0.19.0.61 ship,见 `handoff-after-phase-L-1-1-ship-2026-07-24.md`,下一步做 Phase L 调整 2 还是 调整 3?"

---

— END OF HANDOFF —
