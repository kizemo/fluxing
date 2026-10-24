# Phase L Postship Freeze v0.19.0.67 — 新会话第一句话 prompt

承接 `handoff-after-phase-L-postship-freeze-v0.19.0.67-2026-07-26.md` + **`memory/2026-07-26-v066-disaster-rootcause.md`**(已写完完整 root cause 调查报告,含 3 subagent 综合分析)。session-health 强制 <2h。

**当前阶段**: systematic-debugging **Phase 0+1 已完成**(主会话 3 subagent 已完成深度采证)→ **Phase 4.5 user 决策**(5 个修复候选,推荐 A+B+C 组合) → Phase 5 TDD red-green test + 真修。

**🆕 关键发现** (主会话已锁定,新会话必沿用):
1. **灾难 #1 (安装卡死)** 真因 = `install.nsi:148-161` PPL WeaselServer.exe taskkill loop 30s sleep + `install.nsi:738` WriteRegStr HKLM\...\CTF\KnownClasses 卡 CTF lock → **不是 v0.66 新代码直接造成**,但是叠加触发
2. **灾难 #2 (默认进 Fluxing 无浮动栏)** 真因 = `QuickPanelDialog::Show()` 仍走 v0.18.41.4 `Bitmap::FromStream` GDI+ 路径 → WeaselServer.exe heap corruption → TSF IPC 全死 → 浮动栏创建失败
3. **灾难 #3 (Alt+, 死)** 真因 = pre-existing QuickPanelDialog 单例 state 被 v0.65 `Show(null × 6)` 污染

**v0.19.0.66 sink 代码本身** API 100% 正确(per Microsoft Learn + msctf.h SDK 头文件),但漏 HIGH severity 1 项(H1 Uninit 缺 TF_INVALID_COOKIE guard)。

**必交付清单**:
1. **读 handoff §5** (已更新 5 个修复候选 A/B/C/D/E)
2. **读 `memory/2026-07-26-v066-disaster-rootcause.md`** 完整 root cause 调查报告
3. **必跑 `_check_install_v2.ps1`** 让 user 贴输出,聚焦 Module 4 (event log) — 验证 subagent 假设
4. **必跑 Get-EventLog Application Error** (user 跑,贴 Application Error + AppHang + ctfmon crash)
5. **Phase 4.5 user 决策**(推荐组合):
   - **候选 A**: Patch A+B (3 行) 加 TF_INVALID_COOKIE guard 到 v0.66 sink uninit
   - **候选 B** (主修灾难 #2): `QuickPanelDialog::Show()` 不走 GDI+ Bitmap 路径,改纯 GDI `LoadImage`
   - **候选 C** (主修灾难 #1): install.nsi PPL taskkill 缩短 (30s → 10s) + WriteRegStr KnownClasses 前置 Sleep 3000ms
6. **TDD red-green test 必写 9 项** (详见 `memory/2026-07-26-v066-disaster-rootcause.md` "TDD red-green test 必写清单"):
   - Test 1-4: 装机不卡死 (主目标)
   - Test 5-6: 浮动栏 Win+Space 弹 (次目标)
   - Test 7-9: GDI+ 路径清理 + install.nsi PPL 改进
7. **改源码**(按 user 选择) → `/t:Rebuild` → trigger L3 gate (`grep -RIn "if (false)"`)
8. **sandbox-verify 4 user flow**(沙箱跑 v0.19.0.67 全流程,验证 装机不卡死 + Win+Space 弹 + Alt+, toggle)
9. **build v0.19.0.67 + NSIS + SFTP + Cloudflare cache purge 通知**
10. **CHANGELOG.md 必补 v0.67 段**(本会话漏了 v0.66 段,这次必补,不能漏!)
11. **装机 user flow 验证**(user 跑新 installer,贴 5 项 verify 结果,见 handoff §10)

**严格禁止**:
- ❌ 盲目试错改 sink 注册代码 — Phase 1-4 已走通,真因已锁 (Patch A+B 3 行 + GDI+ 路径清理 + install.nsi PPL 改进)
- ❌ 改 Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点
- ❌ L108 stale binary ship
- ❌ `if (false)` 残留 (Trigger L3 gate)
- ❌ **改 QuickPanelDialog.cpp 内部超过 GDI+ 路径清理范围** (除非所有调用方都不 work)

**加速模式**: bash ≥60 → 切 Write/Edit;单回合 diff >500 → 拆回合;并行 subagent ≤2。

**新会话 KEEP 自检**:
- 本会话时长 < 2h (session-health 强制)
- **不重做 root cause 调查** (主会话已完成,直接用 `memory/2026-07-26-v066-disaster-rootcause.md`)
- 必须**新读** `WeaselServer/QuickPanelDialog.cpp` Show() 函数 (验证 GDI+ 路径位置)
- 必须**新读** `output/install.nsi:148-161` + `install.nsi:738` (PPL taskkill + WriteRegStr KnownClasses)
- 必须**新读** `WeaselTSF/ThreadMgrEventSink.cpp` (Patch A 参考金标准)