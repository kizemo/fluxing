# 0020. 快捷面板常驻态文件落点跟随 fork 用户数据目录，旧路径只读兜底

- **状态**: Accepted
- **日期**: 2026-10-29
- **关联**: ADR 0019 D1（被本 ADR 修订）/ ADR 0012（版本线 3 段化）/ ADR 0016（64 位换装）
- **相关**: `include/FluxingUserPaths.h` · `include/WeaselUtility.h` · `RimeWithWeasel/WeaselUtility.cpp` · `WeaselServer/QuickPanelDialog.h` · `WeaselServer/QuickPanelDialog.cpp` · `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`

## 背景（Context）

v1.1.2 发布后复核 ADR 0019，发现**它的立论前提本身就是错的**：

- ADR 0019 D1 写「常驻勾选态持久化到 `%APPDATA%\Rime\quickpanel.state`」，理由是
  「不用注册表：与 ShortcutSettings 同目录（`%APPDATA%\Rime\`）一致性更好，
  且备份用户数据目录时状态随目录一起走」。
- 但**这个 fork 的用户数据目录从来不是 `%APPDATA%\Rime`**：
  - `HKCU\Software\Fluxing\Weasel :: RimeUserDir` = `D:\Program Files\fluxing\user1\fluxing`；
  - `WeaselUserDataPath()`（`RimeWithWeasel/WeaselUtility.cpp`）解析的就是它；
  - fork 的默认用户数据目录是 `%AppData%\fluxing`，`%APPDATA%\Rime` 是**已迁走的旧路径**
    （P5/P6/P8 的「一次性迁移」语义）。
- ⇒ ADR 0019 D1 选定的介质路径**不在本 fork 的任何一条用户数据路径上**：
  1. 勾选态落在一条**与用户数据目录无关的孤儿路径**上，「备份用户数据目录时状态随目录一起走」
     这条理由**根本没兑现**；
  2. `%APPDATA%\Rime` 在本机**根本不存在** ⇒ 1.1.2 的写入要么建出一个新目录、要么写失败。
     实测装机后 `%APPDATA%\Rime`、`%APPDATA%\Fluxing`、用户数据目录**三处都没有**
     `quickpanel.state` ⇒ 用户从未勾选过「始终显示」⇒ 1.1.2 的持久化修复**未被真机触发**，
     这个缺陷一直在潜伏；
  3. `QuickPanelStatePath()` 是 `WeaselServer` 里**唯一不跟随 `WeaselUserDataPath()` 的用户文件**
     —— 与「用户文件都住在用户数据目录」这条既有不变量冲突。

根因不是笔误，而是**抄了上游的目录约定**：上游 weasel 的用户数据目录确实是 `%APPDATA%\Rime`，
fork 把它换成了注册表可配置的 `RimeUserDir`，而 ADR 0019 D1 连同它的理由一起被搬了过来。

## 决策（Decision）

### D1 用户数据目录解析收敛到单一事实源

- 新增 header-only 的 `include/FluxingUserPaths.h`：
  - `EnsureFluxingUserDataSuffix()`：从 `include/WeaselUtility.h` **原样迁入**，语义不变；
  - `ResolveFluxingUserDataPath()`：读 `HKCU\Software\Fluxing\Weasel :: RimeUserDir`
    → 套 suffix 规则 → 读不到则回退 `%AppData%\fluxing`。
- `RimeWithWeasel/WeaselUtility.cpp` 的 `WeaselUserDataPath()` 改为
  `return ResolveFluxingUserDataPath();` ⇒ **行为逐字节不变**，只是实现搬家。
- **为什么单独开一个 header**：`include/WeaselUtility.h` 依赖 stdafx 预置的 `<chrono>` 等，
  且拖入 COM/wrl；`test/TestQuickPanelDialog` 既不链接 `RimeWithWeasel`，直接包含会编译失败。
  独立 header 让「解析用户数据目录」这件事**在 server 与 test 两侧都能直接调用**，
  测试**无需改工程链接**即可断言落点。

### D2 状态文件落点 = 用户数据目录；旧路径只读兜底

- 新落点：`<用户数据目录>\quickpanel.state`（跟随 `WeaselUserDataPath()`）。
- 旧落点 `%APPDATA%\Rime\quickpanel.state` 降级为**只读兜底**：
  - **读**：新落点优先；新落点不存在 / 打不开 / 内容坏 ⇒ 回退读旧落点；
  - **写**：**只写新落点**，绝不再在已迁走的 `%APPDATA%\Rime` 造目录。
  ⇒ 1.1.2 的存量勾选（若有）不丢，同时不再污染旧路径。
- 读取容错与 ADR 0019 D1 保持一致：文件缺失 / 打不开 / 坏内容 / 非 `always_show=1` ⇒ 一律
  false（默认关）；行尾 `\r` 容忍。

### D3 可测性：纯函数式公开入口 + 测试注入不兜底

- 读盘优先级用公开静态 `LoadPersistedAlwaysShowModeFrom(primary, legacy)` 暴露，
  便于用临时文件覆盖完整优先级矩阵（都不在 / 仅旧在 / 新优先 / 新=1 / legacy 传空串不兜底），
  **不污染真实用户目录**。
- 测试注入口 `SetStatePath()` 时**不做旧落点兜底** ⇒ 单元测试确定性，
  且绝不读取真实用户目录。

## 后果（Consequences）

- (+) 常驻勾选态与用户数据目录同生共死，ADR 0019 D1 原本承诺的「随目录备份」真正兑现。
- (+) `%APPDATA%\Rime` 不再被本 fork 创建 —— 与 P5/P6/P8 的「一次性迁移」语义一致。
- (−) 1.1.2 → 1.1.3 升级时，旧落点下的勾选态是一次性迁移（首次读到旧落点后，
  下次切换模式即写进新落点）。本机实测用户从未勾选过，**实际无迁移损失**。
- (−) `include/FluxingUserPaths.h` 是新增的单一事实源：将来任何地方要解析用户数据目录，
  一律改调它，**禁止再各自拼 `%APPDATA%` 路径**。
- 回归测试：`TestQuickPanelDialog` 新增 T15a–T15d（默认落点文件名 / 父目录 ==
  `ResolveFluxingUserDataPath()` / 不再是 `%APPDATA%\Rime` / ≠ 旧落点）
  与 T16a–T16e（旧落点只读兜底矩阵），**43/43 全过**；红阶段实测 39/43、
  T15b/T15c/T15d 如期 FAIL ⇒ 断言有牙。
