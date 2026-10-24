承接 `handoff-after-phase-L-bugfix-8.2-installer-finalize-freeze-2026-07-29.md`。
v0.71 corrected 已 ship (3 commits: 45264e5 first/broken, cfcb96d release/bad, 5346c07 re-ship/corrected; 13/13 test PASS; SFTP OK; ADR 0001)。
**新 bug**: 装机最后一步"完成"对话框 → 点确定 → 系统无响应 (WeaselServer hung)。

**关键区别**: 不是 v0.70 同款 (boot 后 TSF OnActivated 风暴致 Echo 永久阻塞)。
是 installer finalization 阶段触发 — NSIS line 802 `Exec "$INSTDIR\WeaselServer.exe"` (fire-and-forget) 启动 WeaselServer, 它自身启动 hang (推测 librime engine init 在 `rime/engine.cc:72` 卡死, 跟 v0.70 15:54:34 log 同源)。
Echo(100) / FocusInGuard 不解此 bug, 因为是 WeaselServer 自身 hang, 跟 TSF 无关。

## 必交付
1. **Step 1 (CRITICAL) — capture 装机 hang 现场**:
   - user 装 v0.71 corrected (md5 FA7C7580), 装机到"安装完成"对话框, **不点 OK**
   - PowerShell `Get-Process WeaselServer | Select Id,CPU,Threads,WS,StartTime`
   - 点 OK 触发 freeze
   - 立即 `Get-Process WeaselServer` (PID/CPU/threads 状态 — 0% CPU = deadlock, 100% = busy loop)
   - 看 `%TEMP%\rime.weasel\*.INFO.*.log` tail 50 行 (尤其 `engine.cc:72] starting engine` 之后)
   - 必要时 `procdump -ma <PID>` dump (PPL 需 Process Explorer)
2. **Step 2 — 定位 engine init hang 阶段**:
   - 读 `RimeWithWeasel/RimeWithWeasel.cpp` Initialize / StartMaintenance / deploy 路径
   - 读 `librime/src/rime/engine.cc:72` (engine init 入口)
   - 加 instrumentation (DLOG) 在 RimeWithWeasel Initialize 关键阶段, 用 stopwatch 计时
3. **Step 3 — 决定 fix 策略** (3 个 Option, 选 1 个):
   - **Option 1 (最小, 推荐先试)**: `output/install.nsi` line 802 改 `Exec` → `ExecWait` + 60s timeout。风险: installer 卡 60s; 收益: 至少能诊断 hang 阶段, 让 user 看 hang 状态
   - **Option 2 (架构)**: WeaselServer 启动分阶段, librime init 放 worker thread, main thread 只做 ready check。需 spec + ADR。invasive
   - **Option 3 (workaround)**: 装机不 Exec WeaselServer, 让 user 重启后通过 HKLM Run key 自动启动。0 风险但 UX 差
4. **写 TestWeaselServerStartup** 单元测试 (mock librime, 测 init 路径不 hang)
5. **rebuild + sandbox verify** (不装机, 只在 sandbox 跑 WeaselServer.exe 测启动 ≤ 30s)
6. **ship v0.72** (含 fix) + SFTP + 三件套

## 禁止
- ❌ 不动 `librime/` 子模块 (L107 教训: submodule 改动需 user 授权 + ADR)
- ❌ 不动 `output/install.nsi` 除非 user 同意 + ADR (L66 / nsis-registry-writes 硬约束)
- ❌ 不 commit handoff-* / _prompt-* / diag-* / _build_*.cmd / get_md5.ps1 / verify_v071.ps1 (移交 + 工具文件)
- ❌ 不 git push (K7 仍 500)
- ❌ 不用 PPL taskkill 风险路径做实验 (L17 教训: PPL taskkill /F 无效, 加 /T 子进程)

## 加速模式 (session-health <2h)
- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit
- parallel subagent ≤ 2

## 完成时机
- Step 1 现场 capture 完成 + Step 2 根因确认 → 写新 handoff `handoff-after-phase-L-bugfix-9-v0.72-fix-2026-07-29.md`
- Step 3 fix 实施 + Step 4 test 13+/13+ PASS + Step 5 sandbox verify OK → 写新 handoff `handoff-after-phase-L-bugfix-9-v0.72-ship-2026-07-29.md`
- v0.72 ship 完成 → 关闭 task, push 2 commits (K7)
