# test/regression —— 回归用例（缺陷 ⇄ 用例 一一映射）

本目录是**验收资产**，不是临时脚本。它存在的唯一目的：让「已经修好的 bug」
在后续任何一次改动后**自动翻红**，而不是靠人记得手动跑一遍。

> 机制背景：`docs/analysis/2026-11-03-postmortem-feature-loss-and-bug-recurrence.md`
> 根因 RC1 —— 上一轮把验收脚本写在 git-ignored 的 `tmp/` 里，
> 交付给下一个会话时**资产本身消失**，于是「修过的 bug」没有任何东西守着它。
> 决策记录：`docs/adr/0033-regression-asset-and-staged-gate.md`。

## 1. 目录契约

```
test/regression/
  README.md                 本文件（契约）
  DEFECTS.yaml              缺陷登记表：每个已修缺陷 ⇒ 守它的用例
  run-cases.ps1             用例运行器（CI / 本地同一入口）
  lib/CaseHarness.ps1       用例骨架：断言 / 清理栈 / 退出码 / 结果落盘
  cases/C####-<slug>.ps1    一个缺陷一个用例，文件名前缀 = case-id
  probes/                   用例需要的外部探针源码（编译产物不进库）
```

## 2. 每个用例必须满足的 5 条（缺一条即视为未交付）

| # | 契约 | 判据 |
|---|---|---|
| 1 | **case-id** | 文件名 `C####-<slug>.ps1`，首行注释写明对应的 `DEFECTS.yaml` 条目 |
| 2 | **前置条件可判** | 缺前置时输出 `RESULT ... status=SKIP` 并 `exit 2`，**不准静默通过** |
| 3 | **`finally` 自清理** | 自己拉起的进程、自己改的注册表/状态文件必须还原；用 `Register-Cleanup` |
| 4 | **退出码即结论** | `0`=PASS / `1`=FAIL / `2`=SKIP / `3`=harness 异常 |
| 5 | **不依赖 `tmp/`** | 用例内**禁止**引用 `tmp/` 下任何路径（L0 门禁会扫）；需要的源码进 `probes/` |

第 5 条是本轮的核心，别再犯：

```powershell
# 错：tmp 被 .gitignore 掉，下一台机器上这个用例根本跑不起来
$probe = 'F:\soft\00selfmade\rime_claude\tmp\quickpanel-evidence\ReproEsc.exe'

# 对：源码进 probes/，运行时由用例自己编译到 tmp 下的临时目录
$probe = Build-ProbeSource -Source (Join-Path $PSScriptRoot '..\probes\ReproEsc.cs')
```

## 3. 怎么跑

```powershell
# 全部快用例（不需要真机安装，秒级）—— CI 与 L4 门禁跑这个
pwsh -NoProfile -File test\regression\run-cases.ps1 -Level fast

# 全部用例（含需要已安装产品 + 已部署 server 的重用例）
pwsh -NoProfile -File test\regression\run-cases.ps1 -Level all

# 单条
pwsh -NoProfile -File test\regression\run-cases.ps1 -Case C0175
```

`run-cases.ps1` 把结果写到 `tmp/gate/regression-<stamp>.log`（日志不是资产，不进库），
并打印一行汇总：`REGRESSION total=N pass=N fail=N skip=N`。

## 4. 当前用例

| case-id | 守的缺陷 | 级别 | 需要什么 |
|---|---|---|---|
| `C0032` | ADR 0032：按 Esc 关短语窗口**不得**连带收起快捷设置栏 | heavy | 已安装产品 + 已部署 `output/Win32/WeaselServer.exe` |
| `C0174` | L174：安装器必须清掉**父进程已死**的自家 UI 孤儿（否则目标文件被独占锁） | heavy | 两个安装器（旧=必须失败 / 新=必须成功） |
| `C0175` | L175：PE 版本资源 `FileVersion` 字符串表必须非空且等于 `version.txt` | fast | 仅需 `output/Win32/` 构建产物 |

## 5. 新增一个用例的标准流程

1. 在 `DEFECTS.yaml` 里加条目（`id` / `title` / `fixed_ref` / `lesson` / `guard.case`）。
2. 写 `cases/C####-<slug>.ps1`，开头 `Import-Module` 式点源 harness：
   ````powershell
   . (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')
   Initialize-Case -Id 'C0175' -Title '...'
   try { ... Assert-Case ... } finally { Complete-Case }
   ````
3. 造一份**坏数据**验它有牙：把被测条件反向改掉，用例必须 `exit 1`（L3 变异门禁会做同类事）。
4. 跑两次：第一次 PASS、第二次仍 PASS（幂等，不留残留进程/注册表）。

## 6. 与门禁的关系

`scripts/gate/run-all.ps1` 的 **L4** 阶段就是本目录的 `-Level fast`；
**L5** 之后的重用例由发布前的人工/脚本验收触发（见 `DEFECTS.yaml` 的 `level: heavy`）。
`scripts/gate/L0-static.ps1` 会静态校验本目录与 `DEFECTS.yaml` 的一致性（双向引用 + 禁 `tmp/`）。
