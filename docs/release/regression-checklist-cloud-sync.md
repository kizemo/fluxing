# 云同步回归清单（版本无关 · 长期）

> **范围**：Fluxing 云端账号同步（用户词典）+ 账号窗生命周期。
> **用法**：任何改动同步链路 / 账号窗 / TSF 会话生命周期的版本，发布前跑一遍。
> **口径真相源**：判据 = `include/FluxingMaintenanceGate.h`；决策 = `docs/adr/0062*.md`、`0065*.md`、`0066*.md`、`0067*.md`。
> **版本化的一次性清单**（装机后逐条走）见 `docs/release/restart-checklist-1.3.5.md`；本文件是**长期回归**，不随版本号改。

---

## 0. 前置与环境口径

| 项 | 口径 |
| --- | --- |
| 装机根 | `D:\Program Files\fluxing\weasel\`（注意：不是 `D:\Program Files\fluxing\`） |
| 版本判定 | `WeaselServer.exe` 的 FileVersion；AW 两个修复**只在 `WeaselServer.exe` 侧**（`weasel.dll` 不含）⇒ 判别要看 exe，不是 `weasel.dll` |
| 用户数据目录 | `D:\Program Files\fluxing\user1\fluxing`（真相源 `include/FluxingUserPaths.h` 的 `ResolveFluxingUserDataPath()`） |
| 登录态 | `cmdkey /list` 出现 `LegacyGeneric:target=Fluxing:CloudAccount` |
| 维护导出目录 | `<用户数据目录>\sync\<account-id>\`，其中 `rime_ice.userdb.txt` **仅在轮次通过闸门进入维护导出时重写** |
| ★ 核心判据代理 | **导出文件 mtime 前进 = 该轮被放行（闸门通过）**；mtime 不动 = 被跳过或提前失败 |
| 不要用状态文案当判据 | 状态文案在控制器内存 + 自绘渲染，跨进程读不可靠；跨进程 `WM_GETTEXT` 指针不封送 ⇒ 用 UIA 读控件文本（只作辅助证据） |

---

## 1. AC-G 组：开启输入法 ≠ 正在输入（缺陷 A，ADR 0062 / 0065）

**语义**：IME 会话存活本身**不阻塞**同步；只有「最近有输入」或「有未上屏组合串」才阻塞。

| 编号 | 场景 | 期望 | 判定方式 | 状态（2026-11-20 / 1.3.5） |
| --- | --- | --- | --- | --- |
| G-1 | 激活 IME 但**一个字都没打**，点「立即同步」 | 该轮**放行** | 导出文件 mtime 前进 | ✅ **已自动验证**（mtime 11:38:53 → 11:47:15） |
| G-2 | IME 全程存活、同步照常发生 | 不因「有会话」被跳过 | 同上 | ✅ 已自动验证（G-1 即覆盖） |
| G-3 | 激活后 **10 s 内**有按键 | 该轮**应被跳过**，文案「你正在输入，本轮已跳过用户词典同步，稍后会自动重试。」 | 导出文件 mtime 不动 + 状态行 | ⚠️ **未自动验证**（需制造真实输入历史，见 §5 待补项） |
| G-4 | 有未上屏组合串 | 阻塞到 **60 s** 宽限（比普通按键的 10 s 长） | 单元测试 + 真机 | ⚠️ 真机未验证；单元侧由 `TestMaintenanceGateIdleCriteria` 覆盖 |
| G-5 | 按键后 **≥10 s** 且无组合串 | 该轮**放行** | 导出文件 mtime 前进 | ⚠️ **未自动验证**（同 G-3） |
| G-6 | 闸门跳过**不等于失败** | 不进失败退避阶梯，改 **5 min** 短间隔重试 | 单元测试 + 观察同步节奏 | ✅ 单元侧覆盖 |

**为什么 G-3 / G-5 还没自动验**：要制造「真实输入历史 + 未上屏组合串」必须动用户真实词库与云端数据（不可逆风险），当前没有安全的注入点。

---

## 2. AC-W 组：账号窗生命周期（缺陷 B，ADR 0066）

**语义**：账号窗必须能反复打开 / 关闭；关闭后不得留下半死 HWND。

| 编号 | 场景 | 期望 | 判定方式 | 状态（2026-11-20 / 1.3.5） |
| --- | --- | --- | --- | --- |
| W-1 | 打开 → 关闭，连续 3 轮 | 每轮都**成功关闭**（`IsWindow` 变 FALSE），且每轮拿到**新的 hwnd** | 自动探针（BM_CLICK 关闭按钮） | ✅ **已自动验证**（hwnd 4590294 → 4655830 → 4721366） |
| W-2 | **第二次进入**账号页后关闭 | 能关闭（用户原话「第二次进入后已登录窗口无法关闭」） | 同 W-1 第 2 轮 | ✅ 已自动验证 |
| W-3 | 关闭按钮点击路径 | 统一走 `PostMessage(WM_CLOSE)`，不得在自身 `WM_LBUTTONUP` 未返回时被父窗同步销毁 | 源码 + 单元测试（`TestAccountWindowSlotState`） | ✅ 单元侧 + 源码侧覆盖 |
| W-4 | 窗口销毁时的槽位次序 | **先清槽位、后 Unregister**；`WM_NCDESTROY` 兜底 | 同上 | ✅ 单元侧 + 源码侧覆盖 |
| W-5 | 复用前探活 | 复用前发 `WM_NULL`，200 ms 内无响应 ⇒ 丢弃陈旧窗口另建 | 同上 | ✅ 单元侧覆盖 |
| W-6 | 关不掉时的取证 | 查 `[FluxingAccountDlg]` 诊断日志，区分「复用半死窗 / 新建即坏 / UI 线程卡在 WM_DESTROY」 | 日志 | ⚠️ 本轮未触发（没复现） |

---

## 3. 自动入口

```powershell
# 完整一轮（G 组 + W 组），产出 report.json + 每轮截图到 %TEMP%
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File F:\soft\00selfmade\rime_claude\scripts\verify\verify-cloud-account-window.ps1 `
  -Mode All -SyncWaitSeconds 150
```

| 参数 | 取值 | 说明 |
| --- | --- | --- |
| `-Mode` | `Discover` / `DefectB` / `DefectA` / `All` | Discover = 只读控件树与文本（排查用）；DefectB = W 组；DefectA = G-1；All = 全跑 |
| `-UserDataDir` | 默认 `D:\Program Files\fluxing\user1\fluxing` | 用户数据目录 |
| `-OutDir` | 默认 `%TEMP%\fluxing-ax-verify-<时间戳>` | 证据目录 |
| `-UseRealMouse` | 开关 | 用合成真实鼠标点击代替 `BM_CLICK`（验证真实点击路径） |
| `-SyncWaitSeconds` | 默认 150 | 点「立即同步」后等导出文件 mtime 前进的时长 |

**脚本约定**：
- 按钮一律按**控件 ID** 定位（不按中文文本）—— PowerShell 5.1 对无 BOM 的 `.ps1` 按 ANSI 解释，脚本里放中文字面量必然乱码。
- 账号窗打开靠向 **WeaselServer 进程自身主窗**（`m_server.GetHWnd()`）发 `WM_COMMAND(40024)`；发给托盘窗（`TrayIconClass`）**无效**（托盘 WndProc 只转发菜单选中）。
- 关闭 = 对控件 ID `1010`（`kIdClose`）发 `BM_CLICK`；「立即同步」= 控件 ID `1016`（`kIdSyncNow`）。
- 控件 ID 常量真相源：`WeaselServer/Cloud/CloudAccountUi.h` 的 `AccountControlId`。

---

## 4. 需用户在场 / 需第二台设备（本机无法闭环）

| 编号 | 场景 | 前置 |
| --- | --- | --- |
| M-1 | 亲手登录 `kizemo@163.com` 并看到账号页显示该邮箱 | 用户在场 + 已登录 |
| M-2 | 重启系统与服务后自动续期 | 需重启（`WeaselServer.exe` 是 **PPL** ⇒ 装机后必须注销/重启，旧映像仍在跑） |
| M-3 | 按 exe 路径做防火墙出站阻断 ⇒ 断网表现 | 用户在场 |
| M-4 | 点「清空云端数据」后回查 D1 + R2 | 用户在场 + 登录态 |
| M-5 | 双机 7 条（AC-03/04/05/08/09/13/14） | 第二台设备；`CloudCredentialStore` 是 **Windows 用户级全局单例** ⇒ 两套 Profile 目录不够 |
| M-6 | restore 端到端 | 第二台设备 |
| M-7 | 输入延迟 1.3.4 vs 1.3.5 两组对照 | 需登录态；IME 自绘窗跨进程抓不到 ⇒ 用 PrintWindow / UIA |

---

## 5. 待补的自动化判据（下一批候选）

| 项 | 说明 | 风险控制 |
| --- | --- | --- |
| G-3 / G-5 自动验证 | 需要制造「真实输入历史」与「未上屏组合串」 | 必须在**副本**用户数据目录 + 独立云账号下做，绝不碰用户真实词库与云端 |
| 孤儿会话回收观测 | 宿主崩溃后会话残留 | ADR 0067；判据看 `m_live_session_count` / `m_composing_session_count` 是否回落 |
| 「换钥后云端旧行不可解」的用户提示 | 属用户可见文案 | 需用户拍板 + ADR |

---

## 6. 每次跑完的记录表

| 日期 | 版本 | G-1 | G-3 | G-5 | W-1 | W-2 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-11-20 | 1.3.5 | ✅ | ⚠️未验 | ⚠️未验 | ✅ | ✅ | 导出 mtime 前进判放行；证据见 handoff 批次 AX |
