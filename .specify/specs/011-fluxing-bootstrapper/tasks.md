# 011 · Tasks · 安装 / 卸载 / 首启引导

> v2 P3 仅设计。v2.2+ 实施时此表生效。

## Phase 1 · 设计（v2 阶段）

- [x] T001 [P3] [US11-A] design.md §2.2 详细流程图
- [x] T002 [P3] [US11-A] design.md §2.3 JSON 协议 schema（stdin/stdout 单行事件）

## Phase 2 · 安装器改造（v2.2 实施）

- [ ] T003 [P3] 修改 `output/install.nsi` 为 silent-only（解压 + 写注册表 + 调 Bootstrapper）
- [ ] T004 [P3] 新建 `FluxingBootstrapper/{main,BootstrapWindow}.{h,cpp}`
- [ ] T005 [P3] 新建 `FluxingBootstrapper/PreInstall/{PreInstallPage,NSISBridge}.{h,cpp}`
- [ ] T006 [P3] 新建 `FluxingBootstrapper/UserData/{UserDataPage}.{h,cpp}`

## Phase 3 · 卸载（v2.2 实施）

- [ ] T007 [P3] 新建 `FluxingBootstrapper/Uninstall/{UninstallPage}.{h,cpp}`

## Phase 4 · 首启引导（v2.2 实施）

- [ ] T008 [P3] 新建 `FluxingBootstrapper/FirstRun/{FirstRunPage}.{h,cpp}`（主题 + 云同步 + 主方案 + 上手卡）
- [ ] T009 [P3] 接入 spec 010 云同步账户注册流程

## Phase 5 · 验证

- [ ] T010 [P3] 新建 `test/TestBootstrapper.cpp`（mock NSIS silent 模式，路径合法化，保留/清除用户数据）
- [ ] T011 [P3] `xbuild.bat installer` → 0 errors
- [ ] T012 [P3] AGENTS.md §2.5 静默安装 smoke test PASS
- [ ] T013 [P3] 手动验证 3 平台 3 DPI

## Phase 6 · 提交 & Release

- [ ] T014 [P3] commit：`feat(fluxing): spec 011 fluxing bootstrapper`
- [ ] T015 [P3] release `fluxing-2.2.0-installer.exe` 推 `kizemo`