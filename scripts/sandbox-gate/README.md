# sandbox-gate · Fluxing「宣称完成的任务」沙箱门禁

这套工装把**待验安装包**装进一台一次性的 Windows 虚机（Windows Sandbox），
在里面逐条验证那些「宣称已经完成」的任务，然后产出可归档的 JSON 结论。

> 这套工装**不在**被测项目的产物里，也**不改**被测机器上的任何用户配置：
> 一切都在一次性虚机里发生，崩了只损失那台虚机。

## 目录

| 文件 | 作用 |
|---|---|
| `run-verify.cmd` | 入口（纯 ASCII，双击或 CI 调用都行） |
| `run-verify.ps1` | 主机侧：staging → 生成 .wsb → 启动沙箱 → 等结果 → 打印逐条结论 |
| `New-SandboxConfig.ps1` | 生成 `.wsb`（**路径运行时给，不写死**——这是能进 CI 的关键） |
| `probe-sandbox.ps1` | 探测本机能不能跑沙箱（CI 前置检查，跑不了就明确报错，不静绿） |
| `in-sandbox-claims.ps1` | 沙箱内执行：所有断言都在这里 |
| `lib/*.ps1` | 共享模块（静默装包 / 快照校验 / PE 架构 / 安装包类型） |

## 本机跑

```powershell
# 一键跑完整轮（自动挑 output\archives 里版本号最高的安装包）
scripts\sandbox-gate\run-verify.cmd -RepoRoot F:\soft\00selfmade\rime_claude

# 只把沙箱拉起来，不等结果
scripts\sandbox-gate\run-verify.cmd -NoWait

# 结果落盘位置
%USERPROFILE%\sandbox-artifacts\rime\claims.json     ← 机器可读
%USERPROFILE%\sandbox-artifacts\rime\logs\claims.log  ← 人可读
%USERPROFILE%\sandbox-artifacts\rime\screenshots\     ← 截图
```

退出码：`0` 全部通过 · `1` 有未达标项 · `2` 超时 / 沙箱没起来。

## CI 跑

`.github/workflows/ci.yml` 里的 `sandbox-gate` job。

**它必须跑在自托管 runner 上**，原因是硬约束而不是偏好：
Windows Sandbox 本身就是一台**嵌套虚机**，宿主机必须支持嵌套虚拟化；
GitHub 官方的 windows runner 提供不了。

```
# 在一台带嵌套虚拟化的 Windows 11 机器上注册 runner，带 sandbox 标签
.\actions-runner\_work\_register.cmd --labels sandbox
```

CI 会把三样东西从上游 job 传下来：

| 产物 | 来自 | 用途 |
|---|---|---|
| `fluxing-installer` | build job | 待验安装包 |
| `fluxing-buildout` | build job | `output\weasel*.dll` + `output\data\`，用来比对「注册指向的二进制 == 本次构建」 |
| `fluxing-tests` | test job | `Release\Test*.exe`，沙箱内跑同一批单测 |

`probe-sandbox.ps1` 跑在门禁之前：这台机器跑不了沙箱 → job 直接红，
而不是「绿但什么都没验」。

## 新增断言时

1. 在 `in-sandbox-claims.ps1` 里加一条 `Add-Result`；
2. **顺手把 `$script:toolingVersion` 往上抬一位**（它写进 `claims.json`，
   用来区分「这份结果来自哪一版工装」）；
3. 跑一轮，确认 46 项（当前）里该过的都过、失败项集合没有意外扩大。

## 已知的沙箱限制

- 沙箱只有 C: 盘（`E.2` 永远 FAIL，这是**环境事实**，不是缺陷；
  它存在的意义是给「默认路径在 D 盘」提供对照）；
- 沙箱默认无外网（`E.4` 永远 FAIL，同上）；
- `P6`（切到 Fluxing 自动弹快捷设置栏）需要真正的 TSF 客户端才能触发
  `ITfActiveLanguageProfileNotifySink::OnActivated`，详见仓库根的
  `FLUXING-CLAIMS-VERIFICATION.md` §3.11。

## 编码规矩

所有 `.ps1` 必须是 **UTF-8 with BOM + CRLF**；否则 PS 5.1 会按 GBK 读中文，
输出全是乱码。写完用 `check-scripts.ps1` 那套校验（字节级 BOM/CRLF/FFFD +
语法解析）过一遍再提交。
