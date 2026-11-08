# ADR 0045 — 发布版本号由 `env.bat` 钉死，`build.bat` 不再无条件覆盖

- **状态**：已接受（2026-11-08）
- **影响范围**：`build.bat`、`env.bat`、`scripts/gate/lib/GateChecks.ps1`（`Test-VersionConsistency`）、`output/install.nsi`
- **决策类型**：构建/发布链路

## Context

本 fork 的版本口径是「三元公版号 + 四元构建号」：

- `version.txt` = `1.2.1`（公版号，CHANGELOG 顶部必须与之一致）
- `env.bat` 里 `FLUXING_VERSION=1.2.1`、`WEASEL_BUILD=2`、`PRODUCT_VERSION=1.2.1.0`、`FILE_VERSION=1.2.1.0`
- L0 门禁 `version-consistency` 要求 `weasel.props` / `env.bat` 的 `PRODUCT_VERSION` 与 `FILE_VERSION` 都等于 `version.txt + ".0"`

`build.bat` 的 `:render_props` 子程序用 `cscript render.js weasel.props` 把**进程环境**里的
`PRODUCT_VERSION` 渲染进 `weasel.props`，再由 `weasel.props` 传给每个 `.vcxproj`
（最终进 Windows 资源的 `ProductVersion`）。

问题出在 `build.bat` 第 50 行：

```bat
rem use numeric build version for release build
set PRODUCT_VERSION=%FLUXING_VERSION%.%WEASEL_BUILD%
```

这是一条**无条件赋值**。`env.bat` 在 `build.bat` 第 23 行被 `call`，它显式钉的
`PRODUCT_VERSION=1.2.1.0` 在这里被直接覆盖成 `1.2.1.2`（`WEASEL_BUILD=2`）。
于是**每跑一次 `build.bat`，`weasel.props` 里的 `PRODUCT_VERSION` 就变成 `1.2.1.2`**，
L0 门禁的 `version-consistency` 恒红：

```
[FAIL] version-consistency        weasel.props PRODUCT_VERSION=1.2.1.2 != 1.2.1.0
```

批次 F（11-07）把 `WEASEL_BUILD` 从 1 提到 2 之前，`FLUXING_VERSION.WEASEL_BUILD`
恰好是 `1.2.1.0`，两边一致所以问题没显形；提到 2 之后两者分叉才暴露。

而 `env.bat` 里写得很清楚：

> `PRODUCT_VERSION/FILE_VERSION stay 1.2.1.0 on purpose -- the version gate requires version.txt + ".0".`

也就是说 **`env.bat` 的意图就是钉住 `1.2.1.0`**，`WEASEL_BUILD` 只该进
Windows 资源与版本化 shim 文件名。`build.bat` 下面处理 `FILE_VERSION` 的那行
本来就有 `if not defined` 守卫，`PRODUCT_VERSION` 这行漏了同一个守卫 ——
这是不一致，不是设计。

## Decision

`build.bat` 补齐与 `FILE_VERSION` 同一款守卫：

```bat
if not defined PRODUCT_VERSION set PRODUCT_VERSION=%FLUXING_VERSION%.%WEASEL_BUILD%
```

即：**`env.bat` 钉了就用 `env.bat` 的；没钉（CI / 干净 clone）才回落到
`FLUXING_VERSION.WEASEL_BUILD`。**

## Consequences

- **正面**：`env.bat` 的发布口径（`PRODUCT_VERSION` = `version.txt + ".0"`）成为唯一事实源，
  门禁与构建不再互相打架；「`env.bat` 可以显式钉版本」这件事终于真的生效。
- **Windows 资源**：发布构建的 `ProductVersion` 从 `1.2.1.2` 回到 `1.2.1.0`，
  与 `env.bat` 和门禁的期望一致。`FileVersion` 键不受影响（它取 `FLUXING_VERSION`）。
- **安装器不受影响**：`output/install.nsi` 用的版本号是 `${FLUXING_VERSION}` 与
  `${WEASEL_BUILD}`（`VIProductVersion`、版本化 shim 文件名
  `weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.*.dll`），**不读 `PRODUCT_VERSION`**，
  所以 L107（同名同内容不同字节）的版本化 shim 机制完全不变。
- **开发构建不变**：非发布分支仍走 git 提交号形式的 `PRODUCT_VERSION`
  （`build.bat` 里 `if not defined RELEASE_BUILD` 那段），行为未动。
  注意开发构建的 `PRODUCT_VERSION` 仍不是 `version.txt + ".0"`，
  所以**开发构建后不要用 `-Mode source` 门禁判定发布口径** —— 这是既有行为，不是本次引入。
- **门禁强度不变**：`Test-VersionConsistency` 的判据一个字没改，仍要求
  `PRODUCT_VERSION == version.txt + ".0"`。本次是让构建产出满足门禁，而不是放宽门禁。
