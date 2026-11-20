# 1.3.7 装机后自验清单 —— 账号页「换钥提示」DPI 感知布局（缺陷 C）

> 批次 BB · ADR 0068 · 产物 `output/archives/fluxing-1.3.7-installer.exe`（42 975 921 B · sha16 `F91A7A108252CF91`）
>
> **本版已自动完成的闭环**（无需用户操作）：`INSTALL_EXIT=0` ⇒ 运行中服务映像换成 **1.3.7.0**（pid 77332，sha16 `38316DCEFC8C502E` 与构建产物逐字一致）；无 `*.stage2` 残留（无待开机换件）；BA 探针复跑 `VERDICT=A1_NOT_CLIPPED`（控件 `1035` 由 `456×36` → `684×54`，37 字提示单行排下，余量 36 px）。
>
> **本清单只剩「需要用户在场 / 换机」的部分。**

## 0. 先确认跑的是哪一版（判据口径）

**只认运行中服务进程的映像版本**，不看磁盘上的规范名文件（`weasel.dll` / `weaselx64.dll` 可能被宿主锁定 ⇒ 版本号 shim 降级路径，**不是回归**）：

```powershell
Get-CimInstance Win32_Process -Filter "Name='WeaselServer.exe'" |
  ForEach-Object { "$($_.ProcessId) $($_.CreationDate) $($_.ExecutablePath)" }
(Get-Item 'D:\Program Files\fluxing\weasel\WeaselServer.exe').VersionInfo.FileVersion   # 期望 1.3.7.0
```

期望：`FileVersion = 1.3.7.0`。

## 1. 缺陷 C 专项（>150% 缩放裁切）—— 本批主目标

| # | 动作 | 期望 | 判据来源 |
|---|---|---|---|
| C-1 | 打开「账号与同步」窗口 | 窗口**完整**，换钥提示**两行都不缺字**（尤其末行「清空云端数据。」） | 目视 |
| C-2 | 复跑 BA 探针（当前机 144dpi） | `VERDICT=A1_NOT_CLIPPED`；`judge-A1` 行 `HEADROOM 36px` | `reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1` |
| C-3 | **把系统缩放调到 175%**，重开账号窗 | 提示**不裁**；探针仍 `A1_NOT_CLIPPED` | ⚠️ **本批未验**（144dpi 已验；>150% 仍属推断） |
| C-4 | **把系统缩放调到 200%**，重开账号窗 | 提示**不裁**；探针仍 `A1_NOT_CLIPPED` | ⚠️ **本批未验** |
| C-5 | 缩放回 100% 或 150% | 布局应与旧版**逐像素一致**（96dpi 不变量） | `TestAccountPageUiDpiScale` |

> 探针前置：必须在 **Default 桌面**（不是影刀虚拟桌面），且账号处于**已登录**态；否则会分别报 `FAIL_DESKTOP` / `NOT_LOGGED_IN`。
> 探针判据是**动态计算**（`DT_CALCRECT` 量文本实际需求高 vs 控件实际高），**不硬编码几何** ⇒ 换 DPI 后同样有效。

## 2. 未登录态（A-4 / A-5）—— 需用户明确授权登出

探针**不自行登出**（登出 = 改账号状态，属用户决定）。用户在场并同意后：

| # | 动作 | 期望 |
|---|---|---|
| A-4 | 登出后打开账号窗 | 账号窗正常显示**登录态**布局（无 `1035` 换钥提示控件）；控件不重叠、不出界 |
| A-5 | 登出态下再跑探针 | 走 `NOT_LOGGED_IN` 分支并正常输出，不得崩溃/卡死 |

## 3. 承接上一批、仍未闭环（本批未做）

| # | 项 | 备注 |
|---|---|---|
| B-1/B-2/B-3 | **孤儿会话回收端到端**（ADR 0067） | 强杀宿主进程后，服务端会话表里的孤儿 `ipc_id` 应被惰性清扫；**下一批第 1 项** |
| AZ-3 | 已运行 TSF 宿主**逐 pid 枚举模块**验证实际加载的 shim 版本 | 注意规范名 dll 可能是 shim（宿主锁定） |
| AZ-4 | `TestWeaselIPC.exe /verify-client-pid` | 需先 `WeaselServer.exe /quit` 让出命名管道；真服务端在跑时 mock 必假红 |
| AC | **双机 AC 7 条** + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5 | 用户在场时执行 |
| — | 本批/AZ/BA 文档改动的 **push** | 需用户授权；本地已提交 |

## 4. 「不是缺陷」的已知现象（别误报）

1. **`weasel.dll` / `weaselx64.dll` 版本号落后**（如仍是 1.3.6.0 甚至 1.3.4.0）—— 被已运行宿主锁定 ⇒ 走 `install.nsi` 的版本号 shim 降级路径，**真实落盘新件带版本号文件名并由 `InprocServer32` 指向**。判「新进程有没有用新 shim」只认 `InprocServer32` 指向文件的 `FileVersion`。
2. **目录里残留 `*.old.tmp`** —— 升级回滚脚本的降级痕迹（`.old.tmp` 只在 rename 成功分支被删），不是故障。
3. **同步提示「你正在输入，本轮已跳过用户词典同步」** —— ADR 0062/0065 的**预期行为**（空闲宽限 10 s / 未上屏组合串 60 s），不是缺陷。
4. **`[acctdlg] dpi-mismatch system=144 font_px=21 derived=168`** —— 本批**主动告警**（字体度量反推的 168dpi 会 +17% 超缩，故布局改用**系统 DPI**），**不影响显示**，不必处理。

## 5. 回滚

安装包自带升级回滚（`install.nsi` 三段升级 + `IfErrors`/`Rename` 守卫）。回滚前先 `WeaselServer.exe /quit` 停服，否则 PPL 映像被占用 ⇒ 需注销/重启。
