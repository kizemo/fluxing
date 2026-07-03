# 01 · 项目总览

## 1.1 一句话定义

**Weasel（小狼毫）** = Windows 上的 RIME 输入法前端。技术栈：C++17（部分老代码 C++14）+ ATL + WTL + D2D/DirectWrite + Boost + librime（子模块）。

适用平台：Windows 8.1 ~ Windows 11；架构：x86 / x64 / arm / arm64；arm64x 包装后并入 `weaselARM64X.dll`。

## 1.2 顶层目录地图

```
F:\soft\00selfmade\rime
├── WeaselTSF\              TSF 文本服务（weasel*.dll / weaselARM64X.dll）
├── WeaselServer\           后台算法服务（WeaselServer.exe，含托盘 + WinSparkle）
├── WeaselDeployer\         设置/部署/词库管理 GUI（WeaselDeployer.exe）
├── WeaselSetup\            安装/卸载器（WeaselSetup.exe，x86 only）
├── WeaselUI\               输入法面板 UI（静态库，weasel*.dll 私有依赖）
├── WeaselIPC\              IPC 客户端库（静态库，给 TSF / Deployer 链接）
├── WeaselIPCServer\        IPC 服务端库（静态库，给 WeaselServer 链接）
├── RimeWithWeasel\         RIME 引擎对接（静态库，所有 Weasel*exe/dll 链接）
├── arm64x_wrapper\         arm64ec 转发 DLL 包装器（build.bat 一行构建）
├── include\                公共头（IPC 接口、UI 接口、Weasel 常量、WeaselUtility、KeyEvent、winsparkle、WTL）
├── test\                   仅 Debug 构建时编译的单元测试（TestWeaselIPC / TestResponseParser）
├── resource\               托盘图标 .ico（zh / en / full / half / reload / weasel）
├── update\                 版本脚本与 appcast（bump-version.{ps1,sh}、appcast.xml、write-release-notes.sh）
├── output\                 物化产物（构建后填入 *.exe / *.dll / 资源 / NSIS installer / archives / Win32/）
├── librime\                子模块（rime 引擎 C/C++ 库 + 一级依赖：glog/gtest/leveldb/marisa-trie/opencc/yaml-cpp）
├── plum\                   子模块（方案管理器，可被 WeaselServer 启动）
├── weasel.sln              Visual Studio 解决方案（CI 在用）
├── weasel.props.template   MSBuild 属性表模板（Boost / 平台工具集 / 版本号宏）
├── env.bat.template        本地构建环境变量模板（开发者拷贝为 env.bat）
├── env.vs2019.bat / env.vs2022.bat   预设的 env.bat 模板（CI 用）
├── build.bat / xbuild.bat  构建入口（msbuild / xmake 两种路径）
├── xmake.lua               xmake 顶层描述
├── .github\workflows\      CI（ci.yml: lint + msbuild/xmake 双矩阵 + release）+ update-appcast.yml
├── weasel-deploy-*.sh      部署辅助脚本
├── extract_changelog.ps1   tag release 时从 CHANGELOG.md 抽 release notes
├── install-clang-format.bat / clang-format.{ps1,sh}   clang-format 18 包装
├── render.js               GitHub release 渲染辅助
└── bundled_winsparkle.patch  v0.9.2 内的本地补丁：禁止默认浏览器自动打开 release note
```

## 1.3 进程清单（最终交付物）

| 进程 | 类型 | 输入文件 | 作用 |
|---|---|---|---|
| `weasel.dll` (x86) | TSF 文本服务 | `WeaselTSF/` | 32 位系统/应用的 IME 入口 |
| `weaselx64.dll` (x64) | TSF 文本服务 | `WeaselTSF/` | 64 位系统/应用的 IME 入口 |
| `weaselARM.dll` (arm) | TSF 文本服务 | `WeaselTSF/` | arm 系统 |
| `weaselARM64.dll` (arm64) | TSF 文本服务 | `WeaselTSF/` | 纯 arm64 系统 |
| `weaselARM64X.dll` (arm64x) | TSF 文本服务 | `arm64x_wrapper` | arm64ec 复合体（arm64 + x64 转发） |
| `WeaselServer.exe` | 后台服务 + 托盘 | `WeaselServer/` | RIME 引擎进程 + UI 面板宿主 + 托盘 + 升级 |
| `WeaselDeployer.exe` | GUI 工具 | `WeaselDeployer/` | 部署 / 词库 / 同步 / 样式 / 切换器设置 |
| `WeaselSetup.exe` (x86 only) | 安装器 | `WeaselSetup/` | TSF 注册 / 卸载 / 维护 |
| `output\archives\weasel-<ver>-installer.exe` | NSIS 安装包 | `output\install.nsi` | 终端用户安装包 |
| `WeaselServerApp.exe` 是同一进程名 `WeaselServer.exe` | — | — | 由 `WeaselServerApp.cpp` 启动 |

## 1.4 静态库（被各 exe/dll 链接）

| 静态库 | 链接者 |
|---|---|
| `WeaselIPC` | `WeaselTSF`、`WeaselDeployer` |
| `WeaselUI` | `WeaselTSF`、`WeaselServer` |
| `WeaselIPCServer` | `WeaselServer` |
| `RimeWithWeasel` | `WeaselServer`、`WeaselDeployer` |

每个库自己的 xmake.lua 用 `add_deps` 串起来（见 `xmake.lua` 顶层）。

## 1.5 关键三方依赖

来自 `README.md` 与 `weasel.props.template`：

- **Boost** `>= 1.60`（CI 锁 1.84）：thread、filesystem、serialization
- **rime** 通过子模块 `librime/`，由 CI 用 `get-rime.ps1 -use dev` 拉现成 dll
- **WinSparkle** v0.9.2（带本地 patch）：更新器
- **librime 一级 deps**：glog、gtest、leveldb、marisa-trie、opencc、yaml-cpp（构建 librime 时构建）
- **NSIS**：安装包生成
- **7-Zip**：debug symbols 压缩 / 用户词库归档
- **WTL**（`include\wtl\*`）：ATL 之上薄封装，被 `WeaselUI`、`WeaselServer` 使用
- **Direct2D / DirectWrite**：UI 渲染
- **gdiplus**：UI 旧路径
- **ATL/MFC**：来自 Visual Studio

## 1.6 关键 TSF GUID（用户可见）

- CLSID_TextService = `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}`
- Profile_GUID     = `{3D02CAB6-2B8E-4781-BA20-1C9267529467}`
- 简体 layout 字符串：`0804:{A3F4CDED-...}{3D02CAB6-...}`
- 繁体 layout 字符串：`0404:{A3F4CDED-...}{3D02CAB6-...}`

注册表项（由 `WeaselSetup\imesetup.cpp` 写入；由 `RimeWithWeasel\RimeWithWeasel.cpp` 读取）：

- HKCU\Software\Rime\Weasel（**注意大小写**：Setup 写的 key 是 `Software\\Rime\\Weasel`；而 weaselTSF 读 `Software\\Rime\\weasel` 不一致——这是上游已知历史问题，详见 `05-data-and-config.md`）
  - `RimeUserDir`（REG_SZ）：用户目录覆盖
  - `Language`（REG_SZ）：`chs` / `cht` / `eng`
  - `Hant`（REG_DWORD）：1 = 繁体
  - `ToggleImeOnOpenClose`（REG_SZ）：`yes` / `no`
  - `UpdateChannel`（REG_SZ）：`release` / `testing`
- HKCU\Software\Rime\weasel（小写）
  - `ToggleImeOnOpenClose`
- HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\WeaselServer.exe：崩溃转储位置

## 1.7 关键路径（来自 `RimeWithWeasel\WeaselUtility.cpp` 与 `WeaselServer\WeaselServer.cpp`）

| 路径常量 | 解析逻辑 | 默认值 |
|---|---|---|
| `WeaselSharedDataPath()` | exe 所在目录 + `data\` | `<install>\data` |
| `WeaselUserDataPath()` | 读 `HKCU\Software\Rime\Weasel\RimeUserDir` | `%AppData%\Rime` |
| `WeaselLogPath()` | 固定 | `%TEMP%\rime.weasel` |
| 安装目录 | 启动 exe 所在目录 | `C:\Program Files\rime.weasel\` 或用户选择 |

## 1.8 命令行总览

| 命令 | 谁来执行 | 作用 |
|---|---|---|
| `WeaselServer.exe` | 系统 / 托盘启动 | 后台服务（默认） |
| `WeaselServer.exe /q` 或 `/quit` | installer | 关掉已运行实例 |
| `WeaselServer.exe /userdir` | 托盘"用户文件夹" | 打开用户目录 |
| `WeaselServer.exe /weaseldir` | 托盘"程序文件夹" | 打开安装目录 |
| `WeaselServer.exe /ascii` 或 `/nascii` | 快捷方式 | 切换 ASCII 模式 |
| `WeaselServer.exe /update` | 托盘"检查更新" | 调 WinSparkle |
| `WeaselDeployer.exe [/deploy \| /dict \| /sync \| /install]` | 托盘 / 设置入口 | 部署/词库/同步/首次安装 |
| `WeaselSetup.exe [/i \| /s \| /t]` | 安装器 | 交互 / 简体 / 繁体安装 |
| `WeaselSetup.exe /toggleascii \| /togglehan` | 用户 | 切换 |
| `WeaselSetup.exe /testing \| /release` | 用户 | 切更新通道 |
| `output\install.bat` | 解压安装包后 | 拷贝文件、注册 TSF |

## 1.9 用户可见产品名（多处）

| 出现位置 | 当前值 |
|---|---|
| `WeaselSetup\InstallOptionsDlg.rc` 字符串表 | "小狼毫" / "Weasel" |
| `output\install.nsi` 多个 `Name` / `LangString` | "小狼毫" / "WeaselSetup" |
| `include\WeaselUtility.h::get_weasel_ime_name()` | 按 UI 语言返回 "小狼毫" 或 "Weasel" |
| `include\WeaselConstants.h::WEASEL_CODE_NAME` | `"Weasel"` |
| `RimeWithWeasel\RimeWithWeasel.cpp` RimeTraits | `distribution_name = "小狼毫" / "Weasel"` |
| 资源（`resource\*.ico`） | 鼠标图标 |
| 托盘菜单 | 多个 `ID_WEASELTRAY_*` 命令 |
| 注册表 / appcast | `Software\\Rime\\Weasel`、`appcast.xml` |
| GitHub Release 名 | `weasel-<version>-installer.exe` |

`06-customization-points.md` 会给出把以上所有 "Weasel/小狼毫" → "Fluxing/火流猩输入法" 的精确改动清单。
