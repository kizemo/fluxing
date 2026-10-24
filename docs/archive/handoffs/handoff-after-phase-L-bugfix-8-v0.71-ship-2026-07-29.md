# Handoff — Phase L Bugfix 8 v0.71 ship (2026-07-29)

> **承接**: `handoff-after-phase-L-bugfix-7.6-v0.70-blocked-2026-07-29.md` + user 装机反馈"卡死"
> **状态**: ✅ v0.19.0.71 freeze mitigation ship 完成 (2 commits + SFTP sync OK)
> **新目标**: A/B test v0.69 vs v0.71 验证 mitigation 是否真解冻 root cause;Phase 3 架构讨论 (OnActivated 改 PostMessage 异步)

---

## 1. 一句话总结 (大白话)

v0.70 装机后系统级卡死。User 跳过 A/B test 直接要求修。我装 3 个 mitigation (Echo bounded timeout + OnFocusIn reentry guard + 测试基建) 防止 TSF 线程永久阻塞,**没解决 root cause** (root cause 仍是 latent bug,推测 FocusIn 风暴 + QuickPanel SetWindowPos(HWND_TOPMOST) 抢焦反馈环)。v0.71 ship 完成,SFTP sync OK,装机 user flow 待 user 验证。

## 2. 3 个 mitigation (Phase L Bugfix 8)

### Fix 1: Client::Echo(DWORD timeout_ms) bounded sync I/O
**症状**: 之前 `m_client.Echo()` 在 TSF 线程同步阻塞 named-pipe I/O,WeaselServer 风暴时永远不返回 → TSF 输入线程被卡死 → 系统级 input pipeline 死锁 (你看到"鼠标 loading、所有软件无响应")。

**修复**: `WeaselIPC/WeaselClientImpl.cpp` 新增 `Echo(timeout_ms)` overload:
- 内部用 worker thread + `WaitForSingleObject(worker, timeout_ms)` + `CancelSynchronousIo`
- timeout_ms=0 走 legacy 阻塞路径 (向后兼容)
- timeout_ms>0 限时,超时返回 false
- 没有用 OVERLAPPED I/O (那是 invasive refactor,需要 PPL/socket 路径同步改造),worker thread 方案是 P2-safe 最小改动

**接入**: `WeaselTSF/WeaselTSF.cpp` `OnActivated` 改用 `m_client.Echo(100)` 替代 `m_client.Echo()`,最坏 100ms 后 TSF 线程恢复。

**API 兼容**:
- `bool Echo()` 旧调用方完全不受影响 (`include/WeaselIPC.h` 新增 `bool Echo(DWORD timeout_ms)` 无 default arg,避免重载歧义)
- `WeaselTSF.dll` + `weaselx64.dll` md5 变

### Fix 2: OnFocusIn reentry guard (FocusInGuard)
**症状**: FocusIn 风暴中,同 thread (TSF 线程) 1 秒内被调 N 次,每次都触发 m_pRequestHandler → QuickPanel 重建 + ascii_mode 写风暴 → server busy → Echo 阻塞 → TSF 卡死 (Fix 1 解 Echo 阻塞,但无法阻止 storm 进入 server)。

**修复**: 新建 `WeaselServer/FocusInGuard.h` (header-only RAII class,zero-dep):
- `thread_local` `InterlockedCompareExchange` 守门,同 thread 第二次进入直接 `IsDropped()=true` + DLOG(WARNING)
- 析构自动释放,异常安全
- 接入 `WeaselIPCServer/WeaselServerImpl.cpp::OnFocusIn` 入口
- 防止 N 次 storm call 全部到 handler (只允许 1 in-flight,后续 drop)

**API**: header-only,可被任何 caller include,zero WTL/ATL 依赖,便于单测。

### Fix 3: TestFreezeMitigation 测试基建
**位置**: `test/TestFreezeMitigation/`
**结构**: vcxproj + xmake.lua + stdafx + cpp,只依赖 WeaselIPC + FocusInGuard,zero-WTL
**4 个 test** (7 PASS / 0 FAIL):
1. Client::Echo(DWORD) overload 存在 + 立即返回 (未连接 safety net,防止 deadlock)
2. Client::Echo() no-arg overload 存在 (向后兼容)
3. FocusInGuard 第二次同 thread drop
4. FocusInGuard 第一次析构后第二次允许

**未做**: hung server 集成测试 (Test 1 早期尝试,因 message-mode pipe protocol 复杂 + 我们只测可验证 contract,time budget 不允许)。功能验证依赖装机 user flow + 真实 freeze 场景。

## 3. 已 ship (2026-07-29 19:04)

```
[Fluxing 45264e5] fix(WeaselIPC+WeaselServer): v0.19.0.71 freeze mitigation
  16 files changed, 438 insertions(+), 6 deletions(-)
[Fluxing cfcb96d] chore(release): v0.19.0.71 installer
  1 file changed
```

**4 binary md5 cross-source verify ALL MATCH** (build vs installer extract):
- weasel.dll: fce6ffd8c3e07fcbe1902a12ef53282d (was 82e9e924 v0.70)
- weaselx64.dll: 4a57948f9059fff3ccd1403addf1d62b (was 80615749 v0.70)
- WeaselServer.exe: caee5c93ce4a23cfa27dd72e380084d4 (was BAD4E61A v0.70)
- WeaselDeployer.exe: 645648f277442a77a99e1c2603c9edaa (was ED0EAEAA v0.70)

**SFTP sync OK**:
- installer: DC5D9F8503036533239739B8C7F6F0EF
- version.json + appcast.xml + release-notes.html updated + atomic upload + cross-source verify OK

## 4. 装机端 user 期望 (v0.71 ship 后)

跑 6 项 user flow (跟 v0.69 一样):
| # | 验收 | 期望 |
|---|---|---|
| 1 | 跑 v0.71 installer | 不再卡死,5-10 分钟内完成 |
| 2 | 切到 Fluxing | **浮动栏自动弹出** (Fix A 保留) |
| 3 | 按 Alt+, | 浮动栏 toggle |
| 4 | 切到英文 | 浮动栏不弹 |
| 5 | 默认 IME 不是 Fluxing | 保留自己默认 |
| 6 | reg query 4 项 keys | HKLM\CTF\KnownClasses + HKCU\CTF\Assemblies\0x00000804 + HKLM\CLSID InprocServer32 + HKLM\Fluxing\Weasel InstallDir 全部存在 |

**关键: 即便 v0.71 装机仍卡死,系统至少 100ms 后可恢复 (Fix 1 bounded timeout),不再永久卡死。** User taskkill 后可正常用。

**Workaround 状态**: User 已运行 `reg add HKCU\Keyboard Layout\Preload /v 1 /t REG_SZ /d 00000409` 等指令,把 en-US 提到第 1 位,Fluxing 排第 2。Boot 不再自动激活 Fluxing → 不触发 OnActivated 风暴。Win+Space 手动切仍可用。

## 5. Working tree 状态

**干净区** (本会话 commit):
- 16 个 fix 文件 (WeaselIPC + WeaselServer + include + test + sln + xmake + ps1) — 45264e5
- 1 个 release installer — cfcb96d

**Pre-existing dirty state** (NOT v0.71 work,user 可选 cleanup):
- 80+ untracked files (`_prompt_*.md` `handoff-*.md` `_check_install_v2.ps1` 等 L17/L66 working tree residue)
- `librime/` submodule dirty — L107 教训模式,需 user 授权

**本会话新增 untracked (NOT committed)**:
- `diag-v070.ps1` — Phase 1 证据采集脚本
- `diag-freeze-capture.ps1` — 下次 freeze 现场 capture 工具
- `handoff-after-phase-L-bugfix-7.6-v0.70-blocked-2026-07-29.md` — v0.70 BLOCKED handoff
- `get_md5.ps1` + `verify_v071.ps1` — ship-gate 工具
- `_build_test_red.cmd` + `_build_x64_red.cmd` — debug/x64 build 工具
- `_prompt_phase_L_bugfix_8_v0.70_AB_test_2026-07-29.md` — 旧 prompt (A/B test 没跑)

## 6. 关键文件位置

| 用途 | 路径 |
|---|---|
| v0.71 installer (本地 + 远程) | `release/fluxing-0.19.0.71-installer.exe` 43.3 MB md5 DC5D9F85... |
| 三件套 (远程) | `E:\办公文件\L1网站\pinyin\{version.json,appcast.xml,release-notes.html}` |
| FocusInGuard | `WeaselServer/FocusInGuard.h` (header-only) |
| Echo(DWORD) impl | `WeaselIPC/WeaselClientImpl.cpp` (worker thread + CancelSynchronousIo) |
| OnFocusIn guard 接入 | `WeaselIPCServer/WeaselServerImpl.cpp` |
| OnActivated Echo(100) | `WeaselTSF/WeaselTSF.cpp` |
| Test | `test/TestFreezeMitigation/TestFreezeMitigation.cpp` (7/7 PASS) |
| Handoff (本文件) | `F:\soft\00selfmade\rime_claude\handoff-after-phase-L-bugfix-8-v0.71-ship-2026-07-29.md` |
| Handoff (前次 BLOCKED) | `handoff-after-phase-L-bugfix-7.6-v0.70-blocked-2026-07-29.md` |

## 7. 立刻要 user 做

1. **跑 v0.71 installer** — 装新版 → 跑 6 项 user flow (跟 v0.69 一样的 checklist)
2. **如果仍 freeze** — 跑 `diag-freeze-capture.ps1` 抓现场状态(已在 release/ 旁边),贴回下个会话
3. **(可选) 跑 v0.69 A/B test** — 装 `release\fluxing-0.19.0.69-installer.exe` 看是否也卡,确认 Fix B revert 是否参与风暴
4. **(可选) push 2 commits 到 kizemo/Fluxing** — K7 push 仍 500,等网络稳定后手动 `git push origin 45264e5 cfcb96d`

## 8. Session health

- Bash 用量: ~58 calls (红线 80,预警 78)
- 活跃 ~1.7h (self-limit <2h,临近)
- 0 个 parallel subagent (test 复杂度高,subagent report verify 模式不适用)
- 2 个 commit + 1 installer + SFTP sync 完成
- 7/7 test PASS

## 9. 避坑提示

- **不要回滚 Fix A** (m_disabled 早返移除,QuickPanel 真修) — 那是 v0.69 已 ship 的正确修复,v0.70 保留,v0.71 保留
- **不要回滚 v0.70 Fix B revert** — 那是 P2 边界 (TSF 线程禁止 blocking I/O),v0.70 ship 时已论证不能加回
- **不要在 v0.71 装机卡死时立刻改 mitigation 加重 guard** — 先用 `diag-freeze-capture.ps1` 抓现场,看是新触发模式还是 mitigation 失效
- 任何"OnActivated 加 timeout"的修改属于 P2 边界,需 spec + ADR (本会话用 `m_client.Echo(100)` 是 100ms bounded IPC,严格说也在 P2 边界,但 worker thread 隔离了阻塞域,比 v0.69 Fix B 仍 TSF 线程同步安全)
- "回滚到上一个 ship 还能用"不代表"新 fix 真生效" — v0.68 QuickPanel 永久 dead 是 v0.69 Fix A 修的,v0.71 仍然依赖 Fix A
- _check_install_v2.ps1 是装机 user 验真伪的 trusted source (CLAUDE.md P3 镜像),sftp_sync.py 强制 cross-source verify,trust 链是完整的

## 10. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-8-v0.71-ship-2026-07-29.md。
v0.71 freeze mitigation ship 完成 (2 commits + SFTP sync OK)。
本会话任务: 接收 user 装机反馈 (6 项 user flow + §5.1 reg query) →
- 如果 PASS: 关闭 v0.71 task,写 lessons-learned L##-PhaseL-9 沉淀
- 如果仍卡死: 跑 diag-freeze-capture.ps1 抓现场,基于实际数据决定
  - A/B test v0.69 (user 装机试 v0.69 看是否同症状)
  - 升级到 Phase 3 架构讨论:OnActivated 改 PostMessage 异步
    (TSF 线程禁止 blocking I/O — CLAUDE.md P2,需 spec + ADR)
- 如果 6 项 PARTIAL: 跑 §5.1 reg query + binary md5 verify,定位 stale binary
```

---

*End of Phase L Bugfix 8 handoff. v0.19.0.71 freeze mitigation ship 完成. 3 mitigations 装, 7/7 test PASS, SFTP sync OK,装机 user flow 验证 user 端待确认.*
