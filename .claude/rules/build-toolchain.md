---
paths:
  - "build.bat"
  - "xbuild.bat"
  - "env.bat"
  - "weasel.sln"
  - "xmake.lua"
  - "**/xmake.lua"
  - "**/*.props"
---

# Build & toolchain

| System | Entry | When | Latency |
|---|---|---|---|
| xmake | `xbuild.bat` | inner loop | 30–90s |
| MSBuild | `build.bat all` + `weasel.sln` | CI parity / 月度 hygiene | 5–60min |

## Hard rules

- Release build → `RELEASE_BUILD=1` in `env.bat`。**禁用** `git describe --tags`
  取版本号 (L10 §6, fork 上不可靠)。
- `weasel.props` / `env.bat` 已 gitignored;**绝不** commit。需持久化的值,
 写到 `tools/setup-shims.ps1`。
- `.sln` 为 project membership 真相 (P3),`xmake.lua` 是镜像;不同则 .sln 优先。
- PowerShell 处理 CJK 文本 byte-level:`[IO.File]::ReadAllBytes`,
  或 `[Console]::OutputEncoding = UTF8` + `Out-File -Encoding utf8`。
  `output/install.nsi` **必须 UTF-8 + `EF BB BF` + CRLF** (L01 / L02 / A1 / L09)。

## `output/install.nsi` 变更 — pre-flight (in order)

1. `[0..2] == 0xEF 0xBB 0xBF`,行尾 CRLF — byte-level 验证。
2. 不带 `/D=<path>` 的 silent install → 未知参数污染 `$INSTDIR` 与注册表 (L13 / L17)。
3. Silent-install 测试前清: `HKLM\Software\Fluxing\Weasel` + `HKCU\Software\Fluxing` (L54)。
4. 跑 `AGENTS.md §2.5` 端到端 smoke test (NSIS 改动强制,不可跳过)。
