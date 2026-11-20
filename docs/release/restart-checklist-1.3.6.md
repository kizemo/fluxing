# 重启后测试清单 · 1.3.6（批次 AY）

> 装机前置：`WeaselServer.exe` 是 **PPL**（Protected Process Light）⇒ **必须注销或重启**再装，
> 否则仍在跑旧映像；`taskkill` 被禁，停服只能 `WeaselServer.exe /quit`。
> **磁盘哈希不能证明「在跑的是新码」**⇒ 每条判据都写明怎么确认。
> 本清单只列 1.3.6 新增项；**1.3.5 遗留项（双机 AC 7 条 / restore 端到端 / 输入延迟两组对照 /
> 闸门正反判据）见 [`restart-checklist-1.3.5.md`](restart-checklist-1.3.5.md)**，不在此复述。

## 0. 装对了没有

```powershell
(Get-Item 'D:\Program Files\fluxing\weasel\WeaselServer.exe').VersionInfo.FileVersion   # 期望 1.3.6.0
```

服务进程映像版本 = 验收口径（不是安装时刻）：

```powershell
Get-Process WeaselServer | Select-Object Id,StartTime,@{n='ver';e={$_.MainModule.FileName}}
(Get-Process WeaselServer).MainModule.FileVersionInfo.FileVersion   # 期望 1.3.6.0
```

## 1. 账号页「换钥后果」常驻提示（本批新增，AX §6.8 用户拍板项）

打开方式 = 托盘右键 →「账号与同步」（脚本等价 = 向**服务自身主窗**发 `WM_COMMAND(40024)`；
**发给托盘窗口无效**，`scripts\verify\verify-cloud-account-window.ps1 -Mode Discover` 已改为双窗都试）。

| # | 判据 | 期望 |
| --- | --- | --- |
| A-1 | 已登录态出现常驻说明行 | 「重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端数据。」 |
| A-2 | 提示行**不被裁切**、不重叠 | 单行就显示一行；若折行必须两行都在窗内可见（`h=36` 只是留余量的设计值，**折行宽度未实测**） |
| A-3 | 「关闭」按钮位置 | 位于提示行下方，完整可见且可点 |
| A-4 | 未登录态 | **不出现**该行（`AccountVisibleIds` 按登录态过滤） |
| A-5 | 未登录态底部留白可接受 | `AccountClientSize` 按窗口角色统一取高度 ⇒ 两态同高 290 px，未登录态会有留白（设计取舍，非缺陷） |

取证：`PrintWindow` 截窗 + 目视。判据不可用「状态文案」（自绘 + 刷新滞后）。

## 2. 孤儿 IPC 会话回收（ADR 0067，本批新增）

**场景**：宿主进程崩溃/被强杀而未走 `Deactivate` ⇒ 会话残留 ⇒ 闸门长期判「你在打字」。

| # | 判据 | 期望 |
| --- | --- | --- |
| B-1 | 杀掉宿主进程 | 后续云同步轮次**不再**因该残留会话被「跳过」 |
| B-2 | 诊断日志 | `%TEMP%\fluxing-profile-notify.log` 出现 `reclaim orphan session ipc_id=<n> client_pid=<pid> live=<m>` |
| B-3 | **关键边界**：回收时若活动会话属**另一个存活宿主** | 日志 `orphan_reclaim presence=kept`，**不得**出现「火流猩不在场」类状态变化、快捷面板不得被收起 |
| B-4 | 正常切换窗口 / 用户主动切走 IME | 表现与批次 AW 前一致（presence 与面板收起仍发生） |
| B-5 | 客户端版本兼容 | 旧客户端（不写 `session.client_pid=`）⇒ 服务端按「pid 未知」**不回收**，不得误杀 |

⚠️ 判据 B-3 是本轮主会话复核抓出的真缺陷所在：`RemoveSession` 带两个副作用（翻 presence、无条件清 `m_active_session`），
而回收发生在 `AddSession` 内紧接着注册新活动会话的当口 ⇒ 若不做区分，**宿主崩溃会被误读成用户切走**。

## 3. 账号窗缺陷 B 第三分支复现取证（AX 未复现项）

复发「第二次打开的窗关不掉」时：`Get-WindowInfo` 看 `IsWindow` / `IsVisible` / 窗是否启用；
`Get-Content "$env:TEMP\fluxing-profile-notify.log" -Encoding UTF8 -Tail 60` 看 `[acctdlg]` 行
（本批起该标签由 `OutputDebugStringW` **改接** `fluxing::diag::AppendLine` ⇒ **无需调试器即可取证**）。

## 4. 同步不受影响复核（沿用 1.3.5 口径）

闸门是否放行 = **维护导出文件 mtime** 是否前进（状态文案不可作判据）：

```powershell
Get-Item 'D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-d5b9-4dfc-9929-85eac8509db6\rime_ice.userdb.txt' | Select-Object LastWriteTime
```

反向判据（**G-3 / G-5**，见 [`regression-checklist-cloud-sync.md`](regression-checklist-cloud-sync.md) §二，仍未自动验证）：
打字 10 s 内、有未上屏组合串 60 s 内，同步**应被拦住**（导出文件 mtime 不动）。

## 5. 环境受限项：`/verify-client-pid`

IPC 管道名**全局固定**（`\\.\pipe\<用户名>\WeaselNamedPipe`）⇒ 真实服务端在跑时 mock server 抢不到管道，
必然假红。要跑必须先 `/quit` 真实服务：

```powershell
& 'D:\Program Files\fluxing\weasel\WeaselServer.exe' /quit
& 'F:\soft\00selfmade\rime_claude\test\TestWeaselIPC\x64\Release\TestWeaselIPC.exe' /verify-client-pid   # 期望 EXIT=0
# 跑完重新拉起 WeaselServer
```

本批登记为 **未验证**，不是 FAIL。

## 6. 「不是缺陷」的已知现象（勿误报）

- **云单测 exe 在仓库根 `Release\`**，不在 `output\`；`build.bat weasel` **不构建**它。
- **同步轮次的状态文案自绘且刷新滞后** ⇒ 只用导出文件 mtime 作判据。
- 未登录态账号页底部留白（见 A-5）是高度统一的设计取舍。
- `weasel.props` 是 `build.bat` 生成的、不被 git 跟踪 ⇒ 它出现在脏文件列表里属正常。
- makensis 的 8 条 `warning 7010`（ARM/ARM64 产物、`data\*.gram`、`data\lua\*.txt`）是既有可选产物告警。
