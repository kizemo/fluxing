# 011 · 火流猩输入法 v2 · Fluxing Bootstrapper

> **状态：P3 仅设计**。本 spec 011 不在 v2.0.0 实施范围；产出 = 详细 PRD+TDD，评审后推到 v2.2.0 实施。
> 范围：用 mac 风替换当前安装器的经典灰界面，并把"首启引导"作为云同步设置的最佳锚点。

## 0. 上下文

- 当前 `output/install.nsi` 是经典 MUI 风格（90 年代灰）。
- 用户在 brainstorming 中提出"安装/卸载界面也用 mac 风"。
- v2 范围（spec 004）已声明"安装/卸载界面推到 spec 011"。

## 1. 产品视角

### 1.1 三个阶段

1. **预安装（安装器内）**：
   - 安装器 silent install（无 UI 弹窗）
   - 写入文件、注册输入法服务
   - 调 `FluxingBootstrapper.exe --stage=preinstall`
2. **安装期（FluxingBootstrapper 主导）**：
   - mac 风欢迎页
   - 选择安装路径（默认 `%ProgramFiles%\Fluxing`，无版本子目录）
   - 选择用户数据路径（默认 `%LocalAppData%\Fluxing`，强制后缀）
   - 进度条（安装器 silent 后台跑；UI 实时显示解压进度）
3. **首启引导（v2.2+ 必选，v2.1+ 跳过）**：
   - 主题选择（亮 / 暗 / 跟随系统）
   - 注册云同步账户（spec 010 锚点）
   - 选主方案（朙月拼音 / rime_ice / 其它）
   - 显示"快速上手卡片"：5 个最常用快捷键

### 1.2 卸载界面

- mac 风卸载确认窗
- 显示"将删除：xxx MB 在 %ProgramFiles%\Fluxing"
- 显示"将保留：%LocalAppData%\Fluxing（可勾选'同时清除'）"
- 进度条 + 完成提示

### 1.3 用户故事

- **US11-A** [P3]：用户运行 `fluxing-2.2.0-installer.exe` → 弹 mac 风安装窗 → 选安装路径 → 选用户数据路径 → 进度条 → 完成 → 弹 mac 风首启引导。
- **US11-B** [P3]：用户在首启引导注册云同步账户 → 完成后立即生效（spec 010 已就位）。
- **US11-C** [P3]：用户在控制面板卸载 Fluxing → 弹 mac 风卸载窗 → 确认 → 进度条 → 完成。

## 2. 技术视角

### 2.1 新增模块

| 模块 | 路径 | 角色 |
|---|---|---|
| `FluxingBootstrapper` | `FluxingBootstrapper/main.cpp`, `BootstrapWindow.h/.cpp` | mac 风安装器 exe（独立进程） |
| `FluxingBootstrapper/PreInstall` | `PreInstallPage.{h,cpp}` | 安装路径选择 + 进度 |
| `FluxingBootstrapper/UserData` | `UserDataPage.{h,cpp}` | 用户数据路径选择 |
| `FluxingBootstrapper/FirstRun` | `FirstRunPage.{h,cpp}` | 首启引导（主题 + 云同步 + 主方案 + 上手卡） |
| `FluxingBootstrapper/Uninstall` | `UninstallPage.{h,cpp}` | 卸载界面 |
| `FluxingBootstrapper/NSISBridge` | `NSISBridge.{h,cpp}` | 与安装器 silent 通信（`/quiet` + 进度回调） |

### 2.2 流程

```
fluxing-2.2.0-installer.exe
├─ 安装器解压（quiet）
├─ 安装器调用 FluxingBootstrapper.exe --stage=install
│  ├─ PreInstallPage
│  │  ├─ 用户选安装路径
│  │  ├─ 写 install_args.json
│  │  └─ 调安装器 --install=path --silent
│  │     └─ 安装器解压到 path，实时回调进度
│  │        (stdin pipe, JSON 一行一事件)
│  └─ UserDataPage (若 PreInstall 成功)
│     ├─ 用户选数据路径
│     └─ 写 install_args.json
├─ 安装器调用 FluxingBootstrapper.exe --stage=firstrun
│  └─ FirstRunPage
│     ├─ 主题选择 → 写 weasel.yaml
│     ├─ 云同步（spec 010）→ 注册流程
│     ├─ 主方案选择 → 改 schema_list
│     └─ 上手卡 → 展示 5 个快捷键
└─ 完成

卸载：
unins000.exe (安装器)
├─ 安装器调用 FluxingBootstrapper.exe --stage=uninstall
│  └─ UninstallPage
│     ├─ 确认
│     ├─ 调安装器 --uninstall --silent
│     └─ 进度条
└─ 完成
```

### 2.3 安装器 silent + Bootstrapper 通信

- 安装器调用 `FluxingBootstrapper.exe --stage=...` 时，通过 stdin pipe 发送命令（JSON 一行一事件）。
- Bootstrapper 完成后通过 stdout 返回 JSON。
- 进度条：Bootstrapper 自渲染。
- 安装器仅做解压 + 注册表；不渲染 UI。

### 2.4 验证步骤

1. **TDD**：
   - `TestBootstrapper.cpp` — mock 安装器 silent 模式，断言：
     - 路径合法化（强制后缀 `fluxing`）
     - 卸载时用户数据保留/清除逻辑正确
2. **手动验证**：
   - 装 Fluxing v2.2.0：安装窗 → 路径选择 → 解压 → 完成 → 首启引导 → 主题 + 云同步 + 主方案 + 上手卡。
   - 卸载：控制面板 → 卸载窗 → 确认 → 进度 → 完成。
   - 3 平台 3 DPI。

## 3. Out of scope

- 不做"自动更新器"（v3+；WinSparkle 已可承担）。
- 不做"安装包数字签名 UI"（命令行 `signtool` 即可）。
- 不做"自解压 exe"（安装器自带）。

## 4. 完成定义（待 v2.2 启动时再细化）

> 本 spec 当前仅交付 design.md；评审通过后才会写 tasks.md + plan.md + spec.md。

- [ ] 后续：安装器改造（silent + JSON bridge）
- [ ] 后续：FluxingBootstrapper 实现
- [ ] 后续：PreInstallPage / UserDataPage / FirstRunPage / UninstallPage
- [ ] 后续：单测 + 手动验证
- [ ] 后续：commit + release `fluxing-2.2.0-installer.exe`