# 011 · 火流猩输入法 v2 · 安装 / 卸载 / 首启引导（v2 P3 仅设计）

> 元 spec 004 拆分。v2 阶段 P3 仅交付 design；v2.2+ 实施。

## 0. 上下文

- 当前安装器（NSIS）是经典 MUI 风格（90 年代灰）。
- 用户在 brainstorming 中提出"安装/卸载界面也用 mac 风"。
- v2 范围（spec 004）已声明"安装/卸载界面推到 spec 011"。

## 1. 产品视角

### 1.1 三个阶段

1. **预安装（安装器内）**：silent install，写入文件、注册输入法服务，调 `FluxingBootstrapper.exe --stage=preinstall`。
2. **安装期（FluxingBootstrapper 主导）**：mac 风欢迎页 + 路径选择 + 进度条。
3. **首启引导（v2.2+ 必选，v2.1+ 跳过）**：主题选择 + 云同步账户 + 主方案 + 上手卡。

### 1.2 卸载界面

- mac 风卸载确认窗。
- 显示"将删除：xxx MB 在 %ProgramFiles%\Fluxing"。
- 显示"将保留：%LocalAppData%\Fluxing（可勾选"同时清除"）"。
- 进度条 + 完成提示。

### 1.3 用户故事

- US11-A [P3]：用户运行 `fluxing-2.2.0-installer.exe` → 弹 mac 风安装窗 → 选安装路径 → 选用户数据路径 → 进度条 → 完成 → 弹 mac 风首启引导。
- US11-B [P3]：用户在首启引导注册云同步账户 → 完成后立即生效（spec 010 已就位）。
- US11-C [P3]：用户在控制面板卸载 Fluxing → 弹 mac 风卸载窗 → 确认 → 进度条 → 完成。

## 2. Out of scope

- 不做"自动更新器"（v3+；WinSparkle 已可承担）。
- 不做"安装包数字签名 UI"（命令行 `signtool` 即可）。
- 不做"自解压 exe"（安装器自带）。

## 3. 依赖

- spec 010 云同步账户系统（v2.2+ 时已就位）。
- spec 002 已落地的安装目录与用户数据目录布局。
- 现有 NSIS 安装器（silent 模式 + JSON bridge 协议，spec 011 design.md §2.3 详细）。