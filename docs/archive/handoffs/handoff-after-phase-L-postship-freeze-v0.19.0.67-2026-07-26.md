# Handoff — Phase L Postship Freeze v0.19.0.67 (2026-07-26)

> **承接**: `handoff-after-phase-L-bugfix-3-ship-v0.19.0.66-2026-07-25.md` (v0.19.0.66 ship)
> **新事故 (本次装机反馈)**: **3 个严重问题**
> **状态**: 🚨 **v0.19.0.66 ship 灾难级回归 — 必须 v0.19.0.67 真修才能继续**
> **方向**: systematic-debugging Phase 1-4 → 真因定位 → TDD red-green 验证 → 真修 → 装机真验

---

## 1. user 装机端 3 大灾难性反馈 (2026-07-26)

| # | 现象 | 严重度 | 推测影响 |
|---|---|---|---|
| **1** | **安装过程出现报错，并且导致 Windows 卡死，必须重启** | 🚨🚨🚨 系统级 | 装机直接 fail,任何装机用户都会卡死 |
| **2** | **重启后，自动进入 fluxing 输入法，没有设置栏出现** | 🚨🚨 | v0.19.0.66 修的 sink 完全失效,且 IME 默认激活 = 用户一开机就卡在 fluxing |
| **3** | **使用快捷键，无法调出快捷设置栏** | 🚨🚨 | Alt+, 等 hotkey 全死,基本输入法控制失效 |

**重要**: 之前 v0.19.0.65 也 fail 4 个 user flow,但 user 还能正常用 Windows 装其他输入法(虽然 fluxing 4 flow 不 work)。现在 **v0.19.0.66 直接让 Windows 卡死**,装机层面是灾难性回归。

---

## 2. 改动 diff 范围 (v0.19.0.66)

跟 handoff-after-phase-L-bugfix-3-ship-v0.19.0.66-2026-07-25.md §2 一致:

| 文件 | 改动 |
|---|---|
| `WeaselTSF/WeaselTSF.h` | +5 行:`_InitActiveLanguageProfileSink` / `_UninitActiveLanguageProfileSink` 方法声明 + `_dwActiveLanguageProfileSinkCookie` 字段 |
| `WeaselTSF/WeaselTSF.cpp` 构造函数 (cpp:30) | +1 行:cookie 字段初始化 `TF_INVALID_COOKIE` |
| `WeaselTSF/WeaselTSF.cpp` QueryInterface (cpp:67-70) | +3 行:`IID_ITfActiveLanguageProfileNotifySink` 分支 |
| `WeaselTSF/WeaselTSF.cpp` Deactivate (cpp:107-108) | +2 行:`_UninitActiveLanguageProfileSink()` 调用 |
| `WeaselTSF/WeaselTSF.cpp` ActivateEx (cpp:166-169) | +4 行:`_InitActiveLanguageProfileSink()` 调用 + goto ExitError |
| `WeaselTSF/WeaselTSF.cpp` init/uninit 实现 (cpp:230-251) | +22 行:仿 `_InitThreadFocusSink` 模板 |

**核心嫌疑**:`_InitActiveLanguageProfileSink` 模仿 `_InitThreadFocusSink` 模板,但 **TSF sink 之间的注册语义不是模板化的**——不同 sink 在 ActivateEx 时机、UnadviseSink 失败处理、Cookie 类型等细节都不同。

---

## 3. 5 维度 Root Cause 假设矩阵 (按可能性排序)

按 systematic-debugging Phase 1 强制要求,**严禁盲目试错**,每个假设必须有装机端证据支撑才进入 Phase 4。

### H1 (HIGH) — Sink 注册 deadlock 装机进程
- **机制**: installer 跑 `regsvr32 weaselx64.dll` → TSF subsystem 加载 weaselx64.dll → `DllGetClassObject` → `ITfTextInputProcessorEx::ActivateEx` 被 TSF manager 调 → 我们的 `_InitActiveLanguageProfileSink` 调 `m_pThreadMgr->AdviseSink(IID_ITfActiveLanguageProfileNotifySink, ...)` → AdviseSink 在 regsvr32 上下文里阻塞(等 ctfmon.exe / TextInputHost.exe 释放锁) → 整个 regsvr32 挂死 → Windows installer transaction 卡 → Windows 卡死
- **证据需求**: 装机端 `weasel-install.log` 看 `[ActivateEx]` 顺序 + `[ImmGetDefaultIMEWnd] ...` 时序 + process monitor 看 regsvr32 hang 点

### H2 (HIGH) — Deactivate 漏 UnadviseSink → 装机完卸载老 binary 时 crash
- **机制**: 我们的 `_UninitActiveLanguageProfileSink` 模仿模板,但 **TF_INVALID_COOKIE 检查缺失** → 如果 ActivateEx 在 Deactivate 前未成功(m_cookie = TF_INVALID_COOKIE),UnadviseSink 调无效 cookie → TSF 内部错误 → 关机/重启时 ctfmon.exe 崩溃 → Windows 卡在 shutdown
- **证据需求**: 装机端 Application Error event log (Application Error + AppHang filter "ctfmon|weasel|fluxing|textinputhost") + 我们 init/uninit 的 cookie 检查代码 review

### H3 (MEDIUM) — Cookie 字段类型错 (DWORD vs DWORD_PTR)
- **机制**: 现有代码 `_dwThreadFocusSinkCookie` 等字段用 DWORD 类型,但 `ITfAdviseSink::AdviseSink` 第二个参数是 `DWORD*` (cookie 是 32-bit)。我们的 `_dwActiveLanguageProfileSinkCookie` 字段类型正确吗?
- **证据需求**: read `WeaselTSF.h:90-93` 字段定义 + 跟 `_dwThreadFocusSinkCookie` 对比类型

### H4 (MEDIUM) — 注册时机错(应在 ITfThreadMgrEventSink::OnInitThreadMgr 调,不 在 ActivateEx)
- **机制**: ActiveLanguageProfileNotifySink 是 **profile-level** 通知,不是 thread-level — 应该在 ThreadMgrEventSink::OnInitThreadMgr 调一次,而不是 ActivateEx 每个 text input 调一次。错时机 → 多次 AdviseSink → TSF 内部 reference counting 错误 → 系统级 corruption
- **证据需求**: Microsoft TSF 文档 + 现有 `_InitThreadMgrEventSink` 实现位置对比

### H5 (LOW) — Deactivate 中 `_UninitActiveLanguageProfileSink` 在 profile switch 时 leak
- **机制**: `Deactivate` 调 `_UninitActiveLanguageProfileSink`,但 ITfActiveLanguageProfileNotifySink 是 **跨多个 profile** 的通知,不应在每次 Deactivate unadvise。错实现 → memory leak + cookie stale → 装机/重启时 TSF 状态不一致
- **证据需求**: code review Deactivate 路径

---

## 4. 取证任务清单 (next session 第一件事)

按 systematic-debugging Phase 1 — 采证,不修代码。

### 4.1 装机端 _check_install_v2.ps1 输出 (user 跑)
```powershell
powershell -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\_check_install_v2.ps1
```
必跑:**Module 4 (Event log)** — Application Error / AppHang / .NET Runtime 最近 24h,filter `weasel|fluxing|ctfmon|textinputhost`

### 4.2 Application Event Log dump (user 跑)
```powershell
Get-EventLog -LogName Application -Newest 100 -EntryType Error,Warning | Where-Object {$_.Source -match "weasel|fluxing|ctfmon|TextInputHost|Application Hang"} | Format-List TimeGenerated,Source,Message
```
必贴:**TimeGenerated + Source + 第一段 Message**

### 4.3 Process Monitor 抓 regsvr32 hang (user 跑,需要 sysinternals)
- 下载 `procmon.exe` → 启动 → filter "Process Name is regsvr32.exe"
- 重装 v0.19.0.66 → 等卡死 → 保存 PML → 转 CSV → 贴最后 30s 的 call stack

### 4.4 Code review (next session 跑)
- Read `WeaselTSF.h:90-93` — 字段类型对比
- Read `WeaselTSF.cpp:230-251` — init/uninit 实现逐行 vs `_InitThreadFocusSink`
- Read `WeaselTSF.cpp:166-169` — ActivateEx 调用时机 vs 文档

### 4.5 派 3 个 Explore subagent (并行,Phase 1)
- **Subagent 1**: 查 ITfActiveLanguageProfileNotifySink 在 Microsoft 官方文档 + Windows SDK 头文件的注册语义
- **Subagent 2**: 查现有 `_InitThreadFocusSink` 实现 + 类比 ActiveLanguageProfile 的正确时机
- **Subagent 3**: 查 5 个 sink (ThreadFocus/KeyEvent/ThreadMgrEvent/LanguageBar/DisplayAttribute) 各自的 cookie lifecycle,确认哪个最接近 ActiveLanguageProfile 的注册模式

---

## 5. 修复方向 (Phase 4 后定,先列候选)

> **🆕 2026-07-26: 3 个并行 Explore subagent 调查完成 — 完整报告见 `memory/2026-07-26-v066-disaster-rootcause.md`**
> **关键发现**: 真因比假设矩阵更复杂 — install hang 不是 v0.66 直接造成,是 PPL taskkill + WriteRegStr CTF hive 叠加;post-install 灾难是 QuickPanelDialog::Show() 仍走 v0.18.41.4 GDI+ 路径

### 候选 A — **强烈推荐** (H1+H2 综合修): 让 sink init 失败不 goto ExitError + TF_INVALID_COOKIE guard
- 失败时不 propagate 到 ExitError (sink 是 enhancement,不是关键路径)
- `_UninitActiveLanguageProfileSink` 加 `if (cookie == TF_INVALID_COOKIE) return;`
- 3 行代码改动 (Patch A + B 组合),完全消除 H1 + M1 失败路径累积问题
- 参考金标准: `_UninitThreadMgrEventSink` 在 `ThreadMgrEventSink.cpp:55`

### 候选 B — **必补 (主修灾难 #2)**: QuickPanelDialog::Show() 不走 GDI+ Bitmap 路径
- 检查 `WeaselServer/QuickPanelDialog.cpp` Show() 是否仍调 `LoadLogo()` + `Bitmap::FromStream()`
- 改成纯 GDI `LoadImage` / `LoadIcon` (跟 `ShortcutSettings` 一致)
- 这是灾难 #2 (默认进 Fluxing 无浮动栏) 的**真因** — v0.18.41.0 时代的老 bug,在 v0.65/v0.66 改动中没真删
- 修复后,Win+Space 切 Fluxing → Show() 不触发 GDI+ heap corruption → 浮动栏正常弹

### 候选 C — **强烈推荐** (解灾难 #1 主体): install.nsi PPL taskkill 缩短 + WriteRegStr KnownClasses 加前置 Sleep
- `install.nsi:148-161` PPL WeaselServer.exe taskkill loop 30s sleep → 改成 10s
- `install.nsi:738` WriteRegStr HKLM\...\CTF\KnownClasses 前置 Sleep 3000ms (等 ctfmon 稳定)
- 这是灾难 #1 (安装卡死 Windows) 的**主体**

### 候选 D — 保底 (不推荐): 回滚 v0.19.0.66 sink 注册
- 撤回 5 文件 36 行改动
- 浮动栏弹不出来回到 v0.65 状态(Win+Space 不弹,但 Alt+, work)
- 风险: 灾难 #2 仍未修 (QuickPanelDialog GDI+ 路径)

### 候选 E — 不推荐: 改用 ITfSourceSingle
- Microsoft Learn 明确推荐 ITfSource::AdviseSink 路径
- 改用 AdviseSingleSink 反而违反官方推荐

---

## 6. TDD red-green test 必写 (Phase 4.5 必做)

**Test: 装机不卡死**(主目标 — 这是 user 报告的灾难)
- Test 1: `_InitActiveLanguageProfileSink` 在 cookie = TF_INVALID_COOKIE 时不调 AdviseSink
- Test 2: `_UninitActiveLanguageProfileSink` 在 cookie = TF_INVALID_COOKIE 时不调 UnadviseSink
- Test 3: 连续 ActivateEx / Deactivate 10 次,cookie 每次 cleanup 正确(reference 不 leak)
- Test 4: AdviseSink 模拟失败 → init 不 propagate exception 杀 TSF

**Test: 浮动栏 Win+Space 弹** (次目标 — Phase L 调整 3)
- Test 5: 模拟 TSF profile 切换 → sink 触发 → FocusIn IPC 发出 → QuickPanelDialog::Show 被调
- Test 6: 装机 sandbox-verify 4 user flow 全 PASS

---

## 7. 严格禁止 (本阶段)

- ❌ 改 install.nsi **除非确定 freeze 跟 install.nsi 直接相关**(NSIS write 是 L66 safety-net trap,慎重)
- ❌ 改 QuickPanelDialog.cpp 内部(除非所有调用方都不 work)
- ❌ 改 Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点
- ❌ L108 stale binary ship
- ❌ `if (false)` 残留 (Trigger L3 gate)
- ❌ **盲目试错改 sink 注册代码** — 必须 Phase 1-4 走通才动代码

---

## 8. ship 链 (v0.19.0.67 待定)

```
1. Phase 1 采证 (4.1-4.5) — 不改代码
2. Phase 2 综合 + Phase 3 假设验证 — 不改代码  
3. Phase 4 真因定位 — 不改代码
4. Phase 4.5 user 决策修复方向 (候选 A/B/C/D) — 不改代码
5. Phase 5 TDD red-green test 写 + 修源码
6. trigger L3 gate: grep -RIn "if (false)" + /t:Rebuild smoke test
7. sandbox-verify 4 user flow (新装 → Win+Space 弹 → Alt+, toggle)
8. build v0.19.0.67
9. _check_install_v2.ps1 期望 md5 同步更新
10. CHANGELOG.md v0.67 段补 (本会话漏了 v0.66,这次必补)
11. commit + tag + SFTP atomic upload + Cloudflare cache purge 通知
```

---

## 9. K7 push (持续 fallback)

按 K7 教训: `git push kizemo` 持续 HTTP 500。装机端交付不依赖 GitHub 同步(SFTP 已成功)。

---

## 10. 装机端 expect verify (v0.19.0.67 ship 后)

| # | 操作 | 期望结果 |
|---|---|---|
| 1 | 装新 installer | **不卡死 Windows**,正常 30-60s 完成 |
| 2 | 重启 | Windows 正常进入桌面,无 ctfmon crash dialog |
| 3 | 默认输入法 | 系统默认输入法是上一个(中文/英文),**不是 fluxing** |
| 4 | 手动切到 fluxing (Win+Space) | **浮动栏自动弹** ✓ |
| 5 | Alt+, | **toggle 显/隐** ✓ |

---

## 11. 沉淀 (lessons-learned.md 候选)

| L## | 教训 | 状态 |
|---|---|---|
| **L##-PhaseL-8-SinkRegister-Deadlock** | TSF sink 注册必须在合适的 TSF 生命周期 hook 调,ActivateEx 不是所有 sink 的正确注册点。Copy-paste `_InitThreadFocusSink` 模板而不查 sink 语义 = 18 ship bug + 1 装机灾难 | 等 root cause 确认后写 |

---

## 12. 关联文件

| 文件 | 说明 |
|---|---|
| `handoff-after-phase-L-bugfix-3-ship-v0.19.0.66-2026-07-25.md` | v0.19.0.66 ship handoff (上一阶段) |
| `_verify_v066_recipe_2026-07-26.md` | v0.66 装机验证指南 (本会话写) |
| `_inject_v066_metadata.py` | metadata 注入脚本 (本会话写,可复用) |
| `release/v066-verify-pack/` | v0.66 装机验证包 (installer + recipe + check script) |

---

*End of Phase L Postship Freeze handoff. v0.19.0.66 装机灾难,next session 必走 systematic-debugging Phase 1-4 完整采证,严禁盲目试错。*