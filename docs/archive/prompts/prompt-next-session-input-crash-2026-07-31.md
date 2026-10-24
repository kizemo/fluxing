# 新会话 Prompt — 输入瞬间无响应 + explorer 关闭 真因排查

直接复制下面"Prompt 正文"作为新会话第一句话即可。

---

## Prompt 正文(给新会话的第一句话)

**承接 Fluxing v0.74.2 修复被拒绝,完整上下文在 `F:\soft\00selfmade\rime_claude\handoff-after-c2-rejection-2026-07-31.md`,先 Read 整个文件再动手。**

**bug**:装 v0.19.0.74.2 后,管理员重启,**只切换输入法不崩,一开始输入中文 → 输入法进入无响应 → 宿主软件 + explorer.exe 同时关闭**。多个不相关进程(Claude Code / dopus / HipsMain / aboboo)都是**在用火流猩输入时**崩,真因在 WeaselServer 输入处理路径。

**之前我犯的错**:花 1 周聚焦在 `output\install.nsi` 的 System32 cleanup 残留 DLL 上,补了 3 处 `IfErrors` + `weasel.old.dll` exact-match + completion log → 装机后问题依旧。user 已确认 admin 权限装机 + 重启,**装机流程不是真因**。

**约束(必须遵守)**:

1. **不要试错、不要渐进修、不要"先做小的看反馈"**——一次性把证据拿全,直接定位真因再写修复
2. **不要 commit / push / SFTP 任何东西**——这次只诊断,等 user 确认
3. **不要改 `output/install.nsi`**——已证不是这条
4. **不要碰 stage2 / schtasks / LogonCommand**——已证不是这条

**必须做的事(按顺序)**:

1. **读源码**:用 `codebase-memory-mcp`(`search_graph → trace_path → get_code_snippet → query_graph`)查 `WeaselServer/WeaselServerApp.cpp` / `WeaselServerImpl.cpp` / `WeaselServer/PhrasesDialogIPC.cpp` 三个文件中所有 input callback(`OnKeyEvent` / `OnComposition*` / `OnCandidatePage*`)的代码路径;列出每个 callback 里做的全部事情(`CreateFile` / `SendMessage` / `TerminateProcess` / `system` / `cmd /c` / `EnumWindows` / `ExitWindowsEx` / `taskkill` / `WaitForSingleObject` / 同步 IPC 等)
2. **读 dump**:写 `_diag_v0741_dumps.ps1`(gitignored)枚举 `%LOCALAPPDATA%\CrashDumps\` 最近 7 天 WeaselServer.exe + FluxingPhrasesDialog.exe dump;如果有 dump,让 user 提供路径,我用 `dumpbin` / `WinDbg` 解读
3. **读 log**:让 user 跑 `_diag_v0741_logs.ps1`(已存在)收集 `rime.weasel.*.log` 在输入瞬间的 ERROR/WARN/EXCEPTION/STACK 行
4. **读 .claude/skills / .claude/rules / lessons-learned.md** 里所有红线(L02 / L04 / L07 / L17 / L##-PhaseL 系列),看是否触犯了某条红线导致 input 路径炸掉
5. **形成单一根因假设**(精确到代码行),写到 handoff,等 user 确认后再写修复

**输出**:最终给 user 一份"input 真因报告"(哪个文件 + 哪个函数 + 哪几行 + 触发的具体条件 + 修复方案),不要直接动手修。

---

## 上下文速查

- 当前分支:`Fluxing`
- 工作区根:`F:\soft\00selfmade\rime_claude`
- user 真机路径:`D:\Program Files\fluxing\weasel\`(5 binary md5 与 v0.74.1 一致,即装机包正常)
- v0.74.2 installer:`release\fluxing-0.19.0.74.2-installer.exe` md5 `6ee983a365648d923162c0c3121aee68` (无效,但 user 已装)
- handoff:`F:\soft\00selfmade\rime_claude\handoff-after-c2-rejection-2026-07-31.md`
- 取证脚本可复用:`_diag_v0741_evidence.ps1` / `_diag_v0741_logs.ps1` / `_diag_v0741_crash.ps1` / `_diag_v0741_c1.ps1`
- 待写:`_diag_v0741_dumps.ps1`(枚举 CrashDumps)
- 注意:`librime` / `weasel.props` / `env.bat` 是 dirty / gitignored,**绝对不要碰**

---

## 时间预算

- session 活跃时间 < 2 小时(CLAUDE.md 红线)
- Bash 调用预算 ≤ 80(到点就停)
- 不要"再读一遍 / 再 check 一下"——一次到位

---

## 不要重复的错误

- ❌ 不要假设 System32 / admin / silent 是真因
- ❌ 不要只改 install.nsi 一点点试错
- ❌ 不要在 user 没确认根因前改任何 source code
- ❌ 不要给 user "也许 / 也许 / 大概" 的多假设列表——必须收敛到单一根因 + 修复方案

---

*Prompt 结束。新会话请用上面"Prompt 正文"作为第一句话。*