# 011 · Plan · 安装 / 卸载 / 首启引导

## 1. 技术上下文

- **mac 风 UI**：与 spec 006 共享 `FluxingComponents` 组件库 + `FluxingDarkModeBridge`。
- **NSIS**：保留为 silent 安装器（解压 + 写注册表），不渲染 UI。
- **Bootstrapper**：独立 exe（`FluxingBootstrapper.exe`），承担所有 mac 风 UI 渲染。
- **通信协议**：stdin/stdout JSON 单行事件（spec 011 design.md §2.3 详细 schema）。

## 2. Architecture

- **进程模型**：
  - NSIS 安装器（父进程）：仅解压 + 写注册表 + 调起 Bootstrapper 子 exe。
  - FluxingBootstrapper.exe（子进程）：所有 mac 风 UI 渲染。
- **通信**：stdin/stdout JSON pipe，每行一个事件，UTF-8 无 BOM + LF。
- **CLI**：`FluxingBootstrapper.exe --stage=<install|firstrun|uninstall> [--log=<path>]`。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"安装卸载也 mac 风" |
| II. Test | OK | TestBootstrapper 引入（mock NSIS silent 模式） |
| III. Spec-Artifact | OK | 3 件套齐全 + design.md §2.3 详细 JSON 协议 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | v2 P3 仅设计；v2.2+ 实施 |
| R1-R9 | OK | 引用 spec 004 §5 全局约束 |
| P1-P8 | OK | P8 brand-fork scope |

## 4. 风险

- **R1**：stdin/stdout pipe 在 Windows 下的 buffer 行为（NSIS 调 CreateProcess 默认继承 handle）；task 阶段验证。
- **R2**：首启引导与 spec 010 云同步账户的耦合；v2.2 时 010 已就位，task 阶段做集成测试。
- **R3**：mac 风 UI 资源（按钮 / 列表等）复用 006 组件库；v2.0 ship 时 006 必然先 ship。