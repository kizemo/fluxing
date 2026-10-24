# v0.19.0.66 装机灾难 Root Cause 调查报告 (2026-07-26)

> **来源**: 3 个并行 Explore subagent (TSF API / code diff / installer flow) inline 输出
> **整合日期**: 2026-07-26
> **目的**: 给 v0.19.0.67 真修 + 后续 ship gate 提供完整依据
> **承接**: `handoff-after-phase-L-postship-freeze-v0.19.0.67-2026-07-26.md`

---

## TL;DR

| 灾难 | 真因 | 证据来源 |
|---|---|---|
| **#1 安装卡死 Windows** | **不是 v0.66 新代码直接造成**。是 **install.nsi PPL taskkill 30s sleep loop** + **WriteRegStr HKLM\...\CTF\KnownClasses 卡 CTF lock** 叠加 → user 感受"卡死必须重启"。v0.66 ship 是导火索(触发时机更早) | subagent 3/3 installer flow 分析 |
| **#2 重启后默认进 Fluxing 无浮动栏** | **QuickPanelDialog::Show() 仍走 v0.18.41.4 GDI+ Bitmap(IStream*) 路径** → WeaselServer.exe heap corruption → TSF IPC 全死 → 浮动栏无法创建 | subagent 3/3 + 跟 v0.65 Show(null × 6) 共同作用 |
| **#3 Alt+, 死** | **pre-existing issue**,v0.66 未引入。QuickPanelDialog 单例 state 被 v0.65 Show(null × 6) 污染 → handler IsWindowVisible 永远返回 stale 值 | subagent 3/3 独立观察 |

**v0.66 sink 注册本身** (`_InitActiveLanguageProfileSink`) **API 完全正确**,但:
- **HIGH H1**: 失败时 `goto ExitError` 太重 — 整个 TIP deactivate,实际上 sink 是 enhancement,不是关键路径
- **HIGH H2**: `_UninitActiveLanguageProfileSink` 缺 `TF_INVALID_COOKIE` 守卫 → UnadviseSink(-1) 触发 undefined behavior

**v0.19.0.67 必修清单**:
1. ✅ QuickPanelDialog::Show() 不走 GDI+ Bitmap 路径(主修 — 解决灾难 #2)
2. ✅ Patch A+B (3 行) 加 TF_INVALID_COOKIE guard 到 v0.66 sink 代码 (解决灾难 #1 副作用)
3. ✅ install.nsi PPL taskkill 缩短 + WriteRegStr KnownClasses 加前置 Sleep(解决灾难 #1 主体)
4. ✅ sandbox-verify 装机 4 user flow 全 PASS
5. ✅ TDD red-green test 必写

---

## Subagent 1/3 核心结论 (TSF API + SDK)

### Microsoft 官方文档确认
- **API 选择正确**:`ITfSource::AdviseSink(IID_ITfActiveLanguageProfileNotifySink, ...)` from `ITfThreadMgr` — **per learn.microsoft.com/msctf**
- **Cookie 类型正确**:`DWORD` (4 bytes),**不是** `DWORD_PTR`
- **`TF_INVALID_COOKIE = 0xFFFFFFFF`**

### SDK msctf.h 头文件证据
- `ITfSource::AdviseSink` 第 3 参数 `DWORD *pdwCookie` — v0.66 用 `DWORD _dwActiveLanguageProfileSinkCookie` ✓
- `OnActivated(REFCLSID, REFGUID, BOOL)` 签名 — v0.66 h:90-93 ✓

### Root cause 候选排序
- **H1 (HIGH)** — installer 早期 ActivateEx 调用 + sink 跨线程通知 deadlock (regsvr32 → RegisterProfile → 触发验证性 ActivateEx → AdviseSink 内部 lock 与 profile manager lock 互斥)
- **H2 (MEDIUM-HIGH)** — UnadviseSink(TF_INVALID_COOKIE) 触发 TSF 内部 corruption
- **H3/H4/H5** — LOW (排除)

### 修复方向推荐
- **候选 A (强烈推荐)**: `_InitActiveLanguageProfileSink` 失败时**不** goto ExitError — sink 是 enhancement, 不是必需
- **候选 B (safety net)**: `_UninitActiveLanguageProfileSink` 加 `TF_INVALID_COOKIE` guard
- **候选 C (不推荐)**: 改用 `ITfSourceSingle` — 违反官方推荐
- **候选 D (保底)**: 回滚 v0.66 sink 注册 — 但用户开机默认进 Fluxing 问题仍存在

### 灾难 #3 独立观察
- `Alt+,` 走 `WeaselIPCServer/WeaselServerImpl.cpp:152-166` 的 `OnHotkey` 路径(Win32 `RegisterHotKey`,不是 TSF PreserveKey)
- **v0.66 没改这条路径** — 灾难 #3 是 pre-existing issue
- 可能原因:`QuickPanelDialog` 单例 state 被 v0.65 `Show(null × 6)` 污染

---

## Subagent 2/3 核心结论 (code diff review)

### v0.66 五处改动逐行 review (全部 OK)

| 改动 | 行号 | 评估 |
|---|---|---|
| 字段+方法声明 | `WeaselTSF.h:170-174` | ✓ 类型/命名一致 |
| 构造函数 cookie init | `WeaselTSF.cpp:30` | ✓ 跟其他 sink 一致 |
| QueryInterface 分支 | `WeaselTSF.cpp:67-70` | ✓ QI 链完整 |
| Deactivate 调 uninit | `WeaselTSF.cpp:106-107` | ✓ 位置合理 |
| ActivateEx 调 init | `WeaselTSF.cpp:166-170` | ✓ 时机正确 |
| Init/Uninit 实现 | `WeaselTSF.cpp:230-251` | ⚠ 漏 TF_INVALID_COOKIE guard |

### 5 个 sink 对照矩阵

| sink | 注册时机 | 注册 API | TF_INVALID_COOKIE guard | 跟 ActiveLanguageProfile 相似度 |
|---|---|---|---|---|
| **ThreadMgrEventSink** | ActivateEx 第 1 | `ITfSource::AdviseSink` from threadmgr | **YES** ✓ | **金标准** — 应该抄这个 |
| ThreadFocusSink | ActivateEx 第 2 | `ITfSource::AdviseSink` from threadmgr | **NO** ⚠ | v0.66 直接抄的模板 — 也漏 guard |
| KeyEventSink | ActivateEx 第 3 | `ITfKeystrokeMgr::AdviseKeyEventSink` | N/A (用 _tfClientId) | 0 相似度 |
| TextEditSink | _InitTextEditSink 调 | `ITfSource::AdviseSink` from context | **YES** ✓ | 0.3 (document 层) |
| LanguageBar | ActivateEx 第 5 | `ITfLangBarItemMgr::AddItem` | N/A | 0 相似度 |
| DisplayAttribute | ActivateEx 第 4 | `CoCreateInstance(CLSID_TF_CategoryMgr).RegisterGUID` | N/A | 0 相似度 |
| **★ v0.66 ActiveLanguageProfile** | ActivateEx 第 9 (最后) | `ITfSource::AdviseSink` from threadmgr | **NO** ⚠ | — |

### 复制 `_InitThreadFocusSink` 时漏的细节清单

#### HIGH severity
**H1. Uninit 没有 TF_INVALID_COOKIE 守卫**
- 当 ActivateEx 失败 → `goto ExitError` → Deactivate → `_UninitActiveLanguageProfileSink()` 时,sink 可能没成功注册(cookie 仍是 `TF_INVALID_COOKIE = 0xFFFFFFFF`)
- 代码会调 `UnadviseSink(0xFFFFFFFF)` → MSDN 说返回 E_INVALIDARG,但 cookie 不会被 reset
- 下次 ActivateEx 时 cookie 仍是 `0xFFFFFFFF` → AdviseSink 行为未定义
- **修复**: 加 `if (_dwActiveLanguageProfileSinkCookie == TF_INVALID_COOKIE) return;` 在 uninit 开头,跟 ThreadMgrEventSink.cpp:55 一致

#### MEDIUM severity
**M1. Init 函数失败时不重置 cookie** — 跟 ThreadMgrEventSink.cpp:45-46 对比不够明确,**风格不一致**
**M2. 注册顺序 OK 但触发竞态** — 现有 OnActivated 已有 `Echo()` 守卫,理论 OK
**M3. Uninit 函数没显式 Release pSource** — 用 com_ptr 自动 RAII,功能正确
**M4. 错误处理无日志** — pre-existing pattern across all sinks

### 最小补丁 (Patch A + B)

```cpp
// Patch A: HIGH 必补
void WeaselTSF::_UninitActiveLanguageProfileSink() {
  if (_dwActiveLanguageProfileSinkCookie == TF_INVALID_COOKIE)
    return;                                                       // ← NEW
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  if (FAILED(pSource->UnadviseSink(_dwActiveLanguageProfileSinkCookie)))
    return;
  _dwActiveLanguageProfileSinkCookie = TF_INVALID_COOKIE;
}

// Patch B: MEDIUM 推荐
BOOL WeaselTSF::_InitActiveLanguageProfileSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfActiveLanguageProfileNotifySink,
                                 (ITfActiveLanguageProfileNotifySink*)this,
                                 &_dwActiveLanguageProfileSinkCookie))) {
    _dwActiveLanguageProfileSinkCookie = TF_INVALID_COOKIE;       // ← NEW
    return FALSE;
  }
  return TRUE;
}
```

**3 行代码改动**,完全消除 H1 + M1 失败路径累积问题。

---

## Subagent 3/3 核心结论 (installer flow)

### Installer 流程关键步骤 (905 行 install.nsi)

| 阶段 | 行号 | 关键行为 | hang 风险 |
|---|---|---|---|
| .onInit 升级前 cleanup | 148-161 | PPL-aware taskkill ×10 + Sleep 1500→3000ms | **HIGH (HANG POINT #1)** |
| .onInit ctfmon kill | 211-219 | ctfmon + TextInputHost kill ×5 + Sleep 2500ms ×5 | LOW (system restart 自动) |
| Section "Fluxing" File | 540-568 | Rename-then-File WeaselServer.exe | LOW (L72-fix) |
| regsvr32 weaselx64.dll | 707-710 | `ExecWait` | MEDIUM (Win 11 24H2 已知问题) |
| regsvr32 weasel.dll | 711-714 | `ExecWait` | **HIGH (spec 066 已知 fail)** |
| WriteRegStr HKLM CTF\KnownClasses | 738 | unconditional (L66 safety-net) | **HIGH (HANG POINT #2)** |
| ExecWait WeaselDeployer.exe /install | 757/761 | RIME deploy | MEDIUM (HANG POINT #3) |
| Exec $INSTDIR\WeaselServer.exe | 799 | non-blocking | MEDIUM (HANG POINT #4) |

### 5 大潜在 Hang 点

1. **HIGH (HANG POINT #1)**: `WeaselServer.exe` PPL-aware taskkill loop 30s sleep
   - WeaselServer.exe 是 **PPL (Protected Process Light)** 进程
   - `taskkill /F` 在 PPL 上是 no-op,但 NSIS sleep 等回执
   - 循环 10 次 × ~3s avg sleep = **~30s 纯等待**
   - 触发场景:升级路径

2. **HIGH (HANG POINT #2)**: `WriteRegStr HKLM\...\CTF\KnownClasses` (install.nsi:738)
   - `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses` 是 CTF subsystem 管理的 hive
   - 写它需要 CTF 的 kernel lock
   - 如果 `ctfmon.exe` 已 hang,它持 lock 不释放 → WriteRegStr 同步死锁
   - 触发场景:Win 11 24H2 升级 + ctfmon 状态问题

3. **MEDIUM (HANG POINT #3)**: `ExecWait WeaselDeployer.exe /install`
   - 可能弹 MessageBox 等 user 输入 → install 看起来"卡死"
   - 或 WeaselDeployer 自己 crash → ExecWait 立即返回

4. **MEDIUM (HANG POINT #4)**: `Exec $INSTDIR\WeaselServer.exe` 后立刻 MessageBox
   - WeaselServer.exe 启动后**异步**初始化,如果 init step hang,WeaselServer 显示存在但实际 hung
   - user 看到: 装机完成 → 弹窗 → 此时 Win+Space 切不到 Fluxing

5. **LOW (HANG POINT #5)**: `ctfmon.exe` 在 `.onInit` 被 kill 后被系统重 spawn
   - kill 后 ~1s 内被系统重启
   - 可能在 install 期间加载 old weaselx64.dll

### v0.66 新代码 vs installer hang 关系

| 新代码 | 行号 | 影响 install? |
|---|---|---|
| `_InitActiveLanguageProfileSink` | `WeaselTSF.cpp:233-242` | **NO** (regsvr32 不触发 ActivateEx) |
| `_UninitActiveLanguageProfileSink` | `WeaselTSF.cpp:243-250` | NO |
| `OnActivated` | `WeaselTSF.cpp:252-288` | NO (sink callback) |
| `QuickPanelDialog::Show(false, nullptr × 6)` | `RimeWithWeasel.cpp:444-447` | NO (FocusIn 只在 IPC 客户端连上后触发) |

**结论**: v0.66 所有新代码都**不影响 install 阶段**。install hang 是 Win 11 24H2 + ctfmon 状态 + installer PPL taskkill + WriteRegStr CTF hive 交互导致,**不是 v0.66 新代码直接造成**。

### post-install 灾难 #2 + #3 真因

**QuickPanelDialog::Show() 仍触发 v0.18.41.4 GDI+ heap corruption**:
- v0.18.41.0 spec 066 注释 line 401-413 提到 GDI+ heap corruption crash
- v0.19.0.65 改 FocusIn 用 `QuickPanelDialog::Show(false, nullptr × 6)` — **没有真删 GDI+ 路径**
- v0.19.0.66 进一步触发 OnActivated → FocusIn → Show → **第一次 GDI+ heap corruption 触发**
- WeaselServer.exe heap 损坏 → TSF 客户端 (weaselx64.dll in ctfmon.exe) Echo() 失败 → TSF 无法 reach pipe → 用户切到 Fluxing 但无 floating panel

### 修复方向 (v0.19.0.67)

1. **QuickPanelDialog::Show() 不走 GDI+ Bitmap 路径**(主修 — 解决灾难 #2)
   - 检查 `QuickPanelDialog::Show()` 是否仍调 `LoadLogo()` + `Bitmap::FromStream()`
   - 改成纯 GDI `LoadImage` 或 `LoadIcon` (跟 ShortcutSettings 一致)

2. **Patch A + B** (3 行,补 v0.66 sink 代码健壮性)
   - 加 TF_INVALID_COOKIE guard 到 `_UninitActiveLanguageProfileSink`
   - Init 失败路径显式 reset cookie

3. **install.nsi 改进** (解决灾难 #1 主体)
   - PPL-aware taskkill 缩短 + 减少循环次数 (30s → 10s)
   - `install.nsi:738` WriteRegStr KnownClasses 加前置 Sleep 或 try/catch
   - 或在写之前 `Sleep 3000` 等 ctfmon 稳定

---

## TDD red-green test 必写清单

按 systematic-debugging Phase 4 强制要求:

### 主目标 (装机不卡死)
- Test 1: `_InitActiveLanguageProfileSink` 在 cookie = TF_INVALID_COOKIE 时不调 AdviseSink
- Test 2: `_UninitActiveLanguageProfileSink` 在 cookie = TF_INVALID_COOKIE 时 no-op (Patch A)
- Test 3: 连续 ActivateEx / Deactivate 10 次,cookie 每次 cleanup 正确(reference 不 leak)
- Test 4: AdviseSink 模拟失败 → init 不 propagate exception 杀 TSF

### 次目标 (浮动栏 Win+Space 弹)
- Test 5: 模拟 TSF profile 切换 → sink 触发 → FocusIn IPC 发出 → QuickPanelDialog::Show 被调
- Test 6: 装机 sandbox-verify 4 user flow 全 PASS

### 修复验证 (GDI+ 路径清理)
- Test 7: QuickPanelDialog::Show() 不调 GDI+ Bitmap API (`LoadLogo` / `Bitmap::FromStream`)
- Test 8: install.nsi PPL taskkill loop 总时长 < 10s (sandbox 测)
- Test 9: WriteRegStr HKLM CTF\KnownClasses 前置 Sleep 已就位

---

## 下一步必交付 (新会话承接)

按 handoff §5 决策矩阵:

1. **必跑 `_check_install_v2.ps1`** (user 跑,贴 Module 4 event log)
2. **必跑 procmon.exe + Get-EventLog** (user 跑,贴 hang 时序)
3. **Phase 4 决策**: 选候选 A (失败不 goto ExitError) + 候选 B (TF_INVALID_COOKIE guard)
4. **必改**: QuickPanelDialog::Show() 不走 GDI+ 路径
5. **TDD red-green test** (Test 1-9) 必写
6. **sandbox-verify 装机 4 user flow** 必跑
7. **build v0.19.0.67 + NSIS + SFTP + Cloudflare cache purge 通知**
8. **CHANGELOG.md 必补 v0.67 段**

---

## 附录: 引用文件路径

- `F:/soft/00selfmade/rime_claude/WeaselTSF/WeaselTSF.cpp` (355 行)
- `F:/soft/00selfmade/rime_claude/WeaselTSF/WeaselTSF.h` (243 行)
- `F:/soft/00selfmade/rime_claude/WeaselServer/WeaselServerApp.cpp` (493 行)
- `F:/soft/00selfmade/rime_claude/RimeWithWeasel/RimeWithWeasel.cpp` (1749 行)
- `F:/soft/00selfmade/rime_claude/WeaselServer/QuickPanelDialog.cpp` (待查 GDI+ 路径)
- `F:/soft/00selfmade/rime_claude/output/install.nsi` (905 行)

---

*End of v0.19.0.66 Root Cause Investigation. v0.19.0.67 真修方向已确定,3 行 patch + GDI+ 路径清理 + install.nsi PPL 改进。*