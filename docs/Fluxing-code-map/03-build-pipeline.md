# 03 · 构建与发布流水线

## 3.1 两条构建路径

| 入口 | 类型 | 何时用 | 特点 |
|---|---|---|---|
| `build.bat [all \| rime \| boost \| data \| opencc \| weasel \| installer \| arm64 \| debug \| release \| rebuild]` | `msbuild` + `weasel.sln` | CI（lint 之后的 build 阶段），Windows 用户最简 | 成熟、子模块构建齐全；与 Visual Studio 项目严格对应 |
| `xbuild.bat [all \| rime \| boost \| data \| opencc \| weasel \| installer \| arm64 \| debug \| release \| rebuild \| clean \| commands]` | `xmake` | 平台/配置自由（Linux 也能部分用），CI 的 matrix 之一 | 灵活，新增 target 容易 |

两者都会先把 `env.bat.template` 复制为 `env.bat`（如果不存在），然后 `call env.bat` 读 `BOOST_ROOT` 等。

## 3.2 顶层 xmake 目标集合

来自 `xmake.lua`：

```lua
includes("WeaselIPC", "WeaselUI", "WeaselTSF")
if is_arch("x64") or is_arch("x86") then
  includes("RimeWithWeasel", "WeaselIPCServer", "WeaselServer", "WeaselDeployer")
end
if is_arch("x86") then
  includes("WeaselSetup")
end
if is_mode("debug") then
  includes("test/TestWeaselIPC")
  includes("test/TestResponseParser")
else
  add_cxflags("/GL")
  add_ldflags("/LTCG /INCREMENTAL:NO", {force = true})
end
```

| target | kind | deps | 输出 |
|---|---|---|---|
| WeaselIPC | static | — | (静态库) |
| WeaselUI | static (+ /openmp) | — | (静态库) |
| WeaselTSF | shared + def | WeaselIPC, WeaselUI | `weasel*.dll`（按 arch 重命名） |
| RimeWithWeasel | static (+ use_weaselconstants) | — | (静态库) |
| WeaselIPCServer | static | — | (静态库) |
| WeaselServer | binary | WeaselUI, WeaselIPC, RimeWithWeasel, WeaselIPCServer | `WeaselServer.exe` |
| WeaselDeployer | binary | WeaselIPC, RimeWithWeasel | `WeaselDeployer.exe` |
| WeaselSetup | binary (x86) | — | `WeaselSetup.exe` |
| TestWeaselIPC | binary (debug) | — | `TestWeaselIPC.exe` |
| TestResponseParser | binary (debug) | — | `TestResponseParser.exe` |

## 3.3 共享规则（在 `xmake.lua` 中定义）

```lua
add_cxflags("/utf-8 /MP /O2 /Oi /Gm- /EHsc /MT /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /external:W3 /Gd /TP")
add_ldflags("/TLBID:1 /DYNAMICBASE /NXCOMPAT")
add_links("atls", "shell32", "advapi32", "gdi32", "user32", "uuid", "ole32")
add_includedirs("include", "include/wtl", BOOST/include, ATLMFC/include)
add_linkdirs(BOOST/stage/lib, projectdir/lib[64], ATLMFC/lib/<arch>)

rule("subcmd")  -- /SUBSYSTEM:CONSOLE
rule("subwin")  -- /SUBSYSTEM:WINDOWS
rule("add_rcfiles")  -- 让 *.rc 拿到 VERSION_* / FILE_VERSION / PRODUCT_VERSION 宏
rule("use_weaselconstants")  -- 自动给含 WeaselConstants.h 的 target 加 rcflags
```

## 3.4 版本号生成（`build.bat` / `xbuild.bat`）

```
VERSION_MAJOR / MINOR / PATCH（默认 0.17.4）
WEASEL_VERSION = MAJOR.MINOR.PATCH
WEASEL_BUILD = 0（release build 时）
PRODUCT_VERSION = WEASEL_VERSION.WEASEL_BUILD
  - 非 release build：从 `git tag --sort=-creatordate | findstr %WEASEL_VERSION%` 找最近同版本 tag，
    git rev-list TAG..HEAD --count 得到 commit 距离，再用 git rev-parse --short HEAD 做后缀
    → PRODUCT_VERSION = x.y.z.NN.abcdef
FILE_VERSION = WEASEL_VERSION.WEASEL_BUILD（永远 4 段）
```

NSIS 接收 `WEASEL_VERSION / WEASEL_BUILD / PRODUCT_VERSION` 三个宏。
资源（`*.rc`）通过 `add_rcfiles` / `use_weaselconstants` 注入 `VERSION_MAJOR` 等宏。

## 3.5 子模块

```
.gitmodules:
  [submodule "librime"]  path = librime  url = https://github.com/rime/librime.git
  [submodule "plum"]     path = plum     url = https://github.com/rime/plum.git
```

`build.bat rime`：
- 清掉 librime 及 deps 的 `build/dist/lib`。
- 分别用 x64 与 Win32 调 `librime\build.bat`，输出到 `weasel\lib64\output` 与 `weasel\lib\output\Win32`。

`CI` 走 `get-rime.ps1 -use dev` 直接下载现成 dll/lib 放进 `lib/lib64/output`，避免每次构建 librime。

## 3.6 Boost 构建

`build.bat boost` → `install_boost.bat` 走 `b2`/`bjam` 编译（默认 `msvc-14.2` 即 VS2019 工具集）。产物路径 `%BOOST_ROOT%/stage/lib`。`xbuild.bat` 实际是 `call build.bat boost` 再 `xmake`。

## 3.7 数据 / 资源准备

- `build.bat data`：
  - `rime-install-config.bat` 生成 `output\data\default.yaml`、`output\weasel.yaml`（可选）等的安装配置。
  - 调用 `plum` 子模块或直接调脚本安装默认方案到 `output\data`。
  - `output\data\essay.txt`（词频）等。
- `build.bat opencc`：构建 opencc 词典。

## 3.8 arm64x 包装

```
arm64x_wrapper/
  dummy.c
  WeaselTSF_arm64.def / WeaselTSF_x64.def
  build.bat
```

`xbuild.bat arm64 installer` 会先编出 x64 / x86 / arm / arm64 四份 `weasel*.dll`，再编 arm64x 复合体 `weaselARM64X.dll`（arm64 优先，x64 转发）。`build.bat` 在最后 copy 到 `output\`。

## 3.9 NSIS 安装包

`output\install.nsi`：

- 接收 `WEASEL_VERSION`、`WEASEL_BUILD`、`PRODUCT_VERSION` 三个 `/D` 宏。
- 输出 `archives\weasel-${PRODUCT_VERSION}-installer.exe`。
- 多语言 MUI（LANG_TRADCHINESE、LANG_SIMPCHINESE、LANG_ENGLISH）。
- 安装内容（按 arch）：
  - 拷贝 `weasel*.dll`、`WeaselServer.exe`、`WeaselDeployer.exe`、`WeaselSetup.exe`、`output\*.dll`（rime 系列）
  - 拷贝 `output\data\*`（方案、OpenCC）
  - 写注册表：`HKLM\...\Keyboard Layout\0404:...` / `0804:...` 注册 TSF、桌面 / 开始菜单快捷方式
  - 写 `HKCU\Software\Rime\Weasel` 几个字段
  - 注册 WER 转储位置
- 卸载：删除文件 / 清理注册表 / 移除 TSF

## 3.10 CI（`.github\workflows\ci.yml`）

- 触发：所有 push、所有 `*` 分支、`[0-9]+.*` tag、PR、manual。
- jobs：
  1. **lint**（ubuntu-22.04）：装 clang-format-18，跑 `clang-format.sh -i`（只在非 tag 上跑）
  2. **build**（windows-2022，matrix `[msbuild, xmake]`）：
     - `cp env.vs2022.bat env.bat`
     - cache Boost 1.84 → `install_boost.bat` + `build.bat boost arm64`
     - cache WinSparkle 0.9.2（克隆仓库 + 应用 `bundled_winsparkle.patch`，用 msbuild 编译）
     - 复制 `WinSparkle.{dll,lib}` 到 `output/ lib64/ Win32/lib`
     - `get-rime.ps1 -use dev` 下载 librime dll
     - `build.bat data`
     - xmake job：`xmake-io/github-action-setup-xmake@v1` 装 xmake
     - 构建：`build.bat arm64 installer` 或 `xbuild.bat arm64 installer`
     - 7z 压缩 `*.pdb` 为 `debug_symbols.7z`
     - 上传 artifact `weasel-artifact-<git_ref_name>`
     - tag 构建：抽 `RELEASE_CHANGELOG.md` → 草稿 release
     - master 构建：每日预发布 `Nightly Build`
- `update-appcast.yml`：每次 release published/prereleased 触发 `gh-pages.yml` 更新 appcast（在 rime/home 仓库）。

## 3.11 本地开发者如何用

```bat
:: 1) 克隆（含子模块）
git clone --recursive https://github.com/rime/weasel.git
cd weasel

:: 2) 配环境
copy env.bat.template env.bat
::  编辑 env.bat 填 BOOST_ROOT
set BOOST_ROOT=D:\boost\boost_1_84_0

:: 3) 装 Boost（首次或升级时）
build.bat boost

:: 4) 装/更新 librime（推荐用 get-rime）
pwsh .\get-rime.ps1 -use dev

:: 5) 构建数据
build.bat data

:: 6) 构建主体
build.bat weasel

:: 7) 打包安装器（需 NSIS）
build.bat installer

:: 8) 安装到本机试
cd output
install.bat
```

xmake 路径：

```bat
xbuild.bat boost rime data opencc
xbuild.bat weasel
xbuild.bat installer
```

## 3.12 产物位置速查

| 产物 | 路径 |
|---|---|
| `weasel.dll` (x86) | `output\` |
| `weaselx64.dll` (x64) | `output\` |
| `weaselARM.dll` (arm) | `output\` |
| `weaselARM64.dll` (arm64) | `output\` |
| `weaselARM64X.dll` (arm64x) | `output\` |
| `WeaselServer.exe` | `output\` 与 `output\Win32\` |
| `WeaselDeployer.exe` | `output\` 与 `output\Win32\` |
| `WeaselSetup.exe` | `output\` |
| `WeaselIPC.dll / WeaselUI.dll`（rime 等） | `output\data\` 旁边的 dll |
| 方案数据 | `output\data\` |
| 安装器 | `output\archives\weasel-<ver>-installer.exe` |
| Debug symbols | `output\archives\debug_symbols.7z` |
