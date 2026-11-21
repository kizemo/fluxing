# BG 批次 · 发现「同步结果在界面上不可观测」

时间：2026-11-21 · 性质：取证（产品代码零改动）

## 结论
账号页「最近同步」控件（id=1017 `kIdSyncStatus`）**永远停在初始文案**，同步的真实结果
（成功 / 被维护闸门跳过 / 失败）在界面上**不可见**。

## 证据链（全部 verified）
- `WeaselServer/Cloud/CloudAccountDialog.cpp:232-235` `SetSyncStatus()` 写入 `impl_->sync_status_text`。
- `WeaselServer/Cloud/CloudAccountDialog.cpp:501-504` `CloudAccountController::sync_status()` 读出该字段；
  **全仓 grep `sync_status(` 仅命中声明与定义两处，无任何调用者**（见 `verify-observable-point-20261121.txt`）。
- `WeaselServer/Cloud/CloudAccountDialog.cpp:1311-1313` 刷新时只把 `controller.status(scope)` 写进
  `kIdStatus`(1006)；**从不写 `kIdSyncStatus`(1017)**。
- `WeaselServer/Cloud/CloudAccountDialog.cpp:955-956` 1017 的唯一静态字面量 = 「最近同步：尚未同步。」
  ⇒ 控件文本永不更新。
- `WeaselServer/Cloud/CloudAccountUi.h:53,290` —— `kIdSyncStatus = 1017` 且在布局表里
  （12,108,456,20）⇒ **控件确实存在**，只是永远拿不到真实状态。
- 正常路径（引擎已启动）`CloudAccountDialog.cpp:817-828` 只把 1006 置为
  「手动同步已提交，完成后见下方『最近同步』。」—— **把用户引到一块永不更新的控件**；
  被闸门跳过的真实结果只落在 `sync_status_text`（不可见）。
- 回退路径（引擎未启动）`CloudAccountDialog.cpp:839-844` 会把「同步已跳过：…」写进 1006
  ⇒ **同一语义在两条路径上可见性不一致**。
- 服务端闸门侧：`WeaselServer/Cloud/CloudSyncService.cpp:876-886` 拿到 `gate_reason` 后**丢弃**，
  只回填通用文案 `MaintenanceGateSkipMessage()`；闸门原因令牌 `live_input_session` 只存在于
  `include/FluxingMaintenanceGate.h:39` 与单测 `test/TestFluxingCloud/TestFluxingCloud.cpp:3089`，
  **服务端无任何日志/响应记录它**。

## 对 G-3/G-5 判据的影响（决定性）
状态文案**不能**用来区分「被闸门拦住」与「同步跑完但无可推送」⇒
唯一可用判据是**维护导出文件 mtime 是否前进**，且**必须**配 phase C 正控制
（无合成会话时导出 mtime 必须前进）才有判别力。
→ 已据此判定：worker 的 `orphan-gate-g35-export-mtime.ps1` **无需返工**，设计正确。

## UX 影响
用户点「立即同步」后，若被维护闸门（正在输入）跳过，**界面上没有任何提示**（正常路径），
只能干等；同步失败同理不可见。

## 登记
- 状态：**待用户拍板是否修**（属独立 ADR 批次，本批未做）。
- 最小修法 = 在 `CloudAccountDialog.cpp:1311` 附近把 `controller.sync_status()` 一并写进
  `kIdSyncStatus` 控件（布局里该控件已存在，产品码约 3 行）。
- 对本批 G-3/G-5 **无影响**：判据仍走 mtime 代理 + phase C 正控制。