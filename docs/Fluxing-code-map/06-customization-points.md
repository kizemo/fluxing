# 06 · "火流猩输入法 / Fluxing" 二次编辑可改点清单

> 本文件不解释现状（见 `01`~`05`），只列**把项目从 "Weasel/小狼毫" 改成 "Fluxing/火流猩输入法" 时需要动哪些点**。每条给出位置、建议改法、二次编辑前要做的验证。

## 6.1 命名常量（核心）

| 位置 | 当前值 | 改法 | 验证 |
|---|---|---|---|
| `include\WeaselConstants.h::WEASEL_CODE_NAME` | `"Weasel"` | 改成 `"Fluxing"`（RIME 引擎 distribution_code_name） | 重启 server，rime 引擎识别新名 |
| `include\WeaselConstants.h::WEASEL_REG_KEY` | `L"Software\\Rime\\Weasel"` | 改成 `L"Software\\Fluxing\\Fluxing"`（注意：与下文 6.4 TSF key 对齐） | 装到本机，键路径生效 |
| `include\WeaselConstants.h::RIME_REG_KEY` | `L"Software\\Rime"` | 改成 `L"Software\\Fluxing"`（或保留 Rime 子树作为后向兼容） | 决定 |
| `WeaselSetup\imesetup.cpp` 中的 `c_clsidTextService` | `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` | **生成新的 GUID**（不重用）：用 `uuidgen` 或 VS 的"插入 GUID" | TSF 装/卸后系统不残留旧 CLSID |
| `WeaselSetup\imesetup.cpp` 中的 `c_guidProfile` | `{3D02CAB6-2B8E-4781-BA20-1C9267529467}` | **生成新的 GUID** | 同上 |

> ⚠️ GUID 变更后，老用户升级会变成"两个 IME"共存。建议同时保留 `uninstall` 旧版路径。

## 6.2 用户可见字符串

| 位置 | 当前值 | 改法 |
|---|---|---|
| `include\WeaselUtility.h::get_weasel_ime_name()` | 返回 `L"小狼毫"` 或 `L"Weasel"` | 改成 `L"火流猩输入法"` 与 `L"Fluxing"`（按 UI 语言） |
| `WeaselServer\WeaselServerApp.cpp` 各种托盘标题 | "小狼毫" | 同步改 |
| `WeaselServer\WeaselServer.rc` 字符串表 | "小狼毫" / "Weasel" | 改 "火流猩输入法" / "Fluxing" |
| `WeaselDeployer\WeaselDeployer.rc` 字符串表 | "小狼毫" / "Weasel" | 同上 |
| `WeaselSetup\WeaselSetup.rc` 字符串表（IDS_STR_HELP 等） | "Weasel Deployer"、"小狼毫" | 全部改 |
| `WeaselSetup\InstallOptionsDlg.cpp` `OnInitDialog` 标题 | "小狼毫" | 改 |
| `output\install.nsi` 多处 `LangString / Name / VIAddVersionKey` | "小狼毫" / "Weasel" | 全部改 |
| `output\install.nsi` 卸载 `REG_UNINST_KEY` | `"Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel"` | 改为 `"Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing"` |
| `output\install.nsi` 卸载图标 | `..\resource\weasel.ico` | 替换为新图标（保留 weasel.ico 历史） |
| `README.md` / `INSTALL.md` | "【小狼毫】" 标题 + 商标 | 改 |
| `update\appcast.xml` `<title>` | "小狼毫" | 改 |
| `update\appcast.xml` `<link>` 域名 | `rime.github.io/release/weasel/...` | 改到自己的发布域名 |
| `update\bump-version.ps1`/`sh` 输出文案 | "小狼毫" / "Weasel" | 改 |

## 6.3 二进制文件名（影响安装器、托盘调起、快捷方式）

| 位置 | 当前值 | 改法 | 风险 |
|---|---|---|---|
| `WeaselTSF\xmake.lua::set_filename` | `weasel.dll / weaselx64.dll / weaselARM.dll / weaselARM64.dll` | 改 `fluxing.dll / fluxingx64.dll / fluxingARM.dll / fluxingARM64.dll` | 同步改 TSF 注册指向 |
| `arm64x_wrapper\WeaselTSF_arm64.def / WeaselTSF_x64.def` | `weaselARM64X.dll` | 改 `fluxingARM64X.dll` | 同步改 xbuild.bat |
| `WeaselServer\xmake.lua::after_build` | `WeaselServer.exe` | 改 `FluxingServer.exe`（或保留） | 同步改 NSIS、托盘、注册表 |
| `WeaselDeployer\xmake.lua::after_build` | `WeaselDeployer.exe` | 改 `FluxingDeployer.exe` | 同步改 `WeaselServerApp::SetupMenuHandlers` 中的调起命令 |
| `WeaselSetup\xmake.lua::after_build` | `WeaselSetup.exe` | 改 `FluxingSetup.exe` | 同步改 install.bat / start_service.bat |
| `WeaselServer.cpp` 调 `install_dir()` 用 `GetModuleFileName` | 假定 exe 在安装目录 | 不需改，自动跟随 | — |
| `output\start_service.bat / stop_service.bat` | 启动 `WeaselServer.exe` | 跟随新名 | — |
| `output\install.nsi` `SetOutPath / File` | 拷贝旧名 | 跟随新名 | — |
| `output\install.bat / uninstall.bat` | 调 `WeaselServer.exe /q` 等 | 跟随新名 | — |
| `xbuild.bat` 最后 `copy arm64x_wrapper\weaselARM64X.dll output` | 旧名 | 跟随新名 | — |

## 6.4 注册表与服务

| 位置 | 当前值 | 改法 |
|---|---|---|
| `WeaselSetup\imesetup.cpp::WEASEL_WER_KEY` | `SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\WeaselServer.exe` | 改为 `...\\LocalDumps\\FluxingServer.exe` |
| `WeaselService.h::WEASEL_SERVICE_NAME` | `"WeaselIME"`（如需查证） | 改 `"FluxingIME"` |
| `WeaselServerApp.cpp::win_sparkle_set_registry_path` | `"Software\\Rime\\Weasel\\Updates"` | 改 `"Software\\Fluxing\\Fluxing\\Updates"` |
| 6.2 中所有写 `Software\Rime\Weasel` / `Software\Rime\weasel` 的代码 | 集中在 `WeaselSetup.cpp` / `WeaselServerApp.cpp` / `WeaselTSF.cpp::OnSetThreadFocus` | 统一改 |
| `RimeWithWeaselHandler::_Setup` 写 `RimeTraits::app_name` | `"rime.weasel"` | 改 `"rime.fluxing"`（用于 rime log 路径前缀） |
| `RimeWithWeaselHandler::_Setup` 写 `RimeTraits::distribution_name` | `wtou8(get_weasel_ime_name())` | 跟随 6.2 |
| `RimeWithWeaselHandler::_Setup` 写 `RimeTraits::distribution_code_name` | `WEASEL_CODE_NAME` | 跟随 6.1 |
| `RimeWithWeaselHandler::_Setup` 写 `RimeTraits::distribution_version` | `WEASEL_VERSION` | 保留 |
| `WeaselSetup\imesetup.cpp::PSZTITLE_HANS / PSZTITLE_HANT` | 内嵌 CLSID + Profile GUID | 跟随 6.1 新 GUID |

## 6.5 资源（图标、版本）

| 位置 | 当前值 | 改法 |
|---|---|---|
| `resource\*.ico` | 5 个图标：weasel / zh / en / full / half / reload | 替换为火流猩风格的图标（保留文件名以便 NSIS 引用或一并改名） |
| `WeaselSetup\WeaselSetup.ico` | 安装器图标 | 替换 |
| `WeaselTSF\WeaselTSF.rc`、`WeaselServer\WeaselServer.rc`、`WeaselDeployer\WeaselDeployer.rc`、`WeaselSetup\WeaselSetup.rc` | 资源中图标 ID 与版本 | 替换 + 重设 `VIAddVersionKey` |
| `weasel.props.template` / `build.bat / xbuild.bat` 注入 `VERSION_*` 宏 | 0.17.4 → 0.1.0（建议从 0.x 开始） | 改默认值 |
| `bundled_winsparkle.patch` | 来自上游 | 保留（与产品名无关） |

## 6.6 行为/默认值

| 位置 | 当前值 | 改法 | 备注 |
|---|---|---|---|
| `WeaselServerApp.cpp::WeaselServerApp` 默认 `m_show_notifications_time=1200` | 1200ms | 可调 | — |
| `RimeWithWeaselHandler::Initialize` 读 `global_ascii`、`show_notifications_time` | 读 `weasel.yaml` | 默认模板放在 `output\data\weasel.yaml` 时同步改 | — |
| `RimeWithWeaselHandler::_UpdateUIStyle` 默认色板 | `google` / `aqua` 等 | 跟随 `output\data\weasel.yaml` 修改 | — |
| 托盘菜单命令（`WeaselServer.rc`） | `ID_WEASELTRAY_*` | 资源 ID 一般保留为 1xxx（命令 ID 不暴露），但显示字符串要改 | — |
| 链接 Wiki / Home / Forum URL | `https://rime.im/...` | 改为自己的链接或保留 rime 官方 | 决定保留/替换 |
| `_IsDeployerRunning` 检查 `WeaselDeployerExclusiveMutex` | 互斥名 | 改 `FluxingDeployerExclusiveMutex`（如要避免与原版冲突） | — |
| `WeaselDeployer.cpp` 单实例互斥 | `WeaselDeployerExclusiveMutex` | 同上 | — |
| `WeaselTSF.cpp::_EnsureServerConnected` 检查的进程名 | `WeaselServer.exe` | 改 `FluxingServer.exe` | — |
| `WeaselServer.cpp::install_dir()` 返回的目录 | 安装目录 | 跟随 | — |
| `output\7z.exe / curl.exe / curl-ca-bundle.crt` | 来自下载的二进制 | 不需改 | — |

## 6.7 用户目录与方案数据

| 位置 | 当前值 | 改法 |
|---|---|---|
| 默认用户目录 `HKCU\Software\Rime\Weasel\RimeUserDir` | `%AppData%\Rime` | 跟随 6.1 改为 `%AppData%\Fluxing`；或保留以与 librime 兼容 |
| 共享目录 `WeaselSharedDataPath()` | `<install>\data` | 不需改（路径硬编码） |
| `output\data\default.yaml`、`output\data\weasel.yaml` 默认模板 | 包含 `Rime` 字样的 `schema_list` 注释 | 跟随品牌调整 |
| `output\data\*.schema.yaml` 默认方案 | 朙月拼音等 | 保留（属 RIME 社区） |

## 6.8 构建/CI

| 位置 | 当前值 | 改法 |
|---|---|---|
| `.github\workflows\ci.yml` `marvinpinto/action-automatic-releases` 与 `softprops/action-gh-release` | 仓库名 `rime/weasel`、appcast URL `rime.github.io/release/weasel/` | 改 `fluxing/weasel`（如保留原仓库）或 `Fluxing/fluxing`；artifacts 名称 `weasel-artifact-*` 改 `fluxing-artifact-*` |
| `.github\workflows\update-appcast.yml` | 触发 `rime/home` 的 `gh-pages.yml` | 改自己的 `gh-pages.yml` |
| `output\install.nsi` 安装器输出 | `archives\weasel-${PRODUCT_VERSION}-installer.exe` | 改 `archives\fluxing-${PRODUCT_VERSION}-installer.exe` |
| `bundled_winsparkle.patch` | 描述 WinSparkle 行为 | 不需改 |
| `clang-format.sh`、`install-clang-format.bat` | clang-format 18 | 不需改 |
| `render.js` | release 渲染 | 跟随仓库名 |

## 6.9 文档

| 位置 | 当前值 | 改法 |
|---|---|---|
| `README.md` | "【小狼毫】輸入法" | 改 "【火流猩输入法】" |
| `INSTALL.md` | "Rime with Weasel" | 改 "Fluxing with Rime" |
| `CHANGELOG.md` | 历史 | 顶部新增 `## [0.1.0] - <today>` 一节 |
| 新建 `LICENSE.txt` 提及的产品名 | 小狼毫 | 改 火流猩输入法 |
| `docs\Fluxing-code-map\00-index.md` | 写了 "火流猩输入法 / Fluxing" | 已对齐 |

## 6.10 风险与注意

1. **CLSID 变更**：旧用户升级必须先卸载原版，否则系统里会有两个 IME 同时注册 TSF。`WeaselSetup.cpp::uninstall` 路径要保证先关掉旧 `WeaselServer.exe`。
2. **互斥名变更**：`WeaselDeployerExclusiveMutex` 与 `WeaselServer.exe /q` 关闭逻辑会找不到原进程。**要么保留旧名**（推荐过渡期），**要么同时向原名进程发关停**。
3. **RIME 协议层不动**：`RimeWithWeaselHandler::_Setup` 写 `RimeTraits::app_name` 时建议同时写 `prebuilt_data_dir = shared_data_dir`，否则 `weasel.yaml` 找不到。
4. **PLIST 兼容性**：若你希望与原 Weasel 用户文件互通，把 `RimeUserDir` 改向原路径是可行的（不建议）。
5. **新 GUID 必须全新生成**，绝不能复用，否则会与原版冲突导致 TSF 注册失败。
6. **CI secrets**：如果改用自己的仓库，GitHub Actions secrets 需重设（`GITHUB_TOKEN` 自动可用，`ACTIONS_DEPLOY_KEY` 需要重新配）。

## 6.11 建议的二次编辑顺序（每个改一个原子提交）

1. **rename: 引入 Fluxing 命名空间**（先改 `WEASEL_CODE_NAME / WEASEL_REG_KEY / RIME_REG_KEY`，不影响行为）
2. **feat(brand): 显示文本/字符串表改为 Fluxing / 火流猩输入法**（`get_weasel_ime_name()`、所有 .rc 字符串表、`install.nsi`）
3. **feat(brand): 资源图标替换**（`resource\*.ico`、`WeaselSetup.ico`）
4. **feat(brand): 二进制文件名改为 fluxing*.dll / FluxingServer.exe**（`xmake.lua` 中 `set_filename`、NSIS、安装/卸载 bat、托盘 `SetupMenuHandlers` 调起命令、TSF 重连中的 `WeaselServer.exe` 字符串、Deployer 互斥名）
5. **feat(brand): 生成新 CLSID / Profile GUID 并替换**（`imesetup.cpp` + NSIS 内的 layout 字符串 + 测试如要保留亦同步）
6. **chore(brand): 注册表路径重命名 + 互斥名迁移**（`Software\Rime\Weasel → Software\Fluxing\Fluxing`、`WeaselDeployerExclusiveMutex`）
7. **chore(docs): README/INSTALL/CHANGELOG 同步品牌**
8. **ci: workflow 与 artifacts 名称更新**

每一步独立 commit、消息按 `git-workflow-and-versioning` 规范的 `<type>: <why>` 格式，commit 前 `git status` 必须干净。
