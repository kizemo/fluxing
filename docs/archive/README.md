# 归档：已从历史中移除的未合并工作

本目录保存**已经从 Git 历史里移除、但仍有后续任务价值**的工作副本。

## v0.74.3-stage2-shim-wip.patch

| 项 | 值 |
|---|---|
| 来源 | `stash@{0}`（描述：`On Fluxing: v0.74.3 WIP + artifacts polluted by stray build`） |
| stash 基线 | `654d3b2` — `fix(adr+installer-test): v0.74 evidence-gap closure`（2026-07-30） |
| 归档日期 | 2026-10-22（filter-repo 历史瘦身前置） |
| 内容 | 9 个文件，1087 insertions / 160 deletions（已排除 `release/` 安装包） |
| 编码 | UTF-8，无 BOM，无 NUL，无 U+FFFD |
| 状态 | **stash 已 drop**；本 patch 是这份工作的唯一副本 |

### 为什么归档而不是直接丢弃

`stash@{0}` 的内容分两类：

1. **真实未合并工作** —— v0.74.x 线的 Stage2 shim 安装器、ADR 0003、QuickPanel FocusIn 撤回、hot-path cache、spec 075 与 `L##-PhaseL-9.11` 条目。
2. **约 2GB 的污染** —— 50 多个 `release/fluxing-0.19.0.X-installer.exe`（每个约 43MB），stash 描述里已自认是 `artifacts polluted by stray build`。

第 2 类让 stash 成为 `.git` 无法瘦身的钉子：`refs/stash` 一旦存在，那 50 多个 blob 就一直可达，`git gc` 永远回收不掉。
因此做法是：**把第 1 类导出成 patch 提交进版本库，再 drop 掉 stash**。

### patch 覆盖的文件

```
.specify/memory/lessons-learned.md                    (362 行新增，含 L##-PhaseL-9.11)
RimeWithWeasel/RimeWithWeasel.cpp                     (QuickPanel FocusIn Show 撤回)
WeaselIPCServer/WeaselServerImpl.cpp / .h             (hot-path cache)
docs/adr/0003-installer-shim-stage-at-startup.md      (ADR 0003 全文)
output/install.nsi                                    (Stage2 shim 安装段)
output/stage2-shim-install.bat.cmd
test/TestSystem32InlineCleanup/Test-Stage2ShimInstall.ps1
task.md
```

### 如何使用

patch 的上下文基于 **v0.74.x 时代的代码形态**（`654d3b2`），
**不是**当前 HEAD（`3a0b6e4` / v0.21.0.x）。当前 HEAD 的 `RimeWithWeasel.cpp` 已经完全没有 `QuickPanelDialog::Show`，
QuickPanel 按 spec 075 走了重写路线，所以这份 patch **不能直接 `git apply` 到 HEAD**。

要看的是**设计意图与取舍理由**，不是可直接落地的代码。要复用时：

```bash
# 1. 只读检视
git apply --stat docs/archive/v0.74.3-stage2-shim-wip.patch

# 2. 需要落到 v0.74.x 基线时，先切到对应历史再做三方合并
git checkout 654d3b2
git apply --3way docs/archive/v0.74.3-stage2-shim-wip.patch
```

### 注意

`654d3b2` **不在** 当前 `Fluxing` / fix 分支的祖先链上（该版本线在 2026-07-30 前后被重写放弃）。
若要恢复它所在的整条历史，请用远端兜底分支 `kizemo/archive/pre-filter-repo-2026-10-22`
（= filter-repo 改写前的完整历史，指向 `3a0b6e4`）。
